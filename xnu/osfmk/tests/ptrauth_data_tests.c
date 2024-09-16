/*
 * Copyright (c) 2020 Apple Inc. All rights reserved.
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

#if DEVELOPMENT || DEBUG
#if __has_feature(ptrauth_calls)

#include <pexpert/pexpert.h>
#include <mach/port.h>
#include <mach/task.h>
#include <kern/task.h>
#include <vm/vm_map_xnu.h>
#include <vm/pmap.h>
#include <ipc/ipc_types.h>
#include <ipc/ipc_port.h>
#include <kern/ipc_kobject.h>
#include <kern/kern_types.h>
#include <libkern/ptrauth_utils.h>

kern_return_t ptrauth_data_tests(void);

/*
 * Given an existing PAC pointer (ptr), its declaration type (decl), the (key)
 * used to sign it and the string discriminator (discr), extract the raw pointer
 * along with the signature and compare it with one computed on the fly
 * via ptrauth_sign_unauthenticated().
 *
 * If the two mismatch, return an error and fail the test.
 */
#define VALIDATE_PTR(decl, ptr, key, discr) { \
	decl raw = *(decl *)&(ptr);      \
	decl cmp = ptrauth_sign_unauthenticated(ptr, key, \
	        ptrauth_blend_discriminator(&ptr, ptrauth_string_discriminator(discr))); \
	if (cmp != raw) { \
	        printf("kern.run_pac_test: %s (%s) (discr=%s) is not signed as expected (%p vs %p)\n", #decl, #ptr, #discr, raw, cmp); \
	        kr = KERN_INVALID_ADDRESS; \
	} \
}

/*
 * Allocate the containing structure, and store a pointer to the desired member,
 * which should be subject to pointer signing.
 */
#define ALLOC_VALIDATE_DATA_PTR(structure, decl, member, discr) { \
	__typed_allocators_ignore_push \
	structure *tmp = kalloc_data(sizeof(structure), Z_WAITOK | Z_ZERO); \
	if (!tmp) return KERN_NO_SPACE; \
	tmp->member = (void*)0xffffffff41414141; \
	VALIDATE_DATA_PTR(decl, tmp->member, discr) \
	kfree_data(tmp, sizeof(structure)); \
	__typed_allocators_ignore_pop \
}

#define VALIDATE_DATA_PTR(decl, ptr, discr) VALIDATE_PTR(decl, ptr, ptrauth_key_process_independent_data, discr)

/*
 * Regression test for rdar://103054854
 */
#define PTRAUTH_DATA_BLOB_TESTS(void) { \
	unsigned char a[] = { 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; \
	unsigned char b[] = { 0x41, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; \
	if (ptrauth_utils_sign_blob_generic(a, 1, 0, 0) != ptrauth_utils_sign_blob_generic(b, 1, 0, 0)) { \
	        printf("kern.run_pac_test: ptrauth_data_blob_tests: mismatched blob signatures\n"); \
	        kr = KERN_FAILURE; \
	} \
}

/*
 * Validate that a pointer that is supposed to be signed, is, and that the signature
 * matches based on signing key, location and discriminator
 */
kern_return_t
ptrauth_data_tests(void)
{
	int kr = KERN_SUCCESS;

	/* task_t */
	ALLOC_VALIDATE_DATA_PTR(struct task, vm_map_t, map, "task.map");
	ALLOC_VALIDATE_DATA_PTR(struct task, struct ipc_port *, itk_task_ports[0], "task.itk_task_ports");
#if CONFIG_CSR
	ALLOC_VALIDATE_DATA_PTR(struct task, struct ipc_port *, itk_settable_self, "task.itk_settable_self");
#endif /* CONFIG_CSR */
	ALLOC_VALIDATE_DATA_PTR(struct task, struct ipc_port *, itk_host, "task.itk_host");
	ALLOC_VALIDATE_DATA_PTR(struct task, struct ipc_port *, itk_bootstrap, "task.itk_bootstrap");
	ALLOC_VALIDATE_DATA_PTR(struct task, struct ipc_port *, itk_debug_control, "task.itk_debug_control");
	ALLOC_VALIDATE_DATA_PTR(struct task, struct ipc_space *, itk_space, "task.itk_space");
	ALLOC_VALIDATE_DATA_PTR(struct task, struct ipc_port *, itk_task_access, "task.itk_task_access");
	ALLOC_VALIDATE_DATA_PTR(struct task, struct ipc_port *, itk_resume, "task.itk_resume");

	/* _vm_map */
	ALLOC_VALIDATE_DATA_PTR(struct _vm_map, pmap_t, pmap, "_vm_map.pmap");

	/* ipc_port */
	ALLOC_VALIDATE_DATA_PTR(struct ipc_port, ipc_kobject_label_t, ip_kolabel, "ipc_port.kolabel");

	/* ipc_kobject_label */
	ALLOC_VALIDATE_DATA_PTR(struct ipc_kobject_label, ipc_kobject_t, ikol_alt_port, "ipc_kobject_label.ikol_alt_port");

	/* ipc_entry */
	ALLOC_VALIDATE_DATA_PTR(struct ipc_entry, struct ipc_object *, ie_object, "ipc_entry.ie_object");

	/* ipc_kmsg */
	ALLOC_VALIDATE_DATA_PTR(struct ipc_kmsg, void *, ikm_udata, "kmsg.ikm_udata");
	ALLOC_VALIDATE_DATA_PTR(struct ipc_kmsg, struct ipc_port *, ikm_voucher_port, "kmsg.ikm_voucher_port");

	PTRAUTH_DATA_BLOB_TESTS();
	return kr;
}

#endif /*  __has_feature(ptrauth_calls) */
#endif /* DEVELOPMENT || DEBUG */
