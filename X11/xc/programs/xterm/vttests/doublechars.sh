#!/bin/sh
# $XFree86: xc/programs/xterm/vttests/doublechars.sh,v 1.4 2002/09/30 00:39:08 dickey Exp $
#
# -- Thomas Dickey (1999/7/7)
# Illustrate the use of double-size characters by drawing successive lines in
# the commonly used video attributes.
#
# Use the -w option to force the output to wrap.  It will look ugly, because
# the double-high lines will be split.

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

SAVE=yes
WRAP=no
if test $# != 0 ; then
    while test $# != 0
    do
    	case $1 in
	-n)	SAVE=no ;;
	-w)	WRAP=yes ;;
	esac
	shift
    done
fi

if test $SAVE = yes ; then
    exec </dev/tty
    old=`stty -g`
    stty raw -echo min 0  time 5

    $CMD $OPT "${ESC}[18t${SUF}" > /dev/tty
    IFS=';' read junk high wide

    stty $old

    wide=`echo $wide|sed -e 's/t.*//'`
    original=${ESC}[8\;${high}\;${wide}t${SUF}
    trap '$CMD $OPT "$original" >/dev/tty; exit' 0 1 2 5 15
fi

if test $WRAP = yes ; then
	# turn on wrapping and force the screen to 80 columns
	$CMD $OPT "${ESC}[?7h" >/dev/tty
	$CMD $OPT "${ESC}[?40l" >/dev/tty
else
	# force the screen to 132 columns
	$CMD $OPT "${ESC}[?40h" >/dev/tty
	$CMD $OPT "${ESC}[?3h" >/dev/tty
fi

for SGR in 0 1 4 5 7
do
	$CMD $OPT "${ESC}[0;${SGR}m" >/dev/tty
	for DBL in 5 3 4 6 5
	do
		$CMD $OPT "${ESC}#${DBL}" >/dev/tty
		echo "The quick brown fox jumps over the lazy dog" >/dev/tty
	done
	echo
done
$CMD $OPT "${ESC}[0m" >/dev/tty
