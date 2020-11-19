#!/bin/sh

set -e

if [ $# -ne 3 ]; then
    echo "Usage: $0 output.plist Info.plist input1.exports" 1>&2
    exit 1
fi

OUTPUT="$1"
PLIST="$2"
EXPORTS="$3"
if [ "${OUTPUT##*.}" != "plist" -o "${PLIST##*.}" != "plist" ]; then
    echo "Usage: $0 output.plist Info.plist input1.exports" 1>&2
    exit 1
fi

printf \
'<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
' > "$OUTPUT"

awk '
	/CFBundleIdentifier|OSBundleCompatibleVersion|CFBundleVersion/ {
		print; getline; print
	}
' $PLIST >> "$OUTPUT"

sort -u "$EXPORTS" | awk -F: '
	BEGIN {
		print "	<key>Symbols</key>"
		print "	<array>"
	}
	$1 ~ /^_/ {
		print "		<string>"$1"</string>"
		next
	}
	END {
		print "	</array>"
	}
' >> "$OUTPUT"

printf \
'</dict>
</plist>
' >> "$OUTPUT"

exit 0