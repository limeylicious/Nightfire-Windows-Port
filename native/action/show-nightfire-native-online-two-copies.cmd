@echo off
setlocal
cd /d "%~dp0"
rem TWO COPIES ON THIS PC (online play test, netplay1, 2026-10-11): copy 1 hosts, copy 2 joins it
rem over 127.0.0.1, and at the end the two copies' game memory is compared frame by frame.
rem Copy 2 runs from ..\nightfire-port-native-seat2: folder links (junctions) to this tree's
rem build, game files and sources, with its own logs, sessions, shader cache and online folder.
rem Deleting that folder (rmdir /s /q) removes the links, not what they point to.
rem Two copies need about twice the processor: plug the laptop in.
tasklist /fo csv /nh | findstr /i "nightfire" >nul
if not errorlevel 1 (
  echo A Nightfire game is already running. Close it first, then start this again.
  pause
  exit /b 1
)
set "TREE=%~dp0"
set "SEAT2=%~dp0..\nightfire-port-native-seat2"
if not exist "%SEAT2%" mkdir "%SEAT2%"
for /d %%d in ("%~dp0*") do call :link "%%~nxd" || goto :fail
for %%f in (show-nightfire-native-4pads.cmd run-windows.cmd output-paths.cmd show-nightfire-native-online.cmd) do (
  copy /y "%~dp0%%f" "%SEAT2%\%%f" >nul || goto :fail
)
echo.
echo ==== TWO COPIES ONLINE (test) ====
echo Copy 1 (host, player 1) opens first, then copy 2 (player 2) joins it; each game window's
echo title says which copy it is. Both windows show the same match, and either player's presses
echo move the menus: use copy 1 for the menus, and click copy 2 only for player 2 (to join at
echo character select, then to move player 2). Close copy 2 first, then copy 1.
echo.
set "NF_WINDOW_LABEL=COPY 1 (host, player 1)"
start "Nightfire online - copy 1 (host)" cmd /c ""%~dp0show-nightfire-native-online.cmd" host 2 nopause"
timeout /t 4 /nobreak >nul
set "NF_WINDOW_LABEL=COPY 2 (player 2)"
call "%SEAT2%\show-nightfire-native-online.cmd" join 127.0.0.1 nopause
:wait_host
timeout /t 2 /nobreak >nul
tasklist /fo csv /nh | findstr /i "nightfire_native" >nul
if not errorlevel 1 goto :wait_host
set "RESULT=%~dp0online\TWO-COPIES-RESULT.txt"
python "%~dp0scripts\nf_state_diff.py" "%~dp0online\state.nfh" "%SEAT2%\online\state.nfh" > "%RESULT%" 2>&1
echo.>> "%RESULT%"
echo Copy 1 (host):>> "%RESULT%"
findstr /l /c:"SESSION ENDED" /c:"DESYNC" /c:"finished:" /c:"input delay" "%~dp0online\net.log" >> "%RESULT%" 2>nul
findstr /l /c:"resumed after a stall" /c:"[NETPLAY] match" "%~dp0online\game.log" >> "%RESULT%" 2>nul
echo Copy 2 (guest):>> "%RESULT%"
findstr /l /c:"SESSION ENDED" /c:"DESYNC" /c:"finished:" "%SEAT2%\online\net.log" >> "%RESULT%" 2>nul
findstr /l /c:"resumed after a stall" /c:"[NETPLAY] match" "%SEAT2%\online\game.log" >> "%RESULT%" 2>nul
echo.
type "%RESULT%"
echo.
echo Result saved in %RESULT%
pause
exit /b 0

:fail
echo Could not set up %SEAT2%, so nothing was started.
pause
exit /b 1

rem Copy 2's folder: a link for every folder of this tree except the ones each copy keeps for itself.
:link
for %%x in (logs sessions cache online lockstep recovery) do if /i "%~1"=="%%x" exit /b 0
if exist "%SEAT2%\%~1" exit /b 0
mklink /J "%SEAT2%\%~1" "%TREE%%~1" >nul
exit /b %ERRORLEVEL%
