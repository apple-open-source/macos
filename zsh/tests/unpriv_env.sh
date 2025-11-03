#!/bin/sh

scriptdir=$(dirname $(realpath "$0"))

: ${ZSH=/bin/zsh}
: ${SOURCEFILE=/tmp/sourced}
: ${SETEUSER="$scriptdir/seteuser"}

set -e

uid=$(id -u)
if [ "$uid" -ne 0 ]; then
	1>&2 echo "This test requires root to run."
	exit 1
fi

# Set a ZDOTDIR so that we don't do something unfortunate for other things that
# may run concurrently.
export ZDOTDIR="$PWD"

_check_sourced() {
	local file rc_wanted=$1 type="$2"
	local verbiage="not"
	shift; shift

	if [ "$rc_wanted" -ne 0 ]; then
		verbiage="is"
	fi

	while [ $# -gt 0 ]; do
		local fail

		file="$1"
		shift

		set +e
		grep -q "$file" "$SOURCEFILE"
		rc=$?
		set -e

		# Normalize 0 -> 0, !0 -> 1
		if [ "$rc" -ne 0 ]; then
			fail=1
		else
			fail=0
		fi

		if [ "$fail" -ne "$rc_wanted" ]; then
			1>&2 echo "$file $verbiage sourced in $type execution"
			1>&2 echo "=="
			1>&2 cat "$SOURCEFILE"
			exit 1
		fi
	done
}

check_sourced() {
	_check_sourced 0 "$@"
}

check_not_sourced() {
	_check_sourced 1 "$@"
}


for x in .zshenv .zprofile .zshrc .zlogin; do
	echo "echo \"$x sourced\" >> \"$SOURCEFILE\"" > "$x"
	chmod 0644 "$x"
done

rm -f "$SOURCEFILE"

# Baseline #1: Non-login shell sourcing
if ! "$ZSH" -c "true"; then
	1>&2 echo "non-login command failed; results inconclusive"
	exit 1
fi

if [ ! -s "$SOURCEFILE" ]; then
	1>&2 echo "sourced either was not created or was not populated."
	exit 1
fi

check_sourced "non-login" .zshenv
check_not_sourced "non-login" .zprofile .zshrc .zlogin

rm "$SOURCEFILE"

# Baseline #2: Login shell sourcing
if ! "$ZSH" -lc 'true'; then
	1>&2 echo "login command failed; results inconclusive"
	exit 1
fi

if [ ! -s "$SOURCEFILE" ]; then
	1>&2 echo "sourced either was not created or was not populated."
	exit 1
fi

check_sourced "login" .zshenv .zprofile .zlogin
check_not_sourced "non-login" .zshrc

rm -f "$SOURCEFILE"

1>&2 echo "Test #1: Confirm that PRIVILEGED generally works"

if ! "$ZSH" -pc 'true'; then
	1>&2 echo "non-login command failed; results inconclusive"
	exit 1
elif [ -s "$SOURCEFILE" ]; then
	1>&2 echo "zsh sourced dotfiles with PRIVILEGED (-p) specified, non-login"
	1>&2 echo "=="
	1>&2 cat "$SOURCEFILE"
	exit 1
fi
if ! "$ZSH" -lpc 'true'; then
	1>&2 echo "login command failed; results inconclusive"
	exit 1
elif [ -s "$SOURCEFILE" ]; then
	1>&2 echo "zsh sourced dotfiles with PRIVILEGED (-p) specified, login"
	1>&2 echo "=="
	1>&2 cat "$SOURCEFILE"
	exit 1
fi

1>&2 echo "Test #2: Confirm that setuid-implied PRIVILEGED works"
if ! "$SETEUSER" nobody "$ZSH" -c 'true'; then
	1>&2 echo "non-login command failed; results inconclusive"
	exit 1
elif [ -s "$SOURCEFILE" ]; then
	1>&2 echo "zsh sourced dotfiles with setuid-implied PRIVILEGED, non-login"
	1>&2 echo "=="
	1>&2 cat "$SOURCEFILE"
	exit 1
fi
if ! "$SETEUSER" daemon "$ZSH" -lc 'true'; then
	1>&2 echo "login command failed; results inconclusive"
	exit 1
elif [ -s "$SOURCEFILE" ]; then
	1>&2 echo "zsh sourced dotfiles with setuid-implied PRIVILEGED, login"
	1>&2 echo "=="
	1>&2 cat "$SOURCEFILE"
	exit 1
fi

1>&2 echo "Test #3: Confirm that APPLE_PKGKIT_ESCALATING_ROOT works"
export APPLE_PKGKIT_ESCALATING_ROOT=
if ! "$ZSH" -c 'true'; then
	1>&2 echo "non-login command failed; results inconclusive"
	exit 1
elif [ -s "$SOURCEFILE" ]; then
	1>&2 echo "zsh sourced dotfiles with PRIVILEGED (-p) specified, non-login"
	1>&2 echo "=="
	1>&2 cat "$SOURCEFILE"
	exit 1
fi
if ! "$ZSH" -lc 'true'; then
	1>&2 echo "login command failed; results inconclusive"
	exit 1
elif [ -s "$SOURCEFILE" ]; then
	1>&2 echo "zsh sourced dotfiles with PRIVILEGED (-p) specified, login"
	1>&2 echo "=="
	1>&2 cat "$SOURCEFILE"
	exit 1
fi
