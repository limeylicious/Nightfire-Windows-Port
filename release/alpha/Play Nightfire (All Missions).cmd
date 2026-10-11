@echo off
rem Nightfire PC alpha: as Play Nightfire.cmd, with every mission open in the mission menu.
rem Needs an Action engine built after 10 October 2026 (README.txt); older engines keep the progression.
cd /d "%~dp0"
call "%~dp0engine\find-python.cmd" || (pause & exit /b 1)
%NF_PY% "%~dp0engine\launcher\nightfire.py" --mode play --all-missions %*
echo.
echo Nightfire closed. Press any key to close this window.
pause >nul
