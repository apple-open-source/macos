#!/bin/sh

# Generate test output.
#
./mktestout.sh

# copy master TOC (and other TOCs)
cp verified_output/*.html test_output
# cp verified_output/masterTOC.html test_output

# Translate the HTML to text and diff it.
#
rm -f textchanges
./difftxt.sh verified_output test_output

# Diff the raw HTML.
# 
rm -f htmlchanges
./diffhtml.sh verified_output test_output

# Move the results out of the way of other tests.
# 
mv textchanges textchanges-hd
mv htmlchanges htmlchanges-hd

TEXTDIFF="$(cat textchanges-hd)";
HTMLDIFF="$(cat htmlchanges-hd)";

if [ "X$TEXTDIFF" == "X" ] ; then
	if [ "X$HTMLDIFF" == "X" ] ; then
		exit 0
	fi
fi
exit 1
 
