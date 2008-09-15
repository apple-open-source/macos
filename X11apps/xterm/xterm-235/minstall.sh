#!/bin/sh
# $XTermId: minstall.sh,v 1.13 2008/01/27 15:45:03 tom Exp $
#
# Install manpages, substituting a reasonable section value since XFree86 4.x
# and derived imakes do not use constants...
#
# Parameters:
#	$1 = program to invoke as "install"
#	$2 = manpage to install
#	$3 = final installed-path
#	$4 = app-defaults directory
#

# override locale...
LANG=C;		export LANG
LANGUAGE=C;	export LANGUAGE
LC_ALL=C;	export LC_ALL
LC_CTYPE=C;	export LC_CTYPE
XTERM_LOCALE=C	export XTERM_LOCALE

# these could interfere with the "man" command.
unset MANPAGER
unset PAGER

# get parameters
MINSTALL="$1"
OLD_FILE="$2"
END_FILE="$3"
APPS_DIR="$4"

suffix=`echo "$END_FILE" | sed -e 's%^.*\.%%'`
NEW_FILE=temp$$

MY_MANSECT=$suffix

# "X" is usually in the miscellaneous section, along with "undocumented".
# Use that to guess an appropriate section.
X_MANSECT=`man X 2>&1 | tr '\012' '\020' | sed -e 's/^[^0123456789]*\([^) ][^) ]*\).*/\1/'`
test -z "$X_MANSECT" && X_MANSECT=$suffix

sed	-e 's%__vendorversion__%"X Window System"%' \
	-e s%__apploaddir__%$APPS_DIR% \
	-e s%__mansuffix__%$MY_MANSECT%g \
	-e s%__miscmansuffix__%$X_MANSECT%g \
	$OLD_FILE >$NEW_FILE

echo "$MINSTALL $OLD_FILE $END_FILE"
eval "$MINSTALL $NEW_FILE $END_FILE"

rm -f $NEW_FILE
