#!/usr/bin/env bash
#
# Moves the world between the PC and a handheld, so a character can be taken
# away and brought back.
#
#   tools/sync_world.sh status <serial>   who is holding it
#   tools/sync_world.sh out    <serial>   PC -> handheld   (before leaving)
#   tools/sync_world.sh in     <serial>   handheld -> PC   (after coming home)
#
# THIS COPIES THE WHOLE WORLD, not one character. That is the right tool when
# only one place is played at a time - everybody is in the car, or everybody
# is at home - and the wrong tool if somebody plays at home while a character
# is away, because whichever side is copied second wins and the other side's
# evening is gone.
#
# So it will not let that happen by accident. A single row travels inside the
# database saying which machine currently HOLDS the world, and a copy in the
# wrong direction is refused. Going out hands the token to the handheld;
# coming in hands it back. --force overrides, and means it.
#
# Per-character export - taking one character while the rest stay home - is a
# different and much larger job (the `characters` row plus about 21 tables
# keyed by characterid). Build that when somebody actually needs to play in
# two places at once, and not before.
#
# TWO THINGS THAT WILL BITE, both learned the hard way here:
#
#   Cosmic holds character state IN MEMORY and writes it back when it saves.
#   Restoring a database underneath a running server achieves nothing - the
#   server overwrites it. Both servers must be STOPPED, and this checks.
#
#   The PC runs MySQL 8 and the handheld runs MariaDB, and MySQL 8's default
#   collation (utf8mb4_0900_ai_ci) does not exist in MariaDB. So the SCHEMA is
#   never copied, only the rows. Liquibase has already built the schema on
#   both sides, which is what makes that safe.
#
set -u

export MSYS_NO_PATHCONV=1

MYSQL_DIR="/c/Users/Deck/mysql/mysql-8.4.6-winx64/bin"
MYSQL="$MYSQL_DIR/mysql.exe"
MYSQLDUMP="$MYSQL_DIR/mysqldump.exe"
DB=cosmic

TPATH='export PATH=/data/data/com.termux/files/usr/bin:$PATH'
THOME=/data/data/com.termux/files/home
STAGE=/sdcard/Download/cosmic

WORK="${TMPDIR:-/tmp}/cosmic-sync"
mkdir -p "$WORK"

MODE="${1:-}"
DEV="${2:-}"
FORCE="${3:-}"

say() { printf '%s\n' "$*"; }
die() { printf '\nSTOPPED: %s\n' "$*"; exit 1; }

if [ -z "$MODE" ] || [ -z "$DEV" ]; then
	sed -n '3,9p' "$0" | sed 's/^# \?//'
	echo
	echo "devices:"
	adb devices | tail -n +2
	exit 1
fi

# --- talking to each side ------------------------------------------------

pc_sql() { "$MYSQL" -uroot "$DB" -N -B -e "$1" 2>/dev/null | tr -d '\r'; }

# Anything sent to the handheld is wrapped in `sh -c '...'`, so a single quote
# in the payload would close the quoting early and the rest would be read as
# shell. Every SQL string here has quotes in it, so they are escaped in one
# place rather than remembered at each call site.
dev_sh() {
	local cmd=${1//\'/\'\\\'\'}

	adb -s "$DEV" shell "run-as com.termux sh -c '$TPATH; $cmd'" 2>/dev/null | tr -d '\r'
}

dev_sql() { dev_sh "mariadb -u root $DB -N -B -e \"$1\""; }

# --- the token -----------------------------------------------------------

# Deliberately no COLLATE: each side uses its own default, which is the whole
# reason one definition can live in both a MySQL 8 and a MariaDB database.
HOLDER_DDL="CREATE TABLE IF NOT EXISTS world_holder (id TINYINT NOT NULL PRIMARY KEY, holder VARCHAR(64) NOT NULL, stamp DATETIME NOT NULL)"
HOLDER_ROW="INSERT IGNORE INTO world_holder VALUES (1,'home',NOW())"

ensure_holder() {
	pc_sql "$HOLDER_DDL" >/dev/null
	pc_sql "$HOLDER_ROW" >/dev/null

	dev_sql "$HOLDER_DDL" >/dev/null
	dev_sql "$HOLDER_ROW" >/dev/null
}

pc_holder()  { pc_sql "SELECT holder FROM world_holder WHERE id=1"; }
dev_holder() { dev_sql "SELECT holder FROM world_holder WHERE id=1"; }

# --- refusing to destroy an evening --------------------------------------

pc_server_up() {
	netstat -an 2>/dev/null | grep -qE "[:.]8484[^0-9].*LISTEN"
}

dev_server_up() {
	local n
	n=$(adb -s "$DEV" shell "ps -A -o NAME | grep -c '^java$'" 2>/dev/null | tr -d '\r ')

	[ -n "$n" ] && [ "$n" != "0" ]
}

check_stopped() {
	if [ "$1" = pc ] && pc_server_up; then
		die "the server on this PC is still running.
Stop it first - Cosmic keeps characters in memory and writes them back when
it saves, so anything restored underneath it is simply overwritten."
	fi

	if [ "$1" = dev ] && dev_server_up; then
		die "the server on $DEV is still running. Stop it with:

  adb -s $DEV shell \"run-as com.termux pkill -f Cosmic.jar\"

Cosmic keeps characters in memory and writes them back when it saves, so
anything restored underneath it is simply overwritten."
	fi
}

# Every table except Liquibase's own bookkeeping, which belongs to whichever
# machine it is on and must not travel.
table_list() {
	pc_sql "SELECT table_name FROM information_schema.tables
		WHERE table_schema='$DB' AND table_type='BASE TABLE'
		AND table_name NOT LIKE 'databasechangelog%'"
}

# Builds the whole restore as one file: empty every table, then the rows.
# Assembled here and sent in one piece - doing the DELETEs as separate adb
# calls meant seventy-odd round trips to the device.
build_restore() {
	local dump="$1" out="$2"

	{
		echo "SET FOREIGN_KEY_CHECKS=0;"

		for t in $TABLES; do
			echo "DELETE FROM \`$t\`;"
		done

		cat "$dump"
		echo "SET FOREIGN_KEY_CHECKS=1;"
	} > "$out"
}

winpath() { cygpath -w "$1" 2>/dev/null || echo "$1"; }

# --- status --------------------------------------------------------------

if [ "$MODE" = status ]; then
	ensure_holder

	say "held by the PC:       $(pc_holder)"
	say "held by $DEV: $(dev_holder)"
	say
	say "PC server running:       $(pc_server_up && echo yes || echo no)"
	say "handheld server running: $(dev_server_up && echo yes || echo no)"
	say
	say "characters on the PC:"
	pc_sql "SELECT name, level FROM characters" | sed 's/^/  /'
	say "characters on $DEV:"
	dev_sql "SELECT name, level FROM characters" | sed 's/^/  /'
	exit 0
fi

[ "$MODE" = out ] || [ "$MODE" = in ] || die "unknown mode '$MODE' - use status, out or in"

ensure_holder

DUMP="$WORK/world.sql"
RESTORE="$WORK/restore.sql"
TABLES="$(table_list | tr '\n' ' ')"

[ -n "$TABLES" ] || die "could not list the tables on the PC - is MySQL running?"

if [ "$MODE" = out ]; then
	say "PC -> $DEV"
	say

	HOLDER="$(pc_holder)"

	if [ "$HOLDER" != "home" ] && [ "$FORCE" != "--force" ]; then
		die "the PC is not holding the world - '$HOLDER' is.
Whatever has been played there since would be destroyed by this.
Bring it home first:  $0 in $DEV
Or if you are certain the PC's copy is the one to keep:  $0 out $DEV --force"
	fi

	check_stopped dev

	# Set BEFORE the dump so the token travels inside it and both sides end
	# up agreeing about who has the world.
	pc_sql "UPDATE world_holder SET holder='$DEV', stamp=NOW() WHERE id=1" >/dev/null

	say "reading the PC's world..."
	"$MYSQLDUMP" -uroot --no-create-info --complete-insert --skip-add-locks \
		--single-transaction "$DB" $TABLES > "$DUMP" 2>/dev/null \
		|| die "mysqldump failed"

	say "  $(wc -c < "$DUMP") bytes"

	build_restore "$DUMP" "$RESTORE"

	say "writing it to the handheld..."
	adb -s "$DEV" push "$(winpath "$RESTORE")" "$STAGE/restore.sql" >/dev/null 2>&1
	dev_sh "cp $STAGE/restore.sql $THOME/restore.sql" >/dev/null
	dev_sh "mariadb -u root $DB < $THOME/restore.sql" >/dev/null

	say
	say "the handheld now holds the world: $(dev_holder)"
	say "characters there:"
	dev_sql "SELECT name, level FROM characters" | sed 's/^/  /'
	say
	say "Start it with the game's SERVER switch, or ~/cosmic/run.sh in Termux."
	exit 0
fi

say "$DEV -> PC"
say

HOLDER="$(dev_holder)"

if [ "$HOLDER" != "$DEV" ] && [ "$FORCE" != "--force" ]; then
	die "the handheld is not holding the world - '$HOLDER' is.
Whatever has been played there since would be destroyed by this.
If you are certain the handheld's copy is the one to keep:
  $0 in $DEV --force"
fi

check_stopped pc
check_stopped dev

dev_sql "UPDATE world_holder SET holder='home', stamp=NOW() WHERE id=1" >/dev/null

say "reading the handheld's world..."
dev_sh "mariadb-dump -u root --no-create-info --complete-insert --skip-add-locks $DB $TABLES > $THOME/world.sql"

# Read it back through run-as rather than the SD card: Android 11 and up
# restrict what an app may write to shared storage, and this path is proven.
adb -s "$DEV" shell "run-as com.termux cat $THOME/world.sql" 2>/dev/null | tr -d '\r' > "$DUMP"

[ -s "$DUMP" ] || die "nothing came back from the handheld"

say "  $(wc -c < "$DUMP") bytes"

build_restore "$DUMP" "$RESTORE"

say "writing it to the PC..."
"$MYSQL" -uroot "$DB" < "$RESTORE" || die "the restore failed"

say
say "the PC now holds the world: $(pc_holder)"
say "characters here:"
pc_sql "SELECT name, level FROM characters" | sed 's/^/  /'
