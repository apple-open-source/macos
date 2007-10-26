#! /bin/bash
# Copyright (C) 2007 Apple Inc. All rights reserved.

# Basic script to run netbench

SCRIPTBASE=${SCRIPTBASE:-$(cd $(dirname $0)/.. && pwd )}
.  $SCRIPTBASE/common.sh || exit 2

if [ $# -lt 2 ]; then
cat <<EOF
Usage: smb-netbench.sh SERVER SHARE USERNAME PASSWORD
EOF
exit 1;
fi

SERVER=$1
SHARE=$2
USERNAME=${3:-local}
PASSWORD=${4:-local}

WORKLOAD=${WORKLOAD:-/usr/local/share/misc/client.txt}
SMBTORTURE=${SMBTORTURE:-/usr/local/bin/smbtorture4}

# Desktop only allows 10 client by default.
NUMPROCS=${NUMPROCS:-10}

DEBUGLEVEL=1

smbtorture() {
	vrun $SMBTORTURE \
	    --kerberos=no \
	    --debuglevel=$DEBUGLEVEL  --num-progs=$NUMPROCS \
	    --loadfile=$WORKLOAD \
	    -U "$USERNAME"%"$PASSWORD" \
	    "$@"
}

# Make sure we kill any strays. This is too general, but smbtorture is not a
# very good process controller.
register_cleanup_handler "killall -TERM smbtorture4 2>/dev/null"

smbtorture "//$SERVER/$SHARE" "BENCH-NBENCH"
testok $0 $?
