# Running it on a Meta Quest

The Quest runs Android, so the same APK installs and works. Nothing here is a
Quest build — there is one build, and the parts that do not apply switch
themselves off.

Tested on a **Quest 3S**, Android 14, arm64-v8a.

## It is the same APK

The second screen is an AYN Thor feature and looks for a display in
`DISPLAY_CATEGORY_PRESENTATION`. The Quest has none — its Oculus panel layers
are `FLAG_PRIVATE` and owned by other apps, so they are never offered — and
the client logs `no second display` and carries on as an ordinary
single-screen game.

## Where the app is

**Sideloaded 2D apps are hidden from the main library.** In the headset open
**App Library**, then change the filter in the top-right from *All* to
**Unknown Sources**. It is listed as **MapleStory** — that is the activity's
label, and it is not the same as the application label, which is
`HeavenClient`. Renaming the app means changing both.

## THE THUMBSTICK IS READABLE - IT ARRIVES AS SCROLL

**This appears to be undocumented anywhere.** Meta's own forums carry threads
titled "Sideloaded 2D apps don't register input from Q3 controllers" and "How
to take controller input in a 2d android app?", and every guide points at
Unity or Unreal reading the sticks through OpenXR - which requires being a VR
app in the first place. Nobody mentions this.

A 2D app is never offered the controllers as a joystick. Checked at every
level, and all three say no:

  * SDL enumerates one controller marked `Enabled: false` and the accelerometer
  * `InputDevice.getDeviceIds()` shows the same single gamepad
  * the Activity sees the controllers as two independent TOUCHSCREEN pointers,
    one device id per hand

But Meta had to pick some flat-screen behaviour for the stick, and what they
chose is **scroll**. That is an ordinary Android motion axis and it reaches
`dispatchGenericMotionEvent` perfectly well:

    MOTION src=0x2 dev=-1 vscroll=0.173 hscroll=0.139

Roughly -0.27 to +0.27 on both axes. So:

    float h = event.getAxisValue(MotionEvent.AXIS_HSCROLL);
    float v = event.getAxisValue(MotionEvent.AXIS_VSCROLL);

turned into `KEYCODE_DPAD_*` and handed to `SDLActivity.onNativeKeyDown`.
That route was proven separately first - `adb shell input keyevent 21` walks
the character - so only the reading was new.

### Releasing the stick sends NOTHING

Scroll is a stream of deltas, not a position. A joystick axis reports its way
back to zero, so letting go is just another event; scroll simply STOPS, and
nothing ever says "no longer scrolling". The character kept walking with no
event left to lift the key.

A 120ms idle timeout stands in for the release - comfortably longer than the
~11ms between events while the stick is held, so it cannot fire mid-movement.

### ⏳ BUILT AND INSTALLED, NOT YET PLAYED (28 Aug)

The mapping below is implemented in `HeavenClientActivity.java` and installed on
the Quest, but nobody has played with it yet. Three things to confirm:

  * the four buttons do the right four things
  * nothing is CROSSED - if jump attacks and sit picks up, the left/right guess
    from the device id came out backwards. `adb shell touch
    /sdcard/Android/data/org.heavenclient.android/files/swap_hands`, restart the
    app. No rebuild.
  * holding attack stays held, and the 300ms silent-controller watchdog never
    fires in play

### The buttons, and exactly how many there are

"Only `KEYCODE_BACK` was ever seen" was an artefact of the probe, not a fact
about the headset: that probe filtered out `SOURCE_MOUSE`, which is precisely
where a controller button arrives. An unfiltered probe - every key, every
motion, decoded button state, decoded source, device id, scan code, and all 27
axes that carry a value - found **six** distinguishable signals.

`dev=1048578` is the RIGHT controller, `dev=1048577` the LEFT.

| Control            | What the app receives                    |
|--------------------|------------------------------------------|
| A                  | `BUTTON_PRIMARY` on right                |
| right trigger      | `BUTTON_PRIMARY` on right - *same as A*  |
| X                  | `BUTTON_PRIMARY` on left                 |
| left trigger       | `BUTTON_PRIMARY` on left - *same as X*   |
| B                  | `BUTTON_BACK` + `KEYCODE_BACK` on right  |
| Y                  | `BUTTON_BACK` + `KEYCODE_BACK` on left   |
| right stick click  | `BUTTON_TERTIARY` on right               |
| left stick click   | `BUTTON_TERTIARY` on left                |
| both grips         | nothing                                  |
| Menu               | nothing - the system takes it            |

`BUTTON_PRIMARY` is the laser's mouse click. Repurposing it costs the pointer,
which is how every window, NPC and inventory in the game is operated - so the
four signals genuinely free for gameplay are **B, Y, and the two stick clicks**.

Both thumbsticks scroll, and both arrive on `dev=-1`, a single merged virtual
device. **The two sticks cannot be told apart** - only their clicks can.

## ⚠ WHY A AND THE TRIGGER CANNOT BE SEPARATED - AND WHY THAT IS FINAL

The short version: **the events are not coming from the controllers**. They are
manufactured for us, and the distinction is discarded before the event exists.

  * The device ids - `1048577`, `1048578`, `1048580` - are `0x100001` upward,
    Android's VIRTUAL range. Real hardware gets small ids.
  * Horizon OS runs `oculus.internal.virtual_input.IInputDataInjection`.
  * vrshell reads the controllers through OpenXR and injects synthetic
    TOUCHSCREEN events, applying its own policy on the way in: trigger or
    A -> click, B/Y -> back, stick click -> middle, grips and Menu -> discarded.

Nothing is left in the event to inspect. Checked and identical for A and the
trigger: source, tool type, pointer count, pressure, size, generic axes. Press
durations differ only by the hand (168ms vs 255ms).

### The real gamepad exists, and Android refuses to dispatch it

`/dev/input/event4` is a genuine, complete gamepad, and `adb shell` (group
`1004(input)`) can read it. Pressing buttons produces, at the kernel:

    BTN_GAMEPAD          <- A
    BTN_EAST             <- B
    BTN_NORTH            <- X
    BTN_WEST             <- Y
    BTN_TR + KEY_RIGHTSHIFT   <- right grip   (produces NOTHING in the app)
    BTN_SELECT           <- Menu              (produces NOTHING in the app)
    ABS_RZ 0x000..0x3ff  <- right trigger, ANALOG, 1024 steps
    ABS_X/Y, ABS_RX/RY   <- both sticks, as real axes

Every distinction wanted is right there. Android sees the device, loads its key
layout (`/odm/usr/keylayout/oculus-device.kl`), and marks it:

    Classes: KEYBOARD | GAMEPAD | JOYSTICK | EXTERNAL
    Enabled: false

That flag is set through `InputManagerService.setInputDeviceEnabled()`, which
requires `android.permission.DISABLE_INPUT_DEVICE` - signature-level. No adb
command grants it. It needs a system app or root.

### `controller_emulation_mode` - tried, and it does not do it

`settings get secure controller_emulation_mode` is `0` by default and is
plainly named for this. It was tried properly:

  * Values 1, 2 and 3 with the app force-restarted: no change. The flag is read
    at BOOT, not per app.
  * **Rebooted at mode 1, and it genuinely changed the device topology** - two
    new nodes appeared (`event3`, `event5`) with a class not otherwise seen,
    `INPUT_DEVICE_CLASS_VR_PERIPHERAL`, both `Enabled: true`.
  * But Meta wired it the unhelpful way round. The new enabled nodes are nearly
    silent (25 and 1 events against `event4`'s 349), and **every face button
    still comes out of `event4`, which stays `Enabled: false`.**
  * Rebooted at mode 2: byte-for-byte the same arrangement.
  * The app received nothing either time - no gamepad source, no small device
    id, just the same four injected `KEYCODE_BACK`s.

Restored to `0` afterwards; the phantom devices go away at the next restart.

**Do not re-run this investigation.** The ceiling is the six signals in the
table above. Tap-versus-hold is pure software and doubles them to eight if more
are needed. If full controls ever matter more than convenience, **pair a
Bluetooth gamepad** - Horizon OS enables those normally, and `open_gamepad()`
in `IO/Window_Android.cpp` already opens every pad it finds.

### Two smaller findings from the same session

  * `adb shell monkey -p org.heavenclient.android -c LAUNCHER 1` reports success
    but does **not** bring the app up in the headset - it still has to be opened
    by hand from the library. Do not use it to save someone a step.
  * The cut-off screen has a number now: the app logs
    `blit: scene 800x600 -> screen 1280x800`. The game draws 4:3 into a 16:10
    panel. That is a letterboxing problem, not a rotation one.

## ⚠ Keys reach the app fine - it was never a focus problem

`mCurrentFocus=null` and `mFocusedApp=null` on this headset, because
`com.oculus.vrshell/FocusPlaceholderActivity` holds window focus. That looks
damning and is a red herring: `adb shell input keyevent 21` walks the
character regardless. The app receives keys perfectly well.

Three theories were spent before this was measured - a permission problem, a
missing SDL mapping, and the client opening the sleeping duplicate of two
controller entries. Each was measured with the headset idle on a desk, where
focus is null anyway, so every reading agreed with every theory. **Measure in
the state that matters, and test the last mile first** - injecting a keypress
took thirty seconds and would have ruled out three days of theory.

## The old conclusion, kept because it was WRONG

Measured while wearing it, in the game, pressing buttons:

    dumpsys window   mCurrentFocus=null   mFocusedApp=null
    dumpsys input    FocusedWindows: (empty)

Android delivers **key and gamepad** events only to the focused window. Touch
goes to whatever sits under the pointer regardless of focus - which is exactly
the split seen here: the laser works and nothing else does.

Horizon OS runs 2D apps in a volumetric window and keeps its own idea of focus
(`VolumetricContentMonitor` reports the activity as focused). The classic
WindowManager focus stays null, so the input dispatcher has nowhere to send a
keypress and drops it.

**The hardware is not the problem.** `getevent` on the bluetooth pad shows
`ABS_X` sweeping its whole range and buttons reporting cleanly. The events
reach the kernel and stop there.

Three wrong theories were spent on this before it was measured properly - a
permission problem, a missing SDL mapping, and the client opening the wrong
pad of the two. Each was measured with the headset sitting idle on a desk,
where focus is null anyway, so every measurement agreed with every theory.
**Take the measurement in the state that matters.**

So the Quest needs either on-screen controls the pointer can press, or a
native VR build. It is not something client input code can fix.

## Controls

The headset has no keyboard and no free hand, so the controls are not the
handheld's.

- **The pointer** works anywhere the game expects a mouse. Quest reports it as
  touch, which the UI already consumes.
- **The left thumbstick walks.** Reported as arrow keys, because `Keyboard`
  already binds those to LEFT/RIGHT/UP/DOWN — the stick arrives as something
  the game has always understood. Up and down are included: up enters a portal
  and climbs a rope, down drops through a platform.
- **The face buttons answer an open window**, and only while one is open:

  | Button | Means | Plain box | Two pages | Yes/no | Quest accept |
  |---|---|---|---|---|---|
  | A | confirm | OK | Next | Yes | Accept |
  | B | back | ends it | **Previous** | ends it | ends it |
  | X | deny | ends it | ends it | **No** | **Decline** |
  | Y | close | End Chat | End Chat | End Chat | End Chat |

  The moment the last window closes they revert to whatever the player mapped.
  Where *back* or *deny* has no meaning — no previous page, nothing to decline
  — the conversation ends rather than sending an answer the server is not
  waiting for.

Everything else is the ordinary gamepad mapping in `Setting<Joystick_*>`.

## Logging in

`AUTOMATIC_REGISTER` is on in Cosmic's config, so **an unknown username
creates an account with whatever password is typed**. There is nothing to set
up in advance. Passwords need five characters or more; a shorter one is
rejected before it is sent and looks exactly like a wrong password.

The account is created on **whichever server is joined**, so it belongs to
that device's world.

## What went wrong getting it there

Worth writing down, because every one of these looked like something else.

### The data folder read as empty when it was not

`adb shell ls /sdcard/Android/data/...` returns nothing on Android 13+ whether
the folder is empty or unreadable, and the two are indistinguishable. The
Quest already held game data from a previous session; it was read as a fresh
device and the whole transfer was ordered again. Harmless — `deploy_data.sh`
checks each file by size and skips what matches — but the conclusion was
wrong.

**Check with `du -sh` on the parent instead**, which reports a size even when
the listing is refused.

### adb was not on PATH, and the script blamed the headset

`deploy_data.sh` opened with a reachability test, and when `adb` could not be
found at all it printed *"Cannot reach &lt;serial&gt;. Is USB debugging
authorised on the device?"* — an answer pointing firmly at the wrong thing.
Fixed: it locates the SDK itself, and a genuinely unreachable device now says
so separately and lists what adb can see.

### The fonts silently did not copy

The important one. **adb cannot create a directory under
`/sdcard/Android/data` on Android 11+** — `remote secure_mkdirs failed:
Operation not permitted` — though it can write files into one that already
exists.

The `.nx` files go straight into `HeavenClient/` and landed fine. The fonts go
one level deeper, into `fonts/Roboto/`, which did not exist. And the push was
wrapped in `>/dev/null 2>&1`, so it failed without a word.

The result would have been a complete-looking 4.5 GB install **whose text
never appeared** — which is exactly what the comment two lines above the push
warns about. The script warned about the thing it was doing.

The shell can create the directory even where adb cannot, so `deploy_data.sh`
now does that first and pushes the files rather than the folder.

### "Missing nx file" for 4.5 GB of files that were all present

The last and worst of them. adb writes the data as the **shell** user, into a
folder adb itself created with `drwxrws---` - no world permissions at all. The
game runs as a different user, so it could not traverse into the folder, could
not open a single file, and reported every one of them missing.

    chmod -R a+rX /sdcard/Android/data/org.heavenclient.android/files

`deploy_data.sh` now does this over the whole tree rather than the fonts
alone, and the client says *"present but CANNOT BE READ"* instead of
*"Missing nx file"*, with the command in the message.

### Four failures, four wrong causes

Worth stating plainly, because it is the argument for the installer work:

| What it said | What was true |
|---|---|
| "Cannot reach &lt;serial&gt;. Is USB debugging authorised?" | adb was not on PATH |
| *nothing* | `$USER` unset under `set -u` |
| *nothing* | adb cannot create directories there; fonts never copied |
| "Missing nx file" | 4.5 GB present, owned by shell, unreadable |

Not one of them was the device. Two said nothing; two pointed confidently at
the wrong thing. A stranger would have hit all four with no logcat and no
reason to suspect permissions.

## Still unknown

- Whether it is **playable** rather than merely running. A platformer through a
  floating panel with a gamepad ought to work, but nobody has played it.
- **Wi-Fi Direct.** The Quest's support is untested; joining over ordinary wifi
  is the safe path.
- **Storage.** 4.5 GB of data on a drive that was already 88% full.
- **The panel is 16:10 and the game renders 16:9.** The blit log reads
  `scene 1280x720 -> screen 1280x800`, so the picture is stretched about 11%
  vertically. Letterboxing would fix it; nobody has judged whether it is
  noticeable.
