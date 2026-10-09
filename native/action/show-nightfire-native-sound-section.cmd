@echo off
rem Native sound test, starting in a later part of The Exchange (development shortcut: no carry-over
rem from earlier sections). Usage: show-nightfire-native-sound-section.cmd [2|3|4]  (default 2)
set "NIGHTFIRE_NATIVE_TEST_SECTION=%~1"
if not defined NIGHTFIRE_NATIVE_TEST_SECTION set "NIGHTFIRE_NATIVE_TEST_SECTION=2"
call "%~dp0show-nightfire-native-sound.cmd"
