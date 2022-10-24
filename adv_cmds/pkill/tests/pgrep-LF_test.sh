#!/bin/sh
# $FreeBSD$

base=`basename $0`

echo "1..2"

#ifdef __APPLE__
fails=0
#endif
name="pgrep -LF <pidfile>"
pidfile=$(pwd)/pidfile.txt
sleep=$(pwd)/sleep.txt
ln -sf /bin/sleep $sleep
#ifdef __APPLE__
cat <<EOF > run-sleep.sh
$sleep 5 &
pid=\$!
echo \$pid > $pidfile
shlock -f $pidfile
EOF
sh run-sleep.sh &
#else
#daemon -p $pidfile $sleep 5
#endif
sleep 0.3
chpid=`cat $pidfile`
pid=`pgrep -f -L -F $pidfile $sleep`
if [ "$pid" = "$chpid" ]; then
	echo "ok 1 - $name"
else
	echo "not ok 1 - $name"
#ifdef __APPLE__
	fails=$((fails + 1))
#endif
fi
kill "$chpid"

# Be sure we cannot find process which pidfile is not locked.
$sleep 5 &
sleep 0.3
chpid=$!
echo $chpid > $pidfile
pgrep -f -L -F $pidfile $sleep 2>/dev/null
ec=$?
case $ec in
0)
	echo "not ok 2 - $name"
#ifdef __APPLE__
	fails=$((fails + 1))
#endif
	;;
*)
	echo "ok 2 - $name"
	;;
esac

kill "$chpid"
rm -f $pidfile
rm -f $sleep
#ifdef __APPLE__
rm -f run-sleep.sh
exit $fails
#endif
