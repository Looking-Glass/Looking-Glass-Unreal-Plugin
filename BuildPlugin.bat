@echo off

set uplugin_file=LookingGlass/LookingGlass.uplugin

set src_dir=%~dp0%Plugins
set dst_dir=%~dp0%LookingGlass

set dst_dir=%dst_dir%_%date%
mkdir %dst_dir%

if not "%1" == "" goto engine_provided

::call :build %uplugin_file% 5.3
::call :build %uplugin_file% 5.4
::call :build %uplugin_file% 5.5
call :build %uplugin_file% 5.6

goto :eof

:engine_provided

:: Build for single engine version
call :build %uplugin_file% %1
goto :eof

:: Build function

:build

:: Extract parameters
for %%a in (%1) do set plugin_name=%%~na
set UE_VER=%2

echo.
echo ##
echo ## Building %1 for Unreal engine %UE_VER%...
echo ##
echo.

:: Locate the Unreal Engine installation directory
set ue_dir=
for /f "tokens=2* skip=2" %%a in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\%UE_VER%" /v InstalledDirectory') do set ue_dir=%%b
if "%ue_dir%" == "" (
	echo ERROR: Unreal engine %UE_VER% is not installed
	exit /b 1
)

:: Locate UAT
set UAT=%ue_dir%\Engine\Binaries\DotNET\AutomationTool.exe
if not exist "%ue_dir%\Engine\Source\UE4Game.Target.cs" (
	rem Check for UE5
	set UAT=%ue_dir%\Engine\Binaries\DotNET\AutomationTool\AutomationTool.exe
	if not exist "%ue_dir%\Engine\Source\UnrealGame.Target.cs" (
		echo ERROR: Unreal engine %UE_VER% could not be located at "%ue_dir%"
		exit /b 1
	)
)

:: Build
"%UAT%" BuildPlugin -Plugin=%src_dir%/%uplugin_file% -Package=%dst_dir%/%plugin_name%_%UE_VER% -CreateSubFolder
