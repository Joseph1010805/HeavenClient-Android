# Known issues and things still to do

Everything reported from playing, whether fixed or not. Kept so nothing
raised at the table gets quietly forgotten.

## Open

### Client - visual

- **Characters sit too high on the character select screen.** They are drawn at
  the same point as the signpost they stand on, so where their feet land
  depends on the post's origin. Measured: the post is 71x158 with its origin at
  52,156, so drawn at 135,234 it spans y 78 to 236. Needs the character offset
  correcting against that.
- **Stats do not sit inside their boxes** on character select. Same cause as the
  Start button, which is fixed - positions were tuned for a later UI version.
- **The character select background is mostly white.** Looks like a layer that
  is not being drawn. Not yet investigated.
- **The skill window is too narrow for its contents.** The client lays skills out
  in two columns needing 297px; the artwork here is 174x299. Needs a
  single-column layout, using the scrollbar that is already there.
- **SKILL POINT shows no number.** Probably the same thing - the label drawn at a
  coordinate outside this version's frame.

### Client - missing features

- **Boss HP bar never appears.** The packet is parsed correctly in
  `TestingHandlers.cpp` and every field is then discarded. Needs a UI element;
  the data arrives fine.
- **Summoned monsters are unverified.** The parsing is reasoned and the byte
  alignment cannot break the rest of the packet, but no linked spawn has been
  observed. Test by killing a reviving monster - `!spawn 4300000` (Blue
  Perfume) - and watching for the `spawn: summoned` log line.
- **No controller mapping.** Gamepad buttons map to keyboard keys through
  `Setting<Joystick_*>` in the settings file, but there is no way to change them
  in game. Wanted before the second-screen work.

### Server

- **Monsters stop moving after a session ends badly.** A monster only looks for a
  new controller if its current one is gone or dead, so a stale character object
  left by a crashed session keeps control forever and never moves anything.
  Clears on a server restart. Worth a proper idle-client timeout.
- **A Blue Perfume was spawned in a town** during testing and should be removed.
  Clears on server restart.

### Environment

- **MySQL and the server run as loose processes.** Both die if their parent goes
  away - which has already cost one character's progress - and neither survives
  a reboot. MySQL should be a Windows service, the server a startup script.
- **The PC needs a static LAN IP.** `ServerIP` on the device and Cosmic's
  `HOST`/`LANHOST` are all pinned to 192.168.1.71; a DHCP change breaks both.

## Planned

- **Second screen (Thor branch).** Menus - map, skills, inventory - on the
  handheld's lower display instead of over the game. Confirmed feasible:
  display 4 is a real touch-capable screen carrying FLAG_PRESENTATION. Needs a
  Presentation and a second EGL surface sharing the GL context, since SDL2
  supports only one window on Android.
- **Windows build.** The desktop code is still in the tree. Would make UI work
  far quicker to check, since it could be run and looked at directly rather than
  pushed to a device each time.

## Fixed

- White screen on startup - GLES2 will not convert texture formats on upload
- All text unreadable - `mediump` cannot address an 8192x8192 atlas
- Sprites drawn as blank rectangles - uploads did not bind the atlas
- No way to type - no on-screen keyboard, and no `SDL_TEXTINPUT` handling
- Every letter typed twice - both the text and keycode paths inserted it
- Cursor did not match where the screen was touched - stale window size
- "Password is invalid" whatever was typed - the client rejects under 5
  characters locally and never sends anything
- Start button did nothing - the server handed out `127.0.0.1` as the channel
  address, so the phone connected to itself
- Monsters never appeared - the spawn packet's status block is 16 bytes, not 22
- Monsters spawning with an effect were misread - the new-spawn marker follows
  the effect block, and bosses are what spawn with effects
- Dying did nothing at all - the state existed but nothing ever set it
- Revive did nothing - the request was sent with a map id of -1, which the
  server discards
- No tombstone or death sound - both were in the data, and the sound was already
  being loaded and never played
- Everything sent stopped reaching the server - `would_block` on a non-blocking
  socket was treated as fatal, silently dropping every packet afterwards
- Movement, knockback and falling all too fast - now taken from Nexon's own
  numbers in `Map.wz/Physics.img` rather than guessed
- Start button covered the character stats
- No way to quit - the Android back button now opens the game's quit dialog, so
  quitting logs out properly and saves
- Experience and drops far too generous - server rates were 10x
- Ability points auto-assigned instead of being given to spend
- Characters saved once an hour, so a crash cost an hour
