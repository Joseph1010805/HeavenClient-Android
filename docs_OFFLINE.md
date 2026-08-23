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

## Doing it

    tools/stage_server.sh <device-serial>     # on the PC

then on the handheld, in Termux (from **F-Droid** - the Play Store build is
years old and its repository no longer resolves):

    termux-setup-storage
    cp /sdcard/Download/cosmic/termux_setup.sh ~
    bash ~/termux_setup.sh
    ~/cosmic/run.sh

Both scripts are re-runnable and check every step before doing it.

## What is still unknown

Cosmic under Termux has never been run. Java 21 on aarch64 is routine, the
JAR needs no rebuild, and the SQL checks out, so it ought to start. What
cannot be predicted from here:

- **Memory.** Cosmic, MariaDB and the game all on one handheld, and the game
  holds an 8192x8192 texture atlas of its own. `run.sh` asks for 1536m rather
  than the desktop's 2048m; raise it if the server runs short, lower it if the
  game starts stuttering.
- **Storage.** 4.4 GB of client NX plus 596 MB of server wz is about 5 GB.
- **Startup time**, reading 596 MB off a handheld's SD card.

It fails cheaply. Either it starts or it prints why.

## Carrying a character, and the trap in it

Two different problems, and conflating them makes the easy one look hard.

**One place at a time** - everyone is in the car, nobody is playing at home.
Copy the whole database out before leaving and back afterwards. It is 6.7 MB;
`mysqldump` each way takes seconds. **This is almost certainly the real case,
and it needs no work at all.**

**Two places at once** - somebody plays at home while a character is away.
Now a whole-database copy destroys whichever side is written second, and the
answer is per-character export/import: the `characters` row plus the ~21
tables keyed by `characterid` (inventoryitems, equipment, skills, cooldowns,
skillmacros, quest*, keymap, savedlocations, famelog, monsterbook, ...). A
bounded few days' work.

Do the trivial one first. Build the other when somebody actually needs it.
