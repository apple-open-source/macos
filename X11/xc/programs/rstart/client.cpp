XCOMM! /bin/sh
XCOMM $Xorg: client.cpp,v 1.4 2000/12/20 16:41:43 pookie Exp $
XCOMM

XCOMM Copyright (c) 1993 Quarterdeck Office Systems
XCOMM
XCOMM Permission to use, copy, modify, distribute, and sell this software
XCOMM and software and its documentation for any purpose is hereby granted
XCOMM without fee, provided that the above copyright notice appear in all
XCOMM copies and that both that copyright notice and this permission
XCOMM notice appear in supporting documentation, and that the name
XCOMM Quarterdeck Office Systems, Inc. not be used in advertising or
XCOMM publicity pertaining to distribution of this software without
XCOMM specific, written prior permission.
XCOMM
XCOMM THIS SOFTWARE IS PROVIDED "AS-IS".  QUARTERDECK OFFICE SYSTEMS,
XCOMM INC., DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
XCOMM INCLUDING WITHOUT LIMITATION ALL IMPLIED WARRANTIES OF
XCOMM MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, OR
XCOMM NONINFRINGEMENT.  IN NO EVENT SHALL QUARTERDECK OFFICE SYSTEMS,
XCOMM INC., BE LIABLE FOR ANY DAMAGES WHATSOEVER, INCLUDING SPECIAL,
XCOMM INCIDENTAL OR CONSEQUENTIAL DAMAGES, INCLUDING LOSS OF USE, DATA, OR
XCOMM PROFITS, EVEN IF ADVISED OF THE POSSIBILITY THEREOF, AND REGARDLESS
XCOMM OF WHETHER IN AN ACTION IN CONTRACT, TORT OR NEGLIGENCE, ARISING OUT
XCOMM OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

usage() {
    if [ -n "$1" ]
    then
	echo "`basename $0`: $1"
    fi

    echo ""
    echo "Usage:  `basename $0` [options] hostname command args ..."
    echo ""
    echo "where options include:"
    echo "    -c context    run command in the specified context"
    echo "    -g            interpret command as a generic command"
    echo "    -l username   run command as the specified user"
    echo "    -v            enable verbose output"
    exit 1
}

if [ $# -eq 0 ]
then
    usage
fi

context=X
verbose=DETACH
name=
kind=CMD

while :
do
	case $1 in
	-c)
		if [ $# -lt 2 ]
		then
			usage "-c option requires an argument"
		fi

		context=$2
		shift; shift
		;;
	-g)
		kind=GENERIC-CMD
		shift
		;;
	-l)
		if [ $# -lt 2 ]
		then
			usage "-l option requires an argument"
		fi

		name="-l $2"
		shift; shift
		;;
	-v)
		verbose=NODETACH
		shift
		;;
	*)
		if [ $# -eq 0 ]
		then
			usage "missing host name"
		fi

		host=$1
		shift
		break
		;;
	esac
done

if [ $# -eq 0 ]
then
	usage "missing command"
fi

case $verbose in
DETACH)
	exec > /dev/null
	;;
esac

case "$DISPLAY" in
:*)	disp="`uname -n`$DISPLAY"
	echo expanded $DISPLAY to $disp
	;;
*)	disp="$DISPLAY"
	;;
esac

(
cat << /
CONTEXT $context
MISC X DISPLAY=$disp
$kind $*
$verbose
/
xauth list $disp | sed 's/^/AUTH X11 /'
echo ""
) | RSHCMD $host $name SERVERNAME
