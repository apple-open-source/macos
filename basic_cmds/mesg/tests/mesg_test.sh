#!/bin/sh

fails=0

mesg n
rc=$?
if [ $rc -ne 1 ]; then
	fails=$((fails + 1))
	1>&2 echo "bad exit status for \`mesg n\`, expected 1 for mesg disabled"
fi

status=$(mesg)
rc=$?
if [ "$status" != "is n" ]; then
	fails=$((fails + 1))
	1>&2 echo "mesg status apparently unchanged"
elif [ $rc -ne 1 ]; then
	fails=$((fails + 1))
	1>&2 echo "bad exit status for \`mesg\`, expected 1 for mesg disabled"
fi

mesg y
rc=$?
if [ $rc -ne 0 ]; then
	fails=$((fails + 1))
	1>&2 echo "bad exit status for \`mesg y\`, expected 0 for mesg enabled"
fi

status=$(mesg)
rc=$?
if [ "$status" != "is y" ]; then
	fails=$((fails + 1))
	1>&2 echo "mesg status apparently unchanged"
elif [ $rc -ne 0 ]; then
	fails=$((fails + 1))
	1>&2 echo "bad exit status for \`mesg\`, expected 0 for mesg enabled"
fi

if [ $fails -eq 0 ]; then
	1>&2 echo "All tests passed."
fi
exit $fails
