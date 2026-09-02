@echo off
setlocal enableextensions

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"
for %%I in ("%ROOT%\..") do set "REPO_ROOT=%%~fI"

set "ANDROID_DIR=%ROOT%\android"
set "GRADLEW=%ANDROID_DIR%\gradlew.bat"
set "ASSETS_MODEL_DIR=%ANDROID_DIR%\app\src\main\assets\model"
set "MODEL_DIR=%REPO_ROOT%\model"
set "ANDROID_LIB_DIR=%REPO_ROOT%\lib\android"
set "IOS_LIB_DIR=%REPO_ROOT%\lib\ios"
set "INTERFACE_DIR=%REPO_ROOT%\interface"
set "PACKAGES_DIR=%ANDROID_DIR%\packages"

echo [INFO] Demo root: %ROOT%

if not exist "%GRADLEW%" (
  echo [ERROR] gradlew.bat not found: %GRADLEW%
  exit /b 1
)

call :require_file "%INTERFACE_DIR%\mc_interface.h"
call :require_file "%ROOT%\android\app\src\main\cpp\mc_interface.h"
call :require_file "%ROOT%\ios\MagicCameraSR\mc_interface.h"
call :require_file "%ANDROID_LIB_DIR%\libmagic_sr.a"
call :require_file "%IOS_LIB_DIR%\libmagic_sr.a"

if not exist "%ASSETS_MODEL_DIR%" mkdir "%ASSETS_MODEL_DIR%"
if not exist "%PACKAGES_DIR%" mkdir "%PACKAGES_DIR%"

echo [INFO] Copying Android GLES model files (if present)...
if exist "%MODEL_DIR%\magic_gles_speed_gpu_params.bin" copy /Y "%MODEL_DIR%\magic_gles_speed_gpu_params.bin" "%ASSETS_MODEL_DIR%\" >nul
if exist "%MODEL_DIR%\magic_gles_balanced_gpu_params.bin" copy /Y "%MODEL_DIR%\magic_gles_balanced_gpu_params.bin" "%ASSETS_MODEL_DIR%\" >nul
if exist "%MODEL_DIR%\magic_gles_highspeed_gpu_params.bin" copy /Y "%MODEL_DIR%\magic_gles_highspeed_gpu_params.bin" "%ASSETS_MODEL_DIR%\" >nul
if exist "%MODEL_DIR%\magic_sr_gpu_params.bin" copy /Y "%MODEL_DIR%\magic_sr_gpu_params.bin" "%ASSETS_MODEL_DIR%\" >nul

echo [INFO] Building Android Debug APK...
pushd "%ANDROID_DIR%"
call "%GRADLEW%" :app:assembleDebug
if errorlevel 1 (
  popd
  echo [ERROR] Android build failed.
  exit /b 1
)
popd

copy /Y "%ANDROID_DIR%\app\build\outputs\apk\debug\app-debug.apk" "%PACKAGES_DIR%\MagicMagnifierSR-android-arm64.apk" >nul

echo.
echo [SUCCESS] Android build complete.
echo [SUCCESS] APK: %PACKAGES_DIR%\MagicMagnifierSR-android-arm64.apk
echo [INFO] iOS: build on macOS via Xcode generic iOS destination (core lib is device arm64).
exit /b 0

:require_file
if not exist "%~1" (
  echo [ERROR] Required file missing: %~1
  exit /b 1
)
exit /b 0
