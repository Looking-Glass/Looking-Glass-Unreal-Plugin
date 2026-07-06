// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraComponent.h"

#include "LookingGlassSettings.h"
#include "Render/LookingGlassRenderingConfig.h"

#include "LookingGlassCameraComponent.generated.h"

class UMaterial;
class UMaterialInstanceDynamic;
class UStaticMesh;
class UStaticMeshComponent;
class UTexture2D;
class UTextureRenderTarget2D;
class ULookingGlassDrawFrustumComponent;
class ULookingGlassSceneCaptureComponent2D;

/**
 * The Looking Glass camera. This is the user-facing component of the plugin: it inherits from
 * UCameraComponent, so it supports all conventional camera workflows - possessing it from a
 * Camera Cuts track, piloting it in the viewport, and animating transform and FieldOfView in
 * Sequencer. Movie Render Queue (including Path Tracing) can render through it like through any
 * other camera; note that MRQ currently outputs the standard single camera view, not the quilt.
 *
 * The Looking Glass quilt (multi-view) rendering is performed through an internal, transient
 * ULookingGlassSceneCaptureComponent2D "capture driver" which this component creates and keeps
 * in sync with the camera settings. Rendering quilts through Movie Render Queue itself is a
 * planned follow-up; see the todo on RenderViews() for the intended integration point.
 */
UCLASS(ClassGroup = Rendering, editinlinenew, meta = (BlueprintSpawnableComponent, DisplayName = "Looking Glass Camera"), hidecategories = (Collision, Object, Physics))
class LOOKINGGLASSRUNTIME_API ULookingGlassCameraComponent : public UCameraComponent
{
	GENERATED_BODY()

public:
	ULookingGlassCameraComponent();

	// UObject garbage collector helper
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	virtual void PostInterpChange(FProperty* PropertyThatChanged) override;

#if WITH_EDITOR
	void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	//~ Begin UActorComponent Interface

	// Called when component is created
	virtual void OnRegister() override;

	// Called when component is destroyed
	virtual void OnUnregister() override;

	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Called when the game ends
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End UActorComponent Interface

	//~ Begin UCameraComponent Interface
	virtual void GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView) override;
	//~ End UCameraComponent Interface

	/** Top-level rendering function for making a hologram picture */
	void RenderViews();

	/** Top-level rendering function for making a 2D picture */
	void Render2DView(int32 SizeX = -1, int32 SizeY = -1);

	// Change current tiling settings and do all the required refresh work
	void SetTilingProperties(ELookingGlassQualitySettings InTilingQuailty);

	// Calls UpdateTilingProperties for all existing components
	static void UpdateTilingPropertiesForAllComponents();

	// Refresh current display tiling settings
	void UpdateTilingProperties();

	// Get ELookingGlassQualitySettings depending on the device type
	static ELookingGlassQualitySettings GetAutomaticTilingQuality();

	const FLookingGlassTilingQuality& GetTilingValues() { return TilingValues; }

	float GetAspectRatio() const;

	UTextureRenderTarget2D* GetTextureTarget2DRendering() const { return TextureTarget2DRendering; }

	//todo: remove?
	UTexture2D* GetOverrideQuiltTexture2D() { return OverrideQuiltTexture2D; }

	// Distance from the camera to the focal (zero-parallax) plane, derived from Size and FieldOfView
	float GetCameraDistance() const;

	// FieldOfView clamped to the range supported by the hologram rendering math
	float GetLookingGlassFOV() const;

	const FLookingGlassRenderingConfigs& GetRenderingConfigs() const { return RenderingConfigs; }

	static void SetGlobalTilingProperties(ELookingGlassQualitySettings InTilingQuailty);

	static void ResetGlobalTilingProperties();

	// The internal scene capture component used for quilt rendering. Created on demand.
	ULookingGlassSceneCaptureComponent2D* GetCaptureDriver() const { return CaptureDriver; }

	// Explicitly assign the capture driver. Used by the legacy standalone
	// ULookingGlassSceneCaptureComponent2D auto-migration (see its OnRegister).
	void SetCaptureDriver(ULookingGlassSceneCaptureComponent2D* InDriver) { CaptureDriver = InDriver; }

public:
	// Vertical size of the capture region in Unreal units (cm)
	UPROPERTY(Interp, BlueprintSetter = SetSize, EditAnywhere, BlueprintReadWrite, Category = "CaptureSettings", meta = (ClampMin = "0.1", UIMin = "0.1", UIMax = "2000.0"))
	float Size = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptureSettings", meta = (ClampMin = "0"))
	float NearClipFactor = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptureSettings")
	bool bUseFarClipPlane = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptureSettings",  meta = (EditCondition = "bUseFarClipPlane", ClampMin = "0"))
	float FarClipFactor = 1.5f;

	// Note: there's no LookingGlass-specific "FOV" property anymore. The horizontal field of view
	// angle of the camera is the standard UCameraComponent::FieldOfView property, so it can be
	// animated with a regular camera FOV track in Sequencer and picked up by Movie Render Queue.

	// When disabled, the plugin will render 8 pictures at a time. Enable it if you're experiencing issues with rendering (e.g. out of VRAM problem), with a cost of slower performance.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TilingSettings")
	bool bSingleViewMode = true;

	// A static replacement for Quilt image.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "QuiltSettings")
	UTexture2D* OverrideQuiltTexture2D = nullptr;

	// Customizable Tiling Settings
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TilingSettings")
	ELookingGlassQualitySettings TilingQuality = ELookingGlassQualitySettings::Q_Automatic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TilingSettings", meta = (ShowOnlyInnerProperties))
	FLookingGlassTilingQuality TilingValues;

	// Customized TilingValues for Q_Custom.
	FLookingGlassTilingQuality CustomTilingValues;

	// Force Depth of Field effect to be focused on LookingGlassCapture location
	UPROPERTY(Interp, EditAnywhere, BlueprintReadOnly, Category = "PostProcessing")
	bool bOverrideDOF = false;

	// Distance adjustment for OverrideDOF mode
	UPROPERTY(Interp, EditAnywhere, BlueprintReadOnly, Category = "PostProcessing", meta = (editcondition = "bOverrideDOF"))
	float DOFAdjust = 0.0f;

	// Focal depth for OverrideDOF mode. Lower values makes more blurry images
	UPROPERTY(Interp, EditAnywhere, BlueprintReadOnly, Category = "PostProcessing", meta = (ClampMin = "0.1", ClampMax = "32.0", DisplayName = "Aperture (DoF strength)", editcondition = "bOverrideDOF"))
	float DepthOfFieldFstop = 1.0f;

	// True to draw a translucent plane at the current focus depth, for easy tweaking.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PostProcessing", meta = (editcondition = "bOverrideDOF"))
	bool bDrawDebugFocusPlane = false;

	// Allow to render motion blur based on PostProcessingVolume settings
	UPROPERTY(Interp, EditAnywhere, BlueprintReadOnly, Category = "PostProcessing")
	bool bEnableMotionBlur = false;

	// Container for rendering targets, plus viewport settings for each.
	FLookingGlassRenderingConfigs RenderingConfigs;

	/** Render target for 2D rendering camera. */
	UPROPERTY(transient)
	UTextureRenderTarget2D* TextureTarget2DRendering = nullptr;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptureSettings|Visual")
	FColor FrustumColor = FColor::Blue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptureSettings|Visual")
	FColor MidPlaneFrustumColor = FColor::White;

	// MidPlane Line Length for Frustum Component
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptureSettings|Visual", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
	float MidPlaneLineLength = 0.2f;

	// MidPlane Line Thickness for Frustum Component
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptureSettings|Visual", meta = (ClampMin = "0.0", ClampMax = "10.0", UIMin = "0.0", UIMax = "10.0"))
	float MidLineThickness;

	// Frustum Line Thickness for Frustum Component
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptureSettings|Visual", meta = (ClampMin = "0.0", ClampMax = "10.0", UIMin = "0.0", UIMax = "10.0"))
	float FrustumLineThickness;

	// For customizing the focus plane color, in case the default doesn't show up well in your scene.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CaptureSettings|Visual", meta = (EditCondition = "bDrawDebugFocusPlane"))
	FColor DebugFocusPlaneColor = FColor(102, 26, 204, 153);
#endif // WITH_EDITORONLY_DATA

	DECLARE_DELEGATE_OneParam(FLookingGlassObjectChangedDelegate, ULookingGlassCameraComponent*);
	FLookingGlassObjectChangedDelegate OnLookingGlassObjectChanged;

	UFUNCTION(BlueprintCallable, BlueprintSetter, Category = "LookingGlass")
	void SetSize(float InSize);

	UFUNCTION(BlueprintCallable, Category = "LookingGlass")
	void SetActiveCaptureComponent();

	// Recompute clipping planes / frustum visualization after a change of Size or FieldOfView
	void UpdateCameraPosition();

private:

#if WITH_EDITORONLY_DATA
	// The frustum component used to show visually where the camera field of view is
	ULookingGlassDrawFrustumComponent* DrawFrustum;

	UPROPERTY(transient)
	UStaticMesh* FocusPlaneVisualizationMesh;

	/** Material used for debug focus plane visualization */
	UPROPERTY(transient)
	UMaterial* FocusPlaneVisualizationMaterial;

	/** Component for the debug focus plane visualization */
	UPROPERTY(transient)
	UStaticMeshComponent* DebugFocusPlaneComponent;

	/** Dynamic material instance for the debug focus plane visualization */
	UPROPERTY(transient)
	UMaterialInstanceDynamic* DebugFocusPlaneMID;

	void RefreshVisualRepresentation();

	void CreateDebugFocusPlane();
	void DestroyDebugFocusPlane();
	void UpdateDebugFocusPlane();
#endif // WITH_EDITORONLY_DATA

	/** Internal scene capture component performing the actual multi-view (quilt) rendering.
	 *  Either found among attached children (legacy "RenderCamera" subobject of ALookingGlassCapture),
	 *  or created as a transient component. */
	UPROPERTY(transient)
	ULookingGlassSceneCaptureComponent2D* CaptureDriver = nullptr;

	// Find or create CaptureDriver
	void EnsureCaptureDriver();

	// Push all camera-derived settings (post processing, show flags) into CaptureDriver before rendering
	void SetupCaptureDriver();

	// Apply the Looking Glass DOF override to a copy of the camera post process settings
	void ApplyDOFPostProcessSettings(FPostProcessSettings& InOutSettings) const;

	/**
	 * Generates a projection matrix
	 *
	 * @param	OffsetX	The offset x coordinate.
	 * @param	OffsetY	The offset y coordinate.
	 *
	 * @returns	The projection matrix.
	 */
	FMatrix GenerateProjectionMatrix(float OffsetX, float OffsetY) const;

	/**
	 * Render the scene to the texture target immediately.
	 */
	void CaptureLookingGlassScene(struct FLookingGlassRenderingConfig& RenderingConfig);

	void RebuildRenderConfigs()
	{
		TilingValues.Setup();
		RenderingConfigs.Build(TilingValues, bSingleViewMode);
	}

	float NearClipPlane = 0.f;

	float FarClipPlane = 0.f;
};
