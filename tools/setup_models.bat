@echo off
setlocal EnableExtensions EnableDelayedExpansion

rem Windows helper mirroring tools/setup_models.sh (demo / unity / ue / dir / local)
set "SCRIPT_DIR=%~dp0"
set "REPO_ROOT=%SCRIPT_DIR%.."
set "MODEL_SRC=%REPO_ROOT%\model"

if not exist "%MODEL_SRC%\" (
  echo error: model directory not found: %MODEL_SRC%
  exit /b 1
)

set "CMD=%~1"
if "%CMD%"=="" set "CMD=demo"
shift

set "PROJECT="
set "DEST="

:parse
if "%~1"=="" goto run
if /i "%~1"=="--project" (
  set "PROJECT=%~2"
  shift
  shift
  goto parse
)
if /i "%~1"=="--dest" (
  set "DEST=%~2"
  shift
  shift
  goto parse
)
if /i "%~1"=="-h" goto usage
if /i "%~1"=="--help" goto usage
echo error: unknown argument: %~1
exit /b 1

:run
if /i "%CMD%"=="demo" goto demo
if /i "%CMD%"=="demo-android" goto demo_android
if /i "%CMD%"=="local" goto local
if /i "%CMD%"=="unity" goto unity
if /i "%CMD%"=="ue" goto ue
if /i "%CMD%"=="dir" goto dir
if /i "%CMD%"=="all" goto all
goto usage

:copy_gpu
set "OUTDIR=%~1"
if not exist "%OUTDIR%\" mkdir "%OUTDIR%"
for %%F in (
  magic_metal_speed_gpu_params.bin
  magic_metal_balanced_gpu_params.bin
  magic_gl_speed_gpu_params.bin
  magic_gl_balanced_gpu_params.bin
  magic_gles_speed_gpu_params.bin
  magic_gles_balanced_gpu_params.bin
  magic_vulkan_speed_gpu_params.bin
  magic_vulkan_balanced_gpu_params.bin
) do (
  if exist "%MODEL_SRC%\%%F" (
    copy /Y "%MODEL_SRC%\%%F" "%OUTDIR%\%%F" >nul
    echo [setup_models] copied %%F -^> %OUTDIR%\
  )
)
goto :eof

:demo
call :demo_android
echo [setup_models] iOS demo setup is macOS-only; skipped on Windows.
goto done

:demo_android
call :copy_gpu "%REPO_ROOT%\demo\android\app\src\main\assets\model"
goto :eof

:local
call :copy_gpu "%REPO_ROOT%\MagicSRModels"
echo [setup_models] then MC_Enable_SetModelDir(%REPO_ROOT%\MagicSRModels)
goto done

:unity
if "%PROJECT%"=="" (
  echo error: unity requires --project ^<UnityProjectRoot^>
  exit /b 1
)
call :copy_gpu "%PROJECT%\Assets\StreamingAssets\MagicSRModels"
echo [setup_models] Unity StreamingAssets ready.
goto done

:ue
if "%PROJECT%"=="" (
  echo error: ue requires --project ^<UEProjectRoot^>
  exit /b 1
)
call :copy_gpu "%PROJECT%\Content\MagicSRModels"
echo [setup_models] UE Content models ready. Runtime: call SetModelDir on device.
goto done

:dir
if "%DEST%"=="" (
  echo error: dir requires --dest ^<path^>
  exit /b 1
)
call :copy_gpu "%DEST%"
echo [setup_models] then MC_Enable_SetModelDir(%DEST%)
goto done

:all
call :demo_android
call :local
goto done

:usage
echo MagicSR model setup helper
echo.
echo Usage:
echo   tools\setup_models.bat demo
echo   tools\setup_models.bat local
echo   tools\setup_models.bat unity --project C:\path\to\UnityProject
echo   tools\setup_models.bat ue --project C:\path\to\UEProject
echo   tools\setup_models.bat dir --dest C:\path\to\MagicSRModels
echo   tools\setup_models.bat all
exit /b 0

:done
echo [setup_models] done.
exit /b 0
