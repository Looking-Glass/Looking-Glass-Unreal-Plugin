// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneCaptureComponent.h"

#include "LookingGlassSettings.h"

class UTextureRenderTarget2D;

/**
 * Render configuration, which holds the texture and CaptureViewInfo. Represents a single line in quilt.
 */
struct FLookingGlassRenderingConfig
{
public:
	FLookingGlassRenderingConfig();

	~FLookingGlassRenderingConfig();

	/** Init should be called separately, because we do not control construction of UObject directly */
	void Init(UObject* Parent, uint32 InMinTextureIndex, uint32 InMaxTextureIndex, const FIntPoint& InViewSize);

	void AddReferencedObjects(FReferenceCollector& Collector);

	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget.Get(); }

	TArray<FSceneCaptureViewInfo>& GetViewInfoArr() { return ViewInfoArr; }

	const TArray<FSceneCaptureViewInfo>& GetViewInfoArr() const { return ViewInfoArr; }

	int32 GetNumViews() const { return NumViews; }

	int32 GetFirstViewIndex() const { return FirstViewIndex; }

	int32 GetViewRows() const { return ViewRows; }

	int32 GetViewColumns() const { return ViewColumns; }

	// Resize the rendering target to match our needs
	//todo: resize it back to 1x1 when rendering stops (call ReduceMemoryUse)
	void PrepareRT();

	void ReduceMemoryUse();

	void Release();

	/** Maximal number of views rendered with a single draw call */
	static constexpr uint8 MaxView = 8;

	static void CalculateViewRect(FIntRect& Rect, const FIntPoint& Size, int32 ViewRows, int32 ViewColumns, int32 ViewIndex);

	static void CalculateViewRect(float& U, float& V, float& SizeU, float& SizeV, int32 ViewRows, int32 ViewColumns, int32 ViewCount, int32 ViewIndex);

private:
	TObjectPtr<UTextureRenderTarget2D> RenderTarget = nullptr;

	// Same as ViewInfoArr.Num(). Number of views is limited either by GMaxTextureDimensions or MaxViews.
	uint32 NumViews;

	int32 FirstViewIndex;

	// Size of RenderTarget used for rendering
	FIntPoint TextureSize;

	TArray<FSceneCaptureViewInfo> ViewInfoArr;

	int32 ViewRows;

	int32 ViewColumns;
};

/**
 * A list of FLookingGlassRenderingConfig objects
 */
struct FLookingGlassRenderingConfigs
{
	FLookingGlassRenderingConfigs()
	{}

	FLookingGlassRenderingConfigs(UObject* InOwner)
		: Owner(InOwner)
	{}

	// The outer object which is used for holding all render targets
	UObject* Owner;

	TArray<FLookingGlassRenderingConfig> Configs;

	// Recent TilingValues which were used for RebuildRenderConfigs
	FLookingGlassTilingQuality CachedTilingValues;

	void Build(const FLookingGlassTilingQuality& TilingValues, bool bSingleViewMode);

	void Release()
	{
		Configs.Empty();
	}

	void AddReferencedObjects(FReferenceCollector& Collector)
	{
		for (FLookingGlassRenderingConfig& Config : Configs)
		{
			Config.AddReferencedObjects(Collector);
		}
	}
};
