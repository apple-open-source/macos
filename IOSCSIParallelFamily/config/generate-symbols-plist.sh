#!/bin/sh

set -o errexit
set -o xtrace

SYMBOLSET_PLIST="${SCRIPT_OUTPUT_FILE_0}"
INFO_PLIST="${SCRIPT_INPUT_FILE_0}"
EXPORTS_LIST="${SCRIPT_INPUT_FILE_1}"

if [ "${SYMBOLSET_PLIST##*.}" != "plist" ]; then
    echo "${0}: ${SYMBOLSET_PLIST}: must be a plist" > /dev/stderr
    exit 1
fi
if [ "${INFO_PLIST##*.}" != "plist" ]; then
    echo "${0}: ${INFO_PLIST}: must be a plist" > /dev/stderr
    exit 1
fi

printf \
'<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
' > "${SYMBOLSET_PLIST}"

awk '
	/CFBundleIdentifier|OSBundleCompatibleVersion|CFBundleVersion/ {
		print; getline; print
	}
' ${INFO_PLIST} >> "${SYMBOLSET_PLIST}"

sort -u "${EXPORTS_LIST}" | awk -F: '
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
' >> "${SYMBOLSET_PLIST}"

printf \
'</dict>
</plist>
' >> "${SYMBOLSET_PLIST}"
