/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

#import <objc/malloc.h>

#define SCALABLE_MALLOC_ADD_GUARD_PAGES		(1 << 0)
    // add a guard page before and after each VM region to help debug
#define SCALABLE_MALLOC_DONT_PROTECT_PRELUDE	(1 << 1)
    // do not protect prelude page
#define SCALABLE_MALLOC_DONT_PROTECT_POSTLUDE	(1 << 2)
    // do not protect postlude page
#define SCALABLE_MALLOC_DO_SCRIBBLE		(1 << 3)
    // write 0x55 onto free blocks

extern malloc_zone_t *create_scalable_zone(size_t initial_size, unsigned debug_flags);
    /* Create a new zone that scales for small objects or large objects */

/*****	Private API for debug and performance tools	********/

#define scalable_zone_info_count	9	// maximum number of numbers

extern void scalable_zone_info(malloc_zone_t *zone, unsigned *info, unsigned count);
    /* Fills info[] with some statistical information:
    info[0]: number of objects in use
    info[1]: number of bytes in use
    ...
    */

