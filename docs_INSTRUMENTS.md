# Finding the silent failures without playing

Written 23 August 2026. `docs_DEBUGGING.md` is about how the hard bugs got
found once somebody was already looking at them. This is about finding them
when nobody is.

## The problem

The bugs that cost days in this project do not crash and do not print. They
do nothing. Four shipped in one morning:

- every mask in the game invisible when worn
- the whole CASH tab of the bag inert
- cash equips knocking real gear off
- leaving the shop disconnecting the player

None printed a character. From the outside a silent failure is
indistinguishable from a feature nobody built, so it survives until somebody
at the table happens to try that exact thing - and mostly they do not.

**The reason is not that these are hard to see. It is that the code says
nothing when it does nothing.** A `switch` with no matching case is silent by
construction; so is a lookup that returns nothing, and a handler with no
handler. Every fall-through in this codebase inherited that default. The work
below is mostly inverting it.

---

## Built

### 1. `tools/serverlog.py` - read what the server says

**The highest value per minute of anything here, and it existed all along.**

Cosmic writes to `Cosmic/logs/cosmic-log.log`. The morning the cash shop was
built it logged `Denied to sell cash item with SN 10002319` **thirty-four
times**, once per Buy tap, while the bug was being chased from the client side
by reading code.

The server is the only participant that can see both what we sent and what the
rules are. When something does nothing, look here first.

    python tools/serverlog.py              # last 2 hours, digested
    python tools/serverlog.py --follow     # live, while somebody plays
    python tools/serverlog.py --kind Cash

Lines are grouped by shape - numbers and hex blobs collapsed - so fifty
identical failures are one row with a count and the rare thing is not buried.
Sixty-six complaints came out as **seven distinct** the first time it ran, and
two of the seven were bugs nobody knew about:

- **`UseItemHandler` NullPointerException, ten times.** The client offered to
  "use" arrows, which have no effect node. Fixed: `ItemData::is_usable()`.
- **Four NPCs with no script**: Cody (9200000), Sam (2005), John (20000),
  Pio (10000). A server-content gap, invisible from the client forever.

### 2. `Util/Silent.h` - make "did nothing" impossible

`Silent::report(where, what)` at the bottom of every dispatch - the default
case, the unmatched lookup, the guard that returns early. De-duplicated by
text, so a player tapping a dead control fifty times reports one bug.

    adb logcat -s HeavenSilent:I

Wired in so far:

| Where | Catches |
|---|---|
| `PacketSwitch::warn` | Unhandled opcodes. **These were already being reported - into `Console`, which compiles to nothing unless `PRINT_WARNINGS` is set and writes to stdout even then, and stdout on Android goes nowhere.** The client has been telling itself about every packet it could not handle, into a void, for the whole life of this port. |
| `PacketSwitch::forward` | A handler that returned with bytes unread - it parsed a layout that is not the one the server wrote. The quest-log parser looked exactly like this. |
| `UIItemInventory::activate_slot` / `doubleclick` | A tab with no case. This is where the dead CASH tab lived. |
| `ItemIcon::drop_on_equips` | A drop that matched nothing. |
| `Clothing` constructor | An equip that loaded no art at all and will be worn while drawing nothing. **This alone would have found every mask.** |

Add more as dispatch points are found. The rule: report at the bottom, not for
ordinary conditions.

### 3. `tools/opcode_census.py` - the vocabulary diff

Static, no gameplay, no device.

    python tools/opcode_census.py            # cut systems hidden
    python tools/opcode_census.py --all --verbose

Three questions:

- **DEAF** - packets Cosmic can send that we have no handler for. Something
  the server tells us and we throw away.
- **MUTE** - packets Cosmic can receive that we never send. **A feature that
  cannot be triggered at all.** The dead CASH tab was one of these: cash items
  go out on opcode 79 and nothing in the client ever wrote that number.
- **DRIFT** - same name both sides, different number. Always a bug. Currently
  zero, which is worth knowing.

Current state: Cosmic can send **307** kinds and we handle **94**; it can
receive **178** and we can send **48**. Excluding cut systems that leaves
**170 deaf** and **101 mute**.

Interesting ones already visible in MUTE, all cheap and all in systems we
keep: `FACE_EXPRESSION` (0x33, emotes - wanted for the chat page),
`USE_CHAIR` / `CANCEL_CHAIR`, `STORAGE`, `MONSTER_BOOK_COVER`.

### 4. `tools/shop_audit.py` - ask the data

Already built, for the cash shop. Found 84 undrawable items out of 1,866, of
which 79 were one bug. The pattern generalises: **ask the archives a question
every item must answer, and let the ones that cannot answer name themselves.**

---

## Not built

Roughly in order of value.

**Generalise the data audit.** Every equip in the game, not just the shop
ones: is there art the renderer can load? Every mob, NPC and reactor the
shipped maps reference: does it resolve? Every skill: does its effect exist?

**NX path audit, static.** Extract every string literal used as an NX path
from the source, resolve it against the archives, list the misses. The reactor
sounds were three wrong node names, silent for weeks. This needs no runtime at
all.

**Switch-case divergence.** `tools/divergence.py` compares file sizes;
comparing which *enum values a switch handles* would have found the CASH tab
from the source alone.

**Packet capture into offline replay.** Record a session's bytes to a file,
then re-run every handler against the capture in a test binary on the PC. One
playtest becomes a permanent regression suite with no device in the loop, and
every parser change is checked against real traffic.

**Window smoke-shots.** Script adb to open every window in turn and screenshot
it; diff against last known good. Catches blank windows, controls off the
bottom of the screen, panels cut off - the class this project keeps hitting
because the phone is not the screen the UI was authored for.

**Headless render census.** Draw every equip in the game on a mannequin and
count non-transparent pixels. Zero means invisible. Catches what the data
check cannot: art that exists, loads, and still draws nothing.

---

## The limit, stated honestly

Of the four bugs that started this, three were findable by the instruments
above - the log would have named the refused purchase, the census would have
named the CASH tab, the data audit did name the masks.

**The fourth would not.** Cash equips going to the real slot were accepted by
the server, threw nothing, logged nothing, and produced a perfectly coherent
result: the item went on, and the old one came off. It took the owner knowing
how MapleStory is supposed to work.

Instrumentation catches *the machine objected*. It cannot catch *the machine
did something coherent and wrong*. For that there is no substitute for
somebody who knows the game, and no amount of tooling should be mistaken for
one.
