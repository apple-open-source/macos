/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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

#import <objc/zone.h>
#import <stdio.h>
#import <libc.h>

/*********	NX functions	************/

malloc_zone_t *NXDefaultMallocZone() {
    return malloc_default_zone();
}

malloc_zone_t *NXCreateZone(size_t startsize, size_t granularity, int canfree) {
    return malloc_create_zone(startsize, 0);
}

void NXNameZone(malloc_zone_t *z, const char *name) {
    malloc_set_zone_name(z, name);
}

void *NXZoneMalloc(malloc_zone_t *zone, size_t size) {
    return malloc_zone_malloc(zone, size);
}

void *NXZoneRealloc(malloc_zone_t *zone, void *ptr, size_t size) {
    return malloc_zone_realloc(zone, ptr, size);
}

void *NXZoneCalloc(malloc_zone_t *zone, size_t num_items, size_t size) {
    return malloc_zone_calloc(zone, num_items, size);
}

void NXZoneFree(malloc_zone_t *zone, void *ptr) {
    malloc_zone_free(zone, ptr);
}

void NXDestroyZone(malloc_zone_t *zone) {
    if (zone == malloc_default_zone()) return; // we avoid destroying child zones
    malloc_destroy_zone(zone);
}

NXZone *NXZoneFromPtr(void *ptr) {
    NXZone	*zone = malloc_zone_from_ptr(ptr);
    if (!zone) {
        fprintf(stderr, "*** malloc[%d]: NXZoneFromPtr() did not find any zone for %p; returning default\n", getpid(), ptr);
        zone = NX_NOZONE;
    }
    return zone;
}

void NXAddRegion(void *start, size_t size, malloc_zone_t *zone) {
    fprintf(stderr, "*** malloc[%d]: OBSOLETE: NXAddRegion()\n", getpid());
}

void NXRemoveRegion(void *start) {
    fprintf(stderr, "*** malloc[%d]: OBSOLETE: NXRemoveRegion()\n", getpid());
}

void NXZonePtrInfo(void *ptr) {
    malloc_zone_print_ptr_info(ptr);
}

int NXMallocCheck(void) {
    malloc_zone_check(NULL);
    return 1;
}

void _NXMallocDumpZones(void) {
    malloc_zone_print(NULL, 0);
}

/*****************	UNIMPLEMENTED ENTRY POINTS	********************/

void NXMergeZone(malloc_zone_t *z) {
    static char warned = 0;
    if (!warned) {
        fprintf(stderr, "*** malloc[%d]: NXMergeZone() now obsolete, does nothing\n", getpid());
        warned = 1;
    }
}

boolean_t NXProtectZone(malloc_zone_t *zone, int protection) {
    fprintf(stderr, "*** malloc[%d]: NXProtectZone() is obsolete\n", getpid());
    return 0;
}

malloc_zone_t *NXCreateChildZone(malloc_zone_t *parentzone, size_t startsize, size_t granularity, int canfree) {
    // We can not remove this one as it is still used by IndexingKit
    static char warned = 0;
    if (!warned) {
        fprintf(stderr, "*** malloc[%d]: NXCreateChildZone() now obsolete, has been defined to create new zone\n", getpid());
        warned = 1;
    }
    return NXCreateZone(startsize, granularity, canfree);
}

void _NXMallocDumpFrees(void) {
    fprintf(stderr, "*** malloc[%d]: OBSOLETE: _NXMallocDumpFrees()\n", getpid());
}

