# How the hard bugs actually got found

Written 23 August 2026, after a session that fixed the Amherst reactors, the
NPC dialog, a quest-log parser, a leaked window, buttons firing twice, a
screen drawn through the wrong projection, and a cash shop.

Not a style guide. A record of what worked and what wasted hours, kept
because the same mistakes were made repeatedly in one day and would have been
made again.

---

## The one rule everything else follows from

**Measure before explaining. Every hour lost today was lost to reasoning
where a measurement was available and cheap.**

The pattern each time:

1. A symptom appears.
2. A confident explanation follows, from reading the code.
3. The fix does not work.
4. Repeat two or three times.
5. Instrument it, and the answer arrives in one round trip.

The cash shop layout took **four** wrong explanations - dropped UI scaling,
stale drawable size, offscreen buffer size, shop layout - before a single
`printf` of what the renderer had been told named it exactly. The Amherst
reactors took three. The NPC dialog took one.

The instrumentation was never expensive. It was skipped because an
explanation was already in hand, and an explanation feels like progress.

**When a fix does not work the first time, stop fixing and start printing.**

---

## Two kinds of bug, needing two kinds of search

**"We do less than they do."** A feature that was never written. Findable by
comparison - `tools/divergence.py` lists, for every file present in both this
client and OpenStory, how much larger theirs is and which functions are
missing. The NPC dialog was here: `parse_simple_selections` existed there and
not here.

**"We do it, wrongly, and silently."** Far more expensive, and invisible to
any comparison of size. The Amherst reactors were four such bugs in a file
where OUR copy was the LARGER one. Three instruments help:

- `tools/theirfixes.py` - OpenStory's own comments explaining what used to be
  wrong. 119 of them in files we share. This is the only tool aimed squarely
  at this class, and its first run found that Cosmic checks HP healing against
  an autoban threshold.
- The client's own `Unhandled packet detected, Opcode: N` while playing.
- Playing the game. The boxes were broken for weeks and surfaced because a
  child went and hit one.

---

## Read the server, not the internet

Cosmic's source is the specification for every packet. `tools/PacketCreator.java`
holds the exact bytes of everything it sends. When our reading of a packet and
its writing of one disagree, the server is right.

This settled, in minutes each:
- the reactor stance the server silently refuses (`Reactor.java:265`)
- the quest log counting info-number quests twice (`addQuestInfo`)
- what `ENTER_CASHSHOP` does before it replies (see below)
- that NX genuinely drops, as 100 and 250 NX cards

And the data is the specification for anything read from NX. `tools/nxdump.py`
exists because three wrong node names for the reactor sounds went unnoticed -
we could write NX archives but never look inside one.

---

## Check what a request does before making it

The worst hour of the session came from wiring a button without reading the
handler behind it.

`ENTER_CASHSHOP` removes the character from the channel AND the map and marks
the shop open BEFORE sending a single byte back. So a client that fails to
change screens does not fail harmlessly - it leaves the player looking at a
world they are no longer standing in, unable to walk or use a portal.

"Nothing happens" was never the worst case, and it should have been checked
for rather than assumed.

**Before sending a new packet, read what the server does with it. Ask
specifically what state it changes before it answers.**

---

## Guard the irreversible half

Where a failure would strand someone, make the failure survivable BEFORE
looking for the cause:

    try  { parseCharacterInfo(recv); }
    catch(...) { /* stale stats are recoverable */ }

    transition();   // this must happen either way

Arriving somewhere with wrong data is recoverable. Being stuck is not. This
guard should have gone in before the button was ever wired, not after it had
stranded somebody twice.

---

## Do not read what is not used

The cash shop handler stepped over the shop's catalogue field by field - a
special-item list, eight most-seller tables, 121 bytes of padding - and threw
part-way through when a count disagreed. All of it was discarded afterwards.

Keeping byte counts in step with a server layout for data that is thrown away
buys nothing and is one more thing that can fail. Read what is needed and
stop.

---

## Believe the owner over the code

Twice the owner's memory of his own game beat a grep:

- "I swear NX dropped for me once." A search of the config found no NX
  setting, and the conclusion drawn - that there was no NX - was wrong. NX
  cards drop and are handled in `Character.java`, nowhere near the config.
- "It's not gone, I'm almost certain your instinct is incorrect." He was
  right; the measurement that followed disproved the theory.

A narrow search that finds nothing is not proof of absence. Say which places
were looked at, and treat "I remember seeing X" as strong evidence worth a
second search.

---

## Correct the record, including your own notes

`PLAN.md` said absorbing OpenStory's renderer would reintroduce a text-layout
crash. That was true when written and false by the time it was quoted back -
their renderer had been rewritten and fixes both crashes better than our
patches did. It was cited as a constraint for weeks.

The real obstacle turned out to be different and narrower: 15 GLES2
adaptations ours carries and theirs does not.

**A note explaining why something cannot be done needs re-checking before it
is used to decide anything.** Same for the README's claim that adb "silently
drops small files" - actually Git Bash rewriting the path, and it misled for
weeks.

---

## Traps specific to this project

- **Enums with a sentinel in the middle.** `StatLabel` puts `NUM_NORMAL`
  between its own values, so an array indexed by it shifts and leaves a null.
  Use a switch. This crashed the character page an hour after being written.
- **`MSYS_NO_PATHCONV`.** Needed for adb's device paths, fatal to gradlew.
  Local files in Windows form, device files in Unix form, and never in a shell
  that runs the build.
- **Heredocs eat `\n`.** Three times in one session a `printf` came out with a
  real newline inside the string literal. Use the Edit tool for anything
  containing an escape.
- **`adb install` reports Success when the build failed.** Check the APK's
  mtime, or compare its md5 against the device.
- **`tcpip 5555` dies on sleep.** The paired Wireless-debugging connection
  survives; the port changes, and a parallel TcpClient scan of 30000-50000
  finds it in seconds.
