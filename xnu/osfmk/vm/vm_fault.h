/*
 * Copyright (c) 2000-2020 Apple Computer, Inc. All rights reserved.
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
 */
/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989,1988,1987 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */
/*
 */
/*
 *	File:	vm/vm_fault.h
 *
 *	Page fault handling module declarations.
 */

#ifndef _VM_VM_FAULT_H_
#define _VM_VM_FAULT_H_

#include <mach/mach_types.h>
#include <mach/kern_return.h>
#include <mach/boolean.h>
#include <mach/vm_prot.h>
#include <mach/vm_param.h>
#include <mach/vm_behavior.h>

#ifdef  KERNEL_PRIVATE

typedef kern_return_t   vm_fault_return_t;

#define VM_FAULT_SUCCESS                0
#define VM_FAULT_RETRY                  1
#define VM_FAULT_INTERRUPTED            2
#define VM_FAULT_MEMORY_SHORTAGE        3
#define VM_FAULT_MEMORY_ERROR           5
#define VM_FAULT_SUCCESS_NO_VM_PAGE     6       /* success but no VM page */

/*
 *	Page fault handling based on vm_map (or entries therein)
 */

extern kern_return_t vm_fault(
	vm_map_t        map,
	vm_map_offset_t vaddr,
	vm_prot_t       fault_type,
	boolean_t       change_wiring,
#if XNU_KERNEL_PRIVATE
	vm_tag_t        wire_tag,                   /* if wiring must pass tag != VM_KERN_MEMORY_NONE */
#endif
	int             interruptible,
	pmap_t          pmap,
	vm_map_offset_t pmap_addr)
#if XNU_KERNEL_PRIVATE
__XNU_INTERNAL(vm_fault)
#endif
;

extern void vm_pre_fault(vm_map_offset_t, vm_prot_t);

#endif  /* KERNEL_PRIVATE */

#endif  /* _VM_VM_FAULT_H_ */
