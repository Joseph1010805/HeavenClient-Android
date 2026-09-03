"""EVERY GENERATED QUEST SCRIPT, CHECKED WITHOUT PLAYING IT.

    python tools/quest_lint.py                # the counts
    python tools/quest_lint.py --show ask     # the quest ids for one check

WHY THIS EXISTS
---------------
2,392 quest scripts were generated from Quest.nx/Say.img and perhaps five have
ever been played. Two whole CLASSES of mistake were found by walking into them:

  * a quiz generated as a plain conversation, so every answer was accepted
    (495 scripts had a question in them)
  * `qm.completeQuest()`, which marks a quest done and pays nothing at all
    (all 2,392)

Neither was on the generator's own list of things it could not express. Both
were found by a nine-year-old talking to an NPC.

So this asks Say.img what each quest NEEDS and the script what it DOES, and
reports where they disagree - for all of them at once, in a second, without
the game running.

WHAT IT CANNOT DO
-----------------
It cannot tell whether the words make sense, whether an answer is the right
answer, or whether a quest is winnable. It compares structure. A clean run
means no KNOWN class of mistake is present, not that the quests work.
"""
import argparse
import io
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from gen_quest_scripts import Nx, lines, quiz          # noqa: E402


GENERATED = 'GENERATED from Quest.nx/Say.img'


def check(nx, say, script_dir):
    """Every disagreement between what a quest needs and what its script does."""
    found = {
        'quiz not generated as a quiz':      [],
        'quiz on START not generated as one': [],
        'pays nothing (completeQuest)':      [],
        'starts nothing (startQuest)':       [],
        'options in a non-quiz page':        [],
        'script is empty of dialogue':       [],
    }

    for qid, idx in nx.children(say):
        if not qid.isdigit():
            continue

        path = os.path.join(script_dir, qid + '.js')

        if not os.path.exists(path):
            continue

        with io.open(path, encoding='utf-8', errors='replace') as f:
            body = f.read()

        # Hand-written scripts are somebody's considered work and are not
        # ours to grade. Only what the generator produced is checked.
        if GENERATED not in body:
            continue

        start_node = nx.child(idx, '0')
        end_node = nx.child(idx, '1')

        is_quiz = bool(quiz(nx, end_node))
        looks_like_quiz = 'qm.sendSimple' in body

        if is_quiz and not looks_like_quiz:
            found['quiz not generated as a quiz'].append(qid)

        # The same fault as above, on the way IN. The generator only ever
        # looked at the END phase, so 350 quests asked their questions as a
        # plain conversation and accepted any answer.
        if quiz(nx, start_node) and 'startAsk' not in body:
            found['quiz on START not generated as one'].append(qid)

        # The reward bug: these two mark a quest done or started and skip
        # everything Act.img says should happen.
        if re.search(r'qm\.completeQuest\s*\(', body):
            found['pays nothing (completeQuest)'].append(qid)

        if re.search(r'qm\.startQuest\s*\(', body):
            found['starts nothing (startQuest)'].append(qid)

        # A selection marker outside a quiz script means options are being
        # drawn on a page that only knows how to go "next" - clicking any of
        # them just advances, which is how Robin's quiz behaved.
        # Only counted when it is NOT already reported as a START-phase
        # quiz: those carry their own "#L0#" markers and would be counted
        # twice, which nearly doubled the headline the first time this ran.
        if (not looks_like_quiz and not quiz(nx, start_node)
                and re.search(r'#L\d', body)):
            found['options in a non-quiz page'].append(qid)

        if not lines(nx, start_node) and not lines(nx, end_node):
            found['script is empty of dialogue'].append(qid)

    return found


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--quest-nx', default=os.path.join(
        os.path.expanduser('~'), 'maple', 'wz-v83', 'Quest.nx'))
    ap.add_argument('--scripts', default=os.path.join(
        os.path.expanduser('~'), 'OneDrive', 'Documents', 'Programs', 'Cosmic',
        'scripts', 'quest'))
    ap.add_argument('--show', help='list the quest ids for checks matching this')
    args = ap.parse_args()

    nx = Nx(args.quest_nx)
    say = nx.resolve('Say.img')

    if say is None:
        raise SystemExit('no Say.img in ' + args.quest_nx)

    found = check(nx, say, args.scripts)

    bad = 0

    for what, ids in found.items():
        print('%5d  %s' % (len(ids), what))
        bad += len(ids)

        if args.show and args.show.lower() in what.lower():
            for i in range(0, len(ids), 16):
                print('       ' + ' '.join(ids[i:i + 16]))

    print()

    if bad:
        print('%d generated scripts disagree with what Say.img asks for.' % bad)
    else:
        print('No KNOWN class of mistake in any generated script.')
        print('That is not the same as the quests working - see the header.')

    return 1 if bad else 0


if __name__ == '__main__':
    raise SystemExit(main())
