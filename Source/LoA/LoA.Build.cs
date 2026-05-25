// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LoA : ModuleRules
{
	public LoA(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"Niagara",
			"UMG",
			"Slate",
			"ModelViewViewModel"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"LoA",
			"LoA/UI",
			"LoA/Skill",
			"LoA/Variant_Strategy",
			"LoA/Variant_Strategy/UI",
			"LoA/Variant_TwinStick",
			"LoA/Variant_TwinStick/AI",
			"LoA/Variant_TwinStick/Gameplay",
			"LoA/Variant_TwinStick/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
