// Copyright (c) UE5AgentCVar. All Rights Reserved.
//
// FCVarDatabase loads cvars.json (produced by preprocess_cvars.py) and provides
// substring search over the catalog. Used by both AI mode (for candidate
// pre-filtering before sending to Claude) and manual mode (for the live search
// list shown in the panel).

#pragma once

#include "CoreMinimal.h"

struct FCVarEntry
{
	FString Name;
	FString Default;
	FString Description;
	FString Section;
	TArray<FString> Keywords;

	FString NameLower;
	FString DescriptionLower;
};

class FCVarDatabase
{
public:
	bool LoadFromPluginResources();

	int32 Num() const { return Entries.Num(); }
	const TArray<FCVarEntry>& GetAll() const { return Entries; }

	// Manual substring search across name + description. Returns up to MaxResults.
	// Hits in the name rank above hits in the description; earlier match position
	// ranks higher within each tier.
	TArray<int32> Search(const FString& Query, int32 MaxResults = 50) const;

	// Pre-filter for AI mode: extract significant words (>=3 chars) from the
	// user prompt, score every CVar by how many words match its name/keywords/
	// description, then return the top MaxResults indices.
	TArray<int32> FilterForPrompt(const FString& UserPrompt, int32 MaxResults = 150) const;

private:
	TArray<FCVarEntry> Entries;
};
