#!/bin/sh

fails=0
ps="ps"
outfile="ps.txt"

keywords=$($ps -L)
for kword in ${keywords}; do
	outfile="ps.$kword.txt"
	errfile="ps-err.$kword.txt"

	$ps -axo "$kword" > "$outfile" 2> "$errfile"
	rc=$?

	if [ "$rc" -ne 0 -o -s "$errfile" ]; then
		if [ "$rc" -ne 0 ]; then
			1>&2 echo "$ps -o $kword exited with code $rc"
		else
			1>&2 echo "$ps -o $kword wrote to stderr"
		fi
		fails=$((fails + 1))
	else
		rm "$errfile"
	fi 
done

if [ $fails -eq 0 ]; then
	echo "All tests passed."
else
	1>&2 echo "$fails tests failed"
fi

exit $fails
