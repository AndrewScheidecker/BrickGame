// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class BrickGrid : ModuleRules
	{
        public BrickGrid(ReadOnlyTargetRules Target) : base(Target)
        {
			PrivateIncludePaths.Add("BrickGrid/Private");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"RenderCore",
					"ShaderCore",
					"RHI"
				}
				);
		}
	}
}
