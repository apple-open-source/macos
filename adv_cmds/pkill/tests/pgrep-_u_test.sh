#!/bin/sh
# $FreeBSD$

base=`basename $0`

echo "1..2"

#ifdef __APPLE__
fails=0
#endif
name="pgrep -U <uid>"
ruid=`id -ur`
sleep=$(pwd)/sleep.txt
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
chpid=$!
pid=`pgrep -f -U $ruid $sleep`
if [ "$pid" = "$chpid" ]; then
	echo "ok 1 - $name"
else
	echo "not ok 1 - $name"
#ifdef __APPLE__
	fails=$((fails + 1))
#endif
fi
kill $chpid
rm -f $sleep

name="pgrep -U <user>"
ruid=`id -urn`
sleep=$(pwd)/sleep.txt
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
chpid=$!
pid=`pgrep -f -U $ruid $sleep`
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
#ifdef __APPLE_
exit $fails
#endif
