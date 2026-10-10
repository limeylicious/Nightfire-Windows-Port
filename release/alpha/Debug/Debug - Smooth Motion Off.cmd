@echo off
rem Nightfire PC alpha, debug: Driving shows only the game steps (no in-between pictures), to compare against Smooth Motion.
rem Debug mode records everything the workshop records: see Debug\README.txt.
cd /d "%~dp0.."
call "%~dp0..\engine\find-python.cmd" || (pause & exit /b 1)
%NF_PY% "%~dp0..\engine\launcher\nightfire.py" --mode debug --smooth-off %*
echo.
echo Nightfire closed. Press any key to close this window.
pause >nul
