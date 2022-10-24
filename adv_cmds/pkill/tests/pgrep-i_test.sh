#!/bin/sh
# $FreeBSD$

base=`basename $0`

echo "1..1"

#ifdef __APPLE__
fails=0
#endif
name="pgrep -i"
sleep=$(pwd)/sleep.txt
usleep="${sleep}XXX"
touch $usleep
lsleep="${sleep}xxx"
ln -sf /bin/sleep $usleep
$usleep 5 &
sleep 0.3
chpid=$!
pid=`pgrep -f -i $lsleep`
if [ "$pid" = "$chpid" ]; then
	echo "ok - $name"
else
	echo "not ok - $name"
#ifdef __APPLE__
	fails=$((fails + 1))
#endif
fi
kill $chpid
rm -f $sleep $usleep
#ifdef __APPLE__
exit $fails
#endif
