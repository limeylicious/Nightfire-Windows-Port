@echo off
rem TEST: the linked native game (as play-nightfire-native.cmd) with the F10 settings overlay (NF_OVERLAY=1).
rem F10 opens a small menu: brightness, picture shape (stretch or 4:3 with black bars), smooth or sharp
rem scaling, and a frame counter. Up/Down choose, Left/Right change, F10 or Esc closes; the game gets no
rem input while it is open. Settings are saved to NightfirePC\overlay-settings.ini in the local app data folder.
rem With the overlay on, the picture is drawn at the window's real size (the game still renders 640x480).
rem Borderless fullscreen (the same as play-nightfire-native-overlay.cmd --fullscreen).
cd /d "%~dp0"
powershell -NoProfile -Command "if(Get-Process nightfire* -ErrorAction SilentlyContinue){exit 1}"
if errorlevel 1 (echo A Nightfire game is already running. & pause & exit /b 1)
set NF_OVERLAY=1
python play-nightfire-native.py --fullscreen %*
echo.
echo Linked session finished. Press any key to close this window.
pause >nul
