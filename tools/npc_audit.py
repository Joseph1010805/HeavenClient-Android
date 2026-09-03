"""WHICH NPCs WOULD SAY NOTHING IF YOU WALKED UP AND PRESSED TALK?

    python tools/npc_audit.py                 # the silent ones
    python tools/npc_audit.py --all           # every placed NPC and its verdict
    python tools/npc_audit.py --npc 2102      # one NPC, in detail

WHY THIS EXISTS
---------------
Twice now a whole subsystem has been built, shipped, and silently never run.

  * `Quest.nx/Say.img` holds the words for every quest in the game. Nothing in
    Cosmic read it, so the first two hours of the game were mute. 2,392
    scripts were generated from it to fix that.

  * Then those scripts were thrown away unread, because
    `QuestScriptManager.start` refuses to load one unless the quest declares a
    `startscript` in Check.img - and almost none do, since in the real game
    the CLIENT owns quest dialogue. Roger worked by accident (quest 1021
    happens to declare one) and everything else was silent.

Both were found by a nine-year-old walking up to an NPC, which is the most
expensive possible test harness. The server WAS saying so - "NPC 2102 is not
coded" went into the log every time - and nobody was reading the log.

So this asks the question offline, against the real data, and gives a number.
A number can be checked before somebody walks to Amherst.

WHAT IT CANNOT SEE, and says so rather than guessing
----------------------------------------------------
  * SHOPS live in the `shops` database table, not in any file here. An NPC
    reported as silent may in fact open a shop.
  * QUEST REQUIREMENTS need a real character - level, job, items, what you
    have already done. An NPC with a quest script is reported as "has one",
    not as "will offer it to you today".

Neither weakens the headline: an NPC with no script, no quest anywhere, and
no small talk cannot say anything to anybody, ever.
"""
import argparse
import os
import re
import struct
import sys

HEADER = struct.Struct('<I I Q I Q I Q I Q')
NODE = struct.Struct('<I I H H 8s')


class Nx:
    """Enough of the format to walk it. See tools/nxdump.py."""

    def __init__(self, path):
        with open(path, 'rb') as f:
            self.data = memoryview(f.read())

        if bytes(self.data[:4]) != b'PKG4':
            raise SystemExit('not an NX file: ' + path)

        (_, self.nodecount, self.nodeoff,
         self.stringcount, self.stringoff,
         self.bitmapcount, self.bitmapoff,
         self.audiocount, self.audiooff) = HEADER.unpack_from(self.data, 0)

    def node(self, i):
        return NODE.unpack_from(self.data, self.nodeoff + i * 20)

    def string(self, i):
        off = struct.unpack_from('<Q', self.data, self.stringoff + i * 8)[0]
        n = struct.unpack_from('<H', self.data, off)[0]
        return bytes(self.data[off + 2:off + 2 + n]).decode('utf-8', 'replace')

    def children(self, i):
        _, child, count, _, _ = self.node(i)
        return [(self.string(self.node(child + k)[0]), child + k)
                for k in range(count)]

    def find(self, path):
        i = 0

        for part in [p for p in path.split('/') if p]:
            for name, j in self.children(i):
                if name == part:
                    i = j
                    break
            else:
                return None

        return i

    def value(self, i):
        _, _, _, kind, blob = self.node(i)

        if kind == 1:
            return struct.unpack_from('<q', blob, 0)[0]

        if kind == 3:
            return self.string(struct.unpack_from('<I', blob, 0)[0])

        return None

    def as_int(self, i):
        """An id may be stored as an int OR as a string - Map.nx does both.

        Reading it as an int when it is a string quietly yields 0, which is
        how every NPC in the game once ended up filed under id 0.
        """
        value = self.value(i)

        if isinstance(value, int):
            return value

        try:
            return int(value)
        except (TypeError, ValueError):
            return None


def quest_npcs(quest_nx):
    """npc id -> quests it starts, quests it finishes."""
    starts, ends = {}, {}

    check = quest_nx.find('Check.img')

    if check is None:
        return starts, ends

    for qid, qi in quest_nx.children(check):
        for phase, pi in quest_nx.children(qi):
            for name, ni in quest_nx.children(pi):
                if name != 'npc':
                    continue

                npc = quest_nx.as_int(ni)

                if npc is None:
                    continue

                into = starts if phase == '0' else ends
                into.setdefault(npc, []).append(qid)

    return starts, ends


def npc_strings(string_nx):
    """npc id -> (name, [small talk lines])."""
    out = {}

    root = string_nx.find('Npc.img')

    if root is None:
        return out

    for npcid, i in string_nx.children(root):
        name = None
        lines = {'n': [], 'd': [], 's': []}

        for key, j in string_nx.children(i):
            if key == 'name':
                name = string_nx.value(j)
            elif len(key) > 1 and key[0] in lines and key[1:].isdigit():
                text = string_nx.value(j)

                if text:
                    lines[key[0]].append(text)

        # Same order the server falls back in - see NpcSmallTalk.java.
        said = lines['n'] or lines['d'] or lines['s']

        try:
            out[int(npcid)] = (name, said)
        except ValueError:
            pass

    return out


def placed_npcs(map_nx):
    """Every npc id that actually stands somewhere in the world.

    An NPC nobody can walk up to cannot be silent AT anybody, and there are
    hundreds of them - unused, event-only, or cut. Reporting those would bury
    the ones that matter.
    """
    found = set()

    root = map_nx.find('Map')

    if root is None:
        return found

    for _, region in map_nx.children(root):
        for _, mapnode in map_nx.children(region):
            life = None

            for name, j in map_nx.children(mapnode):
                if name == 'life':
                    life = j
                    break

            if life is None:
                continue

            for _, entry in map_nx.children(life):
                kind = None
                npc = None

                for name, j in map_nx.children(entry):
                    if name == 'type':
                        kind = map_nx.value(j)
                    elif name == 'id':
                        npc = map_nx.as_int(j)

                # `type` is the string "n" for an npc and "m" for a monster,
                # and both live in the same list.
                if kind == 'n' and npc is not None:
                    found.add(npc)

    return found


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--data', default=os.path.join(
        os.path.expanduser('~'), 'maple', 'wz-v83'),
        help='folder holding Quest.nx, String.nx and Map.nx')
    ap.add_argument('--cosmic', default=os.path.join(
        os.path.expanduser('~'), 'OneDrive', 'Documents', 'Programs', 'Cosmic'),
        help="the server, for its scripts/ folder")
    ap.add_argument('--all', action='store_true', help='every placed NPC')
    ap.add_argument('--npc', type=int, help='one NPC, in detail')
    args = ap.parse_args()

    quest_nx = Nx(os.path.join(args.data, 'Quest.nx'))
    string_nx = Nx(os.path.join(args.data, 'String.nx'))
    map_nx = Nx(os.path.join(args.data, 'Map.nx'))

    starts, ends = quest_npcs(quest_nx)
    strings = npc_strings(string_nx)
    placed = placed_npcs(map_nx)

    npc_dir = os.path.join(args.cosmic, 'scripts', 'npc')
    quest_dir = os.path.join(args.cosmic, 'scripts', 'quest')

    has_npc_script = {
        int(f[:-3]) for f in os.listdir(npc_dir)
        if f.endswith('.js') and f[:-3].isdigit()
    } if os.path.isdir(npc_dir) else set()

    has_quest_script = {
        f[:-3] for f in os.listdir(quest_dir) if f.endswith('.js')
    } if os.path.isdir(quest_dir) else set()

    # HANDLED BEFORE ANY OF THIS, in NPCTalkHandler.
    #
    # A Gachapon and a Maple TV never reach the script/quest/small-talk
    # ladder - the handler catches them by id range and by name and runs a
    # fixed script. Reporting them as silent buried the ones that are.
    # NpcId.GACHAPON_MIN .. GACHAPON_MAX, inclusive.
    GACHAPON = range(9100100, 9100118)

    def special(npc):
        if npc in GACHAPON:
            return 'gachapon'

        name, _ = strings.get(npc, (None, []))

        if name and name.endswith('Maple TV'):
            return 'mapleTV'

        return None

    def verdict(npc):
        built_in = special(npc)

        if built_in:
            return 'built-in', built_in

        if npc in has_npc_script:
            return 'script', 'scripts/npc/%d.js' % npc

        quests = starts.get(npc, []) + ends.get(npc, [])
        scripted = [q for q in quests if q in has_quest_script]

        if scripted:
            return 'quest', 'quest %s' % ', '.join(sorted(set(scripted)))

        if quests:
            return 'SILENT', ('has quest %s but NO script for it'
                              % ', '.join(sorted(set(quests))))

        name, said = strings.get(npc, (None, []))

        if said:
            return 'small talk', '%d line(s)' % len(said)

        return 'SILENT', 'no script, no quest, no small talk'

    if args.npc is not None:
        npc = args.npc
        name, said = strings.get(npc, (None, []))
        kind, why = verdict(npc)

        print('NPC %d  %s' % (npc, name or '(unnamed)'))
        print('  placed in a map : %s' % ('yes' if npc in placed else 'no'))
        print('  npc script      : %s' % ('yes' if npc in has_npc_script else 'no'))
        print('  starts quests   : %s' % (', '.join(starts.get(npc, [])) or '-'))
        print('  ends quests     : %s' % (', '.join(ends.get(npc, [])) or '-'))
        print('  small talk      : %d line(s)' % len(said))

        for line in said:
            print('      "%s"' % (line[:100] + ('...' if len(line) > 100 else '')))

        print('  verdict         : %s (%s)' % (kind, why))
        return 0

    rows = []

    for npc in sorted(placed):
        name, _ = strings.get(npc, (None, []))
        kind, why = verdict(npc)
        rows.append((npc, name or '(unnamed)', kind, why))

    silent = [r for r in rows if r[2] == 'SILENT']

    if args.all:
        for npc, name, kind, why in rows:
            print('%-8d %-28s %-11s %s' % (npc, name[:28], kind, why))
        print()

    if silent:
        print('SILENT - these are placed in the world and cannot answer:')
        print()

        for npc, name, _, why in silent:
            print('  %-8d %-28s %s' % (npc, name[:28], why))

        print()

    counts = {}

    for _, _, kind, _ in rows:
        counts[kind] = counts.get(kind, 0) + 1

    print('%d NPCs are placed in the world.' % len(rows))

    for kind in sorted(counts):
        print('  %-11s %d' % (kind, counts[kind]))

    print()
    print('Shops are NOT visible here - they live in the `shops` database')
    print('table - so some of the silent ones may open a shop instead.')

    # A number to check, and a non-zero exit so this can gate something later.
    return 1 if silent else 0


if __name__ == '__main__':
    sys.exit(main())
