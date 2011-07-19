#!/bin/sh
# $XTermId: minstall.sh,v 1.20 2010/03/03 23:44:49 tom Exp $
# -----------------------------------------------------------------------------
# this file is part of xterm
#
# Copyright 2001-2009,2010 by Thomas E. Dickey
#
#                         All Rights Reserved
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#
# Except as contained in this notice, the name(s) of the above copyright
# holders shall not be used in advertising or otherwise to promote the
# sale, use or other dealings in this Software without prior written
# authorization.
# -----------------------------------------------------------------------------
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

# avoid interference by the "man" command.
for p in /bin /usr/bin
do
if test -f $p/cat ; then
MANPAGER=cat;   export MANPAGER
PAGER=cat;      export PAGER
break
fi
done

# get parameters
MINSTALL="$1"
OLD_FILE="$2"
END_FILE="$3"
APPS_DIR="$4"

suffix=`echo "$END_FILE" | sed -e 's%^.*\.%%'`
NEW_FILE=temp$$

MY_MANSECT=$suffix

# utmp and wtmp have different pathnames on different systems, but there
# are only a few common choices.  Note that they may not necessarily appear
# in the same directories.  Prefer utmpx/wtmpx to utmp/wtmp, since that's
# the way the configure script is designed.
UTMP_NAME=utmp
UTMP_PATH=/etc
for name in /etc /var/adm /var/run /var/log
do
	if test -f $name/utmpx ; then
		UTMP_NAME=utmpx
		UTMP_PATH=$name
		break
	elif test -f $name/utmp ; then
		UTMP_PATH=$name
		break
	fi
done
UTMP_PATH=$UTMP_PATH/$UTMP_NAME

WTMP_NAME=wtmp
WTMP_PATH=/etc
for name in /etc /var/adm /var/run /var/log
do
	if test -f $name/wtmpx ; then
		WTMP_NAME=wtmpx
		WTMP_PATH=$name
		break
	elif test -f $name/wtmp ; then
		WTMP_PATH=$name
		break
	fi
done
WTMP_PATH=$WTMP_PATH/$WTMP_NAME

# "X" is usually in the miscellaneous section, along with "undocumented".
# Use that to guess an appropriate section.
X_MANSECT=`man X 2>&1 | tr '\012' '\020' | sed -e 's/^[^0123456789]*\([^) ][^) ]*\).*/\1/'`
test -z "$X_MANSECT" && X_MANSECT=$suffix

sed	-e 's%__vendorversion__%"X Window System"%' \
	-e s%__apploaddir__%$APPS_DIR% \
	-e s%__mansuffix__%$MY_MANSECT%g \
	-e s%__miscmansuffix__%$X_MANSECT%g \
	-e s%fIutmp'\\%fI'$UTMP_NAME'\\%g' \
	-e s%/etc/utmp%$UTMP_PATH%g \
	-e s%fIwtmp'\\%fI'$WTMP_NAME'\\%g' \
	-e s%/etc/wtmp%$WTMP_PATH%g \
	$OLD_FILE >$NEW_FILE

echo "$MINSTALL $OLD_FILE $END_FILE"
eval "$MINSTALL $NEW_FILE $END_FILE"

rm -f $NEW_FILE
