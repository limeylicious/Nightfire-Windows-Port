@echo off
rem Nightfire PC alpha: starts at the menus for local multiplayer, up to four players on this PC.
cd /d "%~dp0"
call "%~dp0engine\find-python.cmd" || (pause & exit /b 1)
%NF_PY% "%~dp0engine\launcher\nightfire.py" --mode play --split-screen %*
echo.
echo Nightfire closed. Press any key to close this window.
pause >nul
