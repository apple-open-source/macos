/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/* cache */

#include "cache.h"
#include "libsa.h"

cache_t *cacheInit(
    int nitems,
    int item_size
)
{
    cache_t *cp;
    item_size += sizeof(item_t);
    cp = (cache_t *)malloc(sizeof(cache_t) + nitems * item_size);
    cp->nitems = nitems;
    cp->item_size = item_size;
    return cp;
}

/*
 * Either find an item in the cache, or find where it should go.
 * Returns 1 if found, 0 if not found.
 * This function assumes that if you find an empty slot, you will use it;
 * therefore, empty slots returned are marked valid.
 */
int cacheFind(
    cache_t *cp,
    int key1,
    int key2,
    char **ip
)
{
    item_t *iip, *min_p;
    int i,j;
    
    for(i=j=0, iip = min_p = (item_t *)cp->storage; i < cp->nitems; i++) {
	if (iip->referenced && (iip->key1 == key1) && (iip->key2 == key2)) {
	    *ip = iip->storage;
	    if (iip->referenced < 65535)
		iip->referenced++;
	    return 1;
	}
	if (iip->referenced < min_p->referenced) {
	    min_p = iip;
	    j = i;
	}
	iip = (item_t *)((char *)iip + cp->item_size);
    }
    *ip = min_p->storage;
    min_p->referenced = 1;
    min_p->key1 = key1;
    min_p->key2 = key2;
    return 0;
}

/*
 * Flush the cache.
 */
void cacheFlush(
    cache_t *cp
)
{
    int i;
    item_t *ip;
    
    if (cp == 0)
	return;
    for(i=0, ip = (item_t *)cp->storage; i < cp->nitems; i++) {
	ip->referenced = 0;
	ip = (item_t *)((char *)ip + cp->item_size);
    }
}
