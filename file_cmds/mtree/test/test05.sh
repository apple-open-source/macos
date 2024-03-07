#!/bin/sh

#  test05.sh
#  file_cmds
#
#  Created by Cary on 12/2/22.
#  Copyright © 2022 Apple Inc. All rights reserved.

if ! [ -x "$(command -v fileproviderctl_internal)" ]; then
  echo 'fileproviderctl is not available on this system, skipping tests.'
  exit 0
fi

TMP=/tmp/mtree.$$

rm -rf ${TMP}
mkdir -p ${TMP} ${TMP}/mr ${TMP}/mt


touch ${TMP}/mr/normal_file
fileproviderctl_internal fault create ${TMP}/mr/dataless_file ${TMP}/mr/normal_file

touch ${TMP}/mt/normal_file
fileproviderctl_internal fault create ${TMP}/mt/dataless_file ${TMP}/mt/normal_file

mtree -c -p ${TMP}/mr -k "dataless" 2>${TMP}/err 1> ${TMP}/_
mtree -c -p ${TMP}/mt -k "dataless" 2>${TMP}/err2 1> ${TMP}/_t
retval=$?
error="$(cat ${TMP}/err)"

# If stderr was not empty
if ! [ -z "$error" ] ; then
	echo "ERROR mtree output error for dataless keyword" 1>&2
	exit 1
fi

if [ $retval -ne 0 ] ; then
	echo "ERROR wrong exit code for dataless keyword" 1>&2
	exit 1
fi

if [ `wc -l < ${TMP}/_` -ne 13 ] ; then
	echo "ERROR wrong line count for dataless output"
	exit 1
fi

mtree -f ${TMP}/_ -p ${TMP}/mt 2>${TMP}/e 1> ${TMP}/final
mtree -f ${TMP}/_t -p ${TMP}/mr 2>${TMP}/e2 1> ${TMP}/final2

if [ `wc -l < ${TMP}/final` -ne 0 ] ; then
	echo "ERROR spec/path compare generated wrong output"
	exit 1
fi

if [ `wc -l < ${TMP}/final2` -ne 0 ] ; then
	echo "ERROR spec/path compare generated wrong output"
	exit 1
fi
