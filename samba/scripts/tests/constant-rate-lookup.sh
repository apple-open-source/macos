#! /bin/bash
# Copyright (C) 2006-2007 Apple Inc. All rights reserved.

# Basic script to run netbench APPLE-LOOKUP-RATE test. This doesn't really
# need to be a separate test script, but doing it whis wway makes quicklooks
# and verification easier.

SCRIPTBASE=${SCRIPTBASE:-$(cd $(dirname $0)/.. && pwd )}
.  $SCRIPTBASE/common.sh || exit 2

if [ $# -lt 2 ]; then
cat <<EOF
Usage: constant-rate-lookup.sh SERVER SHARE USERNAME PASSWORD
EOF
exit 1;
fi

SERVER=$1
SHARE=$2
USERNAME=${3:-local}
PASSWORD=${4:-local}

SMBTORTURE=${SMBTORTURE:-/usr/local/bin/smbtorture4}
DEBUGLEVEL=1

smbtorture() {
	vrun $SMBTORTURE \
	    --debuglevel=$DEBUGLEVEL \
	    -U "$USERNAME"%"$PASSWORD" \
	    "$@"
}

smbtorture "//$SERVER/$SHARE" "APPLE-LOOKUP-RATE"
testok $? $failed
