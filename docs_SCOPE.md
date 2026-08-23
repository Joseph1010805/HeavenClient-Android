# What to fill in, and what to leave

The queue, scoped to a family server rather than to a commercial one. Sizes
are OpenStory's line counts for the whole subsystem - roughly what would have
to be read and ported, not what would have to be typed from nothing.

**Decide each one.** Cutting something is cheap now and expensive later, so
the point of this list is to be argued with. `KEEP` and `CUT` below are
Joseph's calls where he has made them, and a recommendation where he has not.

Two things worth holding in mind while reading:

- These are the gaps of the form **"we do less than they do"**, which is what
  `tools/divergence.py` can see. The other kind - we do it, wrongly, and
  silently - does not appear here at all. The Amherst boxes were four of those
  and our file was *larger* than theirs.
- Anything marked **needs Text engine** depends on OpenStory's rewritten text
  layer (inline images, UTF-8). Porting those means the renderer question in
  `PLAN.md`, not just the subsystem.

---

## Decided

| Subsystem | Files | Lines | Call |
|---|---:|---:|---|
| Cash shop | 9 | 1,151 | **KEEP** |
| Party, full UI | 12 | 2,022 | **KEEP** - 2 already here |
| Hired merchants & trade | 17 | 4,301 | **KEEP** |
| Monster carnival | 4 | 423 | **KEEP** |
| Marriage & family | 6 | 873 | **CUT** |

## To decide

| Subsystem | Files | Lines | What it is | Recommend |
|---|---:|---:|---|---|
| **Quests** | 7 | 3,879 | The quest log itself. Ours is 121 lines to their 2,471 - the largest single gap in the tree, and quests are how you level. **needs Text engine** for the quest text | **YES, first** |
| **Pets** | 2 | 64 | Just the AI; the rest is already here. Tiny | **YES** |
| **Monster book** | 7 | 1,759 | Card collection and the book UI | YES |
| **Buddies** | 15 | 1,590 | Friend list, groups, memos. Three people who live together may not need it | Ask |
| **Messaging** | 9 | 1,168 | Whisper, messenger windows | YES |
| **Trade & shops** | — | — | folded into hired merchants above | — |
| **Storage** | — | — | part of trade & shops | — |
| **Mini-games** | 7 | 1,179 | Omok, memory, rock-paper-scissors | Ask |
| **Map atmosphere** | 12 | 908 | Weather, environments, the in-map clock | YES - cheap, and it is scenery your kids will notice |
| **Character looks** | 16 | 1,952 | Auras, mounts, death animation, procedural hats and weapons | Ask - cosmetic, but mounts are a real feature |
| **Notifications / UX** | 22 | 967 | Toasts, notification centre, emotion panel, mob HP gauge | YES - small, and the emotion panel is wanted for the chat page |
| **Megaphones & TV** | 12 | 1,104 | Server-wide shouting and MapleTV | **CUT** - needs a crowd to matter |
| **Ranking & report** | 6 | 1,268 | Leaderboards, reporting players | **CUT** - reporting is meaningless here |
| **MTS** | 5 | 815 | Cash item trading between accounts | **CUT** |
| **Guilds & alliance** | 8 | 1,190 | Guild, BBS, alliances | Ask - three people is not a guild, but kids like badges |
| **Misc infra** | 36 | ~3,000 | Gamepad handling, crash logging, UI scaling, a few handler files. `EmbeddedFonts` is 57k lines of font data and is not work | Ask - the gamepad file is interesting for the handhelds |

---

## Not in this list, and larger than all of it

The 85 files that exist on both sides where theirs is bigger - `UIChatbar`
(48 functions missing), `UISkillbook` (33), `UIStatusbar` (32), `Char` (17),
`Stage` (25). These are not missing features, they are our versions doing less
than theirs of something we already have. `tools/divergence.py <path>` lists
the specific functions for any of them.

That is where the NPC dialog bug lived, and it is where the next few will.
