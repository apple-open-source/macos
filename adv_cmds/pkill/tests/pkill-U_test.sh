#!/bin/sh
# $FreeBSD$

base=`basename $0`

echo "1..2"

#ifdef __APPLE__
fails=0
#endif
name="pkill -U <uid>"
ruid=`id -ur`
sleep=$(pwd)/sleep.txt
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
pkill -f -U $ruid $sleep
ec=$?
case $ec in
0)
	echo "ok 1 - $name"
	;;
*)
	echo "not ok 1 - $name"
#ifdef __APPLE__
	fails=$((fails + 1))
#endif
	;;
esac
rm -f $sleep

name="pkill -U <user>"
ruid=`id -urn`
sleep=$(pwd)/sleep.txt
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
pkill -f -U $ruid $sleep
ec=$?
case $ec in
0)
	echo "ok 2 - $name"
	;;
*)
	echo "not ok 2 - $name"
#ifdef __APPLE__
	fails=$((fails + 1))
#endif
	;;
esac
rm -f $sleep
#ifdef __APPLE__
exit $fails
#endif
