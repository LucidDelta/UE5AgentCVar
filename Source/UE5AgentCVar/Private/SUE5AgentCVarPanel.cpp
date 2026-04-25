// Copyright (c) UE5AgentCVar. All Rights Reserved.

#include "SUE5AgentCVarPanel.h"
#include "FCVarDatabase.h"
#include "FCVarAgentRunner.h"

#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/AppStyle.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"
#include "Logging/LogMacros.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogUE5AgentCVar, Log, All);

#define LOCTEXT_NAMESPACE "UE5AgentCVarPanel"

namespace
{
	const FString kIniSection(TEXT("UE5AgentCVar"));

	// Default model list — keep small and focused on Haiku per the plugin's
	// scope (cheap, fast). User can switch via the combo and the choice is
	// persisted; we don't fetch the live list to avoid an extra round-trip.
	void GetDefaultModelOptions(TArray<TSharedPtr<FString>>& Out)
	{
		Out.Reset();
		Out.Add(MakeShared<FString>(TEXT("claude-haiku-4-5-20251001")));
		Out.Add(MakeShared<FString>(TEXT("claude-sonnet-4-6")));
		Out.Add(MakeShared<FString>(TEXT("claude-opus-4-7")));
	}
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

void SUE5AgentCVarPanel::Construct(const FArguments& InArgs)
{
	Database = MakeShared<FCVarDatabase>();
	Runner   = MakeShared<FCVarAgentRunner>();

	const bool bDbOK = Database->LoadFromPluginResources();

	GetDefaultModelOptions(ModelOptions);
	LoadSettings();
	if (HaikuModel.IsEmpty() && ModelOptions.Num() > 0)
	{
		HaikuModel = *ModelOptions[0];
	}

	EnsureSavedDirExists();

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(8.f))
		[
			SNew(SVerticalBox)

			// ---- Toolbar row -----------------------------------------------
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.ToolTipText(LOCTEXT("SettingsTooltip", "Toggle API key / model settings"))
					.OnClicked(this, &SUE5AgentCVarPanel::OnGearClicked)
					[
						SNew(SImage).Image(FAppStyle::GetBrush("Icons.Settings"))
					]
				]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 0, 0)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.ToolTipText(LOCTEXT("AgentLogTooltip", "Toggle Agent Process log"))
					.OnClicked(this, &SUE5AgentCVarPanel::OnAgentLogClicked)
					[
						SNew(SImage).Image(FAppStyle::GetBrush("Icons.Details"))
					]
				]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 0, 0)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.ToolTipText(LOCTEXT("HistoryLogTooltip", "Toggle CVar History log"))
					.OnClicked(this, &SUE5AgentCVarPanel::OnHistoryLogClicked)
					[
						SNew(SImage).Image(FAppStyle::GetBrush("Icons.Documentation"))
					]
				]

				+ SHorizontalBox::Slot().FillWidth(1.f) [ SNew(SSpacer) ]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SAssignNew(DatabaseStatusText, STextBlock)
					.Text(FText::FromString(bDbOK
						? FString::Printf(TEXT("CVars loaded: %d"), Database->Num())
						: TEXT("CVar database FAILED to load")))
				]

				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
				[
					SAssignNew(RunningIndicator, SBox)
					.Visibility(EVisibility::Hidden)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SCircularThrobber).Radius(7.f).Period(0.6f).NumPieces(5)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6, 0, 0, 0)
						[
							SNew(STextBlock).Text(LOCTEXT("Running", "Running\u2026"))
						]
					]
				]
			]

			// ---- Collapsible settings --------------------------------------
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				SAssignNew(SettingsBox, SBox)
				.Visibility(EVisibility::Collapsed)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
					.Padding(FMargin(6.f))
					[
						SNew(SWrapBox)
						.UseAllottedSize(true)
						.InnerSlotPadding(FVector2D(8, 4))

						+ SWrapBox::Slot()
						[
							SNew(SBox)
							.WidthOverride(360)
							[
								SAssignNew(APIKeyBox, SEditableTextBox)
								.IsPassword(true)
								.HintText(LOCTEXT("APIKeyHint", "Anthropic API key"))
								.Text(FText::FromString(APIKey))
								.OnTextCommitted(this, &SUE5AgentCVarPanel::OnAPIKeyCommitted)
							]
						]

						+ SWrapBox::Slot()
						[
							SNew(SBox)
							.WidthOverride(280)
							[
								SAssignNew(ModelCombo, SComboBox<TSharedPtr<FString>>)
								.OptionsSource(&ModelOptions)
								.OnGenerateWidget(this, &SUE5AgentCVarPanel::GenerateModelComboItem)
								.OnSelectionChanged(this, &SUE5AgentCVarPanel::OnModelChanged)
								[
									SNew(STextBlock)
									.Text_Lambda([this]()
									{
										return HaikuModel.IsEmpty()
											? LOCTEXT("NoModel", "(no model)")
											: FText::FromString(HaikuModel);
									})
								]
							]
						]

						+ SWrapBox::Slot()
						[
							SNew(SBox)
							.MinDesiredWidth(180)
							.VAlign(VAlign_Center)
							[
								SAssignNew(StatusText, STextBlock)
								.Text(LOCTEXT("StatusReady", "Ready"))
							]
						]
					]
				]
			]

			// ---- Collapsible Agent Process log -----------------------------
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				SAssignNew(AgentLogBox, SBox)
				.Visibility(EVisibility::Collapsed)
				.HeightOverride(180)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
					.Padding(FMargin(4.f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().VAlign(VAlign_Center).AutoWidth()
							[
								SNew(STextBlock).Text(LOCTEXT("AgentLogTitle", "Agent Process Log"))
							]
							+ SHorizontalBox::Slot().FillWidth(1.f) [ SNew(SSpacer) ]
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SNew(SButton)
								.Text(LOCTEXT("ClearLog", "Clear"))
								.OnClicked(this, &SUE5AgentCVarPanel::OnClearAgentLogClicked)
							]
						]
						+ SVerticalBox::Slot().FillHeight(1.f)
						[
							SAssignNew(AgentLogScroll, SScrollBox)
							+ SScrollBox::Slot()
							[
								SAssignNew(AgentLogText, SMultiLineEditableText)
								.IsReadOnly(true)
								.AutoWrapText(true)
								.Text(LOCTEXT("LogEmpty", "(no entries yet)"))
							]
						]
					]
				]
			]

			// ---- Collapsible CVar History log ------------------------------
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				SAssignNew(HistoryLogBox, SBox)
				.Visibility(EVisibility::Collapsed)
				.HeightOverride(160)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
					.Padding(FMargin(4.f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot().VAlign(VAlign_Center).AutoWidth()
							[
								SNew(STextBlock).Text(LOCTEXT("HistoryLogTitle", "CVar History Log"))
							]
							+ SHorizontalBox::Slot().FillWidth(1.f) [ SNew(SSpacer) ]
							+ SHorizontalBox::Slot().AutoWidth()
							[
								SNew(SButton)
								.Text(LOCTEXT("ClearLog", "Clear"))
								.OnClicked(this, &SUE5AgentCVarPanel::OnClearHistoryLogClicked)
							]
						]
						+ SVerticalBox::Slot().FillHeight(1.f)
						[
							SAssignNew(HistoryLogScroll, SScrollBox)
							+ SScrollBox::Slot()
							[
								SAssignNew(HistoryLogText, SMultiLineEditableText)
								.IsReadOnly(true)
								.AutoWrapText(true)
								.Text(LOCTEXT("LogEmpty", "(no entries yet)"))
							]
						]
					]
				]
			]

			// ---- AI Mode section -------------------------------------------
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 4)
			[
				SNew(SExpandableArea)
				.InitiallyCollapsed(false)
				.HeaderContent()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AIMode", "Query AI to find recommended CVars and the best values"))
					.Font(FAppStyle::Get().GetFontStyle("DetailsView.CategoryFontStyle"))
				]
				.BodyContent()
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 4)
					[
						SNew(SBox).MinDesiredHeight(90)
						[
							SAssignNew(AIPromptBox, SMultiLineEditableTextBox)
							.HintText(LOCTEXT("AIPromptHint", "e.g. \"My shadows are flickering on foliage at distance, how do I tune that?\""))
							.AutoWrapText(true)
							.OnTextChanged(this, &SUE5AgentCVarPanel::OnAIPromptChanged)
						]
					]

					+ SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 4)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SButton)
							.Text(LOCTEXT("Submit", "Submit"))
							.OnClicked(this, &SUE5AgentCVarPanel::OnRunAIClicked)
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(12, 0, 6, 0)
						[
							SNew(STextBlock).Text(LOCTEXT("MaxResults", "Max results:"))
						]
						+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
						[
							SNew(SBox)
							.WidthOverride(80.f)
							[
								SNew(SSpinBox<int32>)
								.MinValue(1)
								.MinSliderValue(1)
								.MaxSliderValue(30)
								.Value(MaxResults)
								.OnValueChanged(this, &SUE5AgentCVarPanel::OnMaxResultsChanged)
								.OnValueCommitted_Lambda([this](int32 NewValue, ETextCommit::Type)
								{
									OnMaxResultsChanged(NewValue);
								})
							]
						]
						+ SHorizontalBox::Slot().FillWidth(1.f) [ SNew(SSpacer) ]
					]

					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SBox)
						.HeightOverride(220)
						[
							SAssignNew(AIResultsScroll, SScrollBox)
						]
					]
				]
			]

			// ---- Manual Mode section ---------------------------------------
			+ SVerticalBox::Slot()
			.FillHeight(1.f)
			.Padding(0, 4, 0, 0)
			[
				SNew(SExpandableArea)
				.InitiallyCollapsed(false)
				.HeaderContent()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ManualMode", "Manual Search — substring match across all CVars"))
					.Font(FAppStyle::Get().GetFontStyle("DetailsView.CategoryFontStyle"))
				]
				.BodyContent()
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 4)
					[
						SAssignNew(ManualSearchBox, SEditableTextBox)
						.HintText(LOCTEXT("ManualSearchHint", "Type to search\u2026 (live)"))
						.OnTextChanged(this, &SUE5AgentCVarPanel::OnManualSearchChanged)
					]

					+ SVerticalBox::Slot().FillHeight(1.f)
					[
						SAssignNew(ManualResultsScroll, SScrollBox)
					]
				]
			]
		]
	];

	if (APIKey.IsEmpty())
	{
		if (StatusText.IsValid())
		{
			StatusText->SetText(LOCTEXT("NoKey", "No API key entered"));
		}
	}

	// Pre-select model in combo
	if (ModelCombo.IsValid())
	{
		for (const TSharedPtr<FString>& Opt : ModelOptions)
		{
			if (Opt.IsValid() && *Opt == HaikuModel)
			{
				ModelCombo->SetSelectedItem(Opt);
				break;
			}
		}
	}
}

SUE5AgentCVarPanel::~SUE5AgentCVarPanel()
{
}

// ---------------------------------------------------------------------------
// Settings persistence
// ---------------------------------------------------------------------------

void SUE5AgentCVarPanel::LoadSettings()
{
	GConfig->GetString(*kIniSection, TEXT("AnthropicAPIKey"), APIKey,     GEditorPerProjectIni);
	GConfig->GetString(*kIniSection, TEXT("HaikuModel"),      HaikuModel, GEditorPerProjectIni);
}

void SUE5AgentCVarPanel::SaveSettings()
{
	GConfig->SetString(*kIniSection, TEXT("AnthropicAPIKey"), *APIKey,     GEditorPerProjectIni);
	GConfig->SetString(*kIniSection, TEXT("HaikuModel"),      *HaikuModel, GEditorPerProjectIni);
	GConfig->Flush(false, GEditorPerProjectIni);
}

// ---------------------------------------------------------------------------
// UI handlers — toolbar buttons
// ---------------------------------------------------------------------------

FReply SUE5AgentCVarPanel::OnGearClicked()
{
	bSettingsExpanded = !bSettingsExpanded;
	if (SettingsBox.IsValid())
	{
		SettingsBox->SetVisibility(bSettingsExpanded ? EVisibility::Visible : EVisibility::Collapsed);
	}
	return FReply::Handled();
}

FReply SUE5AgentCVarPanel::OnAgentLogClicked()
{
	bAgentLogExpanded = !bAgentLogExpanded;
	if (AgentLogBox.IsValid())
	{
		AgentLogBox->SetVisibility(bAgentLogExpanded ? EVisibility::Visible : EVisibility::Collapsed);
	}
	if (bAgentLogExpanded)
	{
		RefreshAgentLogDisplay();
	}
	return FReply::Handled();
}

FReply SUE5AgentCVarPanel::OnHistoryLogClicked()
{
	bHistoryLogExpanded = !bHistoryLogExpanded;
	if (HistoryLogBox.IsValid())
	{
		HistoryLogBox->SetVisibility(bHistoryLogExpanded ? EVisibility::Visible : EVisibility::Collapsed);
	}
	if (bHistoryLogExpanded)
	{
		RefreshHistoryLogDisplay();
	}
	return FReply::Handled();
}

FReply SUE5AgentCVarPanel::OnClearAgentLogClicked()
{
	EnsureSavedDirExists();
	const FString AgentLogPath = GetSavedDir() / TEXT("agent.log");
	FFileHelper::SaveStringToFile(FString(), *AgentLogPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	RefreshAgentLogDisplay();
	return FReply::Handled();
}

FReply SUE5AgentCVarPanel::OnClearHistoryLogClicked()
{
	EnsureSavedDirExists();
	const FString HistoryLogPath = GetSavedDir() / TEXT("history.log");
	FFileHelper::SaveStringToFile(FString(), *HistoryLogPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	RefreshHistoryLogDisplay();
	return FReply::Handled();
}

// ---------------------------------------------------------------------------
// UI handlers — settings
// ---------------------------------------------------------------------------

void SUE5AgentCVarPanel::OnAPIKeyCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	APIKey = NewText.ToString();
	SaveSettings();
	if (StatusText.IsValid())
	{
		StatusText->SetText(APIKey.IsEmpty()
			? LOCTEXT("NoKey", "No API key entered")
			: LOCTEXT("StatusReady", "Ready"));
	}
}

void SUE5AgentCVarPanel::OnModelChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (!NewSelection.IsValid()) return;
	HaikuModel = *NewSelection;
	SaveSettings();
}

TSharedRef<SWidget> SUE5AgentCVarPanel::GenerateModelComboItem(TSharedPtr<FString> InItem)
{
	return SNew(STextBlock).Text(FText::FromString(InItem.IsValid() ? *InItem : FString()));
}

// ---------------------------------------------------------------------------
// UI handlers — AI mode
// ---------------------------------------------------------------------------

void SUE5AgentCVarPanel::OnAIPromptChanged(const FText& NewText)
{
	CurrentAIPrompt = NewText.ToString();
}

void SUE5AgentCVarPanel::OnMaxResultsChanged(int32 NewValue)
{
	MaxResults = FMath::Max(NewValue, 1);
}

FReply SUE5AgentCVarPanel::OnRunAIClicked()
{
	if (bRequestInFlight)
	{
		return FReply::Handled();
	}
	if (APIKey.IsEmpty())
	{
		if (StatusText.IsValid()) StatusText->SetText(LOCTEXT("NoKey", "No API key entered"));
		return FReply::Handled();
	}
	if (HaikuModel.IsEmpty())
	{
		if (StatusText.IsValid()) StatusText->SetText(LOCTEXT("NoModelSet", "No model selected"));
		return FReply::Handled();
	}
	if (CurrentAIPrompt.IsEmpty() || !Database.IsValid() || Database->Num() == 0)
	{
		return FReply::Handled();
	}

	const TArray<int32> Candidates = Database->FilterForPrompt(CurrentAIPrompt, 150);
	AppendAgentLog(TEXT("PROMPT"), FString::Printf(
		TEXT("model=%s candidates=%d max_results=%d\n--- prompt ---\n%s\n--- end prompt ---"),
		*HaikuModel, Candidates.Num(), MaxResults, *CurrentAIPrompt));

	bRequestInFlight = true;
	if (RunningIndicator.IsValid())
	{
		RunningIndicator->SetVisibility(EVisibility::Visible);
	}
	if (StatusText.IsValid())
	{
		StatusText->SetText(LOCTEXT("Querying", "Querying Claude\u2026"));
	}

	TWeakPtr<SUE5AgentCVarPanel> WeakPanel = SharedThis(this);
	FOnCVarRecommendationsFetched OnDone;
	OnDone.BindLambda([WeakPanel](bool bOK, int32 Status, const TArray<FCVarRecommendation>& Recs, const FString& RawOrErr)
	{
		TSharedPtr<SUE5AgentCVarPanel> Pinned = WeakPanel.Pin();
		if (!Pinned.IsValid()) return;

		Pinned->bRequestInFlight = false;
		if (Pinned->RunningIndicator.IsValid())
		{
			Pinned->RunningIndicator->SetVisibility(EVisibility::Hidden);
		}

		if (!bOK)
		{
			Pinned->AppendAgentLog(TEXT("ERROR"),
				FString::Printf(TEXT("status=%d %s"), Status, *RawOrErr));
			if (Pinned->StatusText.IsValid())
			{
				Pinned->StatusText->SetText(Status == 401 || Status == 403
					? LOCTEXT("InvalidKey", "Invalid API key")
					: LOCTEXT("FetchFailed", "Request failed"));
			}
			return;
		}

		Pinned->AppendAgentLog(TEXT("OK"),
			FString::Printf(TEXT("status=%d returned=%d\n--- raw ---\n%s\n--- end raw ---"),
				Status, Recs.Num(), *RawOrErr));

		if (Pinned->StatusText.IsValid())
		{
			Pinned->StatusText->SetText(FText::Format(
				LOCTEXT("ResultsCount", "{0} results"), FText::AsNumber(Recs.Num())));
		}

		Pinned->RebuildAIResults(Recs);
	});

	Runner->SendRecommendation(APIKey, HaikuModel, CurrentAIPrompt, MaxResults,
		Database->GetAll(), Candidates, OnDone);

	return FReply::Handled();
}

// ---------------------------------------------------------------------------
// UI handlers — manual mode
// ---------------------------------------------------------------------------

void SUE5AgentCVarPanel::OnManualSearchChanged(const FText& NewText)
{
	CurrentManualQuery = NewText.ToString();
	RebuildManualResults(CurrentManualQuery);
}

// ---------------------------------------------------------------------------
// Result cards
// ---------------------------------------------------------------------------

TSharedRef<SWidget> SUE5AgentCVarPanel::BuildResultCard(const TSharedPtr<FResultRow>& Row)
{
	if (!Row.IsValid())
	{
		return SNew(SBox);
	}

	// Pre-fill the value box with the AI suggestion (or default for manual mode).
	const FString InitialValue = Row->Reason.IsEmpty()
		? Row->Default
		: FString(); // for AI rows we'll set explicitly via the suggested_value below

	TSharedRef<SVerticalBox> Body = SNew(SVerticalBox);

	// Header line: name + section
	Body->AddSlot().AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Row->Name))
			.Font(FAppStyle::Get().GetFontStyle("BoldFont"))
		]
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("[%s]   default: %s"),
				*Row->Section, *Row->Default)))
			.ColorAndOpacity(FLinearColor(0.7f, 0.7f, 0.7f))
		]
	];

	// Description
	Body->AddSlot().AutoHeight().Padding(0, 4, 0, 0)
	[
		SNew(STextBlock)
		.Text(FText::FromString(Row->Description))
		.AutoWrapText(true)
	];

	// Reason (AI only)
	if (!Row->Reason.IsEmpty())
	{
		Body->AddSlot().AutoHeight().Padding(0, 4, 0, 0)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FString::Printf(TEXT("AI: %s"), *Row->Reason)))
			.AutoWrapText(true)
			.ColorAndOpacity(FLinearColor(0.55f, 0.85f, 1.0f))
		];
	}

	// Value + Apply + Reset
	const FString Name        = Row->Name;
	const FString DefaultVal  = Row->Default;
	TWeakPtr<SUE5AgentCVarPanel> WeakPanel = SharedThis(this);

	TSharedPtr<SEditableTextBox> ValueBoxLocal;
	Body->AddSlot().AutoHeight().Padding(0, 6, 0, 0)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(STextBlock).Text(LOCTEXT("Value", "Value:"))
		]
		+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(6, 0, 6, 0)
		[
			SAssignNew(ValueBoxLocal, SEditableTextBox)
		]
		+ SHorizontalBox::Slot().AutoWidth()
		[
			SNew(SButton)
			.Text(LOCTEXT("Apply", "Apply"))
			.OnClicked_Lambda([WeakPanel, Name, ValueBoxLocal]()
			{
				TSharedPtr<SUE5AgentCVarPanel> Pinned = WeakPanel.Pin();
				if (Pinned.IsValid() && ValueBoxLocal.IsValid())
				{
					Pinned->ApplyCVar(Name, ValueBoxLocal->GetText().ToString());
				}
				return FReply::Handled();
			})
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
		[
			SNew(SButton)
			.Text(LOCTEXT("ResetDefault", "Reset to Default"))
			.ToolTipText(LOCTEXT("ResetDefaultTip",
				"Apply the default value to the CVar. Leaves the suggested value in the box so you can flip back with Apply."))
			.OnClicked_Lambda([WeakPanel, Name, DefaultVal]()
			{
				TSharedPtr<SUE5AgentCVarPanel> Pinned = WeakPanel.Pin();
				if (Pinned.IsValid())
				{
					Pinned->ResetCVar(Name, DefaultVal);
				}
				return FReply::Handled();
			})
		]
	];

	Row->ValueBox = ValueBoxLocal;
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
		.Padding(FMargin(8.f))
		[
			Body
		];
}

void SUE5AgentCVarPanel::RebuildAIResults(const TArray<FCVarRecommendation>& Recs)
{
	AIResults.Reset();
	if (!AIResultsScroll.IsValid()) return;
	AIResultsScroll->ClearChildren();

	const TArray<FCVarEntry>& All = Database->GetAll();

	for (const FCVarRecommendation& R : Recs)
	{
		// Find the matching catalog entry so we can show full metadata even if
		// Haiku omitted parts of it.
		const FCVarEntry* Found = nullptr;
		for (const FCVarEntry& E : All)
		{
			if (E.Name.Equals(R.Name, ESearchCase::IgnoreCase))
			{
				Found = &E;
				break;
			}
		}

		TSharedPtr<FResultRow> Row = MakeShared<FResultRow>();
		Row->Name        = Found ? Found->Name        : R.Name;
		Row->Default     = Found ? Found->Default     : FString();
		Row->Section     = Found ? Found->Section     : FString(TEXT("Unknown"));
		Row->Description = Found ? Found->Description : FString();
		Row->Reason      = R.Reason.IsEmpty() ? TEXT("(no reason given)") : R.Reason;

		TSharedRef<SWidget> Card = BuildResultCard(Row);
		AIResultsScroll->AddSlot().Padding(0, 0, 0, 4) [ Card ];

		// Pre-fill suggested value AFTER BuildResultCard creates the box.
		if (Row->ValueBox.IsValid())
		{
			Row->ValueBox->SetText(FText::FromString(R.SuggestedValue));
		}

		AIResults.Add(Row);
	}
}

void SUE5AgentCVarPanel::RebuildManualResults(const FString& Query)
{
	ManualResults.Reset();
	if (!ManualResultsScroll.IsValid()) return;
	ManualResultsScroll->ClearChildren();

	if (!Database.IsValid() || Query.IsEmpty())
	{
		return;
	}

	const TArray<int32> Indices = Database->Search(Query, 50);
	const TArray<FCVarEntry>& All = Database->GetAll();

	for (int32 Idx : Indices)
	{
		if (!All.IsValidIndex(Idx)) continue;
		const FCVarEntry& E = All[Idx];

		TSharedPtr<FResultRow> Row = MakeShared<FResultRow>();
		Row->Name        = E.Name;
		Row->Default     = E.Default;
		Row->Section     = E.Section;
		Row->Description = E.Description;
		Row->Reason      = FString(); // manual: no reason

		TSharedRef<SWidget> Card = BuildResultCard(Row);
		ManualResultsScroll->AddSlot().Padding(0, 0, 0, 4) [ Card ];

		// Pre-fill with default value as a safe starting point.
		if (Row->ValueBox.IsValid())
		{
			Row->ValueBox->SetText(FText::FromString(E.Default));
		}

		ManualResults.Add(Row);
	}
}

// ---------------------------------------------------------------------------
// CVar apply / reset
// ---------------------------------------------------------------------------

void SUE5AgentCVarPanel::ApplyCVar(const FString& Name, const FString& Value)
{
	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*Name);
	if (!CVar)
	{
		AppendHistoryLog(TEXT("APPLY_FAIL"),
			FString::Printf(TEXT("%s — variable does not exist at runtime"), *Name));
		UE_LOG(LogUE5AgentCVar, Warning, TEXT("ApplyCVar: %s does not exist"), *Name);
		return;
	}

	const FString Old = CVar->GetString();
	CVar->Set(*Value, ECVF_SetByConsole);
	const FString New = CVar->GetString();

	AppendHistoryLog(TEXT("APPLY"),
		FString::Printf(TEXT("%s = %s   (was: %s)"), *Name, *New, *Old));
	UE_LOG(LogUE5AgentCVar, Log, TEXT("Apply %s = %s"), *Name, *New);
}

void SUE5AgentCVarPanel::ResetCVar(const FString& Name, const FString& DefaultValue)
{
	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(*Name);
	if (!CVar)
	{
		AppendHistoryLog(TEXT("RESET_FAIL"),
			FString::Printf(TEXT("%s — variable does not exist at runtime"), *Name));
		return;
	}

	const FString Old = CVar->GetString();
	CVar->Set(*DefaultValue, ECVF_SetByConsole);

	AppendHistoryLog(TEXT("RESET"),
		FString::Printf(TEXT("%s = %s   (was: %s)"), *Name, *DefaultValue, *Old));
	UE_LOG(LogUE5AgentCVar, Log, TEXT("Reset %s to default %s"), *Name, *DefaultValue);
}

// ---------------------------------------------------------------------------
// Logging
// ---------------------------------------------------------------------------

FString SUE5AgentCVarPanel::GetSavedDir() const
{
	return FPaths::ProjectSavedDir() / TEXT("UE5AgentCVar");
}

void SUE5AgentCVarPanel::EnsureSavedDirExists() const
{
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	const FString Dir = GetSavedDir();
	if (!PF.DirectoryExists(*Dir))
	{
		PF.CreateDirectoryTree(*Dir);
	}
}

void SUE5AgentCVarPanel::AppendAgentLog(const FString& Section, const FString& Text)
{
	EnsureSavedDirExists();
	const FString AgentLogPath = GetSavedDir() / TEXT("agent.log");
	const FString Stamp = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S"));
	const FString Line  = FString::Printf(TEXT("[%s] [%s] %s\n"), *Stamp, *Section, *Text);
	FFileHelper::SaveStringToFile(Line, *AgentLogPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
		&IFileManager::Get(), FILEWRITE_Append);

	if (bAgentLogExpanded)
	{
		RefreshAgentLogDisplay();
	}
}

void SUE5AgentCVarPanel::AppendHistoryLog(const FString& Section, const FString& Text)
{
	EnsureSavedDirExists();
	const FString HistoryLogPath = GetSavedDir() / TEXT("history.log");
	const FString Stamp = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S"));
	const FString Line  = FString::Printf(TEXT("[%s] [%s] %s\n"), *Stamp, *Section, *Text);
	FFileHelper::SaveStringToFile(Line, *HistoryLogPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
		&IFileManager::Get(), FILEWRITE_Append);

	if (bHistoryLogExpanded)
	{
		RefreshHistoryLogDisplay();
	}
}

void SUE5AgentCVarPanel::RefreshAgentLogDisplay()
{
	const FString AgentLogPath = GetSavedDir() / TEXT("agent.log");
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *AgentLogPath))
	{
		Content = TEXT("(agent.log not found)");
	}
	if (AgentLogText.IsValid())
	{
		AgentLogText->SetText(FText::FromString(Content));
	}
	if (AgentLogScroll.IsValid())
	{
		AgentLogScroll->ScrollToEnd();
	}
}

void SUE5AgentCVarPanel::RefreshHistoryLogDisplay()
{
	const FString HistoryLogPath = GetSavedDir() / TEXT("history.log");
	FString Content;
	if (!FFileHelper::LoadFileToString(Content, *HistoryLogPath))
	{
		Content = TEXT("(history.log not found)");
	}
	if (HistoryLogText.IsValid())
	{
		HistoryLogText->SetText(FText::FromString(Content));
	}
	if (HistoryLogScroll.IsValid())
	{
		HistoryLogScroll->ScrollToEnd();
	}
}

#undef LOCTEXT_NAMESPACE
