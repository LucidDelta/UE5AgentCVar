// Copyright (c) UE5AgentCVar. All Rights Reserved.

#include "UE5AgentCVarStyle.h"

#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateBrush.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

TSharedPtr<FSlateStyleSet> FUE5AgentCVarStyle::StyleInstance = nullptr;

void FUE5AgentCVarStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = Create();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FUE5AgentCVarStyle::Shutdown()
{
	if (StyleInstance.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
		ensure(StyleInstance.IsUnique());
		StyleInstance.Reset();
	}
}

FName FUE5AgentCVarStyle::GetStyleSetName()
{
	static FName Name(TEXT("UE5AgentCVarStyle"));
	return Name;
}

const ISlateStyle& FUE5AgentCVarStyle::Get()
{
	return *StyleInstance;
}

TSharedRef<FSlateStyleSet> FUE5AgentCVarStyle::Create()
{
	TSharedRef<FSlateStyleSet> Style = MakeShared<FSlateStyleSet>(GetStyleSetName());

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("UE5AgentCVar"));
	const FString ResourcesDir = Plugin.IsValid()
		? (Plugin->GetBaseDir() / TEXT("Resources"))
		: FPaths::EngineContentDir();

	Style->Set("UE5AgentCVar.MenuIcon",
		new FSlateVectorImageBrush(ResourcesDir / TEXT("UE5AgentCVar_MenuIcon.svg"), FVector2D(16.f, 16.f)));

	return Style;
}
