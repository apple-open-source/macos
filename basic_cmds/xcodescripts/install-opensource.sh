#!/bin/sh
# Generate and install open source plist

set -e

OSV="$DSTROOT"/usr/local/OpenSourceVersions
install -d -o root -g wheel -m 0755 "$OSV"

PLIST="$OSV"/basic_cmds.plist
echo '<plist version="1.0">' > "$PLIST"
echo '<array>' >> "$PLIST"

for plistpart in "$SRCROOT"/*/*.plist.part; do
    cat "$plistpart" >> "$PLIST"
done

echo '</array>' >> "$PLIST"
echo '</plist>' >> "$PLIST"
