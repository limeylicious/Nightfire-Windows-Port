@echo off
setlocal
cd /d "%~dp0"
if not exist logs mkdir logs
rem Same installed prerequisites and two-worker VS build as the action project.
set "PATH=C:\Program Files\CMake\bin;%PATH%"
for /f %%i in ('git -C "..\xboxrecomp" rev-parse HEAD') do set "DRIVING_TOOLKIT_REV=%%i"
if not "%DRIVING_TOOLKIT_REV%"=="051a128df5ec27ef14f1ceaaead11c5457321eef" exit /b 1
cmake -S . -B build-windows -G "Visual Studio 17 2022" -A x64 > logs\build-configure.log 2>&1
if errorlevel 1 exit /b 1
cmake --build build-windows --config RelWithDebInfo --parallel 2 > logs\build-compile.log 2>&1
exit /b %ERRORLEVEL%
