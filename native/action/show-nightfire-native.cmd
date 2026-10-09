@echo off
setlocal
cd /d "%~dp0"
call output-paths.cmd
set "NIGHTFIRE_EXECUTABLE=%~dp0build-native\RelWithDebInfo\nightfire_native.exe"
rem Checkpoint 242 crash capture: one folder per play session under sessions\.
for /f %%t in ('powershell -NoProfile -Command "Get-Date -Format yyyyMMdd-HHmmss"') do set "NIGHTFIRE_SESSION_STAMP=%%t"
if not exist "sessions" mkdir "sessions"
set "NIGHTFIRE_SESSION242=1"
rem NATIVE GRAPHICS TEST (native-driving section 6): our own D3D routines draw through Direct3D 11.
if not defined LEAN_NATIVE_D3D set "LEAN_NATIVE_D3D=1"
if not defined LEAN_PRESENT_VSYNC set "LEAN_PRESENT_VSYNC=1"
rem Per-call checkpoint only for the routines something watches (native-driving/action-native/README.md).
if not defined NIGHTFIRE_FAST_CALLS set "NIGHTFIRE_FAST_CALLS=1"
rem 60 Hz pacer keeps its ideal timeline after small timer overshoots (holds 60 updates/s, not ~58).
if not defined NIGHTFIRE_PACER_TIMELINE set "NIGHTFIRE_PACER_TIMELINE=1"
rem Native graphics never uses the push-buffer renderer; without it the per-call drain poll is not needed.
if not defined NIGHTFIRE_GRAPHICS_EXPERIMENT set "NIGHTFIRE_GRAPHICS_EXPERIMENT=0"
rem No fake graphics chip: the register-poll thread is not started (KeTickCount on a 1 ms timer).
if not defined LEAN_NO_REGPOLL set "LEAN_NO_REGPOLL=1"
set "NIGHTFIRE_SESSION_DIR=%~dp0sessions\%NIGHTFIRE_SESSION_STAMP%-action-native"
if not exist "%NIGHTFIRE_EXECUTABLE%" (
  echo The native build is missing. Run build-native.cmd.
  pause
  exit /b 1
)
rem Normal startup: movies, menus and mission briefing are retained.
set "NIGHTFIRE_DEV_START_EXCHANGE="
set "NIGHTFIRE_DEV_MISSION137="
set "NIGHTFIRE_DEV_EXCHANGE_SECTION="
set "NIGHTFIRE_MENU_MISSIONS139=1"
set "NIGHTFIRE_ACK_BACKOFF136=0"
set "NIGHTFIRE_CONSUMER135=0"
set "NIGHTFIRE_SHADOW_SUBMIT135=0"
set "NIGHTFIRE_DEV_INVULNERABLE134=0"
set "NIGHTFIRE_DIRECT_MOUSE122=1"
set "NIGHTFIRE_MODERN_CONTROLS140=1"
set "NIGHTFIRE_RGBA_MIPS129=1"
set "NIGHTFIRE_RESIDENT_MAIN130=1"
set "NIGHTFIRE_SWIZZLED_SHADOW131=1"
set "NIGHTFIRE_SWIZZLED_SHADOW132=1"
set "NIGHTFIRE_MATERIAL_TEXTURES132=0"
set "NIGHTFIRE_SHADOW_LAYOUT131=0"
set "NIGHTFIRE_SHADOW_LAYOUT131_TRIGGER="
set "NIGHTFIRE_SHADOW_LAYOUT131_DIR="
set "NIGHTFIRE_DEFER_MAIN128=0"
rem Keep optional game-only history in a dedicated local folder for crash reports.
if not exist "logs\game-history" mkdir "logs\game-history"
set "NIGHTFIRE_HISTORY116_DIR=%~dp0logs\game-history"
set "NIGHTFIRE_VERTEX_BATCH92=1"
set "NIGHTFIRE_GAME_RATE94=1"
set "NIGHTFIRE_MP_RATE231=1"
set "NIGHTFIRE_BOT_TRACE231=0"
set "NIGHTFIRE_PAUSE_RATE133=1"
set "NIGHTFIRE_TEXTURE_REUSE133=0"
set "NIGHTFIRE_TEXTURE_REUSE133_TIMING=0"
set "NIGHTFIRE_GEOMETRY133=0"
set "NIGHTFIRE_GPU_FALLBACK96=1"
set "NIGHTFIRE_GPU_BLEND97=1"
set "NIGHTFIRE_SHADER_CACHE99=1"
set "NIGHTFIRE_TEXTURE_DECODE100=1"
set "NIGHTFIRE_SURFACE_COHERENCE108=1"
set "NIGHTFIRE_EARLY_SUBMIT113=512"
set "NIGHTFIRE_TEXTURE_EQUAL114=1"
set "NIGHTFIRE_COLOR_REUSE110=0"
set "NIGHTFIRE_SHADER_CACHE99_DIR=%~dp0cache\shaders"
set "NIGHTFIRE_GAME_PACING93=0"
set "NIGHTFIRE_COMPACT_VERTEX91=0"
set "NIGHTFIRE_DIRECT_VERTEX88=1"
set "NIGHTFIRE_DIRECT_DEFAULT90=1"
set "NIGHTFIRE_INDEX_GATHER=1"
set "NIGHTFIRE_REPLAY_ROUTE="
set "NIGHTFIRE_INPUT_TEST="
set "NIGHTFIRE_INPUT_TEST_DELAY_MS="
set "NIGHTFIRE_INPUT_TEST_READY_START="
set "NIGHTFIRE_INPUT_TEST_FILE="
set "NIGHTFIRE_INPUT_TEST_FILE_ONLY="
set "NIGHTFIRE_SKY_TRACE="
set "NIGHTFIRE_WORLD_DRAW_CAPTURE="
set "NIGHTFIRE_WORLD_CAPTURE_IMAGES="
set "NIGHTFIRE_PROFILE="
set "NIGHTFIRE_GPU_DIAGNOSTICS="
set "NIGHTFIRE_GPU_FALLBACK_SURVEY="
set "NIGHTFIRE_AUDIO_ISOLATE_STREAM="
set "NIGHTFIRE_POSE_CAPTURE="
set "NIGHTFIRE_POSE_WEAPON="
rem Existing unsupported driving-segment bypass, not a movie skip.
set "NIGHTFIRE_MENU_PREVIEW=1"
set "NIGHTFIRE_VERTEX_PROGRAM=1"
set "NIGHTFIRE_DEPTH=1"
set "NIGHTFIRE_HW_GPU=1"
set "NIGHTFIRE_GPU_VERTEX=1"
set "NIGHTFIRE_GPU_SCREEN=1"
if not defined NIGHTFIRE_GPU_MOVIE set "NIGHTFIRE_GPU_MOVIE=1"
set "NIGHTFIRE_TEXTURE_FILTER=1"
set "NIGHTFIRE_TEXTURE_CACHE=1"
set "NIGHTFIRE_NATIVE_INPUT=1"
set "NIGHTFIRE_NATIVE_VIDEO_AUDIO=1"
if not defined NIGHTFIRE_NATIVE_GAME_AUDIO set "NIGHTFIRE_NATIVE_GAME_AUDIO=1"
if not defined NIGHTFIRE_NATIVE_GAME_STREAM set "NIGHTFIRE_NATIVE_GAME_STREAM=1"
if not defined NIGHTFIRE_NATIVE_GAME_SPATIAL set "NIGHTFIRE_NATIVE_GAME_SPATIAL=1"
if not defined RECOMP_WATCHDOG_SECS set "RECOMP_WATCHDOG_SECS=1800"
rem Development start (tests): NIGHTFIRE_NATIVE_TEST_MISSION=07000005 (The Exchange) etc. skips menus and briefing.
if defined NIGHTFIRE_NATIVE_TEST_MISSION (
  set "NIGHTFIRE_DEV_START_EXCHANGE=1"
  set "NIGHTFIRE_DEV_EXCHANGE_SECTION=1"
  set "NIGHTFIRE_DEV_MISSION137=%NIGHTFIRE_NATIVE_TEST_MISSION%"
)
echo Nightfire Action - NATIVE GRAPHICS TEST build (nightfire-port-native).
echo If the game crashes or freezes, the report is saved in %NIGHTFIRE_SESSION_DIR%
echo Normal startup: let the movies play, then press Enter at START.
echo Menus: arrows move, Z selects, X returns. Choose Multiplayer and join as Player 1.
echo Tested setup: Arena, Skyrail, Bond, one AI Snow Guard, then Continue and Start Game.
echo Skyrail: native mouse and 60 Hz pacing enabled. Bot state repair needs combat verification.
echo Gameplay uses 1/60-second steps, paced to avoid high-FPS overspeed.
echo The normal PAL pause menu now follows its original 50Hz timing.
echo Reuses compiled graphics shaders between runs to reduce repeat hitches.
echo The 60Hz timing trial is retained; low-FPS slowdown is still unresolved.
echo Native mouse aiming is enabled, including guided rocket input for testing.
echo GPU residency and the shadow storage repair are enabled; other visual defects remain under investigation.
echo PC controls: WASD move, mouse look, LMB fire, RMB/T aim, E use, R reload, Ctrl crouch, Space jump.
echo Q next weapon, G next gadget, F alternate fire/mode, Tab objectives, Enter pause.
echo Use and reload share the original contextual action. Click/F1 captures mouse; Esc releases.
echo The separate opening driving segment remains unsupported and bypassed.
echo Close the game window to stop. This preview has a thirty-minute safety timeout.
call run-windows.cmd > logs\show-nightfire-native-console.log 2>&1
set "NIGHTFIRE_OPTIMIZED_EXIT=%ERRORLEVEL%"
if exist "logs\nightfire-startup.log" if exist "%NIGHTFIRE_SESSION_DIR%" copy /y "logs\nightfire-startup.log" "%NIGHTFIRE_SESSION_DIR%\game.log" >nul
echo Session ended. Logs: logs\nightfire-startup.log and logs\show-nightfire-native-console.log. Session report: %NIGHTFIRE_SESSION_DIR%
exit /b %NIGHTFIRE_OPTIMIZED_EXIT%




