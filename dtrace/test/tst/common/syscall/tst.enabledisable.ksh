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
# Copyright 2023 Apple, Inc.  All rights reserved.
# Use is subject to license terms.
#

#
# ASSERTION:
#     Check that syscall enabling/disabling mechanism works.
#

dtrace_script()
{
	$dtrace -s /dev/stdin <<EOF
	syscall:::,
	mach_trap:::
	{ }

	tick-2s
	{
		exit(0);
	}
EOF
}

dtrace="/usr/sbin/dtrace"

for i in {1..5}; do
	dtrace_script &
	wait $!
	status=$?

	if [ $status != 0 ]; then
		break;
	fi
done

exit $status
