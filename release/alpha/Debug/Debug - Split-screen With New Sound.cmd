@echo off
rem Nightfire PC alpha, debug: split-screen with the Action engine's own new sound. Not tried yet in split-screen.
rem Debug mode records everything the workshop records: see Debug\README.txt.
setlocal
cd /d "%~dp0.."
call "%~dp0..\engine\find-python.cmd" || (pause & exit /b 1)
%NF_PY% "%~dp0..\engine\launcher\nightfire.py" --mode debug --split-screen-sound %*
echo.
echo Nightfire closed. Press any key to close this window.
pause >nul
