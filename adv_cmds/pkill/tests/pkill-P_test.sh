#!/bin/sh
# $FreeBSD$

base=`basename $0`

echo "1..1"

#ifdef __APPLE__
fails=0
#endif
name="pkill -P <ppid>"
ppid=$$
sleep=$(pwd)/sleep.txt
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
pkill -f -P $ppid $sleep
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

rm -f $sleep
#ifdef __APPLE__
exit $fails
#endif
