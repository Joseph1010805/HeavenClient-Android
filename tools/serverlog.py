"""Read what the server has been saying about us.

    python tools/serverlog.py                 # the last 2 hours, digested
    python tools/serverlog.py --since 20      # the last 20 minutes
    python tools/serverlog.py --follow        # live, while somebody plays
    python tools/serverlog.py --kind Cash     # only lines matching a word

Cosmic writes warnings and stack traces to `logs/cosmic-log.log`, and for
the whole life of this port nobody read them. The morning the cash shop was
built it logged

    ERROR CashOperationHandler - Denied to sell cash item with SN 10002319

thirty-four times, once per Buy tap, while the bug was being chased from the
client side by reading code.

The server is the only participant that can see BOTH what we sent and what
the rules are. When something does nothing, this is the first place to look
and it costs nothing to look.

Two minutes of it, the first time it was run, found a live NullPointer in
`UseItemHandler` (the client offers to "use" arrows, which have no item
effect) and four NPCs with no script on the server at all.

Lines are grouped by shape - numbers are replaced with N - so fifty
identical failures are one row with a count, and the rare thing is not
buried under the common one.
"""
import argparse
import os
import re
import sys
import time
from datetime import datetime, timedelta

DEFAULT_LOG = ("C:/Users/Deck/OneDrive/Documents/Programs/Cosmic/"
               "logs/cosmic-log.log")

# 09:15:18.788 [nioEventLoopGroup-3-2] ERROR handlers.Foo - message
LINE = re.compile(
    r"^(\d{2}:\d{2}:\d{2})\.\d{3}\s+\[[^\]]*\]\s+(WARN|ERROR|INFO)\s+(.*)$")

# Anything indented or starting with "at " or a Java class name is part of a
# stack trace belonging to the line above it.
CONT = re.compile(r"^(\s+at |\s*\.\.\. |[a-z][\w.]*(Exception|Error)[:\s])")


def shape(message):
    """Collapse the varying parts so repeats of one failure group together.

    Hex blobs first: a packet dump differs on every line (it carries a
    timestamp) and would otherwise put every occurrence in its own group,
    which is exactly the burial this is meant to prevent."""
    message = re.sub(r"\[[0-9A-Fa-f_]{8,}\]", "[HEX]", message)

    return re.sub(r"\d{2,}", "N", message)


def interesting(level):
    return level in ("WARN", "ERROR")


def parse(path, since_minutes):
    """Returns [(time, level, message, first_cause_line)]."""
    cutoff = None

    if since_minutes:
        now = datetime.now()
        cutoff = (now - timedelta(minutes=since_minutes)).strftime("%H:%M:%S")

    out = []

    with open(path, encoding="utf-8", errors="replace") as f:
        for raw in f:
            raw = raw.rstrip("\n")
            m = LINE.match(raw)

            if m:
                stamp, level, message = m.groups()

                if not interesting(level):
                    continue

                # Times only, no date, so a cutoff that wraps midnight would
                # be wrong. Good enough for "what just happened".
                if cutoff and stamp < cutoff:
                    continue

                out.append([stamp, level, message, ""])
                continue

            # A cause line for whatever came last - the first one is the
            # useful one, the rest is netty.
            if out and not out[-1][3] and CONT.match(raw):
                out[-1][3] = raw.strip()

    return out


def digest(rows):
    groups = {}

    for stamp, level, message, cause in rows:
        key = (level, shape(message), shape(cause))
        entry = groups.setdefault(key, {"n": 0, "first": stamp, "last": stamp,
                                        "sample": message, "cause": cause})
        entry["n"] += 1
        entry["last"] = stamp

        if cause and not entry["cause"]:
            entry["cause"] = cause

    ordered = sorted(groups.items(), key=lambda kv: -kv[1]["n"])

    if not ordered:
        print("Nothing. The server has had no complaints in this window.")
        return

    print("%d complaint(s), %d distinct:" % (len(rows), len(ordered)))
    print()

    for (level, _, _), e in ordered:
        span = e["first"] if e["first"] == e["last"] else \
            "%s-%s" % (e["first"], e["last"])

        print("  %-5s x%-4d %s" % (level, e["n"], span))
        print("        %s" % e["sample"])

        if e["cause"]:
            print("        cause: %s" % e["cause"])

        print()


def follow(path, kind):
    """Live. Only the header lines - a stack trace live is unreadable."""
    print("Watching %s. Ctrl-C to stop." % path)
    print()

    with open(path, encoding="utf-8", errors="replace") as f:
        f.seek(0, os.SEEK_END)

        while True:
            line = f.readline()

            if not line:
                time.sleep(0.4)
                continue

            m = LINE.match(line.rstrip("\n"))

            if not m:
                continue

            stamp, level, message = m.groups()

            if not interesting(level):
                continue

            if kind and kind.lower() not in message.lower():
                continue

            print("%s  %-5s %s" % (stamp, level, message))
            sys.stdout.flush()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--log", default=DEFAULT_LOG)
    ap.add_argument("--since", type=int, default=120,
                    help="minutes back to read (default 120; 0 for all)")
    ap.add_argument("--follow", action="store_true")
    ap.add_argument("--kind", default="",
                    help="only lines containing this word")
    args = ap.parse_args()

    if not os.path.exists(args.log):
        raise SystemExit("no log at %s - is the server running?" % args.log)

    if args.follow:
        try:
            follow(args.log, args.kind)
        except KeyboardInterrupt:
            print()
        return

    rows = parse(args.log, args.since)

    if args.kind:
        rows = [r for r in rows if args.kind.lower() in r[2].lower()]

    digest(rows)


if __name__ == "__main__":
    main()
