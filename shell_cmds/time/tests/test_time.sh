#!/bin/sh

set -o nounset

TIME="${TIME-/usr/bin/time}"
echo "SUITE: time(1)"
what $TIME
echo

EXIT=0

echo TEST: check real time

TIME_SLEEP=`$TIME 2>&1 sleep 1 | sed -n -E 's/[ ]+([0-9]+).*/\1/p'`
TIME_STATUS=$?

if [ "$TIME_STATUS" -ne "0" ]; then
	echo FAIL: time failed with "$TIME_STATUS"
	EXIT=1
fi

if [ "$TIME_SLEEP" -lt "0" ]; then
	echo FAIL: time mis-timed sleep
	EXIT=2
fi

MONOTONIC=`sysctl -n kern.monotonic.task_thread_counting`
if [ "$MONOTONIC" -ne "0" ]; then
	echo TEST: check instructions retired

	TIME_INSTRS=`$TIME -l 2>&1 sleep 1 | sed -E -n '/instructions/p'`
	if [ -z "$TIME_INSTRS" ]; then
		echo FAIL: time is not showing instructions retired
		EXIT=3
	fi
else
	echo SKIP: check instructions retired
fi

# NB: SIGINT and SIGQUIT work locally, but the automated test harnesses tries to
# handle those signals itself before the fork.

echo TEST: check child SIGUSR1

TIME_USR1=`$TIME 2>&1 sh -c 'kill -USR1 $$ && sleep 5 && true'`
TIME_STATUS=$?
if [ "$TIME_STATUS" -eq "0" ]; then
	echo FAIL: time should allow child to receive SIGUSR1
	EXIT=4
fi

echo TEST: check non-existent binary
TIME_NONEXIST=`$TIME 2>&1 ./this-wont-exist`
TIME_STATUS=$?
if [ "$TIME_STATUS" -ne "127" ]; then
	echo FAIL: time should error when measuring a non-existent command
	EXIT=5
fi

exit $EXIT
