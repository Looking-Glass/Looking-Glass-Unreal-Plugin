#pragma once

#include "CoreMinimal.h"

class ULookingGlassCameraComponent;
class IMovieSceneCaptureInterface;
struct FLookingGlassBridge;

namespace LookingGlass
{
	/**
	 * @fn	UMovieSceneCapture* GetMovieSceneCapture();
	 *
	 * @brief	Gets movie scene capture
	 * 			It is using for check movie capture during capturing video
	 *
	 * @returns	Null if it fails, else the movie scene capture.
	 */

	LOOKINGGLASSRUNTIME_API IMovieSceneCaptureInterface* GetMovieSceneCapture();

	/** Returns the currently active LookingGlass camera (the one used for hologram rendering) */
	LOOKINGGLASSRUNTIME_API TWeakObjectPtr<ULookingGlassCameraComponent> GetActiveLookingGlassCamera();
};
