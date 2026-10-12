@echo off
setlocal
cd /d "%~dp0"
rem TWO COPIES ON THIS PC (online play test, netplay1 + lobby1 + saves1, 2026-10-11): copy 1 hosts,
rem copy 2 joins it over 127.0.0.1, and at the end the two copies' game memory is compared frame by frame.
rem   show-nightfire-native-online-two-copies.cmd         both open the online lobby: host in COPY 1,
rem                                                       join in COPY 2, then Start match in COPY 1
rem   show-nightfire-native-online-two-copies.cmd auto    copy 1 hosts and copy 2 joins by themselves
rem Each copy keeps its own save folder (online\saves in its own tree), so neither touches your
rem profiles in game_files\UDATA or the other copy's files.
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
if /i not "%~1"=="auto" goto :lobby
echo.
echo ==== TWO COPIES ONLINE (test) ====
echo Copy 1 (host, player 1) opens first, then copy 2 (player 2) joins it; each game window's
echo title says which copy it is. Both windows show the same menus, and either player's presses
echo move them: use copy 1 for the menus, and click copy 2 only for player 2 (to join at
echo character select, then to move player 2). If the game asks for a profile, make it in copy 1.
echo In the match each window shows only its own player's view; F7 shows the whole split screen.
echo Close copy 2 first, then copy 1.
echo.
set "NF_WINDOW_LABEL=COPY 1 (host, player 1)"
start "Nightfire online - copy 1 (host)" cmd /c ""%~dp0show-nightfire-native-online.cmd" host 2 nopause"
timeout /t 4 /nobreak >nul
set "NF_WINDOW_LABEL=COPY 2 (player 2)"
call "%SEAT2%\show-nightfire-native-online.cmd" join 127.0.0.1 nopause
goto :wait_host

:lobby
echo.
echo ==== TWO COPIES ONLINE, LOBBY (test) ====
echo Each copy opens a "Nightfire Online" window, side by side. In COPY 1's window press Host a
echo match. In COPY 2's window pick COPY 1's match and press Join. Then press Start match in
echo COPY 1's window. Both windows then show the same menus, and either player's presses move
echo them: use copy 1 for the menus, and click copy 2 only for player 2. If the game asks
echo for a profile, make it in copy 1. In the match each window shows only its own player's
echo view; F7 shows the whole split screen. Close copy 2 first, then copy 1.
echo.
set "NF_WINDOW_LABEL=COPY 1"
set "NF_NET_NAME=COPY 1"
start "Nightfire online - copy 1" cmd /c ""%~dp0show-nightfire-native-online.cmd" lobby "" nopause"
timeout /t 4 /nobreak >nul
set "NF_WINDOW_LABEL=COPY 2"
set "NF_NET_NAME=COPY 2"
call "%SEAT2%\show-nightfire-native-online.cmd" lobby "" nopause
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
