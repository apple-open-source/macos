/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef __DSCACHE_H__
#define __DSCACHE_H__

#include <NetInfo/dsrecord.h>

typedef struct
{
	u_int32_t merit;
	dsrecord *record;
} dscache_record;

typedef struct
{
	dscache_record *cache;
	u_int32_t cache_size;
	u_int32_t cache_count;
	u_int32_t prune_count;
	u_int32_t save_count;
	u_int32_t remove_count;
	u_int32_t fetch_count;
} dscache;

dscache *dscache_new(u_int32_t);
void dscache_free(dscache *);
void dscache_flush(dscache *);

u_int32_t dscache_index(dscache *, u_int32_t);
void dscache_save(dscache *, dsrecord *);
void dscache_remove(dscache *, u_int32_t);
dsrecord *dscache_fetch(dscache *, u_int32_t);

void dscache_print_statistics(dscache *, FILE *);

#endif __DSCACHE_H__
