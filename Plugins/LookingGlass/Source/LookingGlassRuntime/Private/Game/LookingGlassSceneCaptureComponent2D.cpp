#include "Game/LookingGlassSceneCaptureComponent2D.h"
#include "Game/LookingGlassCameraComponent.h"
#include "Misc/LookingGlassLog.h"

#include "SceneInterface.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/Actor.h"

ULookingGlassSceneCaptureComponent2D::ULookingGlassSceneCaptureComponent2D()
{
	TextureTarget = nullptr;
	SetHiddenInGame(false);
	bCaptureEveryFrame = false;
	bUseCustomProjectionMatrix = true;
	bUseRayTracingIfEnabled = true;
	bAlwaysPersistRenderingState = true;
	CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;

	// Keep the same CDO default as older plugin versions, so legacy TilingValues data (saved as a
	// delta against the CDO) still deserializes correctly for migration
	const ULookingGlassSettings* LookingGlassSettings = GetDefault<ULookingGlassSettings>();
	TilingValues = LookingGlassSettings->_GoPortrait_Settings;
}

void ULookingGlassSceneCaptureComponent2D::OnRegister()
{
	Super::OnRegister();

	// This component is the internal render driver of ULookingGlassCameraComponent. If it is used on
	// its own (a setup from plugin versions before the camera refactoring), warn the user: nothing
	// will be rendered from it anymore.
	if (GetAttachParent() == nullptr || Cast<ULookingGlassCameraComponent>(GetAttachParent()) == nullptr)
	{
		UE_LOG(LookingGlassLogGame, Warning,
			TEXT("%s: ULookingGlassSceneCaptureComponent2D is now an internal component of the LookingGlass plugin. ")
			TEXT("Replace it with a LookingGlassCamera component (or use the LookingGlassCapture actor)."),
			*GetPathName());
	}
}

void ULookingGlassSceneCaptureComponent2D::UpdateSceneCaptureContents(FSceneInterface* Scene)
{
	// This function is called by USceneCaptureComponent2D::CaptureSceneDeferred() and UpdateDeferredCaptures()
	// when any property of this component is changed. This capture is useless because it is executed in a
	// different way, also it causes engine to crash with NULL render target. So, let's just declare an empty
	// implementation. The crash was caused by PostEditChangeProperty or OnRegister calls to parent class.

	if (bAllow2DCapture)
	{
		Super::UpdateSceneCaptureContents(Scene);
	}
}

void ULookingGlassSceneCaptureComponent2D::Capture2D()
{
	bAllow2DCapture = true;
	CaptureScene();
	bAllow2DCapture = false;
}
