@echo off
rem Nightfire PC alpha: the whole game (Action and Driving linked in one window) with the PC settings.
cd /d "%~dp0"
call "%~dp0engine\find-python.cmd" || (pause & exit /b 1)
%NF_PY% "%~dp0engine\launcher\nightfire.py" --mode play %*
echo.
echo Nightfire closed. Press any key to close this window.
pause >nul
