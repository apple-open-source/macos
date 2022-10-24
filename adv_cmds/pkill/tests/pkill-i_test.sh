#!/bin/sh
# $FreeBSD$

base=`basename $0`

echo "1..1"

#ifdef __APPLE__
fails=0
#endif
name="pkill -i"
sleep=$(pwd)/sleep.txt
usleep="${sleep}XXX"
touch $usleep
lsleep="${sleep}xxx"
ln -sf /bin/sleep $usleep
$usleep 5 &
sleep 0.3
pkill -f -i $lsleep
ec=$?
case $ec in
0)
	echo "ok - $name"
	;;
*)
	echo "not ok - $name"
#ifdef __APPLE__
	fails=$((fails + 1))
#endif
	;;
esac
rm -f $sleep $usleep
#ifdef __APPLE__
exit $fails
#endif
