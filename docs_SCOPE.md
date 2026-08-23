# What to fill in, and what to leave

The queue, scoped to a family server rather than a commercial one. Sizes are
OpenStory's line counts for the subsystem - roughly what has to be read and
ported, not typed from nothing.

## The order

1. **Quests** - the biggest single gap in the tree and how you level.
2. **Trade & storage** - somewhere to stash things, and swapping between the
   three of them.
3. **Party, full UI** - the windows behind the `/party` commands.
4. **Pets** - 64 lines; the rest is already here.
5. **Monster book**, **mini-games**, **monster carnival** - the fun pile.
6. **Cash shop**, **character looks**, **map atmosphere**, **notifications**.

Running through all of it, separately and continuously: the **fix list**
below, which is a different kind of work and probably the more valuable.

---

## Keeping

| Subsystem | Files | Lines | Notes |
|---|---:|---:|---|
| **Quests** | 7 | 3,879 | Ours is 121 lines to their 2,471. **needs Text engine** |
| **Storage & player trading** | ~8 | ~3,100 | `UIStorage` 1,180 + `ShopStorageHandlers` 1,225 + `UITrade` 729 |
| **Hired merchant & personal shop** | ~4 | ~1,200 | The market-stall half. Lower priority than the above |
| **Party, full UI** | 12 | 2,022 | 2 already here |
| **Cash shop** | 9 | 1,151 | |
| **Monster book** | 7 | 1,759 | |
| **Character looks** | 16 | 1,952 | Auras, death animation, procedural hats - and **mounts** |
| **Mini-games** | 7 | 1,179 | Omok, memory, rock-paper-scissors - two-player, which is the point |
| **Messaging** | 9 | 1,168 | Whisper and messenger |
| **Notifications / UX** | 22 | 967 | Toasts, emotion panel, mob HP gauge. The emotion panel is wanted for the chat page |
| **Map atmosphere** | 12 | 908 | Weather, environments, the in-map clock |
| **Monster carnival** | 4 | 423 | Smaller than it sounds |
| **Pets** | 2 | 64 | Only the AI is missing |

## Cut

| Subsystem | Files | Lines | Why |
|---|---:|---:|---|
| Marriage & family | 6 | 873 | |
| Buddies | 15 | 1,590 | Three people who live together |
| Guilds & alliance | 8 | 1,190 | Three people is not a guild |
| Megaphones & MapleTV | 12 | 1,104 | Needs a crowd to mean anything |
| Ranking & report | 6 | 1,268 | Reporting players is meaningless here |
| MTS | 5 | 815 | Cash item trading between accounts |
| Misc infra | 36 | ~3,000 | Their `Gamepad` is built on GLFW and cannot work on Android; ours uses SDL's controller API already. `EmbeddedFonts` is 57,159 lines of font data, not work |

---

## The other list, and the more valuable one

Everything above is **"they have a thing we do not"**. Two other tools cover
what that cannot see:

**`tools/divergence.py`** - files present on BOTH sides where theirs does
more. `UIChatbar` is missing 48 functions, `UISkillbook` 33, `UIStatusbar`
32, `Stage` 25. Not missing features: our versions quietly doing less of
something we already have. This is where the NPC dialog bug lived.

**`tools/theirfixes.py`** - **119 comments** in files we share where OpenStory
wrote down what used to be wrong. This is the only instrument that finds the
"we do it, wrongly, and silently" class - the one that produced four Amherst
reactor bugs in a file where OUR copy was the larger one. It found, in its
first run:

- `Player.cpp:316` - Cosmic **autobans** an HP heal above `77 * map recovery
  * 1.5` and drops any MP heal >= 1000. We wrote standing regeneration.
  Checked: ours peaks at 30 against a limit of 115 on a normal map, and heals
  every 10s against a 1.5s fast-heal threshold, so we are inside both. The
  limit scales with the map's own recovery rate, so a map below ~0.26 would
  put us over - unlikely, saunas set it high, but it is the one thing here
  that could ban a character.
- `MapNpcs.cpp:116` - Cosmic silently drops a TalkToNPC arriving within 500ms
  of the previous one. Mirror it client-side or rapid re-clicks wedge.
- `Player.cpp:254` - server-driven HP loss (damage over time, magic) never
  routes through `Player::damage()`, leaving the invincibility window unset.

None of those are visible by counting lines, and all three are in code we run
constantly.
