#!/bin/bash
#
# Script to test srm performance
#
# (Note: we use bash since it honors 'echo -n', unlike sh)

# ----------------------------------------------------------------------

# Location of standard srm command (as a baseline control)
STDSRM=/usr/bin/srm

# Location of srm command we are testing
BASEDIR=`pwd`
TSTSRM="${BASEDIR}/srm"

# Location of temporary directory (warning: this directory will be wiped!)
TMPDIR="${BASEDIR}/temp"

# Strings to identify hardware & software (note: this is OS-specific)
SW_VERS="`sw_vers -productVersion` (`sw_vers -buildVersion`)"
HW_VERS="`system_profiler SPHardwareDataType | grep Name | sed 's/^[ ]*//'`"

# ----------------------------------------------------------------------
# Subroutines

TAB="	"
csh_time () {
/bin/csh -sf << EOF
  time $* >& /dev/null
EOF
}

createLargeFiles () {
	if [ ! -d "${TMPDIR}" ] ; then mkdir -p "${TMPDIR}" ; fi
	if [ ! -f "${TMPDIR}/284mb" ] ; then
	    # Make a 284MB file
	    /bin/dd if=/dev/zero of="${TMPDIR}/284mb" bs=1k count=290816
	    sync
	    echo "Created ${TMPDIR}/284mb"
	    ls -la "${TMPDIR}/284mb"
    	fi
   	if [ ! -f "${TMPDIR}/100mb" ] ; then
	    # Make a 100MB file
	    /bin/dd if=/dev/zero of="${TMPDIR}/100mb" bs=1k count=102400
	    sync
	    echo "Created ${TMPDIR}/100mb"
	    ls -la "${TMPDIR}/100mb"
    	fi
}

createSmallFiles () {
	if [ ! -d "${TMPDIR}" ] ; then mkdir -p "${TMPDIR}" ; fi
	if [ ! -d "${TMPDIR}/smallfiles" ] ; then
		# Create a directory containing many small files
		# (in this case, a copy of /usr/include/sys)
		/bin/cp -r /usr/include/sys "${TMPDIR}/smallfiles"
		sync
		echo "Created ${TMPDIR}/smallfiles"
		du -h "${TMPDIR}/smallfiles"
	fi
}

doForN () {
	# $1 = command to execute
	# $2 = string to print on first iteration
	# $3 = number of times to execute command
	i=1
	q=${3-1}
	echo -n "$2"
	
	while [ $i -le $q ] ; do
		echo -n "${TAB}"
		if [ $i -gt 1 ] ; then echo -n "${TAB}${TAB}" ; fi
		csh_time "$1"
		i=$((i+1))
		if [ ! -d "${TMPDIR}/smallfiles" ] ; then
			# srm incorrectly renamed the folder [4498712]
			createSmallFiles
		fi
	done
}

doTest () {
	# $1 = command-line options
	# $2 = number of times to execute command
	doForN "${STDSRM}""$1" "baseline time${TAB}" "$2"
	doForN "${TSTSRM}"' --bsize=4096 '"$1" "buffsize=4096${TAB}" "$2"
	doForN "${TSTSRM}"' --bsize=65536 '"$1" "buffsize=65536${TAB}" "$2"
	doForN "${TSTSRM}"' --bsize=131072 '"$1" "buffsize=131072${TAB}" "$2"
	doForN "${TSTSRM}"' --bsize=1048576 '"$1" "buffsize=1048576" "$2"
	doForN "${TSTSRM}"' --bsize=4194304 '"$1" "buffsize=4194304" "$2"
	doForN "${TSTSRM}"' --bsize=8388608 '"$1" "buffsize=8388608" "$2"
}

# ----------------------------------------------------------------------
# Begin script

echo "`date`: ${HW_VERS}"
echo "`date`: Current system is ${SW_VERS}"
echo "`date`: Current directory is ${BASEDIR}"
echo "`date`: Starting tests"

if [ -d "${TMPDIR}" ] ; then rm -rf "${TMPDIR}" ; fi
createLargeFiles
createSmallFiles

echo
echo "### 1-pass tests ###"
echo
echo "file: ${TMPDIR}/284mb"
doTest ' -s -n '"${TMPDIR}/284mb" 3
echo
echo "folder: ${TMPDIR}/smallfiles"
doTest ' -s -r -f -n '"${TMPDIR}/smallfiles" 3

echo
echo "### 7-pass tests ###"
echo
echo "file: ${TMPDIR}/284mb"
doTest ' -m -n '"${TMPDIR}/284mb" 3
echo
echo "folder: ${TMPDIR}/smallfiles"
doTest ' -m -r -f -n '"${TMPDIR}/smallfiles" 3

echo
echo "### 35-pass tests ###"
echo
echo "file: ${TMPDIR}/100mb"
doTest ' -n '"${TMPDIR}/100mb" 3
echo
echo "folder: ${TMPDIR}/smallfiles"
doTest ' -r -f -n '"${TMPDIR}/smallfiles" 3

if [ -d "${TMPDIR}" ] ; then rm -rf "${TMPDIR}" ; fi

echo
echo "`date`: Completed tests"

exit 0
