#!/bin/sh
#
# Test that ServerRoot/ doesn't serve up symlinks
#
# Copyright © 2007-2021 by Apple Inc.
#
# Licensed under Apache License v2.0.  See the file "LICENSE" for more
# information.
#

# set -x
set -u

_PRINTERNAME=SHARED_PRINTER
_PPDDIR=$CUPS_SERVERROOT/ppd
_PRINTERPPD=$_PPDDIR/$_PRINTERNAME.ppd

runcups=${runcups:-$CUPS_SERVERROOT/runcups}

echo "#  cupsd symlnk retrieval test"
echo "#  "

echo "#  remove existing shared printer"
$runcups ../systemv/lpadmin -h $CUPS_SERVER -x $_PRINTERNAME 2>&1

echo "#  add a shared printer"
$runcups ../systemv/lpadmin -h $CUPS_SERVER -i testps.ppd -o printer-is-shared=true -p $_PRINTERNAME -v socket://127.0.0.1 2>&1

function fetch_ppd_expect
{
	echo "#  Fetch the $_PRINTERNAME PPD expecting result $1"
	curl --silent --fail -o /dev/null http://$CUPS_SERVER/ppd/$_PRINTERNAME.ppd 2>&1
	_R=$?
	echo "#  curl result $_R"

	if test $_R != $1; then
		echo "    FAILED"
		exit 1
	else
		echo "    PASSED"
	fi
}

echo "# 1: Expect success with normal ppd/printer.ppd"
fetch_ppd_expect 0

echo "# 2: Expect failure with ppd/symlink.ppd"
mv $_PRINTERPPD $_PRINTERPPD.orig
ln -s  $_PRINTERPPD.orig  $_PRINTERPPD
fetch_ppd_expect 22

echo "# 3: Expect failure with ppd(symlink)/symlink.ppd"
mv $_PPDDIR $_PPDDIR.orig
ln -s $_PPDDIR.orig $_PPDDIR
fetch_ppd_expect 22

echo "# 4: Expect success with ppd(symlink)/printer.ppd"
mv $_PRINTERPPD.orig $_PRINTERPPD
fetch_ppd_expect 0

echo "# 5: Restore the directory and check success"
rm $_PPDDIR
mv $_PPDDIR.orig $_PPDDIR
fetch_ppd_expect 0

echo "#  Done"
