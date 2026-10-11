@echo off
setlocal
cd /d "%~dp0"
rem ONLINE PLAY TEST (sound and multiplayer thread, netplay1, 2026-10-11). Every PC runs the same
rem match from boot and only controller presses travel between them (runtime\native_action\nf_net.c).
rem   show-nightfire-native-online.cmd host [PCs]     host a match for PCs (2 to 4, this one included)
rem   show-nightfire-native-online.cmd join ADDRESS   join the match at ADDRESS (name or IP, optional :port)
rem Without arguments it asks. Every PC needs the same build, the same game files and this launcher.
rem The host's PC must let UDP port 47610 in: Windows asks the first time (allow it); over the
rem internet the host's router has to forward that port. Two copies on one PC: use
rem show-nightfire-native-online-two-copies.cmd.
rem Online play needs NIGHTFIRE_LOCKSTEP's frame-counted clocks; no other launcher sets NF_NET.
set "ONLINE_ROLE=%~1"
set "ONLINE_ARG=%~2"
if "%ONLINE_ROLE%"=="" (
  echo Online match: H = host it on this PC, J = join a match someone else hosts.
  choice /c HJ /n /m "Host or join? [H/J] "
  if errorlevel 2 (set "ONLINE_ROLE=join") else (set "ONLINE_ROLE=host")
)
if /i "%ONLINE_ROLE%"=="host" goto :host
if /i "%ONLINE_ROLE%"=="join" goto :join
echo Use: %~nx0 host [PCs]   or   %~nx0 join ADDRESS
pause
exit /b 1

:host
if "%ONLINE_ARG%"=="" set /p "ONLINE_ARG=How many PCs, this one included (2-4)? "
if "%ONLINE_ARG%"=="" set "ONLINE_ARG=2"
set "NF_NET=host"
set "NF_NET_PLAYERS=%ONLINE_ARG%"
set "NF_NET_ADDR="
goto :start

:join
if "%ONLINE_ARG%"=="" set /p "ONLINE_ARG=The host's address (name or IP, optional :port): "
if "%ONLINE_ARG%"=="" (
  echo No address given, so nothing was started.
  pause
  exit /b 1
)
set "NF_NET=join"
set "NF_NET_ADDR=%ONLINE_ARG%"
set "NF_NET_PLAYERS="
goto :start

:start
rem The game window's title says which side this is (the two-copies test names each copy).
if not defined NF_WINDOW_LABEL if /i "%NF_NET%"=="host" set "NF_WINDOW_LABEL=online host (player 1)"
if not defined NF_WINDOW_LABEL set "NF_WINDOW_LABEL=online (joined %NF_NET_ADDR%)"
set "NIGHTFIRE_LOCKSTEP=1"
set "LEAN_NO_REGPOLL=1"
rem No thirty-minute safety stop in an online match.
set "RECOMP_WATCHDOG_SECS=0"
set "ONLINE_DIR=%~dp0online"
if not exist "%ONLINE_DIR%" mkdir "%ONLINE_DIR%"
rem Every PC starts from the same empty save folder, so the copies start identical. Your own
rem saves (%LOCALAPPDATA%\xboxrecomp) are not used or touched; a profile made here lasts one match.
if exist "%ONLINE_DIR%\saves" rmdir /s /q "%ONLINE_DIR%\saves"
mkdir "%ONLINE_DIR%\saves"
set "NIGHTFIRE_SAVE_DIR=%ONLINE_DIR%\saves"
rem What happened, for checking afterwards: the network log, every agreed press (the match can be
rem replayed on one PC) and the game memory hashes (scripts\nf_state_diff.py compares two PCs').
del /q "%ONLINE_DIR%\net.log" "%ONLINE_DIR%\presses.nfi" "%ONLINE_DIR%\state.nfh" "%ONLINE_DIR%\game.log" 2>nul
set "NF_NET_LOG=%ONLINE_DIR%\net.log"
set "NF_INPUT_RECORD=%ONLINE_DIR%\presses.nfi"
set "NF_INPUT_REPLAY="
set "NF_STATE_HASH=%ONLINE_DIR%\state.nfh"
echo.
echo ==== ONLINE MATCH (test): %NF_NET% %ONLINE_ARG% ====
echo A small "Nightfire Online" window shows who is still missing; the game starts on every PC
echo when everyone has joined. Then: Multiplayer, Local, and each player presses their join button.
echo Each player uses their own keyboard or controller as player 1 would. Menus: keyboard
echo (arrows, Z, X); the mouse aims like a thumbstick and does not point in menus.
echo Every PC shows the split screen for now. Don't open F10 or the PC Options pages online.
echo.
call "%~dp0show-nightfire-native-4pads.cmd"
set "ONLINE_EXIT=%ERRORLEVEL%"
copy /y "logs\nightfire-startup.log" "%ONLINE_DIR%\game.log" >nul 2>nul
echo.
echo ==== Online summary ====
findstr /l /c:"SESSION ENDED" /c:"DESYNC" /c:"finished:" /c:"turned away" /c:"dropped" /c:"left" "%ONLINE_DIR%\net.log" 2>nul
findstr /l /c:"[NETPLAY] match" /c:"[NETPLAY] could not" /c:"resumed after a stall" "%ONLINE_DIR%\game.log" 2>nul
echo Logs and hashes: %ONLINE_DIR%
if /i not "%~3"=="nopause" pause
exit /b %ONLINE_EXIT%
