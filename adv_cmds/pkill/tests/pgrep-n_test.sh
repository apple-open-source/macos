#!/bin/sh
# $FreeBSD$

base=`basename $0`

echo "1..1"

#ifdef __APPLE__
fails=0
#endif
name="pgrep -n"
sleep=$(pwd)/sleep.txt
ln -sf /bin/sleep $sleep
$sleep 5 &
oldpid=$!
$sleep 5 &
sleep 0.3
newpid=$!
pid=`pgrep -f -n $sleep`
if [ "$pid" = "$newpid" ]; then
	echo "ok - $name"
else
	echo "not ok - $name"
#ifdef __APPLE__
	fails=$((fails + 1))
#endif
fi
kill $oldpid
kill $newpid
rm -f $sleep
#ifdef __APPLE__
exit $fails
#endif
