#!/bin/sh
set -e -x

# Do nothing for installhdrs
[ "$ACTION" == "installhdrs" ] && exit 0

ver=5.4

if [ "$ACTION" == "install" ] ; then
    for link in libform.dylib libmenu.dylib libncurses.dylib libpanel.dylib ; do
	ln -s -f $(basename -s .dylib $link).${ver}.dylib "$DSTROOT/usr/lib/$link"
    done

    for link in libcurses.dylib libtermcap.dylib ; do
	ln -s -f libncurses.${ver}.dylib $DSTROOT/usr/lib/$link
    done

    # rdar://problem/11542731
    ln -s -f libncurses.${ver}.dylib "$DSTROOT"/usr/lib/libncurses.5.dylib
fi

for link in libform.tbd libmenu.tbd libncurses.tbd libpanel.tbd ; do
    ln -s -f $(basename -s .tbd $link).${ver}.tbd "$DSTROOT/usr/lib/$link"
done

for link in libcurses.tbd libtermcap.tbd ; do
    ln -s -f libncurses.${ver}.tbd $DSTROOT/usr/lib/$link
done

ln -s -f libncurses.${ver}.tbd "$DSTROOT"/usr/lib/libncurses.5.tbd
