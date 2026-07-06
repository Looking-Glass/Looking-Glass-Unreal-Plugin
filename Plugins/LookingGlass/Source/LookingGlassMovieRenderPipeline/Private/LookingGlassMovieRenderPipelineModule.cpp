// Fill out your copyright notice in the Description page of Project Settings.

#include "Modules/ModuleManager.h"

// This module only registers UMoviePipelineLookingGlassQuiltPass with the reflection system;
// Movie Render Queue discovers the setting class automatically.
IMPLEMENT_MODULE(FDefaultModuleImpl, LookingGlassMovieRenderPipeline)
