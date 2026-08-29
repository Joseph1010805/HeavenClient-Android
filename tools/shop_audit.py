"""Check every item the cash shop offers against the data that has to draw it.

    python tools/shop_audit.py [--wz DIR] [--verbose]

The shop lists whatever Etc.nx/Commodity.img marks OnSale and the client can
find an `info` node for. That is a weaker test than it looks: an item can have
an icon, a name and a price - so it shows a perfectly ordinary card, takes the
money and lands in the locker - and still have nothing that the character
renderer can draw, in which case wearing it does nothing at all and there is
no way to tell from inside the game.

That is what happened with a mask. Face accessories are keyed by EXPRESSION in
Character.nx (`default`, `blink`, `smile`), while `Clothing` loads art by
STANCE (`stand1`, `walk1`), so it found no stances, stored nothing, and drew
nothing. Bought, taken out, equipped, invisible.

So this asks four questions of every entry:

  ICON      does the item have an info node at all? (the shop's own filter)
  NAME      does String.nx name it? A card with a blank label is not sellable.
  ART       is there anything under a name `Clothing` looks for?
  SLOT      does the leading digit group map to a slot the renderer draws?

Anything it reports as BROKEN should not be on sale.
"""
import argparse
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from nxdump import Nx  # noqa: E402

# Exactly the names Clothing.cpp iterates (Stance::names, minus the empty one).
# What a face accessory is drawn against. The client picks one of these by
# the character's current expression; there is no stand1 on a mask.
EXPRESSIONS = {
    "default", "blink", "smile", "angry", "sad", "cry", "hum", "despair",
    "oops", "stunned", "vomit", "troubled", "cheers", "chu", "wink",
    "pain", "glitter", "love", "shine", "blaze", "bewildered", "hot",
    "dam", "qBlue",
}

STANCES = {
    "alert", "dead", "fly", "heal", "jump", "ladder", "prone", "proneStab",
    "rope", "shot", "shoot1", "shoot2", "shootF", "sit", "stabO1", "stabO2",
    "stabOF", "stabT1", "stabT2", "stabTF", "stand1", "stand2", "swingO1",
    "swingO2", "swingO3", "swingOF", "swingP1", "swingP2", "swingPF",
    "swingT1", "swingT2", "swingT3", "swingTF", "walk1", "walk2",
}

# ItemData::get_eqcategory - the folder under Character.nx for each group.
EQ_CATEGORY = [
    "Cap", "Accessory", "Accessory", "Accessory", "Coat", "Longcoat",
    "Pants", "Shoes", "Glove", "Shield", "Cape", "Ring", "Accessory",
    "Accessory", "Accessory",
]

# What the player sees the tab called, by group.
SLOT_NAME = {
    100: "hat", 101: "face acc", 102: "eye acc", 103: "earring",
    104: "top", 105: "overall", 106: "bottom", 107: "shoes",
    108: "glove", 109: "shield", 110: "cape", 111: "ring",
    112: "pendant", 113: "belt", 114: "medal",
}

# Slots CharLook actually draws. A ring or a medal carries no character art in
# this version and is not meant to - only the ones in here are worth asking
# about, or every ring in the shop reports a fault it does not have.
DRAWN = {100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110}


def read_int(nx, index):
    if index is None:
        return None
    _, _, _, ntype, payload = nx.node(index)
    if ntype != 1:
        return None
    return struct.unpack("<q", payload)[0]


def child_names(nx, index):
    _, first, num, _, _ = nx.node(index)
    return {nx.string(nx.node(i)[0]) for i in range(first, first + num)}


def eq_category(itemid):
    index = itemid // 10000 - 100
    if 0 <= index < len(EQ_CATEGORY):
        return EQ_CATEGORY[index]
    if 30 <= index <= 70:
        return "Weapon"
    return None


def audit(wz, verbose):
    etc = Nx(os.path.join(wz, "Etc.nx"))
    character = Nx(os.path.join(wz, "Character.nx"))
    item = Nx(os.path.join(wz, "Item.nx"))
    string = Nx(os.path.join(wz, "String.nx"))

    commodity = etc.resolve("Commodity.img")
    if commodity is None:
        raise SystemExit("no Commodity.img in Etc.nx")

    _, first, num, _, _ = etc.node(commodity)

    rows = []

    for i in range(first, first + num):
        entry = i

        if read_int(etc, etc.child(entry, "OnSale")) != 1:
            continue

        itemid = read_int(etc, etc.child(entry, "ItemId"))
        sn = read_int(etc, etc.child(entry, "SN"))
        price = read_int(etc, etc.child(entry, "Price"))

        if itemid is None:
            continue

        group = itemid // 10000
        strid = "0%d" % itemid
        faults = []

        # `listed` mirrors ItemData::is_valid(), which is the only filter the
        # shop applies today: no info node, no card. Anything that fails here
        # is already invisible and is not a problem to be fixed.
        if itemid >= 1000000 and itemid < 2000000:
            category = eq_category(itemid)
            node = None
            listed = False

            if category:
                node = character.resolve("%s/%s.img" % (category, strid))
                listed = character.resolve(
                    "%s/%s.img/info" % (category, strid)) is not None

            if node is not None and group in DRAWN:
                names = child_names(character, node)

                # Face accessories are keyed by EXPRESSION, not by stance.
                #
                # This tool found 84 "broken" items on its first run and it was
                # right - 79 of them were one bug, face accessories being drawn
                # against stand1/walk1 which they do not have. That bug was
                # fixed in the client; this check was not updated with it, so
                # it went on reporting the same 79 items as broken while they
                # rendered perfectly. A tool that cries wolf is worse than no
                # tool, because the real fault hides in the noise.
                #
                # The giveaway was in its own output: "has angry, bewildered,
                # blaze" is a complete set of expressions, not missing art.
                wanted = EXPRESSIONS if group == 101 else STANCES
                art = names - {"info"}

                # AN EQUIP WITH NO ART AT ALL IS THE TRANSPARENT SET, AND IT
                # IS WORKING.
                #
                # Transparent Hat, Earrings, Shoes, Gloves and Cape exist so a
                # player can wear a thing for its stats and show nothing - the
                # game's own description says "if you want to use the abilities
                # available through hats, yet still want to show off your
                # hairstyle". Drawing nothing IS the product.
                #
                # This tool called all five broken, which is the second time it
                # has reported a whole category of working items - and the same
                # mistake as the face accessories: asking whether the art is
                # where THIS tool expects, rather than what the item is for.
                #
                # The distinction that matters is structural, not a list of
                # ids. Nothing but `info` means somebody deliberately shipped an
                # empty equip. SOME art, but none of it under a name the
                # renderer looks for, means art that was meant to be seen and
                # will not be - which is the real fault, and is exactly what a
                # mask keyed by expression looked like.
                if not art:
                    pass
                elif not (names & wanted):
                    # Say what it DOES have - that is the whole diagnosis.
                    faults.append("no %s art (has %s)"
                                  % ("expression" if group == 101 else "stance",
                                     ", ".join(sorted(art)[:3])))

            # A hat can have every stance, every bitmap and every origin and
            # still never be drawn, because the RENDERER never asks for it.
            #
            # CharEquips::getcaptype() used to compare the vslot against three
            # literal strings and answer NONE to everything else, and CharLook's
            # NONE branch draws the hair and never calls equips.draw(HAT, ...).
            # 144 of 908 hats, 60 of them on sale, were loaded and equipped and
            # invisible - and this tool passed every one of them, because it
            # only ever asked whether the ART existed.
            #
            # That is the lesson worth keeping: "the data is present" and "the
            # client will draw it" are different questions, and this file exists
            # to ask the second one. Checked here rather than assumed fixed, so
            # a future rewrite of getcaptype() that drops a spelling says so.
            if group == 100 and node is not None:
                vslot = character.resolve("%s/%s.img/info/vslot" % (category, strid))

                if vslot is None:
                    faults.append("no vslot - the renderer cannot place it")
                else:
                    _, _, _, ntype, payload = character.node(vslot)
                    text = (character.value(ntype, payload).strip("'")
                            if ntype == 3 else "")

                    # Mirrors captype_from_vslot in CharEquips.cpp: a vslot is a
                    # run of two-character tokens, and the client needs at least
                    # the cap's own slot in there to know what it is looking at.
                    tokens = re.findall(r"[A-Z][^A-Z]*", text)

                    if "Cp" not in tokens:
                        faults.append("vslot %r names no cap slot" % text)

            if listed and string.resolve(
                    "Eqp.img/Eqp/%s/%d/name" % (category, itemid)) is None:
                faults.append("unnamed")

            slot = SLOT_NAME.get(group, "group %d" % group)
        else:
            prefix = itemid // 1000000
            folder = {2: "Consume", 3: "Install", 4: "Etc", 5: "Cash"}.get(prefix)
            node = None

            if folder:
                node = item.resolve("%s/0%d.img/%s/info" % (folder, group, strid))

            listed = node is not None
            slot = folder.lower() if folder else "group %d" % group

            if listed:
                strfile = {2: "Consume.img", 3: "Ins.img",
                           4: "Etc.img/Etc", 5: "Cash.img"}[prefix]

                if string.resolve("%s/%d/name" % (strfile, itemid)) is None:
                    faults.append("unnamed")

        if not listed:
            continue

        rows.append((itemid, sn, price, slot, faults))

    broken = [r for r in rows if r[4]]
    ok = [r for r in rows if not r[4]]

    print("On sale and shown in the shop: %d" % len(rows))
    print("  fine:   %d" % len(ok))
    print("  broken: %d" % len(broken))
    print()

    by_fault = {}

    for itemid, sn, price, slot, faults in broken:
        key = faults[0].split(" (")[0]
        by_fault.setdefault(key, []).append((itemid, sn, price, slot, faults))

    for fault in sorted(by_fault, key=lambda k: -len(by_fault[k])):
        entries = by_fault[fault]
        print("%-24s %4d item(s)" % (fault, len(entries)))

        shown = entries if verbose else entries[:8]

        for itemid, sn, price, slot, faults in shown:
            print("    %8d  SN %-9s %-10s %s"
                  % (itemid, sn, slot, "; ".join(faults)))

        if not verbose and len(entries) > len(shown):
            print("    ... and %d more" % (len(entries) - len(shown)))

        print()

    if not verbose:
        return

    print("By slot, of the ones that work:")
    counts = {}

    for itemid, sn, price, slot, faults in ok:
        counts[slot] = counts.get(slot, 0) + 1

    for slot in sorted(counts, key=lambda s: -counts[s]):
        print("  %-12s %d" % (slot, counts[slot]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wz", default="C:/Users/Deck/maple/wz-v83")
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    audit(args.wz, args.verbose)


if __name__ == "__main__":
    main()
