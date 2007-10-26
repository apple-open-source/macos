#! /bin/bash
# Copyright (C) 2006-2007 Apple Inc. All rights reserved.

# Script to verify that nmbd finds all the local IPv4 addresses.

# Figure out where the scripts directory is. Allow external override
# so that the SimonSays wrappers can run these from symlinks be setting
# SCRIPTBASE explicitly.
SCRIPTBASE=${SCRIPTBASE:-$(cd $(dirname $0)/.. && pwd )}
.  $SCRIPTBASE/common.sh || exit 2

if [ $# -ne 0 ]; then
cat <<EOF
Usage: nmbd-finds-all-addresses.sh
EOF
exit 1;
fi

NMBLOOKUP=${NMBLOOKUP:-/usr/bin/nmblookup}
IFCOUNT=${I5COUNT:-5}

failed=0
failtest()
{
    failed=`expr $failed + 1`
}

setup_state()
{
    local n=0

    echo adding $IFCOUNT en0 aliases
    while (($n < $IFCOUNT)) ; do
	$ASROOT ifconfig en0 alias 192.168.$n.127 netmask 255.255.255.0
	n=$(($n + 1))
    done
}

remove_state()
{
    local n=0

    echo removing en0 aliases
    while (($n < $IFCOUNT)) ; do
	$ASROOT ifconfig en0 -alias 192.168.$n.127 netmask 255.255.255.0
	n=$(($n + 1))
    done
}

# In this test, we rely on the fact that nmblookup sends a probe on every
# detected interface. Prior to <rdar://5145975>, only the first interface was
# detected.
test_nmblookup()
{
    local count=0

    echo making sure we search on each alias

    # For each configured address, we expect a message like:
    #	    querying XXXX on 192.168.xx.30
    count=$(
	$NMBLOOKUP $(basename $0) | \
	    awk '/192.168/ { print $0 }' | \
	    wc -l | awk '{print $1}'
    )

    echo searched $count subnets
    (( $count == $IFCOUNT ))
}

setup_state
register_cleanup_handler remove_state

test_nmblookup || failtest

testok $0 $failed
