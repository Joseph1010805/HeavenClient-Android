# Playing with no network

Written 23 August 2026, when the pieces were checked rather than assumed.

The goal is three handhelds in a car with no wifi. The answer is not an
offline mode in the client - it is **the server travelling with you**. One
device carries Cosmic; it plays against its own loopback, and anybody else
joins over a phone hotspot. A faked offline mode would be permanently
second-rate and would drift from the real rules; this IS the real rules.

## The client needs nothing

`ServerIP` lives in the settings file on the device, is editable without a
rebuild, and already defaults to `127.0.0.1`.

## Cosmic needs no second config either

This is the part worth knowing, because it looks like it should need one.
`Server.getInetSocket` picks the address it hands back from who is asking:

    if (IpAddresses.isLocalAddress(remoteIp))    -> LOCALHOST
    else if (IpAddresses.isLanAddress(remoteIp)) -> LANHOST

So one running server answers the handheld it lives on **and** everyone else
on the hotspot, with nothing to switch between them. That matters more than
it used to: leaving the cash shop is a channel reconnect, and this is what
makes the address it hands over correct in both cases.

## What travels

| | | |
|---|---:|---|
| `Cosmic.jar` | 51 MB | architecture-independent, no rebuild |
| `wz/` | **596 MB, 22,180 files** | tar it - see below |
| `scripts/` | 8.4 MB, 1,915 files | the JS behind every NPC and quest |
| `config.yaml` | | `DB_HOST: localhost`, root, no password |

**The database does not travel.** Cosmic manages its schema with Liquibase
and builds the whole thing on first run against an empty database.

Push the wz as ONE tar. 22,180 files pushed individually takes hours; one
file takes minutes. Uncompressed on purpose - the wz is mostly PNG already,
so gzip costs CPU at both ends and saves little, and an uncompressed tar can
be checked by size without unpacking it.

## MariaDB instead of MySQL

Termux has no MySQL; it has MariaDB. Checked before relying on it: none of
Cosmic's 37 schema files use anything MySQL-8-only - no `utf8mb4_0900`
collations, no JSON functions, no CHECK constraints. One
`latin1_german1_ci`, which MariaDB supports. The dialect difference does not
reach us.

## ⚠ Android 12 and up will kill the server unless told not to

The single most important thing on this page, and it is invisible until the
server dies for no reason ten minutes in.

Android 12 introduced the **phantom process killer**: child processes spawned
by an app - which is exactly what `mariadbd` and `java` are under Termux -
are counted, capped at 32, and killed for using CPU in the background. A
database and a game server are the textbook case. Over adb, no root:

    adb shell settings put global settings_enable_monitor_phantom_procs false
    adb shell /system/bin/device_config set_sync_disabled_for_tests persistent
    adb shell /system/bin/device_config put activity_manager max_phantom_processes 2147483647

Done on the Thor (Android 13) on 23 August. **It must be redone on any other
device, and may need redoing after a system update.** Check with:

    adb shell settings get global settings_enable_monitor_phantom_procs

## Doing it

    tools/stage_server.sh <device-serial>     # on the PC

Termux itself can be installed over adb rather than by hand - the official
build, from the project's own GitHub releases:

    # arm64 device; there are per-ABI APKs and a universal one
    BASE=https://github.com/termux/termux-app/releases/download/v0.118.3
    curl -LO "$BASE/termux-app_v0.118.3%2Bgithub-debug_arm64-v8a.apk"
    adb install -r termux-app_*.apk
    adb shell pm grant com.termux android.permission.READ_EXTERNAL_STORAGE
    adb shell pm grant com.termux android.permission.WRITE_EXTERNAL_STORAGE

Granting storage that way saves the on-screen prompt `termux-setup-storage`
would otherwise raise. **Do not mix the GitHub and F-Droid builds** - they
are signed with different keys and will not upgrade over one another.

Then, on the handheld, one line:

    bash /sdcard/Download/cosmic/termux_setup.sh

and after it finishes, the SERVER switch on the game's login screen does the
rest. `~/cosmic/run.sh` still starts it by hand if wanted.

Both scripts are re-runnable and check every step before doing it.

## What is still unknown

Cosmic under Termux now runs on both handhelds - the RP5 comes up in about
6.5 seconds from a cold `run.sh`. What is still not measured:

- **Memory.** Cosmic, MariaDB and the game all on one handheld, and the game
  holds an 8192x8192 texture atlas of its own. `run.sh` asks for 1536m rather
  than the desktop's 2048m; raise it if the server runs short, lower it if the
  game starts stuttering.
- **Storage.** 4.4 GB of client NX plus 596 MB of server wz is about 5 GB.
- **Startup time**, reading 596 MB off a handheld's SD card.

It fails cheaply. Either it starts or it prints why.

## Carrying a character

Two different problems, and conflating them makes the easy one look hard.

**One place at a time** - everyone is in the car, nobody is playing at home.
Copy the whole database out before leaving and back afterwards; that is
`tools/sync_world.sh`, and it refuses to run in the wrong direction.

**Several places at once** - the day this house actually has. The Thor joins
somebody for two hours, the RP5 hosts alone for four, the Quest hosts with
friends for another two, and in the evening all three play together. Three
worlds, all changed, none of them merge-able. A whole-database copy destroys
whichever side is written second.

But nothing needs to merge. Each of those is a different **character**.
Nobody's progress collided - it just ended up in three places. So the answer
is not to reconcile worlds, it is to gather characters, and that is
`tools/character.py`:

```
python tools/character.py where pc 6b0cf210          what is where
python tools/character.py account joey pc 6b0cf210   take a player with you
python tools/character.py verify joey pc 6b0cf210    prove it arrived whole
```

The unit is an **account**, not a character, because Cosmic gives an account
three slots and a person thinks of all three as theirs. Each character is
still judged on its own `lastLogoutTime`, so a stale copy of one cannot ride
along on a fresh copy of another, and moving a copy over a newer one is
refused - that is somebody's evening. `--force` overrides.

What a character IS gets read out of `information_schema` rather than listed
here, so it cannot drift when the server is updated: the `characters` row,
the ~20 tables keyed by `characterid`, and `inventoryequipment` - which hangs
off the inventory **item**, not the character. That last level is the one a
naive copy silently loses, taking every scroll and every stat on every equip
with it.

Three things this cost, every one of them silent:

- The tabbed transport **ate the last field of the last row**. A row whose
  final column is an empty string ends in a tab, and `.strip()` removes it;
  `zip()` then dropped the last *column* without complaining. It surfaced only
  because `giftFrom` happens to be NOT NULL with no default. Had it been
  nullable, characters would have arrived subtly wrong forever.
- **The PC and the handhelds are at different Liquibase revisions**, so a
  column on one side may not exist on the other. Only shared columns travel.
- The first failed run **left an account and a stub character behind**. The
  whole move is one transaction now.

`verify` walks all 72 character fields and all 22 equipment fields rather
than counting rows, because the count was right while the data was wrong.

**What is NOT built:** any of this from inside the game. `character.py` needs
a PC and adb, so today it is a tool for one person, not for the family. The
in-game "bring my characters with me" flow is the next piece.

## If the host dies mid-session

Cosmic autosaves every logged-in character **every 60 seconds**
(`World.java`, `CharacterAutosaverTask`) - it was every two minutes, and the
`config.yaml` comment claiming "each 1 hour" was simply wrong. The save is a
full one: inventory and equipment included. `notAutosave` only changes a log
line.

That interval is exactly how much of everyone's evening an outage costs, and
this server runs on a handheld somebody can close, drop, or run flat with no
warning.

The autosaver now says so at INFO, once per pass - `autosaved 3 character(s)
in 41 ms` in `cosmic.log`. The per-character line it replaced was at DEBUG,
which is off, so there was no way to tell the difference between saving every
minute and not running at all.

On the client side, losing the host is no longer fatal: `Net/Session.cpp`
treats 45 seconds of silence as a disconnect and returns to the login screen,
where hosting yourself or joining somebody else is two taps away. Killing an
app does not always close its sockets, so a client that is not writing can sit
forever on a character select it can no longer act on - which is exactly what
happened, and looked like a broken Start button.
