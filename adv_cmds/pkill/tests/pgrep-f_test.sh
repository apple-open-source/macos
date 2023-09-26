#!/bin/sh
# $FreeBSD$

#ifdef __APPLE__
fails=0
# XXX No dirname/realpath here, but we know where this will be installed.
base=/AppleInternal/Tests/adv_cmds/pgrep
#else
#: ${ARG_MAX:=524288}
#base=$(dirname $(realpath "$0"))
#endif

echo "1..2"

waitfor() {
	flagfile=$1

	iter=0

	while [ ! -f ${flagfile} ] && [ ${iter} -lt 50 ]; do
		sleep 0.10
		iter=$((iter + 1))
	done

	if [ ! -f ${flagfile} ]; then
		return 1
	fi
}

sentinel="findme=test-$$"
sentinelsz=$(printf "${sentinel}" | wc -c | tr -d '[[:space:]]')
name="pgrep -f"
spin="${base}/spin_helper"
flagfile="pgrep_f_short.flag"

${spin} --short ${flagfile} ${sentinel} &
chpid=$!
if ! waitfor ${flagfile}; then
	echo "not ok - $name"
#ifdef __APPLE__
	fails=$((fails + 1))
#endif
else
	pid=$(pgrep -f ${sentinel})
	if [ "$pid" = "$chpid" ]; then
		echo "ok - $name"
	else
		echo "not ok - $name"
#ifdef __APPLE__
		fails=$((fails + 1))
#endif
	fi
fi
kill $chpid

name="pgrep -f long args"
flagfile="pgrep_f_long.flag"
${spin} --long ${flagfile} ${sentinel} &
chpid=$!
if ! waitfor ${flagfile}; then
	echo "not ok - $name"
#ifdef __APPLE__
	fails=$((fails + 1))
#endif
else
	pid=$(pgrep -f ${sentinel})
	if [ "$pid" = "$chpid" ]; then
		echo "ok - $name"
	else
		echo "not ok - $name"
#ifdef __APPLE__
		fails=$((fails + 1))
#endif
	fi
fi
kill $chpid
#ifdef __APPLE__
exit $fails
#endif
