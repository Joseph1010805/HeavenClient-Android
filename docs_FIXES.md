# A running list of fixes

Newest first. One line each, with the *cause* rather than the symptom —
symptoms repeat, causes do not.

Kept because the same shapes come round again: state left behind when a screen
changes, a value read but never used, a doc that stopped being true, and an
error hidden behind `>/dev/null`.

## 28 August 2026

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
