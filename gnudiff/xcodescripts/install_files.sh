#!/bin/sh

# exit immediately on failure
set -e
set -x

MANDIR=/usr/share/man
INFODIR=/usr/share/info

install -d -o root -g wheel -m 0755 "$DSTROOT"/"$MANDIR"/man1
install -c -o root -g wheel -m 0644 "$PROJECT_DIR"/diffutils/man/cmp.1 "$DSTROOT"/"$MANDIR"/man1
install -c -o root -g wheel -m 0644 "$PROJECT_DIR"/diffutils/man/diff.1 "$DSTROOT"/"$MANDIR"/man1
install -c -o root -g wheel -m 0644 "$PROJECT_DIR"/diffutils/man/diff3.1 "$DSTROOT"/"$MANDIR"/man1
install -c -o root -g wheel -m 0644 "$PROJECT_DIR"/diffutils/man/sdiff.1 "$DSTROOT"/"$MANDIR"/man1

install -d -o root -g wheel -m 0755 "$DSTROOT"/"$INFODIR"
install -c -o root -g wheel -m 0644 "$PROJECT_DIR"/diffutils/doc/diff.info "$DSTROOT"/"$INFODIR"

# Install open source information

OSV=/usr/local/OpenSourceVersions
OSL=/usr/local/OpenSourceLicenses

install -d -o root -g wheel -m 0755 "$DSTROOT"/"$OSV"
install -c -o root -g wheel -m 0444 "$PROJECT_DIR"/gnudiff.plist "$DSTROOT"/"$OSV"
install -d -o root -g wheel -m 0755 "$DSTROOT"/"$OSL"
install -c -o root -g wheel -m 0444 "$PROJECT_DIR"/diffutils/COPYING "$DSTROOT"/"$OSL"/gnudiff.txt
