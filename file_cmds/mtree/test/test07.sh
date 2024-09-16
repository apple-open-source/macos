#!/bin/bash
# 
#  test07.sh
#  file_cmds
#
#  Created by Thomas Gallagher on 12/20/23.
#  Copyright © 2022 Apple Inc. All rights reserved.

set -ex

TMP=/tmp/mtree.$$
DISK_IMAGE_EXFAT=${TMP}/disk_image_exfat.dmg

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

if [ $(id -u) -ne 0 ]; then
	echo "Needs to be run as root, failing"
	exit 1
fi

SW_VERS_OUTPUT=$(sw_vers)

Cleanup()
{
	local last_command="${BASH_COMMAND}" return_code=$?
	if [ ${return_code} -ne 0 ]; then
		echo "command \"${last_command}\" exited with code ${return_code}"
	fi
	if ! echo "${SW_VERS_OUTPUT}" | grep -q macOS; then
		defaults delete /Library/Preferences/SystemConfiguration/com.apple.DiskArbitration.diskarbitrationd DAAutoMountDisable
		pkill -1 diskarbitrationd
		datest --testDAIdle
	fi
	if [ -n "${NO_CP_EXFAT_VOL_PATH}" ]; then
		diskutil unmount "${NO_CP_EXFAT_VOL_PATH}"
		rm -rf "${NO_CP_EXFAT_VOL_PATH}"
	fi
	if [ -n "${NO_CP_EXFAT_DISK}" ]; then
		diskutil eject ${NO_CP_EXFAT_DISK}
	fi
}
trap Cleanup INT TERM EXIT

# Generate data
fileIntegrity -p ${TMP}/data -k A -k B -k C -n 3 -c
touch ${TMP}/data/defaultFile
mkdir -p ${TMP}/data/defaultDir ${TMP}/data/classA_dir ${TMP}/data/classB_dir ${TMP}/data/classC_dir
setclass ${TMP}/data/classA_dir A
setclass ${TMP}/data/classB_dir B
setclass ${TMP}/data/classC_dir C


if ! echo "${SW_VERS_OUTPUT}" | grep -q macOS; then
	# This is needed on iOS to enable auto-mount
	defaults write /Library/Preferences/SystemConfiguration/com.apple.DiskArbitration.diskarbitrationd DAAutoMountDisable -bool false
	pkill -1 diskarbitrationd
	datest --testDAIdle
fi


# Create a ExFAT disk image and attach it. Calls to retrieve protection class attrs will fail
diskimagetool create -s 50m --fs NONE ${DISK_IMAGE_EXFAT}
exfat_raw_disk=$(diskimagetool attach ${DISK_IMAGE_EXFAT})
diskutil eraseVolume ExFAT EXFAT ${exfat_raw_disk}
# Attaching and mounting an ExFAT disk on iOS is harder than on macOS
if ! echo "${SW_VERS_OUTPUT}" | grep -q macOS; then
	diskutil eject ${exfat_raw_disk}
	NO_CP_EXFAT_DISK=$(diskimagetool attach --external ${DISK_IMAGE_EXFAT})
	disk_without_dev=${NO_CP_EXFAT_DISK#/dev/}
	df_output=$(df -P "exfat://${disk_without_dev}/EXFAT")
	# awk prints from column 6 forward, as volume names may have spaces
	NO_CP_EXFAT_VOL_PATH=$( echo "${df_output}" | tail -n1 | awk 'sub(/^[[:space:]]*([^[:space:]]*[[:space:]]*){5}/,"")' )
else
	NO_CP_EXFAT_VOL_PATH="/Volumes/EXFAT"
fi
# Copy data to the ExFAT disk
cp -r ${TMP}/data/* "${NO_CP_EXFAT_VOL_PATH}"

# Generate a spec with protectionclass
mtree -c -p ${TMP}/data/ -k 'protectionclass' 2>${TMP}/err 1> ${TMP}/spec

# Check that we cannot create a spec if the volume doesn't support MNT_CPROTECT
# It should fail with ENOTSUP
return_code=0
mtree -c -p "${NO_CP_EXFAT_VOL_PATH}" -k 'protectionclass' || return_code=$?
if [ ${return_code} -ne 45 ]; then
	echo "attempt to create a protectionclass spec on ExFAT volume should have returned ENOTSUP (45), instead returned ${return_code}"
	exit 1
fi

# Check that verifying the created spec against a non supported volume fails
return_code=0
mtree -p "${NO_CP_EXFAT_VOL_PATH}" -f ${TMP}/spec || return_code=$?
# For an ExFAT volume, ENOTSUP is returned indicating that the spec cannot be verified at all
if [ ${return_code} -ne 45 ]; then
	echo "verifying spec against path on NO_CP_EXFAT volume should have returned ENOTSUP (45), instead returned ${return_code}"
	exit 1
fi
