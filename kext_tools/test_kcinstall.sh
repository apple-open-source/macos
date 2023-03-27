#!/bin/sh

### Start of tester parameters ###

# iOS train/build for the personalization test(s)
IOS_TRAIN=Sydney
IOS_BUILD=Current$IOS_TRAIN

# watchOS train/build for the personalization test(s)
WATCHOS_TRAIN=Kincaid
WATCHOS_BUILD=Current$WATCHOS_TRAIN

# tvOS train/build for the personalization test(s)
TVOS_TRAIN=Paris
TVOS_BUILD=Current$TVOS_TRAIN

# The location ID of the embedded device for the end-to-end test
LOCATIONID=0x2130000

### End of tester parameters ###

KCINSTALL=./kcinstall
TCPRELAY=`which tcprelay || xcrun -sdk iphoneos.internal --find tcprelay`
SSH_OPTIONS="-o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o PreferredAuthentications=password -o PubkeyAuthentication=no -o LogLevel=quiet"
KCACHE_DST="/tmp/kci_manifest_root/System/Library/Caches/com.apple.kernelcaches/kernelcache"
PASSWORD=alpine
TCPRELAY_PID=
SSH_PORT=
COMMAND=

function log()
{
    echo "$1"
}

function file_content_to_stdout()
{
    local FILEPATH="$1"
    log "=== Start of Content ==="
    cat $FILEPATH
    log "=== End of Content ==="
}

# This tests d52g personalization
function test_kcinstall_kcache_personalize_d52g()
{
	local rc=0
	local CMD_OUTPUT_FILE=$(mktemp)
	local KCACHE=/SWE/Software/$IOS_TRAIN/Updates/$IOS_BUILD/Roots/KernelCacheBuilder_development/kernelcache.development.d52g
	
	echo "Starting d52g personalization test (iOS)"
	
	if [ ! -f "$KCACHE" ]; then
		echo "$KCACHE does not exist"
		return 1
	fi
	
	$KCINSTALL -k $KCACHE \
			   -g 10 \
			   -j 0x8101 \
		       -o 12345 \
			   -n \
			   > $CMD_OUTPUT_FILE
	rc=$?
	if [ $rc -ne 0 ]; then
		echo $KCINSTALL -k $KCACHE -g 10 -j 0x8101 -o 12345 -n
		file_content_to_stdout $CMD_OUTPUT_FILE
	fi
	rm -f $CMD_OUTPUT_FILE
	return $rc
}

# This tests j318 personalization
function test_kcinstall_kcache_personalize_j318()
{
	local rc=0
	local CMD_OUTPUT_FILE=$(mktemp)
	local KCACHE=/SWE/Software/$IOS_TRAIN/Updates/$IOS_BUILD/Roots/KernelCacheBuilder_development/kernelcache.development.j318
	
	echo "Starting j318 personalization test (ipadOS)"
	
	if [ ! -f "$KCACHE" ]; then
		echo "$KCACHE does not exist"
		return 1
	fi
	
	$KCINSTALL -k $KCACHE \
			   -g 14 \
			   -j 0x8027 \
			   -o 12345 \
			   -n \
			   > $CMD_OUTPUT_FILE
	rc=$?
	if [ $rc -ne 0 ]; then
		echo $KCINSTALL -k $KCACHE -g 14 -j 0x8027 -o 12345 -n
		file_content_to_stdout $CMD_OUTPUT_FILE
	fi
	rm -f $CMD_OUTPUT_FILE
	return $rc
}

# This tests d84 personalization
function test_kcinstall_kcache_personalize_d84()
{
	local rc=0
	local CMD_OUTPUT_FILE=$(mktemp)
	local KCACHE=/SWE/Software/$IOS_TRAIN/Updates/$IOS_BUILD/Roots/KernelCacheBuilder_development/kernelcache.development.d84
	
	echo "Starting d84 personalization test"
	
	if [ ! -f "$KCACHE" ]; then
		echo "$KCACHE does not exist"
		return 1
	fi
	
	$KCINSTALL -k $KCACHE \
			   -g 7 \
			   -j 0x8130 \
			   -o 12345 \
			   -a 0x0 \
			   -n \
			   > $CMD_OUTPUT_FILE
	rc=$?
	if [ $rc -ne 0 ]; then
		echo $KCINSTALL -k $KCACHE -g 7 -j 0x8130 -o 12345 -a 0x0 -n
		file_content_to_stdout $CMD_OUTPUT_FILE
	fi
	rm -f $CMD_OUTPUT_FILE
	return $rc
}

# This tests j305 personalization
function test_kcinstall_kcache_personalize_j305()
{
	local rc=0
	local CMD_OUTPUT_FILE=$(mktemp)
	local KCACHE=/SWE/Software/$TVOS_TRAIN/Updates/$TVOS_BUILD/Roots/KernelCacheBuilder_development/kernelcache.development.j305
	
	echo "Starting j305 personalization test (tvOS)"
	
	if [ ! -f "$KCACHE" ]; then
		echo "$KCACHE does not exist"
		return 1
	fi
	
	$KCINSTALL -k $KCACHE \
			   -g 8 \
			   -j 0x8020 \
			   -o 12345 \
			   -n \
			   > $CMD_OUTPUT_FILE
	rc=$?
	if [ $rc -ne 0 ]; then
		echo $KCINSTALL -k $KCACHE -g 8 -j 0x8020 -o 12345 -n
		file_content_to_stdout $CMD_OUTPUT_FILE
	fi
	rm -f $CMD_OUTPUT_FILE
	return $rc
}

# This tests n140b personalization
function test_kcinstall_kcache_personalize_n140b()
{
	local rc=0
	local CMD_OUTPUT_FILE=$(mktemp)
	local KCACHE=/SWE/Software/$WATCHOS_TRAIN/Updates/$WATCHOS_BUILD/Roots/KernelCacheBuilder_development/kernelcache.development.n140b
	
	echo "Starting n140b personalization test (watchOS)"
	
	if [ ! -f "$KCACHE" ]; then
		echo "$KCACHE does not exist"
		return 1
	fi
	
	$KCINSTALL -k $KCACHE \
			   -g 0x2a \
			   -j 0x8006 \
			   -o 12345 \
			   -n \
			   > $CMD_OUTPUT_FILE
	rc=$?
	if [ $rc -ne 0 ]; then
		echo $KCINSTALL -k $KCACHE -g 0x2a -j 0x8006 -o 12345 -n
		file_content_to_stdout $CMD_OUTPUT_FILE
	fi
	rm -f $CMD_OUTPUT_FILE
	return $rc
}

# Kills the TCP forwarder process
function kill_ssh_relay()
{
	if [[ $TCPRELAY_PID ]]; then
		kill $TCPRELAY_PID
		wait $TCPRELAY_PID 2>/dev/null
	fi
}

# Starts the TCP forwarder process
function start_ssh_relay()
{
	local TCPRELAY_OUTPUT=`mktemp -t tcprelay`
	$TCPRELAY --locationid $1 --dynamicports --autoexit ssh rsync > $TCPRELAY_OUTPUT 2>&1 &
	TCPRELAY_PID=$!

	while [[ `stat -f "%z" $TCPRELAY_OUTPUT` -eq 0 ]]; do
		sleep 1
	done

	SSH_PORT=`awk '{if (/127\.0\.0\.1.*ssh/) {for (x = 1; x <= NF; x++) {if ($x ~ /127\.0\.0\.1*/) {print $x; exit;}}}};' $TCPRELAY_OUTPUT | awk -F : '{print $2}'`
}

# Abstract: Runs the shell command in $COMMAND on the target device
#   and echoes the output
# Usage:
#   COMMAND="whoami"
#   result=`run_command_on_device`
#   echo $result => root
function run_command_on_device()
{
	OUTPUT=$(expect -c "
		set timeout 30
		set passcnt 0
		spawn /usr/bin/ssh -p $SSH_PORT $SSH_OPTIONS root@localhost \"echo __BEGIN_DCMD__ ; $COMMAND ; echo __END_DCMD__\"
		expect {
			\"__END_DCMD__\" {exit 0;}
			eof          {exit 1;}
			timeout      {exit 1;}
			\"assword:\" {if {\$passcnt == 1} {exit 1;} else {incr passcnt;send \"$PASSWORD\r\n\";exp_continue;}}
		}
	")
	OUTPUT=`echo $OUTPUT | sed -e 's|^.*__BEGIN_DCMD__||' -e 's|__END_DCMD__.*$||'`
	echo $OUTPUT
}

# Abstract: Echoes the target device model
# Usage:
#   result=`device_model`
#   echo $result => d52g
function device_model()
{
	COMMAND="sysctl -n hw.targettype"
	MODEL=`run_command_on_device`
	MODEL=`echo $MODEL | tr -d '\r' | awk '{print tolower($0)}' | xargs`
	echo $MODEL
}

# Abstract: Echoes the target device's running kernel UUID
# Usage:
#   result=`running_kernel_uuid`
#   echo $result => CE3AC535-157A-397F-80C1-B450ACC92BF3
function running_kernel_uuid()
{
	COMMAND="sysctl -n kern.uuid"
	LINE=`run_command_on_device`
	echo $LINE | tr -d '\r' | tr -d '\n' | xargs
}

# Abstract: Checks if a file exists on the target device
# Usage:
#   result=file_exists /path/to/file
function file_exists()
{
	COMMAND="if \[ -f $1 ]\; then echo 1 ; else echo 0 ; fi"
	OUTPUT=`run_command_on_device`
	OUTPUT=`echo $OUTPUT | tr -d '\r' | tr -d '\n' | xargs`
	echo $OUTPUT
}

function delete_file()
{
	RESULT=`file_exists $1`
	if [[ "$RESULT" == "1" ]]; then
		COMMAND="rm $1"
		run_command_on_device
	fi
}

# Abstract: Echoes a boot kernel collection's kernel UUID
# Usage:
#   result=`kc_kernel_uuid /path/to/boot.kc`
#   echo $result => CE3AC535-157A-397F-80C1-B450ACC92BF3
function kc_kernel_uuid()
{
	OUTPUT=`kmutil inspect -B $1 -V '' --show-kernel-uuid-only | grep com.apple.kernel | awk '{ print $4 }' | tr -d '()'`
	echo $OUTPUT | tr -d '\r' | tr -d '\n' | xargs
}

# Abstract: Echoes the target device's current build version
# Usage:
#   result=`device_build`
#   echo $result => 20A348
function device_build()
{
	COMMAND="sw_vers -buildVersion"
	OUTPUT=`run_command_on_device`
	OUTPUT=`echo $OUTPUT | tr -d '\r' | tr -d '\n' | xargs`
	echo $OUTPUT
}

# Abstract: Reboots the target device
# Usage:
#   reboot_device
function reboot_device()
{
	COMMAND="reboot"
	run_command_on_device
}

# Abstract: This test performs an actual end-to-end install on a physically
#   connected device. It installs the KASAN boot kernel collection of the
#   equivalent build.
function test_kcinstall_kcache_e2e()
{
	local VARIANT=kasan
	local CMD_OUTPUT_FILE=$(mktemp)
	local rc=0
	
	# Start TCP forwarder
	start_ssh_relay $LOCATIONID
	
	# Determine what variant to install (kasan or development)
	COMMAND="uname -a | grep -i kasan"
	UNAME_OUTPUT=`run_command_on_device`
	UNAME_OUTPUT=`echo $UNAME_OUTPUT | tr -d '\r' | tr -d '\n' | xargs`
	if [[ -n $UNAME_OUTPUT ]]; then
		VARIANT=development
	fi
	echo "installing $VARIANT variant"
	
	# Query the device build
	local RUNNING_BUILD=`device_build`
	if [[ -z $RUNNING_BUILD ]]; then
		echo "failed to query device build"
		return 1
	fi
	
	# Query the device train
	local RUNNING_TRAIN=`/usr/local/bin/xbs getTrainForBuild -quiet --embedded $RUNNING_BUILD`
	if [[ -z $RUNNING_TRAIN ]]; then
		echo "failed to query device train"
		return 1
	fi
	
	echo "Device under test is running $RUNNING_TRAIN$RUNNING_BUILD"

	# Remove existing KC
	delete_file $KCACHE_DST
	
	# Get the kernel UUID that we want to install
	MODEL=`device_model`
	echo "Device model is $MODEL"
	KCACHE=/SWE/Software/$RUNNING_TRAIN/Updates/$RUNNING_TRAIN$RUNNING_BUILD/Roots/KernelCacheBuilder_$VARIANT/kernelcache.$VARIANT.$MODEL
	KC_UUID=`kc_kernel_uuid $KCACHE`
	if [[ -z $KC_UUID ]]; then
		echo "failed to get UUID of $KCACHE"
		return 1
	fi
	
	# Get the kernel UUID that is currently running
	OLD_UUID=`running_kernel_uuid`
	if [[ -z $KC_UUID ]]; then
		echo "failed to find running $TEST_KEXT_BUNDLE"
		return 1
	fi
	
	# Make sure kernel is not already running
	if [[ "$KC_UUID" == "$OLD_UUID" ]]; then
		echo "kernel UUID already installed. Choose a build that is not already installed."
		return 1
	fi
	
	echo "installing $KCACHE"
	
	# Install the new kernel collection
	$KCINSTALL -k $KCACHE -l $LOCATIONID > $CMD_OUTPUT_FILE
	rc=$?
	if [ $rc -ne 0 ]; then
		file_content_to_stdout $CMD_OUTPUT_FILE
		return $rc
	fi
	
	# Ensure KC was copied successfully
	RESULT=`file_exists $KCACHE_DST`
	if [[ "$RESULT" == "0" ]]; then
		echo "failed to copy kernel collection"
		return 1
	fi
	
	# Reboot
	reboot_device
	echo "waiting 60s for device to reboot..."
	sleep 60

	# Get the running kernel UUID
	start_ssh_relay $LOCATIONID
	NEW_UUID=`running_kernel_uuid`
	kill_ssh_relay
	
	# Check equality
	if [[ "$NEW_UUID" != "$KC_UUID" ]]; then
		echo "failed to install"
		rc=1
	fi
	
	rm -f $CMD_OUTPUT_FILE
	return $rc
}

declare -a TestCases=(
    "test_kcinstall_kcache_personalize_d52g"
	#"test_kcinstall_kcache_personalize_d84"
	"test_kcinstall_kcache_personalize_j318"
	"test_kcinstall_kcache_personalize_n140b"
	"test_kcinstall_kcache_personalize_j305"
	#"test_kcinstall_kcache_e2e"
)

### main ###

test_status=0

echo "Run all test cases"
for test_case in "${TestCases[@]}"; do
	echo
	log "[TESTBEGIN] $test_case"
	eval $test_case
	rc=$?
	if [ $rc -ne 0 ]; then
		log "[TESTEND] $test_case failed: $rc"
		test_status=$rc
	else
		log "[TESTEND] $test_case passed"
	fi
done

exit $test_status
