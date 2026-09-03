#!/usr/bin/env bash
#
# PUT THE GAME ON A HANDHELD.
#
#   tools/install.sh                  # find a device, ask, install
#   tools/install.sh --device SERIAL  # skip the picking
#   tools/install.sh --server         # also put Cosmic on it, for offline play
#
# What this is for: somebody who has a Thor, an RP5 or any other Android
# device, a USB cable, and no interest in learning what adb is.
#
# ────────────────────────────────────────────────────────────────────────────
# THE GAME DATA IS NOT DOWNLOADED. IT IS NEVER DOWNLOADED.
#
# The .nx files are converted from Nexon's .wz files. They are Nexon's work.
# Hosting them, or pointing this script at somebody else's copy of them, is
# distributing Nexon's work either way, and that is the whole reason `*.nx`
# is in .gitignore and no release has ever carried one.
#
# So this script FINDS data the user already has. It looks in the usual
# places, it will take a path, and if it finds .wz files instead of .nx it
# offers to convert them - on the user's own machine, from the user's own
# client. What it fetches from the network is only ever OURS: the APK, the
# Cosmic jar, the scripts, the Termux setup.
#
# If you are reading this because you want a one-click installer that
# "just works" with no client of your own: that installer cannot exist
# legally, and this one deliberately does not pretend to be it.
# ────────────────────────────────────────────────────────────────────────────
#
set -u

# ⚠ MSYS_NO_PATHCONV IS NOT SET GLOBALLY HERE, AND MUST NOT BE.
#
# Git Bash rewrites anything that looks like a Unix path into a Windows one.
# That is WRONG for a device path - "/sdcard/Foo" reaches adb as
# "C:/.../Git/sdcard/Foo" - and RIGHT for a local one, because adb.exe is a
# Windows program and cannot open "/c/Users/...".
#
# Turning it off for the whole script fixes the first and breaks the second:
# every device push works and every local file "does not exist". The scripts
# this one calls each set it for themselves around their own adb calls, which
# is the only place it belongs.
#
# So: local paths in WINDOWS form, device paths quoted per-call.
HERE="$(cd "$(dirname "$0")/.." && pwd)"
HERE_WIN="$(cd "$HERE" && pwd -W 2>/dev/null)"
[ -n "$HERE_WIN" ] || HERE_WIN="$HERE"

# A LOCAL PATH AS A WINDOWS PROGRAM NEEDS TO SEE IT.
#
# /c/Users/... is a Git Bash fiction. adb.exe cannot open it, and neither
# can anything else that is not part of the MSYS world. Normally Git Bash
# rewrites such an argument on its way out - but deploy_data.sh turns that
# rewriting OFF for its whole run (it has to; every path it hands adb after
# that is a device path), so anything given to it must already be in
# Windows form.
#
# This is what converts one. `pwd -W` is the MSYS way of asking "where is
# this really"; anywhere else the answer is the path itself, unchanged.
win_path() {
	( cd "$1" 2>/dev/null && pwd -W 2>/dev/null ) || printf '%s' "$1"
}

say()  { printf '\n\033[1m%s\033[0m\n' "$*"; }
step() { printf '  %s\n' "$*"; }
bad()  { printf '\n\033[31m%s\033[0m\n' "$*"; }
ask()  { printf '%s ' "$*"; read -r REPLY; }

# ---------------------------------------------------------------------------
# adb
# ---------------------------------------------------------------------------
find_adb() {
	if command -v adb >/dev/null 2>&1; then
		return 0
	fi

	# The place the Android SDK puts it on Windows, which is where it is if
	# the user has ever installed Android Studio.
	#
	# EVERY ONE OF THESE IS BRACED WITH A DEFAULT, and that is not
	# decoration. This script runs under `set -u`, and USER is not set in
	# every shell Git Bash starts - so a bare "$USER" aborted the whole
	# installer on this line, before it had looked at a device, printed a
	# word, or done anything a person could learn from. It was the first
	# thing that happened the first time anybody ran it.
	#
	# Windows sets USERNAME, not USER. deploy_data.sh already learned
	# this; this script had not.
	for guess in \
		"${LOCALAPPDATA:-}/Android/Sdk/platform-tools" \
		"${HOME:-}/AppData/Local/Android/Sdk/platform-tools" \
		"/c/Users/${USERNAME:-${USER:-}}/AppData/Local/Android/Sdk/platform-tools"
	do
		if [ -x "$guess/adb.exe" ] || [ -x "$guess/adb" ]; then
			PATH="$guess:$PATH"
			export PATH
			return 0
		fi
	done

	return 1
}

if ! find_adb; then
	bad "adb was not found."
	cat <<'HELP'

  adb is the tool that talks to an Android device over USB. It comes with
  Android's "platform tools", a 10MB download that needs no account:

      https://developer.android.com/tools/releases/platform-tools

  Unzip it anywhere, then run this script again from a terminal that has
  that folder on its PATH - or just drop adb.exe next to this script.

HELP
	exit 1
fi

# ---------------------------------------------------------------------------
# The device
# ---------------------------------------------------------------------------
DEV=""
WANT_SERVER=0
GIVEN_DATA=""

while [ $# -gt 0 ]; do
	case "$1" in
	--device) DEV="${2:-}"; shift 2 ;;
	--data)   GIVEN_DATA="${2:-}"; shift 2 ;;
	--server) WANT_SERVER=1; shift ;;
	*) bad "unknown option: $1"; exit 1 ;;
	esac
done

pick_device() {
	adb start-server >/dev/null 2>&1

	# Serial and state, one per line, only the ones actually ready. A device
	# that is "unauthorized" is plugged in and has not had the prompt
	# accepted on its screen, which is worth saying out loud rather than
	# reporting as "no devices".
	local ready unauth
	ready=$(adb devices | awk '$2 == "device" { print $1 }')
	unauth=$(adb devices | awk '$2 == "unauthorized" { print $1 }')

	if [ -n "$unauth" ]; then
		bad "A device is connected but has not been allowed."
		cat <<'HELP'

  Look at the device's screen: there is a prompt asking whether to allow
  USB debugging from this computer. Tick "always allow" and accept it,
  then run this again.

HELP
		exit 1
	fi

	if [ -z "$ready" ]; then
		bad "No device found."
		cat <<'HELP'

  Check, in this order:

    1. The cable carries DATA, not just power. A charging cable will show
       nothing at all here and is the usual cause.
    2. Developer options are on: Settings > About > tap "Build number"
       seven times.
    3. USB debugging is on, in Developer options.
    4. The device is unlocked, with its screen on.

HELP
		exit 1
	fi

	local count
	count=$(printf '%s\n' "$ready" | wc -l)

	if [ "$count" -eq 1 ]; then
		DEV="$ready"
		return 0
	fi

	say "More than one device is connected."

	local n=1
	printf '%s\n' "$ready" | while read -r one; do
		printf '  %d) %-24s %s\n' "$n" "$one" \
			"$(adb -s "$one" shell getprop ro.product.model 2>/dev/null | tr -d '\r')"
		n=$((n + 1))
	done

	ask "Which one? (number)"
	DEV=$(printf '%s\n' "$ready" | sed -n "${REPLY}p")

	if [ -z "$DEV" ]; then
		bad "Not one of the choices."
		exit 1
	fi
}

[ -n "$DEV" ] || pick_device

MODEL=$(adb -s "$DEV" shell getprop ro.product.model 2>/dev/null | tr -d '\r')
ANDROID=$(adb -s "$DEV" shell getprop ro.build.version.release 2>/dev/null | tr -d '\r')
ABI=$(adb -s "$DEV" shell getprop ro.product.cpu.abi 2>/dev/null | tr -d '\r')

say "Installing to: $MODEL  (Android $ANDROID, $ABI)"

# The APK is arm64 only. Saying so now is kinder than a failed install with
# INSTALL_FAILED_NO_MATCHING_ABIS, which explains nothing to anybody.
case "$ABI" in
arm64*) ;;
*)
	bad "This device is $ABI. The game is built for arm64 only."
	exit 1
	;;
esac

# ---------------------------------------------------------------------------
# The APK
# ---------------------------------------------------------------------------
APK=""

# FOUND IN UNIX FORM, HANDED OVER IN WINDOWS FORM.
#
# `[ -f ... ]` is bash and wants /c/Users/...; adb.exe is a Windows
# program and cannot open that path at all. Git Bash normally rewrites
# the argument in between and hides the difference - until something in
# the caller's environment has turned that rewriting off, and then the
# installer reports "the device refused it" about a file adb never
# managed to look at.
#
# So the test uses the Unix path and the handover uses the Windows one,
# and neither depends on the shell being helpful. HERE_WIN is computed
# at the top for exactly this; the download branch below already used it
# and this branch did not.
for guess in \
	"android/app/build/outputs/apk/release/app-release.apk" \
	"android/app/build/outputs/apk/debug/app-debug.apk" \
	"LocalStory.apk"
do
	if [ -f "$HERE/$guess" ]; then
		APK="$HERE_WIN/$guess"
		break
	fi
done

if [ -z "$APK" ]; then
	say "Fetching the app"
	step "no local build found, downloading the latest release"

	# OURS to distribute - the client is AGPL-3.0 and the release page is the
	# project's own. Nothing here touches game data.
	URL=$(curl -fsSL https://api.github.com/repos/Joseph1010805/HeavenClient-Android/releases/latest \
		| grep -oE '"browser_download_url": *"[^"]*\.apk"' \
		| head -1 | sed 's/.*"\(https[^"]*\)"/\1/')

	if [ -z "$URL" ]; then
		bad "Could not find a release to download, and no local build exists."
		step "Build one with:  cd android && ./gradlew assembleDebug"
		exit 1
	fi

	APK="$HERE_WIN/LocalStory.apk"

	if ! curl -fL --progress-bar -o "$HERE/LocalStory.apk" "$URL"; then
		bad "The download failed."
		exit 1
	fi
fi

say "Installing the app"
step "$(basename "$APK")"

# ⚠ SAY WHAT ADB SAID.
#
# This used to be `install ... | grep -q Success`, which throws the reason
# away and then advises the user to guess at signatures. adb's own message
# names the fault every time - INSTALL_FAILED_UPDATE_INCOMPATIBLE,
# _NO_MATCHING_ABIS, _INSUFFICIENT_STORAGE - and none of that reached anybody.
#
# AND IT RETRIES ONCE. An install issued immediately after an uninstall can
# be refused while Android is still tearing the old package down; a couple of
# seconds later the identical command succeeds. That is not worth making a
# person diagnose.
INSTALL_SAID=""

for try in 1 2; do
	INSTALL_SAID=$(adb -s "$DEV" install -r "$APK" 2>&1)

	case "$INSTALL_SAID" in
	*Success*) break ;;
	esac

	if [ "$try" -eq 1 ]; then
		step "the device refused it - waiting a moment and trying once more"
		sleep 4
	fi
done

case "$INSTALL_SAID" in
*Success*) ;;
*)
	bad "The install failed. This is what the device said:"
	printf '%s
' "$INSTALL_SAID" | sed 's/^/      /'
	echo
	step "INSTALL_FAILED_UPDATE_INCOMPATIBLE means an older copy signed with a"
	step "different key is in the way. Remove it first:"
	step "  adb -s $DEV uninstall org.heavenclient.android"
	exit 1
	;;
esac

step "installed"

# ---------------------------------------------------------------------------
# The game data - FOUND, never fetched. See the banner at the top.
# ---------------------------------------------------------------------------
NEEDED="Base Character Effect Etc Item Map Mob Morph Npc Quest Reactor Skill Sound String TamingMob"

has_all_nx() {
	local dir="$1" f
	for f in $NEEDED; do
		[ -f "$dir/$f.nx" ] || return 1
	done
	return 0
}

has_wz() {
	[ -f "$1/Base.wz" ] && [ -f "$1/Character.wz" ]
}

DATA=""

say "Looking for the game data"

# A path given on the command line is taken at its word, and complained about
# clearly if it is wrong. Falling back to a search would hide a typo behind
# ten minutes of copying something else.
if [ -n "$GIVEN_DATA" ]; then
	if has_all_nx "$GIVEN_DATA"; then
		DATA="$GIVEN_DATA"
		step "using: $DATA"
	else
		bad "$GIVEN_DATA does not hold a full set of .nx files."
		step "expected all of: $NEEDED"
		exit 1
	fi
fi

for guess in \
	"${HOME:-}/maple/wz-v83" \
	"${HOME:-}/Documents/maple/wz-v83" \
	"/c/maple/wz-v83" \
	"/c/Nexon/MapleStory" \
	"/c/Program Files (x86)/Wizet/MapleStory" \
	"/c/Program Files/Wizet/MapleStory"
do
	[ -n "$DATA" ] && break

	if has_all_nx "$guess"; then
		DATA="$guess"
		step "found converted data: $DATA"
		break
	fi

	if has_wz "$guess"; then
		step "found a MapleStory client: $guess"
		step "it has .wz files, which have to be converted to .nx first"

		cat <<'HELP'

  Convert them with NoLifeWzToNx, which reads YOUR client and writes .nx
  beside it. It is a separate tool and it is not ours:

      https://github.com/NoLifeDev/NoLifeWzToNx

  Then run this script again.

HELP
		exit 1
	fi
done

if [ -z "$DATA" ]; then
	bad "No game data found."
	cat <<'HELP'

  This installer does not download the game data, and never will. The .nx
  files are converted from Nexon's .wz files - they are Nexon's work, not
  ours, and distributing them is not something this project does.

  You need a MapleStory v83 client of your own. Once you have one, either:

    * point this script at the converted .nx files:
          tools/install.sh --data /path/to/wz-v83

    * or convert your client's .wz files first, with NoLifeWzToNx.

  Everything ELSE - the app, the server, the scripts - this installer will
  fetch or build for you.

HELP
	exit 1
fi

# UI.nx is the awkward one: v83's own interface is too old and the client
# refuses to start on it, so it comes from a later client.
UI_FROM=""

for guess in "${HOME:-}/maple/wz-v178" "$(dirname "$DATA")/wz-v178" "$DATA"; do
	[ -f "$guess/UI.nx" ] && UI_FROM="$guess" && break
done

if [ -z "$UI_FROM" ]; then
	bad "UI.nx was not found."
	step "v83's own interface is too old - the client needs UI.nx from a"
	step "later client (v178 is what this project uses)."
	exit 1
fi

say "Copying the game data"
step "this is several gigabytes over USB - ten minutes or so"
step "from: $DATA"

# Map001.nx is the one data file that IS ours: the login videos, the panel
# icons and the gauge artwork, all built by tools/make_assets.py. On this
# machine it sits beside the borrowed data, so say where to look.
CUSTOM_FROM="$(dirname "$DATA")"
[ -f "$HERE/Map001.nx" ] && CUSTOM_FROM="$HERE_WIN"

# IN WINDOWS FORM. deploy_data.sh says so in its own header and it is not
# being fussy: it runs with MSYS_NO_PATHCONV on, so a /c/Users path reaches
# adb.exe unconverted and every single file fails with "cannot stat".
#
# This is why the installer had never once got past the app. Seventeen
# files, seventeen zero-byte failures, and the reason was a script two
# lines further down being handed the wrong kind of path.
if ! MAPLE_DATA="$(win_path "$DATA")" \
	MAPLE_UI="$(win_path "$UI_FROM")" \
	MAPLE_CUSTOM="$(win_path "$CUSTOM_FROM")" \
	bash "$HERE/tools/deploy_data.sh" "$DEV"; then
	bad "Copying the data failed. Nothing above it was wasted - run this again"
	bad "and it will skip whatever already arrived."
	exit 1
fi

# ---------------------------------------------------------------------------
# The server, if they want to play with no network at all
# ---------------------------------------------------------------------------
if [ "$WANT_SERVER" -eq 1 ]; then
	say "Putting the server on the device"
	bash "$HERE/tools/stage_server.sh" "$DEV" || exit 1
fi

say "Done."

cat <<DONE

  $MODEL now has the game.

  To play, open "Bugs 'n Beans Story" on it.

  Somebody has to be HOSTING for there to be a game to join. The login
  screen looks for one by itself - no addresses, nothing to type:

    * If a game is already running on your wifi, it appears under GAMES
      NEARBY. Tap its name and type the six-digit code the host reads out.

    * If there is no game yet, tap CREATE A GAME. You pick the six digits,
      and everyone else types them in. The device you create on is the one
      that has to stay switched on.

  To put a server on THIS device as well, so it can be the one hosting:

      tools/install.sh --device $DEV --server

DONE
