#include "LookingGlassSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLookingGlassTilingQualityTest,
	"LookingGlass.Settings.TilingQualityPresets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLookingGlassTilingQualityTest::RunTest(const FString& Parameters)
{
	const ULookingGlassSettings* Settings = GetDefault<ULookingGlassSettings>();
	const UEnum* QualityEnum = StaticEnum<ELookingGlassQualitySettings>();
	if (!TestNotNull(TEXT("Looking Glass settings are available"), Settings) ||
		!TestNotNull(TEXT("Looking Glass quality enum is available"), QualityEnum))
	{
		return false;
	}

	// NumEnums includes UnrealHeaderTool's generated _MAX sentinel as the final entry.
	for (int32 Index = 0; Index < QualityEnum->NumEnums() - 1; ++Index)
	{
		const ELookingGlassQualitySettings Quality =
			static_cast<ELookingGlassQualitySettings>(QualityEnum->GetValueByIndex(Index));
		if (Quality == ELookingGlassQualitySettings::Q_Automatic)
		{
			continue;
		}

		const FLookingGlassTilingQuality Tiling = Settings->GetTilingQualityFor(Quality);
		const FString PresetName = QualityEnum->GetNameStringByIndex(Index);
		TestTrue(
			FString::Printf(TEXT("%s resolves to a named tiling preset"), *PresetName),
			Tiling.Name != TEXT("Default"));
		TestTrue(
			FString::Printf(TEXT("%s has positive quilt and tile dimensions"), *PresetName),
			Tiling.TilesX > 0 && Tiling.TilesY > 0 && Tiling.QuiltW > 0 && Tiling.QuiltH > 0);
	}

	const FLookingGlassTilingQuality Portrait =
		Settings->GetTilingQualityFor(ELookingGlassQualitySettings::Q_27_Portrait);
	TestEqual(TEXT("27-inch portrait quilt width"), Portrait.QuiltW, 7680);
	TestEqual(TEXT("27-inch portrait quilt height"), Portrait.QuiltH, 4320);
	TestEqual(TEXT("27-inch portrait columns"), Portrait.TilesX, 12);
	TestEqual(TEXT("27-inch portrait rows"), Portrait.TilesY, 4);
	TestEqual(TEXT("27-inch portrait tile width"), Portrait.TileSizeX, 640);
	TestEqual(TEXT("27-inch portrait tile height"), Portrait.TileSizeY, 1080);
	TestTrue(TEXT("27-inch portrait uses the full quilt width"), FMath::IsNearlyEqual(Portrait.PortionX, 1.0f));
	TestTrue(TEXT("27-inch portrait uses the full quilt height"), FMath::IsNearlyEqual(Portrait.PortionY, 1.0f));

	const FLookingGlassTilingQuality Landscape =
		Settings->GetTilingQualityFor(ELookingGlassQualitySettings::Q_27_Landscape);
	TestEqual(TEXT("27-inch landscape quilt width"), Landscape.QuiltW, 7680);
	TestEqual(TEXT("27-inch landscape quilt height"), Landscape.QuiltH, 4320);
	TestEqual(TEXT("27-inch landscape columns"), Landscape.TilesX, 8);
	TestEqual(TEXT("27-inch landscape rows"), Landscape.TilesY, 6);
	TestEqual(TEXT("27-inch landscape tile width"), Landscape.TileSizeX, 960);
	TestEqual(TEXT("27-inch landscape tile height"), Landscape.TileSizeY, 720);
	TestTrue(TEXT("27-inch landscape uses the full quilt width"), FMath::IsNearlyEqual(Landscape.PortionX, 1.0f));
	TestTrue(TEXT("27-inch landscape uses the full quilt height"), FMath::IsNearlyEqual(Landscape.PortionY, 1.0f));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
