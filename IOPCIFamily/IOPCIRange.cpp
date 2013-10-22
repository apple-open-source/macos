/*
 * Copyright (c) 2010-2010 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 2.0 (the
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

#ifdef KERNEL

#include <IOKit/pci/IOPCIPrivate.h>
#include <IOKit/pci/IOPCIConfigurator.h>

#else

/*
cc IOPCIRange.cpp -o /tmp/pcirange -Wall -framework IOKit -framework CoreFoundation -arch i386 -g -lstdc++
*/

#include <assert.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOKitKeys.h>

#include "IOKit/pci/IOPCIConfigurator.h"
#define panic(x) printf(x)

#endif

IOPCIScalar IOPCIScalarAlign(IOPCIScalar num, IOPCIScalar alignment)
{
    return (num + (alignment - 1) & ~(alignment - 1));
}

IOPCIScalar IOPCIScalarTrunc(IOPCIScalar num, IOPCIScalar alignment)
{
    return (num & ~(alignment - 1));
}

IOPCIRange * IOPCIRangeAlloc(void)
{
#ifdef KERNEL
    return (IONew(IOPCIRange, 1));
#else
    return ((IOPCIRange *) malloc(sizeof(IOPCIRange)));
#endif
}

void IOPCIRangeFree(IOPCIRange * range)
{
//  memset(range, 0xBB, sizeof(*range));
#ifdef KERNEL
    IODelete(range, IOPCIRange, 1);
#else
    free(range);
#endif
}

void IOPCIRangeInit(IOPCIRange * range, uint32_t type,
                    IOPCIScalar start, IOPCIScalar size, IOPCIScalar alignment)
{
    bzero(range, sizeof(*range));
    range->type         = type;
    range->start        = start;
    range->size         = 0;
    range->proposedSize = size;
    range->end          = start;
    range->zero         = 0;
    range->alignment    = alignment ? alignment : size;
    range->minAddress   = 0;
    range->maxAddress   = 0xFFFFFFFF;
    range->allocations  = (IOPCIRange *) &range->end;
}

void IOPCIRangeInitAlloc(IOPCIRange * range, uint32_t type,
                         IOPCIScalar start, IOPCIScalar size, IOPCIScalar alignment)
{
    IOPCIRangeInit(range, type, start, size, alignment);
    range->size = range->proposedSize;
    range->end  = range->start + range->size;
}

bool IOPCIRangeListAddRange(IOPCIRange ** rangeList,
                            uint32_t type,
                            IOPCIScalar start,
                            IOPCIScalar size,
                            IOPCIScalar alignment)
{
    IOPCIRange *  range;
    IOPCIRange *  nextRange;
    IOPCIRange ** prev;
    IOPCIScalar   end;
    bool          result = true;
    bool          alloc  = true;

    end = start + size;
    for (prev = rangeList; (range = *prev); prev = &range->next)
    {
        if (((start >= range->start) && (start < range->end))
         || ((end > range->start) && (end <= range->end))
         || ((start < range->start) && (end > range->end)))
        {
            range = NULL;
            result = false;
            break;
        }
        if (end == range->start)
        {
            range->start        = start;
            range->size         = range->end - range->start;
            range->proposedSize = range->size;
            alloc = false;
            break;
        }
        if (start == range->end)
        {
            if ((nextRange = range->next) && (nextRange->start == end))
            {
                if (nextRange->allocations != (IOPCIRange *) &nextRange->end)
                    assert(false);
                end = nextRange->end;
                range->next = nextRange->next;
                IOPCIRangeFree(nextRange);
            }
            range->end          = end;
            range->size         = end - range->start;
            range->proposedSize = range->size;
            alloc = false;
            break;
        }
        if (range->start > end)
        {
            alloc = true;
            break;
        }
    }

    if (result && alloc)
    {
        nextRange = IOPCIRangeAlloc();
        IOPCIRangeInitAlloc(nextRange, type, start, size, alignment);
        nextRange->next = range;
        *prev = nextRange;
    }

    return (result);
}

IOPCIScalar IOPCIRangeListCollapse(IOPCIRange * headRange, IOPCIScalar alignment)
{
    IOPCIScalar total = 0;

    while (headRange)
    {
        total += IOPCIRangeCollapse(headRange, alignment);
        headRange = headRange->next;
    }

    return (total);
}

IOPCIScalar IOPCIRangeCollapse(IOPCIRange * headRange, IOPCIScalar alignment)
{
    IOPCIScalar   start, end, saving;
    IOPCIRange *  range;

    start  = 0;
    end    = 0;
    saving = headRange->size;
    range  = headRange->allocations;
    do
    {
        // keep walking down the list
        if (!range->size)
            break;
        if (!start)
            start = range->start;
        end = range->end;
        range = range->nextSubRange;
    }
    while(true); 

    start = IOPCIScalarTrunc(start, alignment);
    end   = IOPCIScalarAlign(end,   alignment);

	if (!start) headRange->proposedSize = 0;
	else
	{
		headRange->start = start;
		headRange->end   = end;
		headRange->size  = headRange->proposedSize = end - start;
	}
	if (saving < headRange->proposedSize) panic("IOPCIRangeCollapse");
    saving -= headRange->proposedSize;

    return (saving);
}

IOPCIScalar IOPCIRangeListLastFree(IOPCIRange * headRange, IOPCIScalar align)
{
	IOPCIRange * next;
	next = headRange;
    while (next)
    {
		headRange = next;
        next = next->next;
    }
	return (IOPCIRangeLastFree(headRange, align));
}

IOPCIScalar IOPCIRangeLastFree(IOPCIRange * headRange, IOPCIScalar align)
{
    IOPCIScalar   last;
    IOPCIRange *  range;

    range = headRange->allocations;
    last  = headRange->start;
    do
    {
        // keep walking down the list
        if (!range->size)
            break;
        last = range->end;
        range = range->nextSubRange;
    }
    while(true);

	last = IOPCIScalarAlign(last, align);
	if (headRange->end > last)
		last = headRange->end - last;
	else
		last = 0;

    return (last);
}

bool IOPCIRangeListAllocateSubRange(IOPCIRange * headRange,
                                    IOPCIRange * newRange,
                                    IOPCIScalar  newStart)
{
    IOPCIScalar   minSize, maxSize;
    IOPCIScalar   len, bestFit, waste;
	IOPCIScalar   pos, endPos;
    IOPCIRange ** where = NULL;
    IOPCIRange *  whereNext = NULL;
	IOPCIRange *  range = NULL;
	IOPCIRange ** prev;
	bool 		  splay;

	minSize = newRange->size;
	if (!minSize) minSize = newRange->proposedSize;
	if (!minSize) panic("!minSize");
	if (!newStart) newStart = newRange->start;

	bestFit = 0;
    for (; headRange; headRange = headRange->next)
    {
        if (!headRange->size) continue;

		maxSize = newRange->proposedSize;
		if (kIOPCIRangeFlagMaximizeSize & newRange->flags)
		{
			IOPCIScalar max;
			max = headRange->size;
			if (headRange->count) max = ((max * headRange->pri) / headRange->count);
			if (maxSize < max) maxSize = max;
			maxSize = IOPCIScalarTrunc(maxSize, newRange->alignment);
		}
		if (!maxSize) panic("!maxSize");

        pos  = headRange->start;
        prev = &headRange->allocations;
        range = NULL;
		splay = false;
        do
        {
			if (range)
			{
				// onto next element
				if (!range->size)
				{
					// end of list
					range = NULL;
					break;
				}
				pos = range->end;
				splay = (kIOPCIRangeFlagSplay & range->flags);
				prev = &range->nextSubRange;
			}
            range = *prev;
            if (range == newRange)
            {
				// reallocate in place - treat as free
                range = range->nextSubRange;
            }
			endPos = range->start;

			// [pos,endPos] is free
			waste = endPos - pos;
			if (pos > newRange->maxAddress)    pos = newRange->maxAddress;
			if (endPos > newRange->maxAddress) endPos = newRange->maxAddress;
			if (newStart)
			{
				if (newStart < pos)     continue;
				if (newStart >= endPos) continue;
				pos = newStart;
			}
			else pos = IOPCIScalarAlign(pos, newRange->alignment);
			if (kIOPCIRangeFlagMaximizeSize & newRange->flags)
				endPos = IOPCIScalarTrunc(endPos, newRange->alignment);

			if (endPos < pos) continue;

			len = endPos - pos;
			if (len < minSize) continue;
			if (len > maxSize) len = maxSize;

			if (newStart
				|| (where && (newRange->start < newRange->minAddress) && (pos >= newRange->minAddress))
				|| (len < maxSize)
				|| (kIOPCIRangeFlagMaximizeSize & newRange->flags))
			{
				// new size to look for
				minSize = len;
			}
			else
			{
				if (splay)
				{
					// furthest possible
					if (where && (waste < bestFit)) continue;

					IOPCIScalar bump;
					bump = IOPCIScalarTrunc(((waste - len) >> 1), newRange->alignment);
					pos += bump;
				}
				else if (!(kIOPCIRangeFlagSplay & newRange->flags))
				{
					// least waste position
					waste -= len;
					if (where && (bestFit < waste)) continue;
				}
			}
			// best candidate, will queue prev->newRange->range
			bestFit = waste;

			where           = prev;
			whereNext       = range;
			newRange->start = pos;
			newRange->size  = len;
			newRange->end   = pos + len;

			if (newStart || !waste)
			{
				// use this if placed or zero waste
				break;
			}
        }
        while(true); 
    }

    if (where)
    {
		if (newRange->size > newRange->proposedSize) newRange->proposedSize = newRange->size;
        newRange->nextSubRange = whereNext;
        *where = newRange;
    }

    return (where != NULL);
}

bool IOPCIRangeListDeallocateSubRange(IOPCIRange * headRange,
                                	  IOPCIRange * oldRange)
{
    IOPCIRange *  range = NULL;
    IOPCIRange ** prev = NULL;

    do
    {
		range = oldRange->allocations;
        if (!range->size)
            break;
        IOPCIRangeListDeallocateSubRange(oldRange, range);
    }
    while(true); 

    for (range = NULL;
         headRange && !range; 
         headRange = headRange->next)
    {
        prev = &headRange->allocations;
        do
        {
            range = *prev;
            if (range == oldRange)
                break;
            // keep walking down the list
            if (!range->size)
            {
                range = NULL;
                break;
            }
        }
        while (prev = &range->nextSubRange, true);
    }

    if (range)
    {
        *prev = range->nextSubRange;
		oldRange->nextSubRange = NULL;
		oldRange->end          = 0;
//		oldRange->size         = 0;
	}

    return (range != 0);
}

#ifndef KERNEL
#define kprintf(fmt, args...)  printf(fmt, ## args)
#endif

void IOPCIRangeDump(IOPCIRange * head)
{
    IOPCIRange * range;
    uint32_t idx;
    
    do
    {
        kprintf("head.start     0x%llx\n", head->start);
        kprintf("head.size      0x%llx\n", head->size);
        kprintf("head.end       0x%llx\n", head->end);
        kprintf("head.alignment 0x%llx\n", head->alignment);
    
        kprintf("allocs:\n");
        range = head->allocations;
        idx = 0;
        while (true)
        {
            if (range == (IOPCIRange *) &head->end)
            {
                kprintf("[end]\n");
                break;
            }
            kprintf("[%d].start     0x%llx\n", idx, range->start);
            kprintf("[%d].size      0x%llx\n", idx, range->size);
            kprintf("[%d].end       0x%llx\n", idx, range->end);
            kprintf("[%d].alignment 0x%llx\n", idx, range->alignment);
            idx++;
            range = range->nextSubRange;
        }
        kprintf("------\n");
    }
    while ((head = head->next));

    kprintf("------------------------------------\n");
}

#ifndef KERNEL

int main(int argc, char **argv)
{
    IOPCIRange * head = NULL;
    IOPCIRange * range;
    IOPCIRange * requests = NULL;
    IOPCIRange * elems[8];
    IOPCIScalar  shrink;
    size_t       idx;
    bool         ok;

    for (idx = 0; idx < sizeof(elems) / sizeof(elems[0]); idx++)
    {
        elems[idx] = IOPCIRangeAlloc();
        elems[idx]->maxAddress = 0xFFFFFFFFFFFFFFFFULL;
    }

#if 0
    IOPCIRangeListAddRange(&head, 0, 0,0, 1024*1024);
    IOPCIRangeDump(head);
    shrink = IOPCIRangeListLastFree(head, 1024*1024);
    printf("IOPCIRangeListLastFree 0x%llx\n", shrink);
exit(0);
#endif

#if 0
    IOPCIRangeListAddRange(&head, 0, 0xa0800000, 0x500000, 0x100000);

	range = elems[0];
	IOPCIRangeInit(range, 0, 0xa0800000, 0x40000, 0x40000);
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[1];
	IOPCIRangeInit(range, 0, 0xa0a00000, 0x400000, 0x100000);
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range->proposedSize = 0x300000;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);
//	ok = IOPCIRangeListDeallocateSubRange(head, range);
//	assert(ok);
    IOPCIRangeDump(head);

    exit(0);
#endif

    IOPCIRangeListAddRange(&head, 0, 0x6, 0xfa, 1);
    IOPCIRangeDump(head);

	range = elems[4];
	range->start = 0x06;
	range->proposedSize = 0x01;
	range->alignment = 1;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[3];
	range->start = 0x07;
	range->proposedSize = 0x01;
	range->alignment = 1;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[1];
	range->start = 1*0x87;
	range->size = 3;
	range->proposedSize = 3;
	range->alignment = 1;
	range->flags = kIOPCIRangeFlagSplay;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[2];
	range->start = 0;
	range->proposedSize = 2;
	range->alignment = 1;
	range->flags = kIOPCIRangeFlagSplay;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);


	range = elems[5];
	range->start = 0;
	range->proposedSize = 1;
	range->alignment = 1;
	range->flags = kIOPCIRangeFlagSplay;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);

	range = elems[6];
	range->start = 0;
	range->proposedSize = 1;
	range->alignment = 1;
	range->flags = kIOPCIRangeFlagSplay;
	ok = IOPCIRangeListAllocateSubRange(head, range);
	assert(ok);


    IOPCIRangeDump(head);
	exit(0);



    shrink = IOPCIRangeListLastFree(head, 1024);
    printf("IOPCIRangeListLastFree 0x%llx\n", shrink);
    exit(0);


    idx = 0;
    IOPCIRangeInit(elems[idx++], 0, 0, 1024*1024);

    range = elems[0];
    ok = IOPCIRangeListAllocateSubRange(head, range);
    printf("alloc(%d) [0x%llx, 0x%llx]\n", ok, range->start, range->size);
    IOPCIRangeDump(head);

    shrink = IOPCIRangeListCollapse(head, 1024*1024);
    printf("Collapsed by 0x%llx\n", shrink);
    IOPCIRangeDump(head);
    exit(0);


    IOPCIRangeListAddRange(&head, 0, 0xA0000000, 0x10000000);
    IOPCIRangeListAddRange(&head, 0, 0x98000000, 0x08000000);
    IOPCIRangeListAddRange(&head, 0, 0x90000000, 0x08000000);
    IOPCIRangeListAddRange(&head, 0, 0xB0000000, 0x10000000);

//exit(0);


    idx = 0;
    IOPCIRangeInit(elems[idx++], 0, 0x80001000, 0x1000);
    IOPCIRangeInit(elems[idx++], 0, 0, 0x1000000);
    IOPCIRangeInit(elems[idx++], 0, 0, 0x20000000);
    IOPCIRangeInit(elems[idx++], 0, 0, 0x20000000);
    IOPCIRangeInit(elems[idx++], 0, 0x80002000, 0x800);
    IOPCIRangeInit(elems[idx++], 0, 0, 0x10000, 1);

    for (idx = 0; idx < sizeof(elems) / sizeof(elems[0]); idx++)
    {
        if (elems[idx]->size)
            IOPCIRangeAppendSubRange(&requests, elems[idx]);
    }

    printf("reqs:\n");
    range = requests;
    idx = 0;
    while (range)
    {
        printf("[%ld].start     0x%llx\n", idx, range->start);
        printf("[%ld].size      0x%llx\n", idx, range->size);
        printf("[%ld].end       0x%llx\n", idx, range->end);
        printf("[%ld].alignment 0x%llx\n", idx, range->alignment);
        idx++;
        range = range->nextSubRange;
    }

    while ((range = requests))
    {
        requests = range->nextSubRange;
        ok = IOPCIRangeListAllocateSubRange(head, range);
        printf("alloc(%d) [0x%llx, 0x%llx]\n", ok, range->start, range->size);
    }

    IOPCIRangeDump(head);
    shrink = IOPCIRangeListCollapse(head, 1024*1024);
    printf("Collapsed by 0x%llx\n", shrink);
    IOPCIRangeDump(head);
exit(0);

    for (idx = 0; idx < sizeof(elems) / sizeof(elems[0]); idx++)
    {
        range = elems[idx];
        if (range->size && range->start)
        {
            ok = IOPCIRangeListDeallocateSubRange(head, range);
            printf("dealloc(%d) [0x%llx, 0x%llx]\n", ok, range->start, range->size);
            ok = IOPCIRangeListAllocateSubRange(head, range);
            printf("alloc(%d) [0x%llx, 0x%llx]\n", ok, range->start, range->size);
        }
    }

    // extend
    range = elems[5];
    range->proposedSize = 2 * range->size;
    ok = IOPCIRangeListAllocateSubRange(head, range);
    printf("extalloc(%d) [0x%llx, 0x%llx]\n", ok, range->start, range->size);
    range = elems[0];
    range->proposedSize = 2 * range->size;
    ok = IOPCIRangeListAllocateSubRange(head, range);
    printf("extalloc(%d) [0x%llx, 0x%llx]\n", ok, range->start, range->size);
    range->proposedSize = 2 * range->size;
    ok = IOPCIRangeListAllocateSubRange(head, range, range->start - range->size);
    printf("extalloc(%d) [0x%llx, 0x%llx]\n", ok, range->start, range->size);

    IOPCIRangeDump(head);

    exit(0);    
}

#endif
