/*
 * Copyright (c) 2016 Apple Inc. All rights reserved.
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

#include <sys/sysctl.h>

extern kern_return_t test_pmap_enter_disconnect(unsigned int);
extern kern_return_t test_pmap_compress_remove(unsigned int);
extern kern_return_t test_pmap_exec_remove(unsigned int);
extern kern_return_t test_pmap_nesting(unsigned int);
extern kern_return_t test_pmap_iommu_disconnect(void);
extern kern_return_t test_pmap_extended(void);
extern void test_pmap_call_overhead(unsigned int);
extern uint64_t test_pmap_page_protect_overhead(unsigned int, unsigned int);
#if CONFIG_SPTM
extern kern_return_t test_pmap_huge_pv_list(unsigned int, unsigned int);
extern kern_return_t test_pmap_reentrance(unsigned int);
#endif

static int
sysctl_test_pmap_enter_disconnect(__unused struct sysctl_oid *oidp, __unused void *arg1, __unused int arg2, struct sysctl_req *req)
{
	unsigned int num_loops;
	int error, changed;
	error = sysctl_io_number(req, 0, sizeof(num_loops), &num_loops, &changed);
	if (error || !changed) {
		return error;
	}
	return test_pmap_enter_disconnect(num_loops);
}

SYSCTL_PROC(_kern, OID_AUTO, pmap_enter_disconnect_test,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED,
    0, 0, sysctl_test_pmap_enter_disconnect, "I", "");

static int
sysctl_test_pmap_compress_remove(__unused struct sysctl_oid *oidp, __unused void *arg1, __unused int arg2, struct sysctl_req *req)
{
	unsigned int num_loops;
	int error, changed;
	error = sysctl_io_number(req, 0, sizeof(num_loops), &num_loops, &changed);
	if (error || !changed) {
		return error;
	}
	return test_pmap_compress_remove(num_loops);
}

SYSCTL_PROC(_kern, OID_AUTO, pmap_compress_remove_test,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED,
    0, 0, sysctl_test_pmap_compress_remove, "I", "");

static int
sysctl_test_pmap_exec_remove(__unused struct sysctl_oid *oidp, __unused void *arg1, __unused int arg2, struct sysctl_req *req)
{
	unsigned int num_loops;
	int error, changed;
	error = sysctl_io_number(req, 0, sizeof(num_loops), &num_loops, &changed);
	if (error || !changed) {
		return error;
	}
	return test_pmap_exec_remove(num_loops);
}

SYSCTL_PROC(_kern, OID_AUTO, pmap_exec_remove_test,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED,
    0, 0, sysctl_test_pmap_exec_remove, "I", "");

static int
sysctl_test_pmap_nesting(__unused struct sysctl_oid *oidp, __unused void *arg1, __unused int arg2, struct sysctl_req *req)
{
	unsigned int num_loops;
	int error, changed;
	error = sysctl_io_number(req, 0, sizeof(num_loops), &num_loops, &changed);
	if (error || !changed) {
		return error;
	}
	return test_pmap_nesting(num_loops);
}
SYSCTL_PROC(_kern, OID_AUTO, pmap_nesting_test,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED,
    0, 0, sysctl_test_pmap_nesting, "I", "");

static int
sysctl_test_pmap_iommu_disconnect(__unused struct sysctl_oid *oidp, __unused void *arg1, __unused int arg2, struct sysctl_req *req)
{
	unsigned int run = 0;
	int error, changed;
	error = sysctl_io_number(req, 0, sizeof(run), &run, &changed);
	if (error || !changed) {
		return error;
	}
	return test_pmap_iommu_disconnect();
}

SYSCTL_PROC(_kern, OID_AUTO, pmap_iommu_disconnect_test,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED,
    0, 0, sysctl_test_pmap_iommu_disconnect, "I", "");

static int
sysctl_test_pmap_extended(__unused struct sysctl_oid *oidp, __unused void *arg1, __unused int arg2, struct sysctl_req *req)
{
	unsigned int run = 0;
	int error, changed;
	error = sysctl_io_number(req, 0, sizeof(run), &run, &changed);
	if (error || !changed) {
		return error;
	}
	return test_pmap_extended();
}

SYSCTL_PROC(_kern, OID_AUTO, pmap_extended_test,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED,
    0, 0, sysctl_test_pmap_extended, "I", "");

static int
sysctl_test_pmap_call_overhead(__unused struct sysctl_oid *oidp, __unused void *arg1, __unused int arg2, struct sysctl_req *req)
{
	unsigned int num_loops;
	int error, changed;
	error = sysctl_io_number(req, 0, sizeof(num_loops), &num_loops, &changed);
	if (error || !changed) {
		return error;
	}
	test_pmap_call_overhead(num_loops);
	return 0;
}

SYSCTL_PROC(_kern, OID_AUTO, pmap_call_overhead_test,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED,
    0, 0, sysctl_test_pmap_call_overhead, "I", "");

static int
sysctl_test_pmap_page_protect_overhead(__unused struct sysctl_oid *oidp, __unused void *arg1, __unused int arg2, struct sysctl_req *req)
{
	struct {
		unsigned int num_loops;
		unsigned int num_aliases;
	} ppo_in;

	int error;
	uint64_t duration;

	error = SYSCTL_IN(req, &ppo_in, sizeof(ppo_in));
	if (error) {
		return error;
	}

	duration = test_pmap_page_protect_overhead(ppo_in.num_loops, ppo_in.num_aliases);
	error = SYSCTL_OUT(req, &duration, sizeof(duration));
	return error;
}

SYSCTL_PROC(_kern, OID_AUTO, pmap_page_protect_overhead_test,
    CTLTYPE_OPAQUE | CTLFLAG_RW | CTLFLAG_LOCKED, 0, 0, sysctl_test_pmap_page_protect_overhead, "-", "");

#if CONFIG_SPTM

static int
sysctl_test_pmap_huge_pv_list(__unused struct sysctl_oid *oidp, __unused void *arg1, __unused int arg2, struct sysctl_req *req)
{
	struct {
		unsigned int num_loops;
		unsigned int num_mappings;
	} hugepv_in;

	int error = SYSCTL_IN(req, &hugepv_in, sizeof(hugepv_in));
	if (error) {
		return error;
	}
	return test_pmap_huge_pv_list(hugepv_in.num_loops, hugepv_in.num_mappings);
}

SYSCTL_PROC(_kern, OID_AUTO, pmap_huge_pv_list_test,
    CTLTYPE_OPAQUE | CTLFLAG_RW | CTLFLAG_LOCKED, 0, 0, sysctl_test_pmap_huge_pv_list, "-", "");

static int
sysctl_test_pmap_reentrance(__unused struct sysctl_oid *oidp, __unused void *arg1, __unused int arg2, struct sysctl_req *req)
{
	unsigned int num_loops;
	int error, changed;
	error = sysctl_io_number(req, 0, sizeof(num_loops), &num_loops, &changed);
	if (error || !changed) {
		return error;
	}
	return test_pmap_reentrance(num_loops);
}

SYSCTL_PROC(_kern, OID_AUTO, pmap_reentrance_test,
    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_LOCKED,
    0, 0, sysctl_test_pmap_reentrance, "I", "");

#endif
