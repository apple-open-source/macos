#! /bin/bash
# Copyright (C) 2007 Apple Inc. All rights reserved.

# Script to build the skeleton of an arbitrary bundle.

BUNDLE="$1"
BUNDLEPATH="${2:-.}"

INSTALL="${INSTALL:-/usr/bin/install}"
SUBDIRS=

set -e

case $BUNDLE in
    *.framework|*.app|*.plugin|*.kext)
	SUBDIRS="Headers Resources Libraries"
	;;
    *.bundle)
	# As per mastering verification, bundles are not allowed to
	# contain headers.
	SUBDIRS="Resources Libraries"
	;;
    *)
	echo "$BUNDLE is not a valid bundle name" 2>&1
	exit 2
	;;
esac

BASE="$BUNDLEPATH/$BUNDLE"

$INSTALL -d -m 755 $BASE/Versions/A
ln -s A $BASE/Versions/Current

for d in $SUBDIRS ; do
    $INSTALL -d -m 755 $BASE/Versions/Current/$d
done

for d in $(ls $BASE/Versions/Current) ; do
    ln -s Versions/Current/$d $BASE/$d
done

