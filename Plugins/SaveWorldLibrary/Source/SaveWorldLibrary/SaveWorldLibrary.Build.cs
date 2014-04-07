// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

namespace UnrealBuildTool.Rules
{
	public class SaveWorldLibrary : ModuleRules
	{
		public SaveWorldLibrary(TargetInfo Target)
		{
			PrivateIncludePaths.AddRange(new string[] {
				"Developer/SaveWorldLibrary/Private"
				});
			PublicDependencyModuleNames.AddRange(new string[] {
				"Core",
				"CoreUObject",
				"Engine"
				});
		}
	}
}