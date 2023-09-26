#
# SPDX-License-Identifier: APSL-1.0
#
# Copyright (c) 2022 Apple Inc. All rights reserved.
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

: ${BATS_UID:=501}

atf_test_case auid
auid_head()
{
	atf_set "require.user" "root"
}
auid_body()
{

	# Substitute for the test framework not passing in an unprivileged_user
	# that we can su(1) to.
	unpriv_user=$(atf_config_get unprivileged_user 2>/dev/null)
	if [ -z "$unpriv_user" ]; then
		unpriv_user=$(id -un "$BATS_UID")
	fi

	unpriv_uid=$(id -u "$unpriv_user")

	# Make sure we're running with an auid of 1 (daemon).  We need to make
	# sure that su(1) sets our auid to the target uid (unpriv_uid) rather
	# than to the current uid (root) or leaving it at the previously-set
	# auid (daemon)
	atf_check -o match:"auid=1$" id -A
	atf_check -o match:"auid=${unpriv_uid}$" su - "$unpriv_user" -c "id -A"
}

atf_init_test_cases()
{

	atf_add_test_case auid
}
