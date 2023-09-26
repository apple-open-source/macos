#!/bin/sh
# Generate and install open source plist

set -e

OSV="$DSTROOT"/usr/local/OpenSourceVersions
install -m 0755 -d "$OSV"

tmplist=$(mktemp -t shell_cmds_osv_plist)
trap 'rm "$tmplist"' EXIT

echo '<plist version="1.0">' > "$tmplist"
echo '<array>' >> "$tmplist"

for plistpart in "$SRCROOT"/*/*.plist.part; do
    cat "$plistpart" >> "$tmplist"
done

echo '</array>' >> "$tmplist"
echo '</plist>' >> "$tmplist"

plutil -lint "$tmplist"

install -m 0644 "$tmplist" "$OSV"/shell_cmds.plist
