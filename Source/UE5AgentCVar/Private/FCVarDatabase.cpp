// Copyright (c) UE5AgentCVar. All Rights Reserved.

#include "FCVarDatabase.h"

#include "Misc/FileHelper.h"
#include "Interfaces/IPluginManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Logging/LogMacros.h"

DEFINE_LOG_CATEGORY_STATIC(LogCVarDatabase, Log, All);

namespace
{
	// Split a prompt into significant lowercase tokens for AI pre-filtering.
	// Treats anything non-alphanumeric as a separator and drops short stopwords.
	static const TSet<FString>& StopWords()
	{
		static const TSet<FString> Words = {
			TEXT("the"), TEXT("a"), TEXT("an"), TEXT("and"), TEXT("or"), TEXT("but"),
			TEXT("is"), TEXT("are"), TEXT("was"), TEXT("be"), TEXT("to"), TEXT("of"),
			TEXT("in"), TEXT("on"), TEXT("for"), TEXT("with"), TEXT("at"), TEXT("by"),
			TEXT("how"), TEXT("can"), TEXT("do"), TEXT("does"), TEXT("my"), TEXT("me"),
			TEXT("it"), TEXT("this"), TEXT("that"), TEXT("get"), TEXT("set"), TEXT("from"),
			TEXT("when"), TEXT("which"), TEXT("what"), TEXT("why"), TEXT("not"), TEXT("any"),
			TEXT("all"), TEXT("some"), TEXT("more"), TEXT("less"), TEXT("very"), TEXT("too")
		};
		return Words;
	}

	void TokenizePrompt(const FString& InPrompt, TArray<FString>& OutTokens)
	{
		FString Lower = InPrompt.ToLower();
		FString Current;
		Current.Reserve(32);

		auto Flush = [&]()
		{
			if (Current.Len() >= 3 && !StopWords().Contains(Current))
			{
				OutTokens.AddUnique(Current);
			}
			Current.Reset();
		};

		for (TCHAR C : Lower)
		{
			if (FChar::IsAlnum(C))
			{
				Current.AppendChar(C);
			}
			else
			{
				Flush();
			}
		}
		Flush();
	}
}

bool FCVarDatabase::LoadFromPluginResources()
{
	Entries.Reset();

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UE5AgentCVar"));
	if (!Plugin.IsValid())
	{
		UE_LOG(LogCVarDatabase, Error, TEXT("Plugin UE5AgentCVar not found in plugin manager"));
		return false;
	}

	const FString JsonPath = Plugin->GetBaseDir() / TEXT("Resources/cvars.json");
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *JsonPath))
	{
		UE_LOG(LogCVarDatabase, Error, TEXT("Failed to read %s"), *JsonPath);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogCVarDatabase, Error, TEXT("Failed to parse %s as JSON"), *JsonPath);
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* CVarArr = nullptr;
	if (!Root->TryGetArrayField(TEXT("cvars"), CVarArr) || !CVarArr)
	{
		UE_LOG(LogCVarDatabase, Error, TEXT("cvars.json missing 'cvars' array"));
		return false;
	}

	Entries.Reserve(CVarArr->Num());
	for (const TSharedPtr<FJsonValue>& Val : *CVarArr)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (!Val.IsValid() || !Val->TryGetObject(Obj) || !Obj || !Obj->IsValid())
		{
			continue;
		}

		FCVarEntry E;
		(*Obj)->TryGetStringField(TEXT("name"),        E.Name);
		(*Obj)->TryGetStringField(TEXT("default"),     E.Default);
		(*Obj)->TryGetStringField(TEXT("description"), E.Description);
		(*Obj)->TryGetStringField(TEXT("section"),     E.Section);

		const TArray<TSharedPtr<FJsonValue>>* KwArr = nullptr;
		if ((*Obj)->TryGetArrayField(TEXT("keywords"), KwArr) && KwArr)
		{
			E.Keywords.Reserve(KwArr->Num());
			for (const TSharedPtr<FJsonValue>& K : *KwArr)
			{
				FString S;
				if (K.IsValid() && K->TryGetString(S))
				{
					E.Keywords.Add(S);
				}
			}
		}

		E.NameLower        = E.Name.ToLower();
		E.DescriptionLower = E.Description.ToLower();

		Entries.Add(MoveTemp(E));
	}

	UE_LOG(LogCVarDatabase, Log, TEXT("Loaded %d CVars from %s"), Entries.Num(), *JsonPath);
	return Entries.Num() > 0;
}

TArray<int32> FCVarDatabase::Search(const FString& Query, int32 MaxResults) const
{
	TArray<int32> Result;
	if (Query.IsEmpty() || Entries.Num() == 0)
	{
		return Result;
	}

	const FString Q = Query.ToLower();

	struct FScored
	{
		int32 Index;
		int32 Score; // higher is better
	};
	TArray<FScored> Scored;
	Scored.Reserve(256);

	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		const FCVarEntry& E = Entries[i];
		const int32 NamePos = E.NameLower.Find(Q);
		const int32 DescPos = E.DescriptionLower.Find(Q);

		if (NamePos == INDEX_NONE && DescPos == INDEX_NONE)
		{
			continue;
		}

		int32 Score = 0;
		if (NamePos != INDEX_NONE)
		{
			// Big base for name hits, then prefer earlier match positions and
			// shorter names (so an exact prefix match beats a long buried one).
			Score += 10000 - NamePos * 10 - E.NameLower.Len();
			if (NamePos == 0)
			{
				Score += 5000;
			}
		}
		if (DescPos != INDEX_NONE)
		{
			Score += 1000 - DescPos;
		}

		Scored.Add({ i, Score });
	}

	Scored.Sort([](const FScored& A, const FScored& B) { return A.Score > B.Score; });

	const int32 N = FMath::Min(MaxResults, Scored.Num());
	Result.Reserve(N);
	for (int32 i = 0; i < N; ++i)
	{
		Result.Add(Scored[i].Index);
	}
	return Result;
}

TArray<int32> FCVarDatabase::FilterForPrompt(const FString& UserPrompt, int32 MaxResults) const
{
	TArray<int32> Result;
	if (Entries.Num() == 0)
	{
		return Result;
	}

	TArray<FString> Tokens;
	TokenizePrompt(UserPrompt, Tokens);

	// If no usable tokens, just return the first MaxResults entries — better than
	// nothing and matches the "small uniform sample" intuition.
	if (Tokens.Num() == 0)
	{
		const int32 N = FMath::Min(MaxResults, Entries.Num());
		Result.Reserve(N);
		for (int32 i = 0; i < N; ++i)
		{
			Result.Add(i);
		}
		return Result;
	}

	struct FScored
	{
		int32 Index;
		int32 Score;
	};
	TArray<FScored> Scored;
	Scored.Reserve(Entries.Num());

	for (int32 i = 0; i < Entries.Num(); ++i)
	{
		const FCVarEntry& E = Entries[i];
		int32 Score = 0;

		for (const FString& Tok : Tokens)
		{
			if (E.NameLower.Contains(Tok))
			{
				Score += 50;
			}
			for (const FString& Kw : E.Keywords)
			{
				if (Kw.Equals(Tok))
				{
					Score += 20;
					break;
				}
			}
			if (E.DescriptionLower.Contains(Tok))
			{
				Score += 5;
			}
		}

		if (Score > 0)
		{
			Scored.Add({ i, Score });
		}
	}

	Scored.Sort([](const FScored& A, const FScored& B) { return A.Score > B.Score; });

	const int32 N = FMath::Min(MaxResults, Scored.Num());
	Result.Reserve(N);
	for (int32 i = 0; i < N; ++i)
	{
		Result.Add(Scored[i].Index);
	}
	return Result;
}
