#!/bin/sh
# $XFree86: xc/programs/xterm/vttests/title.sh,v 1.5 2002/09/30 00:39:08 dickey Exp $
#
# -- Thomas Dickey (1999/3/27)
# Obtain the current title of the window, set up a simple clock which runs
# until this script is interrupted, then restore the title.

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

$CMD $OPT "${ESC}[21t${SUF}" > /dev/tty
read original

stty $old

# We actually get this terminated by an <esc>backslash, but the backslash
# is lost.  We may lose doublequote characters when restoring the title,
# depending on the shell.
original=`echo "$original" |sed -e 's/^...//' -e 's/.$//'`
original=${ESC}]2\;"${original}"${SUF}

trap '$CMD $OPT "$original" >/dev/tty; exit' 0 1 2 5 15
while true
do
	sleep 1
	$CMD $OPT "${ESC}]2;`date`" >/dev/tty
done
