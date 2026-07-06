// Fill out your copyright notice in the Description page of Project Settings.

#include "MoviePipelineLookingGlassQuiltPass.h"

#include "MoviePipeline.h"
#include "MoviePipelineOutputBuilder.h"
#include "MovieRenderPipelineCoreModule.h"

#include "Game/LookingGlassCameraComponent.h"
#include "Misc/LookingGlassHelpers.h"
#include "Render/LookingGlassRendering.h"

#include "CanvasTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineModule.h" // for GetRendererModule()
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "ImagePixelData.h"
#include "LegacyScreenPercentageDriver.h"
#include "Misc/App.h"
#include "RenderingThread.h"
#include "RenderUtils.h" // for IsRayTracingEnabled()
#include "RHI.h"
#include "SceneInterface.h"
#include "SceneView.h"
#include "TextureResource.h"

#include "Runtime/Launch/Resources/Version.h"

UMoviePipelineLookingGlassQuiltPass::UMoviePipelineLookingGlassQuiltPass()
	: PassIdentifier(TEXT("LookingGlassQuilt"))
{
}

void UMoviePipelineLookingGlassQuiltPass::SetupImpl(const MoviePipeline::FMoviePipelineRenderPassInitSettings& InPassInitSettings)
{
	Super::SetupImpl(InPassInitSettings);

	bLoggedMissingCamera = false;
}

void UMoviePipelineLookingGlassQuiltPass::TeardownImpl()
{
	// Make sure no in-flight rendering still references our view states or render targets
	FlushRenderingCommands();

	for (FSceneViewStateReference& ViewState : TileViewStates)
	{
		ViewState.Destroy();
	}
	TileViewStates.Empty();

	QuiltRenderTarget = nullptr;
	TileRenderTarget = nullptr;

	Super::TeardownImpl();
}

void UMoviePipelineLookingGlassQuiltPass::GatherOutputPassesImpl(TArray<FMoviePipelinePassIdentifier>& ExpectedRenderPasses)
{
	Super::GatherOutputPassesImpl(ExpectedRenderPasses);
	ExpectedRenderPasses.Add(PassIdentifier);
}

ULookingGlassCameraComponent* UMoviePipelineLookingGlassQuiltPass::ResolveLookingGlassCamera(UWorld* World) const
{
	// Prefer the camera the sequence is actually cutting to (supports multiple LookingGlass
	// captures switched by the Camera Cuts track)
	if (APlayerController* PlayerController = World->GetFirstPlayerController())
	{
		if (PlayerController->PlayerCameraManager != nullptr)
		{
			if (AActor* ViewTarget = PlayerController->PlayerCameraManager->GetViewTarget())
			{
				if (ULookingGlassCameraComponent* Camera = ViewTarget->FindComponentByClass<ULookingGlassCameraComponent>())
				{
					return Camera;
				}
			}
		}
	}

	// Fall back to the globally active LookingGlass camera
	return LookingGlass::GetActiveLookingGlassCamera().Get();
}

FLookingGlassTilingQuality UMoviePipelineLookingGlassQuiltPass::ResolveTilingValues() const
{
	ELookingGlassQualitySettings TilingQuality = TilingSettings;
	if (TilingQuality == ELookingGlassQualitySettings::Q_Automatic)
	{
		TilingQuality = ULookingGlassCameraComponent::GetAutomaticTilingQuality();
	}

	const ULookingGlassSettings* LookingGlassSettings = GetDefault<ULookingGlassSettings>();
	FLookingGlassTilingQuality TilingValues = LookingGlassSettings->GetTilingQualityFor(TilingQuality);
	TilingValues.Setup();

	return TilingValues;
}

void UMoviePipelineLookingGlassQuiltPass::EnsureRenderTargets(const FLookingGlassTilingQuality& TilingValues)
{
	// Quilt-sized composition target
	if (QuiltRenderTarget == nullptr)
	{
		QuiltRenderTarget = NewObject<UTextureRenderTarget2D>(this);
		QuiltRenderTarget->ClearColor = FLinearColor::Black;
		QuiltRenderTarget->InitCustomFormat(TilingValues.QuiltW, TilingValues.QuiltH, PF_A2B10G10R10, false);
		QuiltRenderTarget->UpdateResourceImmediate();
	}
	else if (QuiltRenderTarget->SizeX != TilingValues.QuiltW || QuiltRenderTarget->SizeY != TilingValues.QuiltH)
	{
		QuiltRenderTarget->ResizeTarget(TilingValues.QuiltW, TilingValues.QuiltH);
		QuiltRenderTarget->UpdateResourceImmediate();
	}

	// Tile-sized intermediate target
	if (TileRenderTarget == nullptr)
	{
		TileRenderTarget = NewObject<UTextureRenderTarget2D>(this);
		TileRenderTarget->ClearColor = FLinearColor::Black;
		TileRenderTarget->InitCustomFormat(TilingValues.TileSizeX, TilingValues.TileSizeY, PF_A16B16G16R16, false);
		TileRenderTarget->UpdateResourceImmediate();
	}
	else if (TileRenderTarget->SizeX != TilingValues.TileSizeX || TileRenderTarget->SizeY != TilingValues.TileSizeY)
	{
		TileRenderTarget->ResizeTarget(TilingValues.TileSizeX, TilingValues.TileSizeY);
		TileRenderTarget->UpdateResourceImmediate();
	}
}

void UMoviePipelineLookingGlassQuiltPass::EnsureViewStates(const FLookingGlassTilingQuality& TilingValues, UWorld* World)
{
	// One persistent view state per tile: the Path Tracer accumulates its samples in the view
	// state, and eye adaptation history lives there as well
	const int32 NumTiles = TilingValues.GetNumTiles();
	if (TileViewStates.Num() != NumTiles)
	{
		FlushRenderingCommands();
		for (FSceneViewStateReference& ViewState : TileViewStates)
		{
			ViewState.Destroy();
		}
		TileViewStates.Empty(NumTiles);

		const ERHIFeatureLevel::Type FeatureLevel = World->Scene->GetFeatureLevel();
		for (int32 Index = 0; Index < NumTiles; ++Index)
		{
			FSceneViewStateReference& ViewState = TileViewStates.AddDefaulted_GetRef();
			ViewState.Allocate(FeatureLevel);
		}
	}
}

void UMoviePipelineLookingGlassQuiltPass::RenderTileView(UWorld* World, ULookingGlassCameraComponent* Camera, int32 TileIndex, int32 NumTiles, float ViewConeDegrees, bool bPathTracing)
{
	FVector ViewLocation;
	FMatrix ViewRotationMatrix;
	FMatrix ProjectionMatrix;
	Camera->ComputeViewInfo(TileIndex, NumTiles, ViewConeDegrees, ViewLocation, ViewRotationMatrix, ProjectionMatrix);

	FTextureRenderTargetResource* RTResource = TileRenderTarget->GameThread_GetRenderTargetResource();

	// Post processing: the camera's own settings plus the LookingGlass DOF override, mirroring
	// ULookingGlassCameraComponent::SetupCaptureDriver
	FPostProcessSettings PPSettings = Camera->PostProcessSettings;
	float PPWeight = Camera->PostProcessBlendWeight;
	if (Camera->bOverrideDOF)
	{
		PPSettings.bOverride_DepthOfFieldFocalDistance = true;
		PPSettings.DepthOfFieldFocalDistance = Camera->GetCameraDistance() + Camera->DOFAdjust;
		PPSettings.bOverride_DepthOfFieldFstop = true;
		PPSettings.DepthOfFieldFstop = Camera->DepthOfFieldFstop;
		if (PPWeight <= 0.f)
		{
			PPWeight = 1.0f;
		}
	}

	// The Path Tracer accumulates one sample batch per submitted render; repeat the render until
	// the requested per-view sample count has been reached. For the raster fallback a single
	// render is enough.
	const int32 NumSamples = bPathTracing ? FMath::Max(1, PathTracerSamplesPerView) : 1;
	for (int32 SampleIndex = 0; SampleIndex < NumSamples; ++SampleIndex)
	{
		FEngineShowFlags ShowFlags(ESFIM_Game);
		ShowFlags.EnableAdvancedFeatures();
		ShowFlags.SetTemporalAA(false);
		ShowFlags.SetMotionBlur(false);
#if ENGINE_MAJOR_VERSION >= 5
		ShowFlags.SetLumenGlobalIllumination(true);
		ShowFlags.SetLumenReflections(true);
#endif
		if (bPathTracing)
		{
			ShowFlags.SetPathTracing(true);
		}

		FGameTime GameTime = FGameTime::CreateUndilated(FApp::GetCurrentTime() - GStartTime, FApp::GetDeltaTime());

		FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(RTResource, World->Scene, ShowFlags)
			.SetResolveScene(true)
			.SetRealtimeUpdate(false)
			.SetTime(GameTime));

		ViewFamily.SceneCaptureSource = SCS_FinalColorLDR;
		ViewFamily.EngineShowFlags.ScreenPercentage = false;
		ViewFamily.SetScreenPercentageInterface(new FLegacyScreenPercentageDriver(ViewFamily, 1.0f, false));

		FSceneViewInitOptions ViewInitOptions;
		ViewInitOptions.ViewFamily = &ViewFamily;
		ViewInitOptions.SetViewRectangle(FIntRect(0, 0, TileRenderTarget->SizeX, TileRenderTarget->SizeY));
		ViewInitOptions.ViewOrigin = ViewLocation;
		ViewInitOptions.ViewRotationMatrix = ViewRotationMatrix;
		ViewInitOptions.ProjectionMatrix = ProjectionMatrix;
		ViewInitOptions.ViewActor = Camera->GetOwner();
		ViewInitOptions.SceneViewStateInterface = TileViewStates[TileIndex].GetReference();
		ViewInitOptions.BackgroundColor = FLinearColor::Black;
		if (World->GetWorldSettings() != nullptr)
		{
			ViewInitOptions.WorldToMetersScale = World->GetWorldSettings()->WorldToMeters;
		}

		FSceneView* View = new FSceneView(ViewInitOptions);
		View->bIsOfflineRender = true;
		// Reset Path Tracer accumulation (and other history) on the first sample of each frame;
		// subsequent samples of the same, unchanged view accumulate on top
		View->bCameraCut = (SampleIndex == 0);
		ViewFamily.Views.Add(View);

		View->StartFinalPostprocessSettings(ViewLocation);
		View->OverridePostProcessSettings(PPSettings, PPWeight);
		View->EndFinalPostprocessSettings(ViewInitOptions);

		if (bPathTracing)
		{
			// Offline-marked views take their Path Tracer sample target from the final post process
			// settings (not from r.PathTracing.SamplesPerPixel). Write it after all blending so the
			// value is exact: the Path Tracer accumulates across our repeated renders (through the
			// persistent per-tile view state) until this target is reached.
			View->FinalPostProcessSettings.PathTracingSamplesPerPixel = FMath::Max(1, PathTracerSamplesPerView);
		}

		FCanvas Canvas(RTResource, nullptr, GameTime, World->Scene->GetFeatureLevel());
		GetRendererModule().BeginRenderingViewFamily(&Canvas, &ViewFamily);
	}
}

void UMoviePipelineLookingGlassQuiltPass::CopyTileToQuilt(const FLookingGlassTilingQuality& TilingValues, int32 TileIndex)
{
	LookingGlass::FCopyToQuiltRenderContext RenderContext =
	{
		QuiltRenderTarget->GameThread_GetRenderTargetResource(),
		TilingValues,
		TileRenderTarget->GetResource(),
		TileIndex,
		/* ViewInfoIndex = */ 0,
		/* TotalViews = */ 1,
		/* ViewRows = */ 1,
		/* ViewColumns = */ 1,
		FSceneCaptureViewInfo()
	};

	ENQUEUE_RENDER_COMMAND(CopyTileToQuiltCommand)(
		[RenderContext](FRHICommandListImmediate& RHICmdList)
		{
			LookingGlass::CopyToQuiltShader_RenderThread(RHICmdList, RenderContext);
		});
}

void UMoviePipelineLookingGlassQuiltPass::SubmitQuiltToOutput(const FMoviePipelineRenderPassMetrics& InSampleState, const FLookingGlassTilingQuality& TilingValues)
{
	// Make sure all tile renders and quilt copies have finished before reading back
	FlushRenderingCommands();

	FTextureRenderTargetResource* QuiltResource = QuiltRenderTarget->GameThread_GetRenderTargetResource();

	TArray<FColor> Bitmap;
	FReadSurfaceDataFlags ReadSurfaceDataFlags;
	ReadSurfaceDataFlags.SetLinearToGamma(false);
	if (QuiltResource != nullptr && QuiltResource->ReadPixels(Bitmap, ReadSurfaceDataFlags))
	{
		for (FColor& Color : Bitmap)
		{
			Color.A = 255;
		}
	}
	else
	{
		// The output merger expects a frame for every declared pass; deliver black rather than
		// stranding the render waiting for this pass forever
		UE_LOG(LogMovieRenderPipeline, Error, TEXT("LookingGlassQuilt: failed to read back the quilt render target; submitting a black frame"));
		Bitmap.Init(FColor::Black, QuiltRenderTarget->SizeX * QuiltRenderTarget->SizeY);
	}

	TSharedRef<FImagePixelDataPayload, ESPMode::ThreadSafe> FramePayload = MakeShared<FImagePixelDataPayload, ESPMode::ThreadSafe>();
	FramePayload->PassIdentifier = PassIdentifier;
	FramePayload->SampleState = InSampleState;

	TArray64<FColor> Pixels64(Bitmap.GetData(), Bitmap.Num());
	TUniquePtr<TImagePixelData<FColor>> PixelData = MakeUnique<TImagePixelData<FColor>>(
		FIntPoint(QuiltRenderTarget->SizeX, QuiltRenderTarget->SizeY), MoveTemp(Pixels64), FramePayload);

	GetPipeline()->OutputBuilder->OnCompleteRenderPassDataAvailable_AnyThread(MoveTemp(PixelData));
}

void UMoviePipelineLookingGlassQuiltPass::RenderSample_GameThreadImpl(const FMoviePipelineRenderPassMetrics& InSampleState)
{
	Super::RenderSample_GameThreadImpl(InSampleState);

	// Engine warm-up (and motion-blur centering) samples must not produce output
	if (InSampleState.bDiscardResult)
	{
		return;
	}

	// This pass does its own multi-view sampling; act only on the very last MRQ sample of each
	// output frame and ignore the others (spatial/temporal samples, high-res tiles)
	const bool bLastTile =
		(InSampleState.TileIndexes.X == InSampleState.TileCounts.X - 1) &&
		(InSampleState.TileIndexes.Y == InSampleState.TileCounts.Y - 1);
	const bool bLastSample = bLastTile &&
		(InSampleState.SpatialSampleIndex == InSampleState.SpatialSampleCount - 1) &&
		(InSampleState.TemporalSampleIndex == InSampleState.TemporalSampleCount - 1);
	if (!bLastSample)
	{
		return;
	}

	// The output merger expects a frame from this pass no matter what happens below, so create the
	// quilt target first and route every failure path through SubmitQuiltToOutput
	const FLookingGlassTilingQuality TilingValues = ResolveTilingValues();
	EnsureRenderTargets(TilingValues);

	UWorld* World = GetPipeline()->GetWorld();
	ULookingGlassCameraComponent* Camera = nullptr;
	if (World != nullptr && World->Scene != nullptr)
	{
		Camera = ResolveLookingGlassCamera(World);
	}

	if (Camera == nullptr)
	{
		if (!bLoggedMissingCamera)
		{
			UE_LOG(LogMovieRenderPipeline, Error, TEXT("LookingGlassQuilt: no world scene, or no LookingGlassCamera component on the view target (bind a LookingGlassCapture actor to the Camera Cuts track). Rendering black frames."));
			bLoggedMissingCamera = true;
		}

		// Still submit a (black) frame, so the MRQ output pipeline doesn't stall waiting for this pass
		FTextureRenderTargetResource* QuiltResource = QuiltRenderTarget->GameThread_GetRenderTargetResource();
		ENQUEUE_RENDER_COMMAND(ClearLookingGlassQuiltCommand)(
			[QuiltResource](FRHICommandListImmediate& RHICmdList)
			{
				FRHIRenderPassInfo RPInfo(QuiltResource->GetRenderTargetTexture(), ERenderTargetActions::Clear_Store);
				RHICmdList.BeginRenderPass(RPInfo, TEXT("ClearLookingGlassQuilt"));
				RHICmdList.EndRenderPass();
			});

		SubmitQuiltToOutput(InSampleState, TilingValues);
		return;
	}

	EnsureViewStates(TilingValues, World);

	// Refresh near/far clipping planes in case Size or FieldOfView are animated
	Camera->UpdateCameraPosition();

	float EffectiveViewCone = ViewCone;
	if (bUseDeviceViewCone)
	{
		const float DeviceViewCone = ULookingGlassCameraComponent::GetDeviceViewCone();
		if (DeviceViewCone > 0.f)
		{
			EffectiveViewCone = DeviceViewCone;
		}
	}

	// Path tracing needs both ray tracing and path tracing support on the active shader platform;
	// with only the former, the renderer would silently draw without the path tracer
	const bool bPathTracing = bUsePathTracing
		&& IsRayTracingEnabled()
		&& FDataDrivenShaderPlatformInfo::GetSupportsPathTracing(World->Scene->GetShaderPlatform());
	if (bUsePathTracing && !bPathTracing)
	{
		UE_LOG(LogMovieRenderPipeline, Warning, TEXT("LookingGlassQuilt: Path Tracing requested but ray tracing/path tracing is not enabled or not supported on this platform; falling back to the deferred renderer"));
	}

	// Push any pending scene/component updates before manual scene rendering
	World->SendAllEndOfFrameUpdates();

	const int32 NumTiles = TilingValues.GetNumTiles();
	for (int32 TileIndex = 0; TileIndex < NumTiles; ++TileIndex)
	{
		RenderTileView(World, Camera, TileIndex, NumTiles, EffectiveViewCone, bPathTracing);
		CopyTileToQuilt(TilingValues, TileIndex);

		// Drain the render thread once per tile: with many tiles and many Path Tracer samples
		// per tile, letting all scene renders of a frame queue up unthrottled can exhaust memory
		FlushRenderingCommands();
	}

	SubmitQuiltToOutput(InSampleState, TilingValues);
}
