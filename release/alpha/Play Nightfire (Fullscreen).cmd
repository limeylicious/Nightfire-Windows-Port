@echo off
rem Nightfire PC alpha: as Play Nightfire.cmd, in borderless fullscreen.
cd /d "%~dp0"
call "%~dp0engine\find-python.cmd" || (pause & exit /b 1)
%NF_PY% "%~dp0engine\launcher\nightfire.py" --mode play --fullscreen %*
echo.
echo Nightfire closed. Press any key to close this window.
pause >nul
