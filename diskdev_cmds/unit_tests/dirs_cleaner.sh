#!/bin/bash -e

#  dirs_cleaner.sh
#  diskdev_cmds
#
#  Created by joseph haddad on 18/10/2023.
#  Copyright © 2023 Apple Inc. All rights reserved.

set -o pipefail

gCleanup=true
gPath=""

tmp_paths=("/var/tmp" "/private/var/tmp")
if ! sw_vers | egrep -q "macOS"; then
    tmp_paths+=("/var/mobile/tmp" "/private/var/mobile/tmp")
fi

Cleanup()
{
    local lastCommand="${BASH_COMMAND}" returnCode=$?

    if [ ${returnCode} -ne 0 ]; then
        echo "command \"${lastCommand}\" exited with code ${returnCode}"

        if ! "${gCleanup}"; then
            echo "Cleanup disabled"
            exit "${returnCode}"
        fi
    fi

    if [ -d "${gPath}" ]; then
        rm -rf "${gPath}"
    fi
}
trap Cleanup INT TERM EXIT

Run_Test()
{
    args=$1

    for dir in "${tmp_paths[@]}"; do

        echo "Testing: /usr/libexec/dirs_cleaner ${args} ${dir}"

        # get tmp dir inode number
        inode=`fsutil -i "${dir}"`

        # Create a test directory in case it doesn't exist or was removed
        gPath="${dir}/test_dirs_cleaner"
        if [ ! -d "${gPath}" ]; then
            mkdir "${gPath}"
        fi

        # populate gPath
        apfsctl dataset -p "${gPath}" --single-dir > /dev/null 2>&1

        # Run dirs_cleaner
        /usr/libexec/dirs_cleaner "${args}" "${dir}"

        # Make sure we succesfully cleaned the directory we created in tmp dir
        if [ -d "${gPath}" ]; then
            echo "${gPath} still exists"
            exit 1
        fi

        # get tmp dir inode number after cleaning
        new_inode=`fsutil -i "${dir}"`

        # Make sure inode was changed
        if [ ${inode} -eq ${new_inode} ]; then
            echo "inode number should have changed"
            exit 1
        fi
    done
}


# Main
Run_Test ""
Run_Test "--init"

echo "Tests PASSED!"
