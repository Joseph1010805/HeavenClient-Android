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

## ⚠ A 2D app on this headset gets the POINTER AND NOTHING ELSE

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
