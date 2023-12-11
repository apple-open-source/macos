#!/bin/sh

: ${PROG=ncurses5.4-config}

trap 'rm -f "$tmpf"' EXIT
tmpf=$(mktemp -ut ncurses_test)

fails=0

try_flag()
{
	local flag

	flag="$1"

	${PROG} "--$flag" > "$tmpf" 2> /dev/null
	if [ $? -ne 0 ]; then
		1>&2 echo "--$flag failed"
		fails=$((fails + 1))
		return 1
	fi

	if [ ! -s "$tmpf" ]; then
		1>&2 echo "--$flag output empty"
		fails=$((fails + 1))
		return 1
	fi

	# Leave $tmpf intact in case the caller wants to validate the result
	return 0
}

# Arbitrarily structured output, just make sure they're outputting anything and
# do succeed.
for flag in prefix exec-prefix cflags libs; do
	try_flag "$flag"
done

# These ones should look vaguely like a version number.
for flag in version abi-version mouse-version; do
	if try_flag "$flag"; then
		if grep -vq "[0-9.]" "$tmpf"; then
			fails=$((fails + 1))
			1>&2 echo "--$flag output has non-numeric digit"
			continue
		fi
	fi
done

# These ones should all yield absolute paths, and not just /.
for flag in bindir datadir includedir libdir mandir terminfo terminfo-dirs termpath; do
	if try_flag "$flag"; then
		if ! grep -Eq "^/[^/]+" "$tmpf"; then
			fails=$((fails + 1))
			1>&2 echo "--$flag output not an absolute path"
			continue
		fi
	fi
done

if try_flag "help"; then
	if ! grep -q "^Usage:" "$tmpf"; then
		fails=$((fails + 1))
		1>&2 echo "--help should show usage information"
	fi
fi

if [ "$fails" -eq 0 ]; then
	echo "All tests passed."
else
	echo "$fails tests failed"
fi

exit "$fails"
