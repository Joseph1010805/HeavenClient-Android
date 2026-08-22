# Where this is, and what happens next

Written 22 August 2026, at the end of a long session.

`ISSUES.md` is the running list of everything reported. `RELEASE_CHECKLIST.md`
is what has to be true before a build goes out. This file is the direction:
what state the project is in and what to do next, in order.

## Where it is

The game is playable. Moving, fighting, dying, loot, the inventory, equipping,
skills, shops, NPCs, levelling and standing recovery all work. Three separate
memory-corruption crashes were found and fixed, any of which would kill the app
at random - a stack overread in the NX string reader, an out-of-bounds font
lookup, and a text-layout split that recursed until the stack ran out.

The second screen is the bulk of the new work. The lower panel is a deck of
pages: world map, inventory, equipment, ability, skills, and three not yet
built. The world map is full-bleed with its own controls; the inventory and
equipment pages are drawn by hand rather than using the window artwork, so they
can be transparent over a backdrop; the stat sheet is a compact list of our own
so the detail column has room to open. Each page has its own picture behind it.

What is NOT built: quest completion, pets, summons, doors, mists, chairs, and
other players' skill effects. None of them crash - they simply never happen.
Parties are now built but have never been tested.

## The thing that changed the plan

[OpenStory](https://github.com/rdiol12/OpenStory) is a HeavenClient fork built
for Cosmic, which is the server this plays against. It has **241 packet
handlers to our 75**, and it has the systems behind them - `Character/Party`,
`UIPartyHUD`, `UIPartyInvite`, `Door`, `MapDoors`, `Summon`, `MapSummons`,
`Pet`, `PetAI`. One of its commits is called "Wire the packets the server sends
but nothing was reading", which is the same job as `docs_PACKET_GAP.md`, done.

It is AGPL-3.0, the same licence as this, so it can be used with attribution.
Cloned at `../OpenStory`.

**It cannot be merged.** There is a common ancestor - `4fc96b0`, January 2020 -
but they moved every file into `src/`, so a merge reads as deleting ours. And
absorbing their tree wholesale would hand back the text-layout crash, which
they still have at `GraphicsGL.cpp:1220`, and cost the Android port, the second
screen and this session's fixes.

So: port from it by SUBSYSTEM. Parties as a set, doors as a set. Eight or ten
batches rather than 175 edits, each built and tested on the device before the
next - because a handler that is wrong fails silently, which is worse than one
that crashes.

## Tomorrow, in this order

**1. Take the debug logging out.** Three places write a line per touch event -
`SecondScreen.java`, `SecondScreenPanel.cpp`, `UIWorldMap.cpp`. They are there
to catch the pointer glitch. If it has not been caught by now, take them out
anyway and put them back when it is being hunted deliberately.

**2. The memory-mapped NX reader.** Small, contained, and it deletes bugs
rather than patching them. `file_handle` appears 47 times inside
`libs/NoLifeNx/` and nowhere else in the client, so the change cannot reach
past that directory. Their POSIX path is plain `open`/`fstat`/`mmap` - nothing
to port for Android, and a 4.5GB mapping is nothing against a 64-bit address
space.

It removes the string and bitmap caches entirely (they only exist to paper over
stdio), which takes with them the fragile pointer identity that made the sprite
atlas hard to reason about. Their bitmap decode is also safer than ours -
`LZ4_decompress_safe` bounded both ways, zeroing on failure so a bad bitmap
draws blank instead of crashing.

The one unknown is whether `mmap` behaves on Android's FUSE-backed `/sdcard`.
Read-only `MAP_SHARED` should be fine, but that is the thing to test first, and
it answers itself in minutes: either the login screen draws or the first map
throws. Reverting is one `git checkout` of one directory.

**3. Parties.** DONE, and untested. `Character/Party`, `SocialHandlers` on
PARTY_OPERATION (62), `UIPartyHUD` with overhead gauges on each member, and
`/party create | leave | invite | expel | leader | list` in the chat bar
because nothing else could reach any of it.

`UIPartyInvite` and `UIUserList` were NOT ported - those are the real party
windows, and the typed commands do the same job for now.

Two of the packet layouts are not what the v83 documentation implies, and
both are commented where they are read: the leader id sits between the
channels and the map ids, and leave/expel/disband share one status byte
rather than two booleans - reading it as two crashes the client on a disband.
Both came from OpenStory having already hit them against Cosmic.

**The next thing to do is test it**, which takes two characters logged in at
once. Nothing here has been seen working.

## After that

- The chat page: emotions on top, keyboard pinned underneath, chat opening by
  itself.
- The boxes in Amherst, which do not break reliably and make no sound. Reactor
  handling, never investigated.
- Quest completion, doors, summons, pets - each its own batch from OpenStory.
- Offline play: one device hosts. The first thing to check is whether Termux
  can run Java 21, because Cosmic requires it and everything else depends on
  that answer.

## Still unexplained

- **The pointer jumps on the panel.** Ruled out by measurement: page geometry,
  the coordinate conversion, the touch queue, a second contact, two input
  devices. A speed filter demonstrably rejects bad samples and it still
  happens. Say **RECORD!** and frames of the panel get captured while it is
  reproduced.
- **The level-up sound plays twice.** One call site, reached from one handler,
  and it only fires when the new level is higher - so a repeated packet cannot
  retrigger it.

## Unverified

Built but never seen working: UNEQUIP on the equipment page, and the stat
sheet's DETAIL column at its new position - which is the entire reason the stat
list was redrawn by hand.
