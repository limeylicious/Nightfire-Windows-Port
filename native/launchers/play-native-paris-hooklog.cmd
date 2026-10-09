@echo off
rem NATIVE GRAPHICS test build (native-driving phase 2): the Xbox graphics library is
rem replaced by our own native routines drawing with DirectX 11 (LEAN_NATIVE_D3D=1).
rem No graphics-chip command stream exists in this build. Same guided settings as
rem play-lean-paris-guided.cmd (smooth mode, native sound, PAL60, 62.5 game steps/s).
rem TEST 2026-10-08: logs every mission-script trigger and auto-aim lock change, and each rifle
rem pull (LEAN_CALL_CHAIN, LEAN_RIFLE_PROBE), to find why the construction-site hook cannot be shot.
rem Session folder with crash/freeze reports and F9 marks: nightfire-driving-native\sessions
cd /d "%~dp0..\nightfire-driving-native"
powershell -NoProfile -Command "if(Get-Process nightfire* -ErrorAction SilentlyContinue){exit 1}"
if errorlevel 1 (echo A Nightfire game is already running. & pause & exit /b 1)
python run-lean-guided.py paris --env LEAN_NATIVE_D3D=1 --env LEAN_DISPLAY_GAMMA=0.8 --env LEAN_FPS_COUNTER=1 --env LEAN_INTERP=1 --env LEAN_PRESENT_VSYNC=1 --env LEAN_AUDIO_NATIVE=1 --env LEAN_PRECISE_TICK=1 --env LEAN_AUDIO_LEAD=1600 --env LEAN_STRM_FIX=1 --env LEAN_FIX_CMPSD=audio --env LEAN_VBLANK_HZ=60 --env LEAN_VIDEO_MODE=pal60 --env LEAN_INTERP_GAMECLOCK=1 --env LEAN_AUDIO_EMIT_LOG=1 --env LEAN_FIX_CLIPSEARCH=1 --env LEAN_AUDIO_NO_CHIP=1 --env LEAN_AUDIO_OWN_DSP=1 --env LEAN_CALL_CHAIN=0,9999999,B9590,B9500,3EB50,B72F0,B7310,B7A40,404C0,B66F0,B6720,B67A0,22DD0,C1380,C0F80 --env LEAN_CALL_CHAIN_NOSTACK=1 --env LEAN_RIFLE_PROBE=1
echo.
echo Session saved. Press any key to close this window.
pause >nul
