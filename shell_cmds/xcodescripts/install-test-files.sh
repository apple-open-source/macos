#!/bin/sh
set -e -x

install -d -o root -g wheel -m 0755 "$DSTROOT"/AppleInternal/Tests/shell_cmds
install -o root -g wheel -m 0644 "$SRCROOT"/tests/regress.m4 \
	"$DSTROOT"/AppleInternal/Tests/shell_cmds

install -d -o root -g wheel -m 0755 \
	"$DSTROOT"/AppleInternal/Tests/shell_cmds/time
install -o root -g wheel -m 0644 "$SRCROOT"/time/tests/test_time.sh \
	"$DSTROOT"/AppleInternal/Tests/shell_cmds/time

install -d -o root -g wheel -m 0755 \
	"$DSTROOT"/AppleInternal/CoreOS/BATS/unit_tests
tmplist=$(mktemp -t shell_cmds_plist)
trap 'rm "$tmplist"' EXIT

cat <<EOF > "$tmplist"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>Project</key>
	<string>shell_cmds</string>
	<key>Tests</key>
	<array>
EOF

cat "$SRCROOT"/tests/shell_cmds.plist.common >> "$tmplist"
if [ -n "$1" ]; then
	osplist="$SRCROOT"/tests/shell_cmds.plist."$1"

	[ -f "$osplist" ] && cat "$osplist" >> "$tmplist"
fi

cat <<EOF >> "$tmplist"
	</array>
	<key>Timeout</key>
	<integer>30</integer>
</dict>
</plist>
EOF

install -o root -g wheel -m 0644 "$tmplist" \
	"$DSTROOT"/AppleInternal/CoreOS/BATS/unit_tests/shell_cmds.plist
