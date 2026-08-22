# Before cutting a release

A running list. Tick things off, add to it as they come up.

## Must do

- [ ] **Take the last debug logging out.** The three per-touch lines are gone.
      What remains is the atlas census in `Graphics/GraphicsGL.cpp`
      (`atlas_census` / `report_atlas_contents`), which dumps a table of every
      texture size on each atlas reset. It was for chasing atlas thrashing and
      that is long settled.
- [ ] **Decide the signing key.** It is a debug-signed APK today. Choose the
      key before anyone installs it, not after - changing it later forces an
      uninstall on every device. Keep the keystore somewhere it cannot be lost,
      because a lost key means no upgrades, ever.
- [ ] **Bump the version.** `android/app/build.gradle` still says
      `versionCode 1`, `versionName '1.0'`.
- [ ] **Build from a clean tree at a tag, and push it first.** The licence
      requires the source to match the binary, so the commit the APK is built
      from has to be the commit people can download. No uncommitted changes.
- [ ] **Only link the releases page once a build is on it.** The README now
      points at it.

## Ought to do

- [ ] **The pointer still jumps on the panel.** Ruled out by measurement: page
      geometry, the coordinate conversion, the touch queue, a second contact,
      two input devices. The speed filter rejects bad samples and it still
      happens, so the cause is something not yet looked at. Protocol: say
      **RECORD!** and frames of the panel get captured while it is reproduced.
- [ ] **The level-up sound plays twice.** One call site in the client, reached
      from one handler, and it only fires when the new level is higher - so a
      repeated packet cannot retrigger it. Log the moment it plays and count
      the calls.
- [x] **The Amherst boxes.** Done and confirmed at the table. Four causes:
      `Reactor` shadowed `MapObject::active` with an uninitialised copy (the
      reason it looked random - the box drew, because drawing asks the base,
      while attacking read the garbage), hittability was read from the spawn
      state only, we sent a stance Cosmic silently rejects, and the sounds
      were looked up under three wrong names at once.
- [ ] **Parties are built but untested.** The handler, the panel, the overhead
      gauges and the `/party` commands are all in. None of it has been seen
      working, because it takes two characters logged in at once. Test that
      first, with two devices - it is the reason the port was done.

## Worth checking before anyone else sees it

- [ ] Everything about parties, which needs two characters at once: forming
      one, the invite prompt, the panel, the overhead gauges, expelling.
- [ ] **The movement lag between devices.** Was about a second; two causes
      found and fixed (Nagle on the client socket, and a 400ms jitter buffer
      in `OtherChar`). If other players now look jerky rather than late, the
      dial to raise is `DELAY` in `OtherChar::send_movement`.
- [ ] **The quit / change-character dialog.** It used to trap the player with
      no way out. Should now be a plain Yes/No box.
- [ ] UNEQUIP on the equipment page - added but never seen working.
- [ ] The stat sheet's DETAIL column at its new position - the whole reason the
      list was redrawn by hand, and unverified.
- [ ] Whether 130 pixels is wide enough for the longest class name and stat
      value.

## Decisions still open

- [ ] **The second screen is not mentioned in the README at all.** It is the
      biggest thing in the project and currently invisible to a reader. Needs
      its own section; how to frame it is a judgement call.
- [ ] **`Map001.nx` is mixed.** The login and level-up videos are ours. The
      five page backdrops look like official MapleStory character art and the
      wordmark is a MapleStory derivative, so the file is not cleanly ours to
      publish either. Decide per image.

## Not blockers, but on the list

- [ ] The chat page: emotions on top, keyboard pinned under, chat opening by
      itself.
- [ ] Ability and Skills pages still use the stock layout - only their opacity
      changed.
- [ ] `docs_PACKET_GAP.md` - 233 server messages the client ignores, 50 picked
      out as worth doing. Regenerate it after adding handlers.

## Licence, in one line

AGPL-3.0. Publishing the APK obliges publishing the matching source - which
the repository already is, so a release cut from a pushed tag satisfies it
without any extra work. GitHub attaches the source archives to the tag
automatically. **The game data never goes in a release**; it is Nexon's.
