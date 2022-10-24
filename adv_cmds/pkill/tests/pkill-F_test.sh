#!/bin/sh
# $FreeBSD$

base=`basename $0`

echo "1..1"

#ifdef __APPLE__
fails=0
#endif
name="pkill -F <pidfile>"
pidfile=$(pwd)/pidfile.txt
sleep=$(pwd)/sleep.txt
ln -sf /bin/sleep $sleep
$sleep 5 &
sleep 0.3
echo $! > $pidfile
pkill -f -F $pidfile $sleep
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

rm -f $pidfile
rm -f $sleep
#ifdef __APPLE__
exit $fails
#endif
