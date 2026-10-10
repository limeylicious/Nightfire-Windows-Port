@echo off
setlocal
cd /d "%~dp0"
if not exist logs mkdir logs
rem Native-graphics work copy of the lean Driving build (separate tree and build dir).
set "PATH=C:\Program Files\CMake\bin;%PATH%"
for /f %%i in ('git -C "..\xboxrecomp" rev-parse HEAD') do set "DRIVING_TOOLKIT_REV=%%i"
if not "%DRIVING_TOOLKIT_REV%"=="051a128df5ec27ef14f1ceaaead11c5457321eef" exit /b 1
rem Native D3D: rename the replaced generated routines (idempotent) and regenerate the glue.
python scripts\native_d3d_rename.py --apply > logs\build-native-prep.log 2>&1
if errorlevel 1 exit /b 1
python scripts\native_d3d_glue.py >> logs\build-native-prep.log 2>&1
if errorlevel 1 exit /b 1
rem Hook fix: frndint rounds by the x87 control word (scripts\nf_frndint_fix.py; idempotent).
python scripts\nf_frndint_fix.py --apply >> logs\build-native-prep.log 2>&1
if errorlevel 1 exit /b 1
cmake -S . -B build-native -G "Visual Studio 17 2022" -A x64 -DDRIVING_LEAN_RENDERER=ON -DDRIVING_FAST_BOOTSTRAP_BUILD=OFF -DDRIVING_REGION_CACHE445=ON -DDRIVING_NATIVE_D3D=ON %NATIVE_CMAKE_ARGS% > logs\build-configure.log 2>&1
if errorlevel 1 exit /b 1
cmake --build build-native --config RelWithDebInfo --parallel 2 > logs\build-compile.log 2>&1
exit /b %ERRORLEVEL%
