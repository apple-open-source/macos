#!/bin/sh

fails=0
ps="ps -ax"
stdout="stdout-mflag.txt"
stderr="stderr-mflag.txt"

$ps -m >$stdout 2>$stderr
ret=$?

if [ $ret -eq 1 ]; then
	# Soft fail?
	if ! grep -q "requires entitlement" $stderr; then
		fails=$((fails + 1))
		1>&2 echo "ps -m test failed -- non-entitlement error"
	else
		1>&2 echo "ps -m requires entitlement, non-fatal failure."
	fi
elif [ $ret -gt 1 ]; then
	fails=$((fails + 1))
	1>&2 echo "ps -m test failed - program aborted?"
fi

# Don't leave empty output files hanging around
[ -s "$stdout" ] || rm "$stdout"
[ -s "$stderr" ] || rm "$stderr"

stdout="stdout-vflag.txt"
stderr="stderr-vflag.txt"
$ps -v >$stdout 2>$stderr
ret=$?

if [ $ret -ne 0 ]; then
	fails=$((fails + 1))
	1>&2 echo "ps -v test failed"
fi

[ -s "$stdout" ] || rm "$stdout"
[ -s "$stderr" ] || rm "$stderr"

if [ $fails -eq 0 ]; then
	echo "All tests passed."
else
	1>&2 echo "$fails tests failed"
fi

exit $fails
