#! /bin/bash
# Copyright (C) 2006-2007 Apple Inc. All rights reserved.

# smbtorture-suite.sh - run the tests from the Samba4 smbtorture suite that
# are expected to pass. This is equivalent to runing "make quick".

SCRIPTBASE=${SCRIPTBASE:-$(cd $(dirname $0)/.. && pwd )}
.  $SCRIPTBASE/common.sh || exit 2

if [ $# -lt 2 ]; then
cat <<EOF
Usage: smbtorture-suite.sh SERVER SHARE USERNAME PASSWORD
EOF
exit 1;
fi

SERVER=$1
SHARE=$2
USERNAME=${3:-local}
PASSWORD=${4:-local}

SMBTORTURE=${SMBTORTURE:-/usr/local/bin/smbtorture4}

failed=0

failtest() {
	failed=`expr $failed + 1`
}

# Filter subunit format output, and return status to tell the caller whether
# the set of tests passed or failed.
#
# See http://bazaar-vcs.org/Subunit/Specification for subunit output
# format description
subunit_filter()
{
    perl -e '
	my $testname = "";
	my $testcount = 0;
	my $successcount = 0;
	while (<>) {
	    if (m/test: (\w+)/) {
		$testcount++; $testname = $1;
	    }
	    $successcount++ if (m/success: \Q$testname\E/);
	    print;
	}
	exit ($testcount - $successcount);
	'
}

test_success_output()
{
	cat <<EOF
jpeach:~ jpeach$ /usr/local/bin/smbtorture4 --format=subunit -Ulocal%local //jpeach-mini.local/local  RAW-LOCK
Using seed 1176336980
test: LOCK
Testing RAW_LOCK_LOCKX
Trying 0xEEFFFFFF lock
Trying 0xEF000000 lock
Trying zero lock
Trying max lock
Trying 2^63
Trying 2^63 - 1
Trying max lock 2
Testing RAW_LOCK_LOCK
Trying 0/0 lock
Trying 0/1 lock
Trying 0xEEFFFFFF lock
Trying 0xEF000000 lock
Trying max lock
Trying wrong pid unlock
Testing high pid
High pid is not used on this server (correct)
Testing LOCKING_ANDX_CANCEL_LOCK
testing cancel by CANCEL_LOCK
testing cancel by unlock
testing cancel by close
create a new sessions
create new tree context
testing cancel by exit
testing cancel by ulogoff
testing cancel by tdis
Testing LOCK_NOT_GRANTED vs. FILE_LOCK_CONFLICT
testing with timeout = 0
testing with timeout > 0 (=1)
testing a conflict while a lock is pending
Testing LOCKING_ANDX_CHANGE_LOCKTYPE
success: LOCK
EOF
}

test_failure_output()
{
    cat <<EOF
jpeach:~ jpeach$ /usr/local/bin/smbtorture4 --format=subunit -Ulocal%local //jpeach-mini.local/local  BASE-LOCK
Using seed 1176336710
testsuite: LOCK
test: LOCK1
Testing lock timeout with timeout=14
server slept for 14 seconds for a 14 second timeout
success: LOCK1
test: LOCK2
Testing pid context
success: LOCK2
test: LOCK3
Testing 32 bit offset rangesEstablishing 10 locks
Testing 10 locks
Removing 10 locks
success: LOCK3
test: LOCK4
the same process cannot set overlapping write locks
the same process can set overlapping read locks
a different connection cannot set overlapping write locks
a different connection can set overlapping read locks
a different pid cannot set overlapping write locks
a different pid can set overlapping read locks
the same process can set the same read lock twice
the same process cannot set the same write lock twice
the same process cannot overlay a read lock with a write lock
the same process can overlay a write lock with a read lock
a different pid cannot overlay a write lock with a read lock
the same process cannot coalesce read locks
this server does strict write locking
this server does strict read locking
this server does do recursive read locking
** this server doesn't do recursive lock overlays
** the same process cannot remove a read lock using write locking
the same process can remove a write lock using read locking
** the same process doesn't remove the first lock first
the server doesn't have the NT byte range lock bug
error: LOCK4 [
Unknown error/failure
]
test: LOCK5
this server doesn't have the NT locking bug
the same process can overlay a write with a read lock
a different processs cannot get a read lock on the first process lock stack
the same processs on a different fnum cannot get a read lock
the same process can stack read locks
** the first unlock removes the READ lock
the same process can unlock the stack of 4 locks
the same process can count the lock stack
** a different processs cannot get a write lock on the unlocked stack
error: LOCK5 [
Unknown error/failure
]
test: LOCK6
Testing \lock6.txt
CHANGE_LOCKTYPE gave ERRDOS:ERRnoatomiclocks
CANCEL_LOCK gave ERRDOS:ERRcancelviolation
success: LOCK6
test: LOCK7
pid1 successfully locked range 130:4 for READ
pid1 successfully read the range 130:4
pid1 unable to write to the range 130:4, error was NT_STATUS_FILE_LOCK_CONFLICT
pid2 successfully read the range 130:4
pid2 unable to write to the range 130:4, error was NT_STATUS_FILE_LOCK_CONFLICT
pid1 successfully locked range 130:4 for WRITE
pid1 successfully read the range 130:4
pid1 successfully wrote to the range 130:4
pid2 unable to read the range 130:4, error was NT_STATUS_FILE_LOCK_CONFLICT
pid2 unable to write to the range 130:4, error was NT_STATUS_FILE_LOCK_CONFLICT
Testing truncate of locked file.
Truncated locked file.
success: LOCK7
EOF
}

smbtorture()
{
    local testname=$1

    echo Running smbtorture test $testname ...
    $SMBTORTURE //$SERVER/$SHARE \
	    -U$USERNAME%$PASSWORD \
	    --format=subunit \
	    --option=torture:samba3=yes \
	    -L $testname 2>&1 | subunit_filter

    #test_success_output | subunit_filter	
    #test_failure_output | subunit_filter	
}

while read t ; do
	[ -z "$t" ] && continue
	if smbtorture "$t" ; then
	    echo Finished smbtorture test $t: OK
	else
	    echo Finished smbtorture test $t: FAILED
	    failtest
	fi
done <<EOF
`$SCRIPTBASE/smbtorture.list`
EOF

testok $0 $failed

