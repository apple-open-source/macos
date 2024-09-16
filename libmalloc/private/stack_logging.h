/*
 * Copyright (c) 1999-2007 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#ifndef _STACK_LOGGING_H_
#define _STACK_LOGGING_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <malloc/malloc.h>
#include <mach/boolean.h>
#include <mach/mach_types.h>
#include <mach/vm_statistics.h>
#include <sys/cdefs.h>
#include <os/availability.h>


/*********	MallocStackLogging permanent SPIs  ************/

#define stack_logging_type_free		0
#define stack_logging_type_generic	1	/* anything that is not allocation/deallocation */
#define stack_logging_type_alloc	2	/* malloc, realloc, etc... */
#define stack_logging_type_dealloc	4	/* free, realloc, etc... */
#define stack_logging_type_vm_allocate  16      /* vm_allocate or mmap */
    /* NOTE: the stack_logging_type_vm_allocate type also contains
     * the VM region's user_tag in the top byte, so it is not valid to check
     * type == stack_logging_type_vm_allocate. The user_tag can be accessed
     * using VM_GET_FLAGS_ALIAS */
#define stack_logging_type_vm_deallocate  32	/* vm_deallocate or munmap */
#define stack_logging_type_mapped_file_or_shared_mem	128

// The valid flags include those from VM_FLAGS_ALIAS_MASK, which give the user_tag of allocated VM regions.
#define stack_logging_valid_type_flags ( \
stack_logging_type_generic | \
stack_logging_type_alloc | \
stack_logging_type_dealloc | \
stack_logging_type_vm_allocate | \
stack_logging_type_vm_deallocate | \
stack_logging_type_mapped_file_or_shared_mem | \
VM_FLAGS_ALIAS_MASK);

// Following flags are absorbed by stack_logging_log_stack()
#define	stack_logging_flag_zone		8	/* NSZoneMalloc, etc... */
#define stack_logging_flag_cleared	64	/* for NewEmptyHandle */

typedef void(malloc_logger_t)(uint32_t type,
							  uintptr_t arg1,
							  uintptr_t arg2,
							  uintptr_t arg3,
							  uintptr_t result,
							  uint32_t num_hot_frames_to_skip);
extern malloc_logger_t *malloc_logger;

/*
 * Load the MallocStackLogging library and register it with libmalloc
 */
boolean_t malloc_register_stack_logger(void);


/*********	MallocStackLogging deprecated SPIs  ************
 *
 * Everything here should be considered deprecated and slated for being deleted.
 * Move over to the equivilant in MallocStackLogging.h
 */

#define STACK_LOGGING_MAX_STACK_SIZE 512

#define STACK_LOGGING_VM_USER_TAG(flags) (((flags) & VM_FLAGS_ALIAS_MASK) >> 24)

typedef enum {
	stack_logging_mode_none = 0,
	stack_logging_mode_all,
	stack_logging_mode_malloc,
	stack_logging_mode_vm,
	stack_logging_mode_lite,
	stack_logging_mode_vmlite
} stack_logging_mode_type;

extern boolean_t turn_on_stack_logging(stack_logging_mode_type mode);
extern void turn_off_stack_logging(void);

/* constants for enabling/disabling malloc stack logging via the memorystatus_vm_pressure_send sysctl */
#define	MEMORYSTATUS_ENABLE_MSL_MALLOC		0x10000000
#define MEMORYSTATUS_ENABLE_MSL_VM			0x20000000
#define MEMORYSTATUS_ENABLE_MSL_LITE		0x40000000
#define MEMORYSTATUS_DISABLE_MSL			0x80000000
#define MEMORYSTATUS_ENABLE_MSL_LITE_FULL	(MEMORYSTATUS_ENABLE_MSL_LITE | MEMORYSTATUS_ENABLE_MSL_VM | MEMORYSTATUS_ENABLE_MSL_MALLOC)
#define MEMORYSTATUS_ENABLE_MSL_LITE_VM		(MEMORYSTATUS_ENABLE_MSL_LITE | MEMORYSTATUS_ENABLE_MSL_VM)

#endif // _STACK_LOGGING_H_
