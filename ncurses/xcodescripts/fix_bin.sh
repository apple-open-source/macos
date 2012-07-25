#!/bin/sh
set -e -x

# Do nothing for installhdrs
[ "$ACTION" == "installhdrs" ] && exit 0

if [ "${RC_ProjectName%_Sim}" != "${RC_ProjectName}" ] ; then
	[ -z "${DSTROOT}" ] && exit 1
	if [ -d "${DSTROOT}${SDKROOT}/usr/bin" ]; then
		rm -rf "${DSTROOT}${SDKROOT}/usr/bin"
	fi
	exit 0
fi

ln -s tset "$DSTROOT"/usr/bin/reset
ln -s tic "$DSTROOT"/usr/bin/captoinfo
ln -s tic "$DSTROOT"/usr/bin/infotocap

install -g "$INSTALL_GROUP" -o "$INSTALL_OWNER" -m "$INSTALL_MODE_FLAG" \
	"$SRCROOT/ncurses/misc/ncurses-config" \
	"$DSTROOT/usr/bin/ncurses5.4-config"

chmod +x "$DSTROOT/usr/bin/ncurses5.4-config"
