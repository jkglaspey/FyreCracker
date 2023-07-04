// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class FyrecrackerEditor : ModuleRules
{
	public FyrecrackerEditor(ReadOnlyTargetRules Target) : base(Target)
	{
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Voxel", "Fyrecracker" });

        PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd" });
    }
}
