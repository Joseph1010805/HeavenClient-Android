# What to fill in, and what to leave

**Updated 23 August 2026**, after the cash shop went in.

The queue, scoped to a family server rather than a commercial one. Sizes are
OpenStory's line counts for the subsystem - roughly what has to be read and
ported, not typed from nothing.

**The content is not the work.** Cosmic v83 is a complete server: every quest,
NPC, drop table, map and boss already exists and already runs. Nothing here
has to be authored or invented. Every item on this list is a client window
that has not been built, or a packet we do not send.

## The order

1. **Quests** - the biggest single gap, and what turns wandering into playing.
2. **Trade & storage** - somewhere to stash things, and swapping between the
   three of them.
3. **Pets** - 83 lines of AI; the rest is already here.
4. **Party, full UI** - the windows behind the `/party` commands.
5. **Monster book**, **mini-games**, **monster carnival** - the fun pile.
6. **Character looks**, **map atmosphere**, **notifications**.

Running through all of it, separately and continuously: the **instruments**
below, which are a different kind of work and probably the more valuable.

---

## Keeping

| Subsystem | Lines (theirs) | State | Notes |
|---|---:|---|---|
| **Quests** | 3,879 | 177 of 3,297 in `UIQuestLog` | See below - the packet is missing too |
| **Storage & player trading** | ~3,100 | nothing | `UIStorage` 1,180 + `ShopStorageHandlers` 1,225 + `UITrade` 729 |
| **Party, full UI** | 2,022 | HUD only (236 lines) | Invite / settings / member menu windows |
| **Monster book** | 1,341 | nothing | |
| **Character looks** | 1,952 | nothing | Auras, death animation, procedural hats, **mounts** |
| **Mini-games** | 1,179 | nothing | Omok, memory, rock-paper-scissors - two-player, which is the point |
| **Messaging** | 1,168 | nothing | Whisper and messenger |
| **Notifications / UX** | 967 | nothing | Toasts, emotion panel, mob HP gauge. Emotion panel wanted for the chat page |
| **Map atmosphere** | 908 | nothing | Weather, environments, the in-map clock |
| **Monster carnival** | 423 | nothing | Smaller than it sounds |
| **Pets** | 83 | nothing | Only the AI is missing |

### Quests, specifically

Two separate holes, and the second is the one that matters:

- `UIQuestLog` is **177 lines against their 3,297** - a stub. No tracker on
  screen, no marker over an NPC who has one.
- **There is no `QUEST_ACTION` packet at all.** A quest can be *started* by an
  NPC script, because the NPC dialogue path works, but it cannot be accepted,
  forfeited or handed in from the log. The database says exactly that: joey
  has **2 quests started and 0 completed**.

The window also wants a richer text renderer than we have - quest text
carries formatting the current `Text` cannot lay out.

### Done since this list was written

- **Cash shop** (1,151 theirs) - built to our own layout rather than ported,
  because their artwork is one baked 1024x768 picture. Browse by category,
  buy, take out of the locker, wear, leave. Cash equips wear as cosmetics over
  the real gear. The whole CASH tab of the bag now works (opcode 79).

## Cut

| Subsystem | Lines | Why |
|---|---:|---|
| Marriage & family | 873 | |
| Buddies | 1,590 | Three people who live together |
| Guilds & alliance | 1,190 | Three people is not a guild |
| Megaphones & MapleTV | 1,104 | Needs a crowd to mean anything |
| Ranking & report | 1,268 | Reporting players is meaningless here |
| MTS | 815 | Cash item trading between accounts |
| Misc infra | ~3,000 | Their `Gamepad` is GLFW and cannot work on Android; ours uses SDL's controller API. `EmbeddedFonts` is 57,159 lines of font data, not work |
| Hired merchant & personal shop | ~1,200 | No market here to run a stall in |

---

## The other list, and the more valuable one

Everything above is **"they have a thing we do not"** - findable by counting.
The bugs that actually cost days are the other kind: **we do it, wrongly, and
silently.** Four of them shipped in one morning and not one was on any feature
list:

- every mask in the game invisible when worn, because face accessories are
  keyed by expression and the loader walked stances
- cash equips knocking real gear off, because they belong 100 slots higher
- the entire Cash tab inert, because cash items use their own opcode
- leaving the shop disconnecting the player, because Cosmic reads that packet
  by its length

**`docs_INSTRUMENTS.md` is the plan for finding this class without playing.**
The short version, in the order worth building:

1. **Read the server log.** Cosmic names most of these itself and nobody was
   listening. Free, today.
2. **Make "did nothing" impossible** - one log line whenever a tap, a drop or
   a reply falls off the end of a switch.
3. **Opcode census, both directions** - a static diff of our tables against
   Cosmic's `SendOpcode` (307) and `RecvOpcode` (178).
4. **Leftover-bytes check** on every packet handler.
5. **Data audits**, generalised from `tools/shop_audit.py`.
6. **Window smoke-shots** - open every window over adb and look once.

Two tools already cover part of it:

**`tools/divergence.py`** - files present on BOTH sides where theirs does
more. `UIChatbar` is missing 48 functions, `UISkillbook` 33, `UIStatusbar` 32,
`Stage` 25. Not missing features: our versions quietly doing less of something
we already have. This is where the NPC dialog bug lived.

**`tools/theirfixes.py`** - **119 comments** in files we share where OpenStory
wrote down what used to be wrong. The only instrument aimed squarely at the
silent class. Three of the 119 have been looked at:

- `Player.cpp:316` - Cosmic checks an HP heal against `77 * map recovery *
  1.5` and drops any MP heal >= 1000. **Chased down: there is no ban risk and
  never was.** `USE_AUTOBAN` is `false` in this server's config and both
  `AutobanFactory.autoban()` and `AutobanManager.addPoint()` are gated on it.
  Only the log is on.

  What DOES still bite with autoban off is a handler that drops the action
  after the check. There are exactly two: `HealOvertimeHandler:56` discards an
  over-limit HP heal - ours peaks at 30 against a limit of 115 - and
  **`AbstractDealDamageHandler:208` throws away an entire attack that hits
  more mobs than the skill's declared mob count**, silently, which is a live
  combat path worth remembering the next time damage goes missing.
- `MapNpcs.cpp:116` - Cosmic silently drops a TalkToNPC arriving within 500ms
  of the previous one. Mirror it client-side or rapid re-clicks wedge.
- `Player.cpp:254` - server-driven HP loss (damage over time, magic) never
  routes through `Player::damage()`, leaving the invincibility window unset.

## Open, found by reading the log rather than playing

- **`UseItemHandler` throws on unusable items.** Tapping arrows (2060000) in
  the USE tab sends USE_ITEM; Cosmic looks up an item effect that does not
  exist and throws an NPE. Ten of these in one session. The client should not
  offer to "use" an item with no effect data.
- **Four NPCs have no script on the server**: Cody (9200000), Sam (2005),
  John (20000), Pio (10000). Talking to them does nothing, and that is a
  server-content gap, not a client one.
