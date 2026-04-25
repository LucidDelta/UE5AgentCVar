// Copyright (c) UE5AgentCVar. All Rights Reserved.
//
// SUE5AgentCVarPanel — root Slate widget for the UE5 Agent CVar tool.
//
// Two stacked input regions:
//   * AI mode: prompt + max-results slider + Run; results listed as cards with
//     name, default, section, description, AI reason, editable value, Apply,
//     Reset to Default.
//   * Manual mode: live search field; results identical to AI cards minus the
//     reason row.
//
// Two collapsible log panels:
//   * Agent Process Log — Claude reasoning steps (timestamps).
//   * CVar History Log  — every Apply / Reset action (timestamps).
//
// Settings (collapsible): API key, model selector. Persisted in
// GEditorPerProjectIni under [UE5AgentCVar].

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"

class SEditableTextBox;
class SMultiLineEditableTextBox;
class STextBlock;
class SBox;
class SScrollBox;
class SMultiLineEditableText;
class SVerticalBox;
class FCVarDatabase;
class FCVarAgentRunner;
struct FCVarRecommendation;

class SUE5AgentCVarPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SUE5AgentCVarPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SUE5AgentCVarPanel();

private:
	// ---- Settings persistence ----
	void LoadSettings();
	void SaveSettings();

	// ---- Logging ----
	void AppendAgentLog(const FString& Section, const FString& Text);
	void AppendHistoryLog(const FString& Section, const FString& Text);

	// ---- UI handlers ----
	FReply OnGearClicked();
	FReply OnAgentLogClicked();
	FReply OnHistoryLogClicked();
	FReply OnClearAgentLogClicked();
	FReply OnClearHistoryLogClicked();

	void OnAPIKeyCommitted(const FText& NewText, ETextCommit::Type CommitType);
	void OnModelChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> GenerateModelComboItem(TSharedPtr<FString> InItem);

	void OnAIPromptChanged(const FText& NewText);
	void OnMaxResultsChanged(int32 NewValue);
	FReply OnRunAIClicked();

	void OnManualSearchChanged(const FText& NewText);

	// ---- Result handling ----
	struct FResultRow
	{
		FString Name;
		FString Default;
		FString Section;
		FString Description;
		FString Reason; // empty for manual results

		TSharedPtr<SEditableTextBox> ValueBox;
	};

	TSharedRef<SWidget> BuildResultCard(const TSharedPtr<FResultRow>& Row);
	void RebuildAIResults(const TArray<FCVarRecommendation>& Recs);
	void RebuildManualResults(const FString& Query);

	void ApplyCVar(const FString& Name, const FString& Value);
	void ResetCVar(const FString& Name, const FString& DefaultValue);

	// ---- Misc ----
	void RefreshAgentLogDisplay();
	void RefreshHistoryLogDisplay();
	FString GetSavedDir() const;
	void EnsureSavedDirExists() const;

	// ---- State ----
	FString APIKey;
	FString HaikuModel;

	bool bSettingsExpanded     = false;
	bool bAgentLogExpanded     = false;
	bool bHistoryLogExpanded   = false;
	bool bRequestInFlight      = false;

	FString CurrentAIPrompt;
	int32   MaxResults         = 10;
	FString CurrentManualQuery;

	TSharedPtr<FCVarDatabase>    Database;
	TSharedPtr<FCVarAgentRunner> Runner;

	TArray<TSharedPtr<FResultRow>> AIResults;
	TArray<TSharedPtr<FResultRow>> ManualResults;

	// ---- Combo data ----
	TArray<TSharedPtr<FString>> ModelOptions;

	// ---- Widget refs ----
	TSharedPtr<SBox>                           SettingsBox;
	TSharedPtr<SBox>                           AgentLogBox;
	TSharedPtr<SBox>                           HistoryLogBox;
	TSharedPtr<SScrollBox>                     AgentLogScroll;
	TSharedPtr<SScrollBox>                     HistoryLogScroll;
	TSharedPtr<SMultiLineEditableText>         AgentLogText;
	TSharedPtr<SMultiLineEditableText>         HistoryLogText;
	TSharedPtr<SBox>                           RunningIndicator;
	TSharedPtr<SEditableTextBox>               APIKeyBox;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> ModelCombo;
	TSharedPtr<STextBlock>                     StatusText;
	TSharedPtr<SMultiLineEditableTextBox>      AIPromptBox;
	TSharedPtr<SEditableTextBox>               ManualSearchBox;
	TSharedPtr<SScrollBox>                     AIResultsScroll;
	TSharedPtr<SScrollBox>                     ManualResultsScroll;
	TSharedPtr<STextBlock>                     DatabaseStatusText;
};
