NIGHTFIRE PC - ALPHA TEST (10 October 2026)
===========================================

007 Nightfire (PAL Xbox) running natively on Windows, with no emulator. Both of the game's
engines are recompiled into Windows programs: the Action engine (menus, missions, multiplayer)
and the Driving engine (the car levels, starting with the opening Paris drive). The launcher
hands over between them the way the Xbox does, inside one window.

THIS IS AN ALPHA TEST. Setup makes the two engines on your PC from your own disc (in the
owner's workshop it copies the engines the workshop last built). Those programs are made from
the game's own code, so once Setup has run, this folder must not be shared or uploaded. The
launchers and scripts themselves contain no game code.


FIRST TIME
----------
1. Install, if you do not have them yet:
   - Python 3.8 or newer (https://www.python.org/downloads/; tick "Add python.exe to PATH").
   - Git for Windows (https://git-scm.com/download/win).
   - Build Tools for Visual Studio 2022 (https://visualstudio.microsoft.com/downloads/, under
     "Tools for Visual Studio") with the "Desktop development with C++" workload. A full
     Visual Studio 2022 with that workload works too.
2. Put the contents of your extracted PAL disc in "Game Files" (see the note inside it).
3. Double-click Setup.cmd. It checks that the game files are the PAL disc, translates the
   game's code on your PC, applies the port's fixes and compiles both engines into the
   "workshop" folder at the top of this copy, then sets up this folder. The first run
   downloads the translator (xboxrecomp) and Capstone into that folder and takes a long time,
   mostly compiling. Nothing is installed anywhere else.
4. Run Setup.cmd again after updating this copy: it rebuilds only what changed.
On the owner's workshop laptop Setup only copies the engines, and "Game Files" can stay empty
because Setup links it to the workshop's own game files.


PLAYING
-------
  Play Nightfire.cmd               The whole game in one window, from the opening Paris drive.
  Play Nightfire (Fullscreen).cmd  The same, borderless fullscreen.
  Play Nightfire (All Missions).cmd
                                   The whole game with every mission open in the mission menu.
                                   Without it, missions open in the original order as you finish
                                   them (Paris Prelude, The Exchange, Alpine Escape, ...).
  Split-screen Multiplayer.cmd     Starts at the menus, set up for up to four players on this PC.

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
- Since 11 October the missions always use the new sound (the emulated sound chip is gone).
  Split-screen has not been played with it yet.
- Only Paris Prelude has been played in the Driving engine so far. The other car levels
  (Alpine Escape, Enemies Vanquished, Deep Descent, Island Infiltration) are untested.
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
the Media Feature Pack for the movies. Building the engines also needs Git, the Visual Studio
C++ build tools and about 2 GB of free space.
