/*
 * Copyright (c) 2016 Apple Inc. All rights reserved.
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

#ifndef __TRACE_H
#define __TRACE_H

// <sys/kdebug.h> defines these two subclasses for us:
//   DBG_UMALLOC_EXTERNAL - for external entry points into malloc
//   DBG_UMALLOC_INTERNAL - for tracing internal malloc state

#ifndef _MALLOC_BUILDING_CODES_
#include <sys/kdebug.h>
__attribute__((cold))
extern int
kdebug_trace(uint32_t, uint64_t, uint64_t, uint64_t, uint64_t);
#define MALLOC_TRACE(code,arg1,arg2,arg3,arg4) \
	{ if (malloc_traced()) { kdebug_trace(code, arg1, arg2, arg3, arg4); } }
#define TRACE_CODE(name, subclass, code) \
	static const int TRACE_##name = KDBG_EVENTID(DBG_UMALLOC, subclass, code)
#else
# define DBG_UMALLOC 51
# define DBG_UMALLOC_EXTERNAL 0x1
# define DBG_UMALLOC_INTERNAL 0x2
# define STR(x) #x
# define TRACE_CODE(name, subclass, code) \
	printf("0x%x\t%s\n", ((DBG_UMALLOC << 24) | ((subclass & 0xff) << 16) | ((code & 0x3fff) << 2)), STR(name))
#endif

// "external" trace points
TRACE_CODE(malloc, DBG_UMALLOC_EXTERNAL, 0x01);
TRACE_CODE(free, DBG_UMALLOC_EXTERNAL, 0x02);
TRACE_CODE(realloc, DBG_UMALLOC_EXTERNAL, 0x03);
TRACE_CODE(memalign, DBG_UMALLOC_EXTERNAL, 0x04);
TRACE_CODE(calloc, DBG_UMALLOC_EXTERNAL, 0x05);
TRACE_CODE(valloc, DBG_UMALLOC_EXTERNAL, 0x06);
TRACE_CODE(malloc_options, DBG_UMALLOC_EXTERNAL, 0x07);

// "internal" trace points
TRACE_CODE(nano_malloc, DBG_UMALLOC_INTERNAL, 0x1);
TRACE_CODE(tiny_malloc, DBG_UMALLOC_INTERNAL, 0x2);
TRACE_CODE(small_malloc, DBG_UMALLOC_INTERNAL, 0x3);
TRACE_CODE(large_malloc, DBG_UMALLOC_INTERNAL, 0x4);
TRACE_CODE(nano_free, DBG_UMALLOC_INTERNAL, 0x5);
TRACE_CODE(tiny_free, DBG_UMALLOC_INTERNAL, 0x6);
TRACE_CODE(small_free, DBG_UMALLOC_INTERNAL, 0x7);
TRACE_CODE(large_free, DBG_UMALLOC_INTERNAL, 0x8);
TRACE_CODE(malloc_memory_pressure, DBG_UMALLOC_INTERNAL, 0x9);
TRACE_CODE(nano_memory_pressure, DBG_UMALLOC_INTERNAL, 0xa);
TRACE_CODE(madvise, DBG_UMALLOC_INTERNAL, 0xb);
TRACE_CODE(medium_malloc, DBG_UMALLOC_INTERNAL, 0xc);
TRACE_CODE(medium_free, DBG_UMALLOC_INTERNAL, 0xd);

TRACE_CODE(nanov2_region_allocation, DBG_UMALLOC_INTERNAL, 0x10);
TRACE_CODE(nanov2_region_reservation, DBG_UMALLOC_INTERNAL, 0x20);
TRACE_CODE(nanov2_region_protection, DBG_UMALLOC_INTERNAL, 0x30);

TRACE_CODE(xzone_malloc_install_skip, DBG_UMALLOC_INTERNAL, 0x100);
TRACE_CODE(xzone_malloc_full_skip, DBG_UMALLOC_INTERNAL, 0x200);
TRACE_CODE(xzone_malloc_success, DBG_UMALLOC_INTERNAL, 0x300);
TRACE_CODE(xzone_malloc_from_empty, DBG_UMALLOC_INTERNAL, 0x400);
TRACE_CODE(xzone_malloc_from_fresh, DBG_UMALLOC_INTERNAL, 0x500);
TRACE_CODE(xzone_free, DBG_UMALLOC_INTERNAL, 0x600);
TRACE_CODE(xzone_free_madvise, DBG_UMALLOC_INTERNAL, 0x700);
TRACE_CODE(xzone_slot_upgrade, DBG_UMALLOC_INTERNAL, 0x800);
TRACE_CODE(xzone_walk_lock, DBG_UMALLOC_INTERNAL, 0x900);
TRACE_CODE(xzone_walk_unlock, DBG_UMALLOC_INTERNAL, 0xa00);
TRACE_CODE(xzone_thread_detach, DBG_UMALLOC_INTERNAL, 0xb00);
TRACE_CODE(xzone_thread_cache_malloc, DBG_UMALLOC_INTERNAL, 0xc00);
TRACE_CODE(xzone_thread_cache_free, DBG_UMALLOC_INTERNAL, 0xd00);
TRACE_CODE(xzone_list_upgrade, DBG_UMALLOC_INTERNAL, 0xe00);

#endif // __TRACE_H
