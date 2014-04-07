// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

namespace UnrealBuildTool.Rules
{
	public class SuperLoopLibrary : ModuleRules
	{
		public SuperLoopLibrary(TargetInfo Target)
		{
			PrivateIncludePaths.AddRange(new string[] {
				"Developer/SuperLoopLibrary/Private"
				});
			PublicDependencyModuleNames.AddRange(new string[] {
				"Core",
				"CoreUObject",
				"Engine"
				});
		}
	}
}