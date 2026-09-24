@echo off
setlocal
cd /d "%~dp0"
call output-paths.cmd
rem Optional development reference build; normal launches keep the current build.
if not defined NIGHTFIRE_EXECUTABLE set "NIGHTFIRE_EXECUTABLE=build-windows\Debug\nightfire_diagnostic.exe"
if not exist "%NIGHTFIRE_EXECUTABLE%" (
  echo Build the diagnostic executable with build-windows.cmd first.
  exit /b 1
)
if not exist "game_files\default.xbe" (
  echo Missing game_files\default.xbe.
  exit /b 1
)
rem Diagnostic startup exploration: explicitly simulate GPU completion.
rem Set NIGHTFIRE_DIAGNOSTIC_FENCES=0 before running to preserve hardware waits.
if not defined NIGHTFIRE_DIAGNOSTIC_FENCES set "NIGHTFIRE_DIAGNOSTIC_FENCES=1"
if not defined RECOMP_WATCHDOG_SECS set "RECOMP_WATCHDOG_SECS=60"
set "RECOMP_TRACE_BUDGET=2000"
set "RECOMP_APU_TRACE=1"
rem Display actual guest pixels. This does not implement the GPU.
set "RECOMP_FB_WINDOW=1"
rem Capture prefixes are initialized by output-paths.cmd.

rem Survey GPU commands and enable the existing experimental software renderer.
set "RECOMP_PB_SCAN=1"
if not defined NIGHTFIRE_GRAPHICS_EXPERIMENT set "NIGHTFIRE_GRAPHICS_EXPERIMENT=1"
if "%NIGHTFIRE_GRAPHICS_EXPERIMENT%"=="1" (
  set "RECOMP_PB_EXEC=1"
  set "RECOMP_PB_EXEC_VERBOSE=1"
) else (
  set "RECOMP_PB_EXEC="
  set "RECOMP_PB_EXEC_VERBOSE="
)
rem Never substitute a synthetic raster test for the game's picture.
set "RECOMP_RASTER_TEST="
rem Diagnostic acknowledgment only; DSP programs are not executed.
if not defined NIGHTFIRE_DIAGNOSTIC_DSP set "NIGHTFIRE_DIAGNOSTIC_DSP=1"
"%NIGHTFIRE_EXECUTABLE%" > logs\nightfire-startup.log 2>&1
set "NIGHTFIRE_EXIT=%ERRORLEVEL%"
rem Saved older executables still write the original kernel filename.
if exist "xbox_kernel.log" move /y "xbox_kernel.log" "logs\xbox_kernel.log" >nul
echo Exit code: %NIGHTFIRE_EXIT%>> logs\nightfire-startup.log
type logs\nightfire-startup.log
exit /b %NIGHTFIRE_EXIT%
