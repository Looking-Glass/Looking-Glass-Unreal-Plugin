# Looking Glass Plugin for Unreal Engine

The Looking Glass Plugin for Unreal Engine allows you to create high quality holographic still images and videos for Looking Glass displays. 

It is not intended for realtime content generation at this time. 

For more information on how to use the plugin, check out our [docs site](https://docs.lookingglassfactory.com/developer-tools/index-2)!

## Version 2.0 migration

Version 2.0 intentionally changes the Looking Glass camera API. The user-facing `ULookingGlassCameraComponent` now inherits from Unreal's `UCameraComponent`; `ULookingGlassSceneCaptureComponent2D` remains only as the plugin's internal quilt capture driver.

- Existing `LookingGlassCapture` assets migrate their serialized camera and post-process values when loaded in the Unreal Editor.
- Sequencer tracks bound directly to the legacy `RenderCamera` component or its properties are not redirected. Rebind or recreate those tracks on `LookingGlassCamera`; use standard camera properties such as `FieldOfView` where applicable.
- C++ and Blueprint code that directly used the legacy scene-capture component or removed pre-2.0 properties must migrate to `ULookingGlassCameraComponent`.

## Obtaining the source code

`git clone git@github.com:Looking-Glass/Looking-Glass-Unreal-Plugin.git`
- `cd Looking-Glass-Unreal-Plugin`

Alternatively you may get the snapshot of the repository using downloading of zip file from GitHub instead of using `git clone`.

![](https://github.com/Looking-Glass/Unreal/blob/main/docs/docs-divider-gradient-stroke.png)

## How to build the plugin from the source code

- Pre-requisites: 
- Looking Glass Bridge 2.5.1 or higher
- Visual Studio 2022
- **Unreal Engine 5.5 requires MSVC v143 (14.38 - 17.8). Working with Unreal Engine with another MSVC version will cause the engine to fail to compile.**
- Download the project from Github (see above).
- Find the UnrealSDK.uproject file in the root folder of repository.
- If you have multiple Unreal engine versions installed, right click on this file, select "Switch unreal engine version" option and select the one which you want to use with this plugin.
- Right click on .uproject file again, choose "Generate Visual Studio project files". After this, you'll find UnrealSDK.sln file right next to the .uproject file.
- Open the .sln file (e.g. by double-clicking on it), you'll have Visual Studio started, with the solution file loaded.
- Now, you may build the project, or run it directly from Visual Studio.

![](https://github.com/Looking-Glass/Looking-Glass-Unreal-Plugin/blob/main/docs/docs-divider-gradient-stroke.png)

## How to use pre-built version of the plugin

- As a pre-requisite, you should have an existing project for Unreal Engine.
- Download the latest release for your Unreal engine version [here](https://github.com/Looking-Glass/Looking-Glass-Unreal-Plugin/releases)
- Create Plugins folder in your project (it may already exist if you have any project-level plugins).
- Create a LookingGlass folder inside of Plugins. 
- Unzip the archive file with precompiled plugin into this folder.
- Now you may run your project. The Looking Glass plugin should be automatically enabled, unless if it was explicitly disabled before. You may see the "Looking Glass" button on the editor's toolbar.
- If the button didn't appear, go to the menu, Edit | Plugins | Project | Light Field Display | LookingGlass, and check the "Enabled" option. The engine will prompt to restart the editor after that.

![](https://github.com/Looking-Glass/Looking-Glass-Unreal-Plugin/blob/main/docs/docs-divider-gradient-stroke.png)

## Movie Render Queue quilt rendering

The plugin includes a Movie Render Queue render pass for outputting Looking Glass quilts from a `Looking Glass Camera`.

- Add a Looking Glass camera or capture actor to your sequence and bind it in the Camera Cuts track.
- In Movie Render Queue, choose `+ Setting > Rendering > Looking Glass Quilt`.
- Remove or disable `Deferred Rendering` unless you also want a regular 2D render.
- Set the MRQ output resolution to match the selected quilt preset. For example, Looking Glass Go uses `4092 x 4092`.
- The render pass output identifier is `LookingGlassQuilt`, so `{render_pass}` resolves to that name in MRQ output paths.
- The camera's optical aspect controls projection. Quilt tile pixel aspect is a packing detail, so a rendered view may be scaled when it is copied into its tile.

Path Tracing is supported by the Looking Glass Quilt pass. In Unreal Engine 5.5 on Windows, Path Tracing requires DirectX 12, SM6 shader formats, Support Hardware Ray Tracing, Path Tracing, and Compute Skin Cache if Unreal prompts for it. The pass defaults to 8 path-traced samples per quilt tile because total work is `tile count x samples per tile`; increasing this value increases render time and can trigger a GPU watchdog reset. Larger quilt presets are the main driver of retained GPU memory. The pass reuses one Path Tracer state across tiles instead of retaining one full accumulation history per view.

`Decorrelate Path Tracer Views` is enabled by default. It advances the temporal ray sequence between quilt views so structured noise differs from view to view and can be reduced by the display's lenticular blending.

MRQ anti-aliasing spatial and temporal samples do not control this pass. Leave both at 1 when rendering only the quilt pass, and use `PathTracerSamplesPerView` in the Looking Glass Quilt settings for Path Tracing sample count.

![](https://github.com/Looking-Glass/Looking-Glass-Unreal-Plugin/blob/main/docs/docs-divider-gradient-stroke.png)

## How to package the plugin

Packaging is needed for plugin distribution across different machines, such as publishing a prebuilt GitHub release. Run `BuildPlugin.bat` from the repository root to package for Unreal Engine 5.5, or pass another installed engine version as the first argument (for example, `BuildPlugin.bat 5.4`). Source-only changes do not require committing packaged build output.

Alternatively, package the plugin from the editor: go to Edit | Plugins | Project | Light Field Display | LookingGlass, then choose `Package`.
