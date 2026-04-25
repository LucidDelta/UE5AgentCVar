// Copyright (c) UE5AgentCVar. All Rights Reserved.

#include "UE5AgentCVar.h"
#include "SUE5AgentCVarPanel.h"
#include "UE5AgentCVarStyle.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"
#include "Modules/ModuleManager.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "UE5AgentCVar"

const FName FUE5AgentCVarModule::PanelTabName(TEXT("UE5AgentCVarPanel"));

void FUE5AgentCVarModule::StartupModule()
{
	FUE5AgentCVarStyle::Initialize();

	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
			PanelTabName,
			FOnSpawnTab::CreateRaw(this, &FUE5AgentCVarModule::SpawnPanelTab))
		.SetDisplayName(LOCTEXT("PanelTabTitle", "UE5 Agent CVar"))
		.SetTooltipText(LOCTEXT("PanelTabTooltip", "Open the UE5 Agent CVar assistant."))
		.SetIcon(FSlateIcon(FUE5AgentCVarStyle::GetStyleSetName(), "UE5AgentCVar.MenuIcon"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	UToolMenus::RegisterStartupCallback(
		FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FUE5AgentCVarModule::RegisterMenus));
}

void FUE5AgentCVarModule::ShutdownModule()
{
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(this);
	if (UToolMenus* Menus = UToolMenus::Get())
	{
		Menus->UnregisterOwnerByName(TEXT("UE5AgentCVarMenuOwner"));
	}

	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(PanelTabName);
	}

	FUE5AgentCVarStyle::Shutdown();
}

void FUE5AgentCVarModule::RegisterMenus()
{
	// 🔒 Clear stale registrations from prior loads (live-coding safety).
	UToolMenus::Get()->UnregisterOwnerByName(TEXT("UE5AgentCVarMenuOwner"));

	FToolMenuOwnerScoped OwnerScoped(FName(TEXT("UE5AgentCVarMenuOwner")));

	UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	if (!ToolsMenu)
	{
		return;
	}

	FToolMenuSection& Section = ToolsMenu->FindOrAddSection(TEXT("Tools"));

	Section.AddMenuEntry(
		"UE5AgentCVar_OpenPanel",
		LOCTEXT("UE5AgentCVarMenuEntry", "UE5 Agent CVar"),
		LOCTEXT("UE5AgentCVarMenuTooltip", "Open the UE5 Agent CVar assistant."),
		FSlateIcon(FUE5AgentCVarStyle::GetStyleSetName(), "UE5AgentCVar.MenuIcon"),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(FUE5AgentCVarModule::PanelTabName);
		})));
}

TSharedRef<SDockTab> FUE5AgentCVarModule::SpawnPanelTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(LOCTEXT("PanelTabTitle", "UE5 Agent CVar"))
		[
			SNew(SUE5AgentCVarPanel)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUE5AgentCVarModule, UE5AgentCVar)
