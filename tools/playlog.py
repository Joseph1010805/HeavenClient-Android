"""WHAT WENT WRONG WHILE THEY WERE PLAYING, READ BACK AFTERWARDS.

    python tools/playlog.py                # every device, digested
    python tools/playlog.py --raw          # the lines themselves, in order
    python tools/playlog.py --clear        # digest, then start a fresh log
    python tools/playlog.py --since 30     # only the last 30 minutes

WHY THIS EXISTS
---------------
The client used to report its silent failures to logcat, which is a ring
buffer a few thousand lines deep. By the time a session ended and somebody
said "the quiz was all over the place", the evidence had already scrolled
away - and the person who saw the bug is nine and across the room from the
PC.

`Util/Silent.cpp` now also appends every one of those to a file inside the
app's own external folder, which survives the app closing, the device
rebooting, and a week of not being asked about:

    /sdcard/Android/data/org.heavenclient.android/files/HeavenClient/playlog.txt

This pulls that back, from every device plugged in, and groups it the way
`serverlog.py` groups the server's: numbers collapsed to N, so fifty
identical failures are one row with a count and the rare thing is not buried
under the common one.

It also picks up the SERVER's log if a device is carrying one - the machine
in Termux is usually one of the same handhelds - because half of any bug
here is visible only from the other side.

WHAT IT CANNOT DO
-----------------
It reports what the code THOUGHT was worth reporting. A subsystem that does
something coherent and wrong - a quiz that accepts every answer - passes
through here in total silence. That class of bug is what `quest_lint.py` and
`npc_audit.py` are for.
"""
import argparse
import os
import re
import subprocess
import sys
from datetime import datetime, timedelta

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from serverlog import LINE, interesting, shape                  # noqa: E402

PACKAGE = 'org.heavenclient.android'

CLIENT_LOG = ('/sdcard/Android/data/%s/files/HeavenClient/playlog.txt'
              % PACKAGE)

# Where tools/bootstrap.sh puts the server's own output when it is running on
# a handheld. Absent on a device that is only ever a client.
SERVER_LOGS = [
    '/sdcard/Download/cosmic/bootstrap.log',
    '/sdcard/Download/cosmic/server-warn.log',
    '/sdcard/Download/cosmic/logs/cosmic-log.log',
]

# WHAT A PLAYER TYPED @bug ABOUT, written by ReportBugCommand.
#
# NEVER digested. Everything else here is grouped by shape because fifty
# identical failures are one fact - but two people reporting "the quiz is
# broken" from two different maps are two different bugs, and collapsing them
# would throw away the half that makes a report worth having.
BUG_FILE = '/sdcard/Download/cosmic/bugs.txt'

# 09-02 01:35:12 CLIENT NpcTalk: no such thing
STAMPED = re.compile(r'^(\d{2}-\d{2} \d{2}:\d{2}:\d{2})\s+(.*)$')


def find_adb():
    """adb, wherever it is.

    This is the FOURTH script here to need this - install.sh, deploy_data.sh
    and stage_server.sh all grew it after failing on a machine where the SDK
    is installed but not on PATH, which is the normal state of a Windows box.
    Writing it a fifth time from scratch would be the same mistake again.
    """
    from shutil import which

    found = which('adb')

    if found:
        return found

    guesses = [
        os.path.join(os.environ.get('LOCALAPPDATA', ''),
                     'Android', 'Sdk', 'platform-tools'),
        os.path.join(os.path.expanduser('~'),
                     'AppData', 'Local', 'Android', 'Sdk', 'platform-tools'),
        os.path.join(os.environ.get('ANDROID_HOME', ''), 'platform-tools'),
    ]

    for folder in guesses:
        for name in ('adb.exe', 'adb'):
            candidate = os.path.join(folder, name)

            if os.path.isfile(candidate) and os.access(candidate, os.X_OK):
                return candidate

    raise SystemExit(
        'adb is not on PATH and is not in the usual place.\n'
        'Install Android platform-tools, or add it to PATH:\n'
        '  https://developer.android.com/tools/releases/platform-tools')


ADB = None


def adb(*args, serial=None):
    """One adb call. Its stderr is KEPT, never thrown away.

    Every silent failure this project has had began with somebody discarding
    what a tool said - see tools/deploy_data.sh.
    """
    global ADB

    if ADB is None:
        ADB = find_adb()

    cmd = [ADB]

    if serial:
        cmd += ['-s', serial]

    cmd += list(args)

    done = subprocess.run(cmd, capture_output=True, text=True,
                          errors='replace')

    return done.returncode, done.stdout, done.stderr


def devices():
    """Serial -> friendly name, for everything actually connected."""
    _, out, _ = adb('devices')
    found = {}

    for line in out.splitlines()[1:]:
        parts = line.split()

        if len(parts) >= 2 and parts[1] == 'device':
            serial = parts[0]
            _, model, _ = adb('shell', 'getprop', 'ro.product.model',
                              serial=serial)
            found[serial] = model.strip() or serial

    return found


def read(serial, path):
    """A file off the device.

    Returns (text, problem). Exactly one of them is set - a caller that gets
    no text is TOLD WHY, and prints it. Returning a bare None here is what
    hid the client log for a whole session: the file was sitting on the
    device, 2,640 bytes of it, and this tool said nothing at all.

    TWO WAYS IN, because one is not enough on a modern Android:

      * `adb shell cat` works for anything world-readable, like the server
        logs parked in /sdcard/Download.
      * The app's OWN folder under /sdcard/Android/data is NOT world
        readable on Android 11 and later - the file is 0660 owned by the
        app, and `shell` is refused. `run-as` borrows the app's identity and
        can read it, which works because these are debuggable builds.

    `adb pull` is no use for either: it fails with the same Permission
    denied, and it writes into the working directory, which Git Bash then
    rewrites.
    """
    code, out, err = adb('shell', 'cat', path, serial=serial)

    if code == 0 and 'No such file' not in err and 'Permission denied' not in err:
        return out, None

    if 'No such file' in err:
        return None, None          # Simply absent. Not a fault worth naming.

    # Refused - try again as the app itself.
    code, out, err2 = adb('shell', 'run-as', PACKAGE, 'cat', path,
                          serial=serial)

    if code == 0 and 'Permission denied' not in err2 and 'not debuggable' not in err2:
        return out, None

    if 'No such file' in err2:
        return None, None

    return None, (err2 or err).strip() or 'could not be read'


def recent(lines, minutes):
    """Only the lines stamped within the last `minutes`. All of them if 0."""
    if not minutes:
        return lines

    cutoff = datetime.now() - timedelta(minutes=minutes)
    kept = []

    for line in lines:
        match = STAMPED.match(line)

        if not match:
            # A continuation line belongs to whatever it follows.
            if kept:
                kept.append(line)
            continue

        try:
            when = datetime.strptime(match.group(1), '%m-%d %H:%M:%S')
            when = when.replace(year=datetime.now().year)
        except ValueError:
            kept.append(line)
            continue

        if when >= cutoff:
            kept.append(line)

    return kept


def complaints_only(lines):
    """Just the WARN and ERROR out of a Cosmic log.

    The first real run of this tool printed forty lines of "Client connected
    to login server" and "Attempting to save chr" - DEBUG and INFO chatter -
    with the actual complaints nowhere in sight. `serverlog.py` has always
    filtered these; not reusing that was the whole mistake.

    A log that does not match Cosmic's line format at all (bootstrap.log is
    plain shell output) is passed through untouched - there are no levels in
    it to filter by.
    """
    levelled = [l for l in lines if LINE.match(l)]

    if not levelled:
        return lines, False

    kept = []

    for line in lines:
        match = LINE.match(line)

        if match:
            kept.append(line) if interesting(match.group(2)) else None
        elif kept:
            # A stack trace belongs to the line it follows.
            kept.append(line)

    return kept, True


def digest(lines):
    """Grouped by shape, commonest last so the tail of the screen is the
    thing you are most likely to be looking for."""
    groups = {}

    for line in lines:
        match = STAMPED.match(line)
        message = match.group(2) if match else line.strip()

        if not message:
            continue

        key = shape(message)
        first, count, example = groups.get(key, (None, 0, message))
        stamp = match.group(1) if match else None

        groups[key] = (first or stamp, count + 1, example)

    return sorted(groups.values(), key=lambda row: row[1])


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--raw', action='store_true',
                    help='the lines themselves, not a digest')
    ap.add_argument('--clear', action='store_true',
                    help='start a fresh log once this one has been read')
    ap.add_argument('--since', type=int, default=0, metavar='MINS',
                    help='only the last MINS minutes')
    ap.add_argument('--serial', help='one device, by adb serial')
    args = ap.parse_args()

    found = devices()

    if args.serial:
        found = {k: v for k, v in found.items() if k == args.serial}

    if not found:
        raise SystemExit('no device connected - plug one in, or check '
                         '`adb devices`')

    anything = False

    for serial, name in found.items():
        print()
        print('== %s  (%s)' % (name, serial))

        # PEOPLE FIRST. What somebody took the trouble to type is worth more
        # than anything the machine noticed on its own, so it goes at the top
        # of the device's section and is never summarised.
        reports, problem = read(serial, BUG_FILE)

        if problem:
            print('   player reports could not be read: %s' % problem)

        if reports and reports.strip():
            blocks = [b for b in reports.split('---- ') if b.strip()]
            anything = True

            print()
            print('   WHAT PLAYERS REPORTED - %d' % len(blocks))

            for block in blocks:
                print()

                for line in ('---- ' + block).rstrip().splitlines():
                    print('     ' + line)

        for label, path in ([('the client', CLIENT_LOG)] +
                            [('the server', p) for p in SERVER_LOGS]):
            text, problem = read(serial, path)

            if problem:
                # NEVER SILENTLY. A log that cannot be read is a different
                # fact from a log with nothing in it, and this tool exists
                # precisely because those two were once indistinguishable.
                print('   %s (%s): could not be read - %s'
                      % (label, os.path.basename(path), problem))
                anything = True
                continue

            if text is None:
                continue

            lines = recent([l for l in text.splitlines() if l.strip()],
                           args.since)

            # A levelled log is worth reading only for its complaints.
            lines, levelled = complaints_only(lines)

            if not lines:
                if levelled:
                    print('   %s (%s): nothing to complain about'
                          % (label, os.path.basename(path)))
                else:
                    print('   %s: nothing since %d minutes ago' %
                          (label, args.since) if args.since
                          else '   %s: log is empty' % label)
                continue

            anything = True
            print()
            print('   %s - %s%s' % (label, os.path.basename(path),
                                    '  (WARN and ERROR only)' if levelled else ''))

            if args.raw:
                for line in lines:
                    print('     ' + line)

                continue

            rows = digest(lines)

            for first, count, example in rows:
                head = ('%4d x  ' % count) if count > 1 else '        '
                print('   %s%s' % (head, example[:150]))

                if count > 1 and first:
                    print('           first seen %s' % first)

            # NOT "failures". bootstrap.log is mostly ordinary progress, and
            # calling "old server stopped" a failure is the sort of wrong
            # label that makes a tool untrustworthy on the one day it matters.
            print('   %d line(s), %d distinct' % (len(lines), len(rows)))

        if args.clear:
            # Truncate rather than delete: the app holds no handle to it
            # between writes, but a file it can no longer create is worse
            # than a file with nothing in it.
            adb('shell', 'sh', '-c', "':> %s'" % CLIENT_LOG, serial=serial)
            print()
            print('   client log cleared')

    print()

    if not anything:
        print('Nothing was reported. That means nothing OBJECTED - it does')
        print('not mean nothing went wrong. See quest_lint.py and npc_audit.py.')

    return 0


if __name__ == '__main__':
    raise SystemExit(main())
