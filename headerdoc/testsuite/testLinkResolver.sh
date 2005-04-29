#!/bin/sh

# Copy the verified output to temp1 for testing.
# We can't test on the output of the current HeaderDoc,
# as that would make isolating bugs muhc harder.
# 
rm -rf temp1 temp2
cp -R verified_output temp1
cp -R verified_output temp2

# Run gatherHeaderDoc.
#
../gatherHeaderDoc.pl temp2

# Translate the HTML to text and diff it.
#
rm -f textchanges
./difftxt.sh temp1 temp2

# Diff the raw HTML.
# 
rm -f htmlchanges
./diffhtml.sh temp1 temp2 > /dev/null

# Move the results out of the way of other tests.
#
mv textchanges textchanges-lr
mv htmlchanges htmlchanges-lr

# Of the HTML changes (there will be some), compare them
# to the -expected- changes made by resolveLinks, and
# report any differences in the diffs.
#
diff -u htmlchanges-lr lr_expected_htmlchanges > htmlchanges-lr-diffs
cat htmlchanges-lr-diffs

TEXTDIFF="$(cat textchanges-lr)";
HTMLDIFF="$(cat htmlchanges-lr-diffs)";

if [ "X$TEXTDIFF" == "X" ] ; then
	if [ "X$HTMLDIFF" == "X" ] ; then
		exit 0
	fi
fi
exit 1
 
