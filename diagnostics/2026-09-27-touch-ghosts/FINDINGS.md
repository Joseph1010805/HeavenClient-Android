# The Thor's pointer goes mad — 27 September 2026

Unfinished. The device was restarted after this was written; if the fault is
gone, the cause was the hardware or its state, and this file is the record of
what was measured. If it came back, start from "What is proven" and do not
re-derive it.

## The symptom

The cursor is thrown around the screen, clicks land where they were not aimed,
NPC windows close by themselves. Worst in game, **never on the login screens**.
Reproducible on demand by **minimising the app and restoring it**, and seen
outside the game too. Present on a fresh Explorer character as well as the
Cygnus one, so it is not character or map state.

Reported as new: "after everything was tested this never happened until now."

## What is proven

**1. The digitizer reports contacts nobody made.** Raw `getevent` from
`/dev/input/event6` (`fts_ts`, the main panel), while a stylus and nothing else
was on the glass:

    25287.137  2 contacts: slot0 id638 (99,984)   slot1 id637 (0,703)
    25287.147  3 contacts: slot0 id638 (99,984)   slot1 id637 (0,702)   slot2 id639 (0,1233)
    25287.189  2 contacts:                        slot1 id637 (0,685)   slot2 id639 (0,1233)
    25287.304  2 contacts:                        slot1 id637 (0,666)   slot2 id639 (0,1233)

Up to **four** simultaneous contacts, slots 0-3. Two of them sit at **x = 0** -
the outermost column of the panel - and drift slowly in y (703 -> 666) without
ever lifting. The panel is 1080x1920 in its native portrait; held in landscape,
x=0 is the edge a hand wraps around.

Nothing in the client can manufacture these. They arrive with real
`ABS_MT_TRACKING_ID`s from the kernel driver.

**2. The client hands the single cursor to whichever contact spoke last.**
`IO/Window_Android.cpp`, the `SDL_FINGERDOWN`/`SDL_FINGERMOTION` case, never
looks at `ev.tfinger.fingerId`. With a ghost alive beside a real touch, the
pointer is thrown between them at frame rate. Logged from the client:

    Cursor jump: 292,370 -> 566,370  raw 0.708485,0.617021  finger 3 on device 7 DOWN
    Cursor jump: 566,370 -> 234,370  raw 0.292956,0.617021  finger 1 on device 7 MOVE
    Cursor jump: 236,370 -> 546,370  raw 0.683671,0.617021  finger 3 on device 7 MOVE
    Cursor jump: 550,370 -> 251,370  raw 0.313994,0.617021  finger 1 on device 7 MOVE

16 ms apart, same y, alternating x.

> The two x fractions add up to 1.000, which looked like a mirrored ghost and
> is not. The two edge contacts happen to sit either side of the panel's
> midpoint (960) at nearly equal distances - 703 and 1233 - so their positions
> in the game's space happen to straddle the centre. A coincidence that cost
> an hour.

**3. It is NOT the client's fault, on the evidence so far.**
`IO/Window_Android.cpp` was reverted to its pre-session state, rebuilt and
installed. **The glitch remained.** So nothing changed in that file tonight
caused it.

## What was eliminated

| Theory | Killed by |
|---|---|
| Two thumbs on the glass | The user uses a stylus, nothing else touching |
| A second touchscreen (`fts_ts_3`) leaking in | Client log: every finger arrived on touch device 7, one device |
| `ODIN Station Virtual Mouse` | Same; no `SDL_MOUSE_TOUCHID` (-1) ever seen |
| Controller stick drift | 6 s of raw `getevent` on the pad produced nothing; all four axes read 0 |
| A stale letterbox rectangle in `scene_point()` | Logged `fit 0,0 1920x1080`, correct, and it clamps to the viewport anyway |
| My own instrumentation / pointer-ownership change | Reverted the whole file; glitch remained |

A separate, real bug was found on the way and is NOT this one: a stuck
synthetic `RIGHT` key from `handle_stick` flooding movement packets, which
closes NPC windows by walking the character away. See "Still open" below.

## How to measure it again

**Raw contacts, decoupled from timing** - runs until killed, so there is no
window to hit:

    adb shell "nohup sh -c 'getevent -lt > /sdcard/Download/touch.log 2>&1' >/dev/null 2>&1 &"
    # ... reproduce ...
    adb shell pkill getevent
    adb pull /sdcard/Download/touch.log

Count concurrent contacts:

    grep ABS_MT_TRACKING_ID touch.log | awk '{print $NF}' \
      | awk '{if($1=="ffffffff") d--; else d++; if(d>m) m=d} END {print "max =", m+0}'

**Android's own overlay, which takes the app out of it entirely** - draws every
contact the system sees, in every app:

    adb shell settings put system pointer_location 1
    adb shell settings put system show_touches 1
    # off again with 0

⚠ `getevent` needs no root (shell is in the `input` group) but produces nothing
if nobody is touching the screen - two captures were wasted on that.

## Still open

- **Is it the hardware?** A screen protector lifting at the bezel, a case
  pressing the border, the Station dock, charging, damp or cold - all produce
  edge ghosts. Check with `pointer_location` on the home screen, in no app.
- **If it is real and permanent**, the client's answer is to let only ONE
  contact drive the cursor and to ignore contacts on the extreme edge. A first
  attempt at the ownership half is in `reverted-instrumentation.patch` - note
  that "newest press wins" can let a ghost hold the pointer, and the edge
  rejection was never written.
- **`Expression::byaction()` subtracts 98**, so key FACE1 gives HIT and FACE7
  asks for STUNNED, a cash emote the server refuses. Should be 99.
- **Keyboard emotes send no packet** - `Stage::send_key` changes your own face
  and nobody else sees it.
- **`handle_stick` latches.** It acts only on axis EVENTS and never re-reads
  the stick, so a missed "returned to centre" holds a direction down for the
  rest of the session. Polling the axes each frame would fix it.

## Neinheart's quest (20016) - not a quest bug

Character Joseph (id 21), level 7, `queststatus` 20016 = **1**, started and not
finished. `Check.img/20016/1` is empty: no items, no mobs, no level. The quest
is driven entirely by `scripts/quest/20016.js`, which is present on the device
and md5-identical to the PC's copy, and which ends in `qm.forceCompleteQuest()`
at step 12.

Getting there needs **thirteen presses of Next and one Accept**. Any stray
click does `status--` and walks the conversation backwards; Decline at step 8
closes it with "Talk to me again". With the pointer throwing clicks around,
that conversation is unwinnable - which matches the packet trace exactly:
opcode 60 out, 304 back, repeatedly, without ever reaching the end.

So this is expected to fix itself the moment the pointer behaves. If it does
not, then look at the handler.

## The files here

| File | What it is |
|---|---|
| `getevent-raw.log` | 284 KB of raw kernel input events, all devices, during the fault |
| `screen-recording-60s.mp4` | One minute of the screen during a reproduction |
| `dumpsys-input.txt` | The device's input devices - two touchscreens, a pad, a virtual mouse |
| `reverted-instrumentation.patch` | The client-side logging and pointer-ownership attempt, reverted |
