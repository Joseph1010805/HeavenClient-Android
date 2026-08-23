#!/usr/bin/env bash
#
# Puts the SERVER on a handheld, so it can play with no network at all.
#
#   tools/stage_server.sh <device-serial> [cosmic-dir]
#
# The client already needs nothing for this: `ServerIP` lives in the settings
# file on the device and defaults to 127.0.0.1. And Cosmic needs no second
# config either - `Server.getInetSocket` hands a loopback client LOCALHOST and
# a LAN client LANHOST by itself, so ONE server serves both the handheld it
# runs on and anybody else on a phone hotspot.
#
# What goes over:
#
#   Cosmic.jar     51 MB
#   wz.tar        596 MB, 22,180 files  <- the reason this is a tar
#   scripts.tar   8.4 MB, 1,915 files   <- the JS behind every NPC and quest
#   config.yaml
#   termux_setup.sh
#
# Nothing else. The database is NOT copied: Cosmic manages its schema with
# Liquibase and builds it on first run against an empty one.
#
# Safe to re-run. Every file is checked by size on the device and skipped if
# it is already there, so an interrupted 600 MB copy picks up where it left.
#
set -u

# Git Bash rewrites any argument that looks like a Unix path into a Windows
# one, so "/sdcard/Foo" reaches adb as "C:/.../Git/sdcard/Foo" and the push
# fails AFTER printing "1 file pushed". Do NOT set this globally: it breaks
# the Gradle wrapper. This script never calls Gradle.
export MSYS_NO_PATHCONV=1

DEV="${1:-}"
COSMIC_UNIX="${2:-/c/Users/Deck/OneDrive/Documents/Programs/Cosmic}"

if [ -z "$DEV" ]; then
	echo "usage: $0 <device-serial> [cosmic-dir]"
	echo
	echo "devices:"
	adb devices | tail -n +2
	exit 1
fi

if [ ! -d "$COSMIC_UNIX" ]; then
	echo "no Cosmic at $COSMIC_UNIX"
	exit 1
fi

# adb wants Windows form locally and Unix form on the device. Mixing the two
# is the only combination that works from Git Bash.
COSMIC_WIN="$(cd "$COSMIC_UNIX" && pwd -W 2>/dev/null)"
[ -n "$COSMIC_WIN" ] || COSMIC_WIN="$COSMIC_UNIX"

HERE_UNIX="$(cd "$(dirname "$0")/.." && pwd)"
HERE_WIN="$(cd "$HERE_UNIX" && pwd -W 2>/dev/null)"
[ -n "$HERE_WIN" ] || HERE_WIN="$HERE_UNIX"

# Everything lands somewhere any app may write. Termux cannot be pushed to
# directly - its home is inside /data/data and is not ours to touch - so the
# device-side script moves things across from here.
DIR=/sdcard/Download/cosmic

# Where the tars are built. Beside the source, not in the repo.
BUILD_UNIX="$COSMIC_UNIX/.stage"
BUILD_WIN="$COSMIC_WIN/.stage"

sh() { adb -s "$DEV" shell "$@"; }

remote_size() {
	sh "stat -c %s '$1' 2>/dev/null || echo 0" | tr -d '\r'
}

send() {
	local src_win="$1" src_unix="$2" name="$3"
	local want have

	want=$(stat -c %s "$src_unix")
	have=$(remote_size "$DIR/$name")

	if [ "$want" = "$have" ]; then
		printf '  %-16s %6s MB  already there\n' "$name" "$((want / 1024 / 1024))"
		return 0
	fi

	printf '  %-16s %6s MB  sending... ' "$name" "$((want / 1024 / 1024))"

	adb -s "$DEV" push "$src_win" "$DIR/$name" >/dev/null 2>&1
	have=$(remote_size "$DIR/$name")

	if [ "$want" = "$have" ]; then
		echo "ok"
		return 0
	fi

	echo "FAILED (device has $have bytes, expected $want)"
	return 1
}

# Builds a tar once and reuses it. 22,180 files pushed one at a time takes
# hours; one file takes minutes.
#
# Uncompressed on purpose: the wz is mostly PNG already, so gzip costs CPU on
# both ends and saves little, and an uncompressed tar can be checked by size
# without unpacking it.
make_tar() {
	local dir="$1" name="$2"

	if [ -f "$BUILD_UNIX/$name" ]; then
		printf '  %-16s reusing\n' "$name"
		return 0
	fi

	printf '  %-16s building... ' "$name"
	mkdir -p "$BUILD_UNIX"

	if ! tar -C "$COSMIC_UNIX" -cf "$BUILD_UNIX/$name" "$dir" 2>/dev/null; then
		echo "FAILED"
		return 1
	fi

	echo "ok"
}

echo "Staging the server for $DEV"
echo

sh "mkdir -p '$DIR'" >/dev/null 2>&1

echo "Packing:"
make_tar wz wz.tar || exit 1
make_tar scripts scripts.tar || exit 1
echo

echo "Sending:"
FAILED=0

send "$COSMIC_WIN/target/Cosmic.jar" "$COSMIC_UNIX/target/Cosmic.jar" Cosmic.jar || FAILED=$((FAILED + 1))
send "$BUILD_WIN/wz.tar" "$BUILD_UNIX/wz.tar" wz.tar || FAILED=$((FAILED + 1))
send "$BUILD_WIN/scripts.tar" "$BUILD_UNIX/scripts.tar" scripts.tar || FAILED=$((FAILED + 1))
send "$COSMIC_WIN/config.yaml" "$COSMIC_UNIX/config.yaml" config.yaml || FAILED=$((FAILED + 1))
send "$HERE_WIN/tools/termux_setup.sh" "$HERE_UNIX/tools/termux_setup.sh" termux_setup.sh || FAILED=$((FAILED + 1))

echo

if [ "$FAILED" -gt 0 ]; then
	echo "$FAILED file(s) did not arrive. Nothing else to do until they do."
	exit 1
fi

cat <<'NEXT'
Everything is on the device, in /sdcard/Download/cosmic.

The rest happens in Termux, on the handheld itself - adb cannot reach
Termux's home directory, which is why the files are parked on the SD card
for it to collect.

  1. Install Termux from F-Droid. NOT the Play Store version: it is years
     old and its package repository no longer resolves.

  2. Open it and run, in this order:

        termux-setup-storage          # then grant it, on screen
        cp /sdcard/Download/cosmic/termux_setup.sh ~
        bash ~/termux_setup.sh

     It installs Java and MariaDB, unpacks the server, starts the database
     and builds the schema. Twenty minutes, most of it downloading.

  3. When it finishes:

        ~/cosmic/run.sh

  4. In the game's settings file on that device, set

        ServerIP = 127.0.0.1

     and nothing else changes. The same server still answers anyone else on
     the hotspot at the handheld's LAN address.
NEXT
