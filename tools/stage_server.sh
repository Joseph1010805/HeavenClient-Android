#!/usr/bin/env bash
#
# Puts the SERVER on a handheld, so it can play with no network at all.
#
#   tools/stage_server.sh <device-serial> [cosmic-dir]
#
# The client needs nothing for this: `ServerIP` lives in the settings file on
# the device and defaults to 127.0.0.1.
#
# ⚠ THE SERVER DOES. This used to claim that Cosmic needed no second config,
# because `Server.getInetSocket` hands a loopback client LOCALHOST and a LAN
# client LANHOST by itself. It does choose between them - but LANHOST is a
# FIXED STRING in config.yaml, and nothing ever works out what address this
# machine actually has.
#
# So a config copied from the PC tells every device on the network to reconnect
# to the PC. Joining works, the character list arrives, a character is chosen,
# and then the client reconnects to a machine with no server on it and sits
# there. The Start button looks broken and nothing says why.
#
# tools/set_host_ip.sh sets it, and is run at the end of this script.
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

# ADB, WHEREVER IT IS.
#
# The THIRD script in this folder to need this, and the third to be written
# without it. install.sh and deploy_data.sh both grew a copy after failing in
# the same way; this one had none at all, so every push died with
# "adb: command not found" and reported six files that "did not arrive" -
# blaming the device for a tool that was never on PATH.
#
# Braced with defaults throughout: this runs under `set -u`, Git Bash does not
# always set USER, and Windows sets USERNAME instead. A bare $USER here aborts
# the whole script before it prints anything.
if ! command -v adb >/dev/null 2>&1; then
	for guess in 		"${LOCALAPPDATA:-}/Android/Sdk/platform-tools" 		"${HOME:-}/AppData/Local/Android/Sdk/platform-tools" 		"/c/Users/${USERNAME:-${USER:-}}/AppData/Local/Android/Sdk/platform-tools"
	do
		if [ -x "$guess/adb.exe" ] || [ -x "$guess/adb" ]; then
			PATH="$PATH:$guess"
			export PATH
			break
		fi
	done
fi

if ! command -v adb >/dev/null 2>&1; then
	echo "adb is not on PATH and was not found in the usual SDK location."
	echo "Add platform-tools to PATH and run this again."
	exit 1
fi

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

# QUIET, WHEN tools/install.sh IS DOING THE TALKING. Same rule as
# deploy_data.sh: by hand this is a report, from the installer it is one step,
# and what goes quiet is only ever the good news. Failures print in both.
QUIET="${INSTALL_QUIET:-0}"

q() { [ "$QUIET" = "1" ] || echo "$@"; }

# Where the tars are built. Beside the source, not in the repo.
BUILD_UNIX="$COSMIC_UNIX/.stage"
BUILD_WIN="$COSMIC_WIN/.stage"

# It used to be made by make_tar, which runs after the first send - and the
# stamps that send now writes have to land somewhere.
mkdir -p "$BUILD_UNIX"

sh() { adb -s "$DEV" shell "$@"; }

remote_size() {
	sh "stat -c %s '$1' 2>/dev/null || echo 0" | tr -d '\r'
}

# ⚠ SIZE ALONE IS NOT "THE SAME FILE".
#
# This compared byte counts and nothing else, so a RECOMPILED Cosmic.jar -
# same classes, different code, same length to the byte - was reported as
# "already there" and never left the PC. The server on the device went on
# running the previous build while every line of output said it was fine.
# That is the third time in this project a transfer has been checked by a
# number that can match while the contents differ.
#
# The hash is computed HERE and parked beside the payload as a stamp, rather
# than hashing on the device: md5summing a 555 MB tar over adb costs a minute
# a run, and the device does not need to be asked - it only has to remember
# what it was last given.
stamp_of() {
	md5sum "$1" | cut -d' ' -f1
}

remote_stamp() {
	sh "cat '$1' 2>/dev/null" | tr -d '\r\n'
}

send() {
	local src_win="$1" src_unix="$2" name="$3"
	local want have sum stamp

	# A SOURCE THAT IS NOT THERE IS ITS OWN ANSWER.
	# Without this, stat fails, $want is empty, and the report reads
	# "FAILED (device has 92112664 bytes, expected )" - which blames the
	# device for a file that never existed on this machine.
	if [ ! -f "$src_unix" ]; then
		printf '  %-14s MISSING on this PC: %s
' "$name" "$src_unix"
		return 1
	fi

	want=$(stat -c %s "$src_unix")
	sum=$(stamp_of "$src_unix")
	have=$(remote_size "$DIR/$name")
	stamp=$(remote_stamp "$DIR/$name.md5")

	# BOTH must agree. A missing stamp - anything staged before this check
	# existed - counts as unknown and is sent again, once.
	if [ "$want" = "$have" ] && [ -n "$stamp" ] && [ "$sum" = "$stamp" ]; then
		[ "$QUIET" = "1" ] || printf '  %-16s %6s MB  already there\n' "$name" "$((want / 1024 / 1024))"
		return 0
	fi

	printf '  %-16s %6s MB  sending... ' "$name" "$((want / 1024 / 1024))"

	adb -s "$DEV" push "$src_win" "$DIR/$name" >/dev/null 2>&1
	have=$(remote_size "$DIR/$name")

	if [ "$want" != "$have" ]; then
		echo "FAILED (device has $have bytes, expected $want)"
		return 1
	fi

	# The stamp goes over only AFTER the payload arrived at the right size,
	# so an interrupted copy cannot leave the device claiming to hold a file
	# it does not have.
	printf '%s' "$sum" > "$BUILD_UNIX/$name.md5"
	adb -s "$DEV" push "$BUILD_WIN/$name.md5" "$DIR/$name.md5" >/dev/null 2>&1

	echo "ok"
	return 0
}

# Builds a tar once and reuses it. 22,180 files pushed one at a time takes
# hours; one file takes minutes.
#
# Uncompressed on purpose: the wz is mostly PNG already, so gzip costs CPU on
# both ends and saves little, and an uncompressed tar can be checked by size
# without unpacking it.
make_tar() {
	local dir="$1" name="$2"

	# REUSE ONLY IF IT IS STILL TRUE.
	#
	# This used to reuse any tar that existed, full stop - so a scripts.tar
	# built a week ago was sent again while 2,392 newly generated quest
	# scripts sat on the PC. Every line of output said "ok". Nothing on the
	# device was wrong; the right file simply never left.
	#
	# `find -newer` is the whole test: if anything under the directory is
	# newer than the tar, the tar is out of date.
	if [ -f "$BUILD_UNIX/$name" ]; then
		if [ -z "$(find "$COSMIC_UNIX/$dir" -newer "$BUILD_UNIX/$name" -print -quit 2>/dev/null)" ]; then
			[ "$QUIET" = "1" ] || printf '  %-16s reusing\n' "$name"
			return 0
		fi

		printf '  %-16s stale, rebuilding... ' "$name"
		rm -f "$BUILD_UNIX/$name"
	else
		printf '  %-16s building... ' "$name"
	fi

	mkdir -p "$BUILD_UNIX"

	if ! tar -C "$COSMIC_UNIX" -cf "$BUILD_UNIX/$name" "$dir" 2>/dev/null; then
		echo "FAILED"
		return 1
	fi

	echo "ok"
}

q "Staging the server for $DEV"
q

sh "mkdir -p '$DIR'" >/dev/null 2>&1

# ⚠ DELETE ANY STAGED run.sh. IT CAN ONLY EVER BE STALE.
#
# run.sh is generated ON THE DEVICE, at the end of termux_setup.sh, and
# nothing here has staged one for a month. But an earlier version of this
# script did, and that orphan stayed - and bootstrap.sh copies a staged
# run.sh over the live one whenever the two differ.
#
# So a 28 August launcher was reinstalled on every press of HOST, quietly
# undoing the 14 September fix for the login hang each time. Two weeks of a
# bug that was already fixed. Removing the file is the half of that repair
# that belongs here; bootstrap.sh now also demands that a staged copy be
# NEWER before it wins.
sh "rm -f '$DIR/run.sh'" >/dev/null 2>&1

q "Packing:"
make_tar wz wz.tar || exit 1
make_tar scripts scripts.tar || exit 1
q

q "Sending:"
FAILED=0

send "$COSMIC_WIN/target/Cosmic.jar" "$COSMIC_UNIX/target/Cosmic.jar" Cosmic.jar || FAILED=$((FAILED + 1))
send "$BUILD_WIN/wz.tar" "$BUILD_UNIX/wz.tar" wz.tar || FAILED=$((FAILED + 1))
send "$BUILD_WIN/scripts.tar" "$BUILD_UNIX/scripts.tar" scripts.tar || FAILED=$((FAILED + 1))
# THE CONFIG, WITH THIS DEVICE'S REAL ADDRESS WRITTEN INTO IT.
#
# Cosmic tells a joining client where to reconnect for the channel server,
# and picks the address by where the client came from - LOCALHOST for the
# host itself, LANHOST for anybody else on the network (Server.getInetSocket).
# LANHOST is a fixed string in config.yaml and NOTHING ever worked out what
# address this handheld actually has.
#
# So it goes stale the moment the router hands out a new lease, and then the
# host can still play while everybody else logs in, picks a character, presses
# Start and sits there - reconnecting to an address with nothing on it. The
# header of this script has described that failure since it was written and
# never did anything about it. This is the something.
#
# Derived from the device rather than asked for, because the person running
# this does not know the handheld's address either, and a number typed in is a
# number that goes stale again tomorrow.
LANIP=$(adb -s "$DEV" shell "ip route get 1.1.1.1 2>/dev/null" \
	| tr -d '\r' | sed -n 's/.* src \([0-9.]*\).*/\1/p' | head -1)

if [ -n "$LANIP" ]; then
	q "  this device is $LANIP - writing it into the config"

	STAGED_CONF="$BUILD_UNIX/config.staged.yaml"

	sed -e "s/^\( *HOST:\) *[0-9.]*/\1 $LANIP/" \
	    -e "s/^\( *LANHOST:\) *[0-9.]*/\1 $LANIP/" \
	    "$COSMIC_UNIX/config.yaml" > "$STAGED_CONF"

	# Never ship a config we failed to rewrite - an empty one would take the
	# server down completely rather than merely misdirect it.
	if [ -s "$STAGED_CONF" ] && grep -q "LANHOST: $LANIP" "$STAGED_CONF"; then
		send "$BUILD_WIN/config.staged.yaml" "$STAGED_CONF" config.yaml \
			|| FAILED=$((FAILED + 1))
	else
		echo "  FAILED to write the address in - sending the config unchanged"
		send "$COSMIC_WIN/config.yaml" "$COSMIC_UNIX/config.yaml" config.yaml \
			|| FAILED=$((FAILED + 1))
	fi
else
	echo "  could not read this device's address - config sent unchanged,"
	echo "  which means anyone joining OVER THE NETWORK may not get in"
	send "$COSMIC_WIN/config.yaml" "$COSMIC_UNIX/config.yaml" config.yaml || FAILED=$((FAILED + 1))
fi
send "$HERE_WIN/tools/termux_setup.sh" "$HERE_UNIX/tools/termux_setup.sh" termux_setup.sh || FAILED=$((FAILED + 1))

# The one the GAME runs. See tools/bootstrap.sh for why it is not run.sh.
send "$HERE_WIN/tools/bootstrap.sh" "$HERE_UNIX/tools/bootstrap.sh" bootstrap.sh || FAILED=$((FAILED + 1))

q

if [ "$FAILED" -gt 0 ]; then
	echo "$FAILED file(s) did not arrive. Nothing else to do until they do."
	exit 1
fi

# --- stop Android killing the server -------------------------------------
#
# Two separate mechanisms, and BOTH produce the same symptom: the game plays
# for a minute or two, the connection drops, and the server log ends with
# nothing but "Killed".
#
#   the phantom process killer  counts child processes of an app - which is
#                               what mariadbd and java are under Termux - and
#                               kills them for using CPU in the background.
#
#   app standby and Doze        kill Termux ITSELF, and the server dies with
#                               its parent. run.sh takes a wake lock and the
#                               wake lock genuinely works, but a partial wake
#                               lock only stops the CPU sleeping; it says
#                               nothing about whether the app may be killed.
#
# Done here rather than left in the README, because a step that must be
# remembered per device is a step that will be forgotten - and the way it
# announces itself is a dropped game an hour later that looks like a network
# fault. Neither needs root.
q
q "[$DEV] telling Android to leave the server alone"

adb -s "$DEV" shell "settings put global settings_enable_monitor_phantom_procs false" >/dev/null 2>&1
adb -s "$DEV" shell "/system/bin/device_config set_sync_disabled_for_tests persistent" >/dev/null 2>&1
adb -s "$DEV" shell "/system/bin/device_config put activity_manager max_phantom_processes 2147483647" >/dev/null 2>&1
adb -s "$DEV" shell "dumpsys deviceidle whitelist +com.termux" >/dev/null 2>&1

PHANTOM=$(adb -s "$DEV" shell "settings get global settings_enable_monitor_phantom_procs" 2>/dev/null | tr -d '\r\n')
DOZE=$(adb -s "$DEV" shell "dumpsys deviceidle whitelist" 2>/dev/null | grep -c termux)

q "  phantom process killer : ${PHANTOM:-unknown}   (want: false)"
q "  termux exempt from doze: $([ "${DOZE:-0}" -gt 0 ] && echo yes || echo NO)"

if [ "${PHANTOM:-}" != "false" ] || [ "${DOZE:-0}" -eq 0 ]; then
	echo "  ⚠ one of these did not stick - the server will be killed mid-game."
fi

# ⚠ THE BY-HAND ROUTE, AND ONLY WHEN SOMEBODY IS ACTUALLY DOING IT BY HAND.
#
# This used to print unconditionally, so a run through tools/install.sh ended
# in a page of steps it had just performed itself, under a warning to ignore
# them. Instructions that do not apply are worse than none: they leave the
# person unsure whether the thing they just watched happen actually happened.
#
# adb CAN reach Termux, through `run-as`, on a DEBUGGABLE Termux build - that
# is how install.sh does all of this without anybody typing. A stock F-Droid
# Termux is not debuggable, and falls back to exactly these steps.
if [ "$QUIET" = "1" ]; then
	echo "  staged"
	exit 0
fi

cat <<'NEXT'
Everything is on the device, in /sdcard/Download/cosmic.

To finish it by hand:

  1. Install Termux from F-Droid. NOT the Play Store version: it is years
     old and its package repository no longer resolves.

  2. Open it and run, in this order:

        termux-setup-storage          # then grant it, on screen
        cp /sdcard/Download/cosmic/termux_setup.sh ~
        bash ~/termux_setup.sh

     Java, MariaDB, the server and its schema. Twenty minutes.

  3. When it finishes:

        ~/cosmic/run.sh

  4. Set ServerIP = 127.0.0.1 in the game settings on that device. Nothing
     else changes - the same server still answers everyone else on the
     network at this handheld's LAN address.
NEXT
