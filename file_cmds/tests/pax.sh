#!/bin/sh

function mod_time_preserve()
{
	echo "Verifying that pax(1) preserves modification time with nanosecond resolution."

	src_dir_name=`mktemp -d /tmp/XXXXXX`
	src_file_name=`mktemp ${src_dir_name}/XXXXXX`
	src_dir_mtime=`/usr/bin/stat -f %Fm ${src_dir_name}`
	src_file_mtime=`/usr/bin/stat -f %Fm ${src_file_name}`

	tgt_dir_name=`mktemp -d /tmp/XXXXXX`
	tgt_dir_mtime1=`/usr/bin/stat -f %Fm ${tgt_dir_name}`
	tgt_file_name=${tgt_dir_name}/$(basename ${src_file_name})

	cd ${src_dir_name}
	/bin/pax -rw . ${tgt_dir_name}
	tgt_dir_mtime2=`/usr/bin/stat -f %Fm ${tgt_dir_name}`
	tgt_file_mtime=`/usr/bin/stat -f %Fm ${tgt_file_name}`

	if (( $(echo "${tgt_dir_mtime1} == ${tgt_dir_mtime2}" | bc -l) )); then
		echo "pax(1) did not re-set dir mod time."
		exit 1
	fi
	if (( $(echo "${src_dir_mtime} != ${tgt_dir_mtime2}" | bc -l) )); then
		echo "pax(1) failed to preserve dir mod time: (${src_dir_mtime} != ${tgt_dir_mtime2})."
		exit 1
	fi
	if (( $(echo "${src_file_mtime} != ${tgt_file_mtime}" | bc -l) )); then
		echo "pax(1) failed to preserve file mod time: (${src_file_mtime} != ${tgt_file_mtime})."
		exit 1
	fi
}

function mod_time_set()
{
	echo "Verifying that pax(1) sets modification time with nanosecond resolution."

	# modification time can legitimately have 000 as the ns component, with 1/1000 chance.
	# trying 5 times pretty much guarantees no false positives
	num_tries=5
	src_dir_name=`mktemp -d /tmp/XXXXXX`
	src_file_name=`mktemp ${src_dir_name}/XXXXXX`

	tgt_dir_name=`mktemp -d /tmp/XXXXXX`
	tgt_file_name=${tgt_dir_name}/$(basename ${src_file_name})

	cd ${src_dir_name}
	for i in $(seq 1 ${num_tries}); do
		/bin/pax -rw -p m . ${tgt_dir_name}
		tgt_file_mtime=`/usr/bin/stat -f %Fm ${tgt_file_name}`
		nanoseconds=`echo -n ${tgt_file_mtime} | tail -c 3`
		# remove leading zeros
		nanoseconds=$(sed 's/^0*//'<<< ${nanoseconds})
		if [[ ${nanoseconds} -ne 0 ]]; then
			return 0
		fi
		rm -f ${tgt_file_name}
	done

	echo "pax(1) does not set modification time with nanosecond resolution."
	exit 1
}

# Regression test for 72740362
function regression_72740362()
{
	mod_time_preserve
	mod_time_set
}

set -eu -o pipefail

regression_72740362

exit 0
