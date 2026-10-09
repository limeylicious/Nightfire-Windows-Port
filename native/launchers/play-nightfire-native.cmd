@echo off
rem Nightfire with both engines native and linked (Action <-> Driving), as on the Xbox.
rem Starts the Action engine (native graphics + sound); when the game starts a driving mission
rem the Driving engine takes over with the game's own hand-over data, then returns to Action.
rem One window for the whole session (one_window_host.py; --two-windows = old separate windows, --fullscreen = borderless).
rem The opening Paris drive is played, not skipped. Notes: native-driving/action-native/README.md
cd /d "%~dp0"
powershell -NoProfile -Command "if(Get-Process nightfire* -ErrorAction SilentlyContinue){exit 1}"
if errorlevel 1 (echo A Nightfire game is already running. & pause & exit /b 1)
python play-nightfire-native.py %*
echo.
echo Linked session finished. Press any key to close this window.
pause >nul
