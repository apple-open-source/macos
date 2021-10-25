#!/bin/sh

# Regression test for 70075417
function regression_70075417()
{
	echo "Verifying that touch(1)ing an existing file sets its modification time to be later than its creation time."

	file_name=`mktemp /tmp/XXXXXX`
	file_ctime=`/usr/bin/stat -f %Fc ${file_name}`

	# touching an existing file truncates the nanosecond component (last 3
	# digits). we can remove the sleep after rdar://32722408 is resolved.
	sleep 1

	/usr/bin/touch ${file_name}
	file_mtime=`/usr/bin/stat -f %Fm ${file_name}`

	if (( $(echo "${file_mtime} <= ${file_ctime}" | bc -l) )); then
		echo "file's mod time ($file_mtime) should be later than the file's creation time ($file_ctime)."
		exit 1
	fi
}

set -eu -o pipefail

regression_70075417

exit 0
