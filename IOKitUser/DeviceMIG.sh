# This script is run from the shell script build phases in the DeviceMIG target

if [ install == "$1" ]
then
    if [ $SCRIPT_INPUT_FILE_0 -nt $SCRIPT_OUTPUT_FILE_0 ]
    then
		echo Creating 32 bit Mig header for backward compatibility
		cat > $SCRIPT_OUTPUT_FILE_0 <<- EOI_iokitmig
			#if !defined(__LP64__)

			`cat $SCRIPT_INPUT_FILE_0`

			#endif /* !__LP64__ */
		EOI_iokitmig
    fi
    exit
fi

# This script generates the device.defs mig interface for the IOKit.framework to the kernel
runMig()
{
    local input=$1 head=$2 user=$3; shift 3

    cmd="/usr/bin/mig $@ -server /dev/null -header $head -user $user $input"
    echo $cmd
    eval $cmd
}

# which input files is newest.
if [ $SCRIPT_INPUT_FILE_0 -nt $SCRIPT_INPUT_FILE_1 ]
then
    testFile=$SCRIPT_INPUT_FILE_0
else
    testFile=$SCRIPT_INPUT_FILE_1
fi

if [ $testFile -nt $SCRIPT_OUTPUT_FILE_0 -o $testFile -nt $SCRIPT_OUTPUT_FILE_1 \
  -o $testFile -nt $SCRIPT_OUTPUT_FILE_2 -o $testFile -nt $SCRIPT_OUTPUT_FILE_3 ]
then
    runMig $SCRIPT_INPUT_FILE_0 $SCRIPT_OUTPUT_FILE_0 $SCRIPT_OUTPUT_FILE_1 $OTHER_CFLAGS

    OTHER_CFLAGS="$OTHER_CFLAGS -D__LP64__"
    runMig $SCRIPT_INPUT_FILE_0 $SCRIPT_OUTPUT_FILE_2 $SCRIPT_OUTPUT_FILE_3 $OTHER_CFLAGS
fi
