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

            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "MovieRenderPipelineCore",
                }
                );

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "CoreUObject",
                    "Engine",
                    "RenderCore",
                    "RHI",
                    "Renderer",
                    "ImageWriteQueue",
                    "LookingGlassRuntime",
                }
                );
        }
    }
}
