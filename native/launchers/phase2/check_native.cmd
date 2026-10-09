@echo off
rem Syntax/type check of native D3D sources without building the game:
rem   check_native.cmd nd3d_g1_state.c [more files]
rem Files are looked up in nightfire-driving-native\runtime\native.
setlocal
set "NATIVE=%~dp0..\..\nightfire-driving-native\runtime\native"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%NATIVE%"
cl /nologo /Zs /W3 /std:c11 /D_CRT_SECURE_NO_WARNINGS /DWIN32_LEAN_AND_MEAN /DNOMINMAX /I. /I..\lean /I..\gpu144 %*
exit /b %ERRORLEVEL%
