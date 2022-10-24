#!/bin/sh
# $FreeBSD$

base=`basename $0`

echo "1..1"

#ifdef __APPLE__
fails=0
#endif
name="pgrep -l"
sleep=$(pwd)/sleep.txt
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
pid=$!
if [ "$pid $sleep 5" = "`pgrep -f -l $sleep`" ]; then
	echo "ok - $name"
else
	echo "not ok - $name"
#ifdef __APPLE__
	fails=$((fails + 1))
#endif
fi
kill $pid
rm -f $sleep
#ifdef __APPLE__
exit $fails
#endif
