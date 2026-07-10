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

	// When attached under a ULookingGlassCameraComponent this component acts as its internal render
	// driver and needs no further setup. Transient instances are always such drivers.
	if (Cast<ULookingGlassCameraComponent>(GetAttachParent()) != nullptr || HasAnyFlags(RF_Transient))
	{
		return;
	}

	// Guard against re-registration (e.g. FComponentReregisterContext cycles). If the previously
	// created camera has been destroyed, fall through and create a fresh one.
	if (IsValid(LegacyOwnerCamera))
	{
		return;
	}
	LegacyOwnerCamera = nullptr;

	AActor* MyOwner = GetOwner();
	UWorld* World = GetWorld();
	if (MyOwner == nullptr || World == nullptr)
	{
		return;
	}

	// Legacy standalone usage: a setup from plugin versions before the camera refactoring, where
	// this component was placed directly on an actor. Keep such content rendering by creating a
	// transient LookingGlassCamera from the legacy settings and acting as its capture driver.
	UE_LOG(LookingGlassLogGame, Warning,
		TEXT("%s: ULookingGlassSceneCaptureComponent2D is now an internal component of the LookingGlass plugin. ")
		TEXT("A transient LookingGlassCamera has been created from its settings to keep this actor rendering. ")
		TEXT("Please replace this component with a LookingGlassCamera component (or use the LookingGlassCapture actor)."),
		*GetPathName());

	ULookingGlassCameraComponent* Camera = NewObject<ULookingGlassCameraComponent>(MyOwner, NAME_None, RF_Transient | RF_TextExportTransient);

	CopyLegacySettingsTo(*Camera);

	// Place the camera at this component's transform, and make this component its capture driver
	USceneComponent* CameraParent = GetAttachParent() != nullptr ? GetAttachParent() : this;
	Camera->SetupAttachment(CameraParent);
	if (CameraParent != this)
	{
		Camera->SetRelativeTransform(GetRelativeTransform());
	}
	Camera->SetCaptureDriver(this);
	Camera->CreationMethod = CreationMethod;
	Camera->RegisterComponentWithWorld(World);

	LegacyOwnerCamera = Camera;
}

void ULookingGlassSceneCaptureComponent2D::CopyLegacySettingsTo(ULookingGlassCameraComponent& Camera) const
{
	Camera.Size = Size;
	Camera.NearClipFactor = NearClipFactor;
	Camera.bUseFarClipPlane = bUseFarClipPlane;
	Camera.FarClipFactor = FarClipFactor;
	Camera.FieldOfView = FOV;
	Camera.bSingleViewMode = bSingleViewMode;
	Camera.OverrideQuiltTexture2D = OverrideQuiltTexture2D;
	Camera.TilingQuality = TilingQuality;
	Camera.TilingValues = TilingValues;
	Camera.bOverrideDOF = bOverrideDOF;
	Camera.DOFAdjust = DOFAdjust;
	Camera.DepthOfFieldFstop = DepthOfFieldFstop;
	Camera.bDrawDebugFocusPlane = bDrawDebugFocusPlane;
	Camera.bEnableMotionBlur = bEnableMotionBlur;
	Camera.PostProcessSettings = PostProcessSettings;
	Camera.PostProcessBlendWeight = PostProcessBlendWeight;
#if WITH_EDITORONLY_DATA
	Camera.FrustumColor = FrustumColor;
	Camera.MidPlaneFrustumColor = MidPlaneFrustumColor;
	Camera.MidPlaneLineLength = MidPlaneLineLength;
	Camera.MidLineThickness = MidLineThickness;
	Camera.FrustumLineThickness = FrustumLineThickness;
	Camera.DebugFocusPlaneColor = DebugFocusPlaneColor;
#endif // WITH_EDITORONLY_DATA
}

void ULookingGlassSceneCaptureComponent2D::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);

	// Take the auto-created legacy camera down with us
	if (LegacyOwnerCamera != nullptr && !LegacyOwnerCamera->IsBeingDestroyed())
	{
		LegacyOwnerCamera->DestroyComponent();
	}
	LegacyOwnerCamera = nullptr;
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
