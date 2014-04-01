// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

namespace UnrealBuildTool.Rules
{
	public class SuperLoop : ModuleRules
	{
		public SuperLoop(TargetInfo Target)
		{
			PrivateIncludePaths.AddRange(new string[] {
				"Developer/SuperLoop/Private"
				});
			PublicDependencyModuleNames.AddRange(new string[] {
				"Core",
				"CoreUObject",
				"Engine"
				});
		}
	}
}