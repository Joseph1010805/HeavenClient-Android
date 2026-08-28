#!/usr/bin/env bash
#
# Tell a handheld's Cosmic what its OWN address is.
#
#   tools/set_host_ip.sh <device-serial> [ip]
#
# WHY THIS EXISTS
#
# When a client picks a character, Cosmic answers with the address of the
# channel server to reconnect to - and it reads that address out of
# config.yaml, from LANHOST. It is a FIXED STRING. `Server.getInetSocket`
# chooses BETWEEN LOCALHOST and LANHOST depending on where the client is, but
# nothing ever works out what address this machine actually has.
#
# So a config copied from the PC tells every device on the network to reconnect
# to the PC. The join succeeds, the character list arrives, a character is
# selected - and then the client reconnects to a machine that is not running a
# server and sits there sending SELECT_CHAR into nothing. The Start button
# looks broken. Nothing says "wrong address"; it simply stops.
#
# HOW IT WORKS
#
# The file is pulled off, edited here, and pushed back. Editing it in place
# with sed over `adb shell run-as ...` means a regex passing through three
# levels of quoting, and the first two attempts at that produced a mangled
# script rather than a mangled config only because it was caught. Local text
# editing has no quoting to get wrong.
#
set -u
export MSYS_NO_PATHCONV=1

if ! command -v adb >/dev/null 2>&1; then
	for guess in "${LOCALAPPDATA:-}/Android/Sdk/platform-tools" \
	             "${HOME:-}/AppData/Local/Android/Sdk/platform-tools"; do
		[ -x "$guess/adb.exe" ] && { PATH="$PATH:$guess"; export PATH; break; }
	done
fi

DEV="${1:-}"
WANT="${2:-}"
THOME=/data/data/com.termux/files/home
STAGE=/sdcard/Download/cosmic-config.yaml

if [ -z "$DEV" ]; then
	echo "usage: $0 <device-serial> [ip]"
	echo
	adb devices | tail -n +2
	exit 1
fi

run_as() { adb -s "$DEV" shell "run-as com.termux sh -c '$1'" 2>/dev/null | tr -d '\r'; }

# The device's own LAN address, asked of the device rather than assumed.
if [ -z "$WANT" ]; then
	WANT=$(adb -s "$DEV" shell "ip route get 1.1.1.1 2>/dev/null" 2>/dev/null \
		| tr -d '\r' | sed -n 's/.* src \([0-9.]*\).*/\1/p' | head -1)
fi

if [ -z "$WANT" ]; then
	echo "Could not work out $DEV's LAN address."
	echo "Is it on wifi? Pass one explicitly: $0 $DEV 192.168.1.184"
	exit 1
fi

echo "[$DEV] its own address is $WANT"

# Windows form. MSYS_NO_PATHCONV is on above, so a /tmp/... path reaches adb
# verbatim and it cannot write there - the pull fails and the file looks
# unreadable. Third time this exact trap has come up in this repo.
TMP="${LOCALAPPDATA:-C:/Windows/Temp}/Temp/cosmic-config-$DEV.yaml"

run_as "cp $THOME/cosmic/config.yaml $STAGE && chmod a+rw $STAGE" >/dev/null
adb -s "$DEV" pull "$STAGE" "$TMP" >/dev/null 2>&1

if [ ! -s "$TMP" ]; then
	echo "  could not read the config off the device."
	exit 1
fi

echo "  was: $(grep -E '^ *(HOST|LANHOST):' "$TMP" | tr -s ' ' | tr '\n' ' ')"

python - "$TMP" "$WANT" <<'PY'
import re, sys

path, want = sys.argv[1], sys.argv[2]

with open(path, encoding="utf-8", errors="replace") as f:
    text = f.read()

# Both keys. A client on the same handheld arrives on loopback and gets
# LOCALHOST; anybody else arrives on the LAN and gets LANHOST. LOCALHOST is
# left alone - 127.0.0.1 is correct for that case and always will be.
text = re.sub(r"(?m)^( *HOST:)[^#\n]*", r"\1 %s               " % want, text)
text = re.sub(r"(?m)^( *LANHOST:)[^#\n]*", r"\1 %s            " % want, text)

with open(path, "w", encoding="utf-8", newline="") as f:
    f.write(text)
PY

echo "  now: $(grep -E '^ *(HOST|LANHOST):' "$TMP" | tr -s ' ' | tr '\n' ' ')"

adb -s "$DEV" push "$TMP" "$STAGE" >/dev/null 2>&1
run_as "cp $STAGE $THOME/cosmic/config.yaml" >/dev/null

# No spaces or brackets in the pattern: this string passes through adb shell,
# then sh -c, then grep, and anything clever in it is eaten on the way.
CHECK=$(run_as "grep LANHOST $THOME/cosmic/config.yaml")

if ! echo "$CHECK" | grep -q "$WANT"; then
	echo
	echo "  ⚠ it did not stick on the device. Config now reads: $CHECK"
	exit 1
fi

echo
echo "Set. The address is read at STARTUP, so restart the server:"
echo
echo "    adb -s $DEV shell \"run-as com.termux sh -c 'pkill -f \\\"[C]osmic.jar\\\"'\""
echo
echo "then press HOST in the game again."
