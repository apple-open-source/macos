/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright 1993 NeXT Computer, Inc.
 * All rights reserved.
 *
 * Sam's simple memory allocator.
 *
 */
/*
 *  zalloc.c - malloc functions.
 *
 *  Copyright (c) 1998-2003 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#include <libclite.h>

#define ZALLOC_NODES    768

#define ZDEBUG 0

#if ZDEBUG
int zout;
#endif

typedef struct {
	char *start;
	int size;
} zmem;

static int zalloc_addr;
static int zalloc_len;
static zmem *zalloced;
static zmem *zavailable;
static short availableNodes, allocedNodes, totalNodes;
static char *zalloc_base;
static void (*zerror)();

static void zallocate(char *start,int size);
static void zinsert(zmem *zp, int ndx);
static void zdelete(zmem *zp, int ndx);
static void zcoalesce(void);


// define the block of memory that the allocator will use
void malloc_init(char *start, int size)
{
        int nodes = ZALLOC_NODES;

        zalloc_addr = (int)start;
        zalloc_len = size;
	
	zalloc_base = start;
	totalNodes = nodes;
	zalloced = (zmem *)zalloc_base;
	start += sizeof(zmem) * nodes;
	zavailable = (zmem *)start;
	start += sizeof(zmem) * nodes;
	zavailable[0].start = start;
	zavailable[0].size = size;
	availableNodes = 1;
	allocedNodes = 0;
}

void malloc_error_init(void (*error_fn)())
{
    zerror = error_fn;
}

void * malloc(size_t size)
{
	int i;
	char *ret = 0;
#if 0
	extern char	_DATA__end;
#endif
	if (!zalloc_base) {
	  // return error until malloc_init is called.
	  return NULL;
	}

	size = ((size + 0xf) & ~0xf);
 
	for (i=0; i<availableNodes; i++)
	{
		// uses first possible node, doesn't try to find best fit
		if (zavailable[i].size == size)
		{
			zallocate(ret = zavailable[i].start, size);
			zdelete(zavailable, i); availableNodes--;
			goto done;
		}
		else if (zavailable[i].size > size)
		{
			zallocate(ret = zavailable[i].start, size);
			zavailable[i].start += size;
			zavailable[i].size -= size;
			goto done;
		}
	}

done:
/*	if (ret + size >= (char*)_sp()) _stop("stack clobbered"); */
	if ((ret == 0) || (ret + size >= (char *)(zalloc_addr + zalloc_len)))
		if (zerror)
		    (*zerror)(ret);
	if (ret != 0)
		bzero(ret, size);
	return (void *)ret;
}

void
free(void *pointer)
{
	int i, tsize, found = 0;
	char *start = pointer;

	if (!zalloc_base) {
	  // return until malloc_init is called.
	  return;
	}

	if (!start) return;

	for (i=0; i<allocedNodes; i++)
	{
		if (zalloced[i].start == start)
		{
			tsize = zalloced[i].size;
#if ZDEBUG
			zout -= tsize;
			printf("    zz out %d\n",zout);
#endif
			zdelete(zalloced, i); allocedNodes--;
			found = 1;
			break;
		}
	}
	if (!found) return;

	for (i=0; i<availableNodes; i++)
	{
		if ((start+tsize) == zavailable[i].start)  // merge it in
		{
			zavailable[i].start = start;
			zavailable[i].size += tsize;
			zcoalesce();
			return;
		}

		if (i>0 && (zavailable[i-1].start + zavailable[i-1].size == 
								    start))
		{
			zavailable[i-1].size += tsize;
			zcoalesce();
			return;
		}

		if ((start+tsize) < zavailable[i].start)
		{
			zinsert(zavailable, i); availableNodes++;
			zavailable[i].start = start;
			zavailable[i].size = tsize;
			return;
		}
	}

	zavailable[i].start = start;
	zavailable[i].size = tsize;
	availableNodes++;
	zcoalesce();
	return;
}


static void
zallocate(char *start,int size)
{
#if ZDEBUG
	zout += size;
	printf("    alloc %d, total 0x%x\n",size,zout);
#endif
	zalloced[allocedNodes].start = start;
	zalloced[allocedNodes].size = size;
	allocedNodes++;
}

static void
zinsert(zmem *zp, int ndx)
{
	int i;
	zmem *z1, *z2;

	i=totalNodes-2;
	z1 = zp + i;
	z2 = z1+1;

	for (; i>= ndx; i--, z1--, z2--)
	{
		z2->start = z1->start;
		z2->size = z1->size;
	}
}

static void
zdelete(zmem *zp, int ndx)
{
	int i;
	zmem *z1, *z2;

	z1 = zp + ndx;
	z2 = z1+1;

	for (i=ndx; i<totalNodes-1; i++, z1++, z2++)
	{
		z1->start = z2->start;
		z1->size = z2->size;
	}
}

static void
zcoalesce(void)
{
	int i;
	for (i=0; i<availableNodes-1; i++)
	{
		if (zavailable[i].start + zavailable[i].size == 
						zavailable[i+1].start)
		{
			zavailable[i].size += zavailable[i+1].size;
			zdelete(zavailable, i+1); availableNodes--;
			return;
		}
	}	
}

/* This is the simplest way possible.  Should fix this. */
void *realloc(void *start, size_t newsize)
{
    void *newstart;

    if (!zalloc_base) {
      // return error until malloc_init is called.
      return NULL;
    }

    newstart = malloc(newsize);
    bcopy(start, newstart, newsize);
    free(start);
    return newstart;
}


