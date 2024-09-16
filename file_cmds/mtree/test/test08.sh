#!/bin/bash
#
#  test08.sh
#  file_cmds
#
#  Created by Thomas Gallagher on 02/29/24.
#  Copyright © 2024 Apple Inc. All rights reserved.

set -ex

TMP=/tmp/mtree.$$
rm -rf ${TMP}
mkdir -p ${TMP}/data

touch ${TMP}/data/toto
xattr -w key value ${TMP}/data/toto
mtree -k 'xattrsdigest,size' -p ${TMP}/data -c > ${TMP}/a.spec
mtree -k 'xattrsdigest,size' -p ${TMP}/data -c > ${TMP}/a_2.spec
expected_output=''
return_code=0
output=$(mtree -f ${TMP}/a.spec -f ${TMP}/a_2.spec) || return_code=$?
if [[ "$expected_output" != "$output" ]]; then
	echo "incorrect mtree compare output, here's the 'diff expected_output actual_output'"
	diff <(echo $expected_output) <(echo $output)
	exit 1
fi
if [ ${return_code} -ne 0 ]; then
	echo "comparing specs should have returned 0, returned ${return_code} instead"
	exit 1
fi

xattr -w key othervalue ${TMP}/data/toto
mtree -k 'xattrsdigest,size' -p ${TMP}/data -c > ${TMP}/b.spec
expected_output='		toto file xattrsdigest=ea160af8961a5286039eff1c962c3e72048fd16e77e749e02ccad6a7c7a6b3c0.0
		toto file xattrsdigest=606fc9d68432ccb648a1546c59585e917897b02d86542635097fe9a2566a35a5.0'
output=$(mtree -f ${TMP}/a.spec -f ${TMP}/b.spec) || return_code=$?
if [[ "$expected_output" != "$output" ]]; then
	echo "incorrect mtree compare output, here's the 'diff expected_output actual_output'"
	diff <(echo $expected_output) <(echo $output)
	exit 1
fi
if [ ${return_code} -ne 2 ]; then
	echo "comparing specs should have returned 2, returned ${return_code} instead"
	exit 1
fi

xattr -d key ${TMP}/data/toto
mtree -k 'xattrsdigest,size' -p ${TMP}/data -c > ${TMP}/c.spec
expected_output='		toto file xattrsdigest=ea160af8961a5286039eff1c962c3e72048fd16e77e749e02ccad6a7c7a6b3c0.0
		toto file xattrsdigest=none.0'
output=$(mtree -f ${TMP}/a.spec -f ${TMP}/c.spec) || return_code=$?
if [[ "$expected_output" != "$output" ]]; then
	echo "incorrect mtree compare output, here's the 'diff expected_output actual_output'"
	diff <(echo $expected_output) <(echo $output)
	exit 1
fi
if [ ${return_code} -ne 2 ]; then
	echo "comparing specs should have returned 2, returned ${return_code} instead"
	exit 1
fi

xattr -w key value ${TMP}/data/toto
touch ${TMP}/data/tata
mtree -k 'xattrsdigest,size' -p ${TMP}/data -c > ${TMP}/d.spec
expected_output='	tata file size=0 xattrsdigest=none.0
		. dir size=96
		. dir size=128'
output=$(mtree -f ${TMP}/a.spec -f ${TMP}/d.spec) || return_code=$?
if [[ "$expected_output" != "$output" ]]; then
	echo "incorrect mtree compare output, here's the 'diff expected_output actual_output'"
	diff <(echo $expected_output) <(echo $output)
	exit 1
fi
if [ ${return_code} -ne 2 ]; then
	echo "comparing specs should have returned 2, returned ${return_code} instead"
	exit 1
fi


rm ${TMP}/data/toto
mtree -k 'xattrsdigest,size' -p ${TMP}/data -c > ${TMP}/e.spec
expected_output='	tata file size=0 xattrsdigest=none.0
toto file size=0 xattrsdigest=ea160af8961a5286039eff1c962c3e72048fd16e77e749e02ccad6a7c7a6b3c0.0'
output=$(mtree -f ${TMP}/a.spec -f ${TMP}/e.spec) || return_code=$?
if [[ "$expected_output" != "$output" ]]; then
	echo "incorrect mtree compare output, here's the 'diff expected_output actual_output'"
	diff <(echo $expected_output) <(echo $output)
	exit 1
fi
if [ ${return_code} -ne 2 ]; then
	echo "comparing specs should have returned 2, returned ${return_code} instead"
	exit 1
fi
