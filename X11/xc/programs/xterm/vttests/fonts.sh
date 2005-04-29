#!/bin/sh
# $XFree86: xc/programs/xterm/vttests/fonts.sh,v 1.4 2003/05/19 00:52:30 dickey Exp $
#
# -- Thomas Dickey (1999/3/27)
# Demonstrate the use of dynamic colors by setting the background successively
# to different values.

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

exec </dev/tty
old=`stty -g`
stty raw -echo min 0  time 5

$CMD $OPT "${ESC}]50;?${SUF}" > /dev/tty
read original

stty $old
original="${original}${SUF}"

if ( trap "echo exit" EXIT 2>/dev/null ) >/dev/null
then
    trap '$CMD $OPT "$original" >/dev/tty; exit' EXIT HUP INT TRAP TERM
else
    trap '$CMD $OPT "$original" >/dev/tty; exit' 0    1   2   5    15
fi

F=1
D=1
T=6
while true
do
    $CMD $OPT "${ESC}]50;#$F${SUF}" >/dev/tty
    #sleep 1
    if test .$D = .1 ; then
	test $F = $T && D=-1
    else
	test $F = 1 && D=1
    fi
    F=`expr $F + $D`
done
