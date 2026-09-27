# Changelog

## v0.9.2

**One double-click to install.** `INSTALL.bat` sits at the root of the
repository. It is a shim and not a second installer: it finds the bash that Git
for Windows ships, hands it `tools/install.sh`, and **pauses at the end** so a
failure can be read instead of vanishing with the console. Turn USB debugging
on, plug the device in, double-click.

It asks nothing of its own. Every device gets a server, because any device can
be the one that creates the game, so the only question in the whole install is
the installer's own **`Install to AYN Thor?`**. A PC with no built Cosmic on it
is no longer an error either - the game still installs and will join somebody
else's server.

**The installer asks, checks, and says what happened.** Every step names its own
answer on the same line, and it confirms the device by name before it installs
anything:

```
  Checking adb ................. ok
  Looking for a device ......... found

Device found: AYN Thor  (Android 13, arm64-v8a)
  Install to AYN Thor? [Y/n]
```

`deploy_data.sh` and `stage_server.sh` take `INSTALL_QUIET=1`: run by hand they
are still full reports, run from the installer they are steps. **What goes quiet
is only ever the good news** - every failure prints in full in both modes, and
the two hundred lines of Termux setup output are kept in a file and printed the
moment it fails. Gone: the page of by-hand instructions that used to print
*after* the installer had already done all of it, under a warning to ignore it.

**The game is called LocalStory.** "Bugs 'n Beans Story" is retired.

### Fixed

- **The launcher was reverting its own fix on every press of HOST.** `run.sh` is
  generated on the device at the end of `termux_setup.sh`, and `bootstrap.sh`
  copied a *staged* copy over the live one whenever the two DIFFERED - but
  different is not newer. An orphan `run.sh` from **28 August** was sitting in
  the staging directory (an older `stage_server.sh` left it there; nothing has
  staged one since), so the correct launcher was written and then overwritten
  every single time.

  This is why the v0.9.1 fix for the hanging login never took effect. A live
  Cosmic with a dead database was still counted as a running server, because the
  code that stopped doing that lived in a file replaced seconds after it was
  written. Found by grepping the live `run.sh` on a Thor for the comment the fix
  adds: absent, and byte-identical to the August file - 4,987 bytes against the
  8,766 the current generator writes.

  `stage_server.sh` deletes the orphan now, and `bootstrap.sh` requires a staged
  copy to be **newer**. A device set up before this needs one
  `INSTALL.bat --server` run to regenerate its launcher.
- **`adb shell` was eating the operator's answers.** It reads stdin, and three
  `getprop` calls run between "Looking for a device" and "Install to X?". They
  swallowed the answer to a question that had not been asked yet, so the
  confirmation was answered by EOF, EOF was read as yes, and the install went
  ahead on a device nobody had confirmed. Found by answering `n` and watching it
  install anyway. Every adb call in the installer closes stdin now, and the same
  bug was eating the device LIST in the multi-device picker, whose loop reads
  from that same stdin.
- **A missing source file blamed the device.** `stat` failed, the expected size
  came out empty, and the report read `FAILED (device has 92112664 bytes,
  expected )` about a file that had never existed on the PC. Both transfer
  scripts now name the missing file and where they looked.
- **The launcher icon said neither name.** The application label was
  `HeavenClient` and the activity label - the one Android actually shows - said
  `MapleStory`, while `app_name` said `Game`. All of them read
  `@string/app_name` now.
- **Only Explorer and Cygnus Knights can be created.** Aran was the one other
  branch still enabled and is greyed like the remaining twenty-three;
  `UIAranCreation` is left wired up, so re-enabling it is one line.
- **An uninstall takes the game data with it**, and the installer never said so.
  The advice printed after a signature clash now warns that removing the old
  copy costs a ten-minute re-copy of 4 GB.

### Changed

- Bug reports land in `Download/LocalStory` rather than `Download/BugsNBeans`.
- Three `printf` formats held a literal escape character and a literal carriage
  return where the two-character escape was meant. They behaved identically and
  were unreadable.
- The installer's closing text no longer contradicts itself. It used to tell
  somebody who had just sat through the twenty-minute server install to go and
  install a server.


## v0.8

**Talking to each other.** Speech to text, entirely on the device — press the
balloon, a bubble appears over your character, it fills in as you speak, and a
pause of a second and a half sends it. Recognition is Vosk with a local model:
no speech service, no account, nothing recorded ever leaves the machine, and it
works with the router unplugged. The game mutes its own music and sound effects
while the microphone is open, and asks the platform for noise suppression and
echo cancellation.

**A megaphone button.** Shouting to the channel or the world is now a button
rather than an item you have to buy. It sends the real megaphone packet and the
server runs the code it always did, so the artwork, the name prefix and the
level rules are the same ones the item used.

### Fixed

- **The screen was never fullscreen.** The game rendered a full 1920x1080 frame
  into a slot 55 pixels shorter, pushed down by the status bar's reserved strip
  — so the top was a black band and the HP bar fell off the bottom. This is the
  "cut off screen" that had survived a rotation fix, a stale-drawable fix, a
  letterbox and three resolution changes; none of them was the cause.
- **196 hats could not be drawn**, 60 of them on sale. Two separate faults: the
  cap type was matched against three literal strings and everything else became
  "draw nothing" (144 hats), and the layer a hat asked for was only drawn in one
  branch out of four, so 27 more hid your hair and drew nothing in its place.
- **The map kept the last map's monsters.** Changing character out of the Cygnus
  tutorial put four Tutorial Tinos on Maple Road. All eight map containers
  emptied their live objects on a map change but not their pending spawn queues.
- **Every server message was read and thrown away** — megaphones, notices,
  announcements. Only the scrolling banner was ever shown.
- **Quest EXP was never reported**, and misparsed: the client read a fixed
  layout where the server writes an extra byte, then printed "not handled" in
  red. Finishing a quest levelled you up with nothing to say why.
- **Every "Weekdays" 2x EXP card was inert** at every hour of every day — nine
  rows in the coupon table had lost the leading digit of their item id. Coupons
  are no longer restricted to a weekday and a four-hour window.
- **NPC menus ran off the screen.** A long list of choices was drawn past the
  dialogue box, over the minimap and off the bottom edge; the box was sized from
  the message and never counted the choices. Now sized, clipped and scrollable.
- **Nothing scrolled.** `Slider::send_scroll` had always existed and no window
  ever called it. The inventory, skill book, quest log, NPC dialogue and cash
  shop now take the wheel — and a thumbstick.
- **The cash shop character could not move.** Walking, jumping and the stances
  were all implemented and had never been handed a key. He also faced backwards
  and stood shin-deep in the floor.
- **A megaphone needed level 10** — Nexon's anti-spam rule for a game where they
  cost real money, which only stopped a new character talking.

### Changed

- The client is no longer pinned to four screen resolutions. The status bar's
  position was written four different ways that all meant "120 from the bottom",
  and the login and character screens each hardcoded 800x600 twice over.
- Status bar: Event and Community retired, the HP/MP panel moved left, and the
  remaining buttons drawn larger for a thumb rather than a mouse.
- Cash shop: **MY CASH ITEMS** replaced with an **INFORMATION** panel showing
  what the selected item actually does, and buying now asks first, naming the
  item and the price.
- The thumbstick scrolls; walking is on the d-pad.

### Known

- The minimap cannot be sharpened by enlarging it. There is one set of frame
  artwork and one bitmap per map in the game data — no higher-resolution source
  exists to draw from.
- Quest 3 controllers still reach a 2D app as six signals, not as a gamepad.
  See `docs_QUEST.md`; the investigation is finished and should not be repeated.
