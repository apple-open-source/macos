#!/bin/bash

# Measures the number of probes / distinct providers
# at boot


COUNT_PROBES_AWK='
BEGIN {
	nproviders = 0;
	nfasttrap_providers = 0;
	nprobes = 0;
}

/$1 > 0/
{
	if (match($2, /[0-9]+/)) {
		gsub(/[0-9]+/, "", $2);
		if (fasttrap_providers[$2] != 1) {
			fasttrap_providers[$2] = 1;
			nfasttrap_providers++;
		}
	}
	else {
		if (providers[$2] != 1) {
			providers[$2] = 1;
			nproviders++;
		}
	}
	nprobes++;
}

END {
	print nprobes;
	print nproviders;
	print nfasttrap_providers;
}
'

record_perf_probes()
{
	local nprobes=$1
	local nproviders=$2
	local nfasttrap_providers=$3
	cat >> $PERFDATA_FILE <<EOF
{
	"version": "1.0",
	"measurements": {
		"dtrace_probes": {
			"description": "Number of registered dtrace probes",
			"names": ["dtrace_probes"],
			"units": ["probes"],
			"data": [$nprobes]
		},
		"dtrace_providers": {
			"description": "Number of registered dtrace kernel providers",
			"names": ["dtrace_providers"],
			"units": ["providers"],
			"data": [$nproviders]
		},
		"dtrace_fasttrap_providers": {
			"description": "Number of registered dtrace fasttrap providers",
			"names": ["dtrace_fasttrap_providers"],
			"units": ["providers"],
			"data": [$nfasttrap_providers]
		}
	}
}
EOF
}

if [ -z $PERFDATA_FILE]; then
	PERFDATA_FILE='/dev/fd/1'
fi

values=$(dtrace -xnolibs -l | awk "$COUNT_PROBES_AWK")
status=${PIPESTATUS[0]}
if [ $status != 0 ]; then
	exit $status
fi
read nprobes nproviders nfasttrap_providers <<< $values
record_perf_probes $nprobes $nproviders $nfasttrap_providers
