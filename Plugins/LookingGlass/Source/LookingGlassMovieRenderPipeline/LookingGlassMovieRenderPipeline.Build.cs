namespace UnrealBuildTool.Rules
{
    public class LookingGlassMovieRenderPipeline : ModuleRules
    {
        public LookingGlassMovieRenderPipeline(ReadOnlyTargetRules Target) : base(Target)
        {
#if UE_5_4_OR_LATER
            DefaultBuildSettings = BuildSettingsVersion.V5;
#else
            DefaultBuildSettings = BuildSettingsVersion.V3;
#endif

            PCHUsage = PCHUsageMode.NoPCHs;

#if !UE_5_5_OR_LATER
            // See LookingGlassRuntime.Build.cs: avoid UE 5.3/5.4's broken modern-MSVC ASan probe.
            if (Target.Platform == UnrealTargetPlatform.Win64)
            {
                PrivateDefinitions.Add("PLATFORM_HAS_ASAN_INCLUDE=0");
            }
#endif

            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "LookingGlassRuntime",
                    "MovieRenderPipelineCore",
                }
                );

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "RenderCore",
                    "RHI",
                    "Renderer",
                    "ImageWriteQueue",
                }
                );
        }
    }
}
