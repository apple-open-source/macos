#!/bin/sh
#
# To install a tkcon entry into a FreeDesktop.org compatible menu system such
# as used by GNOME, KDE or most modern X11 desktop environments the tkcon.desktop
# and icons/* files are installed. This should be done using the xdg-desktop-menu
# utility and xdg-icon-resource utility from the xdg-utils package. See
# http://portland.freedesktop.org/xdg-utils-1.0/ for further details.
# 

PROG_XDG_DESKTOP_MENU=`which xdg-desktop-menu`
PROG_XDG_ICON_RESOURCE=`which xdg-icon-resource`

ICONFILE=icons/tkcon-small48.png

if [ -x $PROG_XDG_DESKTOP_MENU -a -x PROG_XDG_ICON_RESOURCE ]
then
    $PROG_XDG_DESKTOP_MENU install tkcon-console.desktop
    $PROG_XDG_ICON_RESOURCE install --size 48 $ICONFILE tkcon-icon
else
    [ -d $HOME/.local/share/applications ] || mkdirhier $HOME/.local/share/applications
    [ -d $HOME/.local/share/icons ] || mkdirhier $HOME/.local/share/icons
    install tkcon-console.desktop $HOME/.local/share/applications/tkcon-console.desktop
    install $ICONFILE $HOME/.local/share/icons/tkcon-icon.png
fi


