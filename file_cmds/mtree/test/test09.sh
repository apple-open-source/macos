#!/bin/bash
#
#  test09.sh
#  file_cmds
#
#  Created by Thomas Gallagher on 12/16/24.
#  Copyright 2024 Apple Inc. All rights reserved.

# Tests for the 'purgeable' keyword

set -ex

if ! which -s apfsctl; then
    echo 'apfsctl is not available on this system, skipping.'
    exit 0
fi

TMP=/tmp/mtree.$$
rm -rf ${TMP}
mkdir -p ${TMP}/data

Cleanup()
{
	local last_command="${BASH_COMMAND}" return_code=$?
	trap - EXIT
	if [ ${return_code} -ne 0 ]; then
		echo "command \"${last_command}\" exited with code ${return_code}"
	fi
	rm -rf ${TMP}
	exit ${return_code}
}
trap Cleanup INT TERM EXIT

# Test all combinations of flags, one out of two being a fault
purgencies="-low -med -high -su"
types="-photo -music -mail -document -data -podcast -video -movie -messages -books -su"
i=0
for p in $purgencies; do
	for t in $types; do
		if [ $(( ${i} % 2 )) -eq 0 ]; then
			f="-fault"
		else
			f=""
		fi
		((i++))
		file_name=${TMP}/data/${p}_${t}_${f}
		touch ${file_name}
		apfsctl purgeable -m ${p} ${t} ${f} ${file_name}
	done
done

# Create a spec with purgeable and verify it
mtree -c -p ${TMP}/data/ -k 'purgeable' 2>${TMP}/err 1>${TMP}/spec
mtree -p ${TMP}/data/ -f ${TMP}/spec

# Create another spec and compare them, it should succeed
mtree -c -p ${TMP}/data/ -k 'purgeable' 2>${TMP}/err 1>${TMP}/another_spec
mtree -f ${TMP}/spec -f ${TMP}/another_spec

# Change one value on disk and check that the verification fails
apfsctl purgeable -m -high "${TMP}/data/-low_-photo_-fault"
expected_output='-low_-photo_-fault changed
	purgeable expected 769 found 2048 for file -low_-photo_-fault'
output=$(mtree -p ${TMP}/data/ -f ${TMP}/spec) || return_code=$?
if [[ "$expected_output" != "$output" ]]; then
	echo "incorrect mtree compare output, here's the 'diff expected_output actual_output'"
	diff <(echo $expected_output) <(echo $output)
	exit 1
fi
if [ ${return_code} -ne 2 ]; then
	echo "verifying the spec should have returned 2, returned ${return_code} instead"
	exit 1
fi

# Generate a new spec and compare it to the initial one, it should fail
mtree -c -p ${TMP}/data/ -k 'purgeable' 2>${TMP}/err 1>${TMP}/new_spec
expected_output='		-low_-photo_-fault file purgeable=769
		-low_-photo_-fault file purgeable=2048'
output=$(mtree -f ${TMP}/spec -f ${TMP}/new_spec) || return_code=$?
if [[ "$expected_output" != "$output" ]]; then
	echo "incorrect mtree compare output, here's the 'diff expected_output actual_output'"
	diff <(echo $expected_output) <(echo $output)
	exit 1
fi
if [ ${return_code} -ne 2 ]; then
	echo "comparing specs should have returned 2, returned ${return_code} instead"
	exit 1
fi

# Check that an internal flag is not encoded in the spec by creating two files and checking that they have the same purgeable flags
if mount | grep mobile; then	# this checks that the system supports eAPFS
	mkdir -p ${TMP}/umove ${TMP}/regular
	touch ${TMP}/umove/file ${TMP}/regular/file
	apfsctl purgeable -m -umove ${TMP}/umove/file	# this one will have the APFS_PURGEABLE_LOW_URGENCY as well as APFS_PURGEABLE_UNTIL_MOVED
	apfsctl purgeable -m -low ${TMP}/regular/file	# compare with this one who will only have APFS_PURGEABLE_LOW_URGENCY
	mtree -c -p ${TMP}/umove/ -k 'purgeable' 2>${TMP}/err 1>${TMP}/spec_1
	mtree -c -p ${TMP}/regular/ -k 'purgeable' 2>${TMP}/err 1>${TMP}/spec_2
	mtree -f ${TMP}/spec_1 -f ${TMP}/spec_2
fi
