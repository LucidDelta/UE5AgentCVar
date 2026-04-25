// Copyright (c) UE5AgentCVar. All Rights Reserved.
//
// FCVarAgentRunner — thin Anthropic-only HTTP layer. The panel calls
// SendRecommendation with a candidate CVar slice and a user prompt; the runner
// sends a single Messages API call and parses the returned JSON array of
// {name, suggested_value, reason} objects.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"

struct FCVarEntry;

struct FCVarRecommendation
{
	FString Name;
	FString SuggestedValue;
	FString Reason;
};

DECLARE_DELEGATE_FourParams(FOnCVarRecommendationsFetched,
	bool /*bOK*/, int32 /*HttpStatus*/, const TArray<FCVarRecommendation>& /*Results*/, const FString& /*ErrorOrRaw*/);

class FCVarAgentRunner
{
public:
	void SendRecommendation(
		const FString& APIKey,
		const FString& Model,
		const FString& UserPrompt,
		int32 MaxResults,
		const TArray<FCVarEntry>& Entries,
		const TArray<int32>& CandidateIndices,
		FOnCVarRecommendationsFetched OnDone);

	static FString StripMarkdownFences(const FString& In);
};
