# A running list of fixes

Newest first. One line each, with the *cause* rather than the symptom —
symptoms repeat, causes do not.

Kept because the same shapes come round again: state left behind when a screen
changes, a value read but never used, a doc that stopped being true, and an
error hidden behind `>/dev/null`.

## 28 August 2026

## 28 August 2026 — the server ate its own config

- **`run.sh` carried a literal 0x01 byte where a sed backreference belonged, and
  deleted `HOST:` and `LANHOST:` out of config.yaml.** A heredoc in
  `tools/termux_setup.sh` ate the backslash when the script was generated, so
  `\1` was written as the control character. That made two things go wrong at
  once:

      MY_IP=$(... sed -n 's/.* src \([0-9.]*\).*/<0x01>/p' ...)

  set `MY_IP` to a control character - **not empty**, so the "no address found"
  guard passed straight through - and then

      sed -i "s/^\( *HOST:\)[^#]*/<0x01> $MY_IP /" config.yaml

  replaced the whole `HOST: 192.168.1.71   #WAN IPv4 address` line with
  `<0x01> <0x01> #WAN IPv4 address`, destroying the key and the value together.

  Cosmic then refused to start with **"Could not successfully parse charset from
  config file config.yaml"** - which names the charset and points nowhere near
  the damage. The log line that did tell the truth was `address is  - updating
  config.yaml`, where the address prints as blank because it is a control byte.

  **Same class of bug as the two C++ heredoc manglings already in this file, and
  it bit again WHILE FIXING IT** - a heredoc silently ate the backslash out of
  the repair script too. Do not put backslashes through a heredoc in this
  project. Write the script to a file and run the file.

  Fixed in the generator as well as on the device, so a re-run cannot rewrite a
  broken `run.sh`.

## New features, 28 August 2026

- **The megaphone is a BUTTON now, not an item.** It sends the ordinary
  `USE_CASH_ITEM` packet naming a real megaphone (5070000 channel / 5071000
  world), and the server runs the megaphone code it always had — the only
  change there is that `USE_FREE_MEGAPHONES` skips the two steps that are about
  *owning* one: the inventory check and taking it away. So the artwork, the
  `<medal> Name : ` prefix, the level-10 rule and the channel-versus-world scope
  come from the one implementation that has ever existed and cannot drift from
  it. **Deliberately not a new opcode** — Cosmic decides some packets by their
  LENGTH and a wrong guess fails silently.
  `IO/UITypes/UIMegaphone.*`, `Net/Packets/InventoryPackets.h`,
  `UseCashItemHandler.java`
- **Speech to text, entirely on the device.** Vosk with a small English model,
  no network, no account — a mic button on the chat bar (talks to the map) and
  another in the megaphone window (talks to the channel or the world).
  Android's own `SpeechRecognizer` is not an option here: the Thor answers
  `cmd package query-services android.speech.RecognitionService` with **"No
  services found"**, so there is no engine installed to call. The model is
  **not in the apk** — ~68MB, not ours to redistribute — it goes out through
  `tools/deploy_data.sh`, and when absent the buttons are simply not drawn.
  `Speech.*`, `SpeechInput.java`
- **Pushing a DIRECTORY into `Android/data` fails in a new way.** Per file:
  `remote fchown failed: Operation not permitted`, then `failed to read copy
  response: EOF` — *after* creating the folders, so it looks half-successful.
  Staged through `/sdcard/Download` and moved, like the `.nx` files.
- **`InventoryPackets.h` used `Sound` without including `Audio.h`** and got away
  with it because every file that included it happened to include Audio.h
  first. The first one that did not got an error on a line nobody had touched.

## 28 August 2026

- **52 more hats: the cap type was deciding whether the HAT got drawn, not just
  the hair.** A hat's parts carry their own `z` — `cap` or `capOverHair` — and
  that is what picks the layer. `CharLook` asked for `CAP_OVER_HAIR` in one
  branch out of four, so a `capOverHair` hat was invisible in the other three:
  25 simply did not appear (Tilted Fedora), and 27 hid all the hair and then
  drew an empty layer, which is a player standing there **bald with no helmet**
  (Red Snowboard Helmet). Both layers are now asked for in every branch, in the
  climbing view too, where `CAP_OVER_HAIR` was never drawn at all. A hat using
  only `cap` is untouched — `Clothing::draw` does nothing for an empty layer.
  `Character/Look/CharLook.cpp`
- **Every "Weekdays" 2x EXP card was inert, at every hour of every day.** Nine
  rows in `nxcoupons` had lost the leading `5` of their item id — `211004` where
  the item is `5211004` — so `Server.updateActiveCoupons` matched nothing and
  `Character.setActiveCoupons` skipped the coupon. 8 of the 35 rate coupons on
  sale could never work. Renumbered `+5000000`. **Coupons are also gated by a
  weekday bitmask and a 4-hour window**, so buying the wrong one for the current
  hour did nothing and said nothing; all 40 rows are now `activeday=254`,
  `starthour=0`, `endhour=24`. Takes effect on server restart, or within
  `COUPON_INTERVAL` (60 min).
- **144 of the game's 908 hats could never be drawn — 60 of them on sale in the
  cash shop.** `CharEquips::getcaptype()` compared the item's `vslot` against
  three literal strings and returned `NONE` for every other spelling, and
  `CharLook`'s `NONE` branch draws the hair and never calls
  `equips.draw(HAT, ...)` at all. The hat had an icon, a name, a price, every
  stance, every bitmap and every origin — it listed, sold, banked, equipped, and
  was invisible, with nothing in the game to say why. A vslot is a LIST of the
  visual slots an equip covers (`CpHdH1H2H3H4`), not a name, so it is now read
  by counting the hair tokens. The three known literals are kept ahead of it, so
  the 764 hats that already drew correctly cannot be touched.
  `Character/Look/CharEquips.cpp`
- **`shop_audit.py` passed all 60 of them**, because it only ever asked whether
  the ART existed, never whether the renderer would reach it. "The data is
  present" and "the client will draw it" are different questions. Now checks the
  vslot too.
- **The audit's last 5 "broken" items were the Transparent set, working
  perfectly.** Transparent Hat / Earrings / Shoes / Gloves / Cape have no
  character art because drawing nothing IS the product — you wear them for the
  stats and show your hairstyle. Second time this tool has condemned a whole
  category of working items, and the same mistake as the face accessories:
  asking whether art sits where the tool expects instead of what the item is
  for. Told apart structurally rather than by an id list — nothing but `info` is
  a deliberately empty equip, whereas art that exists under names the renderer
  never asks for is the real fault. **The shop now audits 1866 of 1866 clean.**
- **The texture atlas had no eviction, and one animation owned 70% of it.**
  `Logo.img/Nexon` in the v178 `UI.nx` is 136 frames of 720x480 — 47M pixels
  against an atlas holding 67M — and playing it once at launch cost that space
  for the whole session. Add the cash shop's ~14M and there was no room left for
  the map, so sprites were skipped one at a time and the world drew flat grey
  with only the chat text on it. The text survived because fonts sit below
  `fontymax` and a reset does not touch them, which is what made it read as a
  rendering bug rather than running out of room. The atlas is now emptied at
  `UI::change_state` and when the intro ends, via the existing `reset_pending`
  request so it happens at the top of a frame rather than mid-draw.
  `IO/UI.cpp`, `IO/UITypes/UILogo.cpp`, `Graphics/GraphicsGL.cpp`
- **"Only `KEYCODE_BACK` was ever seen" from the Quest controllers was my own
  filter.** The probe excluded `SOURCE_MOUSE`, which is exactly where a
  controller button arrives, so it reported the absence it had created. An
  unfiltered probe found six distinguishable signals. Findings and the ceiling
  are in `docs_QUEST.md`; **do not re-run that investigation.**
- **A and the trigger are the same event because vrshell makes them the same
  event.** The controllers reach a 2D app as *injected* touchscreen events from
  `IInputDataInjection` (device ids in the `0x100000` virtual range), with the
  distinction discarded on the way in. The real gamepad node carries all of it —
  `BTN_GAMEPAD/EAST/NORTH/WEST`, both grips, Menu, an analog trigger — and
  Android marks it `Enabled: false` behind a signature permission.
  `secure/controller_emulation_mode` 1 and 2 were both tried across reboots:
  they add enabled-but-silent sibling nodes and leave the real one off.
- **"Missing nx file" for a complete 4.5 GB install.** adb writes the data as
  the shell user into a folder it creates with `drwxrws---`; the game runs as
  someone else and could not traverse in. Present, unreadable, reported
  missing. `tools/deploy_data.sh`, `Util/NxFiles.cpp`
- **Fonts never reached the Quest, silently.** adb cannot create a directory
  under `/sdcard/Android/data` on Android 11+; the `.nx` files went into a
  folder that existed, the fonts needed one that did not, and the push was
  hidden behind `>/dev/null`. Would have been 4.5 GB installed with no text
  ever appearing. `tools/deploy_data.sh`
- **`deploy_data.sh` blamed the device for adb not being on PATH** — "Cannot
  reach &lt;serial&gt;. Is USB debugging authorised?" when the truth was that
  the command did not exist.
- **Closing a window answered "no" rather than "close".** `NpcTalkMore` carries
  three answers — 1 confirm, 0 decline, -1 end — and Escape has always sent 0.
  On a yes/no question, dismissing the box was answering No.
  `UIElement::Action`
- **Enter never advanced NPC dialogue.** Only escape was handled, so a
  conversation could be abandoned from the keyboard but never continued.
  `UINpcTalk::send_key`
- **Both thumbsticks were discarded.** Only the triggers were read, so a
  controller could press every button and never take a step. Hidden on the
  Thor by its d-pad. `IO/Window_Android.cpp`
- **Party HP bars had a class, a setter and no handler.**
  `UPDATE_PARTYMEMBER_HP` (201) was never registered, so nothing ever called
  `Party::update_member_hp`. `Net/Handlers/SocialHandlers.cpp`
- **`shop_audit.py` cried wolf on 79 working items.** It still checked face
  accessories for stance art after they were fixed to use expressions. 84
  reported, 5 real.
- **Ereve showed Kerning Square.** The world map filename was computed as
  `mapid / 10000000`; the files are a hierarchy of regions and the numbers mean
  nothing. `UIWorldMap::world_containing`
- **Cygnus and Aran read the explorer look list.** Their own — `PremiumChar*`,
  `OrientChar*` — sit at the root of `MakeCharInfo`, not under `Info`. No
  visible change in v83 data, which has no Cygnus wardrobe.
- **Scripted portals were drawn as nothing**, and portals are entered by
  pressing UP, so an invisible portal is an exit that cannot be found. The
  Cygnus tutorial's way out was one. `MapPortals::init`
- **Contact portals needed UP.** Types 3 and 9 are *collision* portals — the
  hint and cutscene triggers tutorials are built from — and only ever fired by
  accident. `Stage::check_touch_portals`
- **Every scripted portal in the game was ignored.** `valid = mapid <
  999999999`, and a scripted portal always carries exactly that, because the
  server decides where it leads. `Gameplay/MapleMap/Portal.h`
- **NPC dialogue types were the wrong numbers.** The server sends 0 say / 1
  yes-no / 4 simple / 12 accept-decline; the client had 4 as accept-decline and
  no 12, so quests could not be accepted. The two trailing bytes that tell
  ok/next/prev apart were read, passed in, and discarded — the parameter had no
  name. `UINpcTalk::layout_for`
- **The lower panel claimed the keyboard.** `get_element` falls through to the
  panel by design; `send_key` used the same lookup, found the panel's
  permanently-open world map, and gave it every keypress. The character could
  not move. `UI::get_main_element`
- **A destroyed text box kept keyboard focus.** `change_state` swapped the UI
  state without releasing `focusedtextfield`, an `Optional<Textfield>` that
  does not own what it points at. Also a use-after-free that happened not to
  crash. `UI::change_state`
- **The skill window could kill the game.** `std::stoi` on the SP label, which
  is empty until `change_sp` runs, at the top of `button_pressed` — so every
  button went through it, Close included. `UISkillbook::spare_sp`
- **Disconnects mid-play were the client's own timeout.** Cosmic has no
  heartbeat; it pings only an *idle* connection, so silence while playing means
  nothing. Raised to 120s and now requires silence in both directions.
  `Net/Session.cpp`
- **AFK ended the session two ways** — the timeout above had 15 seconds of
  slack against a 30-second ping, and nothing kept the screen awake, so the
  display slept, SDL halted, the pongs stopped and the server hung up after 15
  seconds. `FLAG_KEEP_SCREEN_ON`
- **An include that only existed on Windows.** `UIChatBar.h` for a file spelled
  `UIChatbar.h`; case-insensitive locally, fatal on the CI runner.
  `tools/check_includes.py` now catches the class in seconds.

## 24 August 2026

- **Character transfer lost the last field of the last row.** A tab-separated
  row ending in an empty string ends in a tab, `.strip()` removed it, and
  `zip()` dropped the last *column* without complaint. `tools/character.py`
- **`questprogress` and `medalmaps` hang off a queststatus row**, not the
  character, so carrying their parent id verbatim silently dropped every kill
  count.
- **Cosmic spells the character key three ways** — `characterid`, `cid`,
  `charid` — and looking for two left behind the monster book, cooldowns and
  `area_info`.
- **One server, not seven.** The duplicate-server guard existed only in the
  repo, and tested for a process rather than the port it cannot share.
  `tools/termux_setup.sh`

---

## The shapes worth recognising

**State outliving the screen that owned it.** The text box, the world map, the
keymap. When a screen changes, whatever the last one registered is still
registered.

**A value read and then discarded.** The dialogue style bytes had no parameter
name. The party HP handler did not exist. Both look like features that were
never written; both were three-quarters written already.

**A doc that stopped being true.** The second screen, the skill window and
parties were all described as unbuilt while working. Three wrong turns in one
session. *Check the code before planning around a doc.*

**An error behind `>/dev/null`.** The fonts. The wake lock. Silence is not
success.

**A message naming the wrong cause.** "Missing nx file" for files that were
present; "Cannot reach the device" for adb not being installed. Worse than
silence, because it sends the search somewhere real and wrong. When a message
names a cause, check that the cause is even possible.
