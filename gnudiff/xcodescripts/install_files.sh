#!/bin/sh

# exit immediately on failure
set -e
set -x

MANDIR=/usr/share/man

install -d -o root -g wheel -m 0755 "$DSTROOT"/"$MANDIR"/man1
install -c -o root -g wheel -m 0644 "$PROJECT_DIR"/diffutils/man/diff3.1 "$DSTROOT"/"$MANDIR"/man1

# Install open source information

OSV=/usr/local/OpenSourceVersions
OSL=/usr/local/OpenSourceLicenses

install -d -o root -g wheel -m 0755 "$DSTROOT"/"$OSV"
install -c -o root -g wheel -m 0444 "$PROJECT_DIR"/gnudiff.plist "$DSTROOT"/"$OSV"
install -d -o root -g wheel -m 0755 "$DSTROOT"/"$OSL"
install -c -o root -g wheel -m 0444 "$PROJECT_DIR"/diffutils/COPYING "$DSTROOT"/"$OSL"/gnudiff.txt
