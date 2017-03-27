#!/bin/sh -p
dtrace=/usr/sbin/dtrace

############################################################################
# ASSERTION:
#	To verify that a binary launched with dtrace -c with the default options
#	is controlled and can hit a probe specified by module and function name
#
#	This relies on the /usr/bin/true being on the file system and having a
#	libSystem having a initializer named libSystem_initializer

script()
{
	$dtrace -Z -xnolibs -xevaltime=preinit -c /usr/bin/true -qs /dev/stdin <<EOF
	pid\$target:libSystem.B.dylib:libSystem_initializer:entry
	{
		trace("Called");
		exit(0);
	}
EOF
}


script | tee /dev/fd/2 | grep 'Called'
status=$?

exit $status
