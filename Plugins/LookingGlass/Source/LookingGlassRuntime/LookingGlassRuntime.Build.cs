namespace UnrealBuildTool.Rules
{
    using System;
    using System.IO;
    using System.Collections.Generic;

    public class LookingGlassRuntime : ModuleRules
    {
		private string GetThirdPartyPath()
		{
			return Path.GetFullPath(Path.Combine(ModuleDirectory, "../../ThirdParty/"));
		}

        public LookingGlassRuntime(ReadOnlyTargetRules Target) : base(Target)
        {
#if UE_5_4_OR_LATER
			DefaultBuildSettings = BuildSettingsVersion.V5;
#else
			DefaultBuildSettings = BuildSettingsVersion.V3;
#endif

            PCHUsage = PCHUsageMode.NoPCHs;
            bPrecompile = true;
            bEnableExceptions = true;


            PublicIncludePaths.AddRange(
                new string[] {
                    // ... add public include paths required here ...
                }
                );


            PrivateIncludePaths.AddRange(
                new string[] {
                }
                );


            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "InputCore",
                    "Slate",
                    "SlateCore",
                    "MovieSceneCapture",
                    "AVIWriter",
                }
                );


            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "ApplicationCore",
                    "RenderCore",
                    "RHI",
                    "Projects",
                    "HeadMountedDisplay",
                    "ImageWrapper",
                    "EngineSettings",
                    "ImageWriteQueue",
                    "Renderer",
                    "Json",
                }
                );

            if (Target.bBuildEditor == true)
            {
                PrivateDependencyModuleNames.Add("UnrealEd");
				PrivateDependencyModuleNames.Add("Sequencer");
			}

			string BridgeDirectoryName = "LookingGlassBridge";
            PublicIncludePaths.Add(Path.Combine(GetThirdPartyPath(), BridgeDirectoryName, "Include"));
        }
    }
}
