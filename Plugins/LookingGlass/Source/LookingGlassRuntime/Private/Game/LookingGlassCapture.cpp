#include "Game/LookingGlassCapture.h"

#include "Game/LookingGlassCameraComponent.h"
#include "Game/LookingGlassSceneCaptureComponent2D.h"
#include "GameFramework/SpringArmComponent.h"

#include "Misc/LookingGlassLog.h"

struct FLookingGlassCustomVersion
{
	enum Type
	{
		// Before any version changes were made in the plugin
		BeforeCustomVersionWasAdded = 0,

		// Moved most of the code to ULookingGlassSceneCaptureComponent2D
		MovedFunctionalityToComponent = 1,

		// Refactored to use ULookingGlassCameraComponent (UCameraComponent subclass) as the
		// user-facing component; ULookingGlassSceneCaptureComponent2D became an internal driver
		RefactoredToCameraComponent = 2,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FLookingGlassCustomVersion() {}
};

const FGuid FLookingGlassCustomVersion::GUID(0xD5E9D981, 0xD59D4CB9, 0x81B18E40, 0xB789E2B0);

// Register the custom version with core
FCustomVersionRegistration GRegisterLookingGlassCustomVersion(FLookingGlassCustomVersion::GUID, FLookingGlassCustomVersion::LatestVersion, TEXT("LookingGlassVer"));


// Sets default values
ALookingGlassCapture::ALookingGlassCapture()
{
	CameraSpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraSpringArm"));
	CameraSpringArm->SetupAttachment(RootComponent);
	CameraSpringArm->bDoCollisionTest = false;

	LookingGlassCamera = CreateDefaultSubobject<ULookingGlassCameraComponent>(TEXT("LookingGlassCamera"));
	LookingGlassCamera->SetupAttachment(CameraSpringArm);

	// The internal capture driver used by LookingGlassCamera for quilt rendering. It keeps the
	// subobject name of the pre-camera-refactoring component, so old data still loads into it
	// (see MigrateRenderCameraToCamera). LookingGlassCamera picks it up in EnsureCaptureDriver().
	RenderCamera = CreateDefaultSubobject<ULookingGlassSceneCaptureComponent2D>(TEXT("RenderCamera"));
	RenderCamera->SetupAttachment(LookingGlassCamera);

	// Subscribe to camera updates. PostEditChangeProperty() does work only when
	// user is editing its property when Actor is selected. When the camera component
	// is selected, PostEditChangeProperty is called only for component.
	LookingGlassCamera->OnLookingGlassObjectChanged.BindLambda([this](ULookingGlassCameraComponent* Component)
		{
			UpdateCameraPosition();
		});

	// Initialize child component positions
	UpdateCameraPosition();
}

void ALookingGlassCapture::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITORONLY_DATA
	Ar.UsingCustomVersion(FLookingGlassCustomVersion::GUID);

	if (Ar.IsLoading() && Ar.IsPersistent())
	{
		// Remember the version for PostLoad, where migration of old data happens (in PostLoad all
		// subobjects of the actor are available, unlike in Serialize)
		LoadedCustomVersion = Ar.CustomVer(FLookingGlassCustomVersion::GUID);
	}
#endif // WITH_EDITORONLY_DATA
}

void ALookingGlassCapture::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	if (LoadedCustomVersion < FLookingGlassCustomVersion::MovedFunctionalityToComponent)
	{
		LoadLegacyProperties();
	}
	else if (LoadedCustomVersion < FLookingGlassCustomVersion::RefactoredToCameraComponent)
	{
		MigrateRenderCameraToCamera();
	}
	LoadedCustomVersion = MAX_int32;
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITORONLY_DATA

void ALookingGlassCapture::LoadLegacyProperties()
{
	// Data saved before settings were moved from the actor to a component: copy actor properties
	// directly into the camera component
	LookingGlassCamera->Size = Size_DEPRECATED;
	LookingGlassCamera->NearClipFactor = NearClipFactor_DEPRECATED;
	LookingGlassCamera->bUseFarClipPlane = bUseFarClipPlane_DEPRECATED;
	LookingGlassCamera->FarClipFactor = FarClipFactor_DEPRECATED;
	LookingGlassCamera->FieldOfView = FOV_DEPRECATED;
	LookingGlassCamera->OverrideQuiltTexture2D = OverrideQuiltTexture2D_DEPRECATED;
	LookingGlassCamera->bSingleViewMode = bSingleViewMode_DEPRECATED;
	LookingGlassCamera->TilingQuality = TilingQuality_DEPRECATED;
	LookingGlassCamera->TilingValues = TilingValues_DEPRECATED;
}

void ALookingGlassCapture::MigrateRenderCameraToCamera()
{
	if (RenderCamera == nullptr || LookingGlassCamera == nullptr)
	{
		return;
	}

	// Make sure the legacy component has finished loading its data
	RenderCamera->ConditionalPostLoad();

	LookingGlassCamera->Size = RenderCamera->Size;
	LookingGlassCamera->NearClipFactor = RenderCamera->NearClipFactor;
	LookingGlassCamera->bUseFarClipPlane = RenderCamera->bUseFarClipPlane;
	LookingGlassCamera->FarClipFactor = RenderCamera->FarClipFactor;
	LookingGlassCamera->FieldOfView = RenderCamera->FOV;
	LookingGlassCamera->bSingleViewMode = RenderCamera->bSingleViewMode;
	LookingGlassCamera->OverrideQuiltTexture2D = RenderCamera->OverrideQuiltTexture2D;
	LookingGlassCamera->TilingQuality = RenderCamera->TilingQuality;
	LookingGlassCamera->TilingValues = RenderCamera->TilingValues;
	LookingGlassCamera->bOverrideDOF = RenderCamera->bOverrideDOF;
	LookingGlassCamera->DOFAdjust = RenderCamera->DOFAdjust;
	LookingGlassCamera->DepthOfFieldFstop = RenderCamera->DepthOfFieldFstop;
	LookingGlassCamera->bDrawDebugFocusPlane = RenderCamera->bDrawDebugFocusPlane;
	LookingGlassCamera->bEnableMotionBlur = RenderCamera->bEnableMotionBlur;
	LookingGlassCamera->FrustumColor = RenderCamera->FrustumColor;
	LookingGlassCamera->MidPlaneFrustumColor = RenderCamera->MidPlaneFrustumColor;
	LookingGlassCamera->MidPlaneLineLength = RenderCamera->MidPlaneLineLength;
	LookingGlassCamera->MidLineThickness = RenderCamera->MidLineThickness;
	LookingGlassCamera->FrustumLineThickness = RenderCamera->FrustumLineThickness;
	LookingGlassCamera->DebugFocusPlaneColor = RenderCamera->DebugFocusPlaneColor;

	// The legacy component may have been saved attached to the spring arm (old hierarchy); make
	// sure it sits under the camera, at identity transform, where the quilt rendering expects it
	RenderCamera->SetupAttachment(LookingGlassCamera);
	RenderCamera->SetRelativeTransform(FTransform::Identity);

	UE_LOG(LookingGlassLogGame, Log, TEXT("%s: migrated LookingGlass capture settings to the camera component"), *GetName());
}

#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR

void ALookingGlassCapture::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Editor property changes are handled via LookingGlassCamera->OnLookingGlassObjectChanged
}

#endif // WITH_EDITOR

void ALookingGlassCapture::UpdateCameraPosition()
{
	float CamDistance = LookingGlassCamera->GetCameraDistance();
	CameraSpringArm->TargetArmLength = CamDistance;
#if WITH_EDITOR
	if (GIsEditor && CameraSpringArm->IsRegistered())
	{
		// In editor, changing "Size" property with slider may cause a lot of flicker.
		// Explicitly updating CameraSpringArm component resolves the issue.
		CameraSpringArm->TickComponent(0, ELevelTick::LEVELTICK_All, nullptr);
	}
#endif // WITH_EDITOR
}
