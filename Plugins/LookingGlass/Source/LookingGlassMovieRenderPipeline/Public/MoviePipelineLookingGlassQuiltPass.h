// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MoviePipelineRenderPass.h"
#include "MovieRenderPipelineDataTypes.h"
#include "SceneView.h"

#include "LookingGlassSettings.h"

#include "MoviePipelineLookingGlassQuiltPass.generated.h"

class UTextureRenderTarget2D;
class ULookingGlassCameraComponent;

/**
 * Movie Render Queue render pass which outputs Looking Glass quilts.
 *
 * For every output frame, the pass renders each quilt tile view through its own scene renderer
 * (using the same off-axis view math as the realtime hologram preview, via
 * ULookingGlassCameraComponent::ComputeViewInfo), composites the tiles into a quilt image, and
 * submits it to the Movie Render Queue output pipeline. All standard MRQ output formats
 * (PNG/EXR sequences, movie encoders, ...) therefore work; the {render_pass} filename token
 * resolves to "LookingGlassQuilt".
 *
 * When bUsePathTracing is enabled (and the project/hardware support ray tracing), each tile view
 * is rendered with the Path Tracer, accumulating PathTracerSamplesPerView samples per tile through
 * a persistent per-tile view state.
 *
 * Usage notes:
 *  - Bind your LookingGlassCapture actor (or any actor with a LookingGlassCamera component) to the
 *    Camera Cuts track of the rendered sequence.
 *  - Set the Output Setting resolution to the quilt resolution (e.g. 4092x4092), so movie encoders
 *    and the preview widget agree with the quilt size produced by this pass.
 *  - Remove/disable the "Deferred Rendering" pass unless you also want the plain 2D camera view.
 *  - Anti Aliasing / spatial / temporal sample settings of MRQ do not apply to this pass; sampling
 *    is controlled by PathTracerSamplesPerView.
 */
UCLASS(BlueprintType)
class LOOKINGGLASSMOVIERENDERPIPELINE_API UMoviePipelineLookingGlassQuiltPass : public UMoviePipelineRenderPass
{
	GENERATED_BODY()

public:
	UMoviePipelineLookingGlassQuiltPass();

#if WITH_EDITOR
	virtual FText GetDisplayText() const override
	{
		return NSLOCTEXT("MovieRenderPipeline", "LookingGlassQuiltPass_DisplayName", "Looking Glass Quilt");
	}
#endif

	virtual bool IsValidOnShots() const override { return true; }
	virtual bool IsValidOnPrimary() const override { return true; }

protected:
	//~ Begin UMoviePipelineRenderPass Interface
	virtual void SetupImpl(const MoviePipeline::FMoviePipelineRenderPassInitSettings& InPassInitSettings) override;
	virtual void TeardownImpl() override;
	virtual void GatherOutputPassesImpl(TArray<FMoviePipelinePassIdentifier>& ExpectedRenderPasses) override;
	virtual void RenderSample_GameThreadImpl(const FMoviePipelineRenderPassMetrics& InSampleState) override;
	//~ End UMoviePipelineRenderPass Interface

public:
	/** Quilt tiling preset to render. "Automatic" resolves from the connected Looking Glass device (falls back to Portrait). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quilt Settings")
	ELookingGlassQualitySettings TilingSettings = ELookingGlassQualitySettings::Q_Automatic;

	/** Render each tile view with the Path Tracer (requires hardware ray tracing and the Path Tracer enabled in project settings). Falls back to the regular deferred renderer when unsupported. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quilt Settings")
	bool bUsePathTracing = true;

	/** Number of Path Tracer samples accumulated per tile view. Higher is cleaner and slower. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quilt Settings", meta = (EditCondition = "bUsePathTracing", ClampMin = "1", UIMin = "1", UIMax = "4096"))
	int32 PathTracerSamplesPerView = 64;

	/** When a Looking Glass device is connected, use its calibrated view cone angle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quilt Settings")
	bool bUseDeviceViewCone = true;

	/** Full horizontal view cone angle (degrees) used to spread the quilt views when no device view cone is available. Typical Looking Glass devices use 40-58 degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Quilt Settings", meta = (ClampMin = "1.0", ClampMax = "120.0", UIMin = "10.0", UIMax = "80.0"))
	float ViewCone = 40.0f;

private:
	// Resolve the LookingGlass camera to render from: the current view target's camera, or the
	// globally active one as fallback
	ULookingGlassCameraComponent* ResolveLookingGlassCamera(UWorld* World) const;

	// Resolve TilingSettings into concrete tiling values
	FLookingGlassTilingQuality ResolveTilingValues() const;

	// Make sure render targets and per-tile view states match the tiling setup
	void EnsureResources(const FLookingGlassTilingQuality& TilingValues, UWorld* World);

	// Render one tile view into TileRenderTarget
	void RenderTileView(UWorld* World, ULookingGlassCameraComponent* Camera, int32 TileIndex, int32 NumTiles, float ViewConeDegrees, bool bPathTracing);

	// Composite TileRenderTarget into its quilt position
	void CopyTileToQuilt(const FLookingGlassTilingQuality& TilingValues, int32 TileIndex);

	// Read back the quilt and hand it to the MRQ output pipeline
	void SubmitQuiltToOutput(const FMoviePipelineRenderPassMetrics& InSampleState, const FLookingGlassTilingQuality& TilingValues);

	/** Identifier of this pass in the MRQ output pipeline; resolves the {render_pass} format token */
	FMoviePipelinePassIdentifier PassIdentifier;

	/** Quilt-sized render target the tile views are composited into */
	UPROPERTY(Transient)
	UTextureRenderTarget2D* QuiltRenderTarget = nullptr;

	/** Tile-sized intermediate render target each view is rendered into */
	UPROPERTY(Transient)
	UTextureRenderTarget2D* TileRenderTarget = nullptr;

	/** Persistent per-tile view states; required for Path Tracer sample accumulation (and eye adaptation) */
	TArray<FSceneViewStateReference> TileViewStates;

	/** Saved value of r.PathTracing.SamplesPerPixel, restored on teardown */
	int32 SavedPathTracingSamplesPerPixel = -1;

	/** Whether a "no camera found" error was already logged (log once per job, render black frames) */
	bool bLoggedMissingCamera = false;
};
