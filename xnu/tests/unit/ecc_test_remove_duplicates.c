/*
 * Copyright (c) 2000-2024 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#include <darwintest.h>
#include <vm/pmap.h>

extern uint32_t remove_bad_ram_duplicates(uint32_t bad_pages_count, ppnum_t *bad_pages);

#define UT_MODULE osfmk
T_GLOBAL_META(
	T_META_NAMESPACE("xnu.unit.ecc_test_remove_duplicates"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_OWNER("tgal2"),
	T_META_RUN_CONCURRENTLY(false)
	);

T_DECL(ecc_rm_dups_zero_elements, "make sure 0 is returned for empty input")
{
	uint32_t bp_count = 0;
	ppnum_t bp[] = {};
	bp_count = remove_bad_ram_duplicates(bp_count, bp);
	if (bp_count != 0) {
		T_FAIL("Expected: bp_count == 0 but got: bp_count == %u", bp_count);
	}
	T_PASS("bp_count == 0 as expected");
}

T_DECL(ecc_rm_dups_0, "make sure 1 is returned for [0]")
{
	uint32_t bp_count = 1;
	ppnum_t bp[] = { 0 };
	bp_count = remove_bad_ram_duplicates(bp_count, bp);
	if (bp_count != 1 || bp[0] != 0) {
		T_FAIL("Expected: bp_count == 1 and bp[0] == 0 but got: bp_count == %u, bp[0] == %u", bp_count, bp[0]);
	} else {
		T_PASS("bp_count == 1 and bp[0] == 0 as expected");
	}
}

T_DECL(ecc_rm_dups_00, "make sure 1 is returned for [0, 0]")
{
	uint32_t bp_count = 2;
	ppnum_t bp[] = { 0, 0 };
	bp_count = remove_bad_ram_duplicates(bp_count, bp);
	if (bp_count != 1 || bp[0] != 0) {
		T_FAIL("Expected: bp_count == 1 and bp[0] == 0 but got: bp_count == %u, bp[0] == %u", bp_count, bp[0]);
	} else {
		T_PASS("bp_count == 1 and bp[0] == 0 as expected");
	}
}

T_DECL(ecc_rm_dups_001, "make sure 2 is returned for [0, 0, 1]")
{
	uint32_t bp_count = 3;
	ppnum_t bp[] = { 0, 0, 1 };
	bp_count = remove_bad_ram_duplicates(bp_count, bp);
	if (bp_count != 2 || bp[0] != 0 || bp[1] != 1) {
		T_FAIL("Expected: bp_count == 2 and bp == [0, 1] but got: bp_count == %u, bp == [%u, %u]", bp_count, bp[0], bp[1]);
	} else {
		T_PASS("bp_count == 2 and bp == [0, 1] as expected");
	}
}

T_DECL(ecc_rm_dups_101, "make sure 2 is returned for [1, 0, 1]")
{
	uint32_t bp_count = 3;
	ppnum_t bp[] = { 1, 0, 1 };
	bp_count = remove_bad_ram_duplicates(bp_count, bp);
	if (bp_count != 2 || bp[0] != 0 || bp[1] != 1) {
		T_FAIL("Expected: bp_count == 2 and bp == [0, 1] but got: bp_count == %u, bp == [%u, %u]", bp_count, bp[0], bp[1]);
	} else {
		T_PASS("bp_count == 2 and bp == [0, 1] as expected");
	}
}

T_DECL(ecc_rm_dups_201, "make sure 3 is returned for [2, 0, 1]")
{
	uint32_t bp_count = 3;
	ppnum_t bp[] = { 2, 0, 1 };
	bp_count = remove_bad_ram_duplicates(bp_count, bp);
	if (bp_count != 3 || bp[0] != 0 || bp[1] != 1 || bp[2] != 2) {
		T_FAIL("Expected: bp_count == 3 and bp == [0, 1, 2] but got: bp_count == %u, bp == [%u, %u, %u]", bp_count, bp[0], bp[1], bp[2]);
	} else {
		T_PASS("bp_count == 3 and bp == [0, 1, 2] as expected");
	}
}

T_DECL(ecc_rm_dups_2012, "make sure 3 is returned for [2, 0, 1, 2]")
{
	uint32_t bp_count = 4;
	ppnum_t bp[] = { 2, 0, 1, 2 };
	bp_count = remove_bad_ram_duplicates(bp_count, bp);
	if (bp_count != 3 || bp[0] != 0 || bp[1] != 1 || bp[2] != 2) {
		T_FAIL("Expected: bp_count == 3 and bp == [0, 1, 2] but got: bp_count == %u, bp == [%u, %u, %u]", bp_count, bp[0], bp[1], bp[2]);
	} else {
		T_PASS("bp_count == 3 and bp == [0, 1, 2] as expected");
	}
}

T_DECL(ecc_rm_dups_large, "make sure large input is handled correctly")
{
	uint32_t bp_count = 1000;
	ppnum_t bp[1000];
	for (uint32_t i = 0; i < bp_count; i++) {
		bp[i] = (i % 10); // Repeated numbers [0-9]
	}
	bp_count = remove_bad_ram_duplicates(bp_count, bp);
	bool valid = (bp_count == 10);
	for (uint32_t i = 0; i < bp_count && valid; i++) {
		if (bp[i] != i) {
			valid = false;
		}
	}
	if (!valid) {
		T_FAIL("Expected: bp_count == 10 and bp == [0-9] but got: bp_count == %u", bp_count);
	} else {
		T_PASS("bp_count == 10 and bp == [0-9] as expected");
	}
}
