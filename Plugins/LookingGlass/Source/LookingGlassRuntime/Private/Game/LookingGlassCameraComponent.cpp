// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/LookingGlassCameraComponent.h"
#include "Game/LookingGlassSceneCaptureComponent2D.h"
#include "Game/LookingGlassDrawFrustumComponent.h"
#include "LookingGlassBridge.h"
#include "LookingGlassSettings.h"
#include "Misc/LookingGlassLog.h"
#include "Misc/LookingGlassStats.h"
#include "ILookingGlassRuntime.h" // for Editor/GameLookingGlassCameras

#include "SceneInterface.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Math/UnrealMathUtility.h"
#include "Engine/TextureRenderTarget2D.h"

// For focus plane mesh and component
#include "UObject/ConstructorHelpers.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/CollisionProfile.h"

#include "Runtime/Launch/Resources/Version.h"

static ELookingGlassQualitySettings GlobalTilingQuailty = ELookingGlassQualitySettings::Q_GoPortrait;
static bool IsGlobalTilingQualitySet = false;

ULookingGlassCameraComponent::ULookingGlassCameraComponent()
	: RenderingConfigs(this)
{
	// Default Looking Glass camera setup. Note: FieldOfView is the standard camera FOV property,
	// LookingGlass content traditionally uses a narrow 14 degrees angle.
	FieldOfView = 14.0f;
	// Match the camera preview (and Movie Render Queue output) aspect to the Looking Glass device
	bConstrainAspectRatio = true;

#if WITH_EDITORONLY_DATA
	DrawFrustum = nullptr;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/ArtTools/RenderToTexture/Meshes/S_1_Unit_Plane.S_1_Unit_Plane"));
	FocusPlaneVisualizationMesh = PlaneMesh.Object;

	static ConstructorHelpers::FObjectFinder<UMaterial> PlaneMat(TEXT("/Engine/EngineDebugMaterials/M_SimpleUnlitTranslucent.M_SimpleUnlitTranslucent"));
	FocusPlaneVisualizationMaterial = PlaneMat.Object;
#endif // WITH_EDITORONLY_DATA

	// Init Config Values
	const ULookingGlassSettings* LookingGlassSettings = GetDefault<ULookingGlassSettings>();
	TilingValues = LookingGlassSettings->_GoPortrait_Settings;
	CustomTilingValues = LookingGlassSettings->CustomSettings;
	AspectRatio = TilingValues.Aspect > 0.f ? TilingValues.Aspect : 1.0f;
}

void ULookingGlassCameraComponent::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	ULookingGlassCameraComponent* This = CastChecked<ULookingGlassCameraComponent>(InThis);
	This->RenderingConfigs.AddReferencedObjects(Collector);
	Super::AddReferencedObjects(InThis, Collector);
}

void ULookingGlassCameraComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITORONLY_DATA
	if (DrawFrustum == nullptr)
	{
		if (AActor* MyOwner = GetOwner())
		{
			// Note: if we'll use SetIsVisualizationComponent(true), this component will be hidden in actor's component tree.
			// Things which affects representation in tree:
			// - RF_Transactional | RF_TextExportTransient in NewObject
			// - SetIsVisualizationComponent(true)
			DrawFrustum = NewObject<ULookingGlassDrawFrustumComponent>(MyOwner, NAME_None, RF_Transactional | RF_TextExportTransient);
			DrawFrustum->SetupAttachment(this);
			DrawFrustum->SetIsVisualizationComponent(true);
			DrawFrustum->CreationMethod = CreationMethod;
			DrawFrustum->RegisterComponentWithWorld(GetWorld());
		}
	}
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	if (GIsEditor)
	{
		ILookingGlassRuntime::Get().EditorLookingGlassCameras.Insert(this, 0);
		// Reset scale if it was changed by property editor
		SetRelativeScale3D(FVector::OneVector);
	}

	RefreshVisualRepresentation();
#endif // WITH_EDITOR

	EnsureCaptureDriver();

	UpdateTilingProperties();
}

void ULookingGlassCameraComponent::OnUnregister()
{
	Super::OnUnregister();

#if WITH_EDITOR
	if (GIsEditor)
	{
		ILookingGlassRuntime::Get().EditorLookingGlassCameras.Remove(this);
	}
#endif // WITH_EDITOR
}

void ULookingGlassCameraComponent::SetActiveCaptureComponent()
{
	// Re-insert this component to be first in LookingGlass components list
	auto& LookingGlassRuntime = ILookingGlassRuntime::Get();

	LookingGlassRuntime.GameLookingGlassCameras.Remove(this);
	LookingGlassRuntime.GameLookingGlassCameras.Insert(this, 0);

#if WITH_EDITOR
	if (GIsEditor)
	{
		LookingGlassRuntime.EditorLookingGlassCameras.Remove(this);
		LookingGlassRuntime.EditorLookingGlassCameras.Insert(this, 0);
	}
#endif // WITH_EDITOR
}

void ULookingGlassCameraComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Super::OnComponentDestroyed(bDestroyingHierarchy);

	RenderingConfigs.Release();

#if WITH_EDITORONLY_DATA
	if (DrawFrustum)
	{
		DrawFrustum->DestroyComponent();
	}
#endif

	// Destroy the capture driver only if we own it (created as transient); the legacy
	// "RenderCamera" subobject of ALookingGlassCapture is destroyed along with its actor.
	if (CaptureDriver != nullptr && CaptureDriver->HasAnyFlags(RF_Transient) && !CaptureDriver->IsBeingDestroyed())
	{
		CaptureDriver->DestroyComponent();
	}
	CaptureDriver = nullptr;
}

void ULookingGlassCameraComponent::EnsureCaptureDriver()
{
	if (IsValid(CaptureDriver))
	{
		return;
	}

	AActor* MyOwner = GetOwner();
	if (MyOwner == nullptr)
	{
		return;
	}

	// Reuse a capture component which is already attached to this camera. This is the case for
	// ALookingGlassCapture, which keeps its legacy "RenderCamera" subobject as the capture driver
	// (and for loading data saved with older plugin versions).
	for (UActorComponent* Component : MyOwner->GetComponents())
	{
		if (ULookingGlassSceneCaptureComponent2D* Existing = Cast<ULookingGlassSceneCaptureComponent2D>(Component))
		{
			if (Existing->GetAttachParent() == this)
			{
				CaptureDriver = Existing;
				return;
			}
		}
	}

	CaptureDriver = NewObject<ULookingGlassSceneCaptureComponent2D>(MyOwner, NAME_None, RF_Transient | RF_TextExportTransient);
	CaptureDriver->SetupAttachment(this);
	CaptureDriver->CreationMethod = CreationMethod;
	CaptureDriver->RegisterComponentWithWorld(GetWorld());
}

#if WITH_EDITORONLY_DATA
void ULookingGlassCameraComponent::RefreshVisualRepresentation()
{
	if (DrawFrustum != nullptr)
	{
		float Aspect = GetAspectRatio();

		DrawFrustum->FrustumAngle = GetLookingGlassFOV();
		DrawFrustum->FrustumStartDist = NearClipPlane;
		DrawFrustum->FrustumMidDist = GetCameraDistance();
		DrawFrustum->FrustumEndDist = FarClipPlane;
		DrawFrustum->VerticalAngle = 0.0f;
		DrawFrustum->FrustumAspectRatio = Aspect;
		DrawFrustum->MidPlaneLineLength = MidPlaneLineLength;
		DrawFrustum->FrustumLineThickness = FrustumLineThickness;
		DrawFrustum->MidLineThickness = MidLineThickness;
		DrawFrustum->MarkRenderStateDirty();
		DrawFrustum->FrustumColor = FrustumColor;
		DrawFrustum->MidFrustumColor = MidPlaneFrustumColor;
	}
}

void ULookingGlassCameraComponent::CreateDebugFocusPlane()
{
	if (AActor* const MyOwner = GetOwner())
	{
		if (DebugFocusPlaneComponent == nullptr)
		{
			DebugFocusPlaneComponent = NewObject<UStaticMeshComponent>(MyOwner, NAME_None, RF_Transient | RF_Transactional | RF_TextExportTransient);
			DebugFocusPlaneComponent->SetupAttachment(this);
			DebugFocusPlaneComponent->SetIsVisualizationComponent(true);
			DebugFocusPlaneComponent->SetStaticMesh(FocusPlaneVisualizationMesh);
			DebugFocusPlaneComponent->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
			DebugFocusPlaneComponent->bHiddenInGame = false;
			DebugFocusPlaneComponent->CastShadow = false;
			DebugFocusPlaneComponent->CreationMethod = CreationMethod;
			DebugFocusPlaneComponent->bSelectable = false;

			DebugFocusPlaneComponent->SetRelativeScale3D_Direct(FVector(10000.f, 10000.f, 1.f));
			DebugFocusPlaneComponent->SetRelativeRotation_Direct(FRotator(90.f, 0.f, 0.f));

			DebugFocusPlaneComponent->RegisterComponentWithWorld(GetWorld());

			DebugFocusPlaneMID = DebugFocusPlaneComponent->CreateAndSetMaterialInstanceDynamicFromMaterial(0, FocusPlaneVisualizationMaterial);
			if (DebugFocusPlaneMID)
			{
				DebugFocusPlaneMID->SetVectorParameterValue(FName(TEXT("Color")), DebugFocusPlaneColor.ReinterpretAsLinear());
			}
		}
	}
}

void ULookingGlassCameraComponent::DestroyDebugFocusPlane()
{
	if (DebugFocusPlaneComponent)
	{
		DebugFocusPlaneComponent->SetVisibility(false);
		DebugFocusPlaneComponent = nullptr;

		DebugFocusPlaneMID = nullptr;
	}
}

void ULookingGlassCameraComponent::UpdateDebugFocusPlane()
{
	if (FocusPlaneVisualizationMesh && DebugFocusPlaneComponent)
	{
		FVector const CamDir = GetComponentTransform().GetRotation().Vector();

		float const FocusDistance = GetCameraDistance() + DOFAdjust;
		FVector const FocusPoint = GetComponentTransform().GetLocation() + CamDir * FocusDistance;

		DebugFocusPlaneComponent->SetWorldLocation(FocusPoint);
	}
}
#endif // WITH_EDITORONLY_DATA

void ULookingGlassCameraComponent::BeginPlay()
{
	Super::BeginPlay();

	// Initialize TilingQuality (important when it's set to "Automatic")
	UpdateTilingProperties();

	ILookingGlassRuntime::Get().GameLookingGlassCameras.Insert(this, 0);
}

// Called when the game ends
void ULookingGlassCameraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	// It should automatically clean all render texture targets
	RenderingConfigs.Release();

	ILookingGlassRuntime::Get().GameLookingGlassCameras.Remove(this);
}

void ULookingGlassCameraComponent::GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView)
{
	Super::GetCameraView(DeltaTime, DesiredView);

	// Apply the Looking Glass DOF override to the resulting view only. The component's stored
	// PostProcessSettings are never touched, so ordinary camera DOF setups (Sequencer, Movie
	// Render Queue) keep working when bOverrideDOF is disabled.
	if (bOverrideDOF)
	{
		GetEffectivePostProcessSettings(DesiredView.PostProcessSettings, DesiredView.PostProcessBlendWeight);
	}
}

/**
 * This is called when property is modified by InterpPropertyTracks
 *
 * @param PropertyThatChanged	Property that changed
 */
void ULookingGlassCameraComponent::PostInterpChange(FProperty* PropertyThatChanged)
{
	// Note: deliberately not calling Super::PostInterpChange() - it's a deprecated no-op left over
	// from Matinee, which was removed in UE 5.0.
	const FName PropertyName = PropertyThatChanged->GetFName();
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ULookingGlassCameraComponent, Size) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ULookingGlassCameraComponent, FieldOfView))
	{
		UpdateCameraPosition();
		OnLookingGlassObjectChanged.ExecuteIfBound(this);
	}
}

// This function is called when "Size" property is changed in blueprint or interpolated by movie sequence
void ULookingGlassCameraComponent::SetSize(float InSize)
{
	Size = InSize;
	UpdateCameraPosition();
#if WITH_EDITORONLY_DATA
	UpdateDebugFocusPlane();
#endif
	OnLookingGlassObjectChanged.ExecuteIfBound(this);
}

#if WITH_EDITOR

void ULookingGlassCameraComponent::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property != nullptr)
	{
		FName PropertyName = PropertyChangedEvent.Property->GetFName();

		if (PropertyName == GET_MEMBER_NAME_CHECKED(ULookingGlassCameraComponent, TilingQuality))
		{
			UpdateTilingProperties();
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(FLookingGlassTilingQuality, TilesX) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(FLookingGlassTilingQuality, TilesY) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(FLookingGlassTilingQuality, QuiltW) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(FLookingGlassTilingQuality, QuiltH) ||
			PropertyName == GET_MEMBER_NAME_CHECKED(ULookingGlassCameraComponent, bSingleViewMode)
			)
		{
			// Reset our render textures and configuration after it
			RebuildRenderConfigs();
		}

		// Recompute clipping planes and refresh the frustum visualization
		UpdateCameraPosition();

		// Focus plane controls
		bool bDrawFocusPlane = bDrawDebugFocusPlane && bOverrideDOF;
		if (bDrawFocusPlane && (DebugFocusPlaneComponent == nullptr))
		{
			CreateDebugFocusPlane();
		}
		else if (!bDrawFocusPlane && (DebugFocusPlaneComponent != nullptr))
		{
			DestroyDebugFocusPlane();
		}
		// set focus plane color in case that's what changed
		if (DebugFocusPlaneMID)
		{
			DebugFocusPlaneMID->SetVectorParameterValue(FName(TEXT("Color")), DebugFocusPlaneColor.ReinterpretAsLinear());
		}
		UpdateDebugFocusPlane();

		OnLookingGlassObjectChanged.ExecuteIfBound(this);
	}
}

#endif // WITH_EDITOR

void ULookingGlassCameraComponent::CaptureLookingGlassScene(FLookingGlassRenderingConfig& RenderingConfig)
{
	UWorld* World = GetWorld();
	if (World && World->Scene && IsVisible() && CaptureDriver != nullptr)
	{
		//? We must push any deferred render state recreations before causing any rendering to happen, to make sure that deleted resource references are updated
		World->SendAllEndOfFrameUpdates();
		ULookingGlassSceneCaptureComponent2D::UpdateLookingGlassSceneCaptureContents(CaptureDriver, RenderingConfig, World->Scene);
	}
}

// Recalculates clipping planes and refreshes frustum visualization
void ULookingGlassCameraComponent::UpdateCameraPosition()
{
	float CameraDistance = GetCameraDistance();

	// Compute clipping planes
	NearClipPlane = CameraDistance - NearClipFactor * Size;
	NearClipPlane = FMath::Clamp(NearClipPlane, 1.0f, CameraDistance);

	FarClipPlane = CameraDistance + FMath::Max(FarClipFactor, 0.0f) * Size;

	// Keep the standard camera aspect in sync with the Looking Glass device, so the in-editor
	// camera preview and Movie Render Queue output match what the device shows
	AspectRatio = GetAspectRatio();

#if WITH_EDITOR
	RefreshVisualRepresentation();
#endif // WITH_EDITOR
}

// The camera's FieldOfView property allows a much wider range than the hologram math can handle,
// so clamp it for all Looking Glass computations
float ULookingGlassCameraComponent::GetLookingGlassFOV() const
{
	return FMath::Clamp(FieldOfView, 1.0f, 170.0f);
}

// Calculate and return matrix used for camera projection matrices
FMatrix ULookingGlassCameraComponent::GenerateProjectionMatrix(float OffsetX, float OffsetY) const
{
	float HalfFOV = FMath::DegreesToRadians(GetLookingGlassFOV()) / 2.0f;
	float Aspect = GetAspectRatio();

	float MaxZ = 0.f;
	float MinZ = 0.f;
	if (bUseFarClipPlane)
	{
		MaxZ = ((NearClipPlane == FarClipPlane) ? 0.0f : NearClipPlane / (NearClipPlane - FarClipPlane));
		MinZ = ((NearClipPlane == FarClipPlane) ? NearClipPlane : -FarClipPlane * NearClipPlane / (NearClipPlane - FarClipPlane));
	}
	else
	{
		MinZ = NearClipPlane;
	}

	FMatrix PerspectiveMatrix(
		FPlane(1.0f / FMath::Tan(HalfFOV), 0.0f, 0.0f, 0.0f),
		FPlane(0.0f, Aspect / FMath::Tan(HalfFOV), 0.0f, 0.0f),
		FPlane(OffsetX, OffsetY, MaxZ, 1.0f),
		FPlane(0.0f, 0.0f, MinZ, 0.0f)
	);

	UE_LOG(LookingGlassLogGame, Verbose, TEXT("PerspectiveMatrix: %s"), *PerspectiveMatrix.ToString());

	return PerspectiveMatrix;
}

float ULookingGlassCameraComponent::GetAspectRatio() const
{
	float Aspect = 0.0f;

	if (TilingValues.Aspect == 0.0f)
	{
		// Compute aspect from device
		const FLGDeviceCalibration& Calibration = ILookingGlassRuntime::Get().GetCurrentCalibration();
		Aspect = Calibration.Aspect != 0 ? Calibration.Aspect : (float)Calibration.Width / Calibration.Height;
	}
	else
	{
		Aspect = TilingValues.Aspect;
	}

	// Clamp the value to something reasonable, including impossibility to set zero value
	Aspect = FMath::Clamp(Aspect, 0.05f, 20.0f);

	return Aspect;
}

float ULookingGlassCameraComponent::GetCameraDistance() const
{
	return Size / FMath::Tan(FMath::DegreesToRadians(GetLookingGlassFOV() * 0.5f));
}

void ULookingGlassCameraComponent::ComputeViewInfo(int32 ViewIndex, int32 NumTiles, float ViewConeDegrees, FVector& OutViewLocation, FMatrix& OutViewRotationMatrix, FMatrix& OutProjectionMatrix) const
{
	float CamDistance = GetCameraDistance();
	float ViewConeSweep = CamDistance * FMath::Tan(FMath::DegreesToRadians(ViewConeDegrees));

	// Compute rotation matrix. Remove the translation from the transform because we only need rotation.
	FTransform Transform = GetComponentToWorld();
	Transform.SetTranslation(FVector::ZeroVector);
	Transform.SetScale3D(FVector::OneVector);

	// Swap axis st. x=z,y=x,z=y (Unreal coordinate space) so that z is up
	OutViewRotationMatrix = Transform.ToInverseMatrixWithScale() * FMatrix(
		FPlane(0, 0, 1, 0),
		FPlane(1, 0, 0, 0),
		FPlane(0, 1, 0, 0),
		FPlane(0, 0, 0, 1));

	// If NumTiles is 1, take the center view
	float CurrentViewLerp = 0.f;
	if (NumTiles > 1)
	{
		CurrentViewLerp = (float)ViewIndex / (NumTiles - 1.f) - .5f;
	}

	float ViewOffsetX = CurrentViewLerp * ViewConeSweep;
	float ProjModifier = 1.0f / Size;
	float ProjOffsetX = ViewOffsetX * ProjModifier;

	UE_LOG(LookingGlassLogGame, Verbose, TEXT("ViewOffsetX: %f, ProjOffsetX: %f"), ViewOffsetX, ProjOffsetX);

	FTransform RelativeTransform = FTransform::Identity;
	RelativeTransform.SetTranslation(FVector(0.0f, ViewOffsetX, 0.0f));

	FTransform WorldTransform = RelativeTransform * GetComponentToWorld();
	OutViewLocation = WorldTransform.GetTranslation();

	OutProjectionMatrix = GenerateProjectionMatrix(ProjOffsetX, 0.f);
}

void ULookingGlassCameraComponent::SetTilingProperties(ELookingGlassQualitySettings InTilingQuailty)
{
	if (TilingQuality != InTilingQuailty)
	{
		TilingQuality = InTilingQuailty;
		UpdateTilingProperties();
	}
}

void ULookingGlassCameraComponent::UpdateTilingProperties()
{
	// Save edited CustomTilingValues for later use
	if (TilingValues.Name == "Custom")
	{
		CustomTilingValues = TilingValues;
	}

	// Get the current tiling setup enum value, and resolve "automatic" setting
	ELookingGlassQualitySettings TilingSettings = TilingQuality;
	if (IsGlobalTilingQualitySet)
	{
		TilingSettings = GlobalTilingQuailty;
	}

	// Handle the "automatic" setting separately. Recognize the current device.
	if (TilingSettings == ELookingGlassQualitySettings::Q_Automatic)
	{
		TilingSettings = GetAutomaticTilingQuality();
	}

	if (TilingSettings == ELookingGlassQualitySettings::Q_Custom)
	{
		// Use custom values stored in this component
		TilingValues = CustomTilingValues;
		TilingValues.Setup();
	}
	else
	{
		const ULookingGlassSettings* LookingGlassSettings = GetDefault<ULookingGlassSettings>();
		TilingValues = LookingGlassSettings->GetTilingQualityFor(TilingSettings);
	}

	// Reset our render textures and configuration after it
	RebuildRenderConfigs();

	UpdateCameraPosition();
}

void ULookingGlassCameraComponent::UpdateTilingPropertiesForAllComponents()
{
	auto& LookingGlassRuntime = ILookingGlassRuntime::Get();

	for (auto& It : LookingGlassRuntime.GameLookingGlassCameras)
	{
		if (It.IsValid())
		{
			It->UpdateTilingProperties();
		}
	}
#if WITH_EDITOR
	if (GIsEditor)
	{
		for (auto& It : LookingGlassRuntime.EditorLookingGlassCameras)
		{
			if (It.IsValid())
			{
				It->UpdateTilingProperties();
			}
		}
	}
#endif // WITH_EDITOR
}

void ULookingGlassCameraComponent::SetGlobalTilingProperties(ELookingGlassQualitySettings InTilingQuailty)
{
	GlobalTilingQuailty = InTilingQuailty;
	IsGlobalTilingQualitySet = true;
	UpdateTilingPropertiesForAllComponents();
}

void ULookingGlassCameraComponent::ResetGlobalTilingProperties()
{
	IsGlobalTilingQualitySet = false;
	UpdateTilingPropertiesForAllComponents();
}

float ULookingGlassCameraComponent::GetDeviceViewCone()
{
	return ILookingGlassRuntime::Get().GetCurrentCalibration().ViewCone;
}

ELookingGlassQualitySettings ULookingGlassCameraComponent::GetAutomaticTilingQuality()
{
	const FLGDeviceCalibration& Calibration = ILookingGlassRuntime::Get().GetCurrentCalibration();

	// Recognize the device by its Serial

	if (Calibration.Serial.Contains(TEXT("Looking Glass - Portrait")) ||
		Calibration.Serial.Contains(TEXT("PORT")) ||
		Calibration.Serial.Contains(TEXT("Portrait")))
	{
		return ELookingGlassQualitySettings::Q_Portrait;
	}

	if (Calibration.Serial.Contains(TEXT("Looking Glass - 16\"")) ||
		Calibration.Serial.Contains(TEXT("LKG-A")) ||
		Calibration.Serial.Contains(TEXT("LKG-4K")))
	{
		return ELookingGlassQualitySettings::Q_FourK;
	}

	if (Calibration.Serial.Contains(TEXT("Looking Glass - 32\"")) ||
		Calibration.Serial.Contains(TEXT("LKG-B")) ||
		Calibration.Serial.Contains(TEXT("LKG-8K")))
	{
		return ELookingGlassQualitySettings::Q_EightK;
	}

	if (Calibration.Serial.Contains(TEXT("Looking Glass - 65\"")) ||
		Calibration.Serial.Contains(TEXT("LKG-D")))
	{
		return ELookingGlassQualitySettings::Q_65_Inch;
	}

	if (Calibration.Serial.Contains(TEXT("LKG-Q")))
	{
		return ELookingGlassQualitySettings::Q_Prototype;
	}

	if (Calibration.Serial.Contains(TEXT("LKG-E")))
	{
		return ELookingGlassQualitySettings::Q_GoPortrait;
	}

	if (Calibration.Serial.Contains(TEXT("LKG-F")))
	{
		return ELookingGlassQualitySettings::Q_Kiosk;
	}

	if (Calibration.Serial.Contains(TEXT("LKG-H")))
	{
		return ELookingGlassQualitySettings::Q_16_Portrait;
	}

	if (Calibration.Serial.Contains(TEXT("LKG-J")))
	{
		return ELookingGlassQualitySettings::Q_16_Landscape;
	}

	if (Calibration.Serial.Contains(TEXT("LKG-K")))
	{
		return ELookingGlassQualitySettings::Q_32_Portrait;
	}

	if (Calibration.Serial.Contains(TEXT("LKG-L")))
	{
		return ELookingGlassQualitySettings::Q_32_Landscape;
	}

	if (Calibration.Serial.Contains(TEXT("Looking Glass - 8.9\"")) ||
		Calibration.Serial.Contains(TEXT("LKG-2K")))
	{
		return ELookingGlassQualitySettings::Q_EightPointNineLegacy;
	}

	// Fallback to Portrait
	return ELookingGlassQualitySettings::Q_Portrait;
}

// Apply the "focus on the hologram plane" DOF override to the given post process settings. Only
// called when bOverrideDOF is enabled, and always works on a copy of the camera settings (the view
// or the capture driver), never on the component's own PostProcessSettings.
void ULookingGlassCameraComponent::ApplyDOFPostProcessSettings(FPostProcessSettings& InOutSettings) const
{
	InOutSettings.bOverride_DepthOfFieldFocalDistance = true;
	InOutSettings.DepthOfFieldFocalDistance = GetCameraDistance() + DOFAdjust;
	InOutSettings.bOverride_DepthOfFieldFstop = true;
	InOutSettings.DepthOfFieldFstop = DepthOfFieldFstop;
}

void ULookingGlassCameraComponent::GetEffectivePostProcessSettings(FPostProcessSettings& OutSettings, float& OutBlendWeight) const
{
	OutSettings = PostProcessSettings;
	OutBlendWeight = PostProcessBlendWeight;
	if (bOverrideDOF)
	{
		ApplyDOFPostProcessSettings(OutSettings);
		if (OutBlendWeight <= 0.f)
		{
			// Camera post processing is turned off; enable it just for the DOF override
			OutBlendWeight = 1.0f;
		}
	}
}

void ULookingGlassCameraComponent::SetupCaptureDriver()
{
	EnsureCaptureDriver();
	if (CaptureDriver == nullptr)
	{
		return;
	}

	// The hologram rendering inherits the camera's post processing setup; the Looking Glass DOF
	// override is applied to the capture driver's copy only
	GetEffectivePostProcessSettings(CaptureDriver->PostProcessSettings, CaptureDriver->PostProcessBlendWeight);

	FEngineShowFlags& CaptureShowFlags = CaptureDriver->ShowFlags;
	CaptureShowFlags.EnableAdvancedFeatures();
	CaptureShowFlags.SetTemporalAA(false);

#if ENGINE_MAJOR_VERSION >= 5
	CaptureShowFlags.SetLumenGlobalIllumination(true);
	CaptureShowFlags.SetLumenReflections(true);
#endif

	// Enable or disable motion blur rendering
	CaptureShowFlags.SetMotionBlur(bEnableMotionBlur);
}

// Called by LookingGlassViewportClient used for capturing new snapshot of scene from the camera.
// Note: this is the realtime preview path. Offline quilt rendering through Movie Render Queue
// (including Path Tracing) is implemented by UMoviePipelineLookingGlassQuiltPass in the
// LookingGlassMovieRenderPipeline module, which shares the per-view math via ComputeViewInfo().
void ULookingGlassCameraComponent::RenderViews()
{
	SetupCaptureDriver();
	if (CaptureDriver == nullptr)
	{
		return;
	}

	// Release RT used for 2D rendering, if any
	TextureTarget2DRendering = nullptr;

	// Get the ViewCone value for current device
	const FLGDeviceCalibration& Calibration = ILookingGlassRuntime::Get().GetCurrentCalibration();
	float ViewCone = Calibration.ViewCone;

	int32 NumTiles = TilingValues.GetNumTiles();

	for (FLookingGlassRenderingConfig& RenderingConfig : RenderingConfigs.Configs)
	{
		// Rendering target is initialized as 1x1 texture, so it won't take much space until rendering starts.
		// We should resize the target before rendering.
		RenderingConfig.PrepareRT();

		// Set render target texture to the capture driver. The rendering code which is called from
		// this function receives RenderingConfig as input, but it relies on TextureTarget to be set
		CaptureDriver->TextureTarget = RenderingConfig.GetRenderTarget();

		int32 NumViews = RenderingConfig.GetNumViews();
		check(NumViews);

		for (int32 ViewIndex = 0; ViewIndex < NumViews; ++ViewIndex)
		{
			FSceneCaptureViewInfo& ViewInfo = RenderingConfig.GetViewInfoArr()[ViewIndex];
			ComputeViewInfo(RenderingConfig.GetFirstViewIndex() + ViewIndex, NumTiles, ViewCone,
				ViewInfo.ViewLocation, ViewInfo.ViewRotationMatrix, ViewInfo.ProjectionMatrix);
		}

		// Render view
		SCOPE_CYCLE_COUNTER(STAT_CaptureScene_GameThread);
		CaptureLookingGlassScene(RenderingConfig);

		// Do not hold TextureTarget after rendering
		CaptureDriver->TextureTarget = nullptr;
	}
}

void ULookingGlassCameraComponent::Render2DView(int32 SizeX, int32 SizeY)
{
	SetupCaptureDriver();
	if (CaptureDriver == nullptr)
	{
		return;
	}

	const FLGDeviceCalibration& Calibration = ILookingGlassRuntime::Get().GetCurrentCalibration();

	// Check if the texture is exists
	if (TextureTarget2DRendering == nullptr)
	{
		TextureTarget2DRendering = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), UTextureRenderTarget2D::StaticClass());
		TextureTarget2DRendering->InitCustomFormat(Calibration.Width, Calibration.Height, PF_A16B16G16R16, false);
		TextureTarget2DRendering->ClearColor = FLinearColor::Red;
		TextureTarget2DRendering->UpdateResourceImmediate();
	}

	if (SizeX < 0 && SizeY < 0)
	{
		SizeX = Calibration.Width;
		SizeY = Calibration.Height;
	}

	if (SizeX != TextureTarget2DRendering->SizeX || SizeY != TextureTarget2DRendering->SizeY)
	{
		TextureTarget2DRendering->ResizeTarget(SizeX, SizeY);
		TextureTarget2DRendering->UpdateResourceImmediate();
		// Sync with rendering thread, in order to reset texture resolution
		FlushRenderingCommands();
	}

	// Set 2D texture
	CaptureDriver->TextureTarget = TextureTarget2DRendering;
	CaptureDriver->CustomProjectionMatrix = GenerateProjectionMatrix(0.f, 0.f);

	// Do the simple 2D rendering
	CaptureDriver->Capture2D();

	// Do not hold TextureTarget after rendering
	CaptureDriver->TextureTarget = nullptr;
}
