# Known issues and things still to do

Everything reported from playing, whether fixed or not. Kept so nothing
raised at the table gets quietly forgotten.

## Open

### Client - visual

- **Stats do not sit inside their boxes** on character select. Same cause as the
  Start button, which is fixed - positions were tuned for a later UI version.

### Client - the settings menu

- **The quit entry is labelled "Change Character".** Not a positioning problem,
  which is what two attempts assumed. Only four button names exist in the v178
  artwork - `GameQuit`, `channel`, `option`, `keySetting` - and the one called
  `GameQuit` holds artwork reading "Change Character"; the name was reused by
  the version this client was written for. Clicking it does open the quit
  dialog. Wants a label drawn over it, or a text entry of our own.
- **The menu floats above the quick slots.** Should sit flush against the top of
  the quick menu buttons rather than leaving a gap.

### Client - gameplay

- **Standing still does not restore health or mana.** Should be roughly 10 HP
  every 10 seconds, and more while sitting on a chair. Cosmic has no passive
  regeneration task that I can find - `USE_ULTRA_RECOVERY` turns out to concern
  the Recovery *skill*, not idle regen - so this may need writing rather than
  enabling.
- **Levelling up has no sound.** `Sound::Name::LEVELUP` is loaded and the level
  up effect plays, so this is likely the same shape as the tombstone was - the
  sound is there and nothing plays it.
- **Monsters take about a second to start moving** after appearing. A controlled
  monster waits for its animation to end and a counter to pass 200 updates,
  which at 125 a second is 1.6 seconds before its first move.

### Client - combat, reported 23 August

Both mild, both seen once, both worth a name so the next sighting is a second
data point rather than a first one.

- **A drop was slower to pick up than it should have been.** Once. Could be
  the pickup range, the settle time before a drop becomes lootable, or the
  loot packet being answered late.
- **An attack hit a different monster than the one aimed at.** The client
  chooses its own targets and tells the server which it hit, so this is ours,
  not the server's. Look at how the attack picks its targets - nearest by
  centre, by bounding box, or first in the map's object order - and whether
  it prefers the one being faced.

Ruled out already, from the packet instrument: neither is a parser stopping
short. The only handler that discards a whole combat packet is
MOVE_MOB_RESPONSE, which is a NullHandler on purpose - so **monster skills
never happen at all**, since the controlling client is the one told to use
them. Longstanding, and worth fixing on its own account.

### Client - controller mapping on the quickslot

The slide-out quickslot at the bottom right has twelve empty slots, in two rows
of six, and is the natural place to bind a gamepad. Skills and items cannot
currently be dragged into it.

Wanted layout, left to right:

| | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|---|
| **top** | Y | X | L2 | R2 | Start | Select |
| **bottom** | B | A | L1 | R1 | L3 | R3 |

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

### Client - the lower panel (Thor only)

- **The pointer jumps about on the world map.** Not reproducible on demand.
  Ruled out by measurement: page geometry, the coordinate conversion, the touch
  queue and thread hand-off, a second contact, two input devices. A speed
  filter in `SecondScreen.java` rejects impossible samples and it still
  happens, so the cause is something not yet looked at. Say **RECORD!** and
  frames of the panel are captured while it is reproduced.
- **UNEQUIP is unverified.** Built, never seen working.
- **The stat sheet's DETAIL column is unverified** at its new position, which
  is the whole reason the stat list was redrawn by hand.
- **Chat, hotkeys and quests are empty pages.** Chat wants the emotion buttons
  along the top, the keyboard pinned underneath, and chat opening by itself.
- **Ability and Skills use the stock layout.** Only their opacity was changed.

### Client - sound

- **The level-up sound plays twice.** One call site in the client, reached from
  one handler, and it only fires when the new level is higher - so a repeated
  packet cannot retrigger it. Unexplained.

## Planned

- **Bringing your characters with you, from inside the game.** DECIDED: a
  **helper port on the host**. A small listener in Termux beside Cosmic; the
  joining device exports its account as a blob, sends it, the host imports it
  before login, and it comes back on the way out.

  The alternative - custom packets inside Cosmic - is cleaner in principle (no
  new port or permission, rides the connection you already have) but has to
  happen *around login*, because the joining player has no account on the host
  yet. That means surgery on the login handler, the one place where a bug locks
  everybody out, plus new opcodes kept in sync on both sides. This project's
  entire history is silent protocol failures; the helper port puts the risky
  part somewhere errors are visible and can be tested from a PC with `curl`,
  with no game running and no second person.

  `tools/character.py` already knows what a character is, and that knowledge is
  the hard part - it transfers to either transport.

- **📌 PINNED: party quests have hard minimum headcounts, and three players
  isn't enough for many of them.** Every PQ script in `scripts/event/` opens
  with a `minPlayers`. With three:

  | Playable at 3 | Locked out |
  |---|---|
  | Kerning (3), Henesys (3), Cafe (3), Ludi Maze (3), Pirate (3), Holiday (3) | Ellin (4), Magatia (4), Treasure (4), **Ludibrium (5)**, Orbis (5), Amoria (6), Crimsonwood (6), Horntail (6) |
  | plus Boss Rush, Elnath and Zakum, which allow 1 | |

  Kerning PQ - the first one anybody wants - works at exactly three.
  Ludibrium PQ needing five is the real loss.

  `minPlayers` is one variable at the top of each script and is used only for
  entry gating, so lowering it is a one-line change per PQ. Not yet done, and
  not obviously safe: some stages are physically built around N bodies
  (platforms, simultaneous switches). The gating was checked; the stages were
  not.

- **Guild and family ids point at rows that don't travel.** A character carries
  `guildid`, `familyId`, `partnerId` and `allianceRank`, but `guilds` is a
  world-level table and isn't carried. A guild member arriving in a world
  without that guild would be pointing at nothing. Latent right now - there are
  zero guilds - but it is the same class of bug as the quest-progress one, and
  it will bite the day somebody makes a guild.

- **Windows build.** The desktop code is still in the tree. Would make UI work
  far quicker to check, since it could be run and looked at directly rather than
  pushed to a device each time.

## Fixed

- **The skill window was too narrow for its contents, and SKILL POINT showed no
  number.** Fixed 22 August in `8f274c9` - the client laid skills out in two
  columns needing 297px against artwork 174x299 wide, which put half the
  spend-point arrows at x278 where they could not be clicked, and the SP label
  at x304 where it was off the window entirely. Now one column with the
  scrollbar that was already there. *(These sat under Open for two days after
  being fixed, and the stale entries were convincing enough to argue from.)*
- **The skill window could take the whole game down instead of closing.** Three
  places read the spare SP by parsing it back out of the label text with
  `std::stoi`, and that label is empty until `change_sp()` has run - `std::stoi("")`
  throws. The parse sat at the top of `button_pressed()` before the switch, so
  every button in the window went through it, Close included. It reads the
  `sp` / `beginner_sp` members directly now, and both are initialised.

- **The AYN Thor's second screen.** The menus live on the handheld's lower
  display instead of on top of the game: a deck of eight swipeable pages -
  world map, inventory, equipment, ability, skills, quests, hotkeys, chat -
  each staying where it was left. Display 4 is a real touch-capable screen
  carrying FLAG_PRESENTATION; it works through a Presentation and a second EGL
  surface sharing the GL context, since SDL2 allows only one window on Android.
  `android/.../SecondScreen.java` and `IO/SecondScreenPanel.cpp`.
  *(Its remaining rough edges are under Open, in "the lower panel".)*
- **A dead host left the client stuck on a Start button that did nothing.**
  Killing an app doesn't reliably send a FIN, so a client that isn't writing
  never finds out. 45 seconds of silence is now treated as a disconnect and
  returns to the login screen rather than quitting the game - losing the host is
  an ordinary event when the host is a handheld somebody can close.
- **Every press of HOST started another server.** Seven Cosmic instances were
  found on the Thor after an evening of testing; only the first can hold the
  login port, and the rest sat there having each asked for up to 1536 MB. The
  guard existed only in the repo and had never reached either device. It now
  tests whether **port 8484 is taken** rather than whether a process exists -
  and the `pgrep` fallback is written `[C]osmic`, because `pgrep -f Cosmic.jar`
  cheerfully matches its own parent and would have made the guard refuse
  forever.
- **An outage cost up to two minutes of play** (an hour on older Cosmic).
  Autosave is now every 60 seconds and says so at INFO.
- **Characters couldn't leave the device they were made on.**
  `tools/character.py` - see the README. Not yet reachable from inside the game.

- The login flow had no artwork of its own - the login, character select and
  character creation screens all read numbered frames out of `Map001.wz`, which
  no version this client can obtain actually contains (v83's `back/13` is a
  mushroom house, v202's is a single pixel), and world select drew two stacked
  city backdrops from far later content. All four now come from a custom
  `Map001.nx` built by `tools/make_assets.py`, and the login and character
  screens are video rather than stills. See CHANGES.md.
- Characters sat too high on character select - they are drawn at the same point
  as the signpost they stand on, so the post's origin (52,156) decided where
  their feet landed. The row now stands just above the Create/Delete buttons,
  with the name above the head rather than at the feet, where it collided with
  the page arrows.
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
