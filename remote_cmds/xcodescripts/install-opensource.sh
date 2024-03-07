#!/bin/sh
# Generate and install open source plist

set -e

OSV="$DSTROOT"/usr/local/OpenSourceVersions
install -d -m 0755 "$OSV"

tmplist=$(mktemp -t remote_cmds_osv_plist)
trap 'rm "$tmplist"' EXIT

(
	echo '<?xml version="1.0" encoding="UTF-8"?>'
	echo '<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">'
	echo '<plist version="1.0">'
	echo '<array>'
	for plistpart in "$SRCROOT"/*/*.plist.part ; do
		cat "$plistpart"
	done
	echo '</array>'
	echo '</plist>'
) >"$tmplist"

plutil -lint "$tmplist"
install -m 0644 "$tmplist" "$OSV"/remote_cmds.plist
