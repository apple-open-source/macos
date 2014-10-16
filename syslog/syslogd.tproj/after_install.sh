#! /bin/bash
set -e

if [ "${RC_ProjectName%_Sim}" != "${RC_ProjectName}" ] ; then
    PLIST="${SRCROOT}"/syslogd.tproj/com.apple.syslogd_sim.plist
else
    PLIST="${SRCROOT}"/syslogd.tproj/com.apple.syslogd.plist
fi

DESTDIR="${DSTROOT}${INSTALL_PATH_PREFIX}"/System/Library/LaunchDaemons

install -d -m 0755 -o root -g wheel "${DESTDIR}"
install -m 0644 -o root -g wheel "${PLIST}" "${DESTDIR}"/com.apple.syslogd.plist
plutil -convert binary1 "${DESTDIR}"/com.apple.syslogd.plist

if [ "${RC_ProjectName%_Sim}" != "${RC_ProjectName}" ] ; then
    exit 0
fi

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
