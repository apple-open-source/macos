#!/bin/sh -p
dtrace=/usr/sbin/dtrace

############################################################################
# ASSERTION:
#	To verify that a binary launched with dtrace -W can trace before main 
#	in initializers
#
#	This relies on the /usr/bin/true being on the file system and libSystem
#	having an initializer called libSystem_initializer.


# Start a subshell that will run /usr/bin/true periodically

(
	while [ 1 -eq 1 ]
	do
		/usr/bin/true
		sleep 0.01
	done
) &
SUBPID=$!


script()
{
	$dtrace -xnolibs -W true -qs /dev/stdin <<EOF
	pid\$target::libSystem_initializer:entry
	{
		trace("Called");
		exit(0);
	}
EOF
}


script | tee /dev/fd/2 | grep 'Called'
status=$?

kill -TERM $SUBPID

exit $status
