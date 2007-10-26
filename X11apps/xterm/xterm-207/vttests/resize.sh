#!/bin/sh
# $XFree86: xc/programs/xterm/vttests/resize.sh,v 1.5 2003/05/19 00:52:30 dickey Exp $
#
# -- Thomas Dickey (1999/3/27)
# Obtain the current screen size, then resize the terminal to the nominal
# screen width/height, and restore the size.

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

$CMD $OPT "${ESC}[18t${SUF}" > /dev/tty
IFS=';' read junk high wide

$CMD $OPT "${ESC}[19t${SUF}" > /dev/tty
IFS=';' read junk maxhigh maxwide

stty $old

wide=`echo $wide|sed -e 's/t.*//'`
maxwide=`echo $maxwide|sed -e 's/t.*//'`
original=${ESC}[8\;${high}\;${wide}t${SUF}

test $maxwide = 0 && maxwide=`expr $wide \* 2`
test $maxhigh = 0 && maxhigh=`expr $high \* 2`

if ( trap "echo exit" EXIT 2>/dev/null ) >/dev/null
then
    trap '$CMD $OPT "$original" >/dev/tty; exit' EXIT HUP INT TRAP TERM
else
    trap '$CMD $OPT "$original" >/dev/tty; exit' 0    1   2   5    15
fi

w=$wide
h=$high
a=1
while true
do
#	sleep 1
	echo resizing to $h by $w
	$CMD $OPT "${ESC}[8;${h};${w}t" >/dev/tty
	if test $a = 1 ; then
		if test $w = $maxwide ; then
			h=`expr $h + $a`
			if test $h = $maxhigh ; then
				a=-1
			fi
		else
			w=`expr $w + $a`
		fi
	else
		if test $w = $wide ; then
			h=`expr $h + $a`
			if test $h = $high ; then
				a=1
			fi
		else
			w=`expr $w + $a`
		fi
	fi
done
