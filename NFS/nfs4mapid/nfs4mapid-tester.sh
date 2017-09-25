#!/bin/sh

ODNODE=nod.apple.com
DF_USERS=bricker
DF_GROUPS=apple_sw
WKIDS="OWNER@ GROUP@ EVERYONE@ INTERACTIVE@ NETWORK@ DIALUP@ BATCH@ ANONYMOUS@ AUTHENTICATED@ SERVICE@"
NFS4_CURRENT_DOMAIN=$(sysctl -n vfs.generic.nfs.client.default_nfs4domain)

function set_nfs4_domain {
    typeset domain=$(sysctl -n vfs.generic.nfs.client.default_nfs4domain)

    if [[ $domain == $1 ]]; then
	return 0
    fi
    sudo sysctl -w vfs.generic.nfs.client.default_nfs4domain=$1 || {
	echo "[INFO] test aborted"
	echo "[FAIL] Could not set nfs4 default domain"
	return 1
    }
}

function getid_and_guid {
    unset ID
    unset GUID
    eval $(sudo nfs4mapid $* 2>/dev/null | awk 'NR == 1 { printf "ID=%s; ", $NF} NR == 2 { print "GUID=" $NF}')
}

function getmapid {
    sudo nfs4mapid $* | awk '{print $NF}'
}

function testname {
    typeset  SID SGUID SNAME STATUS=0 SNAME2 IDOPT="-u" OPT STATUS=0

    if [[ $1 == "-g" ]]; then
	IDOPT=$1
	OPT="-G"
	shift
    fi
    SNAME=$1

    getid_and_guid $OPT $SNAME
    SID=$ID; SGUID=$GUID

    if [[ $ID == -2 && $SNAME != "nobody" ]]; then
	echo "[INFO] test aborted"
	echo "[INFO] $SNAME does not map"
	return 1
    fi

    # Now check the reverse mapping
    getid_and_guid $IDOPT $ID
    if [[ $SNAME != $ID || $GUID != $SGUID ]]; then
	echo "[INFO] $SID maps to $ID not $SNAME and/or $GUID does not map to $SGUID"
	STATUS=1
    fi

    # Check that the we get the user/group from the guid mapping
    SNAME2=$(getmapid $OPT $GUID)
    if [[ $SNAME != $SNAME2 ]]; then
	echo "[INFO] $GUID maps to $SNAME2 not $SNAME"
	STATUS=1
    fi
    return $STATUS
}

function testid {
    typeset  SNAME SGUID  STATUS=0 SNAME2 IDOPT="-u" OPT STATUS=0

    if [[ $1 == "-g" ]]; then
	IDOPT=$1
	OPT="-G"
	shift
    fi
    NID=$1

    getid_and_guid $IDOPT $NID
    SNAME=$ID; SGUID=$GUID

    if [[ $SNAME == $NID ]]; then
	echo "[INFO] $NID does not map to a name"
    fi

    # Now check the reverse mapping
    getid_and_guid $OPT $SNAME
    if [[ $NID != $ID || $GUID != $SGUID ]]; then
	echo "[INFO] $SNAME maps to $ID not $NID and/or $GUID does not map to $SGUID"
	STATUS=1
    fi

    # Check that the we get the user/group from the guid mapping
    SNAME2=$(getmapid $OPT $GUID)
    if [[ $SNAME != $SNAME2 ]]; then
	echo "[INFO] $GUID maps to $SNAME2 not $SNAME"
	STATUS=1
    fi
    return $STATUS
}

function status
{
    local stat=$?

    if [[ $stat == 0 ]]; then
	echo "[PASS] $1"
    else
	echo "[FAIL] $1"
    fi
    return $stat
}

function testwellknowns {
    typeset i STATUS=0 SNAME GUID TNAME

    for i in $WKIDS
    do
	TNAME="Testing wellknown id $i"
	echo "[BEGIN] $TNAME"
	GUID=$(getmapid -G $i)
	SNAME=$(getmapid -G $GUID)
	if [[ $i != $SNAME ]]; then
	    echo "[INFO] $i maps to $GUID, but that maps to $SNAME"
	    false
	fi
	status "$TNAME" || STATUS=1
    done

    return $STATUS
}

function testusers {
    typeset i STATUS=0 NODE TNAME

    NODE=${1:-$ODNODE}
    shift
    for i in $@
    do
	if [[ ${i##*@} == $i ]]; then
	    i=$i@$NODE
	fi
	TNAME="Testing user $i"
	echo "[BEGIN] $TNAME"
	testname $i
	status "$TNAME" || STATUS=1
    done

    return $STATUS
}

function testgroups {
    typeset i STATUS=0 TNAME

    NODE=${1:-$ODNODE}
    shift
    for i in $@
    do
	if [[ ${i##*@} == $i ]]; then
	    i=$i@$NODE
	fi
	TNAME="Testing group $i"
	echo "[BEGIN] $TNAME"
	testname -g $i
	status "$TNAME" || STATUS=1
    done

    return $STATUS
}

function testdomain {
    typeset STATUS=0

    set_nfs4_domain $1 || return 1

    testusers "$1" $XUSERS || STATUS=1
    testgroups "$1" $XGROUPS || STATUS=1
    testwellknowns || STATUS=1

    return $STATUS
}


function Usage {
    echo ${0##*/} [-h] [[-U user] ...] [[-G group] ...] [-D NFS4DOMAIN] [-d ODNODE]
    exit 1
}

while getopts "hU:u:G:g:D:d:" opt
do
    case $opt in
	U) XUSERS="$USERS $OPTARG";;
	G) XGROUPS="$GROUPS $OPTARG";;
	D) NFS4DOMAIN=$OPTARG;;
	d) ODNODE=$OPTARG;;
	*) Usage;;
    esac
done

XUSERS=${XUSERS:-$DF_USERS}
XGROUPS=${XGROUPS:-$DF_GROUPS}

shift $(($OPTIND-1))
if (( $# > 0)); then
    Usage
fi

STATUS=0
testdomain  || STATUS=1

if [[ -n $NFS4DOMAIN ]]; then
    testdomain $NFS4DOMAIN || STATUS=1
fi

if [[ -n $NFS4_CURRENT_DOMAIN &&  $NFS4_CURRENT_DOMAIN != $NFS4DOMAIN ]]; then
	testdomain $NFS4_CURRENT_DOMAIN || STATUS=1
fi

exit $STATUS
