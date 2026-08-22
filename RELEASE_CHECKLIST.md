# Before cutting a release

A running list. Tick things off, add to it as they come up.

## Must do

- [ ] **Take the debug logging out.** Three of these write a line per touch
      event, which floods the log and buries anything real:
    - `android/.../SecondScreen.java` - the `[raw]` line, every MotionEvent
    - `IO/SecondScreenPanel.cpp` - the `[cursor]` line, every touch
    - `IO/UITypes/UIWorldMap.cpp` - the `[cursor] worldmap ->` line
      They exist to chase the pointer glitch. Keep them until that is settled,
      then remove all three together.
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
- [ ] **Boxes in Amherst do not break reliably, and make no sound.** Reactor
      handling. Never investigated.
- [ ] **Parties do not work.** The client sends an invite and has no handler
      for the reply, so it never learns a party formed. The largest functional
      gap for playing together, and the first thing anyone else will hit.

## Worth checking before anyone else sees it

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
