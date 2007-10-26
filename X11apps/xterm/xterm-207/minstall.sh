#!/bin/sh
# $XFree86: xc/programs/xterm/minstall.sh,v 1.4 2002/12/27 21:05:22 dickey Exp $
#
# Install manpages, substituting a reasonable section value since XFree86 4.x
# doesn't use constants...
#
# Parameters:
#	$1 = program to invoke as "install"
#	$2 = manpage to install
#	$3 = final installed-path
#

MINSTALL="$1"
OLD_FILE="$2"
END_FILE="$3"

suffix=`echo "$END_FILE" | sed -e 's%^[^.]*.%%'`
NEW_FILE=temp$$

sed	-e 's%__vendorversion__%"X Window System"%' \
	-e s%__apploaddir__%/usr/lib/X11/app-defaults% \
	-e s%__mansuffix__%$suffix%g \
	-e s%__miscmansuffix__%$suffix%g \
	$OLD_FILE >$NEW_FILE

echo "$MINSTALL $OLD_FILE $END_FILE"
eval "$MINSTALL $NEW_FILE $END_FILE"

rm -f $NEW_FILE
