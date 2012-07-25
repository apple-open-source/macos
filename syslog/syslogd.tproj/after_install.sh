#! /bin/bash
set -e

install -d -m 0755 -o root -g wheel "$DSTROOT"/private/var/log/asl

PRODUCT=$(xcodebuild -sdk "${SDKROOT}" -version PlatformPath | head -1 | sed 's,^.*/\([^/]*\)\.platform$,\1,')

if [ ${SDKROOT}x = x ]; then
	PRODUCT=MacOSX
fi

if [ ${PRODUCT}x = x ]; then
	PRODUCT=MacOSX
fi

if [ ${PRODUCT} = iPhone ]; then
    install -d -m 0755 -o root -g wheel "$DSTROOT"/usr/share/sandbox
    install -m 0644 -o root -g wheel "$SRCROOT"/syslogd.tproj/syslogd.sb "$DSTROOT"/usr/share/sandbox
fi

DESTDIR="$DSTROOT"/System/Library/LaunchDaemons
install -d -m 0755 -o root -g wheel "$DESTDIR"
install -m 0644 -o root -g wheel "$SRCROOT"/syslogd.tproj/com.apple.syslogd.plist "$DESTDIR"
if [ ${PRODUCT} = iPhoneOS ]; then
	/usr/libexec/PlistBuddy \
		-c "Add :POSIXSpawnType string Interactive" \
		"$DESTDIR"/com.apple.syslogd.plist
fi
plutil -convert binary1 "$DESTDIR"/com.apple.syslogd.plist

mkfile 8 "$DSTROOT"/private/var/log/asl/SweepStore
chmod 0644 "$DSTROOT"/private/var/log/asl/SweepStore
