#!/usr/bin/env bash
#
# Make the signing key that release APKs are signed with, and print exactly
# what to paste into GitHub.
#
#     tools/make_release_key.sh
#
# ⚠ BACK THE KEYSTORE UP SOMEWHERE THAT OUTLIVES THIS MACHINE.
#
# Android identifies an app by its signature, not by its name. Lose this file
# and you can never ship an update that installs over an existing copy again -
# every player would have to uninstall and lose nothing but it looks exactly
# like the app is broken, and there is no recovery, no reset, and nobody to
# appeal to. Treat it like the deed to the project.
#
# It is NOT committed - the .gitignore below makes sure - because anyone
# holding it can publish an app that Android believes is yours.
#
set -eu

OUT="${1:-release.jks}"
ALIAS="localstory"

if [ -f "$OUT" ]; then
	echo "'$OUT' already exists. Refusing to overwrite it."
	echo
	echo "If you genuinely want a new key, move the old one somewhere safe"
	echo "first - anything already installed from it can never be updated by"
	echo "a different key."
	exit 1
fi

command -v keytool >/dev/null 2>&1 || {
	echo "keytool not found. It ships with the JDK - try the one Cosmic builds with."
	exit 1
}

echo "Making a 10,000-day signing key in '$OUT'."
echo
echo "You will be asked for a password. Choose one you can find again in five"
echo "years; write it down with the backup of the keystore itself. There is no"
echo "way to recover either."
echo

keytool -genkeypair \
	-keystore "$OUT" \
	-alias "$ALIAS" \
	-keyalg RSA \
	-keysize 4096 \
	-validity 10000

echo
echo "================================================================"
echo "Done. Now put it in GitHub:"
echo
echo "  Settings -> Secrets and variables -> Actions -> New secret"
echo
echo "  RELEASE_KEYSTORE_BASE64   the block printed below"
echo "  RELEASE_KEYSTORE_PASSWORD the password you just chose"
echo "  RELEASE_KEY_ALIAS         $ALIAS"
echo "  RELEASE_KEY_PASSWORD      the key password (same one, unless you"
echo "                            deliberately set a different one)"
echo
echo "Until all four exist, tagged builds still work - they just come out"
echo "DEBUG-SIGNED, and the release notes say so."
echo "================================================================"
echo
echo "----- RELEASE_KEYSTORE_BASE64 (copy everything between the lines) -----"
base64 -w0 "$OUT" 2>/dev/null || base64 -i "$OUT"
echo
echo "----------------------------------------------------------------------"
echo
echo "⚠ Now back up '$OUT' somewhere off this machine."
