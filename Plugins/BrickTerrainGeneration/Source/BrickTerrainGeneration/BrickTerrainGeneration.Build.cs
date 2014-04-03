// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class BrickTerrainGeneration : ModuleRules
	{
		public BrickTerrainGeneration(TargetInfo Target)
		{
			PrivateIncludePaths.Add("BrickTerrainGeneration/Private");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"BrickGrid"
				}
				);
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"SimplexNoise"
				}
				);
		}
	}
}
