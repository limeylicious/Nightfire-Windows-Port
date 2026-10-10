@echo off
rem Nightfire PC alpha, debug: starts at the Action menus; the opening Paris drive is bypassed.
rem Debug mode records everything the workshop records: see Debug\README.txt.
cd /d "%~dp0.."
call "%~dp0..\engine\find-python.cmd" || (pause & exit /b 1)
%NF_PY% "%~dp0..\engine\launcher\nightfire.py" --mode debug --skip-opening-drive %*
echo.
echo Nightfire closed. Press any key to close this window.
pause >nul
