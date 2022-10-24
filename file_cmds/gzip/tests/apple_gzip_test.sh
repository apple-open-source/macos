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

# This series of tests deals with the regression surfaced in rdar://83282793,
# file metadata was lost upon extraction.
atf_test_case extract_chmod
extract_chmod_body()
{
	echo "Foo" > bar
	chmod 644 bar

	atf_check -o match:"100644" stat -f '%p' bar
	atf_check gzip bar
	atf_check gunzip bar.gz
	atf_check -o match:"100644" stat -f '%p' bar
}

atf_test_case extract_mtime
extract_mtime_body()
{
	echo "Foo" > bar

	sleep 1
	touch bar
	atf_check -o save:bar-times.out stat -f '%m' bar
	atf_check gzip bar

	sleep 1
	atf_check gunzip bar.gz
	atf_check -o file:bar-times.out stat -f '%m' bar
}

atf_test_case extract_unlink
extract_unlink_body()
{
	echo "Foo" > bar

	atf_check test '!' -f bar.gz
	atf_check gzip bar
	atf_check test '!' -f bar
	atf_check gunzip bar.gz
	atf_check test '!' -f bar.gz
	atf_check test -f bar
}

# Test for rdar://82466738, -f forcing a .Z suffix breaks man.sh -- it expects
# the filename as-is to work.
atf_test_case cat_force
cat_force_body()
{
	echo "Foo" > bar

	atf_check -o file:bar zcat -f bar

	echo "Zoo" > bar.Z
	atf_check -o file:bar zcat -f bar
}

# Test for rdar://17946745, `gzip -tlv` should work.
atf_test_case test_tlv
test_tlv_body()
{
	echo "Foo" > bar

	atf_check xz bar
	atf_check -e match:'^bar.xz:[[:space:]]+OK$' gzip -tlv bar.xz
	rm bar.xz

	echo "Foo" > bar
	atf_check gzip bar
	atf_check -o ignore -e match:'^bar.gz:[[:space:]]+OK$' \
	    gunzip -tlv bar.gz
}

atf_init_test_cases()
{
	atf_add_test_case extract_chmod
	atf_add_test_case extract_mtime
	atf_add_test_case extract_unlink
	atf_add_test_case cat_force
	atf_add_test_case test_tlv
}
