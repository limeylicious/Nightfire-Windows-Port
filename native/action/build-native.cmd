@echo off
setlocal
cd /d "%~dp0"
if not exist logs mkdir logs
rem Native-graphics work copy of the Action engine (separate tree and build dir).
set "PATH=C:\Program Files\CMake\bin;%PATH%"
for /f %%i in ('git -C "..\xboxrecomp" rev-parse HEAD') do set "NIGHTFIRE_TOOLKIT_REV=%%i"
if not "%NIGHTFIRE_TOOLKIT_REV%"=="051a128df5ec27ef14f1ceaaead11c5457321eef" exit /b 1
rem Native D3D: regenerate the Action library from the Driving sources, then rename
rem the replaced generated routines (idempotent).
python scripts\nd3d_action_translate.py > logs\build-native-prep.log 2>&1
if errorlevel 1 exit /b 1
python scripts\nd3d_action_rename.py --apply >> logs\build-native-prep.log 2>&1
if errorlevel 1 exit /b 1
rem Native sound: call-site hook for the DirectSound entry points (idempotent).
python scripts\nds_action_hook.py >> logs\build-native-prep.log 2>&1
if errorlevel 1 exit /b 1
rem Per-call checkpoint filter (NIGHTFIRE_FAST_CALLS; idempotent).
python scripts\nf_fast_calls_hook.py >> logs\build-native-prep.log 2>&1
if errorlevel 1 exit /b 1
rem PC Graphics page (runtime/native_action/pcg_menu.c; idempotent).
python scripts\pcg_menu_rename.py --apply >> logs\build-native-prep.log 2>&1
if errorlevel 1 exit /b 1
rem Profile crash fix: rcr in the 64-bit divide helpers (scripts/nf_rcr_fix.py; idempotent).
python scripts\nf_rcr_fix.py --apply >> logs\build-native-prep.log 2>&1
if errorlevel 1 exit /b 1
set /p NATIVE_ARGS=<native-cmake-args.txt
cmake -S . -B build-native -G "Visual Studio 17 2022" -A x64 -C native-cache.cmake -DNIGHTFIRE_HOST_EXE_NAME:STRING=nightfire_native -DNIGHTFIRE_NATIVE_D3D=ON > logs\build-configure.log 2>&1
if errorlevel 1 exit /b 1
cmake --build build-native --config RelWithDebInfo --target nightfire_diagnostic --parallel 2 > logs\build-compile.log 2>&1
exit /b %ERRORLEVEL%
