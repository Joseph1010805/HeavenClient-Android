#!/usr/bin/env bash
#
# PUT THE GAME ON A HANDHELD.
#
#   tools/install.sh                  # find a device, ask, install
#   tools/install.sh --device SERIAL  # skip the picking
#   tools/install.sh --data DIR       # where your .nx or .wz files are
#   tools/install.sh --server         # also put Cosmic on it, for offline play
#   tools/install.sh --cosmic DIR     # where YOUR built Cosmic is
#   tools/install.sh --converter EXE # your built NoLifeWzToNx, to convert .wz
#   tools/install.sh --ui DIR        # where your v178 client is (for UI.nx)
#
# --server looks for a built Cosmic in the usual places beside this checkout;
# --cosmic says where when it is somewhere else. Both the jar and its wz files
# are yours to supply, for the same reason the game data is.
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

# A NAMED STEP WITH ITS ANSWER ON THE SAME LINE.
#
#     Checking adb ................. ok
#
# The dots are not decoration. A step that announces itself and then says
# nothing is exactly how this project keeps losing reasons; a line that has
# to end in its own result cannot do it.
DOTS='..............................'
checking() { printf '  %s %s ' "$1" "${DOTS:$((${#1} + 1))}"; }
ok()       { printf '\033[32m%s\033[0m\n' "${1:-ok}"; }
nope()     { printf '\033[31m%s\033[0m\n' "${1:-FAILED}"; }

# ⚠ EVERY adb CALL GETS ITS OWN STDIN. THIS IS NOT A STYLE CHOICE.
#
# `adb shell` READS STDIN, and takes whatever is there whether it wants it or
# not. Three getprops run between "Looking for a device" and "Install to X?",
# and they swallowed the answer to a question that had not been asked yet - so
# the confirmation was answered by EOF, EOF read as yes, and the install went
# ahead on a device nobody had confirmed. Caught by answering "n" and watching
# it install anyway.
#
# The same bug eats the device LIST in the picker below, where the loop's
# stdin is the list itself.
#
# So nothing here calls adb directly any more. These close stdin, once.
dev()  { adb -s "$DEV" "$@" </dev/null; }
adbq() { adb "$@" </dev/null; }

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

# ---------------------------------------------------------------------------
# The device
# ---------------------------------------------------------------------------
DEV=""
WANT_SERVER=0
GIVEN_DATA=""
GIVEN_COSMIC=""
GIVEN_CONV=""
GIVEN_UI=""

while [ $# -gt 0 ]; do
	case "$1" in
	--device) DEV="${2:-}"; shift 2 ;;
	--data)   GIVEN_DATA="${2:-}"; shift 2 ;;
	--cosmic) GIVEN_COSMIC="${2:-}"; shift 2 ;;
	--converter) GIVEN_CONV="${2:-}"; shift 2 ;;
	--ui)        GIVEN_UI="${2:-}"; shift 2 ;;
	--server) WANT_SERVER=1; shift ;;
	*) bad "unknown option: $1"; exit 1 ;;
	esac
done

# -------------------------------------------------------------------------
# ONE THING AT A TIME, AND SAY WHAT HAPPENED TO IT.
#
# Everything below asks, checks, and names the answer on the same line. This
# used to narrate instead - what the .nx files are, why they are never
# downloaded, how to stage the server by hand - and the one line that
# mattered on the night it failed ("no device") sat inside four paragraphs
# that did not. The explanation belongs in comments like this one and in the
# README. The run itself should read like a checklist.
# -------------------------------------------------------------------------
say "Connect the handheld"
step "The cable has to carry DATA. A charge-only cable shows up as nothing"
step "at all, and is the usual reason a device is not found."
printf '\n'
ask "  Plugged in? Press ENTER to continue."
printf '\n'

checking "Checking adb"
if ! find_adb; then
	nope "not found"
	cat <<'HELP'

  adb is what talks to the device over USB. It comes with Android's
  "platform tools" - a 10MB download, no account needed:

      https://developer.android.com/tools/releases/platform-tools

  Unzip it anywhere, put that folder on your PATH, and run this again.

HELP
	exit 1
fi
ok


pick_device() {
	adbq start-server >/dev/null 2>&1

	# Serial and state, one per line, only the ones actually ready. A device
	# that is "unauthorized" is plugged in and has not had the prompt
	# accepted on its screen, which is worth saying out loud rather than
	# reporting as "no devices".
	local ready unauth
	ready=$(adbq devices | awk '$2 == "device" { print $1 }')
	unauth=$(adbq devices | awk '$2 == "unauthorized" { print $1 }')

	if [ -n "$unauth" ]; then
		nope "not allowed"
		cat <<'HELP'

  The device is asking on its own screen whether to allow USB debugging
  from this computer. Tick "always allow", accept it, and run this again.

HELP
		exit 1
	fi

	if [ -z "$ready" ]; then
		nope "none found"
		cat <<'HELP'

  Check, in this order:

    1. The cable carries DATA, not just power. A charging cable will show
       nothing at all here and is the usual cause.
    2. Developer options are on: Settings > About > tap "Build number"
       seven times.
    3. USB debugging is on, in Developer options.
       Step by step, with pictures of what each prompt says:
       https://github.com/Joseph1010805/HeavenClient-Android#how-to-turn-on-usb-debugging
    4. The device is unlocked, with its screen on.

HELP
		exit 1
	fi

	local count
	count=$(printf '%s\n' "$ready" | wc -l)

	if [ "$count" -eq 1 ]; then
		DEV="$ready"
		ok "found"
		return 0
	fi

	printf '\n'
	say "More than one device is connected."

	local n=1
	printf '%s\n' "$ready" | while read -r one; do
		printf '  %d) %-24s %s\n' "$n" "$one" \
			"$(adb -s "$one" shell getprop ro.product.model </dev/null 2>/dev/null | tr -d '\r')"
		n=$((n + 1))
	done

	ask "Which one? (number)"
	DEV=$(printf '%s\n' "$ready" | sed -n "${REPLY}p")

	if [ -z "$DEV" ]; then
		bad "Not one of the choices."
		exit 1
	fi
}

if [ -n "$DEV" ]; then
	GIVEN_DEVICE=1
else
	GIVEN_DEVICE=0
	checking "Looking for a device"
	pick_device
fi

MODEL=$(dev shell getprop ro.product.model 2>/dev/null | tr -d '\r')
ANDROID=$(dev shell getprop ro.build.version.release 2>/dev/null | tr -d '\r')
ABI=$(dev shell getprop ro.product.cpu.abi 2>/dev/null | tr -d '\r')

# ⚠ THE CONFIRMATION IS NOT A FORMALITY.
#
# What follows installs an app, writes several gigabytes, and reconfigures
# Android's process killer. Naming the device out loud is the cheapest way to
# stop all of that happening to the wrong one - and with two handhelds on the
# desk, both plugged in at some point, that is not a theoretical worry.
#
# Skipped when --device named one: saying which device is the whole point of
# that flag, and asking again is just noise in a scripted run.
if [ "$GIVEN_DEVICE" -eq 0 ]; then
	say "Device found: $MODEL  (Android $ANDROID, $ABI)"
	ask "  Install to $MODEL? [Y/n]"

	case "$REPLY" in
	[Nn]*)
		bad "Stopped. Nothing on the device was changed."
		exit 1
		;;
	esac
else
	say "Installing to: $MODEL  (Android $ANDROID, $ABI)"
fi

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
say "Downloading the game"
	step "no build here - fetching the latest release"

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

checking "Installing the game"

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
	INSTALL_SAID=$(dev install -r "$APK" 2>&1)

	case "$INSTALL_SAID" in
	*Success*) break ;;
	esac

	if [ "$try" -eq 1 ]; then
		printf 'refused, retrying... '
		sleep 4
	fi
done

case "$INSTALL_SAID" in
*Success*) ok "done" ;;
*)
	nope "failed"
	bad "This is what the device said:"
	printf '%s\n' "$INSTALL_SAID" | sed 's/^/      /'
	echo
	step "INSTALL_FAILED_UPDATE_INCOMPATIBLE means an older copy signed with a"
	step "different key is in the way. Removing it clears the way - but it also"
	step "DELETES the game data already on the device, which is a ten-minute"
	step "copy to put back:"
	step "  adb -s $DEV uninstall org.heavenclient.android"
	exit 1
	;;
esac


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

# ---------------------------------------------------------------------------
# CONVERTING .wz TO .nx, SO NOBODY DOES IT FIFTEEN TIMES BY HAND
# ---------------------------------------------------------------------------
#
# ⚠ THE CONVERTER CANNOT BE SHIPPED WITH THIS, and that is not an oversight.
# NoLifeWzToNx declares NO LICENCE - neither does the project it forked from -
# which means all rights reserved and no right to redistribute a build of it.
# So the one step we cannot take off the player is building it once.
#
# Everything AFTER that we can: which fifteen files, the -c flag that decides
# whether the output is even readable, running it once per file, and knowing
# where the results go. That was the whole of the old instructions and all of
# it was on the player.
#
# The converter writes its .nx BESIDE THE INPUT - measured, not assumed:
#     .../cosmic-wz/Etc.wz -> .../cosmic-wz/Etc.nx
# so a converted client needs nothing moved afterwards.
find_converter() {
	local guess

	for guess in \
		"$GIVEN_CONV" \
		"$HERE/NoLifeWzToNx.exe" \
		"${HOME:-}/maple/NoLifeWzToNx/x64/Release/NoLifeWzToNx.exe" \
		"${HOME:-}/maple/NoLifeWzToNx/Release/NoLifeWzToNx.exe" \
		"${HOME:-}/Downloads/NoLifeWzToNx.exe" \
		"$1/NoLifeWzToNx.exe"
	do
		[ -n "$guess" ] && [ -f "$guess" ] && CONVERTER="$guess" && return 0
	done

	command -v NoLifeWzToNx.exe >/dev/null 2>&1 \
		&& CONVERTER="$(command -v NoLifeWzToNx.exe)" && return 0

	return 1
}

# Converts in place and says which file it is on, because this takes a while
# and silence for twenty minutes is indistinguishable from a hang.
convert_wz() {
	local dir="$1" f done_n=0 failed=0 total=0

	for f in $NEEDED; do total=$((total + 1)); done

	for f in $NEEDED; do
		if [ -f "$dir/$f.nx" ]; then
			step "$f.nx already converted"
			done_n=$((done_n + 1))
			continue
		fi

		if [ ! -f "$dir/$f.wz" ]; then
			step "$f.wz is NOT IN THIS CLIENT - cannot convert what is not there"
			failed=$((failed + 1))
			continue
		fi

		checking "  $f.wz"

		# ⚠ -c OR THE OUTPUT IS UNREADABLE. Without it the converter writes
		# the SERVER format, the client starts to a black screen, and nothing
		# anywhere says why. It is the single most common way this goes wrong
		# and it is one character.
		if "$CONVERTER" -c "$(win_path "$dir")/$f.wz" >/dev/null 2>&1 \
			&& [ -f "$dir/$f.nx" ]; then
			ok "done"
			done_n=$((done_n + 1))
		else
			nope "FAILED"
			failed=$((failed + 1))
		fi
	done

	step ""
	step "$done_n of $total converted, $failed missing or failed"

	[ "$failed" -eq 0 ]
}

DATA=""

checking "Looking for the game data"

# A path given on the command line is taken at its word, and complained about
# clearly if it is wrong. Falling back to a search would hide a typo behind
# ten minutes of copying something else.
if [ -n "$GIVEN_DATA" ]; then
	if has_all_nx "$GIVEN_DATA"; then
		DATA="$GIVEN_DATA"
		ok "$DATA"
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
		ok "$DATA"
		break
	fi

	if has_wz "$guess"; then
		nope "needs converting"
		say "Found a MapleStory client that has not been converted"
		step "$guess"
		step "Its .wz files have to become .nx before the game can read them."

		if find_converter "$guess"; then
			step ""
			step "Converter: $CONVERTER"
			step "This takes a while - twenty minutes or so for a full client -"
			step "and writes the .nx files beside the .wz ones. Nothing is moved"
			step "and nothing is deleted."
			step ""
			ask "  Convert them now? [Y/n]"

			case "$REPLY" in
			[Nn]*) ;;
			*)
				say "Converting"

				if convert_wz "$guess"; then
					DATA="$guess"
					break
				fi

				bad "Not every file converted, so the game would start to a"
				bad "black screen. What is missing is listed above."
				exit 1
				;;
			esac
		else
			# ⚠ THE ONE STEP THAT CANNOT BE AUTOMATED, AND WHY.
			#
			# NoLifeWzToNx declares no licence, and neither does the project it
			# forked from. No licence means all rights reserved: we may not ship
			# a build of it with this installer, however much easier that would
			# be. Build it once and this script does the rest for ever after.
			cat <<HELP

  No converter found, and this installer is not allowed to ship one:
  NoLifeWzToNx declares no licence, so a build of it cannot be
  redistributed. You have to build it once, yourself:

      https://github.com/ryantpayton/NoLifeWzToNx

  The README has the three fixes it needs to compile on a modern Visual
  Studio - it will not build as-is:

      https://github.com/Joseph1010805/HeavenClient-Android#2-convert-your-game-files-from-wz-to-nx

  Then put NoLifeWzToNx.exe beside this installer, or point at it:

      INSTALL.bat --converter C:\path\to\NoLifeWzToNx.exe

  After that this script converts all fifteen files for you, with the
  -c flag, in the right order, and carries straight on to the install.

HELP
			exit 1
		fi
	fi
done

if [ -z "$DATA" ]; then
	nope "not found"
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

# ⚠ THE SECOND CLIENT, AND WHY IT CANNOT BE GUESSED.
#
# UI.nx has to come from a LATER client - v178 - because the v83 interface is
# too old for this client to start on. That means the player installed TWO
# MapleStory clients, and there is no default path for the second: both name
# their folder MapleStory, so the second one either went somewhere custom or
# overwrote the first.
#
# So it is searched for, and then ASKED FOR. One question is a far better
# experience than a page of instructions about arranging folders by hand.
#
# Takes .wz as readily as .nx, because a freshly installed client has .wz and
# making somebody convert one file by hand to answer one question would be a
# poor joke.
ui_from_dir() {
	local dir="$1"

	[ -n "$dir" ] && [ -d "$dir" ] || return 1

	if [ -f "$dir/UI.nx" ]; then
		UI_FROM="$dir"
		return 0
	fi

	[ -f "$dir/UI.wz" ] || return 1

	# Only worth offering if there is something to convert with.
	find_converter "$dir" || return 1

	step "converting UI.wz from $dir"

	"$CONVERTER" -c "$(win_path "$dir")/UI.wz" >/dev/null 2>&1

	[ -f "$dir/UI.nx" ] || return 1

	UI_FROM="$dir"
	return 0
}

checking "Looking for UI.nx"

# ⚠ $DATA IS LAST, AND IT IS THE DANGEROUS ONE.
#
# A v83 client has a UI.nx of its own once converted, and it is exactly the
# file that does not work. Taking it silently is how somebody ends up with a
# client that will not start and fifteen files that all look correct.
for guess in 	"$GIVEN_UI" 	"${HOME:-}/maple/wz-v178" 	"$(dirname "$DATA")/wz-v178" 	"$DATA"
do
	ui_from_dir "$guess" && break
done

if [ -n "$UI_FROM" ]; then
	ok "$UI_FROM"

	if [ "$UI_FROM" = "$DATA" ]; then
		step "⚠ that is the v83 folder - if the game opens to a black screen,"
		step "  that UI.nx came from the wrong client. Use --ui to point at v178."
	fi
else
	nope "not found"

	say "Where is your v178 client?"
	step "One file comes from it: UI.nx. The v83 interface is too old for this"
	step "client to start on, which is why a second client is needed at all."
	step ""
	step "Paste the folder you installed it to, or press ENTER to stop."
	step "Somewhere like C:\Nexon\MapleStoryV178"
	step ""

	while [ -z "$UI_FROM" ]; do
		ask "  v178 folder:"

		[ -n "$REPLY" ] || break

		# Typed by a person, so it arrives in whatever form Windows gave
		# them - and a dragged folder brings its quotes along.
		REPLY="${REPLY%\"}"
		REPLY="${REPLY#\"}"

		if ui_from_dir "$REPLY"; then
			ok "using $UI_FROM"
			break
		fi

		bad "  No UI.nx or UI.wz in there. Look for the folder with the"
		bad "  game's .exe in it."
	done
fi

if [ -z "$UI_FROM" ]; then
	bad "Cannot continue without UI.nx."
	step "The app installs fine without it, but the client will not start -"
	step "so this stops here rather than leaving you with a game that opens"
	step "to nothing and says why nowhere."
	step ""
	step "Install any v178 client and run this again, or point straight at it:"
	step "  INSTALL.bat --ui C:\path\to\v178client"
	exit 1
fi

say "Copying the game data"
step "Several gigabytes over USB if this device is new - ten minutes or so."
step "Anything already there is skipped, so re-running this is cheap."

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
if ! INSTALL_QUIET=1 \
	MAPLE_DATA="$(win_path "$DATA")" \
	MAPLE_UI="$(win_path "$UI_FROM")" \
	MAPLE_CUSTOM="$(win_path "$CUSTOM_FROM")" \
	bash "$HERE/tools/deploy_data.sh" "$DEV"; then
	bad "Copying the data failed. Nothing above it was wasted - run this again"
	bad "and it will skip whatever already arrived."
	exit 1
fi

# ---------------------------------------------------------------------------
# Whose Cosmic, and where
# ---------------------------------------------------------------------------
#
# The server half used to read one hardcoded path with the author's username
# in it. That is fine on one machine and useless on anybody else's - the first
# file it wanted did not exist and it stopped there.
#
# Same principle as the game data: FIND what the person already has, never
# assume. A server is Cosmic BUILT (target/Cosmic.jar, from `mvnw package`)
# plus its own wz/ directory, and neither is ours to hand out - the jar is
# theirs to build and the wz files are Nexon's, exactly like the .nx files
# this script already refuses to fetch.
COSMIC=""

find_cosmic() {
	local tried=""

	# What they told us, first and without argument.
	if [ -n "$GIVEN_COSMIC" ]; then
		if [ -f "$GIVEN_COSMIC/target/Cosmic.jar" ]; then
			COSMIC="$GIVEN_COSMIC"
			step "Cosmic: $COSMIC"
			return 0
		fi

		bad "No Cosmic.jar under $GIVEN_COSMIC/target."
		step "Build it there first:  ./mvnw -DskipTests package"
		return 1
	fi

	# The usual places, relative to this checkout rather than to one machine.
	local guess
	for guess in \
		"$HERE/../Cosmic" \
		"$HERE/../../Cosmic" \
		"$HOME/Cosmic" \
		"$HOME/Documents/Programs/Cosmic"
	do
		tried="$tried
    $guess"

		if [ -f "$guess/target/Cosmic.jar" ]; then
			COSMIC="$(cd "$guess" && pwd)"
			step "Cosmic: $COSMIC"
			return 0
		fi
	done

	bad "Could not find a built Cosmic."
	cat <<HELP

  Hosting needs a Cosmic server that YOU have built. It is not in this
  repository and cannot be downloaded from here - it is a separate project,
  and its game data is Nexon's for the same reason the .nx files are.

  Get one, build it, and point this at it:

      git clone https://github.com/P0nk/Cosmic
      cd Cosmic && ./mvnw -DskipTests package
      tools/install.sh --server --cosmic /path/to/Cosmic

  Looked in:$tried

  Everything else has already been installed - the game itself is on the
  device and will play against somebody else's server. This step is only
  needed if you want THIS device to be the one hosting.
HELP

	return 1
}

# ---------------------------------------------------------------------------
# Termux, prepared from here so the player types nothing
# ---------------------------------------------------------------------------
#
# Setting a server up by hand is four commands in a terminal, and asking
# somebody who wanted to play a game to type four commands is where most of
# them stop. Two of those steps can be done from this side:
#
#   1. STORAGE. `termux-setup-storage` exists to raise a permission dialog.
#      `pm grant` gives the same two permissions without one.
#
#   2. allow-external-apps. Termux refuses RUN_COMMAND from another app unless
#      this is set, and it is what lets the GAME start and stop the server
#      later - see LocalServer.java. It lives in Termux's private home, which
#      is reachable only through `run-as`, and `run-as` only works on a
#      DEBUGGABLE build.
#
# ⚠ THE SECOND ONE WILL FAIL FOR MOST PEOPLE, AND THAT IS CORRECT.
# An ordinary F-Droid Termux is not debuggable; refusing to be driven by other
# apps is the whole point of the setting. So this TRIES, says plainly whether
# it worked, and prints the one line to paste when it did not. Never silently.
prepare_termux() {
	say "Preparing Termux"

	if ! dev shell "pm list packages" 2>/dev/null | tr -d '\r' \
		| grep -q "^package:com.termux$"; then
		step "Termux is not installed, so this device cannot host."
		step "Install it from F-DROID (not the Play Store; that build is"
		step "years out of date), then run this again with --server."
		return 0
	fi

	# 1. Storage, in place of termux-setup-storage.
	dev shell \
		"pm grant com.termux android.permission.READ_EXTERNAL_STORAGE" \
		>/dev/null 2>&1
	dev shell \
		"pm grant com.termux android.permission.WRITE_EXTERNAL_STORAGE" \
		>/dev/null 2>&1

	checking "Storage permission"

	if dev shell "dumpsys package com.termux" 2>/dev/null \
		| tr -d '\r' | grep -q "READ_EXTERNAL_STORAGE: granted=true"; then
		ok "granted"
	else
		nope "could not grant"
		step "Run termux-setup-storage in Termux yourself."
	fi

	# 2. allow-external-apps, so the game can start the server on its own.
	#
	# Read first, and only write when it is actually missing: the file is the
	# player's, it may hold their own settings, and clobbering it to set one
	# key would be a poor trade.
	local props="files/home/.termux/termux.properties"

	checking "Letting the game start it"

	if dev shell "run-as com.termux cat $props" 2>/dev/null \
		| tr -d '\r' | grep -qE "^[[:space:]]*allow-external-apps[[:space:]]*=[[:space:]]*true"; then
		ok "already set"
		return 0
	fi

	if ! dev shell "run-as com.termux id" >/dev/null 2>&1; then
		nope "not from here"
		step ""
		step "  This Termux is not a debuggable build, which is normal and is"
		step "  deliberately what stops other apps driving it. Open Termux once"
		step "  and paste this single line:"
		step ""
		step "    mkdir -p ~/.termux && echo 'allow-external-apps = true' >> ~/.termux/termux.properties"
		step ""
		step "  Without it the game cannot start or stop the server for you;"
		step "  everything else still works and you can run it by hand."
		return 0
	fi

	# Appended, never overwritten, and the directory made first - on a Termux
	# that has never been opened, ~/.termux does not exist yet.
	dev shell \
		"run-as com.termux sh -c 'mkdir -p files/home/.termux && printf \"allow-external-apps = true\n\" >> files/home/.termux/termux.properties'" \
		>/dev/null 2>&1

	if dev shell "run-as com.termux cat $props" 2>/dev/null \
		| tr -d '\r' | grep -qE "^[[:space:]]*allow-external-apps[[:space:]]*=[[:space:]]*true"; then
		ok "set"
	else
		nope "write failed"
		step "Set it by hand in Termux:"
		step "    mkdir -p ~/.termux && echo 'allow-external-apps = true' >> ~/.termux/termux.properties"
	fi
}

# ---------------------------------------------------------------------------
# Running the setup itself, so nobody opens a terminal
# ---------------------------------------------------------------------------
#
# ⚠ NOT THROUGH RUN_COMMAND. The obvious route is the intent the game already
# uses (see LocalServer.java), and it does not work from here: Termux protects
# `com.termux.RUN_COMMAND` at dangerous level and adb's shell user does not
# hold it. Trying it gets a flat "Requires permission" and nothing runs.
#
# `run-as` is the way in. It executes as Termux's OWN uid, so Termux's bash,
# pkg, apt and java are all simply there - no intent, no permission, no
# dialog. What is NOT there is Termux's environment: a login shell sets PREFIX
# and friends, and run-as does not, so `pkg` would be on no PATH and every
# library lookup would fail. Exported below, which is the whole trick.
#
# Same debuggable-build limit as prepare_termux, and the same honest failure.
TERMUX_HOME=/data/data/com.termux/files/home
TERMUX_PREFIX=/data/data/com.termux/files/usr

run_termux_setup() {
	say "Setting the server up on the device"

	if ! dev shell "run-as com.termux id" >/dev/null 2>&1; then
		step "Cannot drive Termux from here - this build is not debuggable."
		step "Open Termux on the device and run these two lines:"
		step ""
		step "    cp /sdcard/Download/cosmic/termux_setup.sh ~"
		step "    bash ~/termux_setup.sh"
		return 0
	fi

	step "Java, MariaDB and the server itself. Twenty minutes or so, most of"
	step "it downloading. Nothing to do but leave it alone."
	printf '\n'

	# KEPT, NOT SHOWN - AND SHOWN THE INSTANT IT MATTERS.
	#
	# Two hundred lines of apt output is not something to read while it works,
	# and it is the only thing worth reading when it breaks. So all of it goes
	# to a file, the six milestones termux_setup.sh prints go to the screen,
	# and the failure branch below prints the lot. Throwing it away to keep
	# the output tidy would be the one habit this project has sworn off.
	SETUP_LOG=$(mktemp 2>/dev/null || echo "${TMPDIR:-/tmp}/localstory-setup.$$")

	# The script is copied into Termux's home first, exactly as the by-hand
	# instructions do - it expects to be run from there, and a copy on the SD
	# card is on a filesystem with no execute bit.
	#
	# `set -o pipefail` matters: without it the exit status is `tee`'s, which
	# is always 0, and a failed setup would report success.
	dev shell "run-as com.termux files/usr/bin/bash -c '
		set -o pipefail
		export PREFIX=$TERMUX_PREFIX
		export HOME=$TERMUX_HOME
		export PATH=\$PREFIX/bin:\$PATH
		export LD_LIBRARY_PATH=\$PREFIX/lib
		export TMPDIR=\$PREFIX/tmp
		cp /sdcard/Download/cosmic/termux_setup.sh \$HOME/ || exit 1
		bash \$HOME/termux_setup.sh 2>&1 | tee \$HOME/setup.log
	'" 2>&1 | tr -d '\r' | tee "$SETUP_LOG" | awk '/^=== /{sub(/^=== /,"    "); print; fflush()}'

	# Asked of the device rather than trusted from the pipe above - adb shell
	# does not forward the remote exit status, so the only honest way to know
	# is to look for what a finished setup leaves behind.
	if dev shell "run-as com.termux ls files/home/cosmic/run.sh" \
		>/dev/null 2>&1; then
		step "Server installed - the login screen can start it."
		rm -f "$SETUP_LOG"
	else
		bad "The server setup did not finish. This is what it printed:"
		sed 's/^/      /' "$SETUP_LOG"
		step "The same log is on the device at ~/setup.log."
		step ""
		step "The game itself IS installed and the data is there - only the"
		step "server failed, so this device cannot host. It can still join one."
		exit 1
	fi
}

# ---------------------------------------------------------------------------
# The server, if they want to play with no network at all
# ---------------------------------------------------------------------------
if [ "$WANT_SERVER" -eq 1 ]; then
	# A MISSING COSMIC IS NOT A FAILED INSTALL.
	#
	# INSTALL.bat asks for --server on every run now, because any device can be
	# the one that CREATES the game - the login screen has no HOST / JOIN pair.
	# So "there is no built server on this PC" has to mean "this device will
	# join somebody else's", not an exit code handed over after the app and four
	# gigabytes have already landed. find_cosmic says what is missing either way.
	if find_cosmic; then
		prepare_termux
		say "Putting the server on the device"
		INSTALL_QUIET=1 bash "$HERE/tools/stage_server.sh" "$DEV" "$COSMIC" || exit 1
		run_termux_setup
	else
		# So the closing text offers the server instead of congratulating them
		# on one they have not got.
		WANT_SERVER=0
	fi
fi

say "Done."

# ⚠ WHAT THEY NEED NEXT, AND NOTHING ELSE.
#
# This used to end in four paragraphs and then contradict itself: it told
# somebody who had just sat through the twenty-minute server install to go
# and install a server. Advice that argues with what just happened reads as a
# failure, and the obvious response is to run the whole thing again.
cat <<DONE
  $MODEL now has LocalStory. Open it from the app list.

  Somebody has to be hosting for there to be a game to join, and the login
  screen finds one by itself - there is no address to type.

    * A game already running on your wifi appears under GAMES NEARBY. Tap
      it and type the six digits the host reads out.

    * No game yet? Tap CREATE A GAME, choose six digits, and read them out.
      The device you create on has to stay switched on.

DONE

if [ "$WANT_SERVER" -eq 1 ]; then
	cat <<'HOSTING'
  This device can host by itself: tap SERVER at the top left of the login
  screen and it starts its own server.

HOSTING
else
	cat <<HOSTING
  To let THIS device host as well, so the others join it:

      INSTALL.bat --server

HOSTING
fi
