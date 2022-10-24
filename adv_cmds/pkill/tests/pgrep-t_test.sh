#!/bin/sh
# $FreeBSD$

base=`basename $0`

echo "1..2"

#ifdef __APPLE__
fails=0
#endif
name="pgrep -t <tty>"
tty=`ps -x -o tty -p $$ | tail -1`
if [ "$tty" = "??" -o "$tty" = "-" ]; then
	tty="-"
	ttyshort="-"
else
	case $tty in
	pts/*)	ttyshort=`echo $tty | cut -c 5-` ;;
	*)	ttyshort=`echo $tty | cut -c 4-` ;;
	esac
fi
sleep=$(pwd)/sleep.txt
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
chpid=$!
pid=`pgrep -f -t $tty $sleep`
if [ "$pid" = "$chpid" ]; then
	echo "ok 1 - $name"
else
	echo "not ok 1 - $name"
#ifdef __APPLE__
	fails=$((fails + 1))
#endif
fi
pid=`pgrep -f -t $ttyshort $sleep`
if [ "$pid" = "$chpid" ]; then
	echo "ok 2 - $name"
else
	echo "not ok 2 - $name"
#ifdef __APPLE__
	fails=$((fails + 1))
#endif
fi
kill $chpid
rm -f $sleep
#ifdef __APPLE__
exit $fails
#endif
