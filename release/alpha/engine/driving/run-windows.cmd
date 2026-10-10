@echo off
setlocal
cd /d "%~dp0"
if not exist logs mkdir logs
if not exist saves mkdir saves
set "RECOMP_CMDLINE="
rem Original complete first-Paris handoff captured from saved action141 execution.
rem Explicit no-launch flag retains the checkpoint143 null-page diagnostic.
if "%DRIVING_NO_LAUNCH144%"=="1" (
  set "DRIVING_LAUNCH_PAYLOAD144="
) else if not defined DRIVING_LAUNCH_PAYLOAD144 (
  set "DRIVING_LAUNCH_PAYLOAD144=%~dp0launch-data\original-first-paris144.bin"
)
if not defined RECOMP_WATCHDOG_SECS set "RECOMP_WATCHDOG_SECS=20"
set "RECOMP_TRACE_BUDGET=200"
set "RECOMP_KERNEL_LOG_BUDGET=100"
set "RECOMP_FB_WINDOW="
set "RECOMP_RASTER_TEST="
set "DRIVING_PB_SYNC=1"
set "RECOMP_PB_EXEC=1"
set "RECOMP_VBLANK=1"
rem Explicit bootstrap device model and scoped startup-only DSP simulation.
rem Set either to 0 to retain its original wait/failure for diagnosis.
if "%RECOMP_AC97_READY%"=="0" (
  set "RECOMP_AC97_READY="
) else if not defined RECOMP_AC97_READY (
  set "RECOMP_AC97_READY=1"
)
if not defined DRIVING_DIAGNOSTIC_DSP149 set "DRIVING_DIAGNOSTIC_DSP149=1"
rem Never use the toolkit's allocation-dependent blanket doorbell clearing.
set "RECOMP_APU_DSP_ACK="
rem Experimental software execution and 50Hz diagnostic IRQ delivery, not a complete renderer.
rem Bootstrap only: no action-engine address hooks, game substitutions or menu bypass.
echo [RUN] watchdog=%RECOMP_WATCHDOG_SECS% synchronous_gpu=%DRIVING_PB_SYNC% software_executor=%RECOMP_PB_EXEC% diagnostic_vblank=%RECOMP_VBLANK% > logs\driving-startup.log
echo [RUN] codec_ready=%RECOMP_AC97_READY% scoped_dsp_simulation=%DRIVING_DIAGNOSTIC_DSP149% no_DSP_execution=1 >> logs\driving-startup.log
if not defined DRIVING_EXECUTABLE set "DRIVING_EXECUTABLE=%~dp0build-windows\RelWithDebInfo\nightfire_driving.exe"
"%DRIVING_EXECUTABLE%" >> logs\driving-startup.log 2>&1
set "DRIVING_RESULT=%ERRORLEVEL%"
if exist xbox_kernel.log move /y xbox_kernel.log logs\xbox_kernel.log >nul
echo Exit code: %DRIVING_RESULT%>>logs\driving-startup.log
exit /b %DRIVING_RESULT%
