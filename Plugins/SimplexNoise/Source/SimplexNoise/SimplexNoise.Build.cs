// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

namespace UnrealBuildTool.Rules
{
	public class SimplexNoise : ModuleRules
	{
		public SimplexNoise(TargetInfo Target)
		{
			PrivateIncludePaths.AddRange(new string[] {
				"Developer/SimplexNoise/Private",
				});
			PublicDependencyModuleNames.AddRange(new string[] {
				"Core",
				"CoreUObject",
				"Engine"
				});
		}
	}
}