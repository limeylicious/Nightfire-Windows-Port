@echo off
setlocal
cd /d "%~dp0"
rem ONLINE PLAY TEST (sound and multiplayer thread, netplay1 + lobby1 + saves1, 2026-10-11). Every PC runs the
rem same match from boot and only controller presses travel between them (runtime\native_action\nf_net.c).
rem   show-nightfire-native-online.cmd                the online lobby: a window that lists the matches
rem                                                   on your network, joins one or a typed address, or
rem                                                   hosts a match and starts it when everyone is in
rem   show-nightfire-native-online.cmd host [PCs]     host a match for PCs (2 to 4, this one included)
rem   show-nightfire-native-online.cmd join ADDRESS   join the match at ADDRESS (name or IP, optional :port)
rem Every PC needs the same build, the same game files and this launcher.
rem The host's PC must let UDP port 47610 in: Windows asks the first time (allow it); over the
rem internet the host's router has to forward that port. Two copies on one PC: use
rem show-nightfire-native-online-two-copies.cmd.
rem Online play needs NIGHTFIRE_LOCKSTEP's frame-counted clocks; no other launcher sets NF_NET.
set "ONLINE_ROLE=%~1"
set "ONLINE_ARG=%~2"
if "%ONLINE_ROLE%"=="" set "ONLINE_ROLE=lobby"
if /i "%ONLINE_ROLE%"=="lobby" goto :lobby
if /i "%ONLINE_ROLE%"=="host" goto :host
if /i "%ONLINE_ROLE%"=="join" goto :join
echo Use: %~nx0   or   %~nx0 host [PCs]   or   %~nx0 join ADDRESS
pause
exit /b 1

:lobby
set "NF_NET=lobby"
set "NF_NET_ADDR="
set "NF_NET_PLAYERS="
set "ONLINE_ARG="
goto :start

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
rem The game window's title says which side this is (the two-copies test names each copy; after
rem the lobby the game names this PC's players itself).
if /i "%NF_NET%"=="lobby" goto :labelled
if not defined NF_WINDOW_LABEL if /i "%NF_NET%"=="host" set "NF_WINDOW_LABEL=online host (player 1)"
if not defined NF_WINDOW_LABEL set "NF_WINDOW_LABEL=online (joined %NF_NET_ADDR%)"
:labelled
set "NIGHTFIRE_LOCKSTEP=1"
set "LEAN_NO_REGPOLL=1"
rem No thirty-minute safety stop in an online match.
set "RECOMP_WATCHDOG_SECS=0"
rem Smooth Motion on: its replay is what shows this PC's player on the whole screen in a
rem split-screen match (runtime\lean\lean_interp.inc). Display only: the PCs stay identical.
if not defined LEAN_INTERP set "LEAN_INTERP=1"
set "ONLINE_DIR=%~dp0online"
if not exist "%ONLINE_DIR%" mkdir "%ONLINE_DIR%"
rem Every PC starts from the same empty save folder, so the copies start identical; the game's
rem title data and profiles go in it too (saves\Partition1, runtime\kernel_path.c). Your own
rem profiles and saves (game_files\UDATA) are not used or touched; a profile made here lasts one match.
if exist "%ONLINE_DIR%\saves" rmdir /s /q "%ONLINE_DIR%\saves"
mkdir "%ONLINE_DIR%\saves"
set "NIGHTFIRE_SAVE_DIR=%ONLINE_DIR%\saves"
rem The one thing carried over is the game's language choice, an empty marker file in its title
rem data (game_files\TDATA\<title>\lang*.dat), so the game starts in the same language as it does
rem offline. Its name is one of the settings every PC must share (NF_GAME_LANGUAGE).
set "NF_GAME_LANGUAGE=none"
if exist "%~dp0game_files\TDATA\" call :seed_languages "%~dp0game_files\TDATA"
rem What happened, for checking afterwards: the network log, every agreed press (the match can be
rem replayed on one PC) and the game memory hashes (scripts\nf_state_diff.py compares two PCs').
del /q "%ONLINE_DIR%\net.log" "%ONLINE_DIR%\presses.nfi" "%ONLINE_DIR%\state.nfh" "%ONLINE_DIR%\game.log" 2>nul
set "NF_NET_LOG=%ONLINE_DIR%\net.log"
set "NF_INPUT_RECORD=%ONLINE_DIR%\presses.nfi"
set "NF_INPUT_REPLAY="
set "NF_STATE_HASH=%ONLINE_DIR%\state.nfh"
echo.
echo ==== ONLINE MATCH (test): %NF_NET% %ONLINE_ARG% ====
if /i "%NF_NET%"=="lobby" goto :lobby_text
echo A small "Nightfire Online" window shows who is still missing; the game starts on every PC
echo when everyone has joined.
goto :more_text
:lobby_text
echo The "Nightfire Online" window lists the matches on your network. Pick one and press Join,
echo or type the host's address and press Join address, or press Host a match and then Start
echo match when everyone is in. The game starts on every PC at the same moment.
:more_text
echo Then: Multiplayer, Local, and each player presses their join button.
echo Each player uses their own keyboard or controller as player 1 would. Menus: keyboard
echo (arrows, Z, X); the mouse aims like a thumbstick and does not point in menus.
echo In a split-screen match each PC shows only its own player, on the whole screen (Smooth
echo Motion is on for online play); F7 switches to the whole split screen and back.
echo Don't open F10 or the PC Options pages online.
echo The game starts without your own profiles: if it asks for a profile, make one (it lasts
echo this match only).
echo.
call "%~dp0show-nightfire-native-4pads.cmd"
set "ONLINE_EXIT=%ERRORLEVEL%"
copy /y "logs\nightfire-startup.log" "%ONLINE_DIR%\game.log" >nul 2>nul
echo.
echo ==== Online summary ====
findstr /l /c:"SESSION ENDED" /c:"DESYNC" /c:"finished:" /c:"turned away" /c:"dropped" /c:"left" /c:"lobby: hosting" /c:"lobby: joining" /c:"lobby: back to" /c:"lobby: starting" "%ONLINE_DIR%\net.log" 2>nul
findstr /l /c:"[NETPLAY] match" /c:"[NETPLAY] could not" /c:"[NETPLAY] the online lobby" /c:"resumed after a stall" "%ONLINE_DIR%\game.log" 2>nul
echo Logs and hashes: %ONLINE_DIR%
if /i not "%~3"=="nopause" pause
exit /b %ONLINE_EXIT%

:seed_languages
pushd "%~1"
for /d %%t in (*) do call :seed_title "%%t"
popd
exit /b 0

:seed_title
pushd "%~1"
for %%l in (lang*.dat) do call :seed_language "%~1" "%%~fl"
popd
exit /b 0

:seed_language
if not exist "%NIGHTFIRE_SAVE_DIR%\Partition1\TDATA\%~1" mkdir "%NIGHTFIRE_SAVE_DIR%\Partition1\TDATA\%~1"
copy /y /b "%~2" "%NIGHTFIRE_SAVE_DIR%\Partition1\TDATA\%~1\" >nul
if "%NF_GAME_LANGUAGE%"=="none" (set "NF_GAME_LANGUAGE=%~1-%~n2") else set "NF_GAME_LANGUAGE=%NF_GAME_LANGUAGE%+%~1-%~n2"
exit /b 0
