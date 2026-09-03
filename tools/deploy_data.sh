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
# Only touch ServerIP when one is GIVEN. Defaulting it and then writing that
# default over a working device is how the Thor - which hosts, and so wants
# 127.0.0.1 - got pointed at the PC and spent an evening unable to log in.
SERVER_IP="${2:-}"
SERVER_IP_GIVEN=1
[ -z "$SERVER_IP" ] && SERVER_IP=127.0.0.1 && SERVER_IP_GIVEN=0

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
# WHERE THE DATA IS. Overridable, because tools/install.sh runs on somebody
# else's machine where none of these paths exist. The defaults are this
# machine's, so every existing invocation keeps working untouched.
V83="${MAPLE_DATA:-C:/Users/Deck/maple/wz-v83}"
V178="${MAPLE_UI:-C:/Users/Deck/maple/wz-v178}"
CUSTOM="${MAPLE_CUSTOM:-C:/Users/Deck/maple}"

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

	# ⚠ KEEP WHAT ADB SAID. Do not send it to /dev/null.
	#
	# Both pushes here used to discard stdout AND stderr, so when every file
	# in a fresh install landed at zero bytes the only thing anybody had to
	# work with was "expected 13296, got 0" - seventeen times. The reason was
	# in the message that had just been thrown away.
	#
	# It costs one temporary file per attempt and it is the difference
	# between a diagnosis and an afternoon.
	local err
	err=$(mktemp 2>/dev/null || echo "/tmp/deploy.$$")

	# Straight in, if the device allows it.
	adb -s "$DEV" push "$src" "$DIR/$name" >"$err" 2>&1
	sh "chmod 644 '$DIR/$name'" >/dev/null 2>&1
	have=$(remote_size "$DIR/$name")

	if [ "$want" != "$have" ]; then
		# Some devices refuse a direct write to an app's data folder and
		# say nothing about it. Land it somewhere permissive and move it -
		# a move, not a copy, so a 1.5 GB file needs no extra room.
		printf 'via staging... '
		adb -s "$DEV" push "$src" "$STAGE/$name" >>"$err" 2>&1
		sh "mv '$STAGE/$name' '$DIR/$name' && chmod 644 '$DIR/$name'" >>"$err" 2>&1
		have=$(remote_size "$DIR/$name")
	fi

	if [ "$want" = "$have" ]; then
		rm -f "$err"
		echo "ok"
		return 0
	fi

	echo "FAILED (device has $have bytes, expected $want)"

	# Indented under the failure so it reads as belonging to it, and only on
	# a failure so a good run stays quiet.
	sed 's/^/      /' "$err" 2>/dev/null | grep -v '^ *$' | tail -6
	rm -f "$err"

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
	sh "chmod 666 '$DIR/Settings' 2>/dev/null" >/dev/null 2>&1

sh "mkdir -p '$DIR/fonts/Roboto'" >/dev/null 2>&1

for font in "$REPO"/fonts/Roboto/*; do
	[ -f "$font" ] || continue
	adb -s "$DEV" push "$font" "$DIR/fonts/Roboto/" >/dev/null 2>&1
done
if [ "$(sh "ls '$DIR/fonts' 2>/dev/null | wc -l" | tr -d '\r')" -gt 0 ]; then
	# a+rX, not 644 - a flat 644 on the directory itself makes it
	# untraversable and the fonts inside unreachable.
	sh "chmod -R a+rX '$DIR'" >/dev/null 2>&1
	sh "chmod 666 '$DIR/Settings' 2>/dev/null" >/dev/null 2>&1
	echo "  ok"
else
	echo "  FAILED"
	FAILED=$((FAILED + 1))
fi

# THE SPEECH MODEL.
#
# None of these machines has a keyboard, so saying a sentence beats spelling it
# out with a thumbstick. Recognition is Vosk, entirely on the device - nothing
# recorded ever leaves the house, and it works with the router unplugged.
#
# Here rather than in the apk: it is ~68MB unpacked, it is not ours to put on a
# release page, and this is already the pipeline for large data. When it is
# absent SpeechInput reports itself unavailable and the microphone buttons stay
# quiet, which is why this is a warning and not a failure.
#
#   curl -LO https://alphacephei.com/vosk/models/vosk-model-small-en-us-0.15.zip
#   unzip it next to the wz folder and rename it to  vosk-model
echo
echo "Speech model:"
MODEL="$(dirname "$V83")/vosk-model"

if [ -d "$MODEL" ]; then
	# Via staging, like the .nx files, and for a sharper reason than they
	# have: pushing a DIRECTORY straight into Android/data fails per-file with
	# "remote fchown failed: Operation not permitted" and then dies with
	# "failed to read copy response: EOF" - having already created the folders,
	# so it looks like it half worked. Landing it somewhere permissive and
	# moving it sidesteps the ownership rules entirely.
	sh "rm -rf '$STAGE/vosk-model'" >/dev/null 2>&1
	adb -s "$DEV" push "$MODEL" "$STAGE/" >/dev/null 2>&1
	sh "rm -rf '$DIR/vosk-model' && mv '$STAGE/vosk-model' '$DIR/vosk-model'" >/dev/null 2>&1
	sh "chmod -R a+rX '$DIR'" >/dev/null 2>&1
	sh "chmod 666 '$DIR/Settings' 2>/dev/null" >/dev/null 2>&1

	if [ "$(sh "ls '$DIR/vosk-model' 2>/dev/null | wc -l" | tr -d '')" -gt 0 ]; then
		echo "  ok"
	else
		echo "  FAILED"
		FAILED=$((FAILED + 1))
	fi
else
	echo "  none at $MODEL - speech input will be unavailable (not an error)"
fi

# Four lines is all the client needs.
#
# 1280x720 now, not 800x600. It was 800x600 because "the login and character
# screens were drawn for it and do not adapt" - which was true: each of them
# hardcoded 800x600 twice over, for its own size and for the rectangle it
# stretched its background into, so at any other resolution the artwork covered
# the top-left corner and the rest was empty. Those are the view size now.
#
# 720p is also what the client itself defaults to, and what Window_Android's
# offscreen buffer was written around - it scales 1280x720 to a 1920x1080 panel
# by exactly 1.5, where 800x600 had to be stretched unevenly (2.4x across, 1.8x
# down) and the game was a third too wide for as long as anyone can remember.
#
# The catch: at 1.5x everything is SMALLER than it was at 2.4x. The font scale
# in GraphicsGL is the counterweight for text; artwork still wants its own.
echo
if [ "$SERVER_IP_GIVEN" -eq 1 ]; then
	echo "Settings (server $SERVER_IP):"
else
	echo "Settings (no address given - leaving ServerIP alone):"
fi
# Staged in the repo rather than /tmp: adb is being given raw paths here, and
# a Unix /tmp/... is exactly the thing it cannot resolve.
# WHICH ADDRESS THIS DEVICE SHOULD USE.
#
# The default here is the PC. A device that HOSTS its own server - the Thor
# does - must point at 127.0.0.1 instead, or it spends the whole login talking
# to a machine that is not running a server: the character list never arrives
# and pressing Start gives "lost connection".
#
# Getting this wrong looks exactly like data loss. It is not; the characters
# are in the database on the host the whole time.
#
#   tools/deploy_data.sh <serial> 127.0.0.1     # a device that hosts
#   tools/deploy_data.sh <serial> 192.168.1.184 # a device that joins the Thor
#
# DO NOT overwrite a Settings that already exists.
#
# This used to write a fixed Width/Height every run, and on 31 August 2026 a
# deploy silently put the client back to 1280x720 - a resolution that had been
# tried and DECIDED AGAINST in favour of 800x600. Nothing announced it; the
# game simply came back the wrong size.
#
# Settings is the player's file, not the deploy's. The only thing this script
# has any business setting is the server address, and only when there is no
# file at all to read one from.
if [ "$(remote_size "$DIR/Settings")" -gt 0 ]; then
	echo "  kept (already on the device - resolution and bindings are the player's)"

	# Still worth making sure it points at this machine.
	if [ "$SERVER_IP_GIVEN" -eq 1 ]; then
		sh "sed -i 's/^ServerIP = .*/ServerIP = $SERVER_IP/' '$DIR/Settings'" >/dev/null 2>&1
		echo "  ServerIP set to $SERVER_IP"
	else
		echo "  ServerIP left as $(sh "grep '^ServerIP' '$DIR/Settings'" | tr -d '' | awk '{print $3}')"
	fi

	SETTINGS_DONE=1
else
	SETTINGS_DONE=0
fi

if [ "$SETTINGS_DONE" -eq 0 ]; then
printf 'ServerIP = %s\nServerPort = 8484\nWidth = 800\nHeight = 600\n' "$SERVER_IP" > "$REPO_UNIX/.Settings.tmp"

# THROUGH THE STAGE, like everything else.
#
# This used to push straight into $DIR, which scoped storage refuses:
#
#     adb: error: failed to copy ...: remote fchown failed: ...
#     1 file pushed, 0 skipped.
#
# Both lines. The push TRUNCATES the Settings already on the device and then
# fails, so a run left the device with no settings at all while reporting
# only that one item "did not make it" - and the .nx files above were already
# doing this correctly, which is what hid it.
adb -s "$DEV" push "$REPO/.Settings.tmp" "$STAGE/Settings" >/dev/null 2>&1
# 666, NOT 644. Everything else here is read-only data the game only
# reads, but Settings is the one file it WRITES - the saved login, the
# window positions, the key bindings. adb pushes land owned by `shell`
# and the game runs as a different user, so at 644 it can read its own
# settings and never save them: "Save ID" ticks and nothing persists.
sh "mv '$STAGE/Settings' '$DIR/Settings' && chmod 666 '$DIR/Settings'" >/dev/null 2>&1
rm -f "$REPO_UNIX/.Settings.tmp"
fi

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
