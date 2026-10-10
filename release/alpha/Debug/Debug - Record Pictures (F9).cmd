@echo off
rem Nightfire PC alpha, debug: Driving keeps the last 72 pictures; press F9 right after a glitch to save them.
rem Debug mode records everything the workshop records: see Debug\README.txt.
cd /d "%~dp0.."
call "%~dp0..\engine\find-python.cmd" || (pause & exit /b 1)
%NF_PY% "%~dp0..\engine\launcher\nightfire.py" --mode debug --record %*
echo.
echo Nightfire closed. Press any key to close this window.
pause >nul
