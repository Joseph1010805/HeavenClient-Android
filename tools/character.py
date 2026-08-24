"""Carry ONE character between worlds, so it can be played anywhere.

    python tools/character.py where <place> [<place>]      what is where
    python tools/character.py account <who> <from> <to>    take a PLAYER with you
    python tools/character.py move <name> <from> <to>      take one character
    python tools/character.py verify <name> <a> <b>        prove it arrived whole

`from` and `to` are `pc` or a device serial (`adb devices`).

WHY THIS AND NOT sync_world.sh
------------------------------
`sync_world.sh` moves a whole world and refuses to do it in the wrong
direction, which is right when only one place is played at a time. It cannot
help with the day this house actually has:

    the Thor joins somebody for two hours
    the RP5 hosts alone for four
    the Quest hosts with friends for another two
    in the evening all three play together

Three worlds, all changed, none of them merge-able. But nothing NEEDS to
merge: each of those is a DIFFERENT character. Nobody's progress collided -
it just ended up in three places. So the answer is not to reconcile worlds,
it is to gather characters.

WHAT A CHARACTER IS
-------------------
More than its row. Discovered from the schema rather than written down here,
so it cannot drift when the server is updated:

  the `characters` row              level, exp, position, look, mesos
  ~20 tables keyed by characterid   skills, keymap, quests, inventory,
                                    saved locations, macros, fame, medals
  `inventoryequipment`              hangs off inventory ITEMS, not the
                                    character - the second level that a
                                    naive copy silently loses, taking every
                                    scroll and stat on every equip with it
  the `accounts` row                a character with no account cannot be
                                    logged into

NEWEST WINS
-----------
`lastLogoutTime` says when a character was last put down. Moving a copy over
a NEWER one is refused, because that is somebody's evening. --force overrides.
"""
import argparse
import subprocess
import sys

MYSQL = "C:/Users/Deck/mysql/mysql-8.4.6-winx64/bin/mysql.exe"
DB = "cosmic"
TERMUX = "/data/data/com.termux/files"


class World:
    """One database, on the PC or on a handheld."""

    def __init__(self, where):
        self.where = where
        self.is_pc = (where == "pc")

    def __str__(self):
        return "the PC" if self.is_pc else self.where

    def sql(self, statement, tabbed=True):
        """Run SQL and return the rows. Errors are raised, not swallowed -
        a half-applied character is worse than a refused one."""
        if self.is_pc:
            args = [MYSQL, "-uroot", DB, "-B"] + (["-N"] if tabbed else [])
            done = subprocess.run(args, input=statement, capture_output=True,
                                  text=True, encoding="utf-8", errors="replace")
        else:
            # Through run-as, which works because Termux ships a debug build.
            # The statement goes in on stdin so no quoting of it survives to
            # be mangled by three layers of shell.
            inner = ("export PATH=%s/usr/bin:$PATH; "
                     "mariadb -u root %s -B %s") % (TERMUX, DB, "-N" if tabbed else "")

            done = subprocess.run(
                ["adb", "-s", self.where, "shell", "run-as com.termux sh -c '%s'" % inner],
                input=statement, capture_output=True, text=True,
                encoding="utf-8", errors="replace")

        if done.returncode != 0:
            raise SystemExit("SQL failed on %s:\n%s" % (self, done.stderr.strip()))

        # Strip NEWLINES only, never whitespace.
        #
        # A tabbed row whose last column is an empty string ends in a tab, and
        # a plain .strip() eats it - so the final row of every table came back
        # one field short and zip() silently dropped its last COLUMN. That is
        # how `giftFrom` went missing from an insert: not an error in the SQL,
        # a field quietly deleted on the way out of the source.
        text = done.stdout.replace("\r", "").strip("\n")

        return [line.split("\t") for line in text.split("\n") if line]

    def one(self, statement):
        rows = self.sql(statement)

        return rows[0][0] if rows and rows[0] else None


def quote(value):
    """A single value, as SQL. NULL stays NULL rather than becoming the
    four-letter string, which is the classic way to corrupt a copy."""
    if value is None or value == "NULL":
        return "NULL"

    return "'" + value.replace("\\", "\\\\").replace("'", "\\'") + "'"


# Tables that hang off ANOTHER ROW of the character's data rather than off the
# character directly, and the column that points at their parent.
#
# Cosmic loads quest progress by looking the parent up in a map keyed on
# `queststatusid` - so a progress row carrying the id it had in the world it
# came from finds nothing there, and `loadedQuestStatus.get(...)` quietly
# returns null. The quest stays STARTED and "12 of 20 killed" is simply gone.
# `medalmaps` is loaded the same way, and `inventorymerchant` points at an
# item the same way `inventoryequipment` does.
#
# Nothing in `information_schema` describes any of this - the schema declares
# foreign keys for famelog and skills but not for a single one of these - so
# it is the one thing here that has to be written down.
SECOND_LEVEL = {
    "inventoryitems": [("inventoryequipment", "inventoryitemid"),
                       ("inventorymerchant", "inventoryitemid")],
    "queststatus": [("questprogress", "queststatusid"),
                    ("medalmaps", "queststatusid")],
}

# Anything reached through SECOND_LEVEL must not ALSO be carried by the
# character loop, or it arrives twice. Most of them do carry a characterid as
# well - which still has to be set, just not used to find them.
NESTED = {child for kids in SECOND_LEVEL.values() for child, _ in kids}

# `worldtransfers` and `namechanges` are records of administrative acts on a
# particular server. They are not part of who the character IS, and carrying
# them would claim a history that did not happen here. `characterexplogs` is
# logging and grows without bound; `playerdiseases` are temporary debuffs
# nobody should keep across a move; the family tables describe a relationship
# to characters that are not coming along.
SKIP = {"worldtransfers", "namechanges", "mts_cart",
        "family_character", "family_entitlement",
        "characterexplogs", "playerdiseases"}


def child_tables(world):
    """Every table that hangs off a character, read from the schema.

    Cosmic spells the key THREE ways - `characterid`, `cid` and `charid` -
    and looking for only the first two silently left behind the monster book,
    skill cooldowns, and `area_info`, which is where NPC scripts keep their
    per-character memory."""
    rows = world.sql(
        "SELECT table_name, column_name FROM information_schema.columns "
        "WHERE table_schema='%s' AND column_name IN "
        "('characterid','cid','charid') ORDER BY table_name;" % DB)

    return [(t, c) for t, c in rows if t not in SKIP and t not in NESTED]


def key_of(world, table, cache={}):
    """A table's own primary key column."""
    if (str(world), table) not in cache:
        got = world.sql(
            "SELECT column_name FROM information_schema.columns "
            "WHERE table_schema='%s' AND table_name='%s' AND column_key='PRI' "
            "ORDER BY ordinal_position;" % (DB, table))

        cache[(str(world), table)] = got[0][0] if got and got[0] else None

    return cache[(str(world), table)]


def columns(world, table):
    rows = world.sql(
        "SELECT column_name FROM information_schema.columns "
        "WHERE table_schema='%s' AND table_name='%s' ORDER BY ordinal_position;"
        % (DB, table))

    return [r[0] for r in rows]


def rows_of(world, table, where):
    cols = columns(world, table)
    got = world.sql("SELECT %s FROM `%s` WHERE %s;"
                    % (", ".join("`%s`" % c for c in cols), table, where))

    rows = []

    for r in got:
        # A row can only ever be SHORT, and only by trailing empty strings.
        # Anything else means the values themselves contain tabs or newlines
        # and this whole transport is wrong for the data - say so rather than
        # carrying a character across with its fields shifted by one.
        if len(r) > len(cols):
            raise SystemExit(
                "%s.%s: a value contains a tab - cannot copy this safely" % (world, table))

        r = r + [""] * (len(cols) - len(r))

        # A tabbed row of NULL comes back as the literal \N.
        rows.append([None if v == "\\N" else v for v in r])

    return cols, rows


def shared(source_cols, dest, table, cache={}):
    """Only the columns BOTH sides have.

    The PC and the handhelds were set up months apart, so their Liquibase
    changelogs are not necessarily at the same revision. A column the
    destination has never heard of makes the whole insert fail; one it has and
    the source does not is left to its own default."""
    if (str(dest), table) not in cache:
        cache[(str(dest), table)] = set(columns(dest, table))

    theirs = cache[(str(dest), table)]
    missing = [c for c in source_cols if c not in theirs]

    if missing:
        print("  %s: not carrying %s (not in the schema there)"
              % (table, ", ".join(missing)))

    return [c for c in source_cols if c in theirs]


def pairs(cols, row, dest, table, drop=()):
    """(column, value) for everything worth carrying to `dest`."""
    keep = set(shared(cols, dest, table))

    return [(c, v) for c, v in zip(cols, row) if c in keep and c not in drop]


def insert(into, table, cols, values, skip=()):
    keep = pairs(cols, values, into, table, drop=skip)

    if not keep:
        return

    return "INSERT INTO `%s` (%s) VALUES (%s);" % (
        table,
        ", ".join("`%s`" % c for c, _ in keep),
        ", ".join(quote(v) for _, v in keep))


def show(worlds):
    for world in worlds:
        print("\n%s" % world)

        rows = world.sql(
            "SELECT c.name, c.level, a.name, "
            "IFNULL(c.lastLogoutTime,'never') "
            "FROM characters c JOIN accounts a ON a.id = c.accountid "
            "ORDER BY c.name;")

        if not rows or rows == [['']]:
            print("  (nothing)")
            continue

        for name, level, account, last in rows:
            print("  %-14s Lv%-4s account %-10s last put down %s"
                  % (name, level, account, last))


def one_character(name, source, dest, force, statements):
    """Everything needed to put ONE character on `dest`, appended to
    `statements`. Returns the number of rows, or None if it was refused."""
    got = source.sql("SELECT id, IFNULL(lastLogoutTime,'') "
                     "FROM characters WHERE name=%s;" % quote(name))

    if not got or not got[0][0]:
        raise SystemExit("%s has no character called '%s'." % (source, name))

    cid, mine_last = got[0]

    # --- refuse to overwrite something newer -----------------------------
    theirs = dest.sql("SELECT id, IFNULL(lastLogoutTime,'') "
                      "FROM characters WHERE name=%s;" % quote(name))

    dest_cid = None

    if theirs and theirs[0][0]:
        dest_cid, their_last = theirs[0]

        if their_last and mine_last and their_last > mine_last and not force:
            print("  REFUSED '%s' - %s has a NEWER one" % (name, dest))
            print("           there: %s" % their_last)
            print("           here:  %s" % mine_last)
            print("           that is somebody's evening. --force overrides.")

            return None

    acc_name = source.one(
        "SELECT a.name FROM accounts a JOIN characters c ON c.accountid=a.id "
        "WHERE c.id=%s;" % cid)

    # --- out with the old copy, if there is one --------------------------
    if dest_cid:
        print("  '%s' replaces the copy already there" % name)

        # Nested rows first, while their parents are still there to find them
        # by - afterwards they are unreachable orphans, and `inventoryequipment`
        # has no owner column of its own, so nothing could ever find them again.
        #
        # The parent is selected by ITS CHARACTER, not by its own primary key.
        # Getting that wrong reads as `WHERE inventoryitemid = <a character id>`,
        # which matches nothing, deletes nothing, and quietly left ten dead
        # equipment rows in the database on every single move.
        for parent, kids in SECOND_LEVEL.items():
            owner = [c for c in ("characterid", "cid", "charid")
                     if c in columns(dest, parent)][0]

            for child, link in kids:
                statements.append(
                    "DELETE FROM `%s` WHERE `%s` IN (SELECT `%s` FROM `%s` "
                    "WHERE `%s`=%s);" % (child, link, link, parent,
                                         owner, dest_cid))

        for table, key in child_tables(dest):
            statements.append("DELETE FROM `%s` WHERE `%s`=%s;" % (table, key, dest_cid))

        statements.append("DELETE FROM characters WHERE id=%s;" % dest_cid)

    # --- the character itself --------------------------------------------
    chr_cols, chr_rows = rows_of(source, "characters", "id=%s" % cid)
    chr_keep = pairs(chr_cols, chr_rows[0], dest, "characters", drop=("id", "accountid"))

    # The account id is looked up on the DESTINATION rather than copied - the
    # same person is a different row number in each world.
    statements.append(
        "INSERT INTO characters (accountid, %s) SELECT id, %s FROM accounts "
        "WHERE name=%s;" % (
            ", ".join("`%s`" % c for c, _ in chr_keep),
            ", ".join(quote(v) for _, v in chr_keep),
            quote(acc_name)))

    statements.append("SET @cid = LAST_INSERT_ID();")

    # --- everything keyed to the character -------------------------------
    carried = 1

    for table, key in child_tables(source):
        cols, rows = rows_of(source, table, "`%s`=%s" % (key, cid))
        kids = SECOND_LEVEL.get(table, [])

        # The parent's own id is dropped on the way in - the destination
        # assigns its own - but it is still needed HERE, to find the rows
        # that hang off it in the world it came from.
        own_id = key_of(source, table) if kids else None

        for row in rows:
            drop = [key] + ([own_id] if own_id else [])

            keep = pairs(cols, row, dest, table, drop=drop)

            statements.append("INSERT INTO `%s` (`%s`, %s) VALUES (@cid, %s);" % (
                table, key,
                ", ".join("`%s`" % c for c, _ in keep),
                ", ".join(quote(v) for _, v in keep)))

            if not kids:
                carried += 1
                continue

            # Held in a variable rather than read again with LAST_INSERT_ID(),
            # which the child inserts below would themselves overwrite.
            statements.append("SET @parent = LAST_INSERT_ID();")

            was = row[cols.index(own_id)]

            for child, link in kids:
                kid_cols, kid_rows = rows_of(source, child, "`%s`=%s" % (link, was))

                for kid in kid_rows:
                    kid_key = key_of(source, child)

                    # A nested row may ALSO carry a characterid, and Cosmic
                    # reads questprogress by characterid before joining on the
                    # parent - so both have to be right, not just the link.
                    mine = [c for c in ("characterid", "cid", "charid") if c in kid_cols]

                    keep_kid = pairs(kid_cols, kid, dest, child,
                                     drop=[kid_key, link] + mine)

                    cols_sql = ["`%s`" % link] + ["`%s`" % c for c in mine]
                    vals_sql = ["@parent"] + ["@cid"] * len(mine)

                    cols_sql += ["`%s`" % c for c, _ in keep_kid]
                    vals_sql += [quote(v) for _, v in keep_kid]

                    statements.append("INSERT INTO `%s` (%s) VALUES (%s);" % (
                        child, ", ".join(cols_sql), ", ".join(vals_sql)))

                    carried += 1

            carried += 1

    return carried


def bring_account(acc_name, source, dest, statements):
    """The account row has to exist over there - a character with no account
    cannot be logged into."""
    acc_cols, acc_rows = rows_of(source, "accounts", "name=%s" % quote(acc_name))

    if not acc_rows:
        raise SystemExit("%s has no account called '%s'." % (source, acc_name))

    if dest.one("SELECT id FROM accounts WHERE name=%s;" % quote(acc_name)) is None:
        print("  account '%s' does not exist there - taking it too" % acc_name)
        statements.append(insert(dest, "accounts", acc_cols, acc_rows[0], skip=("id",)))

    statements.append("SET @acc = (SELECT id FROM accounts WHERE name=%s);"
                      % quote(acc_name))

    # The bank belongs to the ACCOUNT, not to any of its characters.
    #
    # `storages` is keyed on accountid alone, so nothing in the per-character
    # walk ever reaches it - and it holds the slot count and the stored mesos,
    # of which one account here has a billion. Items sitting in storage are
    # rows in `inventoryitems` with an account-scoped `type` (ItemFactory:
    # STORAGE is 2, the CASH_* tabs are 3/4/5/7), and those DO carry a
    # characterid, so they travel with whichever character holds them - which
    # is another reason the unit is the account and not one character.
    src_acc = source.one("SELECT id FROM accounts WHERE name=%s;" % quote(acc_name))

    st_cols, st_rows = rows_of(source, "storages", "accountid=%s" % src_acc)

    if st_rows:
        statements.append("DELETE FROM storages WHERE accountid=@acc;")

        for row in st_rows:
            keep = pairs(st_cols, row, dest, "storages", drop=("storageid", "accountid"))

            statements.append("INSERT INTO storages (accountid, %s) VALUES (@acc, %s);" % (
                ", ".join("`%s`" % c for c, _ in keep),
                ", ".join(quote(v) for _, v in keep)))

        print("  carrying the bank (%d storage row(s))" % len(st_rows))


def apply(names, acc_name, source, dest, force):
    """Carry a set of characters. All of it or none of it."""
    # The first attempt at this died partway through - on a column the
    # destination's schema did not have - and left an account and a stub
    # character behind that nobody asked for. The client stops at the first
    # error and exits without committing, so a failed move rolls back.
    statements = ["SET FOREIGN_KEY_CHECKS=0;", "START TRANSACTION;"]

    bring_account(acc_name, source, dest, statements)

    carried = 0
    brought = []

    for name in names:
        rows = one_character(name, source, dest, force, statements)

        if rows is not None:
            carried += rows
            brought.append(name)

    if not brought:
        print("  nothing to carry - nothing was changed on %s" % dest)

        return

    statements.append("COMMIT;")
    statements.append("SET FOREIGN_KEY_CHECKS=1;")

    dest.sql("\n".join(s for s in statements if s))

    print("  carried %d rows" % carried)

    for name in brought:
        now = dest.sql("SELECT name, level, meso FROM characters WHERE name=%s;"
                       % quote(name))

        if not now or not now[0][0]:
            raise SystemExit("'%s' did not arrive" % name)

        print("  '%s' is now on %s: level %s, %s mesos"
              % (now[0][0], dest, now[0][1], now[0][2]))


def move(name, source, dest, force):
    print("Moving '%s' from %s to %s" % (name, source, dest))

    acc_name = source.one(
        "SELECT a.name FROM accounts a JOIN characters c ON c.accountid=a.id "
        "WHERE c.name=%s;" % quote(name))

    if acc_name is None:
        raise SystemExit("%s has no character called '%s'." % (source, name))

    apply([name], acc_name, source, dest, force)


def move_account(acc_name, source, dest, force):
    """A PLAYER, not a character.

    Cosmic gives an account three character slots and a person thinks of all
    three as theirs. Carrying one and leaving the others is how somebody finds
    two of their characters missing on the handheld they took out for the
    afternoon - so the whole account travels together.

    Each character is still judged on its own age, so a stale copy of one
    cannot ride along on a fresh copy of another."""
    names = [r[0] for r in source.sql(
        "SELECT c.name FROM characters c JOIN accounts a ON a.id=c.accountid "
        "WHERE a.name=%s ORDER BY c.name;" % quote(acc_name)) if r and r[0]]

    if not names:
        raise SystemExit("%s has no characters on account '%s'." % (source, acc_name))

    print("Moving account '%s' (%s) from %s to %s"
          % (acc_name, ", ".join(names), source, dest))

    apply(names, acc_name, source, dest, force)


def verify(name, a, b):
    """Compare a character in two worlds, field by field.

    A row count proves nothing: the bug that made this necessary carried the
    right NUMBER of rows with a column quietly missing from each. So this
    walks the actual values."""
    print("Comparing '%s' on %s and on %s" % (name, a, b))

    cids = {}

    for world in (a, b):
        cid = world.one("SELECT id FROM characters WHERE name=%s;" % quote(name))

        if cid is None:
            raise SystemExit("%s has no '%s'." % (world, name))

        cids[str(world)] = cid

    faults = 0

    def compare(what, table, where_a, where_b):
        nonlocal faults

        cols_a, rows_a = rows_of(a, table, where_a)
        cols_b, rows_b = rows_of(b, table, where_b)

        # Ids differ by design - the same character is a different row number
        # in each world - so they are not part of the comparison.
        ignore = {"id", "characterid", "cid", "charid", "accountid",
                  "inventoryitemid", "inventoryequipmentid",
                  "queststatusid", "inventorymerchantid", "storageid"}

        common = [c for c in cols_a if c in set(cols_b) and c not in ignore]

        def shrink(cols, rows):
            index = [cols.index(c) for c in common]

            # Sorted on the compared values themselves, so this asks whether
            # the two sides hold the same SET of rows - which is the real
            # question. An ORDER BY on hand-picked columns is not unique: two
            # low-level armours that agree on those columns sorted differently
            # on each side and the walk below reported a difference in wdef
            # between two rows that were never the same row.
            got = [tuple(r[i] for i in index) for r in rows]

            return sorted(got, key=lambda t: tuple("" if v is None else v for v in t))

        left, right = shrink(cols_a, rows_a), shrink(cols_b, rows_b)

        if left == right:
            print("  ok   %-20s %d rows x %d fields" % (what, len(left), len(common)))
            return

        faults += 1
        print("  DIFF %-20s %d rows there, %d here" % (what, len(left), len(right)))

        for i in range(max(len(left), len(right))):
            l = left[i] if i < len(left) else None
            r = right[i] if i < len(right) else None

            if l == r:
                continue

            if l is None or r is None:
                print("       row %d only on %s" % (i, a if r is None else b))
                continue

            for c, x, y in zip(common, l, r):
                if x != y:
                    print("       row %d  %s: %r vs %r" % (i, c, x, y))

    ca, cb = cids[str(a)], cids[str(b)]

    compare("characters", "characters", "id=%s" % ca, "id=%s" % cb)

    for table, key in child_tables(a):
        compare(table, table, "`%s`=%s" % (key, ca), "`%s`=%s" % (key, cb))

    # The nested tables are deliberately absent from child_tables - they are
    # carried through their parent, not by characterid - so they have to be
    # asked for by name here or they would go unchecked entirely.
    compare("inventoryequipment", "inventoryequipment",
            "inventoryitemid IN (SELECT inventoryitemid FROM inventoryitems "
            "WHERE characterid=%s)" % ca,
            "inventoryitemid IN (SELECT inventoryitemid FROM inventoryitems "
            "WHERE characterid=%s)" % cb)

    for nested in sorted(NESTED - {"inventoryequipment"}):
        compare(nested, nested, "characterid=%s" % ca, "characterid=%s" % cb)

    # And the bank, which hangs off the account rather than the character.
    aa = a.one("SELECT accountid FROM characters WHERE id=%s;" % ca)
    bb = b.one("SELECT accountid FROM characters WHERE id=%s;" % cb)

    compare("storages", "storages", "accountid=%s" % aa, "accountid=%s" % bb)

    # --- and that nothing points at a row that is not there ---------------
    #
    # The comparison above deliberately ignores id columns, because the same
    # character legitimately holds different row numbers in each world. That
    # blind spot is exactly where the worst bug lived: a questprogress row
    # carrying the queststatusid it had somewhere else compares EQUAL on
    # every field it is judged on, and still points at nothing. Cosmic reads
    # that as no progress at all - the quest stays started and the kill count
    # is gone - so the link itself has to be checked separately.
    for world, cid in ((a, ca), (b, cb)):
        for parent, kids in SECOND_LEVEL.items():
            parent_key = key_of(world, parent)

            for child, link in kids:
                # Some children carry a character column and can be asked
                # about this character alone. `inventoryequipment` does not -
                # it is reachable ONLY through its item - so an orphan there
                # belongs to nobody and the only honest question is whether
                # the world holds any at all.
                mine = [c for c in ("characterid", "cid", "charid")
                        if c in columns(world, child)]

                scope = ("c.`%s`=%s" % (mine[0], cid)) if mine else "1=1"

                lost = world.one(
                    "SELECT COUNT(*) FROM `%s` c LEFT JOIN `%s` p "
                    "ON p.`%s` = c.`%s` WHERE %s AND p.`%s` IS NULL;"
                    % (child, parent, parent_key, link, scope, parent_key))

                if lost and int(lost) > 0:
                    faults += 1
                    print("  ORPHANS %s on %s: %s row(s) point at no %s%s"
                          % (child, world, lost, parent,
                             "" if mine else " (world-wide - this table has no owner column)"))

    print("\n%s" % ("IDENTICAL" if not faults else "%d PROBLEM(S)" % faults))

    if faults:
        raise SystemExit(1)


def main():
    ap = argparse.ArgumentParser(add_help=False)
    ap.add_argument("command", nargs="?", default="where")
    ap.add_argument("rest", nargs="*")
    ap.add_argument("--force", action="store_true")
    args = ap.parse_args()

    if args.command == "where":
        places = args.rest or ["pc"]

        show([World(p) for p in places])
        return

    if args.command == "move":
        if len(args.rest) != 3:
            raise SystemExit(__doc__)

        name, src, dst = args.rest

        move(name, World(src), World(dst), args.force)
        return

    if args.command == "account":
        if len(args.rest) != 3:
            raise SystemExit(__doc__)

        name, src, dst = args.rest

        move_account(name, World(src), World(dst), args.force)
        return

    if args.command == "verify":
        if len(args.rest) != 3:
            raise SystemExit(__doc__)

        name, one, two = args.rest

        verify(name, World(one), World(two))
        return

    raise SystemExit(__doc__)


if __name__ == "__main__":
    main()
