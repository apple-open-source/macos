#!/bin/sh
# $FreeBSD$

base=`basename $0`

echo "1..4"

#ifdef __APPLE__
fails=0
#endif
name="pgrep -x"
sleep=$(pwd)/sleep.txt
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
pid=$!
#ifdef __APPLE__
# macOS preserves the link's name.
if [ ! -z "`pgrep -x sleep.txt | egrep '^'"$pid"'$'`" ]; then
#else
#if [ ! -z "`pgrep -x sleep | egrep '^'"$pid"'$'`" ]; then
#endif
	echo "ok 1 - $name"
else
	echo "not ok 1 - $name"
#ifdef __APPLE__
	fails=$((fails + 1))
#endif
fi
if [ -z "`pgrep -x slee | egrep '^'"$pid"'$'`" ]; then
	echo "ok 2 - $name"
else
	echo "not ok 2 - $name"
#ifdef __APPLE__
	fails=$((fails + 1))
#endif
fi
name="pgrep -x -f"
if [ ! -z "`pgrep -x -f ''"$sleep"' 5' | egrep '^'"$pid"'$'`" ]; then
	echo "ok 3 - $name"
else
	echo "not ok 3 - $name"
#ifdef __APPLE__
	fails=$((fails + 1))
#endif
fi
if [ -z "`pgrep -x -f ''"$sleep"' ' | egrep '^'"$pid"'$'`" ]; then
	echo "ok 4 - $name"
else
	echo "not ok 4 - $name"
#ifdef __APPLE__
	fails=$((fails + 1))
#endif
fi
kill $pid
rm -f $sleep
#ifdef __APPLE__
exit $fails
#endif
