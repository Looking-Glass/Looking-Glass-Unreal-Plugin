using UnrealBuildTool;

public class LookingGlassEditor : ModuleRules
{
	public LookingGlassEditor(ReadOnlyTargetRules Target) : base(Target)
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

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Json",
				"JsonUtilities",
				"HTTP",
				"DetailCustomizations",
				"DesktopWidgets",
				"MovieSceneCapture",
				"Slate",
				"SlateCore",
                "InputCore",
                "Projects",
				"PropertyEditor",
				"UnrealEd",
				"EditorStyle",
				"MainFrame",
				"LevelEditor",
                "Settings",
#if UE_5_0_OR_LATER
				"ToolMenus",
				"ToolWidgets",
#endif
                "LookingGlassRuntime"
            }
			);
	}
}
