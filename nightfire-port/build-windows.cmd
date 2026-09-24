@echo off
setlocal
cd /d "%~dp0"
call output-paths.cmd
if not defined NIGHTFIRE_BUILD_CONFIG set "NIGHTFIRE_BUILD_CONFIG=Debug"
if /i not "%NIGHTFIRE_BUILD_CONFIG%"=="Debug" if /i not "%NIGHTFIRE_BUILD_CONFIG%"=="RelWithDebInfo" (
  echo Supported build configurations: Debug or RelWithDebInfo.
  exit /b 1
)
where git >nul 2>nul
if errorlevel 1 (
  echo Git is required. Install Git for Windows, then retry.
  exit /b 1
)
where cmake >nul 2>nul
if errorlevel 1 (
  echo CMake is required. Install it or enable the Visual Studio CMake tools.
  exit /b 1
)
if not exist "..\xboxrecomp\.git" (
  git clone https://github.com/sp00nznet/xboxrecomp.git "..\xboxrecomp"
  if errorlevel 1 exit /b 1
  git -C "..\xboxrecomp" checkout 051a128df5ec27ef14f1ceaaead11c5457321eef
  if errorlevel 1 exit /b 1
)
for /f %%i in ('git -C "..\xboxrecomp" rev-parse HEAD') do set "NIGHTFIRE_TOOLKIT_REV=%%i"
if not "%NIGHTFIRE_TOOLKIT_REV%"=="051a128df5ec27ef14f1ceaaead11c5457321eef" (
  echo The existing xboxrecomp checkout differs from the tested revision.
  echo Use a fresh extracted project folder to avoid changing an existing checkout.
  exit /b 1
)
cmake -S . -B build-windows -G "Visual Studio 17 2022" -A x64 > logs\build-configure.log 2>&1
if errorlevel 1 (
  type logs\build-configure.log
  exit /b 1
)
cmake --build build-windows --config %NIGHTFIRE_BUILD_CONFIG% --parallel 2 > logs\build-compile.log 2>&1
if errorlevel 1 (
  type logs\build-compile.log
  exit /b 1
)
echo Build completed. Run run-windows.cmd to capture startup diagnostics.
