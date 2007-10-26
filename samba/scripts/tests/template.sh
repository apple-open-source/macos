#! /bin/bash
# Copyright (C) 2006-2007 Apple Inc. All rights reserved.

# Template test script.
PROGNAME=template.sh

# Figure out where the scripts directory is. Allow external override
# so that the SimonSays wrappers can run these from symlinks be setting
# SCRIPTBASE explicitly.
SCRIPTBASE=${SCRIPTBASE:-$(cd $(dirname $0)/.. && pwd )}
.  $SCRIPTBASE/common.sh || exit 2

if [ $# -lt 2 ]; then
cat <<EOF
Usage: $PROGNAME SERVER SHARE USERNAME PASSWORD
EOF
exit 1;
fi

SERVER=$1
SHARE=$2
USERNAME=${3:-local}
PASSWORD=${4:-local}

failed=0
failtest()
{
    failed=`expr $failed + 1`
}

setup_state()
{
    true
}

remove_state()
{
    true
}

do_first_test()
{
    true
}

do_next_test()
{
    false
}

setup_state
register_cleanup_handler remove_state

do_first_test || failtest
do_next_test || failtest

testok $0 $failed
