#!/bin/sh -p
dtrace=/usr/sbin/dtrace

############################################################################
# ASSERTION:
#	To verify that a binary launched with dtrace -c with -xevaltime=preinit
#	(which starts tracing after intiializers are run)
#	is controlled and can start tracing correctly.
#
#	This relies on the /usr/bin/true being on the file system and the binary
#	calling the libSystem_initializer function. This is very
#	implementation-specific and might break if the name of the libSystem
#	initializer function changes.

# NOTE:
# We run this with '-Z', because at the time of evaluation, only
# dyld is loaded.


script()
{
	$dtrace -xnolibs -Z -c /usr/bin/true -xevaltime=preinit -qs /dev/stdin <<EOF
	pid\$target::libSystem_initializer:entry
	{
		trace("Called");
		exit(0);
	}
EOF
}


script | tee /dev/fd/2 | grep 'Called'
status=$?

exit $status
