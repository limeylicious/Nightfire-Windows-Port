NIGHTFIRE PC - ALPHA TEST (10 October 2026)
===========================================

007 Nightfire (PAL Xbox) running natively on Windows, with no emulator. Both of the game's
engines are recompiled into Windows programs: the Action engine (menus, missions, multiplayer)
and the Driving engine (the car levels, starting with the opening Paris drive). The launcher
hands over between them the way the Xbox does, inside one window.

THIS IS A PERSONAL TEST BUILD. Setup copies the two engines the workshop last built. Those
programs are made from the game's own code, so once Setup has run, this folder must not be
shared or uploaded. The launchers and scripts themselves contain no game code. A public alpha,
where Setup builds everything from each player's own disc, comes later.


FIRST TIME
----------
1. Python 3.8 or newer must be installed (https://www.python.org/downloads/, and tick
   "Add python.exe to PATH" in the installer).
2. Double-click Setup.cmd. It copies the engines from the workshop, links the game files and
   checks that they are the PAL disc. On the workshop laptop "Game Files" can stay empty, because
   Setup links it to the workshop's own game files. Elsewhere, put the extracted disc in
   "Game Files" first (see the note inside it).
3. Run Setup.cmd again whenever the workshop builds new engines.


PLAYING
-------
  Play Nightfire.cmd               The whole game in one window, from the opening Paris drive.
  Play Nightfire (Fullscreen).cmd  The same, borderless fullscreen.
  Split-screen Multiplayer.cmd     Starts at the menus, set up for up to four players on this PC.
                                   This is the tested split-screen setup, which uses the older
                                   sound path.

Close the game window to stop. The black launcher window behind the game then asks for a key
press to close.


CONTROLS
--------
Missions (Action engine):
  WASD move, mouse look, left button fire, right button or T aim, E use, R reload, Ctrl crouch,
  Space jump, Q next weapon, G next gadget, F alternate fire, Tab objectives, Enter pause.
  Click in the window (or F1) to capture the mouse; Esc releases it. Xbox-style controllers work.
Car levels (Driving engine):
  Left mouse fires or accelerates, right mouse or T zooms or brakes, A / D steer, mouse aims,
  Z / E / R = A button, X / Q = B, Ctrl / C = X, Space = Y, Enter pauses.
Split-screen on one keyboard: F6 hands the keyboard and mouse to the next player, F5 back to
player 1. In Multiplayer setup press F6, then your join button, for each extra player.
Keys can be changed in Options > Audio/Video > PC Options > Key Binds, or in the F10 menu.


PC SETTINGS
-----------
  F10 (or the ` key)   PC settings menu in both engines: resolution, widescreen, display mode,
                       frame rate, key binds.
  Options > Audio/Video > PC Options > PC Graphics   the same settings inside the game's menus.
  Multiplayer > Local / Online                        the multiplayer pages.
Settings are kept in %LOCALAPPDATA%\NightfirePC and are shared with the workshop builds.
Frame rate settings change only what the screen shows; the game itself always runs at the
Xbox speed.


KNOWN ISSUES
------------
- Smooth Motion is EXPERIMENTAL. It draws in-between pictures. It is always on in the car levels
  and off by default in missions (PC Graphics page). In Paris the red roadster's front tyre can
  lean inwards for a moment, and lamp and headlight glows can sit one step ahead.
  "Debug\Debug - Smooth Motion Off.cmd" shows the car levels without it.
- Online multiplayer does NOT work yet. The Online pages are there and Host Game runs on this
  PC only; Find Games and Join by IP are not active yet.
- Split-screen with the new sound has not been tried yet (it is in the Debug folder).
- On-screen button prompts still show Xbox buttons, even when you use the keyboard.
- Controller rumble is off.
- The game runs slower on battery: plug the laptop in.
- Driving saves start fresh in this folder. Mission saves and PC settings are shared with the
  workshop builds.


IF SOMETHING GOES WRONG
-----------------------
Every crash or freeze is saved automatically (crash.txt, crash.dmp, freeze.txt):
  engine\action\sessions\<date-time>-action    missions and menus
  engine\driving\sessions\<date-time>          car levels
  engine\linked-sessions\<date-time>\steps.txt  which engine ran when
Send the newest of these folders with a note of what happened. The newest 10 are kept.
For a problem you can repeat, the Debug folder records much more (see Debug\README.txt).


NEEDS
-----
Windows 10 or 11 (64-bit), a DirectX 11 graphics card, Python 3.8 or newer, and the Microsoft
Visual C++ 2015-2022 x64 runtime (Setup warns if it is missing). Windows N editions also need
the Media Feature Pack for the movies.
