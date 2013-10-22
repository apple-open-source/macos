#!/bin/ksh -p
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

#ident	"@(#)tst.PreprocessorAttribute.d.ksh	1.1	13/04/10 SMI"

##
#
# ASSERTION:
# The -C option is used to run the C preprocessor over D programs before
# compiling them. The -H option used in conjuction with the -C option
# lists the pathnames of the included files to STDERR.
#
# SECTION: dtrace Utility/-C Option;
# 	dtrace Utility/-H Option
#
##

script()
{
	$dtrace -CH -s /dev/stdin <<EOF

#include <stdio.h>

__attribute__((should be removed))
__attribute__((should be removed));

__attribute__((should be removed)) void func0(void);
__attribute__((should be removed)) void func1(int a, void(*)());
__attribute__((should be removed)) struct foo { int bar; };

void func2(void) __attribute__((should be removed));

	BEGIN
	{
		printf("This test should compile\n");
		exit(0);
	}
EOF
}

dtrace=/usr/sbin/dtrace

script
status=$?

if [ "$status" -ne 0 ]; then
	echo $tst: dtrace failed
fi

exit $status
