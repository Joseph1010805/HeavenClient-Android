# The running list

Everything that has been asked for and is not finished, plus what was
finished recently enough to still need checking at the table.

**This file is the tally.** When something is asked for it goes in here the
same day, whether or not it is started; when it is done it moves to *Recently
landed* and then out. A list that only records decisions already acted on is
a diary, not a plan.

*Last updated: 1 September 2026.*

---

## Waiting on a test at the table

Built and deployed, not yet confirmed by a human. An unverified fix is a
claim, not a result.

| | Needs |
|---|---|
| ⭐ **PETS - CALL ONE OUT** | The client could never ask for a pet: every pet opcode here was inbound, and `grep -i pet Net/Packets/` returned nothing. New `SpawnPetPacket` (opcode 98, layout read out of Cosmic's `SpawnPetHandler`), and `use_item` now routes 500xxxx to it instead of USE_CASH_ITEM. **APK on the RP5.** Buy a pet, tap it in the CASH tab - it should appear and follow you. ⚠ It still will not pick up loot or learn tricks; those are three more packets that also do not exist. |
| ⭐ **THREE SNAILS - NOBODY ELSE SAW THE SHELLS** | Not a drawing problem: Cosmic's `RangedAttackHandler` hunts the bag for an arrow, star or bullet, and a shell is none of them, so `projectile` stayed 0 and the block that **broadcasts the attack AND applies its damage** was skipped entirely. The thrower saw their own animation because the client draws that itself; the onlooker saw nothing and the monster took nothing. Also null-guarded the weapon slot - a bare-handed beginner threw an NPE instead. Server staged on the RP5. Two characters, one throws snails, the other should see the shell fly. |
| ⭐ **THE PANEL, ON THE RP5's ONE SCREEN** | Mail, Party, the bag pages and the stat popup existed only on the Thor - the RP5 logs `no second display` and the whole panel was a no-op there. It is now drawn OVER the game on a one-screen device, in the main pass, at the main screen's own design size. **MENU opens it** (that key toggles the game's own icon row where a real panel exists, so the Thor is unchanged); **swipe back from the top level puts it away**; every touch while it is open belongs to it, so a tap on a button cannot also walk the character. APK on the RP5. |
| ⭐ **AUTO-ASSIGN IS GONE, AND POINTS ARE STAGED** | Refusing AUTO for Beginners only narrowed the accident - it still poured every point into one stat in one press for everyone else, with no undo. Removed entirely (button never built, its case removed, `panel_auto_box` replaced). The chips now STAGE: each press counts up locally and shows `4 > 5` under the button, and nothing goes down the wire until **LOCK IN**; **GO BACK** and the X throw it all away. ⚠ Also fixed while doing it: the AP loop ran `<= BT_AUTO` and would have dereferenced the now-missing button on every level-up. APK on the RP5. |
| ⭐ **THE RP5's BOTTOM ROW IS OURS NOW** | Character / Community / Menu / Event / Settings down there opened the game's own icon popups - built for a mouse, and several leading to things v83 does not have (Union, Auction, Monster Life). On a one-screen device each now opens the panel at the matching section instead: Adventure, Settings, Character, Social, Daily. `open_overlay` returns false where a real panel exists, so every one of them falls through to what it always did and **the Thor is unchanged**. Cash Shop deliberately left alone - it works and the panel does not host it. |
| ⭐ **ONE MENU ICON ON THE RP5, NOTHING ON THE THOR** | The top-screen row - the game's Menu / Settings / Character / Cash Shop popups plus the megaphone, voice-to-text and party buttons - is all in the panel already, and all of it was drawn for a mouse. **Thor: gone entirely** (the panel is downstairs permanently). **RP5: ONE button** at the old Menu slot, our Home icon, labelled MENU, opening the panel at the top. It carries the ⭕ alert badge for everything inside - points to spend, post waiting, an unclaimed daily - since there is nowhere else for it to go. `apply_menu_policy()` runs at build AND when the quickslot bar folds, which used to resurrect retired buttons. |
| ⭐ **CASH SHOP MOVED TO THE HOME MENU** | It had to - its button went with the row. An ACTION on the menu, not a page: the server pulls the character out of the map and takes over the client, so there is nothing for the panel to navigate to. |
| ⭐ **PARCHMENT UNDER LEAF WINDOWS ONLY** | Menus keep the frame showing through, which is what makes Home / Character / Social read as one place. A **final branch** that hosts one of the game's own windows - the ETC tab, the equipment rack - now gets the brown parchment behind it, drawn over the content area so the gauges and breadcrumb stay on the frame. Those windows were drawn in 2005 to sit on paper and had nothing to read against over a live map. |
| ⭐ **POTION PIPS OUT, GAUGES LOUDER** | The red bottle at the foot of the red bar labelled a colour with the same colour. Gone. Both bars: rest colours pushed toward their hue, vivid ends made pure, fill opacity 0.90 → 0.98. The low warning starts at **60%** instead of 50% and now changes BRIGHTNESS as well as hue - `breathe()` was written and never called, so the whole warning was a hue shift that was easy to miss at arm's length. Urgency is squared, so nearly-empty shouts rather than sloping. |
| ⭐ **R3 SPEAKS, L3 BROADCASTS** | Both stick clicks start the SAME voice capture - the live balloon over your head, the pause that decides the sentence ended - and differ only in reach. **R3** → map chat: a bubble over your head and a line in the running chat. **L3** → a super megaphone, a banner across every screen in the world. Taken before the rebinder (like Back), so they cannot be rebound and leave a keyboard-less handheld with no way to speak. |
| ⭐ **MAIL PAGE DELETED, DELIVERY KEPT** | The page, its pick-list, its keyboard branches and the envelope badge are gone - roughly 250 lines. **PostBox is untouched and still runs.** L3's banner is also posted to everyone this device has ever played with, so words spoken with no network wait in the outbox and land in that person's running chat when they next connect, from another state if a relay is set. That is the car-to-college case, without a page to look at. |
| **GIFTING - not done, and cheap if wanted** | Server side is complete (Duey, `CarryPort`). What is missing is a UI. Trade already covers hand-to-hand; a gift page would only add "leave it for somebody who is out". Say the word. |
| ⭐ **GIFTS - DUEY, WIRED UP** | Cosmic's `DueyProcessor` was complete and `USE_DUEY: true`; this client had never sent the packet. New `DueyPackets.h` (open / send / claim on opcode 65) and `ParcelHandler` (322) parsing the counter - the 13-byte fixed sender and the always-200-byte note are what would silently eat the rest of the packet, so both are read exactly. New **Gifts** page under Social: parcels waiting at the top (tap to collect), people below (tap to send). Item picking reuses the bag carry that already feeds the hotkey page - pick a potion on the item page, open Gifts, tap a name. ⚠ **Duey charges 5,000 mesos** and **refuses same-account**, so you cannot pass items between your own characters. Both are the server's rules. Untested on the wire. |
| ⭐ **HOLD A HOTKEY TO CLEAR IT** | A short tap USES what is in a slot, so there was no gesture left meaning "take it out" - a slot could be overwritten but never emptied, and a skill bound by mistake was permanent. ~0.75s hold now frees it and saves. ⚠ Counted in `update()`, not from `touch_ticks`: that only advances when a touch EVENT arrives, so a finger held still never aged and the hold would never have fired. |
| **TO HOTKEYS no longer covers the MP figure** | It sat 10px off the bottom; the MP number sits at `VITAL_W + 17`. Now measured off those same two constants so they cannot drift apart again. |
| **Home menu on parchment** | The other menus keep the frame showing through - Home is where the panel rests and is what gets looked at most. |
| ⭐ **TWO CONTROLLERS - SECOND ONE NOW WORKS** | `SDL_CONTROLLERDEVICEADDED` only opened a pad **if none was open yet**, so a Bluetooth controller paired AFTER the game started was seen, logged and ignored - exactly the guest-joins-on-the-couch case. It now opens the pad the event names. Also fixed the reverse: REMOVED closed `gamepad` whichever device actually left, so a guest's pad going flat took the host's controller down with it and left a dangling handle in `pads`. Matched by instance id now. ⚠ `which` is a device INDEX on ADDED and an INSTANCE ID on REMOVED - different numbers, and the usual way this is written wrong. |
| **Gift list: duplicate names fixed** | `PostBox::load()` APPENDED to the known list instead of replacing it, and `set_account()` re-runs load once per session when the name switches from account to character - so everyone appeared twice, three times after a relog. Now cleared first, deduped, and your own name skipped. |
| **Scrolling on our own lists** | Gift and Trade lists scroll with a vertical drag, with a thumb that shows how much more there is. Only pages the panel draws BY HAND - the ones hosting the game's own windows already have sliders and `send_scroll` reaches them. ⚠ Other hand-drawn lists (party, room chat) still need the same treatment if they grow. |
| **Panel tidy-up** | Parchment removed everywhere (the moving background is better). Daily moved under Adventure. "Character > Character" renamed **Stats**. TO HOTKEYS moved off the EQUIPMENT folder onto its three leaves, where something can actually be picked up. New **Worn** and **Cosmetic** icons wired and deployed in Map001.nx. |
| ⭐ **THE 3-MINUTE RUSH** | New `DailyRush` on the server, once a day, ladder **10 / 25 / 50 / 100 / 200 kills**: potions → mesos → mesos+NX → more → mesos+NX+**gachapon ticket**. Same five-level kill rule as `DailyPve` (so it cannot be farmed on snails) and hooked on the same `Monster` call. Started by **`@rush`** - a chat command, not a new opcode, because inventing opcodes on this project has meant a silent protocol failure every time; the PvE page's START strip sends the same words. `@rush done` collects early; logging off mid-rush still pays. Countdown published through info quest **7771** as `secondsLeft:kills`, the trick DailyPve already uses. ⚠ **NOT PLAYED YET.** |
| ⭐ **MESSAGES = REACHING SOMEBODY WHO IS OFFLINE** | The page was the room's chat log with a microphone under it - which is what R3 does now from anywhere, no page needed. It is a list of people and a SPEAK button: pick a name, say the words. Online, it arrives at once; offline, it waits in the outbox and lands in their running chat when they next connect. Renamed **Message**, scrolls, and the button says who it is for. |
| ⚠ **L3 BROADCAST HAD BEEN LOST** | The megaphone branch I added to `UIChatbar` was not in the file any more - `dictate_to_world` was set and never read, so L3 fell through to ordinary map chat. Restored with the post-box leg, and verified by grep this time, not by the build passing. |
| **Duplicate name: the real cause** | Not the loader. `note.from` comes off a newline-separated wire and keeps a trailing `
`, so "ianjuicce" from the map and "ianjuicce
" from a message were two strings that DRAW IDENTICALLY - and `save_known` wrote the stray byte to disk. One `tidy()` now guards every path in. The device's list was deleted so the bad entry cannot return. |
| **Panel** | Options dropped from Settings (nothing on it was changeable); Keys, Pad, Report and Exit kept. "Type, then ENTER" removed from the chat line. |
| ⭐ **THE THOR's TOP SCREEN IS THE GAME NOW** | HP/MP bar, EXP bar, level, name **and the minimap** are gone from the main screen where a panel exists. All of it is on the panel already - HP up its left edge, MP up its right, EXP along its foot, Minimap its own page - so the top screen was covering the map to repeat the other screen. The RP5 keeps the lot; it has nowhere else to put them. |
| ⭐ **SWIPE NAVIGATION REMOVED** | Left-to-right for back and right-to-left for home are gone from the panel. The address bar along the top has been a row of buttons for longer, and every crumb goes straight to that level - so the gesture was an invisible second copy of something already on screen, and it fired by accident constantly (dragging a list, panning a map, a thumb sliding while pressing). ⚠ **The one-screen overlay is now closed by pressing MENU again**, not by swiping. |
| **Hotkeys: a tap is a tap** | Drag tolerance tripled on that page only - its cells are 74 across and nothing there pans, so a thumb that slid ten pixels was a press being thrown away as a drag. Plus a 12-frame refusal window so one tap can never use two potions, whatever the touch layer sends. |
| **Minimap sits higher** | A third of the way down its page rather than dead centre - a lone object reads as centred when it sits slightly high. |
| ⭐ **NPC DIALOGUE: BUTTONS NO LONGER OVERLAP** | The pairs were placed a fixed **65px** apart - measured when the artwork was drawn at its own size, before every button in the window was scaled 1.5x for a thumb. 65 is narrower than the buttons are, so Accept sat across Decline. They are now laid out from the RIGHT EDGE off the button's own `bounds()`, which already honours the scale - so this stays right if the scale changes again. All four pairs (Yes/No, Accept/Decline, Back/OK, Back/Next). Text up a size to **A13M**, wrap 320 → 350. |
| → **CASH SHOP RETURN - INSTRUMENTED, NOT GUESSED** | Leaving the shop is a CHANNEL CHANGE: you reconnect and SET_FIELD tells the client where it now is. Found a **silent return** in `set_field` - if the remembered character does not match the one in the packet it drops the whole thing, so no player load, no map load, no transition, and the screen keeps drawing the previous map's scenery and NPCs over a character that is really elsewhere. That is the exact shape of the bug. It now says so. Server logs the map on the way IN and on the way OUT, so the two numbers can be compared. **Reproduce it once and the log will name the cause.** |
| **The panel keyboard** | Type an account with it, then press **next** to reach the other field. Two separate faults, both fixed: `deliver_touches` threw away every touch while a textfield had focus (written when Android's IME covered the panel - it now discards exactly the presses meant for our own keyboard), and nothing was ever focused to type INTO, so the login screen now focuses a field itself. |
| **Login: games nearby** | The list should find the other handheld with no button pressed. |
| **Login: the six-digit code** | Create a game, read the code off the screen, type it on the other device. A wrong code must be refused. |
| **Login screen looks** | Parchment cropped to its plain middle (the half mushroom and the cut ornament are gone), everything inside the frame, wordmark bottom left. |
| **Equipment on a hotkey** | A weapon, a hat and a cash item: each should go on and come off. |
| **Controller bind** | Arm it, press a face button, check the action follows the button afterwards. |
| **The report** | `adb shell run-as org.heavenclient.android cat files/report.txt` |
| **Voice chat** | Two devices. Cannot be proven on one. |
| ⭐⭐ **QUEST REWARDS - EVERY GENERATED QUEST PAID NOTHING** | `qm.completeQuest()` reaches `Quest.forceComplete`, which marks it done and plays the sparkle and **never runs `completeActs`** - so no exp, no meso, no items, on all 2,392. Hand-written scripts hid it by paying out themselves. New `qm.finishQuest()` / `qm.beginQuest()` apply the Act.img actions; the old names are untouched so the 229 hand-written scripts cannot pay twice. Staged - press CREATE A GAME. |
| ⭐ **ROBIN'S QUIZ - 213 QUIZZES NOW REAL** | New scripts staged; press CREATE A GAME on the RP5 to load them. Robin should ask three questions, refuse a wrong answer with the game's own "Incorrect! ... Start over!" line, and re-ask. ⚠ Only the END phase is handled - a quiz on the START phase would need the accept box threaded through it. |
| ⭐ **MONSTERS ON LONG PLATFORMS** | `Footholdtree::get_edge` looked exactly TWO footholds ahead and returned the map's outer wall for anything longer - so a platform of 3+ footholds reported no edge at all and everything walked off the end. Nothing to do with jumping. APK on the RP5; watch a Jr. Sentinel. |
| ⭐ **NINA - TALK TO HER ON THE RP5** | Server live there. She should read out two paragraphs about Sen and offer Accept/Decline, instead of the quest appearing in the journal unannounced. ⚠ Needs a character who has NOT already taken 1032 - the ones already accepted stay accepted. |
| ⭐ **ROGER'S REWARD PAGE - TEST ON THE RP5** | APK installed. It should read as a sentence AND show the two potion icons inline: "[icon] 3 Apple / [icon] 3 Green Apple". |
| (was) **ROGER'S REWARD PAGE - TEST ON THE RP5** | APK installed there. Hand the quest in: "I will give you a present" -> "this is all I can teach you" -> the last page must now have **Back AND OK**, and OK must hand over the potions and complete it. |
| (done) **ROGER: blocked on [ITEM]** | The log said it outright: the apple must be CONSUMED, not waited out. `count 0` = "have none left". Working as designed; it was the silence that was the bug. |
| (was) **ROGER - PRESS COMPLETE AGAIN ON THE RP5** | It will now TELL YOU why, on screen, instead of doing nothing. Live there (jar installed, "Cosmic is now online"). |
| (was) **ROGER - TALK TO HIM AGAIN ON THE RP5** | The instrumented server IS now running there (verified: "installing a different Cosmic.jar", then "Cosmic is now online"). Talk to Roger and `bootstrap.log` will print `cannot finish: [...]`. |
| **(was) ROGER: THE SERVER NOW SAYS WHY** | `Quest.whyCannotComplete` + a log line in `QuestDialogue`. Talk to Roger with the quest open and `server.log` prints e.g. `cannot finish: [ITEM]`. That is the answer I have twice failed to reason out. Staged on the RP5; the Thor needs staging. |
| ⭐ **RP5 IS LIVE - GO TALK TO NINA** | New jar staged and the server restarted on it (verified: "Cosmic is now online", 8484 listening). Log in on the RP5 and walk to Nina in Amherst. She should read out two paragraphs about Sen and offer accept/decline; NPCs with no quest for you should say their small-talk line instead of nothing. **I could not test it myself** - the RP5's database is its own and has no character on it yet. |
| **THOR - still the old server** | Off USB all session. `tools/stage_server.sh <serial>` then press CREATE A GAME. |
| **Say.img quest dialogue** | Do Pio, Sam and John talk. The scripts only actually loaded on 1 Sep. |
| **Minimap: drag it** | With a finger AND with the stylus. It could not be dragged at all before - see below. |
| **Minimap: the portals** | They should sit on the portals, not below and left of them. |
| **Minimap: CENTRE** | Press it. The dot should end up in the middle of the screen. |
| **Monsters staying on their platforms** | Watch a layered map for a few minutes. Nothing should end up at the bottom that did not start there. |
| ⚠ **STORAGE - NEVER RUN** | Built to Cosmic's spec and it compiles; not one byte on a wire. Talk to a storage keeper (level 15+). Tap a bank item to take it out, a bag item to put it away, the meso buttons for money. |
| **Cash equipment tooltip** | Tap equipped cash glasses. It should show THAT item and not take the game down. Crash confirmed and fixed - trace below. |
| ⚠ **TRADE — NEEDS TWO CLIENTS** | Every layout was read out of `PlayerInteractionHandler.java` and `PacketCreator.java` rather than guessed, and Social > Trade is live on the Thor - it opens, it reads the map, and with nobody else standing there it says so. But **not one byte has crossed a wire**: that takes two characters logged in at once and there is one device. Do not treat it as working. |
| **Hotkeys surviving a restart** | Deployed and half-proven: placing a potion now writes `HotkeySlots = 2:2000000,...` into the Settings FILE the moment it lands, where that line used to stay empty. The last step - kill the game, log back in, see the potion still on slot 1 - was not run, so confirm it once. |

---

## THEIR MENU vs OURS - what we are missing (audited 3 Sep)

⚠ **WHY OUR ICONS EXIST AT ALL:** screen real estate, not disk space. The
game's own menu is built for a mouse on a monitor; ours is built for a thumb
on a handheld. Replacing their icons with ours is the point - and it means
anything only reachable through THEIR menu becomes unreachable once ours
replaces it. That is what this list is for.

**On a one-screen device the panel does not exist at all** (`no second
display` in the RP5's log), so today those handhelds have ONLY the game's own
menu, and none of Mail, Party, the bag pages or the stat popup. That is the
gap that makes this list urgent rather than tidy.

### In their menu, NOT in ours

| theirs | state here | worth adopting |
|---|---|---|
| **Cash Shop** | works (browse/buy/take out/wear) | **yes** - reachable only by their button today |
| **Fishing** | not built; needs client chair support | yes, on his list |
| **Monster Collection** | not built | yes, on his list |
| **Help** | not built | yes - a child's first stop |
| **Friends / buddy list** | **nothing at all on our side** | **yes** - the only way to see who is online |
| **Party** | ours has it (Chat > Party) | already done |
| **Guild** | not built, no guilds exist yet | later |
| **Medal / Achievement** | not built | later |
| **Battle statistics** | not built | low |
| **Event schedule** | not built | low |
| **Claim** (cash items) | not built | low - locker already works |
| **Channel change** | one channel only | not applicable |
| **Union / Auction / Monster Life** | not in v83 | never |

### In ours, not in theirs

Mail, Trade-by-name, Emotions, Room chat, Shout, Daily PvE/PvP, Hotkeys,
Controller bind, Report-a-bug, the world map and minimap as pages.

### PETS - the answer to "do they work now?"  **NO, and here is the exact reason**

Nearly everything is there, which is why this looked finished:

  * client: `PetLook` (draws it, walks it, HANG/FLY/STAND stances), `Char`
    already holds and draws up to three pets, `SpawnPetHandler` **receives**
    opcode 168 and puts a pet on screen
  * server: EIGHT handlers - spawn, move, chat, command, food, loot,
    auto-pot, exclude-items - all live

**The one missing piece is a packet the client never sends.** Cosmic listens
for `SPAWN_PET (0x62)` and there is no such packet anywhere in `Net/Packets/`
- `grep -i pet Net/Packets/` returns nothing at all. Every pet opcode in this
client is INBOUND. So a pet can be bought, and it sits in the cash bag doing
nothing, because using it sends "use a cash item" and spawning a pet is its
own request, not a use.

⚠ **This is also why the CASH-tab USE button did not make pets work** - I
nearly recorded that it had. It fixed hotkeys and 2x cards; it could not fix
this, because the packet does not exist to send.

Also missing once one spawns, in order of how much they matter:

| | what breaks without it |
|---|---|
| `MOVE_PET` | your pet follows YOU fine, but everyone else sees it frozen |
| `PET_LOOT` | it will not pick anything up - the main reason to own one |
| `PET_COMMAND` | no tricks, no closeness gain, so it never levels |
| `PET_FOOD` | fullness only falls; at 0 the pet goes home |

### The single-screen plan (his direction, 3 Sep)

  * put OUR panel on the main screen where there is no second display -
    `SecondScreenPanel` is self-contained (`update()` + `draw(space)`), so it
    can be drawn onto the main surface, toggled by a button, with touches
    routed to it
  * **sub-menus become LISTS rather than big buttons** on one screen - less
    room, and a list scrolls where a grid cannot
  * **drop what only makes sense with two screens**: the panel minimap, and
    anything whose whole purpose was "keep the top screen clear". Character
    and equipment open on the main screen there, obviously.
  * adopt the five above (cash shop, fishing, monster book, help, friends)
    with OUR icons

## MORE PLAYERS, AND A WAY FOR THEM TO REPORT (asked 2 Sep)

The strategy: the bottleneck is TABLE TIME, not engineering time, because the
bugs that remain are *coherent and wrong* and no tool sees those. More people
playing is the only cure, and a report that survives the moment is the only
way what they find reaches anybody.

**`@bug` already existed - and had this project's disease.** It logged at
INFO, and `serverlog.py` keeps only WARN and ERROR, so **every bug report
ever filed was discarded by our own reading tool.** It also recorded only the
words: "the quiz is broken", with no map and no NPC, cannot be acted on.

Rewritten (`ReportBugCommand.java`), all verified to compile:
- **The report writes itself.** The player supplies one sentence; the server
  attaches who / when / map id + name / x,y / **the three nearest NPCs by id**
  / quests in progress. The NPC id is the whole diagnosis for most of these.
- **Durable and REACHABLE.** It picks `/sdcard/Download/cosmic/bugs.txt` when
  that folder exists. Writing to the working directory would have been
  perfectly durable and completely unreadable - the server runs in Termux's
  private home, which adb cannot see. Same failure in a new coat, caught
  before it shipped. Override with `COSMIC_BUG_FILE`.
- **Logged at WARN**, so `serverlog.py` shows a report beside whatever the
  server was complaining about at that second - usually the answer.
- **Honest confirmation**: if the file could not be written the player is told
  so, not thanked.
- **Discoverability**: one line at login, in words a child can act on. A
  reporting channel nobody knows about is not a reporting channel, which is
  why `@bug` had sat unused in this codebase for years.
- **`playlog.py` prints reports verbatim, first, never digested.** Everything
  else is grouped by shape; two people reporting the same words from two maps
  are two different bugs.

**The installer.** The path fault behind every failure in the 1 Sep logs is
already fixed (`install.sh:442-444` converts all three data paths to Windows
form for adb.exe). ⚠ **It has not been run since the fix.** That is the next
action and it needs a device.

⚠ **The honest bound on "easy install": the NX data.** We cannot ship Nexon's
files, so this can never be download-and-play. It is: a PC, a cable, and their
own v83 data - or a device prepared in person. That caps the audience at
family and friends nearby, which is the intended audience anyway.

## The two automation tools (asked 2 Sep, landed)

**`tools/playlog.py`** — the client now keeps its silent failures on disk
(`Util/Silent.cpp` appends to `files/HeavenClient/playlog.txt` in the app's
own external folder), and this pulls that back off every plugged-in device
and groups it by shape the way `serverlog.py` does. It picks up a device's
`bootstrap.log` and `cosmic-log.log` too, since half of any bug is only
visible from the server's side. `--raw`, `--since MINS`, `--clear`.
⚠ **The APK carrying the Silent.cpp change is built but NOT yet installed**,
and playlog.py has only been exercised against a synthetic log — no device
was connected. First job with a handheld in hand.

**`tools/quest_lint.py`** — asks Say.img what each quest NEEDS and the
script what it DOES, for all 2,392 at once, in a second, without the game
running. It earned itself immediately:

- **350 quests ask a QUIZ on the way IN** and were generated as a plain
  conversation, so every answer was accepted — the exact fault Robin's quiz
  had, in 350 more places. `gen_quest_scripts.py` now lets **each phase pick
  its own form** independently; regenerated, and the lint is clean.
- The first run said **632** problems. It was **354**: the START-quiz check
  and the stray-options check were counting the same 278 scripts twice,
  because a start quiz carries its own `#L0#` markers. Fixed — the checks no
  longer overlap.
- **4 remain**, single "you say this" lines outside a quiz. The client
  already renders `#L<n>#…#l` as a selectable line (`UINpcTalk.cpp:776`), so
  they read fine. Left alone deliberately.

Neither tool can see a subsystem that does something coherent and *wrong* —
both say so in their own headers. That is still what playing finds.

## Asked for, not started

**The chat and messaging set (asked 1 Sep)**
- **Messages = DMs and developer messages.** ⚠ The design question:
  v83's whisper reaches somebody **online, on your channel, right now**.
  There is no offline mail in the protocol, so "a DM from anyone you have
  played with" means storing messages server-side and delivering them at
  login - a new table and a new delivery path in Cosmic. Developer messages
  are the easy half and could land first.
- **A red unread count** outside the envelope. Easy once messages are
  stored; the storing is the work.
- **Voice becomes the megaphone**: dictate with Vosk, send as a megaphone.
  ✅ The hard half already exists - `USE_FREE_MEGAPHONES` in Cosmic and
  `UIMegaphone` in the client - so this is wiring, not building. **v83 does
  have world shouts**: the Super Megaphone banner.

**The panel**
- **TO HOTKEYS overlaps the MP number** at the bottom right of the Use page.
  Seen while testing the hotkey fix; not chased.
- NPC dialogue on the lower screen, big buttons and scrolling. `UINpcTalk`
  has no `set_panel`, so it is a panel-native rebuild, not a redirect.
- The equipment window's tabs as buttons. The inventory is the pattern and
  the artwork is already in `Map001.nx`.
- The shop windows restyled to match the panel.
- **Controller** page contents (an empty page).
- Adventure as one hub: quest, map and minimap together.
- **Chat backlog.** The page shows as many balloons as fit and no more -
  older lines are simply off the top. It wants the same touch scroll the
  hotkey list and the minimap have.
- Keyboard: the caps are a brown wash on the parchment with dark letters.
  Worth a look at whether they read as pressable.

**Noticed on the RP5, not chased:**
- On a ONE-screen device Android's own keyboard opens the moment the login
  screen appears, because `UILogin`'s constructor focuses a field - upstream
  behaviour, not new. Everything that matters is above it and it is usable,
  but it does cover half the screen unasked.
- `deploy_data.sh` writes `ServerIP = 127.0.0.1` into a fresh device's
  Settings. Harmless now that the login screen finds games by itself, but it
  is pointing a non-hosting handheld at nothing.

**Asked for 2 Sep, not yet started:**
- **Monster Book.** Cosmic has `MonsterBook.java` and `MONSTER_BOOK_COVER`
  (0x39 out), and the card data comes down with the character. Needs a
  window and a card list.
- **Duey.** `DUEY_ACTION` (0x41). Post an item to somebody who is offline -
  which is most of what the DM idea was actually for.
- **Owl of Minerva.** `OWL_ACTION` (0x42) and `OWL_WARP` (0x43). Search who
  is selling what, then warp to their shop.
- **Fishing.** ⚠ The server half is one config line - `USE_FISHING_SYSTEM` is
  **false** in `config.yaml`. The CLIENT half is the work: it has no chair
  support at all. Nothing sends `USE_CHAIR` (0x2B), nothing sends
  `CANCEL_CHAIR` (0x2A), and nothing draws a seated character. Fishing needs
  a FISHING_CHAIR (item 3011000) sat on in one of three maps, so the chair
  has to exist before the fishing can.

**Worth adding from MapleStory's own menu?** Asked 2 Sep. What the SERVER
can actually back, which is the only thing that decides it:
- ✅ **Monster Book.** Cosmic has `client/MonsterBook.java` and a handler.
  Real v83, card drops already work, and it is the one on this list a child
  would actually chase. The client has no window for it at all.
- ✅ **Fishing.** Cosmic has it behind `USE_FISHING_SYSTEM`: sit on a fishing
  chair in a fishing area and it pays out. Needs almost nothing client-side.
- ✅ **Help guide.** Pure client, no server, and the most useful thing on the
  list for somebody who has never played.
- ✅ **Report** - already built.
- ❌ **Medals.** v83 has medal EQUIPS (a title over your head) but no medal
  collection window; that is post-Big-Bang. Nothing to open.
- ❌ **Monster Collection**, ❌ **Auction House**, ❌ **Farm**,
  ❌ **Battle statistics.** All post-Big-Bang. Cosmic has not one line of any
  of them - building these means inventing the server half too.
- Also there and unbuilt, and more use than most of the above:
  **Storage**, **Duey** (post items to someone offline), **Buddy list**,
  **Guild**, **Owl of Minerva** (search who is selling what).

**Roger's quest 1021 - what is known for certain, and what is not.**
- ✅ Completion needs **ZERO Roger's Apples** in the bag. The requirement is
  `item: id 2010007` with NO `count`, and `ItemRequirement.check` reads a
  missing count as 0, which its own condition turns into "must have none".
- ✅ The apple heals **30 HP** (`Consume/0201.img/02010007/spec/hp`) and
  Roger drops you to 25, so 25+30 = 55 clears his script's `getHp() >= 50`
  hand-in gate. That gate is NOT the blocker.
- ✅ No `infoEx`, so `canQuestByInfoProgress` cannot be it either.
- ❌ **Why it still will not complete is UNKNOWN.** Two rounds of reasoning
  from the data got the mechanism right and the outcome wrong. The log line
  above is there to stop a third.

**Gameplay**
- **Enemies clustering in the left corner of one map.** Parked on request,
  not diagnosed. Suspicion - and only that - is that they are falling off
  ledges and collecting at the lowest point.
- **Storage: no scroll past the first 16 of a tab.** Two rows of eight are
  what fits; a full bank has more. It wants arrows, or the touch scroll the
  hotkey list has.
- **Trade, the parts left out of the first build:**
  - **Splitting a stack.** A tap puts the WHOLE stack down. Splitting needs a
    number typed in.
  - **Trade chat.** The packets are handled and the line is shown as a status
    message; there is no way to send one. The room has a Chat page now.
  - **Player shops and hired merchants.** Same opcode, different branches -
    the handler names them in the log rather than swallowing them.
  - ⚠ **Items cannot come off the table.** That is the SERVER's rule, not a
    shortcut: v83 has no take-back. Cancelling returns everything.
- **EXP ticket** as an item you use.
- Mob skill effect visuals from `MobSkill.img`; `attackAfter` timing.
- **Cross-map quest navigation.** Knowing an NPC is in Ellinia does not say
  which portal to walk to. Needs a map-connection graph and a path search.
- **PvP**, after trade. Either a duel with **normalised damage** (flat % of
  max HP, gear and level ignored), which is playable in a session, or a
  Battle Square recreation, which is a project. v83 has no PvP of any kind
  and Cosmic has not one line of it.

**More daily challenges.** The framework is `DailyPve`; each is a hook.
- Explore N maps · kill N beside a party member · a boss a day · meso
  earned · items looted · quests completed.

**The three-device goal**
- Ownership: discovery and election, no HOST/JOIN choice, account handover.
- Thor + RP5 + Quest 3 on the home wifi. Needs no new architecture.

**The installer's front door.** It is a bash script in a terminal, which is
right for us and wrong for anybody else - it needs Git Bash, which a Windows
player will not have. A PowerShell port with a double-clickable `.bat` shim
is the cheapest thing that removes the dependency; a real GUI is a project.
⚠ Whatever it becomes, the PC step cannot be designed away: the game data is
converted from the player's OWN MapleStory client and is never downloaded.

**OpenStory is the reference implementation, and it is ours.**
`~/Documents/Programs/OpenStory` - a far more complete v83 client for the
same Cosmic server. It already has **storage, buddy list, skill macros, a
quest UI with NPC quest indicators, gamepad support and spatial sound**.
Several things on this list were built there first; check it before
inventing. One difference already found: its scripted-quest packets send
`byte, questid, npcid` and NO player position, where ours appends the
position for `isNpcNearby`. Both are accepted - that argument is optional.

**Housekeeping**
- Commit the Cosmic changes. It is a fork; ask before pushing.
- Publish v0.8.
- **Publish a v0.8 release.** The installer's download branch still fetches
  **v0.7** - anybody without a local build gets a months-old client.
- `tools/install.sh --server` (stage_server.sh) is still the one path in the
  installer nobody has run.

---

## Recently landed

- **The login screen has no HOST / JOIN pair any more.** It browses from the
  moment it opens and shows what it finds, with CREATE A GAME as the last row
  of the list. A host chooses a **six-digit code** on creation and a joiner
  must type it. ⚠ The code makes joining *deliberate*; it is **not a
  password** - the scrambled form travels in the mDNS announcement, where
  anyone already on the wifi can see it, and six digits is a million guesses.
  It stops mistakes, not intruders. See `Util/Multiplayer.h`.
- **Social > Chat.** Its own page with its own icon - the speech balloon the
  game draws over a player's head, assembled from `ChatBalloon.img` by
  `tools/make_chat_icon.py`, so it cannot be mistaken for the Messages
  envelope. Type, press ENTER, it goes out as ordinary chat: verified end to
  end on the Thor - typed on the panel, echoed by the server, drawn as a
  balloon over the character.
  - The log is drawn as **balloons**, sized to the words: people on the left
    and brighter, the system on the right and dimmer, so which side a line is
    on answers "is this somebody talking" before it is read.
  - Typing does **not** drive the top screen's chat field. A focused
    textfield swallows every key the game would otherwise get, so opening one
    to talk would take the controls away for as long as you were talking. The
    panel holds the line and hands it to the new `UIChatbar::say`.
- **Monsters no longer migrate to the bottom of the map.** His diagnosis was
  right: they were falling off their ledges, over and over, until they piled
  up at the floor. `TURNATEDGES` is what holds a mob on its foothold, and a
  jump has to let go of it - but EVERY jump did, including the random hop a
  wandering monster takes a quarter of the time. Worse, the take-off ran on
  every frame the stance was JUMP and the mob was on the ground, so a mob
  that had landed relaunched itself until it next thought. Now: only a jump
  taken while HUNTING may leave the platform, take-off happens once, and the
  ledge is retaken on the very frame the mob lands.
- **Storage.** `STORAGE` (62 out / 309 in), and the window is two lists -
  the bank on top, your bag underneath - with one tap to move an item either
  way. ⚠ Opened by an NPC only; there is no request to send, and the server
  refuses the whole thing below level 15.
- **`tools/npc_audit.py`** - the guard. Walks Quest.nx, String.nx and Map.nx
  against Cosmic's `scripts/`, and answers "which NPCs would say nothing if
  you walked up and pressed talk?" offline, with a number. **21 silent of
  1,349 placed** now; it would have said 346 before the small talk went in,
  and it names every one. It cannot see shops (they live in the `shops` DB
  table) or evaluate quest requirements, and it says so rather than guessing.
- ⭐ **`f`, `v` AND `z` WERE BEING TREATED AS COLOUR CODES.** `format_text`
  had them in the "colour and style switches, which carry no text" list. They
  are not colours: `#f<path>#` is a bitmap, `#v<id>#` an item icon, `#z<id>#`
  an item name. So only the two-character `#v` was eaten and the rest spilled
  into the dialogue - Roger's reward page read
  `...my friend!??UI/UIWindow.img/QuestIcon/4/0??2010000 3 Apple`.
  Now: `#v #i #q #s #f` are consumed whole (a gap, not gibberish), `#z` reads
  as an item name, EVERY control byte is dropped rather than just `
` - the
  `??` were newlines with no glyph - and item names go through `ItemData`
  instead of `Consume.img`, which only ever named consumables.
- ⭐⭐ **QUIZZES WERE GENERATED AS PLAIN CONVERSATIONS.** Robin's quest 1036
  asks three questions; the generator emitted `sendNext` for pages full of
  `#L0# ... #l` options, so every answer was accepted, the reply always
  congratulated you, and the quiz could not be failed or really taken.
  **495 of the 2,392 generated scripts had a question in them.**
  Say.img had the whole thing on file and it was simply not read:
  `ask = 1` marks the phase a quiz, `stop/<n>/answer` is the correct option
  **counted from one**, and `stop/<n>/<option>` is the specific rebuke for
  each wrong choice. The generator now emits a real quiz - `sendSimple`,
  check the selection, show the rebuke, ask again - for **213** of them.
  ⚠ The bug that hid it for one round: `answer` is an INT node in some
  quests and a STRING in others, so reading it as text gave None and every
  quiz quietly was not a quiz. The same trap as `life/id` in Map.nx.
- ⭐⭐ **`get_edge` ONLY LOOKED TWO FOOTHOLDS AHEAD.** This is the real
  "monsters walk off ledges" bug, and it was never about jumping - his own
  correction, that Jr. Sentinels cannot jump, is what ruled the rest out.
  `TURNATEDGES` is measured against `get_edge`, which followed one link, then
  another, and if the ground still carried on returned `walls.first()` - the
  far side of the MAP. A platform of three or more footholds therefore
  reported no edge, the clamp never fired, and anything on it walked calmly
  off the end. Most early-map platforms are one or two footholds, which is
  why it looked like it worked and why only SOME monsters ever fell. Now the
  chain is walked to its end, with a guard against a cycle in bad data.
- **Picking up a quest ETC item said the wrong thing.** `showItemUnavailable`
  is one packet for several different refusals, and the client guessed at
  "somebody else owns it" - baffling when alone in the map. The commonest
  cause by far is `needQuestItem`: a quest drop only exists for people on
  that quest. The SERVER now says which reason ("That is for a quest you are
  not on.") and the client's wording is the fallback for the rest.
- ⭐ **QUESTS WITH NO `startscript` WERE ACCEPTED IN SILENCE.** The last face
  of it. `QuestActionHandler` case 1 just called `quest.start(...)` - no
  script, no words - because in the real game the CLIENT has already said
  them and this packet only carries the answer. Ours says nothing, so Nina's
  "Nina's Brother Sen" landed in the journal unannounced. Cases 1 and 2 now
  try `startDialogue`/`endDialogue` first and fall back to the plain
  start/complete only when no script exists, so a quest with no words behaves
  exactly as before. Both entry points return a boolean for that, and a
  missing script is no longer logged as "uncoded" - it is a normal outcome.
- ⭐ **INLINE ICONS IN NPC TEXT - PORTED FROM OPENSTORY.** Quest text writes
  the picture into the sentence (`#v2010000# 3 #t2010000#` = apple icon,
  "3", "Apple"). Four files:
    - `Text::Layout` gained `ImageKind` + `Image` and carries a vector of
      them, with `Text::images()` to read it.
    - `LayoutBuilder::add` recognises `#v #i #q #s`, reserves a 14px square
      and records `(pos, id, kind)`. 14 rather than a true 32px icon on
      OpenStory's advice: a taller box pushes the picture up into the line
      above and it stops reading as part of the sentence.
    - `UINpcTalk::format_text` passes those macros THROUGH untouched - a
      picture cannot be emitted as text - while `#f<path>#` is still dropped,
      since there is no id to look a bitmap up by.
    - `UINpcTalk::draw_inline_icons` opens Item/UI/Skill NX, scales to the
      slot, and clips to the frame so a scrolled line cannot paint over the
      border.
- ⭐ **A PREV-ONLY DIALOGUE PAGE WAS A DEAD END.** The server marks the last
  page of a conversation prev-but-not-next, and `UINpcTalk` rendered that
  literally: one Back button and Close. But the SCRIPT is not finished there
  - its next step hands over the reward and completes the quest, and it only
  runs on a forward answer (mode 1). No button sent one. Roger's 1021 hit it
  exactly: "I will give you a present" -> "this is all I can teach you" ->
  Back, for ever, potions never handed over.
  **Found by comparing against OpenStory**, which throws the prev/next bytes
  away entirely (`int16_t style = 0;` with a comment saying so) and gives
  every text page a single OK that goes forward. Ours keeps Back and adds the
  forward button that has to exist.
- **`isNpcNearby` returned false in SILENCE**, and it sits in front of every
  branch in `QuestActionHandler`. A wrong or missing npc id made the entire
  quest operation vanish with no message, no log, nothing - ahead of all the
  logging added below it. Both its failure paths now log and tell the player.
- ⭐ **"COMPLETE" ON A QUEST THAT IS NOT READY NOW SAYS WHY.**
  ⚠ First attempt fixed `case 2` (COMPLETE) - the WRONG branch. Clicking an
  NPC with a finished-quest mark sends **`case 5` (SCRIPTED_END)**, because
  `MapNpcs::talk_to` picks by whether Check.img gave the quest an
  `endscript`, and Roger's 1021 has one. So the explanation went somewhere no
  click ever reached. Cases 4 and 5 now carry it too, and use the new
  `startDialogue`/`endDialogue` entry points. This was the
  actual Roger bug, and it was never in the dialogue at all: the client sends
  `QUEST_ACTION` (107) from the quest log, not `NPC_TALK`, so none of the NPC
  work touched it. `QuestActionHandler` case 2 ran `if (canComplete) {...}`
  with **no else** - press it and nothing happened, no message, no log line.
  Found by reading the client's own packet log: 107 over and over, and not
  one NPC packet. It now names every failing requirement in the log AND puts
  a plain sentence on the player's screen ("You are still carrying something
  this quest wants used or handed over."). Same for a start that is refused.
- ⭐ **BOOTSTRAP COMPARED TIMESTAMPS, SO NEW BUILDS WERE SKIPPED IN SILENCE.**
  `[ "$STAGE/Cosmic.jar" -nt Cosmic.jar ]`. A staged jar carries the time it
  was BUILT; the installed copy carries the time it was COPIED, which is
  always later. So a jar built at 04:04 staged over one installed at 04:27
  looked OLDER, was skipped without a word, and the log printed the installed
  jar's date as if all was well - while the server ran code from two builds
  back. This is the third face of "staged is not installed": the push said
  ok, the file was on the device, and the install step decided against it.
  Now compared with `cmp -s` - content, never clocks - which also makes
  staging an OLDER build work, to back something out. The scripts tar is
  compared against a copy of the tar rather than against the folder it
  unpacks into, which was the same mistake in a different hat.
- **`stage_server.sh` had NO adb discovery.** The third script in that folder
  to need it and the third written without it: every push failed with
  "adb: command not found" and it reported six files that "did not arrive",
  blaming the device for a tool that was never on PATH.
- **`bootstrap.sh` left the RP5 with no server at all.** It waits for the
  login PORT to come free after killing the old jar - but `run.sh` has a
  second guard that pgreps for the process, and a JVM that has released 8484
  but not finished dying satisfies one and trips the other. So: old server
  stopped, new one refused with "the server is already running", exit 0,
  everything looked fine, nothing was listening. It now waits for the process
  as well as the port. ⚠ `run.sh` itself is generated on the device by
  `termux_setup.sh` and is NOT staged, so a fix there would not travel -
  which is why this had to be fixed in bootstrap.
- ⭐ **THE GENERATED QUEST SCRIPTS WERE NEVER RUNNING.** `QuestScriptManager
  .start(c, questid, npc)` refuses to load a script unless the quest declares
  a `startscript` in `Check.img` - and almost none do, **because in the real
  game the CLIENT owns quest dialogue**: it has its own copy of Say.img and
  reads the words out itself, telling the server only the answer. That is why
  Cosmic never needed Say.img, and why that gate is right for every path
  except ours. So all 2,392 generated scripts were being thrown away before
  they loaded. Roger worked ONLY because quest 1021 happens to declare
  `startscript = q1021s`. New `startDialogue` / `endDialogue` entry points
  skip the gate; `QuestDialogue` uses them.
- **NPCs WITH NOTHING TO GIVE YOU NOW SPEAK.** Same discovery as the Say.img
  one, a layer down: **1,217 of the game's 1,733 NPCs have lines written for
  them in `String.nx/Npc.img/<id>/n0,n1,...`** and Cosmic has never read one.
  Nina has "What would be good for dinner?" on file; talking to her produced
  a log line and silence, because all three of her quests need something the
  player has not done yet. New `server/life/NpcSmallTalk.java`, asked LAST in
  `NPCTalkHandler` - a quest beats it because that is why you walked over, and
  a SHOP beats it because an NPC that sells things and instead remarks about
  dinner is broken.
- **A NEW DEVICE COULD NEVER HOST.** `Readiness::can_try()` required the
  Termux RUN_COMMAND permission - and `LocalServer::start()` is the thing
  that ASKS for that permission. So the permission was needed to reach the
  button, and the button was needed to request the permission. On every
  machine that had granted it once this cost nothing; on a fresh one the
  dialog could not be shown at all and CREATE A GAME's NEXT was dead. Found
  by pressing it on the RP5. `can_try()` is `termux` alone now - exactly the
  reasoning its own comment already applied to the server being up - and the
  popup says "Termux will ask to allow this. Say yes, then press CREATE
  again" instead of showing a wall.
- **THE INSTALLER, RUN FOR THE FIRST TIME.** Wiped an RP5 - app, 4.5GB of
  data, the lot - and ran `tools/install.sh` as a stranger would. It had
  never worked. Four faults, in the order they bit:
  1. `USER: unbound variable` on line 65. Under `set -u` a bare `$USER` -
     which Git Bash does not always set, it sets `USERNAME` - killed the
     script before it printed one word. `deploy_data.sh` had already learned
     this and braced its own; this had not.
  2. An install issued straight after an uninstall is refused while Android
     tears the old package down. It now waits and tries once more.
  3. `install ... | grep -q Success` threw adb's reason away and then advised
     the user to guess at signatures. It prints what the device actually
     said.
  4. ⭐ **The one that mattered:** install.sh handed `deploy_data.sh` its
     source paths in UNIX form (`/c/Users/...`). That script runs with
     `MSYS_NO_PATHCONV=1` and says in its own header that its sources must be
     Windows paths - so adb.exe could not open a single file and all 17
     failed at zero bytes. Now converted with `win_path()`.
  Made it visible in passing: `send()` in deploy_data.sh discarded adb's
  stderr, which is the only reason (4) took as long as it did to see.
  ✅ **A clean device now installs end to end and reaches the login screen.**
- **The cash-equipment crash.** Tapping equipped cash glasses killed the
  game. Confirmed from the tombstone, not guessed:
  `nl::bitmap::id()` <- `Texture::draw` <- `EquipTooltip::draw`. Two faults,
  both fixed. The window asked about the BASE slot (1) rather than the
  cosmetic's own (101), so it looked up an item that was not there; and every
  "nothing here" path in `set_equip` returned while leaving `invpos` set,
  which is the one thing `draw()` checks - so the tooltip armed itself and
  then drew out of members that had never been assigned.
- **The minimap can be dragged.** A held pointer that moves was being
  reported to the page as a HOVER - which is what lights up places on the
  world map - so the minimap, whose panning only runs while it believes a
  button is down, was told the button was up on every move. New
  `UIElement::send_drag`, offered before the hover; pages that ignore it
  behave as they did.
- **The minimap's portals sit on the portals.** A marker's half-width and the
  window's canvas corner are SCREEN distances and were being scaled with the
  map, and `MAX_ADJ` - the drop the largest window layout gives its canvas -
  was being added on a panel that draws no window, then scaled as well.
- **CENTRE centres.** The button was drawn at `position + box` and hit-tested
  at `box`, so it answered to presses about 24 by 40 pixels up and to the
  left of itself. `PANEL_LIFT` is 0 now too - the player rode 34px above the
  middle, which made a button called CENTRE visibly not centre.
- **Messages keeps the whole page again.** The keyboard used to be drawn over
  it, covering the lower half - the log it exists to show, and the SPEAK
  button. Messages reads; Chat types.
- The lower panel keyboard, full width on the panel's own parchment with a
  brown wash on the caps and dark letters. It was white-on-white at a tenth
  opacity over the live map, which over a noon sky was very nearly invisible.
  The paper is opaque on purpose: contrast must not depend on which map you
  are standing in.
- **Hotkeys are written to disk when they are placed.** `Setting::save` only
  changes a value in memory; `Configuration::save` writes the file, and the
  only thing calling it was Configuration's own destructor - which runs on a
  clean shutdown, and Android kills a game from outside. ⚠ **The selected
  CHARACTER and the window size are still lost the same way** - same one-line
  fault, not yet fixed.
- **Trade, first build.** `PLAYER_INTERACTION` (0x7B out, 0x13A in) both
  ways, a nine-a-side table on the panel, and no dragging: the window carries
  a strip of your own tradeable things and one tap puts one down. The
  handler that was there before read one byte, printed it, and dropped
  trades, shops, merchants and both minigames on the floor.
- **Assets: 82.4 MB -> 5.9 MB.** Three videos and eight page backgrounds
  gone; only the two LocalStory wordmarks are ours now, so `Map001.nx` is
  rebuildable by anyone with their own client - which is what the installer
  needed to exist.
- Keys page: every bindable action, what it is bound to, TO HOTKEY and
  CONTROLLER BIND wired to a real SDL capture.
- One `content_area()` that every page defers to.
- Level-up spending moved to a popup with thumb-sized buttons.
- Daily PvE working end to end.
- `Say.img` -> 2,392 generated quest scripts, plus the `QuestDialogue`
  fallback `NPCTalkHandler` never had.
- Quest navigation, the map/minimap pages, and deliberate map swipes.
- Enemy AI: pursuit, edge awareness, purposeful jumping.
- `tools/install.sh`, and `bootstrap.sh` so HOST always takes the newest
  staged build.

---

## HOW MANY MORE ARE THERE? (asked 2 Sep)

An honest estimate, from what tonight actually showed.

**The pattern that matters:** every single thing touched tonight had something
wrong with it. Roger, Nina, the item on the ground, the monsters, Robin's
quiz, the installer, the server staging. That is not bad luck - it is what a
**first playthrough** of code that has never been played looks like. The bugs
are not rare; they are simply undiscovered, and walking around finds them at
roughly the rate you can walk.

**Where the remaining ones are concentrated, worst first:**

1. **The 2,392 generated quest scripts.** Tested on perhaps five. The
   generator's own header admits it cannot express "a quest that damages you,
   hands you an item mid-sentence or branches on what you are carrying" - and
   tonight found two whole CLASSES it got wrong (quizzes, rewards) that were
   not on that list. Expect more classes, not more instances.
2. **Trade and Storage.** Built from the server spec in one pass and never
   run. Based on tonight's hit rate on code written that way, assume several
   each.
3. **The 213 quizzes.** A brand-new code path, run zero times.
4. **~30-40 player-facing silent returns** of the 213 counted by
   `tools/silent_returns.py`.
5. **21 NPCs that still say nothing**, per `tools/npc_audit.py`.

**The number, and why it is not comforting:** dozens more on the Maple Island
to Victoria Road stretch alone. But they arrive in a useful order - the ones a
player meets first are the ones found first - and they are getting *cheaper*
to fix, because the instruments now say what happened instead of nothing.
Roger cost a whole evening; the quiz cost twenty minutes because the data was
readable and the log spoke.

**The thing that would change the shape:** one deliberate playthrough of Maple
Island, start to finish, writing down everything odd WITHOUT stopping to fix
it. Tonight was a debugging session interrupted by discoveries; a survey would
find the same bugs in a tenth of the time and let them be fixed in batches.

---

## THE ONE BUG THIS PROJECT ACTUALLY HAS

Every fault found on 1-2 September was the same shape: **something did not
happen, and nothing said so.**

  * the installer died on line 65 - no message
  * the data push failed 17 times - adb's reason thrown away
  * the APK install was refused - reason thrown away
  * the new jar was not installed - timestamps compared, no message
  * the server did not restart - "already running", exit 0
  * 2,392 quest scripts were discarded unread - a gate, no message
  * 1,217 NPCs said nothing - a log line nobody read
  * pressing COMPLETE on a quest that was not ready did nothing whatever -
    no message, no log, no packet

Not one was a wrong calculation. Every one was a silence. "Be more careful"
does not fix that; four habits do, and three are now in the code:

  1. **Never discard what a tool said.** Keep stderr, print it on failure.
     (`install.sh`, `deploy_data.sh` - done.)
  2. **Compare content, not clocks.** (`bootstrap.sh` - done.)
  3. **Audits that end in a NUMBER** you can read in ten seconds.
     (`npc_audit.py`, `shop_audit.py`, `opcode_census.py` - done.)
  4. **A preflight that runs them all and REFUSES on a regression.**
     ⬅ NOT BUILT. This is the missing one, and it is the same idea as
     `sim/canon.py` in the board game: a gate that fails loudly rather than a
     document that asks you to remember. Proposed contents:
       - `npc_audit.py` - silent NPCs must not increase
       - `shop_audit.py` - undrawable shop items must not increase
       - the APK exists, and its version matches the release being cut
       - `install.sh --dry-run` against a real device
       - the staged jar on each device matches `target/Cosmic.jar` by CONTENT
     Ask before building: it is a release process, and that is the owner's
     call to make.

---

## Traps worth not falling into again

**Staged is not installed, and installed is not running.** A build sat on
the device for days while the push said `ok`; then it was installed while a
three-hour-old process kept serving, because `run.sh` refuses to start when
the port is taken and there was no way to restart. Both now impossible:
`bootstrap.sh` installs on every start and stops a server it has just
superseded.

**Make the server log readable from the PC.** Cosmic logs into Termux's
private home where adb cannot reach, so for weeks the server was a black box
and every diagnosis was a guess. Mirroring it to `/sdcard/Download/cosmic`
ended three separate hunts in one afternoon.

**Instrument before theorising, and instrument the case you are in.** The
first daily-hunt trace only spoke every fifth kill, so killing four monsters
produced silence - indistinguishable from the hook never firing. An
instrument that can be quiet for the case under investigation is not an
instrument.

**Read the field, do not assume the type.** `Map.nx` stores `life/id` as a
STRING; nlnx answers an integer conversion on a string node with zero, so
every NPC in the game was filed under id 0 and an index of 1,349 entries
could not find one of them. The `type` field beside it was already being
read as a string.
