#!/data/data/com.termux/files/usr/bin/bash
#
# Sets up the Cosmic server INSIDE Termux, on the handheld.
#
#   termux-setup-storage          # grant it on screen first
#   cp /sdcard/Download/cosmic/termux_setup.sh ~
#   bash ~/termux_setup.sh
#
# Run `tools/stage_server.sh <serial>` on the PC first - this expects to find
# the payload waiting in /sdcard/Download/cosmic.
#
# Safe to re-run: every step checks whether it has already been done. If it
# stops, fix what it complains about and run it again.
#
set -u

STAGE=/sdcard/Download/cosmic
HOME_DIR="$HOME/cosmic"
DB=cosmic

step() { printf '\n=== %s\n' "$1"; }
die()  { printf '\nSTOPPED: %s\n' "$1"; exit 1; }

[ -d "$STAGE" ] || die "nothing at $STAGE.
Either stage_server.sh has not run, or storage access was not granted -
run 'termux-setup-storage' and accept the prompt, then try again."

step "Packages"
# MariaDB stands in for MySQL. Checked before relying on it: none of Cosmic's
# 37 schema files use anything MySQL-8-only - no utf8mb4_0900 collations, no
# JSON functions, no CHECK constraints - so the dialect difference does not
# reach us.
pkg update -y >/dev/null 2>&1
pkg install -y openjdk-21 mariadb tar which >/dev/null 2>&1

command -v java >/dev/null    || die "java did not install"
command -v mariadbd >/dev/null || die "mariadb did not install"

echo "java:    $(java -version 2>&1 | head -1)"
echo "mariadb: $(mariadbd --version 2>&1 | head -1)"

step "Unpacking the server"
mkdir -p "$HOME_DIR"

if [ ! -f "$HOME_DIR/Cosmic.jar" ]; then
	cp "$STAGE/Cosmic.jar" "$HOME_DIR/" || die "could not copy Cosmic.jar"
fi

if [ ! -f "$HOME_DIR/config.yaml" ]; then
	cp "$STAGE/config.yaml" "$HOME_DIR/" || die "could not copy config.yaml"
fi

# 596 MB and 22,180 files. This is the slow part, and only happens once.
if [ ! -d "$HOME_DIR/wz" ]; then
	echo "wz: unpacking 596 MB, this takes a while..."
	tar -C "$HOME_DIR" -xf "$STAGE/wz.tar" || die "could not unpack wz.tar"
else
	echo "wz: already unpacked"
fi

if [ ! -d "$HOME_DIR/scripts" ]; then
	echo "scripts: unpacking..."
	tar -C "$HOME_DIR" -xf "$STAGE/scripts.tar" || die "could not unpack scripts.tar"
else
	echo "scripts: already unpacked"
fi

step "Database"
# Termux's MariaDB keeps its data under the prefix rather than /var.
DATADIR="$PREFIX/var/lib/mysql"

if [ ! -d "$DATADIR/mysql" ]; then
	echo "first-time setup..."
	mariadb-install-db >/dev/null 2>&1 || die "mariadb-install-db failed"
else
	echo "already initialised"
fi

if ! pgrep -f mariadbd >/dev/null 2>&1; then
	echo "starting..."
	mariadbd-safe -u "$(whoami)" >/dev/null 2>&1 &

	# It is not ready the moment the process exists.
	for i in $(seq 1 30); do
		mariadb -u root -e "SELECT 1" >/dev/null 2>&1 && break
		sleep 1
	done
fi

mariadb -u root -e "SELECT 1" >/dev/null 2>&1 || die "the database did not come up.
Look at $PREFIX/var/lib/mysql/*.err for why."

# Cosmic connects as root with no password, per config.yaml. That is fine on a
# handheld nobody else can reach - the database listens on loopback only.
mariadb -u root -e "CREATE DATABASE IF NOT EXISTS $DB
	DEFAULT CHARACTER SET utf8mb4" || die "could not create the database"

echo "database '$DB' ready"

step "Writing the launcher"
# -Xmx1536m rather than the desktop's 2048m: this device is also running the
# game, which holds an 8192x8192 texture atlas of its own. Raise it if the
# server runs out; lower it if the game starts stuttering.
cat > "$HOME_DIR/run.sh" <<'RUN'
#!/data/data/com.termux/files/usr/bin/bash
cd "$(dirname "$0")" || exit 1

# The database has to be up before the server looks for it, and Termux does
# not keep it running across a reboot.
if ! pgrep -f mariadbd >/dev/null 2>&1; then
	echo "starting the database..."
	mariadbd-safe -u "$(whoami)" >/dev/null 2>&1 &

	for i in $(seq 1 30); do
		mariadb -u root -e "SELECT 1" >/dev/null 2>&1 && break
		sleep 1
	done
fi

# Stops the phone sleeping the server out from under the game.
termux-wake-lock 2>/dev/null

echo "starting Cosmic - first run builds the schema and takes a few minutes"
java -Xmx1536m -Dwz-path=wz -jar Cosmic.jar
RUN

chmod +x "$HOME_DIR/run.sh"

cat <<NEXT

=== Done

Start the server with:

    ~/cosmic/run.sh

The FIRST run is slow: Liquibase builds the whole schema from nothing, and
Cosmic reads 596 MB of game data off the SD card. Later runs are quicker.
Leave it until it stops printing.

Then, in the game's settings file on this device:

    ServerIP = 127.0.0.1

That is the whole of offline mode. Nothing in the client changes, because
Cosmic hands a loopback client its LOCALHOST address and a LAN client its
LANHOST one, all by itself - so this same server still answers the other
handhelds over a phone hotspot at this device's LAN address.

If it does not start, the reason will be in what it printed, and
'python tools/serverlog.py' on the PC reads the same log format.
NEXT
