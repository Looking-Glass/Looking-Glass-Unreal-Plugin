# Looking Glass Plugin for Unreal Engine

The Looking Glass Plugin for Unreal Engine allows you to create high quality holographic still images and videos for Looking Glass displays. 

It is not intended for realtime content generation at this time. 

For more information on how to use the plugin, check out our [docs site](https://docs.lookingglassfactory.com/developer-tools/index-2)!

## Obtaining the source code

`git clone git@github.com:Looking-Glass/Looking-Glass-Unreal-Plugin.git`
- `cd Looking-Glass-Unreal-Plugin`

Alternatively you may get the snapshot of the repository using downloading of zip file from GitHub instead of using `git clone`.

![](https://github.com/Looking-Glass/Unreal/blob/main/docs/docs-divider-gradient-stroke.png)

## How to build the plugin from the source code

- Pre-requisites: 
- Looking Glass Bridge 2.5.1 or higher
- Visual Studio 2022
- **Unreal Engine 5.6 requires MSVC v143 (14.38 - 17.8). Working with Unreal Engine with another MSVC version will cause the engine to fail to compile.**
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

## How to package the plugin

Packaging is needed for plugin's distribution accross different machines, e.g. providing builds over the Internet. The easiest way to package is to run `BuildPlugin.bat` file in the root folder of the project. This file will build and package the plugin for multiple engine versions, so if you don't need that - you may customize it to work with the version you needed.

Alternatively, there's a standard way of packaging the plugin from the editor. Just doing the same actions as for enabling the plugin (to open the plugin's page in engine settings): go to the menu, Edit | Plugins | Project | Light Field Display | LookingGlass. There you'll see the "Package" option, click on it. The editor will actomatically execute all needed actions and pass the plugin over the Visual Studio to build it.
