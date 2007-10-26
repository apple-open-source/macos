#!/bin/sh
# $XTermId: plink.sh,v 1.4 2005/05/03 00:38:24 tom Exp $
# $XFree86: xc/programs/xterm/plink.sh,v 3.2 2005/05/03 00:38:24 dickey Exp $
#
# Reduce the number of dynamic libraries used to link an executable.
LINKIT=
while test $# != 0
do
	OPT="$1"
	shift
	case $OPT in
	-l*)
		echo "testing if $OPT is needed"
		if ( eval $LINKIT $* >/dev/null 2>/dev/null )
		then
			: echo ...no
		else
			echo ...yes
			LINKIT="$LINKIT $OPT"
		fi
		;;
	*)
		LINKIT="$LINKIT $OPT"
		;;
	esac
done
eval $LINKIT
