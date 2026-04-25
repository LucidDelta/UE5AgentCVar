// Copyright (c) UE5AgentCVar. All Rights Reserved.

using UnrealBuildTool;

public class UE5AgentCVar : ModuleRules
{
	public UE5AgentCVar(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"InputCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"EditorStyle",
			"EditorWidgets",
			"LevelEditor",
			"ToolMenus",
			"UnrealEd",
			"Projects",
			"ApplicationCore",
			"WorkspaceMenuStructure",
			"EditorFramework",
			"HTTP",
			"Json",
			"JsonUtilities"
		});
	}
}
