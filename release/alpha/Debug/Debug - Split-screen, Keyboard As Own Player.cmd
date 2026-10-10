@echo off
rem Nightfire PC alpha, debug: split-screen with keyboard and mouse as player 1 on their own; controllers 1-3 become players 2-4.
rem Debug mode records everything the workshop records: see Debug\README.txt.
setlocal
cd /d "%~dp0.."
set "NF_KEYBOARD_PLAYER=own"
call "%~dp0..\engine\find-python.cmd" || (pause & exit /b 1)
%NF_PY% "%~dp0..\engine\launcher\nightfire.py" --mode debug --split-screen %*
echo.
echo Nightfire closed. Press any key to close this window.
pause >nul
