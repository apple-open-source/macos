/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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

#include "internal.h"


// Retrieve PAC-stripped address of introspection struct.  We avoid
// authenticating the pointer when loading it's value, because we can't
// authenticate pointers copied from remote processes (or corpses).
static vm_address_t
get_introspection_addr(const malloc_zone_t *zone)
{
	// return zone->malloc_zone.introspect;  // but without ptrauth
	vm_address_t ptr_addr =
			(vm_address_t)zone + offsetof(malloc_zone_t, introspect);
	vm_address_t addr = *(vm_address_t *)ptr_addr;
	return (vm_address_t)ptrauth_strip((malloc_introspection_t *)addr,
			ptrauth_key_process_independent_data);
}

kern_return_t
get_zone_type(task_t task, memory_reader_t reader,
		vm_address_t zone_address, unsigned *zone_type)
{
	MALLOC_ASSERT(reader);

	kern_return_t kr;
	*zone_type = MALLOC_ZONE_TYPE_UNKNOWN;

	// malloc_introspection_t::zone_type requires zone version >= 14
	malloc_zone_t *zone;
	kr = reader(task, zone_address, sizeof(malloc_zone_t), (void **)&zone);
	if (kr != KERN_SUCCESS) {
		return kr;
	}
	if (zone->version < 14) {
		return KERN_SUCCESS;
	}

	// Retrieve zone type
	vm_address_t zone_type_addr = get_introspection_addr(zone) +
			offsetof(malloc_introspection_t, zone_type);
	unsigned *zt;
	kr = reader(task, zone_type_addr, sizeof(unsigned), (void **)&zt);
	if (kr != KERN_SUCCESS) {
		return kr;
	}

	*zone_type = *zt;
	return KERN_SUCCESS;
}

kern_return_t
malloc_get_wrapped_zone(task_t task, memory_reader_t reader,
		vm_address_t zone_address, vm_address_t *wrapped_zone_address)
{
	reader = reader_or_in_memory_fallback(reader, task);

	kern_return_t kr;
	*wrapped_zone_address = (vm_address_t)NULL;

	unsigned zone_type;
	kr = get_zone_type(task, reader, zone_address, &zone_type);
	if (kr != KERN_SUCCESS) {
		return kr;
	}
	if (zone_type != MALLOC_ZONE_TYPE_PGM &&
			zone_type != MALLOC_ZONE_TYPE_SANITIZER) {
		return KERN_SUCCESS;
	}

	// Load wrapped zone address
	vm_address_t wrapped_zone_ptr_addr = zone_address + WRAPPED_ZONE_OFFSET;
	vm_address_t *wrapped_zone_addr;
	kr = reader(task, wrapped_zone_ptr_addr, sizeof(vm_address_t),
			(void **)&wrapped_zone_addr);
	if (kr != KERN_SUCCESS) {
		return kr;
	}

	*wrapped_zone_address = *wrapped_zone_addr;
	return KERN_SUCCESS;
}

// Internal, in-process helper for task-based SPI
malloc_zone_t *
get_wrapped_zone(malloc_zone_t *zone)
{
	malloc_zone_t *wrapped_zone;
	kern_return_t kr = malloc_get_wrapped_zone(mach_task_self(),
			/*memory_reader=*/NULL, (vm_address_t)zone, (vm_address_t *)&wrapped_zone);
	MALLOC_ASSERT(kr == KERN_SUCCESS);  // In-process lookup cannot fail
	return wrapped_zone;
}
