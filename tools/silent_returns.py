"""WHERE THE SERVER GIVES UP WITHOUT SAYING SO.

    python tools/silent_returns.py                # the count, worst first
    python tools/silent_returns.py --show Quest   # the actual lines

WHY THIS EXISTS
---------------
Every fault found on 1-2 September 2026 was the same shape: a handler decided
not to do something and told nobody. Seven of them, all in one evening:

    the installer died on line 65             no message
    the data push failed 17 times             adb's reason thrown away
    the APK install was refused               reason thrown away
    the new jar was not installed             timestamps, no message
    the server did not restart                "already running", exit 0
    2,392 quest scripts discarded unread      a gate, no message
    1,217 NPCs said nothing                   a log line nobody read

and then five more in one flow, hunting a single quest: `QuestDialogue`,
`isNpcNearby`, and cases 1, 2 and 5 of `QuestActionHandler` - each an early
`return` or an `if` with no `else`.

So this counts them. A `return;` in a packet handler with no log line, no
dropMessage and no packet in the three lines above it is a place where a
player pressed something and the game did nothing at all.

WHAT THE NUMBER IS NOT
----------------------
It is not a list of bugs. Most of these are correct: guards against malformed
packets, anti-cheat checks, requests from a client in the wrong state. Nobody
should be told about those, and adding a message to all 219 would be worse
than leaving them.

The ones that matter are on paths a PLAYER walks - pressing a button, clicking
an NPC, picking something up. Those are what the ranking is for: the handlers
at the top of the list are the ones a nine-year-old touches most.
"""
import argparse
import os
import re


HANDLERS = os.path.join('src', 'main', 'java', 'net', 'server', 'channel', 'handlers')

# Anything in the few lines above a return that means "somebody was told".
SPOKE = ('log.', 'dropmessage', 'sendpacket', 'enableactions', 'message(',
         'announce', 'showinfo')


def scan(path, look_back=3):
    """Every silent `return;` in one file, as (line number, the line)."""
    with open(path, encoding='utf-8', errors='replace') as f:
        lines = f.read().split('\n')

    found = []

    for i, line in enumerate(lines):
        if not re.match(r'\s*return;\s*$', line):
            continue

        back = '\n'.join(lines[max(0, i - look_back):i]).lower()

        if not any(word in back for word in SPOKE):
            found.append((i + 1, line.strip()))

    return found


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--cosmic', default=os.path.join(
        os.path.expanduser('~'), 'OneDrive', 'Documents', 'Programs', 'Cosmic'))
    ap.add_argument('--show', help='print the lines for handlers matching this')
    args = ap.parse_args()

    root = os.path.join(args.cosmic, HANDLERS)

    if not os.path.isdir(root):
        raise SystemExit('no handlers at ' + root)

    rows = []

    for name in sorted(os.listdir(root)):
        if not name.endswith('.java'):
            continue

        hits = scan(os.path.join(root, name))

        if hits:
            rows.append((len(hits), name, hits))

    rows.sort(reverse=True)

    total = sum(r[0] for r in rows)

    for count, name, hits in rows:
        if args.show and args.show.lower() not in name.lower():
            continue

        print('%4d  %s' % (count, name))

        if args.show:
            for line_no, text in hits:
                print('        %s:%d  %s' % (name, line_no, text))

    print()
    print('%d silent returns across %d handlers.' % (total, len(rows)))
    print()
    print('Most are correct - guards against bad packets. The ones that matter')
    print('are on paths a player walks; those are the handlers at the top.')

    return 0


if __name__ == '__main__':
    raise SystemExit(main())
