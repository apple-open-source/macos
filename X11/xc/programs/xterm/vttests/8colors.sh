#!/bin/sh
# $XFree86: xc/programs/xterm/vttests/8colors.sh,v 1.4 2002/09/30 00:39:08 dickey Exp $
#
# -- Thomas Dickey (1999/3/27)
# Show a simple 8-color test pattern

ESC=""
CMD='echo'
OPT='-n'
SUF=''
TMP=/tmp/xterm$$
eval '$CMD $OPT >$TMP || echo fail >$TMP' 2>/dev/null
( test ! -f $TMP || test -s $TMP ) &&
for verb in printf print ; do
    rm -f $TMP
    eval '$verb "\c" >$TMP || echo fail >$TMP' 2>/dev/null
    if test -f $TMP ; then
	if test ! -s $TMP ; then
	    CMD="$verb"
	    OPT=
	    SUF='\c'
	    break
	fi
    fi
done
rm -f $TMP

trap '$CMD $OPT "[0m"; exit' 0 1 2 5 15
echo "[0m"
while true
do
    for AT in 0 1 4 7
    do
    	case $AT in
	0) attr="normal  ";;
	1) attr="bold    ";;
	4) attr="under   ";;
	7) attr="reverse ";;
	esac
	for FG in 0 1 2 3 4 5 6 7
	do
	    case $FG in
	    0) fcolor="black   ";;
	    1) fcolor="red     ";;
	    2) fcolor="green   ";;
	    3) fcolor="yellow  ";;
	    4) fcolor="blue    ";;
	    5) fcolor="magenta ";;
	    6) fcolor="cyan    ";;
	    7) fcolor="white   ";;
	    esac
	    $CMD $OPT "[0;${AT}m$attr"
	    $CMD $OPT "[3${FG}m$fcolor"
	    for BG in 1 2 3 4 5 6 7
	    do
		case $BG in
		0) bcolor="black   ";;
		1) bcolor="red     ";;
		2) bcolor="green   ";;
		3) bcolor="yellow  ";;
		4) bcolor="blue    ";;
		5) bcolor="magenta ";;
		6) bcolor="cyan    ";;
		7) bcolor="white   ";;
		esac
		$CMD $OPT "[4${BG}m$bcolor"
	    done
	    echo "[0m"
	done
	sleep 1
    done
done
