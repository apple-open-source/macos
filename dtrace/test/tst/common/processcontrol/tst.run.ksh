#!/bin/sh -p
dtrace=/usr/sbin/dtrace

############################################################################
# ASSERTION:
#	To verify that a binary launched with dtrace -c with the default options
#	is controlled and can start tracing correctly.
#
#	This relies on the /usr/bin/true being on the file system and the binary
#	calling some form of "exit" function.



script()
{
	$dtrace -xnolibs -c /usr/bin/true -qs /dev/stdin <<EOF
	pid\$target::*exit*:entry
	{
		trace("Called");
		exit(0);
	}
EOF
}


script | tee /dev/fd/2 | grep 'Called'
status=$?

exit $status
