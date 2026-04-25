// Copyright (c) UE5AgentCVar. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FSpawnTabArgs;
class SDockTab;

class FUE5AgentCVarModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static const FName PanelTabName;

private:
	void RegisterMenus();
	TSharedRef<SDockTab> SpawnPanelTab(const FSpawnTabArgs& Args);
};
