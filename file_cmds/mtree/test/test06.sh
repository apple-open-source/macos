#!/bin/bash
#
#  test06.sh
#  file_cmds
#
#  Created by Thomas Gallagher on 12/20/23.
#  Copyright © 2022 Apple Inc. All rights reserved.

set -ex

TMP=/tmp/mtree.$$
DISK_IMAGE_APFS=${TMP}/disk_image_apfs.dmg

rm -rf ${TMP}
mkdir -p ${TMP}/data

# Check that we support content protection
if ! statfs ${TMP} | grep -q "MNT_CPROTECT"; then
    echo "This test must be run on a system that supports content protection, skipping."
    exit 0
fi

if ! which -s diskimagetool; then
    echo 'diskimagetool is not available on this system, skipping.'
    exit 0
fi

if ! which -s diskutil; then
    echo 'diskutil is not available on this system, skipping.'
    exit 0
fi

# This is done because this script can be run on macOS & iOS and the two platforms mount disks at different places
mountPointPrefix="/Volumes"
if ! [ -d "${mountPointPrefix}" ]; then
    mountPointPrefix="/private/var/mnt"
fi

Cleanup()
{
	local last_command="${BASH_COMMAND}" return_code=$?
	trap - EXIT
	if [ ${return_code} -ne 0 ]; then
		echo "command \"${last_command}\" exited with code ${return_code}"
	fi
	if [ -n "${NO_CP_APFS_VOL_PATH}" ]; then
		diskutil unmount "${NO_CP_APFS_VOL_PATH}"
		rm -rf "${NO_CP_APFS_VOL_PATH}"
	fi
	if [ -n "${NO_CP_APFS_DISK}" ]; then
		diskutil eject ${NO_CP_APFS_DISK}
	fi
	rm -rf ${TMP}
	exit $return_code
}
trap Cleanup INT TERM EXIT

# Generate data
fileIntegrity -p ${TMP}/data -k A -k B -k C -n 3 -c
touch ${TMP}/data/defaultFile
mkdir -p ${TMP}/data/defaultDir ${TMP}/data/classA_dir ${TMP}/data/classB_dir ${TMP}/data/classC_dir
setclass ${TMP}/data/classA_dir A
setclass ${TMP}/data/classB_dir B
setclass ${TMP}/data/classC_dir C

# Create an APFS disk image and attach it, calls to retrieve protection class attrs will succeed but always return the default protection class.
# This is expected behaviour on an APFS volume that doesn't support CPROTECT
diskimagetool create -f raw --srcfolder ${TMP}/data --volname mtree_test_apfs ${DISK_IMAGE_APFS}
attach_output=$(diskutil image attach ${DISK_IMAGE_APFS})
NO_CP_APFS_DISK=$(echo "${attach_output}" | egrep -m1 -o "/dev/disk[0-9]+(s[0-9]+)?(s[0-9]+)?")
NO_CP_APFS_VOL_PATH="${mountPointPrefix}/mtree_test_apfs"

# Check that we can create and verify a spec with protectionclass
mtree -c -p ${TMP}/data/ -k 'protectionclass' 2>${TMP}/err 1> ${TMP}/spec
mtree -p ${TMP}/data/ -f ${TMP}/spec

# Change a protection class and check that it's detected
setclass ${TMP}/data/fileIntegrity/file_001 B
return_code=0
mtree -p ${TMP}/data/ -f ${TMP}/spec || return_code=$?
if [ ${return_code} -ne 2 ]; then
    echo "change in protectionclass between disk and spec not detected, should have returned 2, instead returned ${return_code}"
    exit 1
fi
# Check that it's also detected when doing -f -f
mtree -c -p ${TMP}/data/ -k 'protectionclass' 2>${TMP}/err 1> ${TMP}/spec_after_change
return_code=0
mtree -f ${TMP}/spec -f ${TMP}/spec_after_change || return_code=$?
if [ ${return_code} -ne 2 ]; then
    echo "change in protectionclass between two spec files not detected, should have returned 2, instead returned ${return_code}"
    exit 1
fi
setclass ${TMP}/data/fileIntegrity/file_001 A

# Create a spec from the APFS volume that doesn't support CPROTECT, all protection_classes should be equal to 3
mtree -c -p "${NO_CP_APFS_VOL_PATH}" -k 'protectionclass' 2>${TMP}/err 1> ${TMP}/spec_non_supported_apfs
# Verifying the spec against the initial data should detect the mismatch
return_code=0
mtree -p ${TMP}/data/ -f ${TMP}/spec_non_supported_apfs || return_code=$?
if [ ${return_code} -ne 2 ]; then
    echo "change in protectionclass on NO_CP_APFS volume not detected, should have returned 2, instead returned ${return_code}"
    exit 1
fi
# Comparing both specs should also detect the mismatch
return_code=0
mtree -f ${TMP}/spec -f ${TMP}/spec_non_supported_apfs || return_code=$?
if [ ${return_code} -ne 2 ]; then
    echo "change in protectionclass between disk and NO_CP_APFS volume spec files not detected, should have returned 2, instead returned ${return_code}"
    exit 1
fi

# Check that verifying the created spec against a non supported volume fails
return_code=0
mtree -p "${NO_CP_APFS_VOL_PATH}" -f ${TMP}/spec || return_code=$?
# For an non supporting APFS volume, 2 is returned indicating that the specs don't match
if [ ${return_code} -ne 2 ]; then
    echo "verifying spec against path on NO_CP_APFS volume should have returned 2, instead returned ${return_code}"
    exit 1
fi

# Generate another spec with the keyword and compare both specs, it should succeed
mtree -c -p ${TMP}/data/ -k 'protectionclass' 2>${TMP}/err 1> ${TMP}/identical_spec
mtree -f ${TMP}/spec -f ${TMP}/identical_spec

# Generate another spec without the keyword and compare both specs, it should succeed
mtree -c -p ${TMP}/data/ 2>${TMP}/err 1> ${TMP}/spec_without
mtree -f ${TMP}/spec -f ${TMP}/spec_without
