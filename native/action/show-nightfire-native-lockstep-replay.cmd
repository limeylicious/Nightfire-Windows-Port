@echo off
setlocal
cd /d "%~dp0"
rem LOCKSTEP TEST, part 2 of 2 (groundwork for online play; sound and multiplayer thread, 2026-10-10).
rem Plays back the presses show-nightfire-native-lockstep-record.cmd recorded, starting from the same
rem saves, then compares the game memory of both runs frame by frame (scripts\nf_state_diff.py).
rem Your own saves are kept aside during the replay and put back afterwards.
rem NIGHTFIRE_LOCKSTEP=1 is set for this run only; no other launcher sets it.
tasklist /fo csv /nh | findstr /i "nightfire" >nul
if not errorlevel 1 (
  echo A Nightfire game is already running. Close it first, then start this again.
  pause
  exit /b 1
)
set "NF_LS=%~dp0lockstep"
if not exist "%NF_LS%\presses.nfi" (
  echo There is no recording yet. Run show-nightfire-native-lockstep-record.cmd first.
  pause
  exit /b 1
)
call :restore_pending || goto :fail
call :save_to "%NF_LS%\saves-live" || goto :fail
echo pending> "%NF_LS%\saves-live\RESTORE-PENDING"
call :load_from "%NF_LS%\saves-start" || goto :fail_back
del /q "%NF_LS%\replay.nfh" "%NF_LS%\replay-log.txt" "%NF_LS%\RESULT.txt" 2>nul
set "NIGHTFIRE_LOCKSTEP=1"
set "LEAN_NO_REGPOLL=1"
set "NF_INPUT_RECORD="
set "NF_INPUT_REPLAY=%NF_LS%\presses.nfi"
set "NF_STATE_HASH=%NF_LS%\replay.nfh"
echo.
echo ==== LOCKSTEP TEST, part 2 of 2: REPLAY ====
echo The game now repeats your recorded presses by itself. Don't touch the keyboard or mouse.
echo When it stops repeating what you did, close the game window. The result appears here.
echo.
call "%~dp0show-nightfire-native-4pads.cmd"
findstr /l /c:"[LOCKSTEP]" "logs\nightfire-startup.log" > "%NF_LS%\replay-log.txt" 2>nul
call :load_from "%NF_LS%\saves-live" || goto :fail_back
del "%NF_LS%\saves-live\RESTORE-PENDING"
python "%~dp0scripts\nf_state_diff.py" "%NF_LS%\record.nfh" "%NF_LS%\replay.nfh" > "%NF_LS%\RESULT.txt" 2>&1
findstr /l /c:"REPLAY DIVERGED" /c:"replay finished" /c:"stall" "%NF_LS%\replay-log.txt" >> "%NF_LS%\RESULT.txt" 2>nul
echo.
type "%NF_LS%\RESULT.txt"
echo.
echo Your saves are back as they were. Result saved in %NF_LS%\RESULT.txt
pause
exit /b 0

:fail
echo Could not copy the save folders, so nothing was started. See the robocopy message above.
pause
exit /b 1

:fail_back
echo Could not copy the save folders back. Your own saves are safe in %NF_LS%\saves-live;
echo starting either lockstep launcher again puts them back first.
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
