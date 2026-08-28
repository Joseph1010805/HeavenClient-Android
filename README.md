# HeavenClient for Android

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
can't give you any - see below.

## Getting the game files

The client needs MapleStory's `.nx` data - the artwork, maps, music and sound.
Those belong to Nexon, so they're not here and I can't send them to you. You
convert them yourself from a client you already have, using
[NoLifeWzToNx](https://github.com/ryantpayton/NoLifeWzToNx).

There's no download for that tool - it's a Visual Studio project you build
yourself, and it won't compile as-is on anything recent. Three things need
fixing first:

- It uses `std::experimental::filesystem`, which no longer exists. Change it to
  `std::filesystem` and set the project to C++17.
- That then trips a deprecation warning on `<codecvt>`, and warnings are treated
  as errors. Define `_SILENCE_ALL_CXX17_DEPRECATION_WARNINGS`.
- The `libsquish.lib` it ships with is too old to link against and fails with
  `C1047`. The source is in the same folder - add `includes/libsquish/*.cpp` to
  the project and drop the `.lib`.

Then run it once per file:

```
NoLifeWzToNx.exe -c Character.wz
```

The `-c` matters. Without it you get server-format files the client can't read.

You need all 15 files listed in `Util/NxFiles.h`. Nearly all of them come from
a v83 client, with one exception: **`UI.nx` has to come from a later client**
(I used v178). The v83 interface is too old - the client looks for menus that
didn't exist yet and refuses to start.

There's also an optional 16th file, `Map001.nx`, holding custom artwork for the
login, world select, character select and character creation screens. It isn't
converted from anything - `tools/make_assets.py` builds it from your own video
and images, and CHANGES.md explains how. Without it those screens fall back to
the stock artwork, which is what they used before.

## Downloading a build

There's an APK on the [releases page](https://github.com/Joseph1010805/HeavenClient-Android/releases).
It's the app and nothing else - no game data, for the reason above - so you
still need everything in the previous section before it will start.

It's arm64 only. Every Android handheld and phone made in the last several
years is arm64, but an old 32-bit tablet or an x86 emulator won't install it,
and Android says "app not installed" without saying why. Android 7 or newer.

You'll also have to let your device install it. It isn't from the Play Store, so
Android blocks it the first time and offers you a settings screen - allow this
source and press install again.

If you'd rather build it yourself, that's the next section.

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

**The server needs Termux**, which the app checks for and reports honestly
rather than failing at the moment you press Host. `tools/stage_server.sh` and
`tools/termux_setup.sh` put Cosmic on a device; `docs_OFFLINE.md` has the
whole story.

### Carrying characters between devices

Three devices played separately all day is three worlds, all changed, none of
them merge-able. But nothing needs to merge - each is a different **character**.
Nobody's progress collided, it just ended up in three places. So the answer
isn't to reconcile worlds, it's to gather characters:

```
python tools/character.py where pc <serial>            what is where
python tools/character.py account joey pc <serial>     take a player with you
python tools/character.py verify joey pc <serial>      prove it arrived whole
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
