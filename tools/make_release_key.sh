#!/usr/bin/env bash
#
# Make the signing key that release APKs are signed with, put it in GitHub,
# and back it up. One command, one question.
#
#     tools/make_release_key.sh
#
# ⚠ WHAT THIS KEY IS
#
# Android identifies an app by its signature, not by its name. Lose this file
# and you can never ship an update that installs over an existing copy again -
# every player has to uninstall first, and there is no recovery, no reset, and
# nobody to appeal to. Treat it like the deed to the project.
#
# It is never committed (`*.jks` is ignored) because anyone holding it can
# publish an app that Android believes is yours.
#
# The password is read silently and never appears on a command line, in shell
# history, or on screen. keytool and gh are both fed it on stdin rather than
# in their arguments, so it never shows up in a process list either.
#
set -eu

OUT="${1:-release.jks}"
ALIAS="localstory"
REPO="Joseph1010805/HeavenClient-Android"

# OneDrive, because the real risk here is LOSING it, not somebody stealing it.
# A hobby signing key sitting in a synced folder is a small exposure; the same
# key existing only on one Windows machine is a near-certainty of loss the day
# that machine dies. If that trade ever stops being right, move it.
BACKUP="$HOME/OneDrive/Documents/keys"

say() { printf '%s\n' "$*"; }
die() { printf '\n%s\n' "$*" >&2; exit 1; }

# --- the tools -------------------------------------------------------------
GH="$(command -v gh || true)"
[ -n "$GH" ] || GH="/c/Program Files/GitHub CLI/gh.exe"
[ -x "$GH" ] || GH=""

command -v keytool >/dev/null 2>&1 \
	|| die "keytool not found. It ships with the JDK - try the one Cosmic builds with."

if [ -f "$OUT" ]; then
	die "'$OUT' already exists, and overwriting it would strand anything already
installed from it. Move it somewhere safe first if you genuinely want a new one."
fi

# --- the one question ------------------------------------------------------
say "Making the signing key for $REPO."
say
say "Choose a password you can still find in five years, and write it down"
say "with the backup. There is no way to recover it, and no way to replace"
say "this key later without everyone uninstalling the app first."
say
say "Nothing you type is shown."
say

if [ -t 0 ]; then
	read -rsp "Password: " PW
	echo
	read -rsp "Again:    " PW2
	echo

	[ "$PW" = "$PW2" ] || die "Those did not match. Nothing was created - run it again."
	[ "${#PW}" -ge 6 ] || die "Android requires at least 6 characters. Nothing was created."
else
	# No terminal to type into - so make one up instead of failing.
	#
	# bash's `read -p` prints nothing at all when stdin is not a tty, so
	# running this through anything that is not a real terminal window looked
	# exactly like the script had hung with no prompt. A generated password is
	# also simply better: it is long, random, and cannot be forgotten, and it
	# is written next to the keystore rather than into anyone's memory.
	PW="$(openssl rand -base64 24 2>/dev/null | tr -d '\n/+=' | cut -c1-24)"

	[ "${#PW}" -ge 12 ] \
		|| PW="$(head -c 32 /dev/urandom | base64 | tr -d '\n/+=' | cut -c1-24)"

	[ "${#PW}" -ge 12 ] || die "Could not generate a password. Run this in a real terminal instead."

	GENERATED=yes

	say "No terminal to type into, so a 24-character random password was made."
	say "It is written to the backup folder - see the end."
fi

# --- the key ---------------------------------------------------------------
#
# The certificate identity is supplied rather than prompted for: nothing checks
# those fields and no authority vouches for them. Android cares only that
# today's signature matches the one the app was installed with.
say
say "Making a 10,000-day key..."

printf '%s\n%s\n\n' "$PW" "$PW" | keytool -genkeypair \
	-keystore "$OUT" \
	-alias "$ALIAS" \
	-keyalg RSA \
	-keysize 4096 \
	-validity 10000 \
	-dname "CN=LocalStory, OU=LocalStory, O=LocalStory, L=Unknown, ST=Unknown, C=US" \
	>/dev/null 2>&1 \
	|| die "keytool failed. Nothing was created."

[ -f "$OUT" ] || die "keytool reported success but there is no '$OUT'."

# Prove it can actually be opened with that password, rather than assuming.
printf '%s\n' "$PW" | keytool -list -keystore "$OUT" -alias "$ALIAS" >/dev/null 2>&1 \
	|| die "The key was written but will not open with that password. Delete '$OUT' and retry."

say "  made $OUT and checked it opens"

# --- the backup, before anything else can go wrong -------------------------
mkdir -p "$BACKUP"
cp "$OUT" "$BACKUP/release.jks"
say "  copied to $BACKUP/release.jks"

# The password goes with it when nobody chose it, because a generated password
# that exists only inside a GitHub secret is not recoverable - GitHub will let
# you REPLACE a secret but never show you one. Written here it can be read back
# the day the machine dies, which is the day it is needed.
#
# Key and password in the same folder is weaker than keeping them apart, and
# that is the deliberate trade: the realistic risk to a hobby signing key is
# losing it, not somebody going after it.
if [ "${GENERATED:-no}" = "yes" ]; then
	{
		printf 'LocalStory release signing key\n'
		printf '==============================\n\n'
		printf 'Keystore : release.jks (in this folder)\n'
		printf 'Alias    : %s\n' "$ALIAS"
		printf 'Password : %s\n\n' "$PW"
		printf 'This password unlocks release.jks, which is what proves an\n'
		printf 'update to the app really came from you. Android will refuse to\n'
		printf 'install an update signed by any other key.\n\n'
		printf 'If both of these are lost, the app can never be updated again -\n'
		printf 'every player would have to uninstall first. There is no reset\n'
		printf 'and nobody to appeal to. Keep this folder backed up.\n\n'
		printf 'The same values are stored as GitHub Actions secrets on\n'
		printf '%s, which is where the release build reads them\n' "$REPO"
		printf 'from. GitHub can replace a secret but will never show you one,\n'
		printf 'so this file is the only readable copy.\n'
	} > "$BACKUP/release-key-password.txt"

	say "  password written to $BACKUP/release-key-password.txt"
fi

# --- GitHub ----------------------------------------------------------------
if [ -z "$GH" ]; then
	say
	say "GitHub CLI not found, so the secrets were NOT set. Everything else is done."
	say "Install it (winget install GitHub.cli), then run this again with the"
	say "keystore moved aside, or set these four by hand at"
	say "  https://github.com/$REPO/settings/secrets/actions"
	exit 0
fi

if ! "$GH" auth status >/dev/null 2>&1; then
	say
	say "GitHub CLI is not logged in - run 'gh auth login', then set the four"
	say "secrets by hand at https://github.com/$REPO/settings/secrets/actions"
	exit 0
fi

say
say "Putting the four secrets in $REPO..."

# Piped, not passed as arguments, so none of this lands in a process list.
base64 -w0 "$OUT" 2>/dev/null | "$GH" secret set RELEASE_KEYSTORE_BASE64 -R "$REPO" \
	|| base64 -i "$OUT" | tr -d '\n' | "$GH" secret set RELEASE_KEYSTORE_BASE64 -R "$REPO"
say "  RELEASE_KEYSTORE_BASE64"

printf '%s' "$PW"    | "$GH" secret set RELEASE_KEYSTORE_PASSWORD -R "$REPO"
say "  RELEASE_KEYSTORE_PASSWORD"

printf '%s' "$ALIAS" | "$GH" secret set RELEASE_KEY_ALIAS -R "$REPO"
say "  RELEASE_KEY_ALIAS"

printf '%s' "$PW"    | "$GH" secret set RELEASE_KEY_PASSWORD -R "$REPO"
say "  RELEASE_KEY_PASSWORD"

say
say "================================================================"
say "Done. Releases will now be properly signed."
say
say "  key       $OUT        (not committed - *.jks is ignored)"
say "  backup    $BACKUP/release.jks"
say "  secrets   4 set on $REPO"
say
say "Write the password down next to the backup. Then:"
say
say "    git tag v0.7 && git push upstream-mine v0.7"
say "================================================================"
