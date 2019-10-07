#!/bin/sh

GROUPID=31337

dscl /Local/Default -read /Groups/$GROUPID > /dev/null 2>&1
if [ $? != "0" ]; then
	dscl /Local/Default -create /Groups/$GROUPID 
	dscl /Local/Default -create /Groups/$GROUPID PrimaryGroupID 9999
fi

mkdir /tmp/$$
chgrp $GROUPID /tmp/$$
gid=`/usr/bin/stat -f '%g' /tmp/$$`
if [ "$gid" != "9999" ]; then
	echo "chgrp $GROUPID, expected group 9999, is $gid"
	exit 1
fi

chgrp -n $GROUPID /tmp/$$
gid=`/usr/bin/stat -f '%g' /tmp/$$`
if [ "$gid" != "$GROUPID" ]; then
	echo "chgrp -n $GROUPID, expected group $GROUPID, is $gid"
	exit 1
fi

exit 0
