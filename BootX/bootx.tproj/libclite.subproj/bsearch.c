/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *  bsearch.c - bsearch from libc.
 *
 *  Copyright (c) 1998-2000 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#include <libclite.h>

void *
bsearch(const void *key, const void *base, size_t nmemb, size_t size,
	int (*compar)(const void *, const void *))
{
    int l = 0;
    int u = nmemb - 1;
    int m;
    void *mp;
    int r;

    while (l <= u) {
	m = (l + u) / 2;
	mp = (void *)(((char *)base) + (m * size));
	if ((r = (*compar) (key, mp)) == 0) {
	    return mp;
	} else if (r < 0) {
	    u = m - 1;
	} else {
	    l = m + 1;
	}
    }
    return NULL;
}

