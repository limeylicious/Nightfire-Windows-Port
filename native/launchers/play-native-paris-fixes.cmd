@echo off
rem NATIVE GRAPHICS test build (native-driving phase 2): the Xbox graphics library is
rem replaced by our own native routines drawing with DirectX 11 (LEAN_NATIVE_D3D=1).
rem No graphics-chip command stream exists in this build. Same guided settings as
rem play-lean-paris-guided.cmd (smooth mode, native sound, PAL60, 62.5 game steps/s).
rem TEST 2026-10-08: plus the hardware-exact rounding/compare fixes (LEAN_FIX_CVTSS2SI, LEAN_FIX_FIST,
rem LEAN_FIX_SAHF, LEAN_FIX_CMPSD=audio,d3d) for the ramp hinge that could not be shot.
rem Session folder with crash/freeze reports and F9 marks: nightfire-driving-native\sessions
cd /d "%~dp0..\nightfire-driving-native"
powershell -NoProfile -Command "if(Get-Process nightfire* -ErrorAction SilentlyContinue){exit 1}"
if errorlevel 1 (echo A Nightfire game is already running. & pause & exit /b 1)
python run-lean-guided.py paris --env LEAN_NATIVE_D3D=1 --env LEAN_DISPLAY_GAMMA=0.8 --env LEAN_FPS_COUNTER=1 --env LEAN_INTERP=1 --env LEAN_PRESENT_VSYNC=1 --env LEAN_AUDIO_NATIVE=1 --env LEAN_PRECISE_TICK=1 --env LEAN_AUDIO_LEAD=1600 --env LEAN_STRM_FIX=1 --env LEAN_FIX_CMPSD=audio,d3d --env LEAN_FIX_CVTSS2SI=1 --env LEAN_FIX_FIST=1 --env LEAN_FIX_SAHF=1 --env LEAN_CVTSS2SI_LOG=1 --env LEAN_VBLANK_HZ=60 --env LEAN_VIDEO_MODE=pal60 --env LEAN_INTERP_GAMECLOCK=1 --env LEAN_AUDIO_EMIT_LOG=1 --env LEAN_FIX_CLIPSEARCH=1 --env LEAN_AUDIO_NO_CHIP=1 --env LEAN_AUDIO_OWN_DSP=1
echo.
echo Session saved. Press any key to close this window.
pause >nul
