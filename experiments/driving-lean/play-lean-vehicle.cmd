@echo off
rem Lean GPU-resident renderer: vehicle scene (dev start), up to 30 minutes. Close the game window to stop.
cd /d "%~dp0"
powershell -NoProfile -Command "if(Get-Process nightfire* -ErrorAction SilentlyContinue){exit 1}"
if errorlevel 1 (echo A Nightfire game is already running. & pause & exit /b 1)
python run-lean-scene.py vehicle --build lean --seconds 1800
