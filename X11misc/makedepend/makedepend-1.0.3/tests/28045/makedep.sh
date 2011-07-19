#!/bin/sh

# Test case from https://bugs.freedesktop.org/show_bug.cgi?id=28045
# modified to work with automake test framework

set -e

if [ "x$MAKEDEPEND" == "x" ]; then
    MAKEDEPEND=makedepend
fi

mkdir -p ${test_builddir}/28045
cd ${test_builddir}/28045

# Create test conditions:
#  - directory named "one"
#  - directory named "two" containing empty file named "one"
mkdir one two
touch two/one

# Happens to work before fix for 28045
$MAKEDEPEND -f- -- -Itwo -Ione -I. -- ${test_srcdir}/28045/foo.cpp > /dev/null

# Happens to fail before fix for 28045
$MAKEDEPEND -f- -- -I. -Ione -Itwo -- ${test_srcdir}/28045/foo.cpp > /dev/null

# Clean up
rm -rf one two
