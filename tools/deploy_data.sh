#!/usr/bin/env bash
#
# Puts the game data on a device.
#
#   tools/deploy_data.sh <device-serial> [server-ip]
#
# Safe to re-run. Every file is checked by size against the device first and
# skipped if it is already there, so an interrupted run picks up where it
# stopped rather than sending 4.6 GB again.
#
# The data is Nexon's and is never distributed with this project - this script
# copies files you already have on your own machine.
#
set -u

# Git Bash rewrites any argument that looks like a Unix path into a Windows
# one, so "/sdcard/Foo" is handed to adb as "C:/.../Git/sdcard/Foo" and the
# push fails - after printing "1 file pushed", which is what makes it look
# like a device fault rather than a path fault. Do NOT set this globally:
# it breaks the Gradle wrapper. This script never calls Gradle.
export MSYS_NO_PATHCONV=1

DEV="${1:-}"
SERVER_IP="${2:-192.168.1.71}"

if [ -z "$DEV" ]; then
	echo "usage: $0 <device-serial> [server-ip]"
	echo
	echo "devices:"
	adb devices | tail -n +2
	exit 1
fi

# Source paths are in Windows form because MSYS_NO_PATHCONV is on above:
# with it set, adb gets whatever we type, and adb wants C:/... locally and
# /sdcard/... on the device. Mixing the two is the only combination that
# works from Git Bash.
V83="C:/Users/Deck/maple/wz-v83"
V178="C:/Users/Deck/maple/wz-v178"
CUSTOM="C:/Users/Deck/maple"
REPO="$(cd "$(dirname "$0")/.." && pwd -W 2>/dev/null || cd "$(dirname "$0")/.." && pwd)"

DIR=/sdcard/Android/data/org.heavenclient.android/files/HeavenClient
STAGE=/sdcard/Download

sh() { adb -s "$DEV" shell "$@"; }

# Size of a file on the device, or 0 if it isn't there.
remote_size() {
	sh "stat -c %s '$1' 2>/dev/null || echo 0" | tr -d '\r'
}

# Sends one file and proves it arrived. adb push has been seen reporting
# success while writing nothing, so the size is always checked afterwards.
send() {
	local src="$1" name="$2"
	local want have

	want=$(stat -c %s "$src")
	have=$(remote_size "$DIR/$name")

	if [ "$want" = "$have" ]; then
		printf '  %-14s %6s MB  already there\n' "$name" "$((want / 1024 / 1024))"
		return 0
	fi

	printf '  %-14s %6s MB  sending... ' "$name" "$((want / 1024 / 1024))"

	# Straight in, if the device allows it.
	adb -s "$DEV" push "$src" "$DIR/$name" >/dev/null 2>&1
	sh "chmod 644 '$DIR/$name'" >/dev/null 2>&1
	have=$(remote_size "$DIR/$name")

	if [ "$want" != "$have" ]; then
		# Some devices refuse a direct write to an app's data folder and
		# say nothing about it. Land it somewhere permissive and move it -
		# a move, not a copy, so a 1.5 GB file needs no extra room.
		printf 'via staging... '
		adb -s "$DEV" push "$src" "$STAGE/$name" >/dev/null 2>&1
		sh "mv '$STAGE/$name' '$DIR/$name' && chmod 644 '$DIR/$name'" >/dev/null 2>&1
		have=$(remote_size "$DIR/$name")
	fi

	if [ "$want" = "$have" ]; then
		echo "ok"
		return 0
	fi

	echo "FAILED (device has $have bytes, expected $want)"
	return 1
}

echo "Deploying to $DEV"
echo

if ! sh "echo ok" >/dev/null 2>&1; then
	echo "Cannot reach $DEV. Is USB debugging authorised on the device?"
	exit 1
fi

# The folder only exists once the app has been run at least once, so make it
# rather than assuming.
sh "mkdir -p '$DIR'" >/dev/null 2>&1

echo "Game data:"
FAILED=0

for f in Base Character Effect Etc Item Map Mob Morph Npc Quest Reactor Skill Sound String TamingMob; do
	send "$V83/$f.nx" "$f.nx" || FAILED=$((FAILED + 1))
done

# UI.nx has to come from a later client - v83's interface is too old and the
# client refuses to start on it.
send "$V178/UI.nx" "UI.nx" || FAILED=$((FAILED + 1))

# Custom artwork for the login and character screens. Ours, not Nexon's.
send "$CUSTOM/Map001.nx" "Map001.nx" || FAILED=$((FAILED + 1))

# Without the fonts, text simply does not appear - no error, no warning.
echo
echo "Fonts:"
adb -s "$DEV" push "$REPO/fonts" "$DIR/" >/dev/null 2>&1
if [ "$(sh "ls '$DIR/fonts' 2>/dev/null | wc -l" | tr -d '\r')" -gt 0 ]; then
	sh "chmod -R 644 '$DIR/fonts'/*" >/dev/null 2>&1
	echo "  ok"
else
	echo "  FAILED"
	FAILED=$((FAILED + 1))
fi

# Four lines is all the client needs. 800x600 because the login and character
# screens were drawn for it and do not adapt; the picture is scaled up to fill
# the display anyway.
echo
echo "Settings (server $SERVER_IP):"
printf 'ServerIP = %s\nServerPort = 8484\nWidth = 800\nHeight = 600\n' "$SERVER_IP" > /tmp/hc_settings
adb -s "$DEV" push /tmp/hc_settings "$STAGE/Settings" >/dev/null 2>&1
sh "mv '$STAGE/Settings' '$DIR/Settings' && chmod 644 '$DIR/Settings'" >/dev/null 2>&1
rm -f /tmp/hc_settings

if [ "$(remote_size "$DIR/Settings")" -gt 0 ]; then
	echo "  ok"
else
	echo "  FAILED"
	FAILED=$((FAILED + 1))
fi

echo
echo "On the device:"
sh "ls -la '$DIR'" | tr -d '\r'

echo
if [ "$FAILED" -eq 0 ]; then
	echo "All present."
else
	echo "$FAILED item(s) did not make it. Re-run - it only sends what is missing."
	exit 1
fi
