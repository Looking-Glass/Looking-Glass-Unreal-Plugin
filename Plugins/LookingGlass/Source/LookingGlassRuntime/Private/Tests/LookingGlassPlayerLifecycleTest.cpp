#include "ILookingGlassRuntime.h"
#include "LookingGlassSettings.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FLookingGlassPlayerLifecycleTest,
	"LookingGlass.Runtime.PlayerLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLookingGlassPlayerLifecycleTest::RunTest(const FString& Parameters)
{
	ILookingGlassRuntime& Runtime = ILookingGlassRuntime::Get();
	ULookingGlassSettings* Settings = GetMutableDefault<ULookingGlassSettings>();
	if (!TestNotNull(TEXT("Looking Glass settings are available"), Settings))
	{
		return false;
	}

	struct FScopedWindowSettingsRestore
	{
		ULookingGlassSettings* Settings;
		FLookingGlassWindowSettings SavedSettings;

		~FScopedWindowSettingsRestore()
		{
			Settings->LookingGlassWindowSettings = SavedSettings;
		}
	};

	const FScopedWindowSettingsRestore RestoreSettings{Settings, Settings->LookingGlassWindowSettings};
	Settings->LookingGlassWindowSettings.PlacementMode = ELookingGlassPlacement::AlwaysDebugWindow;
	// StopPlayer otherwise redirects to main-viewport teardown and skips the separate-window
	// lifecycle this test exists to cover.
	Settings->LookingGlassWindowSettings.bLockInMainViewport = false;

	// Begin from a known state in case editor startup or another test left playback active.
	Runtime.StopPlayer();
	for (int32 Cycle = 1; Cycle <= 2; ++Cycle)
	{
		Runtime.StartPlayer(ELookingGlassModeType::PlayMode_InSeparateWindow);
		TestTrue(FString::Printf(TEXT("Cycle %d starts playback"), Cycle), Runtime.IsPlaying());
		TestTrue(
			FString::Printf(TEXT("Cycle %d creates a viewport"), Cycle),
			Runtime.GetLookingGlassViewport().IsValid());
		TestFalse(
			FString::Printf(TEXT("Cycle %d re-arms window destruction"), Cycle),
			Runtime.IsDestroyWindowRequested());

		Runtime.StopPlayer();
		TestFalse(FString::Printf(TEXT("Cycle %d stops playback"), Cycle), Runtime.IsPlaying());
		TestFalse(
			FString::Printf(TEXT("Cycle %d releases the viewport"), Cycle),
			Runtime.GetLookingGlassViewport().IsValid());
		TestTrue(
			FString::Printf(TEXT("Cycle %d tears down the separate window"), Cycle),
			Runtime.IsDestroyWindowRequested());
	}

	// StopPlayer must remain idempotent after the session has already been torn down.
	Runtime.StopPlayer();
	TestFalse(TEXT("Repeated stop leaves playback inactive"), Runtime.IsPlaying());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
