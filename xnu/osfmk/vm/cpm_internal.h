/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
/*
 * @OSF_COPYRIGHT@
 *
 */

#ifndef _VM_CPM_H_
#define _VM_CPM_H_

/*
 *	File:	vm/cpm_internal.h
 *	Author:	Alan Langerman
 *	Date:	April 1995 and January 1996
 *
 *	Contiguous physical memory allocator.
 */

#include <mach/mach_types.h>
#include <vm/vm_page.h>
/*
 *	Return a linked list of physically contiguous
 *	wired pages.  Caller is responsible for disposal
 *	via cpm_release.
 *
 *	These pages are all in "gobbled" state when
 *	wired is FALSE.
 */
extern kern_return_t
cpm_allocate(vm_size_t size, vm_page_t *list, ppnum_t max_pnum, ppnum_t pnum_mask, boolean_t wire, int flags);

#endif  /* _VM_CPM_H_ */
