#!/bin/sh

export TEST_SSH_SSH=/usr/bin/ssh
export TEST_SSH_SSHD=/usr/sbin/sshd
export TEST_SSH_SSHAGENT=/usr/bin/ssh-agent
export TEST_SSH_SSHADD=/usr/bin/ssh-add
export TEST_SSH_SSHKEYGEN=/usr/bin/ssh-keygen
export TEST_SSH_SSHKEYSCAN=/usr/bin/ssh-keyscan
export TEST_SSH_SFTP=/usr/bin/sftp
export TEST_SSH_SFTPSERVER=/usr/libexec/sftp-server
export SUDO=/usr/bin/sudo

template="/usr/local/libexec/openssh/regression-tests"
workingdir="/private/tmp/ssh_regression_tests_$$"
mkdir "${workingdir}"

ditto "${template}" "${workingdir}"
cd "${workingdir}"

export TEST_SHELL=/bin/sh
export OBJ=`pwd`


#export TEST_SSH_TRACE=yes
#export TEST_SSH_QUIET=no

make

