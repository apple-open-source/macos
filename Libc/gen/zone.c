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
#import <pthread.h>
#import <stdlib.h>
#import <unistd.h>

#define OBSOLETED

static pthread_mutex_t _zone_mutex = PTHREAD_MUTEX_INITIALIZER;

enum {
    nXDefaultMallocZone = 0,
    nXCreateZone,
    nXNameZone,
    nXZoneMalloc,
    nXZoneRealloc,
    nXZoneCalloc,
    nXZoneFree,
    nXDestroyZone,
    nXZoneFromPtr,
    nXZonePtrInfo,
    nXMallocCheck,
    _nXMallocDumpZones
};
static char *once[] = {
    "NXDefaultMallocZone",
    "NXCreateZone",
    "NXNameZone",
    "NXZoneMalloc",
    "NXZoneRealloc",
    "NXZoneCalloc",
    "NXZoneFree",
    "NXDestroyZone",
    "NXZoneFromPtr",
    "NXZonePtrInfo",
    "NXMallocCheck",
    "_NXMallocDumpZones"
};

extern int __is_threaded;

/*********	NX functions	************/

static void
_deprecated(int index)
{
    if(__is_threaded)
	pthread_mutex_lock(&_zone_mutex);
    if(once[index]) {
	fprintf(stderr, "*** %s[%d]: %s() is deprecated and will be removed in the future\n", getprogname(), getpid(), once[index]);
	once[index] = NULL;
    }
    if(__is_threaded)
	pthread_mutex_unlock(&_zone_mutex);
}

malloc_zone_t *NXDefaultMallocZone() {
    _deprecated(nXDefaultMallocZone);
    return malloc_default_zone();
}

malloc_zone_t *NXCreateZone(size_t startsize, size_t granularity, int canfree) {
    _deprecated(nXCreateZone);
    return malloc_create_zone(startsize, 0);
}

void NXNameZone(malloc_zone_t *z, const char *name) {
    _deprecated(nXNameZone);
    malloc_set_zone_name(z, name);
}

void *NXZoneMalloc(malloc_zone_t *zone, size_t size) {
    _deprecated(nXZoneMalloc);
    return malloc_zone_malloc(zone, size);
}

void *NXZoneRealloc(malloc_zone_t *zone, void *ptr, size_t size) {
    _deprecated(nXZoneRealloc);
    return malloc_zone_realloc(zone, ptr, size);
}

void *NXZoneCalloc(malloc_zone_t *zone, size_t num_items, size_t size) {
    _deprecated(nXZoneCalloc);
    return malloc_zone_calloc(zone, num_items, size);
}

void NXZoneFree(malloc_zone_t *zone, void *ptr) {
    _deprecated(nXZoneFromPtr);
    malloc_zone_free(zone, ptr);
}

void NXDestroyZone(malloc_zone_t *zone) {
    _deprecated(nXDestroyZone);
    if (zone == malloc_default_zone()) return; // we avoid destroying child zones
    malloc_destroy_zone(zone);
}

NXZone *NXZoneFromPtr(void *ptr) {
    NXZone	*zone = malloc_zone_from_ptr(ptr);
    _deprecated(nXZoneFromPtr);
    if (!zone) {
        malloc_printf("*** NXZoneFromPtr() did not find any zone for %p; returning default\n", ptr);
        zone = NX_NOZONE;
    }
    return zone;
}

#ifndef OBSOLETED
void NXAddRegion(void *start, size_t size, malloc_zone_t *zone) {
    malloc_printf("*** OBSOLETE: NXAddRegion()\n");
}

void NXRemoveRegion(void *start) {
    malloc_printf("*** OBSOLETE: NXRemoveRegion()\n");
}
#endif /* OBSOLETED */

void NXZonePtrInfo(void *ptr) {
    _deprecated(nXZonePtrInfo);
    malloc_zone_print_ptr_info(ptr);
}

int NXMallocCheck(void) {
    _deprecated(nXMallocCheck);
    malloc_zone_check(NULL);
    return 1;
}

void _NXMallocDumpZones(void) {
    _deprecated(_nXMallocDumpZones);
    malloc_zone_print(NULL, 0);
}

/*****************	UNIMPLEMENTED ENTRY POINTS	********************/

#ifndef OBSOLETED
void NXMergeZone(malloc_zone_t *z) {
    static char warned = 0;
    if (!warned) {
        malloc_printf("*** NXMergeZone() now obsolete, does nothing\n");
        warned = 1;
    }
}

boolean_t NXProtectZone(malloc_zone_t *zone, int protection) {
    malloc_printf("*** NXProtectZone() is obsolete\n");
    return 0;
}

malloc_zone_t *NXCreateChildZone(malloc_zone_t *parentzone, size_t startsize, size_t granularity, int canfree) {
    // We can not remove this one as it is still used by IndexingKit
    static char warned = 0;
    if (!warned) {
        malloc_printf("*** NXCreateChildZone() now obsolete, has been defined to create new zone\n");
        warned = 1;
    }
    return NXCreateZone(startsize, granularity, canfree);
}

void _NXMallocDumpFrees(void) {
    malloc_printf("*** OBSOLETE: _NXMallocDumpFrees()\n");
}
#endif /* OBSOLETED */
