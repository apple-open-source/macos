#!/bin/sh

fails=0
ps="ps"
outfile="ps.txt"
procfile="ps-processed.txt"

$ps -x -a -o vsize,command | grep -v 'ps -x' > "$outfile"

cat "$outfile" |
	sed -Ee 's/^[[:space:]]+//' |
	cut -f1 -d' ' |
	sed -e 's/[^1-9]//'g -e '/^$/d' > "$procfile"

if [ ! -s "$procfile" ]; then
	1>&2 echo "ps couldn't fetch vsize"
	fails=$((fails + 1))
fi

if [ $fails -eq 0 ]; then
	echo "All tests passed."
else
	1>&2 echo "$fails tests failed"
fi

exit $fails
