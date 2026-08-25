// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Runtime/Launch/Resources/Version.h"

#include "LookingGlassSettings.h"
#include "Render/LookingGlassRenderingConfig.h"

#include "LookingGlassSceneCaptureComponent2D.generated.h"

class FSceneInterface;
class UTexture2D;
class ULookingGlassCameraComponent;

/**
 * Internal scene capture component which performs the actual multi-view (quilt) rendering for
 * ULookingGlassCameraComponent.
 *
 * This class used to be the user-facing component of the plugin. All user settings were moved to
 * ULookingGlassCameraComponent (which inherits from UCameraComponent, enabling conventional camera
 * workflows like Sequencer manipulation and Movie Render Queue). This component is now only a thin
 * "capture driver": it provides the scene capture plumbing (per-view view states, show flags, post
 * process settings, hidden/show-only lists, render target) which the low-level rendering code needs.
 *
 * It is created automatically by ULookingGlassCameraComponent - do not add it to actors manually.
 * For content saved with older plugin versions which used this component directly (without the
 * LookingGlassCapture actor), OnRegister creates a transient ULookingGlassCameraComponent from the
 * legacy settings so such setups keep rendering; users should still migrate to the camera component.
 */
UCLASS(hidecategories = (Collision, Object, Physics, SceneComponent, Projection), ClassGroup = Rendering, meta = (DisplayName = "Looking Glass Capture Driver (internal)"))
class LOOKINGGLASSRUNTIME_API ULookingGlassSceneCaptureComponent2D : public USceneCaptureComponent2D
{
	GENERATED_BODY()

public:
	ULookingGlassSceneCaptureComponent2D();

	//~ Begin UActorComponent Interface
	/** All the time we render everything manually, never call deferred rendering */
	virtual void SendRenderTransform_Concurrent() override
	{
		USceneCaptureComponent::SendRenderTransform_Concurrent();
	}

	virtual void OnRegister() override;

	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	//~ End UActorComponent Interface

	//~ Begin USceneCaptureComponent Interface
#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 6)
	virtual void UpdateSceneCaptureContents(FSceneInterface* Scene) override;
#else
	virtual void UpdateSceneCaptureContents(FSceneInterface* Scene, class ISceneRenderBuilder& SceneRenderBuilder) override;
#endif
	//~ End USceneCaptureComponent Interface

	/**
	 * Perform a plain single-view 2D capture into TextureTarget, using CustomProjectionMatrix.
	 * Regular deferred captures of this component are suppressed (see UpdateSceneCaptureContents),
	 * so this is the only way to trigger a 2D capture.
	 */
	void Capture2D();

	/** Render one multi-view rendering config into the component's TextureTarget, using the view infos prepared by the camera */
	static void UpdateLookingGlassSceneCaptureContents(USceneCaptureComponent2D* CaptureComponent, struct FLookingGlassRenderingConfig& RenderingConfig, FSceneInterface* Scene);

	/** Copy settings deserialized from the pre-2.0 component into the user-facing camera. */
	void CopyLegacySettingsTo(ULookingGlassCameraComponent& Camera) const;

public:
	// ---------------------------------------------------------------------------------------------
	// Legacy properties.
	//
	// All the settings below were moved to ULookingGlassCameraComponent. They are kept here as plain,
	// non-editable UPROPERTYs only so that data saved with older plugin versions still deserializes;
	// ALookingGlassCapture migrates them to its camera component on load. Do not use them in new code.
	// ---------------------------------------------------------------------------------------------

	UPROPERTY()
	float Size = 150.0f;

	UPROPERTY()
	float NearClipFactor = 1.0f;

	UPROPERTY()
	bool bUseFarClipPlane = false;

	UPROPERTY()
	float FarClipFactor = 1.5f;

	UPROPERTY()
	float FOV = 14.0f;

	UPROPERTY()
	bool bSingleViewMode = true;

	UPROPERTY()
	UTexture2D* OverrideQuiltTexture2D = nullptr;

	UPROPERTY()
	ELookingGlassQualitySettings TilingQuality = ELookingGlassQualitySettings::Q_Automatic;

	UPROPERTY()
	FLookingGlassTilingQuality TilingValues;

	UPROPERTY()
	bool bOverrideDOF = false;

	UPROPERTY()
	float DOFAdjust = 0.0f;

	UPROPERTY()
	float DepthOfFieldFstop = 1.0f;

	UPROPERTY()
	bool bDrawDebugFocusPlane = false;

	UPROPERTY()
	bool bEnableMotionBlur = false;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FColor FrustumColor = FColor::Blue;

	UPROPERTY()
	FColor MidPlaneFrustumColor = FColor::White;

	UPROPERTY()
	float MidPlaneLineLength = 0.2f;

	UPROPERTY()
	float MidLineThickness;

	UPROPERTY()
	float FrustumLineThickness;

	UPROPERTY()
	FColor DebugFocusPlaneColor = FColor(102, 26, 204, 153);
#endif // WITH_EDITORONLY_DATA

private:
	// Flag telling that UpdateSceneCaptureContents() should pass execution to parent class
	bool bAllow2DCapture = false;

	/** Transient camera auto-created by OnRegister for legacy standalone usage of this component */
	UPROPERTY(Transient)
	ULookingGlassCameraComponent* LegacyOwnerCamera = nullptr;
};
