"""What the SERVER can hand out that the CLIENT cannot draw.

    python tools/asset_audit.py [nx-dir]

The server and the client keep separate copies of the game data - Cosmic reads
`wz/`, the client reads `.nx` files on the device - and nothing checks that
they agree. Where they do not, the failure is silent in the worst way: the
item exists, is given, sits in the inventory, and renders as a blank square or
nothing at all. That is how the face accessories went missing for weeks.

This walks every reward the server's quest data can grant and asks whether the
client has artwork for it.

WHY THE ID RANGES MATTER
------------------------
Equips live in Character.nx under a category folder, one `########.img` per
item, and the folder is chosen by the first two digits of the id: 100 is a
hat, 101 a face accessory, 104 a coat, and so on. Everything else lives in
Item.nx under Consume/Etc/Install/Cash, bucketed four digits at a time.

Getting a category wrong reports the whole category missing, which is why
each one is listed rather than guessed at from the number.
"""
import os
import subprocess
import sys
import xml.etree.ElementTree as ET

COSMIC = "C:/Users/Deck/OneDrive/Documents/Programs/Cosmic"
# Windows-style: this runs under Python, not Git Bash, and "/c/..."
# is not a path Python can open.
NX = "C:/Users/Deck/maple/wz-v83"
HERE = os.path.dirname(os.path.abspath(__file__))

# id prefix -> the Character.nx folder that holds it
EQUIP_DIRS = {
    "100": "Cap",
    "101": "Accessory", "102": "Accessory", "103": "Accessory",
    "104": "Coat", "105": "Longcoat", "106": "Pants",
    "107": "Shoes", "108": "Glove", "109": "Shield",
    "110": "Cape", "111": "Ring", "112": "Accessory",
    "161": "Weapon", "162": "Weapon", "163": "Weapon", "164": "Weapon",
    "165": "Weapon", "166": "Weapon", "167": "Weapon", "168": "Weapon",
    "169": "Weapon", "170": "Weapon",
}


def dump(nx_file, path):
    out = subprocess.run(
        [sys.executable, os.path.join(HERE, "nxdump.py"), nx_file, path],
        capture_output=True, text=True)

    return out.stdout


def children(nx_file, path):
    """The names of a node's children, without their values."""
    lines = dump(nx_file, path).split("\n")[1:]

    return [l.split()[0] for l in lines if l.strip() and not l.startswith("no such")]


def have_equips(nx_dir):
    """Every equip id the client can draw."""
    character = os.path.join(nx_dir, "Character.nx")
    have = set()

    for folder in sorted(set(EQUIP_DIRS.values())):
        for name in children(character, folder):
            if name.endswith(".img") and name[:-4].isdigit():
                have.add(int(name[:-4]))

    return have


def have_mounts(nx_dir):
    """Mounts and their saddles - 190xxxx and 191xxxx.

    Character.nx/TamingMob, not TamingMob.nx. The separate file is keyed by
    mob TYPE - 0001 to 0007 - and holds how a mount MOVES; the folder inside
    Character.nx is keyed by item id and holds how it LOOKS. Reaching for the
    obvious one reported eleven rewards missing that are perfectly present."""
    character = os.path.join(nx_dir, "Character.nx")
    have = set()

    for name in children(character, "TamingMob"):
        if name.endswith(".img") and name[:-4].isdigit():
            have.add(int(name[:-4]))

    return have


def have_items(nx_dir):
    """Every non-equip item id the client can draw."""
    item = os.path.join(nx_dir, "Item.nx")
    have = set()

    for folder in ("Consume", "Etc", "Install", "Cash", "Special"):
        for bucket in children(item, folder):
            for name in children(item, "%s/%s" % (folder, bucket)):
                if name.isdigit():
                    have.add(int(name))

    return have


def quest_rewards():
    """Every item the server's quest data can grant, and which quest grants it."""
    path = os.path.join(COSMIC, "wz", "Quest.wz", "Act.img.xml")
    root = ET.parse(path).getroot()
    given = {}

    for quest in root:
        qid = quest.get("name")

        for branch in quest:
            for node in branch:
                if node.get("name") != "item":
                    continue

                for entry in node:
                    fields = {c.get("name"): c.get("value") for c in entry}
                    iid = fields.get("id", "")
                    count = fields.get("count", "1")

                    # A negative count TAKES the item away rather than giving
                    # it, so the client never has to draw it on that account.
                    if iid.isdigit() and int(count or 1) > 0:
                        given.setdefault(int(iid), set()).add(qid)

    return given


def main():
    nx_dir = sys.argv[1] if len(sys.argv) > 1 else NX

    print("reading what the client can draw...")
    equips = have_equips(nx_dir)
    items = have_items(nx_dir)
    mounts = have_mounts(nx_dir)

    equips |= mounts

    print("  %d equips (incl. %d mounts), %d other items"
          % (len(equips), len(mounts), len(items)))

    given = quest_rewards()
    print("  %d distinct items granted by quests" % len(given))

    missing = []

    for iid, quests in sorted(given.items()):
        prefix = str(iid).zfill(8)[:3] if iid >= 1000000 else None
        is_equip = 1000000 <= iid <= 1999999

        if is_equip:
            if iid not in equips:
                missing.append((iid, quests, EQUIP_DIRS.get(prefix, "?")))
        else:
            if iid not in items:
                missing.append((iid, quests, "item"))

    print()

    if not missing:
        print("Every quest reward has artwork. Nothing to do.")
        return 0

    print("%d reward(s) the server can grant and the client cannot draw:" % len(missing))
    print()

    for iid, quests, where in missing[:60]:
        shown = sorted(quests)[:3]
        more = "" if len(quests) <= 3 else " (+%d more)" % (len(quests) - 3)

        print("  %-9d %-10s quests %s%s" % (iid, where, ", ".join(shown), more))

    if len(missing) > 60:
        print("  ... and %d more" % (len(missing) - 60))

    return 1


if __name__ == "__main__":
    sys.exit(main())
