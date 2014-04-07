// Copyright 2014, Andrew Scheidecker. All Rights Reserved. 

namespace UnrealBuildTool.Rules
{
	public class ConsoleAPI : ModuleRules
	{
		public ConsoleAPI(TargetInfo Target)
		{
			PrivateIncludePaths.AddRange(new string[] {
				"Developer/ConsoleAPI/Private"
				});
			PublicDependencyModuleNames.AddRange(new string[] {
				"Core",
				"CoreUObject",
				"Engine"
				});
		}
	}
}