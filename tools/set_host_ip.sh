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
# config.yaml, from LANHOST. It is a fixed string. `Server.getInetSocket`
# chooses BETWEEN LOCALHOST and LANHOST depending on where the client is, but
# it never works out what this machine's address actually is.
#
# So a config copied from the PC tells every device on the network to
# reconnect to the PC. The join succeeds, the character list arrives, the
# character is selected - and then the client reconnects to a machine that is
# not running a server and sits there sending SELECT_CHAR into nothing. The
# Start button looks broken. Nothing in the log says "wrong address"; it just
# stops.
#
# That is exactly what happened after config.yaml was pushed to the Thor
# unchanged, which is why deploying the server now has to set this.
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

BEFORE=$(run_as "grep -E '^\\s*(HOST|LANHOST):' $THOME/cosmic/config.yaml")
echo "  was:"
echo "$BEFORE" | sed 's/^/    /'

# sed in place on the device. Both keys, because a client on the same handheld
# comes in on loopback and a client elsewhere comes in on the LAN, and only one
# of those is LANHOST.
run_as "sed -i -E 's/^([[:space:]]*HOST:)[[:space:]]*[0-9.]+/\\1 $WANT/; s/^([[:space:]]*LANHOST:)[[:space:]]*[0-9.]+/\\1 $WANT/' $THOME/cosmic/config.yaml"

AFTER=$(run_as "grep -E '^\\s*(HOST|LANHOST):' $THOME/cosmic/config.yaml")
echo "  now:"
echo "$AFTER" | sed 's/^/    /'

if ! echo "$AFTER" | grep -q "$WANT"; then
	echo
	echo "  ⚠ the edit did not stick - check the file by hand."
	exit 1
fi

echo
echo "Restart the server for this to take effect - the address is read at"
echo "startup. Press HOST in the game again, or:"
echo
echo "    adb -s $DEV shell \"run-as com.termux sh -c 'pkill -f \\\"[C]osmic.jar\\\"'\""
