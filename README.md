# HeavenClient for Android

MapleStory on your phone or handheld. This is an Android port of
[HeavenClient](https://github.com/ryantpayton/MapleStory-Client), an open-source
client written from scratch, and it plays against a v83 private server like
[Cosmic](https://github.com/P0nk/Cosmic) or HeavenMS.

I built it to play with my son over our home WiFi on an AYN Thor. It works:
login, character creation, quests, combat, the lot.

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

One thing to watch: `adb push` sometimes reports small files as copied when
nothing arrived. Check with `ls` rather than trusting what it says.

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

Cosmic only saves characters once an hour by default, so a crash can cost you
an hour of play. It isn't in the config file - it's hardcoded in
`src/main/java/net/server/world/World.java`, in the line registering
`CharacterAutosaverTask`. Change both `HOURS.toMillis(1)` to
`MINUTES.toMillis(2)` and rebuild with `./mvnw -DskipTests package`. `MINUTES`
is already imported.

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
