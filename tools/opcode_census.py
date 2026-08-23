"""Compare the client's packet vocabulary against the server's, both ways.

    python tools/opcode_census.py [--cosmic DIR] [--verbose]

Two questions, neither of which needs the game to be running:

  DEAF   Which packets can Cosmic SEND that this client has no handler for?
         Every one is something the server tells us and we throw away.

  MUTE   Which packets can Cosmic RECEIVE that this client never sends?
         Every one is a feature that cannot be triggered at all - not
         broken, not missing a window, just unreachable. The entire CASH
         tab of the bag was one of these: cash items go out on opcode 79
         and nothing in the client ever wrote that number, so tapping one
         did nothing, forever, in silence.

  DRIFT  Same name on both sides, different number. The worst kind, because
         it works right up until it doesn't.

Names are matched case-insensitively with underscores stripped, so
`CHANGE_CHANNEL` matches `ChangeChannel`. Anything unmatched by name is
still checked by NUMBER, which is what actually goes on the wire.
"""
import argparse
import os
import re
import sys

DEFAULT_COSMIC = "C:/Users/Deck/OneDrive/Documents/Programs/Cosmic"

# Java enum entries: NAME(0x1F), or NAME(31),
JAVA_ENTRY = re.compile(r"^\s*([A-Z][A-Z_0-9]*)\s*\(\s*(0[xX][0-9a-fA-F]+|\d+)\s*\)")

# C++ enum entries: NAME = 31,
CPP_ENTRY = re.compile(r"^\s*([A-Z][A-Z_0-9]*)\s*=\s*(0[xX][0-9a-fA-F]+|\d+)\s*,")

# emplace<OPCODE, SomeHandler>();
EMPLACE = re.compile(r"emplace<\s*([A-Z][A-Z_0-9]*)\s*,")


# Systems this project has decided not to build (docs_SCOPE.md, "Cut"), plus
# the parts of the login flow this client does not use. Without this the
# answer is 340 lines long and nobody reads it twice; with it, it is a work
# queue. `--all` shows everything.
CUT_WORDS = [
    "GUILD", "ALLIANCE", "BBS",
    "BUDDY", "FAMILY", "WEDDING", "MARRIAGE", "ENGAGE", "DIVORCE",
    "MTS", "MERCHANT", "PLAYER_SHOP", "RANKING", "REPORT",
    "MEGAPHONE", "MAPLETV", "AVATAR_MEGAPHONE",
    "PINCODE", "PIC", "VAC", "HACKSHIELD", "CRC", "GUEST",
    "KOREAN", "NEXON", "CLAIM", "MAPLELIFE", "DUEY",
    "MONSTER_LIFE", "MONSTERLIFE", "FARM", "BEANS",
]


def is_cut(name):
    return any(word in name for word in CUT_WORDS)


def norm(name):
    return name.replace("_", "").lower()


def read_java_enum(path):
    out = {}

    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = JAVA_ENTRY.match(line)

            if m:
                out[m.group(1)] = int(m.group(2), 0)

    return out


def read_cpp_enum(path, start_marker, end_marker="};"):
    """The opcode enums are plain blocks in a larger file, so read between
    the marker and the closing brace rather than the whole file - other
    enums live in the same headers."""
    out = {}
    inside = False

    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            if not inside:
                if start_marker in line:
                    inside = True
                continue

            if end_marker in line:
                break

            m = CPP_ENTRY.match(line)

            if m:
                out[m.group(1)] = int(m.group(2), 0)

    return out


def read_registered(path):
    """Which opcode names PacketSwitch actually wires to a handler. An entry
    in the enum is not the same as a handler existing for it."""
    out = set()

    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            for m in EMPLACE.finditer(line):
                out.add(m.group(1))

    return out


def report(title, blurb, rows, verbose, limit=25):
    print("=" * 72)
    print(title + "  -  %d" % len(rows))
    print(blurb)
    print()

    if not rows:
        print("    (none)")
        print()
        return

    shown = rows if verbose else rows[:limit]

    for value, name in shown:
        print("    %5d  0x%04X  %s" % (value, value, name))

    if len(shown) < len(rows):
        print("    ... and %d more (--verbose for all)" % (len(rows) - len(shown)))

    print()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cosmic", default=DEFAULT_COSMIC)
    ap.add_argument("--verbose", action="store_true")
    ap.add_argument("--all", action="store_true",
                    help="include systems docs_SCOPE.md says are cut")
    args = ap.parse_args()

    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    opcodes = os.path.join(args.cosmic, "src/main/java/net/opcodes")

    server_send = read_java_enum(os.path.join(opcodes, "SendOpcode.java"))
    server_recv = read_java_enum(os.path.join(opcodes, "RecvOpcode.java"))

    switch = os.path.join(here, "Net/PacketSwitch.cpp")
    client_in = read_cpp_enum(switch, "enum Opcode")
    registered = read_registered(switch)

    client_out = read_cpp_enum(
        os.path.join(here, "Net/OutPacket.h"), "enum Opcode : uint16_t")

    # An opcode is handled only if it is BOTH in the enum and wired up.
    handled_values = {v for k, v in client_in.items() if k in registered}
    sendable_values = set(client_out.values())

    print()
    print("Cosmic can send %d kinds of packet; we handle %d."
          % (len(server_send), len(handled_values)))
    print("Cosmic can receive %d kinds; we can send %d."
          % (len(server_recv), len(sendable_values)))
    print()

    keep = (lambda n: True) if args.all else (lambda n: not is_cut(n))

    deaf = sorted((v, n) for n, v in server_send.items()
                  if v not in handled_values and keep(n))
    mute = sorted((v, n) for n, v in server_recv.items()
                  if v not in sendable_values and keep(n))

    if not args.all:
        print("Systems docs_SCOPE.md says are cut are hidden; --all shows them.")
        print()

    # Same name on both sides, different number.
    drift = []

    for pairs, ours in ((server_send, client_in), (server_recv, client_out)):
        theirs_by_norm = {norm(n): (n, v) for n, v in pairs.items()}

        for name, value in ours.items():
            match = theirs_by_norm.get(norm(name))

            if match and match[1] != value:
                drift.append((name, value, match[0], match[1]))

    report("DEAF - the server says it, we ignore it",
           "Not all of these matter: plenty are for systems deliberately cut.\n"
           "The ones to look at are in systems we DO have.",
           deaf, args.verbose)

    report("MUTE - the server would answer it, we never ask",
           "A feature that cannot be triggered at all. This is the list the\n"
           "dead cash tab was hiding in.",
           mute, args.verbose)

    print("=" * 72)
    print("DRIFT - same name, different number  -  %d" % len(drift))
    print("Works until it doesn't. Any hit here is a bug.")
    print()

    if not drift:
        print("    (none)")
    else:
        for ourname, ourval, theirname, theirval in sorted(drift):
            print("    %-28s ours %5d  theirs %5d  (%s)"
                  % (ourname, ourval, theirval, theirname))

    print()

    # Non-zero exit on drift only: deaf and mute are a work queue, not a
    # failure, but a number mismatch is always wrong.
    return 1 if drift else 0


if __name__ == "__main__":
    sys.exit(main())
