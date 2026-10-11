@echo off
rem Nightfire PC alpha set-up. Builds both engines from the game files (in the owner's workshop:
rem copies the engines it last built) and links the game files (engine\setup.py explains each
rem step). Run it again to pick up newer source or workshop builds.
cd /d "%~dp0"
call "%~dp0engine\find-python.cmd" || (pause & exit /b 1)
%NF_PY% "%~dp0engine\setup.py" %*
echo.
echo Press any key to close this window.
pause >nul
