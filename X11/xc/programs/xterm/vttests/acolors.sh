#!/bin/sh
# $XFree86: xc/programs/xterm/vttests/acolors.sh,v 1.1 2002/09/30 00:39:08 dickey Exp $
#
# -- Thomas Dickey (1999/3/27)
# Demonstrate the use of the control sequence for changing ANSI colors.

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

LIST="00 30 80 d0 ff"

exec </dev/tty
old=`stty -g`
stty raw -echo min 0  time 5

$CMD $OPT "${ESC}]4;4;?${SUF}" > /dev/tty
read original
stty $old
original=${original}${SUF}

trap '$CMD $OPT "$original" >/dev/tty; exit' 0 1 2 5 15
$CMD "${ESC}[0;1;34mThis message is BLUE"
$CMD "${ESC}[0;1;31mThis message is RED ${ESC}[0;31m(sometimes)"
$CMD "${ESC}[0;1;32mThis message is GREEN${ESC}[0m"
while true
do
    for R in $LIST
    do
	for G in $LIST
	do
	    for B in $LIST
	    do
		# color "9" is bold-red
		test $R != 00 && test $G = 00 && test $B = 00 && $CMD $OPT "" >/dev/tty
		$CMD $OPT "${ESC}]4;9;rgb:$R/$G/$B${SUF}" >/dev/tty
		sleep 1
	    done
	done
    done
done
