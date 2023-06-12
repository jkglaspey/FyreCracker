// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;

public class Fyrecracker : ModuleRules
{
	public Fyrecracker(ReadOnlyTargetRules Target) : base(Target)
	{
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "Voxel" });

        //TODO this is needed for the factory class, but why?
        PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd" });
    }
}
