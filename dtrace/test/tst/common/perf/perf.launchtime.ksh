#!/bin/bash

# Measures DTrace launch time, both cold (first launch) and hot (with and
# without use of /usr/lib/dtrace)

# Measure the time to first launch of dtrace
FIRST_TIME=` /usr/bin/time dtrace -q -n 'BEGIN { exit(0) }' 2>&1`
status=$?
if [ $status != 0 ]; then
	exit $status
fi
FIRST_REALTIME=`echo ${FIRST_TIME} | awk '$2 == "real" {print $1}'`
FIRST_TOTALTIME=`echo ${FIRST_TIME} | awk '$2 == "real" {print $3 + $5}'`

# Measure the "hot" launch, when dtrace has already been launched once
# Measure it multiple times to avoid noise, and grab only the minimum value here as it is CPU-bound

HOT_REALTIME=3600
HOT_TOTALTIME=3600
ITERATIONS=5

for i in `seq 1 $ITERATIONS`
do

	LAUNCHTIME=` /usr/bin/time dtrace -q -n 'BEGIN { exit(0) }' 2>&1`
	status=$?
	if [ $status != 0 ]; then
		exit $status
	fi
	REALTIME=`echo ${LAUNCHTIME} | awk '$2 == "real" {print $1}'`
	TOTALTIME=`echo ${LAUNCHTIME} | awk '$2 == "real" {print $3 + $5}'`

	HOT_REALTIME=`awk "BEGIN { print ($REALTIME < $HOT_REALTIME ? $REALTIME : $HOT_REALTIME)}"`
	HOT_TOTALTIME=`awk "BEGIN { print ($TOTALTIME < $HOT_TOTALTIME ? $TOTALTIME : $HOT_TOTALTIME)}"`
done

# Measure the launch time using -xnolibs
NOLIBS_REALTIME=3600
NOLIBS_TOTALTIME=3600

for i in `seq 1 $ITERATIONS`
do
	LAUNCHTIME=` /usr/bin/time dtrace -xnolibs -q -n 'BEGIN { exit(0) }' 2>&1`
	status=$?
	if [ $status != 0 ]; then
		exit $status
	fi
	REALTIME=`echo ${LAUNCHTIME} | awk '$2 == "real" {print $1}'`
	TOTALTIME=`echo ${LAUNCHTIME} | awk '$2 == "real" {print $3 + $5}'`

	NOLIBS_REALTIME=`awk "BEGIN { print ($REALTIME < $NOLIBS_REALTIME ? $REALTIME : $NOLIBS_REALTIME)}"`
	NOLIBS_TOTALTIME=`awk "BEGIN { print ($TOTALTIME < $NOLIBS_TOTALTIME ? $TOTALTIME : $NOLIBS_TOTALTIME)}"`
done

if [ -z $PERFDATA_FILE ]; then
	PERFDATA_FILE='/dev/fd/1'
fi

echo '{
	"version": "1.0",
	"measurements": {' > $PERFDATA_FILE

# Only record the first launch if it is was an actual first launch (by checking
# that the first launch time is slow enough from the first one)
if [ `awk "BEGIN { print ($FIRST_REALTIME > $HOT_REALTIME * 3 ? 1: 0) }"` -eq 1 ]
then
echo "
		\"launch_first_real_time\" : {
			\"description\": \"DTrace first real launch time in seconds. Lower is better\",
			\"names\": [\"dtrace_first_launch_real_time\"],
			\"units\": [\"seconds\"],
			\"data\": [$FIRST_REALTIME]
		}," >> $PERFDATA_FILE

echo "
		\"launch_first_total_time\" : {
			\"description\": \"DTrace first total launch time in seconds. Lower is better\",
			\"names\": [\"dtrace_first_launch_total_time\"],
			\"units\": [\"seconds\"],
			\"data\": [$FIRST_TOTALTIME]
		}," >> $PERFDATA_FILE

fi
echo "
		\"launch_hot_real_time\" : {
			\"description\": \"DTrace hot real launch time in seconds. Lower is better\",
			\"names\": [\"dtrace_hot_launch_real_time\"],
			\"units\": [\"seconds\"],
			\"data\": [$HOT_REALTIME]
		}," >> $PERFDATA_FILE

echo "
		\"launch_hot_total_time\" : {
			\"description\": \"DTrace hot total launch time in seconds. Lower is better\",
			\"names\": [\"dtrace_hot_launch_total_time\"],
			\"units\": [\"seconds\"],
			\"data\": [$HOT_TOTALTIME]
		}," >> $PERFDATA_FILE

echo "
		\"launch_hot_nolibs_total_time\" : {
			\"description\": \"DTrace hot with -xnolibs total launch time in seconds. Lower is better\",
			\"names\": [\"dtrace_nolibs_hot_launch_total_time\"],
			\"units\": [\"seconds\"],
			\"data\": [$NOLIBS_TOTALTIME]
		}," >> $PERFDATA_FILE
echo "
		\"launch_hot_nolibs_real_time\" : {
			\"description\": \"DTrace hot with -xnolibs real launch time in seconds. Lower is better\",
			\"names\": [\"dtrace_nolibs_hot_launch_real_time\"],
			\"units\": [\"seconds\"],
			\"data\": [$NOLIBS_REALTIME]
		}" >> $PERFDATA_FILE

echo '
	}
}' >> $PERFDATA_FILE
