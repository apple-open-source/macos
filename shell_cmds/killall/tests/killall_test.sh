#
# SPDX-License-Identifier: APSL-1.0
#
# Copyright (c) 2023 Apple Inc. All rights reserved.
#
# @APPLE_LICENSE_HEADER_START@
#
# "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
# Reserved.  This file contains Original Code and/or Modifications of
# Original Code as defined in and that are subject to the Apple Public
# Source License Version 1.0 (the 'License').  You may not use this file
# except in compliance with the License.  Please obtain a copy of the
# License at http://www.apple.com/publicsource and read it before using
# this file.
#
# The Original Code and all software distributed under the License are
# distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
# INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
# License for the specific language governing rights and limitations
# under the License."
#
# @APPLE_LICENSE_HEADER_END@
#

atf_test_case argv0
argv0_body()
{
	FLAG=killall_test_prog.flag
	TESTPROG=/AppleInternal/Tests/shell_cmds/killall/killall_test_prog

	$TESTPROG "$FLAG" &
	pid=$!
	iter=0

	# Wait up to 2s for the grandchild to spawn.
	while [ ! -f "$FLAG" ] && [ "$iter" -lt 20 ]; do
		sleep 0.10
		iter=$((iter + 1))
	done

	if [ ! -f "$FLAG" ]; then
		atf_fail "Child's child failed to spawn"
	fi

	# We have to save it off, otherwise the string we're searching for will
	# appear as an argument name in the process output.
	atf_check -o save:argv0.ps ps -x
	atf_check grep -q 'innocent_test_prog' argv0.ps

	atf_check killall innocent_test_prog
	wait "$pid"

	# Make sure that `innocent_test_prog` is actually gone.  Avoid assuming
	# that the pid doesn't get reused.
	atf_check -s ignore -o save:afterkill.ps ps -xp "$pid"
	atf_check -s not-exit:0 grep -q innocent_test_prog afterkill.ps
}

atf_init_test_cases()
{

	atf_add_test_case argv0
}
