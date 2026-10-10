NIGHTFIRE PC ALPHA - DEBUG LAUNCHERS
====================================

These start the same game as the main launchers, in debug mode: everything the workshop records
is kept, and the newest 30 sessions are kept instead of 10. Expect larger folders and, with
the sound recording and picture dumps, a slightly slower game.

What debug mode adds (on top of the crash and freeze reports every mode keeps):
  Car levels: audio.wav and the sound logs in engine\driving\sessions\<date-time>, the frame
              counter in the title bar, pictures in engine\driving\runs.
  Missions:   picture dumps in engine\action\captures; logs in engine\action\logs.

  Debug - Full Logging.cmd               The whole game with all of the above.
  Debug - Record Pictures (F9).cmd       The car levels keep the last 72 pictures. Press F9 right
                                         after a glitch: a chime plays, the title shows
                                         "MARK n SAVED", and the pictures go to the newest folder
                                         in engine\driving\runs.
  Debug - Smooth Motion Off.cmd          Car levels without the in-between pictures, to compare
                                         against Smooth Motion.
  Debug - Two Windows.cmd                Each engine opens its own window (if the single window
                                         misbehaves).
  Debug - Skip Opening Drive.cmd         Starts at the mission menus; the opening Paris drive is
                                         skipped.
  Debug - Split-screen With New Sound.cmd
                                         Split-screen with the missions' new sound. Not tried in
                                         split-screen yet.
  Debug - Split-screen, Keyboard As Own Player.cmd
                                         Split-screen where the keyboard and mouse are player 1 on
                                         their own and controllers 1-3 are players 2-4.
  Debug - Lockstep Test 1 Record.cmd     Groundwork for online play. Records every press of a short
  Debug - Lockstep Test 2 Replay.cmd     split-screen match, then replays it and checks that the
                                         game stays identical frame by frame. Use the keyboard in
                                         the menus, don't open F10, and don't run Setup between the
                                         two. The replay sets your saves aside and puts them back
                                         afterwards. Result: Debug\lockstep\RESULT.txt.
