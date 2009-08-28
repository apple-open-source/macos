#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright 2006 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#
#pragma ident	"@(#)tst.ucaller.ksh	1.1	06/08/28 SMI"

#
# This test is a bit naughty; it's assuming that ld.so.1 has an implementation
# of calloc(3C), and that it's implemented in terms of the ld.so.1
# implementation of malloc(3C).  If you're reading this comment because
# those assumptions have become false, please accept my apologies...
#
if [ -f /usr/lib/dtrace/darwin.d ]; then
dylib=libSystem.B.dylib
prog="/bin/echo"
caller=exit
callee=__cxa_finalize
#
# Added -xevaltime="exec" and -Z flags. The target process was exiting before we could get it instrumented with the default postinit evaltime
#
else
prog="/usr/bin/echo"
dylib=ld.so.1
caller=calloc
callee=malloc
fi

dtrace -xevaltime="exec" -Zqs /dev/stdin -c $prog <<EOF
pid\$target:$dylib:$caller:entry
{
	self->$caller = 1;
}

pid\$target:$dylib:$callee:entry
/self->$caller/
{
	@[umod(ucaller), ufunc(ucaller)] = count();
}

pid\$target:$dylib:$caller:return
/self->$caller/
{
	self->$caller = 0;
}

END
{
	printa("%A %A\n", @);
}
EOF

exit 0
