#!/bin/sh

# This script should be run when the current output has been
# certified to be entirely more correct than the previous output,
# with no regressions.  It makes the new output become the
# baseline for future testing.


# Generate the new test output
#
./mktestout.sh
rm -rf verified_output
rm -rf temp1 temp2


# Make a copy in temp1 and rename to temp2.
# 
cp -r test_output temp1
mv test_output temp2


# Gather headerdoc comments and modify the tree for temp2.
# Copy the masterTOC file into temp1.  This simulates the
# environment used by testLinkResolver.
# 
../gatherHeaderDoc.pl temp2
cp temp2/masterTOC.html temp1


# Generate the list of expected diffs that should be
# caused by gatherHeaderDoc and resolvelinks
# 
./diffhtml.sh temp1 temp2 > /dev/null
mv htmlchanges lr_expected_htmlchanges


# Make temp1 (the one on which gatherHeaderDoc/resolveLinks has NOT
# been run) the official certified source.
# 
mv temp1 verified_output


# Clean up....
#
rm -rf temp2

