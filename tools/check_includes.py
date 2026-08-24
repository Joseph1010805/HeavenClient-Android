"""Find #includes that only resolve on a case-insensitive filesystem.

    python tools/check_includes.py

Windows and macOS do not care whether you wrote UIChatbar.h or UIChatBar.h.
Linux does, and the CI runner is Linux - so a mismatch builds perfectly on the
machine it was written on and fails only once it is pushed, which is the worst
possible moment to find out. That is exactly how the v0.7 build broke.

Exits non-zero if anything is wrong, so CI can run it in seconds rather than
discovering the same thing twenty minutes into a native build.
"""
import os
import re
import sys

# Where the project's own headers live. Anything not found under one of these
# is assumed to be a system or third-party include and left alone.
ROOTS = ["Audio", "Character", "Data", "Gameplay", "Graphics", "IO", "Net",
         "Template", "Util"]

INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)


def real_case(path):
    """The path as the filesystem actually spells it, walking down from the
    repo root a component at a time. Returns None if it does not exist."""
    parts = path.replace("\\", "/").split("/")
    here = "."

    for want in parts:
        if want in ("", "."):
            continue

        if want == "..":
            here = os.path.dirname(here) or "."
            continue

        try:
            listing = os.listdir(here)
        except OSError:
            return None

        if want in listing:
            here = os.path.join(here, want)
            continue

        # Present, but spelled differently - the whole point of this check.
        lowered = {n.lower(): n for n in listing}

        if want.lower() in lowered:
            return os.path.join(here, lowered[want.lower()])

        return None

    return here


def main():
    bad = []
    checked = 0

    for root in ROOTS:
        for here, _, files in os.walk(root):
            for name in files:
                if not name.endswith((".cpp", ".h", ".hpp")):
                    continue

                source = os.path.join(here, name)

                with open(source, encoding="utf-8", errors="replace") as f:
                    text = f.read()

                for inc in INCLUDE.findall(text):
                    target = os.path.normpath(os.path.join(here, inc))

                    if os.path.exists(target) and os.path.basename(target) in \
                            os.listdir(os.path.dirname(target) or "."):
                        checked += 1
                        continue

                    actual = real_case(os.path.join(here, inc))

                    if actual is None:
                        # Not one of ours - a system or library header.
                        continue

                    bad.append((source, inc, os.path.basename(actual)))
                    checked += 1

    print("checked %d project includes" % checked)

    if not bad:
        print("all of them resolve exactly - this will build on Linux")
        return 0

    print("\n%d include(s) only resolve on a case-insensitive filesystem:\n" % len(bad))

    for source, inc, actual in bad:
        print("  %s" % source)
        print("      wrote  %s" % inc)
        print("      but it is spelled  %s\n" % actual)

    return 1


if __name__ == "__main__":
    sys.exit(main())
