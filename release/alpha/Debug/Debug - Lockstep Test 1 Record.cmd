@echo off
setlocal
rem Nightfire PC alpha, debug: LOCKSTEP TEST, part 1 of 2 (groundwork for online play).
rem Copied from the workshop's nightfire-port-native\show-nightfire-native-lockstep-record.cmd
rem (10 October 2026) with the alpha's paths. Runs the Action engine alone, in its own window, with
rem the split-screen settings (show-nightfire-native-4pads.cmd). NIGHTFIRE_LOCKSTEP=1 makes the clocks
rem the game reads count frames (1/60 s each) for this run only; no other launcher sets it.
rem Record and replay must use the same engine: don't run Setup between them.
cd /d "%~dp0"
tasklist /fo csv /nh | findstr /i "nightfire" >nul
if not errorlevel 1 (
  echo A Nightfire game is already running. Close it first, then start this again.
  pause
  exit /b 1
)
set "NF_ACTION=%~dp0..\engine\action"
if not exist "%NF_ACTION%\nightfire_native.exe" (
  echo Setup has not been completed. Run Setup.cmd in the Nightfire-PC-Alpha folder first.
  pause
  exit /b 1
)
set "NF_LS=%~dp0lockstep"
if not exist "%NF_LS%" mkdir "%NF_LS%"
call :restore_pending || goto :fail
rem The replay has to start from the same saves and settings, so keep a copy of them as they are now.
call :save_to "%NF_LS%\saves-start" || goto :fail
del /q "%NF_LS%\presses.nfi" "%NF_LS%\record.nfh" "%NF_LS%\replay.nfh" "%NF_LS%\record-log.txt" "%NF_LS%\replay-log.txt" "%NF_LS%\RESULT.txt" 2>nul
set "NF_PADS=4"
set "NF_VIRTUAL_PADS=3"
set "NF_OVERLAY=1"
set "NF_ALPHA_ACTION_SOUND=off"
set "NIGHTFIRE_MENU_PREVIEW=1"
set "NIGHTFIRE_INPUT_TEST="
set "NIGHTFIRE_INPUT_TEST_DELAY_MS="
set "NIGHTFIRE_INPUT_TEST_READY_START="
set "NIGHTFIRE_INPUT_TEST_FILE="
set "NIGHTFIRE_INPUT_TEST_FILE_ONLY="
set "NIGHTFIRE_LOCKSTEP=1"
set "LEAN_NO_REGPOLL=1"
set "NF_INPUT_REPLAY="
set "NF_INPUT_RECORD=%NF_LS%\presses.nfi"
set "NF_STATE_HASH=%NF_LS%\record.nfh"
echo.
echo ==== LOCKSTEP TEST, part 1 of 2: RECORDING ====
echo Play as usual. In the menus use the keyboard (arrows, Z, X), not the mouse.
echo In this test the mouse aims like a thumbstick. Don't open F10 or change settings.
echo Play a minute or two of a split-screen match, then close the game window.
echo.
call "%NF_ACTION%\start-action.cmd"
findstr /l /c:"[LOCKSTEP]" "%NF_ACTION%\logs\nightfire-startup.log" > "%NF_LS%\record-log.txt" 2>nul
echo.
echo Recording saved in %NF_LS%
echo Next: start "Debug - Lockstep Test 2 Replay.cmd" and don't touch anything while it plays.
pause
exit /b 0

:fail
echo Could not copy the save folders, so nothing was started. See the robocopy message above.
pause
exit /b 1

:fail_back
echo Could not copy the save folders back. Your own saves are safe in %NF_LS%\saves-live;
echo starting either lockstep test again puts them back first.
pause
exit /b 1

rem ---- save folders: %LOCALAPPDATA%\xboxrecomp (the game's T:, U:, Z: drives) and NightfirePC (PC settings)
:save_to
if exist "%~1" rmdir /s /q "%~1"
for %%d in (TitleData UserData SystemData Cache) do call :mirror "%LOCALAPPDATA%\xboxrecomp\%%d" "%~1\%%d" || exit /b 1
call :mirror "%LOCALAPPDATA%\NightfirePC" "%~1\NightfirePC" || exit /b 1
exit /b 0

:load_from
for %%d in (TitleData UserData SystemData Cache) do call :mirror "%~1\%%d" "%LOCALAPPDATA%\xboxrecomp\%%d" || exit /b 1
call :mirror "%~1\NightfirePC" "%LOCALAPPDATA%\NightfirePC" || exit /b 1
exit /b 0

:mirror
if not exist "%~2" mkdir "%~2"
if not exist "%~1" exit /b 0
robocopy "%~1" "%~2" /MIR /COPY:DAT /DCOPY:T /R:1 /W:1 /NFL /NDL /NJH /NJS /NP >nul
if errorlevel 8 exit /b 1
exit /b 0

rem A replay that was stopped before it finished leaves the player's own saves in saves-live.
:restore_pending
if not exist "%NF_LS%\saves-live\RESTORE-PENDING" exit /b 0
echo The last replay did not finish; putting your own saves back first.
call :load_from "%NF_LS%\saves-live" || exit /b 1
del "%NF_LS%\saves-live\RESTORE-PENDING"
exit /b 0
