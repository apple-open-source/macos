#!/bin/sh
# $FreeBSD$

base=`basename $0`

echo "1..2"

#ifdef __APPLE__
fails=0
#endif
name="pgrep -v"
sleep=$(pwd)/sleep.txt
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
pid=$!
if [ -z "`pgrep -f -v $sleep | egrep '^'"$pid"'$'`" ]; then
	echo "ok 1 - $name"
else
	echo "not ok 1 - $name"
#ifdef __APPLE__
	fails=$((fails + 1))
#endif
fi
if [ ! -z "`pgrep -f -v -x x | egrep '^'"$pid"'$'`" ]; then
	echo "ok 2 - $name"
else
	echo "not ok 2 - $name"
#ifdef __APPLE__
	fails=$((fails + 1))
#endif
fi
kill $pid
rm -f $sleep
#ifdef __APPLE__
exit $fails
#endif
