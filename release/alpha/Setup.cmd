@echo off
rem Nightfire PC alpha: one-time set-up. Copies the engines the workshop last built and links the
rem game files (engine\setup.py explains each step). Run it again to pick up newer workshop builds.
cd /d "%~dp0"
call "%~dp0engine\find-python.cmd" || (pause & exit /b 1)
%NF_PY% "%~dp0engine\setup.py" %*
echo.
echo Press any key to close this window.
pause >nul
