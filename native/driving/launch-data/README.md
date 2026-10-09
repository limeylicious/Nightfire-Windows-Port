# Original first-Paris launch data

`original-first-paris144.bin` is the complete 2,640-byte output of the original
PAL action game's serializer, captured during a private run of saved action141.
It selects the opening Paris mission with original fresh-game settings. No
profile fields, language bytes or mission values were synthesized or patched.
The adjacent JSON records image and payload hashes and the capture boundary.

This is development startup data, not a saved driving game or completed process
handoff. The debugger stopped its child at the original DFB50 handoff entry,
before action shutdown. A future launcher must capture each actual transition's
state instead of always reusing these first-Paris bytes.

The driving launcher wraps the payload in a zeroed 4,096-byte type-0 launch page
with title ID 45410026. Its original XGetLaunchInfo copies all 3,072 payload bytes
and consumes the page. The remaining bytes are zero. This CPU-only page uses the
runtime's tracked guest heap to match the existing free bridge; physical Xbox
contiguity is not claimed.

`DRIVING_LAUNCH_PAYLOAD144` can select a different complete original packet.
`DRIVING_NO_LAUNCH144=1` selects the earlier null-page diagnostic. Neither is a
command-line map string. The current driving executable is still a bootstrap
diagnostic with incomplete graphics and no verified driving scene.
