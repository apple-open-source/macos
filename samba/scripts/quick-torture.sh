#! /bin/bash

SHARE=${SHARE:-test}
SERVER=${SERVER:-127.0.0.1}
SMBTORTURE=${SMBTORTURE:-/usr/local/bin/smbtorture4}
PORT=${PORT:-445}

USERNAME=${USERNAME:-local}
PASSWORD=${PASSWORD:-local}

. $(dirname $0)/common.sh

tmpfile=.tmp-$(basename $0).$$
trap "rm -f $tmpfile" 0 1 2 3 15

error()
{
    echo "$@" 1>&1
    exit 1
}

# Run an arbitrary netbench test
smbd_run_smbtorture_adhoc()
{

    echo Running smbtorture test $TORTURETEST ...
    vrun $SMBTORTURE //$SERVER/$SHARE -p $PORT \
	    -U$USERNAME%$PASSWORD -L $TORTURETEST 2>&1 | \
	grep -iv "unknown parameter"
}

# Run the full set of smbtorture tests that are expected to pass
smbd_run_quicklooks()
{
    echo Running samba quicklooks ...
    tmpfile=.tmp-$(basename $0).$$

    $(dirname $0)/smbtorture.list | while read t ; do
	[ -z "$t" ] && continue
	TORTURETEST=$t smbd_run_smbtorture_adhoc | tee $tmpfile
	if grep NT_STATUS_CONNECTION_REFUSED $tmpfile >/dev/null 2>&1 ; then
	    error //$SERVER/$SHARE not accepting connections
	fi
    done
}

smbd_run_quicklooks
