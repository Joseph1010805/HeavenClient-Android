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

# adb is not on PATH in a plain shell here, and without this the very first
# check fails and blames the DEVICE - "Cannot reach <serial>. Is USB debugging
# authorised?" - when the truth is that the command does not exist. That is a
# wrong answer pointing at the wrong thing, which is worse than no answer.
if ! command -v adb >/dev/null 2>&1; then
	# Braced with defaults: this script runs under `set -u`, and USER is not
	# set in every shell here - an unset one aborts the whole run with
	# "unbound variable" before it has done anything.
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

# Both forms of the repo path. adb needs the Windows one; the shell needs the
# Unix one. `pwd -W` is a Git Bash extension, so fall back for other shells.
REPO_UNIX="$(cd "$(dirname "$0")/.." && pwd)"
REPO="$(cd "$REPO_UNIX" && pwd -W 2>/dev/null)"
[ -n "$REPO" ] || REPO="$REPO_UNIX"

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
	echo "adb is present but $DEV does not answer."
	echo
	echo "Devices adb can see:"
	adb devices | tail -n +2
	echo
	echo "If it is listed as 'unauthorized', accept the prompt on the headset."
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
# adb cannot CREATE a directory under /sdcard/Android/data on Android 11 and
# up - "remote secure_mkdirs failed: Operation not permitted" - though it can
# write files into one that already exists. That is why the .nx files landed
# and the fonts silently did not: they go one level deeper, into a folder that
# had to be made first.
#
# The shell CAN make it, so make it, then push the files rather than the
# directory. The error was hidden behind >/dev/null, so on the Quest this
# produced a complete-looking install whose text would simply never appear.
# Everything adb has just written is owned by SHELL, and the folder adb
# created has no world permissions at all - drwxrws---. The game runs as a
# different user, so it cannot even traverse in, and reports "Missing nx file"
# for files that are plainly there.
#
# This is the chmod the README calls not optional, applied to the whole tree
# rather than to the fonts alone. Without it a 4.5 GB install sits there
# perfectly and the game shows a black screen.
sh "chmod -R a+rX '$DIR'" >/dev/null 2>&1

sh "mkdir -p '$DIR/fonts/Roboto'" >/dev/null 2>&1

for font in "$REPO"/fonts/Roboto/*; do
	[ -f "$font" ] || continue
	adb -s "$DEV" push "$font" "$DIR/fonts/Roboto/" >/dev/null 2>&1
done
if [ "$(sh "ls '$DIR/fonts' 2>/dev/null | wc -l" | tr -d '\r')" -gt 0 ]; then
	# a+rX, not 644 - a flat 644 on the directory itself makes it
	# untraversable and the fonts inside unreachable.
	sh "chmod -R a+rX '$DIR'" >/dev/null 2>&1
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
# Staged in the repo rather than /tmp: adb is being given raw paths here, and
# a Unix /tmp/... is exactly the thing it cannot resolve.
printf 'ServerIP = %s\nServerPort = 8484\nWidth = 800\nHeight = 600\n' "$SERVER_IP" > "$REPO_UNIX/.Settings.tmp"
adb -s "$DEV" push "$REPO/.Settings.tmp" "$DIR/Settings" >/dev/null 2>&1
rm -f "$REPO_UNIX/.Settings.tmp"

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
