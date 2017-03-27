#!/bin/bash

# Measures the number of fbt entry/return probes instrumnetable
# as well as the gap between the number of fbt entry and return probes,
# both as a raw probe numbers and as a percentage

ENTRY_PROBES=`dtrace -xnolibs -ln 'fbt:::entry' | wc -l | awk '{print $1}'`
status=$?
if [ $status != 0 ]; then
	exit $status
fi
RETURN_PROBES=`dtrace -xnolibs -ln 'fbt:::return' | wc -l | awk '{print $1}'`
if [ $status != 0 ]; then
	exit $status
fi

# We always have more entry probes than return probes (or, in the best case, as
# many entry and return probes)

GAP=`expr $ENTRY_PROBES - $RETURN_PROBES`
PERCENT_RETURN=`awk "BEGIN { print $RETURN_PROBES / $ENTRY_PROBES * 100}"`


if [ -z $PERFDATA_FILE ]; then
	PERFDATA_FILE='/dev/fd/1'
fi

echo "{
	\"version\": \"1.0\",
	\"measurements\": {
		\"dtrace_fbt_have_return_percent\": {
			\"description\": \"Percentage of dtrace fbt probes that have both an entry and a return probe\",
			\"names\":  [\"dtrace_fbt_have_return_percent\"],
			\"units\": [\"percent\"],
			\"data\": [$PERCENT_RETURN]
		},
		\"dtrace_fbt_return_gap\": {
			\"description\": \"Number of fbt probes that have an entry probe but no return probe\",
			\"names\": [\"dtrace_fbt_return_gap\"],
			\"units\": [\"probes\"],
			\"data\": [$GAP]
		},
		\"dtrace_fbt_entry\": {
			\"description\": \"Number of fbt:::entry probes\",
			\"names\": [\"dtrace_fbt_entry\"],
			\"units\": [\"probes\"],
			\"data\": [$ENTRY_PROBES]
		},
		\"dtrace_fbt_return\": {
			\"description\": \"Number of fbt:::return probes\",
			\"names\": [\"dtrace_fbt_return\"],
			\"units\": [\"probes\"],
			\"data\": [$RETURN_PROBES]
		}
	}
}"> $PERFDATA_FILE

# Fail the test if the number of fbt probes without return fail under 80%
MIN_PERCENTAGE=80

awk "BEGIN { if ($PERCENT_RETURN < $MIN_PERCENTAGE) { exit(1) } }"

if [ $? -eq 1 ]; then
	exit $MIN_PERCENTAGE ;
fi
