#!/bin/sh -e

if [ "$SCRIPT_INPUT_FILE_COUNT" -ne "$SCRIPT_OUTPUT_FILE_COUNT" ]; then
	1>&2 echo input and output file counts differ
	exit 1
fi

X=0

while [ "$X" -lt "$SCRIPT_INPUT_FILE_COUNT" ]; do
	eval infile=\"\$SCRIPT_INPUT_FILE_$X\"
	eval outfile=\"\$SCRIPT_OUTPUT_FILE_$X\"

	outdir="$(dirname "$outfile")"
	[ -d "$outdir" ] || install -d -o root -g wheel -m 0755 "$outdir"

	install -c -o root -g wheel -m 4755 "$infile" "$outfile"
	X=$((X+1))
done
