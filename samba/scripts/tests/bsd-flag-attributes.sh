#! /bin/bash
# Copyright (C) 2006-2007 Apple Inc. All rights reserved.

# Test that we are propagating BSD flags correctly. This test runs on the
# local machine and relies on the [homes] share getting to the home directory
# of the user who is running the test.

SCRIPTBASE=${SCRIPTBASE:-$(cd $(dirname $0)/.. && pwd )}
.  $SCRIPTBASE/common.sh || exit 2

if [ $# -ne 1 ]; then
cat <<EOF
Usage: bsd-flag-attributes.sh PASSWORD
EOF
exit 1;
fi

SERVER=localhost
SHARE=homes
USERNAME=$LOGNAME
PASSWORD=${1:-local}

TESTDIR="$(basename $0)"
TMPFILE=/tmp/$(basename $0).$$

ASROOT=sudo

# NOTE: we do not test the simmutable flag because we need to boot to
# single user mode to clear it.
FLAGS="dump archived hidden uimmutable"

failed=0
failtest()
{
    failed=`expr $failed + 1`
}

setup_failed()
{
    rm -rf $TESTDIR
    echo failed to set up test data
    testok $0 1
}

create_test_files()
{
    mkdir -p $HOME/$TESTDIR || setup_failed

    for f in $FLAGS regular ; do
	touch $HOME/$TESTDIR/$f
	chmod 777 $HOME/$TESTDIR/$f
    done
}

remove_test_files()
{
    clear_bsd_flags
    rm -rf $HOME/$TESTDIR
    rm -f $TMPFILE
}


set_bsd_flags()
{
    for f in $FLAGS ; do
	$ASROOT chflags $f $HOME/$TESTDIR/$f
    done
}

clear_bsd_flags()
{
    for f in $FLAGS ; do
	$ASROOT chflags no$f $HOME/$TESTDIR/$f
    done
}

smbclient_listing()
{
    smbclient -g -N  -U"$LOGNAME"%"$PASSWORD" //$SERVER/$SHARE <<EOF
cd $TESTDIR
ls
EOF
}

checkflag()
{
    echo -n checking $1 flags are $2... ' '
    result=$( awk -v flag=$1 -v attr=$2 '
	$1==flag{
	    print $2
	}
	END { print NO }' < $3
    )

    echo $result
    [ $2 == "$result" ]
}

echo creating test files
create_test_files
register_cleanup_handler remove_test_files

echo setting BSD flag attributes
set_bsd_flags
ls -lO $HOME/$TESTDIR | indent

echo checking SMB view
smbclient_listing | indent | tee $TMPFILE

checkflag regular A $TMPFILE || failtest
checkflag archived 0  $TMPFILE || failtest
checkflag dump A  $TMPFILE || failtest
checkflag hidden AH $TMPFILE || failtest
checkflag uimmutable AR $TMPFILE || failtest

echo clearing BSD flag attributes
clear_bsd_flags
ls -lO $HOME/$TESTDIR | indent

echo checking SMB view
smbclient_listing | indent | tee $TMPFILE

checkflag regular A $TMPFILE || failtest
checkflag archived A $TMPFILE || failtest
checkflag dump 0 $TMPFILE || failtest
checkflag hidden A $TMPFILE || failtest
checkflag uimmutable A $TMPFILE || failtest

testok $0 $failed
