#!/bin/bash -e


PIDs=()
error=false
currPid="$$"
defaultTestDirName="AppleFSTests"
testDir="${defaultTestDirName}-${currPid}"
mountPath="/tmp/mountPoint"
serverID="Server"
resultsDir="/tmp/results"
allTests=( "fsx" "fstorture" "filesbuster" "piston" )
filesystem="smb"
allFilesystems=( "smb" "smbfs" "nfs" "apfs" )
requiresSudo="false"
requiresUnmount="false"
remoteTestDir=""
user=""
pass=""
ip=""
share=""

cleanup()
{
    local lastCommand="${BASH_COMMAND}" returnCode=$?
    if [ ${returnCode} -ne 0 ]; then
        echo "Test Wrapper: command \"${lastCommand}\" exited with code ${returnCode}"
    fi
    echo "----------Test Wrapper cleaning up----------"

    # Don't delete the files if there was a test error, they may be interesting
    if ! "${error}" && [ -d "${remoteTestDir}" ]; then
        rm -rf "${remoteTestDir}"
    fi

    if mount -t "${filesystem}" | grep -q `basename "${mountPath}"` \
        && "${requiresUnmount}"; then

        echo "Unmounting ${mountPath}"
        if "${requiresSudo}"; then
            echo "May prompt for password"
            sudo umount -f "${mountPath}"
        else
            umount -f "${mountPath}"
        fi

        if [ -d "${mountPath}" ]; then
            rm -r "${mountPath}"
        fi

    fi

    echo "----------Test Wrapper cleanup complete----------"

}
trap cleanup EXIT

waitForPIDs ()
{

    echo "Waiting for PIDs"
    while [ "${#PIDs[@]}" -gt 0 ]; do

        for i in "${!PIDs[@]}"; do
            pid="${PIDs[${i}]}"
            if ! kill -0 ${pid} 2>/dev/null; then

                # Check the exit status, if it's nonzero print the errors
                if ! wait ${pid}; then
                    echo "${pid} FAILED"
                    error=true
                else
                    echo "${pid} FINISHED"
                fi

                # Remove it from the array
                unset PIDs[${i}]

            fi

        done

    # Wait 1 second before checking all PIDs again
    sleep 1
    done

}

usage() {

cat << EOF

TestWrapper.sh -u <shareURL> [options]
    Required Arguments:
        -u, --url <shareURL>        the url to mount.
                                    e.g. [smb://]test:pasword@192.168.1.1/Share
                                    e.g. /dev/disk2s5

        and/or

        -m, --mount <mountPath>     the path to the local mount point. The
                                    default is "${mountPath}". The url can be
                                    omitted if the target device is already
                                    mounted. Otherwise, this is where the url
                                    will get mounted
Options:
        Note all tests are run when none of these are set. If one or more of
        these flags are specified, only the tests set or not skipped are run.
        -x, --fsx                   runs fsx
        -t, --fstorture             runs fstorture
        -b, --filesbuster           runs FilesBuster
        -p, --piston                runs SMBClientEngine piston unit tests, note
                                    that if -u is not specified, then the script
                                    will prompt for a password for the share
        --no-fsx                    Skips fsx, overrides -x
        --no-fstorture              Skips fstorture, overrides -t
        --no-filesbuster            Skips FilesBuster, overrides -b
        --no-piston                 Skips piston unit tests, overrides -p

        These options do not modify which tests get run
        -d, --dir <testDir>         the name of the directory to place in the
                                    share. The default is "${testDir}"
        -s, --server <serverName>   the name of the server, just defines the
                                    name in the results directory. The default
                                    is "${serverID}"
        -r, --results <resultsPath> the path to the local directory to place
                                    results. The default is "${resultsDir}"
        -f, --fs <filesystem>       can be one of: smb[fs], nfs, apfs. This
                                    defines the default flags passed to each
                                    test. The default is "${filesystem}"

EOF

}

parseSMBURL() {

    # Use the input string if there is one
    if ! [ -z "${sharePath}" ]; then
        mountURL="${sharePath}"
        mountURL="${mountURL#smb://}"
    else
        # Parse mount for the rest of the info
        mountOutput=`mount -t smbfs | grep "${mountPath}"`
        mountURL=`echo "${mountOutput}" | egrep -o \
            '//.+(:.+)?@([0-9]{1,3}\.){3}[0-9]{1,3}/\S+'`
        mountURL="${mountURL#//}"
    fi

    user=`echo "${mountURL}" | egrep -o '^[^@:]+'`
    mountURL="${mountURL#${user}}"

    # Get the password if it is there
    firstChar="${mountURL:0:1}"
    mountURL="${mountURL#${firstChar}}"
    if [ "${firstChar}" == ":" ]; then
        pass=`echo "${mountURL}" | egrep -o '^[^@]+'`
        pass="${pass#:}"
        mountURL="${mountURL#${pass}@}"
    elif [ -z "${pass}" ]; then
        echo "No password specified, please enter password: "
        read -s pass
    fi

    ip=`echo "${mountURL}" | egrep -o '^[^/]+'`

    share="${mountURL#${ip}/}"

}

# Arg parsing
arguments=( "${@}" )
argLen="${#arguments[@]}"

runlist=()
ignorelist=()

# Arg parsing
for (( i = 0; i < argLen; i++ )); do
    arg="${arguments[${i}]}"
    nextInd=$(( ${i} + 1 ))
    next="${arguments[${nextInd}]}"

    case "${arg}" in
        -u | --url )
            sharePath=${next}
            i="${nextInd}"
            ;;
        -m | --mount )
            mountPath="${next}"
            i="${nextInd}"
            ;;
        -s | --server )
            serverID="${next}"
            i="${nextInd}"
            ;;
        -d | --dir )
            testDir="${next}"
            i="${nextInd}"
            ;;
        -r | --results )
            resultsDir="${next}"
            i="${nextInd}"
            ;;
        -f | --fs )
            filesystem="${next}"
            i="${nextInd}"
            ;;
        -x | --fsx )
            runlist+=( "fsx" )
            ;;
        -t | --fstorture )
            runlist+=( "fstorture" )
            ;;
        -b | --filesbuster )
            runlist+=( "filesbuster" )
            ;;
        -p | --piston )
            runlist+=( "piston" )
            ;;
        --no-fsx )
            ignorelist+=( "fsx" )
            ;;
        --no-fstorture )
            ignorelist+=( "fstorture" )
            ;;
        --no-filesbuster )
            ignorelist+=( "filesbuster" )
            ;;
        --no-piston )
            ignorelist+=( "piston" )
            ;;
        * )
            echo "Unrecognized arg ${arg}"
            usage
            exit 1
            ;;

    esac

done

# Argument sanity checking
if [ -z "${sharePath}" ] && ! mount | grep -q "${mountPath}"; then
    echo "Need a share url to mount or a path to an already mounted mount point"
    usage
    exit 1
fi

if [ "${filesystem}" == "smb" ]; then
    filesystem="smbfs"
elif ! echo "${allFilesystems[@]}" | grep -iq "${filesystem}"; then
    echo "Invalid file system ${filesystem}"
    exit 1
fi

# Make sure that there are no overlaps while also removing ignorelisted tests
for test in "${ignorelist[@]}"; do
    if echo "${runlist[@]}" | grep -q "${test}"; then
        echo "Test ${test} cannot be skipped and run in the same invocation"
        exit 1
    fi

    allTests=( ${allTests[@]//${test}} )
done

# If specific tests have been specified, then only run those. This is correct
# as runlist and ignorelist have been show to have no overlaps. Therefore,
# ignorelist is either empty or cannot skip runlisted tests. In the event
# that runlist is empty, all the ignorelisted tests have already been removed.
# If both are empty, allTests still has all the tests in it
if ! [ -z "${runlist[@]}" ]; then
    allTests=( "${runlist[@]}" )
fi

if [ "${#allTests[@]}" -eq 0 ]; then
    echo "No tests to run"
    exit 0
fi



remoteTestDir="${mountPath%/}/${testDir#/}"
mkdir -p "${mountPath}"

# General test setup
# Only attempt to mount when given a url and the mountPath isn't mounted
if ! [ -z "${sharePath}" ] && ! mount | grep "${mountPath}"; then

    case "${filesystem}" in
        "smbfs" )
            sharePath="smb://${sharePath#smb://}"
            ;;
        "nfs" )
            ;;
        "apfs" )
            echo "Mounting an apfs device identifier requires sudo"
            requiresSudo="true"
            ;;
        * )
            echo "Unkown fs ${filesystem}"
            ;;
    esac

    echo "Mounting using 'mount -t ${filesystem} -o nobrowse ${sharePath} ${mountPath}'"
    if "${requiresSudo}"; then
        sudo mount -t "${filesystem}" -o nobrowse "${sharePath}" "${mountPath}"

        echo "chown-ing mount point so tests can run as the user, may prompt again"
        sudo chown "${USER}" "${mountPath}"
    else
        mount -t "${filesystem}" -o nobrowse "${sharePath}" "${mountPath}"
    fi

    requiresUnmount="true"

else

    # Otherwise, need to check if the given mountPath is mounted as expected
    if ! mount -t "${filesystem}" | grep -q "${mountPath}"; then
        echo "Mount point ${mountPath} does not match fs ${filesystem}"
        exit 1
    fi

fi

testCount=0
resultsPath="${resultsDir%/}/${serverID#/}-Results"
while [ -e "${resultsPath}${testCount}" ]; do
    testCount=$(( testCount + 1 ))
done
resultsPath="${resultsPath}${testCount}"
echo "Creating local results directory at ${resultsPath}"
mkdir -p "${resultsPath}"

echo "Creating target folder ${remoteTestDir} on share"
if [ -d "${remoteTestDir}" ]; then
    echo "Found an existing folder, removing it"
    rm -r "${remoteTestDir}"
fi

mkdir "${remoteTestDir}"

# Create directories for Test tools
fsxDir="${remoteTestDir}/fsx"
fstortureDir="${remoteTestDir}/fstorture"
filesBusterDir="${remoteTestDir}/FilesBuster"
mkdir -p "${fsxDir}"
mkdir -p "${fstortureDir}/"{ShareFolder1,ShareFolder2}
mkdir -p "${filesBusterDir}"

# actually run the test here
for test in "${allTests[@]}"; do

    case "${test}" in

        "filesbuster" )
            # cd into the FilesBuster scripts folder
            currDir=`pwd`
            pushd /AppleInternal/Applications/FilesBuster.app/Contents/Resources/Scripts > /dev/null
            echo
            echo "Running FilesBuster"
            /AppleInternal/Applications/FilesBuster.app/Contents/MacOS/FilesBuster_BE \
                -s /AppleInternal/Applications/FilesBuster.app/Contents/Resources/Scripts/FBMainScript \
                -d EXPECT_DENY_MODE -d FILESYSTEM_SMB \
                -d PRIMARY_TARGET="${filesBusterDir}" &> "${resultsPath}/FilesBusterResults.txt" &
            FilesBuster=$!
            PIDs+=( "${FilesBuster}" )
            echo "FilesBuster pid ${FilesBuster}"
            popd > /dev/null
            ;;

        "fsx" )
            echo
            echo "Running FSX"
            /usr/local/bin/fsx -C -N 32768 "${fsxDir}/fsx.test" &> "${resultsPath}/fsxResults.txt" &
            fsx=$!
            PIDs+=( "${fsx}" )
            echo "fsx pid ${fsx}"
            ;;

        "fstorture" )
            echo
            echo "Running fstorture"
            /usr/local/bin/fstorture "${fstortureDir}/ShareFolder1" "${fstortureDir}/ShareFolder2" \
                2 nohardlinks no_perms no_stats windows_volume \
                -t 5m &> "${resultsPath}/fstortureResults.txt" &
            fstorture=$!
            PIDs+=( "${fstorture}" )
            echo "fstorture pid ${fstorture}"
            ;;

        "piston" )
            echo

            if [ "${filesystem}" != "smbfs" ]; then
                echo "Cannot run piston smb unit tests against non-smb mount"
                exit 1
            fi

            parseSMBURL

            echo "Running piston unit tests"
            /AppleInternal/CoreOS/tests/SMBClientEngine/piston_test \
                -u "${user}" -a "${ip}" -s "${share}" -p "${pass}" -j \
                -f "${resultsPath}" &> "${resultsPath}/pistonResults.txt" &
            piston=$!
            PIDs+=( "${piston}" )
            echo "piston unit tests pid ${piston}"
            ;;

        * )
            echo "How did you get here?"
            exit 1

    esac

done

echo
waitForPIDs

if echo "${allTests[@]}" | grep -q "filesbuster"; then
    # FilesBuster always returns 0 so parse the results
    fbResults=`tail -1 "${resultsPath}/FilesBusterResults.txt"`
    warnings=`echo "${fbResults}" | egrep -o "Warnings:\d+" | egrep -o "\d+"`
    failed=`echo "${fbResults}" | egrep -o "Failed:\d+" | egrep -o "\d+"`
    incomplete=`echo "${fbResults}" | egrep -o "Incompletes:\d+" | egrep -o "\d+"`

    if [ "${warnings}" -gt 0 ] || [ "${failed}" -gt 0 ] || [ "${incomplete}" -gt 0 ]; then
        echo "FilesBuster FAILED"
        error=true
    fi
fi

if ! "${error}"; then
    echo "Test passed, removing test directory"
    rm -rf "${remoteTestDir}"

    echo "All tests PASSED"
fi

echo
echo
echo "----------Test Output----------"
case "${test}" in
	"filesbuster" )
        cat "${resultsPath}/FilesBusterResults.txt"
		;;

	"fsx" )
        cat "${resultsPath}/fsxResults.txt"
		;;

	"fstorture" )
        cat "${resultsPath}/fstortureResults.txt"
		;;

	"piston" )
        cat "${resultsPath}/pistonResults.txt"
		;;

	* )
		echo "How did you get here?"
		exit 1
esac




