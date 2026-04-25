// Copyright (c) UE5AgentCVar. All Rights Reserved.

#include "FCVarAgentRunner.h"
#include "FCVarDatabase.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogCVarAgent, Log, All);

namespace
{
	const TCHAR* kEndpoint   = TEXT("https://api.anthropic.com/v1/messages");
	const TCHAR* kApiVersion = TEXT("2023-06-01");

	FString BuildSystemPrompt()
	{
		// 🔒 Format instruction per CLAUDE.md: raw JSON array, no fences, no preamble.
		return TEXT(
			"You are an Unreal Engine 5.5 console-variable expert helping a developer find "
			"and tune CVars to solve a specific problem.\n"
			"\n"
			"You are given a candidate list of CVars (subset of the engine's catalog) and the "
			"developer's plain-English problem description. Pick the most relevant CVars from "
			"that list — only ones that actually exist in the candidate list — and for each one "
			"suggest a concrete value that would help with the stated problem.\n"
			"\n"
			"OUTPUT RULES:\n"
			"- Respond with a RAW JSON ARRAY only.\n"
			"- No markdown. No triple-backtick code fences. No prose, preamble, or trailing text.\n"
			"- The entire response must parse as JSON via a strict parser.\n"
			"- Each element must be an object with exactly these fields:\n"
			"    \"name\":            the exact CVar name (must match the candidate list)\n"
			"    \"suggested_value\": a string holding the value to set (e.g. \"1\", \"0.5\", \"true\")\n"
			"    \"reason\":          one short sentence explaining why this CVar and value help\n"
			"- Order the array from most useful to least useful.\n"
			"- Never invent CVar names that are not in the candidate list.\n");
	}

	FString BuildCandidateBlock(const TArray<FCVarEntry>& Entries, const TArray<int32>& Indices)
	{
		FString Out;
		Out.Reserve(Indices.Num() * 128);
		Out += TEXT("[CANDIDATE CVARS]\n");
		for (int32 Idx : Indices)
		{
			if (!Entries.IsValidIndex(Idx)) continue;
			const FCVarEntry& E = Entries[Idx];
			Out += FString::Printf(TEXT("- %s | default=%s | section=%s | %s\n"),
				*E.Name, *E.Default, *E.Section, *E.Description);
		}
		Out += TEXT("[END CANDIDATE CVARS]\n");
		return Out;
	}

	FString EscapeForJson(const FString& In)
	{
		FString Out;
		Out.Reserve(In.Len() + 8);
		for (TCHAR C : In)
		{
			switch (C)
			{
			case TEXT('\\'): Out += TEXT("\\\\"); break;
			case TEXT('"'):  Out += TEXT("\\\""); break;
			case TEXT('\b'): Out += TEXT("\\b");  break;
			case TEXT('\f'): Out += TEXT("\\f");  break;
			case TEXT('\n'): Out += TEXT("\\n");  break;
			case TEXT('\r'): Out += TEXT("\\r");  break;
			case TEXT('\t'): Out += TEXT("\\t");  break;
			default:
				if (C < 0x20)
				{
					Out += FString::Printf(TEXT("\\u%04x"), (int32)C);
				}
				else
				{
					Out.AppendChar(C);
				}
				break;
			}
		}
		return Out;
	}
}

FString FCVarAgentRunner::StripMarkdownFences(const FString& In)
{
	FString S = In;
	S.TrimStartAndEndInline();

	// Strip leading ``` or ```json
	if (S.StartsWith(TEXT("```")))
	{
		const int32 NewlineIdx = S.Find(TEXT("\n"));
		if (NewlineIdx != INDEX_NONE)
		{
			S = S.Mid(NewlineIdx + 1);
		}
		else
		{
			S = S.Mid(3);
		}
	}
	// Strip trailing ```
	if (S.EndsWith(TEXT("```")))
	{
		S = S.Left(S.Len() - 3);
	}
	S.TrimStartAndEndInline();
	return S;
}

void FCVarAgentRunner::SendRecommendation(
	const FString& APIKey,
	const FString& Model,
	const FString& UserPrompt,
	int32 MaxResults,
	const TArray<FCVarEntry>& Entries,
	const TArray<int32>& CandidateIndices,
	FOnCVarRecommendationsFetched OnDone)
{
	if (APIKey.IsEmpty() || Model.IsEmpty())
	{
		OnDone.ExecuteIfBound(false, 0, TArray<FCVarRecommendation>(), TEXT("Missing API key or model"));
		return;
	}

	const FString System = BuildSystemPrompt();
	const FString Candidates = BuildCandidateBlock(Entries, CandidateIndices);

	const FString FullUserContent = FString::Printf(
		TEXT("%s\n[USER PROBLEM]\n%s\n\nReturn at most %d CVar recommendations as a raw JSON array."),
		*Candidates, *UserPrompt, MaxResults);

	// Build request body manually to keep the system field at top level
	// (Anthropic shape, per CLAUDE.md).
	const FString Body = FString::Printf(TEXT(
		"{"
		"\"model\":\"%s\","
		"\"max_tokens\":2048,"
		"\"system\":\"%s\","
		"\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}]"
		"}"),
		*EscapeForJson(Model),
		*EscapeForJson(System),
		*EscapeForJson(FullUserContent));

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(kEndpoint);
	Req->SetVerb(TEXT("POST"));
	Req->SetHeader(TEXT("x-api-key"), APIKey);
	Req->SetHeader(TEXT("anthropic-version"), kApiVersion);
	Req->SetHeader(TEXT("content-type"), TEXT("application/json"));
	Req->SetContentAsString(Body);

	Req->OnProcessRequestComplete().BindLambda(
		[OnDone](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
		{
			const int32 Status = Response.IsValid() ? Response->GetResponseCode() : 0;
			if (!bSuccess || !Response.IsValid() || Status < 200 || Status >= 300)
			{
				const FString ErrStr = Response.IsValid() ? Response->GetContentAsString() : FString(TEXT("HTTP error"));
				UE_LOG(LogCVarAgent, Warning, TEXT("HTTP failure status=%d body=%s"), Status, *ErrStr);
				OnDone.ExecuteIfBound(false, Status, TArray<FCVarRecommendation>(), ErrStr);
				return;
			}

			const FString RawBody = Response->GetContentAsString();

			TSharedPtr<FJsonObject> Root;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawBody);
			if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
			{
				OnDone.ExecuteIfBound(false, Status, TArray<FCVarRecommendation>(),
					FString::Printf(TEXT("Could not parse Anthropic envelope: %s"), *RawBody));
				return;
			}

			// content[0].text
			const TArray<TSharedPtr<FJsonValue>>* ContentArr = nullptr;
			if (!Root->TryGetArrayField(TEXT("content"), ContentArr) || !ContentArr || ContentArr->Num() == 0)
			{
				OnDone.ExecuteIfBound(false, Status, TArray<FCVarRecommendation>(),
					FString::Printf(TEXT("Anthropic response missing content[]: %s"), *RawBody));
				return;
			}

			FString TextOut;
			for (const TSharedPtr<FJsonValue>& V : *ContentArr)
			{
				const TSharedPtr<FJsonObject>* Obj = nullptr;
				if (V.IsValid() && V->TryGetObject(Obj) && Obj && (*Obj).IsValid())
				{
					FString Type;
					(*Obj)->TryGetStringField(TEXT("type"), Type);
					if (Type == TEXT("text"))
					{
						FString T;
						(*Obj)->TryGetStringField(TEXT("text"), T);
						TextOut += T;
					}
				}
			}

			const FString Cleaned = StripMarkdownFences(TextOut);

			// Parse cleaned text as JSON array
			TArray<TSharedPtr<FJsonValue>> Arr;
			TSharedRef<TJsonReader<>> ArrReader = TJsonReaderFactory<>::Create(Cleaned);
			if (!FJsonSerializer::Deserialize(ArrReader, Arr))
			{
				OnDone.ExecuteIfBound(false, Status, TArray<FCVarRecommendation>(),
					FString::Printf(TEXT("Model output was not a JSON array: %s"), *Cleaned));
				return;
			}

			TArray<FCVarRecommendation> Recs;
			Recs.Reserve(Arr.Num());
			for (const TSharedPtr<FJsonValue>& V : Arr)
			{
				const TSharedPtr<FJsonObject>* Obj = nullptr;
				if (!V.IsValid() || !V->TryGetObject(Obj) || !Obj || !(*Obj).IsValid())
				{
					continue;
				}
				FCVarRecommendation R;
				(*Obj)->TryGetStringField(TEXT("name"),            R.Name);
				(*Obj)->TryGetStringField(TEXT("suggested_value"), R.SuggestedValue);
				(*Obj)->TryGetStringField(TEXT("reason"),          R.Reason);
				if (!R.Name.IsEmpty())
				{
					Recs.Add(MoveTemp(R));
				}
			}

			OnDone.ExecuteIfBound(true, Status, Recs, Cleaned);
		});

	Req->ProcessRequest();
}
