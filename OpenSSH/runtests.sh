#!/bin/sh

# to run a single test file
test_file=""
use_current_dir=0
verbose=0

while getopts "h?cvt:" opt; do
		case "$opt" in
		h|\?)
				echo "$0: [-c] [-t <testfile>]"
				echo "   -c : run from current directory"
				echo "   -t <file> : run the specified testfile (without the .sh)"
				exit 0
				;;
		c)
				use_current_dir=1
				;;
		v)
				verbose=1
				;;
		t)
				test_file=$OPTARG
				;;
		esac
done

shift $((OPTIND-1))

[ "$1" = "--" ] && shift

if [ ${verbose} -ne 0 ]; then
		echo "verbose=$verbose, use_current_dir=$use_current_dir, test_file='$test_file', Leftovers: $@"
		exit 0
fi

export TEST_SSH_SSH=/usr/bin/ssh
export TEST_SSH_SSHD=/usr/sbin/sshd
export TEST_SSH_SSHAGENT=/usr/bin/ssh-agent
export TEST_SSH_SSHADD=/usr/bin/ssh-add
export TEST_SSH_SSHKEYGEN=/usr/bin/ssh-keygen
export TEST_SSH_SSHKEYSCAN=/usr/bin/ssh-keyscan
export TEST_SSH_SFTP=/usr/bin/sftp
export TEST_SSH_SFTPSERVER=/usr/libexec/sftp-server
export SUDO=/usr/bin/sudo
export SKIP_UNIT=YES

if [ ${use_current_dir} -eq 0 ] ; then
	template="/usr/local/libexec/openssh/regression-tests"
else
	template="."
fi

workingdir="/private/tmp/ssh_regression_tests_$$"
mkdir "${workingdir}"

ditto "${template}" "${workingdir}"
cd "${workingdir}"

export TEST_SHELL=/bin/sh
export OBJ=`pwd`

echo "running test from ${OBJ}"

# The OpenSSH test suite relies heavily on /var/run, which is group-writable on OS X.
# Temporarily disable group writing so that auth_secure_path() does not fail when running unit tests.
umask 022
trap "${SUDO} chmod g+w /private/var/run" EXIT SIGQUIT SIGTERM
${SUDO} chmod g-w /private/var/run
if [ "x${test_file}" == "x" ] ; then
		make .CURDIR=${OBJ} .OBJDIR=${OBJ}
else
		export TEST_SSH_TRACE=yes
		export TEST_SSH_QUIET=no
		export TEST_SSH_LOGFILE=/tmp/ssh-single-ssh-log.txt
		export TEST_REGRESS_LOGFILE=/tmp/ssh-single-regess-log.txt
		make .CURDIR=${OBJ} .OBJDIR=${OBJ} tests LTESTS=${test_file}
fi
