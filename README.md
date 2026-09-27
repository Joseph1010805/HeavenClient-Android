# LocalStory

MapleStory on your phone or handheld. This is an Android port of
[HeavenClient](https://github.com/ryantpayton/MapleStory-Client), an open-source
client written from scratch, and it plays against a v83 private server like
[Cosmic](https://github.com/P0nk/Cosmic) or HeavenMS.

I built it to play with my sons over our home WiFi on an AYN Thor. It works:
login, character creation, quests, combat, the lot.

It has since grown a second half. The app can **run the server itself**, so a
handheld can host a game with no PC, no router and no internet at all - one
device becomes the network and the others join it by name. See
[Playing together](#playing-together).

**You supply your own game files.** There are none in this repository and I
can't give you any - see [About the game files](#about-the-game-files).

---

## Quick start

Already have MapleStory v83 `.nx` files? This is the whole thing:

| | |
|---|---|
| **1. Get this** | [**Download the installer**](../../releases/latest) - the `LocalStory-installer-*.zip` on that page |
| **2. Unzip it** | Anywhere. Your Desktop is fine. |
| **3. Put your game files here** | `C:\Users\<you>\maple\wz-v83\` - and `UI.nx` on its own in `C:\Users\<you>\maple\wz-v178\` |
| **4. Plug the handheld in** | USB cable that carries data, with [USB debugging](#how-to-turn-on-usb-debugging) turned on |
| **5. Run the installer** | Double-click **`INSTALL.bat`**, confirm it named the right device, wait |
| **6. Finished** | Open **LocalStory** on the device and tap **CREATE A GAME** |

That is it. No accounts, no server address, nothing to type.

**Do not have the game files yet, or not sure what `.nx` means?**
[Getting it running](#getting-it-running) below is the same six steps with
every detail filled in.

---

## Getting it running

Six steps. Most of the work is step 2, and it is a one-off.

### What you need before you start

- **A Windows PC and a USB cable that carries DATA.** A charge-only cable shows
  up as nothing at all and is the usual reason a device "isn't found".
- **An Android device**, arm64. Built for the AYN Thor; it runs on others.
- **A MapleStory v83 client of your own.** The game data is Nexon's. It is not
  in this repository, it is not in the APK, and the installer will never
  download it - see [About the game files](#about-the-game-files) below.
- **[Git for Windows](https://git-scm.com/download/win)**, installed with the
  default options. The installer uses the bash that comes with it.

### 1. Get this repository

Green **Code** button at the top of this page, then **Download ZIP**, and unzip
it somewhere. You only need `INSTALL.bat` and the `tools/` folder - the rest is
source code and you do not have to build anything.

> Already looking at the [releases page](../../releases)?
> `LocalStory-installer-*.zip` is the same two things and smaller.

### 2. Convert your game files from .wz to .nx

The client reads `.nx`. Your MapleStory client has `.wz`. Converting is a
one-off with [NoLifeWzToNx](https://github.com/ryantpayton/NoLifeWzToNx):

```
NoLifeWzToNx.exe -c Character.wz
```

**The `-c` matters.** Without it you get server-format files the client cannot
read. Do that for all 15 files listed in `Util/NxFiles.h`:

```
Base  Character  Effect  Etc  Item  Map  Mob  Morph
Npc   Quest      Reactor  Skill  Sound  String  TamingMob
```

WARNING: **`UI.nx` is the exception, and it catches everybody out.** It must
come from a LATER client - v178 is what this project uses. The v83 interface is
too old: the client looks for menus that did not exist yet and refuses to
start. So you need two clients, and exactly one file from the second.

<details>
<summary>NoLifeWzToNx will not compile on a modern Visual Studio - three fixes</summary>

There is no download; it is a Visual Studio project you build yourself, and it
has aged. In order:

- It uses `std::experimental::filesystem`, which no longer exists. Change it to
  `std::filesystem` and set the project to C++17.
- That trips a deprecation warning on `<codecvt>`, and warnings are errors.
  Define `_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS`.
- The bundled `libsquish.lib` is too old to link and fails with `C1047`. The
  source is in the same folder - add `includes/libsquish/*.cpp` to the project
  and drop the `.lib`.

</details>

### 3. Put the files where the installer looks

```
C:\Users\<you>\maple\wz-v83\     <- the 14 .nx files converted from v83
C:\Users\<you>\maple\wz-v178\    <- UI.nx ONLY, from the later client
```

It also finds `Documents\maple\wz-v83`, `C:\maple\wz-v83`, `C:\Nexon\MapleStory`
and both `Wizet\MapleStory` folders under Program Files, so an untouched install
is found where it already sits.

Anywhere else, point at it:

```
INSTALL.bat --data D:\somewhere\wz-v83
```

### 4. Turn on USB debugging

Nothing works without this, and the symptom is a device the installer cannot
see at all - which looks exactly like a broken cable.

**[Don't know how? Open this.](#how-to-turn-on-usb-debugging)**

The short version: **Settings, About, tap "Build number" seven times**, then
**Developer options, USB debugging, on**. Then plug in and say yes to the
prompt that appears on the handheld's own screen.

### 5. Plug it in and double-click `INSTALL.bat`

That is the whole install. It names the device and asks you to confirm it, puts
the app on, finds your game files, copies them across, and sets the device up to
host as well as play:

```
  Checking adb ................. ok
  Looking for a device ......... found

Device found: AYN Thor  (Android 13, arm64-v8a)
  Install to AYN Thor? [Y/n]
```

Several gigabytes go over USB the first time - ten minutes or so. It is safe to
re-run: every file is checked against the device first, so an interrupted copy
picks up where it stopped rather than starting again.

On Mac or Linux there is no `.bat`; run `tools/install.sh` from the same folder.

### 6. Play

Open **LocalStory** on the device. Somebody has to be hosting for there to be a
game to join, and the login screen finds one by itself - there is no address to
type. No game yet? Tap **CREATE A GAME**, pick six digits, and read them out to
whoever is joining.

---

### How to turn on USB debugging

It is hidden on every Android device, in the same way on nearly all of them.

**1. Unlock the developer options.**
Open **Settings**, then **About phone** (or **About tablet**, or **About
device**). Find **Build number** and **tap it seven times**. It counts down at
you - "you are now 3 steps away from being a developer" - and finishes with
"You are now a developer!". You may have to enter your PIN.

> On some devices Build number is one level deeper, under
> **About phone, Software information**. On an AYN Thor it is directly under
> About.

**2. Turn USB debugging on.**
Go back to **Settings**, then **System**, then **Developer options** - a new
entry that was not there a minute ago. Scroll to **USB debugging** and switch
it on. Confirm the warning.

**3. Plug it into the PC and say yes on the HANDHELD.**
The first time a particular computer connects, the device asks - on its own
screen, not the PC's:

> **Allow USB debugging?**
> The computer's RSA key fingerprint is...

Tick **Always allow from this computer**, then **Allow**. Miss this and the
installer reports the device as `not allowed` rather than missing, which is at
least an honest difference.

**If no prompt appears at all**, swipe down on the handheld, tap the USB
notification ("Charging this device via USB") and change it to **File
transfer**. Some devices will not talk over a cable that is only charging.

**Still nothing?** It is almost always the cable. A charge-only cable has no
data wires, looks identical, and produces no error whatsoever - just silence.
Try the cable the device came with, or one you know copies files.

---

### When it does not work

The installer says what went wrong rather than just failing. The three you are
most likely to meet:

| It says | It means |
|---|---|
| `Looking for a device ... none found` | Charge-only cable, or USB debugging is off. The cable is the usual one. |
| `No game data found` | Step 3 - the `.nx` files are not in a folder it searches. Use `--data`. |
| `UI.nx was not found` | Step 2 - that one file has to come from a v178 client, not v83. |

Everything else it prints is the device's own words rather than a guess. If the
game itself misbehaves once installed, see
[Something went wrong? Send a report](#something-went-wrong-send-a-report).

---

### About the game files

The `.nx` data is converted from Nexon's `.wz` files. It is Nexon's work, so:

- it is not in this repository (`*.nx` is in `.gitignore`)
- it is not in the APK
- the installer will never download it, and this project will not point you at
  somebody else's copy - that is distributing Nexon's work either way

You need a client of your own. That caps who can install this at people who
already have one, and that is understood and accepted.

There is also an optional 16th file, `Map001.nx`, holding custom artwork for the
login, world select, character select and character creation screens. It is not
converted from anything - `tools/make_assets.py` builds it from your own video
and images, and CHANGES.md explains how. Put it beside the `wz-v83` folder.
Without it those screens fall back to the stock artwork.


### If you'd rather do it by hand

There's an APK on the
[releases page](https://github.com/Joseph1010805/HeavenClient-Android/releases).
It's the app and nothing else - no game data, for the reason above - so you
still need everything in the previous section before it will start.

It's arm64 only. Every Android handheld and phone made in the last several
years is arm64, but an old 32-bit tablet or an x86 emulator won't install it,
and Android says "app not installed" without saying why. Android 7 or newer.

You'll also have to let your device install it. It isn't from the Play Store, so
Android blocks it the first time and offers you a settings screen - allow this
source and press install again.

Where the files go by hand is under
[Putting the files on your device](#putting-the-files-on-your-device).

If you'd rather build it yourself, that's the next section.

## Something went wrong? Send a report

There's a **Report** page in the game - lower screen, Settings → Report.

Choose whether the picture should be of the **game** or the **panel**, press
**SAVE A REPORT**, and the game writes a file, takes a screenshot, and offers
to send them. Pick any app you like. Then open an
[issue](https://github.com/Joseph1010805/HeavenClient-Android/issues/new/choose)
and drag the files in.

The report holds the build, your device, which map and page you were on, your
character's stats and gear, and the game's own error notices. It does **not**
hold your password, your account name, your chat, or anybody else's messages -
the file says so in its own first lines, so you can check before you send it.

## What works, and what doesn't

Worth knowing before you spend an evening on it. None of the missing things
crash - they just quietly never happen, which is harder to diagnose than a
crash if you don't know to expect it.

**Works:** moving, jumping, ladders and ropes, combat, dying, loot, the
inventory, equipping, skills and spending points, shops, NPC conversation,
levelling, standing HP and MP recovery, the minimap and the world map, the
cash shop (browsing, buying, taking out, wearing), hosting or joining a game
from the login screen, and the AYN Thor's second screen.

**Doesn't, yet:**

- **Parties are unproven, not unbuilt.** Every message Cosmic sends about a
  party is handled - invitation, creation, joining, leaving, being expelled,
  disbanding, the leader changing, and the status messages. The member HP bars
  were the one real gap and are now wired up. What has never happened is two
  people actually forming a party at a table, so treat it as untested rather
  than working.
- **Quest completion.** Quests can be started and turned in, but the client
  ignores the packet that says one finished, so nothing tells you it did.
- Pets, summons, mage doors and mist skills.
- Other players' skill effects and buffs - you see them move and attack, but
  not what they cast.
- Chairs, including chair healing.

`docs_QUEST.md` covers running it on a Meta Quest - it is the same APK, but
the controls and the sideloading differ, and the setup has three traps in it.

`docs_FIXES.md` is a running list of what has been fixed and *why*, newest
first. Kept because the same shapes keep coming round: state left behind when
a screen changes, a value read but never used, a doc that stopped being true,
and an error hidden behind `>/dev/null`.

`docs_PACKET_GAP.md` is the full list: every message the server can send that
this client currently ignores, generated by diffing Cosmic's `SendOpcode`
against the handlers here. It's the checklist I'm working through.

## Building it

You'll need Android Studio for the SDK, NDK r27, and **JDK 17** - Gradle rejects
JDK 21, which catches most people out.

Clone it with the submodules, or you'll get a pile of confusing CMake errors:

```
git clone --recursive https://github.com/Joseph1010805/HeavenClient-Android.git
```

Tell Gradle where your SDK is by creating `android/local.properties`:

```
sdk.dir=C:/Users/YourName/AppData/Local/Android/Sdk
```

Use forward slashes even on Windows. Backslashes get eaten as escape characters
and Gradle will tell you the SDK is missing from a path you can see is right.

Then build and install:

```
cd android
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Run it once and let it complain about missing data. That first run creates the
folder your files go in, with the right permissions.

## ⚠ Upgrading from a build you made yourself

**Read this before installing a release over a hand-built APK, or you will lose
the game data.**

Android identifies an app by its signature and refuses to install an update
signed by a different key. Everything before v0.7 was debug-signed; releases
are signed with the real key. So the first release has to go on over an
*uninstall* - and the game data lives in
`/sdcard/Android/data/org.heavenclient.android/files/`, which **Android deletes
along with the app.** That's several GB of NX you'd have to push again.

Move it aside first. It's the same volume, so this is an instant rename rather
than a copy:

```bash
DIR=/sdcard/Android/data/org.heavenclient.android/files/HeavenClient

adb shell mv $DIR /sdcard/HeavenClient-keep
adb uninstall org.heavenclient.android
adb install LocalStory-0.7.apk
adb shell mkdir -p $(dirname $DIR)
adb shell mv /sdcard/HeavenClient-keep $DIR
```

Only needed once. Every later release is signed with the same key and installs
straight over the top.

A fresh device needs none of this - there's nothing to preserve.

## Cutting a release

Pushing a `v*` tag builds an APK and opens a draft release with it attached:

```
git tag v0.7
git push upstream-mine v0.7
```

Only `v*` triggers it - the repo also carries `latest` and `known-good-*` tags
that mark states worth returning to, and those must not produce releases. You
can also run the workflow by hand from the Actions tab to prove a build without
spending a version number on it.

The version comes from the tag, so a release can't claim a version that
disagrees with what it was built from. `versionCode` is the commit count, which
is monotonic - an older tag can never outrank a newer one.

**Signing.** Run `tools/make_release_key.sh` once. It makes a keystore, then
prints the four values to paste into *Settings → Secrets and variables →
Actions*. Until those secrets exist, tagged builds still work; they just come
out debug-signed, and the release notes say so.

> ⚠ **Back the keystore up somewhere that outlives the machine.** Android
> identifies an app by its signature. Lose that file and you can never ship an
> update that installs over an existing copy - there's no recovery and nobody to
> appeal to. It's deliberately not in the repo (`*.jks` is ignored), because
> anyone holding it can publish an app Android believes is yours.

Nothing Nexon owns is in the repository or in the APK - the client reads its
data from the device at runtime - so CI builds the app without any game files.
Keep it that way.

## Putting the files on your device

They go here:

```
/sdcard/Android/data/org.heavenclient.android/files/HeavenClient/
```

You can't `adb push` straight into that folder - Android won't allow it. Copy
to somewhere else on the device first, then move them across:

```bash
DIR=/sdcard/Android/data/org.heavenclient.android/files/HeavenClient

for f in *.nx; do
    adb push "$f" /sdcard/Download/
    adb shell "mv /sdcard/Download/$f $DIR/ && chmod 644 $DIR/$f"
done
```

It's about 4 GB in total, so give it a few minutes.

The `chmod` isn't optional. Files moved this way are owned by the shell user and
the game can't read them without it.

Copy the `fonts` folder across the same way. If you forget it, text simply
doesn't appear - no error, no warning.

One thing to watch: `adb push` can print `1 file pushed` and then fail on the
next line, leaving nothing on the device. Check with `ls` rather than trusting
what it says.

On Windows this is usually Git Bash rewriting the device path - it turns
`/sdcard/...` into `C:/Program Files/Git/sdcard/...` before adb ever sees it.
Set `MSYS_NO_PATHCONV=1`, and then give local files in Windows form
(`C:/maple/Base.nx`) and device files in Unix form (`/sdcard/...`). Don't set
it globally: it breaks the Gradle wrapper.

`tools/deploy_data.sh` does all of this, checks every file's size on the
device afterwards, and skips whatever is already there, so it can be re-run
after an interrupted transfer:

```bash
tools/deploy_data.sh <device-serial> <server-ip>
```

## Settings

Make a plain text file called `Settings` - no `.txt`, no extension at all - and
put it next to the `.nx` files. Windows hides extensions by default and will
quietly save it as `Settings.txt`, which the client won't find, so turn
extensions on in Explorer and check the name is right.

The contents are `name = value`, one per line, spaces around the `=`:

```
ServerIP = 192.168.1.71
ServerPort = 8484
Width = 800
Height = 600
```

Change the IP to whichever machine your server runs on. Everything else the
client needs has a sensible default, so those four lines are enough.

Use 800x600. The login and character screens were built for that size and don't
adapt, so anything else leaves buttons and characters in the wrong places. The
picture is scaled up to fill your screen either way.

## The second screen (AYN Thor)

On a Thor the menus move off the game and onto the handheld's lower display -
a deck of eight pages you swipe between, each staying where you left it:

**world map · inventory · equipment · ability · skills · quests · hotkeys · chat**

So the map is simply *there* while you play, rather than something you open on
top of what you're looking at.

Display 4 on the Thor is a real touch-capable screen carrying
`FLAG_PRESENTATION`. It's driven through an Android `Presentation` with a
second EGL surface sharing the GL context, because SDL2 allows only one window
on Android - `android/.../SecondScreen.java` and `IO/SecondScreenPanel.cpp`.

Devices without a second display are unaffected; the panel simply doesn't
appear and the menus behave normally.

## Playing together

The login screen has **HOST** and **JOIN**. Neither is chosen for you, and
each opens a panel that checks what it needs before it will commit.

**HOST** starts a server on the device itself, then asks how the others should
reach you:

- **Use this wifi** - everyone joins over the network you're already on.
- **Make my own network** - the device *becomes* the network, via Wi-Fi
  Direct. For a car, a hotel, or a router that blocks devices from seeing each
  other. Needs the wifi radio on, but no network to be connected.

**JOIN** looks for hosts and lists them **by name** - "AYN Thor", not an IP
address. You pick one and press Join. There is nowhere to type an address and
that is deliberate; discovery is mDNS (`_maplestory._tcp`) over the network,
falling back to Wi-Fi Direct peer discovery.

Losing the host is survivable: 45 seconds of silence returns you to the login
screen, where hosting yourself or joining someone else is two taps away.
Killing an app doesn't reliably close its sockets, so a client that isn't
writing can otherwise sit forever on a character select it can no longer act
on - which looks exactly like a broken Start button.

**The server needs Termux** (from **F-Droid** — the Play Store build is years
old and its package repository no longer resolves). The app checks for it and
reports honestly rather than failing at the moment you press Host.

`tools/install.sh --server` does the whole thing: it grants Termux storage,
delivers Cosmic, and **runs the setup for you** — Java, MariaDB, the database
and the schema — without you opening a terminal. It takes about twenty
minutes, most of it downloading.

One caveat it will tell you about itself: driving Termux from the PC uses
`run-as`, which only works on a **debuggable** Termux build. A stock F-Droid
Termux is not, and refusing to be driven by other programs is precisely the
point of that. When it cannot, it prints the two lines to paste instead of
pretending it worked. `docs_OFFLINE.md` has the whole story.

### Carrying characters between devices

Three devices played separately all day is three worlds, all changed, none of
them merge-able. But nothing needs to merge - each is a different **character**.
Nobody's progress collided, it just ended up in three places. So the answer
isn't to reconcile worlds, it's to gather characters:

```
python tools/character.py where pc <serial>            what is where
python tools/character.py account alex pc <serial>     take a player with you
python tools/character.py verify alex pc <serial>      prove it arrived whole
```

The unit is an **account**, not a character, because Cosmic gives an account
three slots and a person thinks of all three as theirs. Each is still judged on
its own `lastLogoutTime`, so moving a copy over a newer one is refused -
that's somebody's evening - and a stale copy of one character can't ride along
on a fresh copy of another. `--force` overrides.

What a character *is* gets read out of `information_schema` rather than listed
in the script, so it can't drift when the server is updated. Three things this
cost, every one of them silent, and all three are the same lesson:

- **The last field of the last row went missing.** A tab-separated row whose
  final column is an empty string ends in a tab, `.strip()` removes it, and
  `zip()` then drops the last *column* without complaining.
- **Rows that hang off other rows.** `inventoryequipment` belongs to an
  inventory *item*, and `questprogress` and `medalmaps` belong to a
  *queststatus row*, not to the character. Carry their parent id verbatim and
  Cosmic looks it up, finds nothing, and drops it in silence - the quest stays
  started and the kill count is gone. `information_schema` declares foreign
  keys for `famelog` and `skills` and for not one of these.
- **Cosmic spells the character key three ways** - `characterid`, `cid` and
  `charid`. Looking for only the first two silently leaves behind the monster
  book, cooldowns, and `area_info`, where NPC scripts keep their memory.

`verify` compares every field rather than counting rows, because the count was
right while the data was wrong - and it checks for orphans separately, since
the field comparison has to ignore id columns and that is precisely where this
class of bug hides.

**This is a tool, not a feature.** It needs a PC and adb. Doing it from inside
the game is the next piece of work - see below.

## If you're running Cosmic

Two settings, or nothing works from a phone:

```yaml
HOST: 192.168.1.71
LANHOST: 192.168.1.71
```

These are the addresses the server hands out when you pick a character. Left at
`127.0.0.1` your phone tries to connect to itself, and the Start button looks
completely dead.

Passwords need five characters or more. Shorter ones are rejected before
anything is sent, so they look exactly like a wrong password.

**Turn the autosave up.** Cosmic's own `config.yaml` says it saves "each 1
hour"; that comment is simply wrong, and the interval isn't in the config file
at all. It's hardcoded in `src/main/java/net/server/world/World.java`, in the
line registering `CharacterAutosaverTask` - two minutes in current Cosmic, an
hour in older builds. Whatever it says there is exactly how much of an evening
a crash costs you, so set it to `SECONDS.toMillis(60)` and rebuild with
`./mvnw -DskipTests package`.

It's a full save - inventory and equipment included. The `notAutosave` flag
only changes a log message. Worth saying out loud at INFO too, so you can see
it working rather than hoping:

```java
if (saved > 0) {
    log.info("autosaved {} character(s) in {} ms", saved, ...);
}
```

The line it replaces is at DEBUG, which is off, so there's otherwise no way to
tell "saving every minute" from "not running at all".

Characters also save when you log out properly, so quit through the game rather
than closing the app if you can.

## Something's wrong

**Black or white screen** - the data isn't being found, or `UI.nx` is from v83.

**No text anywhere** - the `fonts` folder is missing.

**Start button does nothing** - `HOST`/`LANHOST` are still pointing at localhost.

**"Password is invalid" no matter what** - your password is under five characters.

**Monsters never appear, even in hunting maps** - your server probably encodes
the spawn packet differently to Cosmic. It's the byte count in
`SpawnMobHandler` and `SpawnMobControllerHandler` in
`Net/Handlers/MapObjectHandlers.cpp`: Cosmic writes 16 bytes of monster status
before the position, the original client expected 22. Get it wrong and monsters
spawn at a nonsense position on a nonsense platform, so they're never drawn -
the packets arrive perfectly and you see nothing.

## Credits

[HeavenClient](https://github.com/ryantpayton/MapleStory-Client) by Ryan Payton,
built on Daniel Allendorf's Journey. The Switch port by
[lain3d](https://github.com/lain3d/HeavenClientNX) is what this started from -
its README is here as `README_SWITCH.md`.

[CHANGES.md](CHANGES.md) lists what I changed and the bugs I fixed along the
way, some of which affect the desktop client too.

## Licence

AGPL-3.0, same as HeavenClient. If you share a build, share the source too.
