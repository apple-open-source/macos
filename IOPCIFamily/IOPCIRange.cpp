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
    range->end          = start + size;
    range->zero         = 0;
    range->alignment    = alignment ? alignment : size;
    range->minAddress   = 0;
    range->maxAddress   = 0xFFFFFFFF;
    range->allocations  = (IOPCIRange *) &range->end;
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
        IOPCIRangeInit(nextRange, type, start, size, alignment);
        nextRange->size = nextRange->proposedSize;
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

    start = 0;
    end   = 0;
    range = headRange->allocations;
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

	if (start)
		headRange->start = start;
    headRange->proposedSize = end - start;
    saving = headRange->size - headRange->proposedSize;

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
    IOPCIScalar   size, len, bestFit = 0;
	IOPCIScalar   pos, end, endPos, preferAddress = 0;
    IOPCIRange *  range = NULL;
    IOPCIRange *  relocateHead = NULL;
    IOPCIRange *  relocateLast = NULL;
    IOPCIRange ** prev;
    IOPCIRange ** where = NULL;
    IOPCIRange *  whereNext = NULL;

	if (kIOPCIRangeFlagMaximizeSize & newRange->flags)
		size = newRange->alignment;
	else
		size = newRange->proposedSize;
    if (!size) panic("!size");

    if (!newStart)
        newStart = newRange->start;

	if ((!newStart) && (newRange->maxAddress >= (1ULL << 32)))
		preferAddress = (1ULL << 32);

    for (; headRange; headRange = headRange->next)
    {
		bool splay;

        if (!headRange->size)
            continue;

        pos = newStart;
        if (pos)
        {
            // request fixed address
            end = newStart + newRange->proposedSize;
            if ((pos < headRange->start) || (end > headRange->end))
                continue;
        }
        else
        {
            // request any address
            pos = headRange->start;
            pos = IOPCIScalarAlign(pos, newRange->alignment);
            end = 0;
        }
    
        prev = &headRange->allocations;
        range = *prev;
		splay = false;
        do
        {
            if (range == newRange)
            {
				// reallocate in place - treat as free
                range = range->nextSubRange;
                continue;
            }

            if (end)
            {
                // request fixed address
                len = range->start + range->size;
                if (((pos >= range->start) && (pos < len))
                 || ((end > range->start) && (end <= len))
                 || ((pos < range->start) && (end > len)))
                {
					// overlaps existing
					if (kIOPCIRangeFlagRelocatable & range->flags)
					{
						if (!relocateHead)
							relocateHead = relocateLast = range;
						else
							relocateLast = range;
						range = range->nextSubRange;
						continue;
					}
					else
					{
						// failed
						range = NULL;
						break;
					}
                }
            }
    
			endPos = range->start;
			if (kIOPCIRangeFlagMaximizeSize & newRange->flags)
				endPos = IOPCIScalarTrunc(endPos, newRange->alignment);

            if ((pos >= newRange->minAddress)
             && ((pos + size - 1) <= newRange->maxAddress)
             && (endPos > pos))
            {
            	bool useit;
				bool done = (0 != end);

                len = endPos - pos;
                useit = (len >= size);
                if (useit)
                {
                    if (kIOPCIRangeFlagMaximizeSize & newRange->flags)
					{
						// new size to look for
						size = len;
						done = (size >= newRange->proposedSize); // big enough
						if (done) size = newRange->proposedSize;
					}
					else if (!done)
					{
						// not placed
						if (splay)
						{
							//  middle of largest free space
							useit = (len > bestFit) || (preferAddress && (pos >= preferAddress));
							if (useit)
							{
								IOPCIScalar bump;
								bestFit = len;
								bump = IOPCIScalarTrunc(len >> 1, newRange->alignment);
								if (bump < (len - size))
									pos += bump;
							}
						}
						else if (!(kIOPCIRangeFlagSplay & newRange->flags))
						{
							// least waste position
							len -= size;	// waste
							useit = ((len < bestFit) || !bestFit || (preferAddress && (pos >= preferAddress)));
							if (useit) bestFit = len;
							done = (0 == bestFit);
						}
					}
                }
                if (useit)
                {
                    // success, could queue prev->newRange->range
					where           = prev;
					whereNext       = range;
					newRange->start = pos;
					newRange->end   = pos + size;
					if (preferAddress && (pos >= preferAddress))
						preferAddress = 0;
					if (done && !preferAddress)
					{
						// use this if placed or zero waste
						break;
					}
                }
            }
            // onto next element
            if (!range->size)
            {
                // end of list
                range = NULL;
                break;
            }
            if (!end)
            {
                // next position to try
                pos = IOPCIScalarAlign(range->start + range->size, newRange->alignment);
            }
			splay = (kIOPCIRangeFlagSplay & range->flags);
            prev = &range->nextSubRange;
            range = *prev;
        }
        while(true); 
    }

    if (where)
    {
		if (relocateHead)
		{
			IOPCIRange * next = relocateHead;
			do
			{
				relocateHead = next;
				next = relocateHead->nextSubRange;
				relocateHead->nextSubRange = NULL;

				do
				{
					range = relocateHead->allocations;
					if (!range->size)
						break;
					IOPCIRangeListDeallocateSubRange(relocateHead, range);
				}
				while(true);

				relocateHead->size         = 0;
				relocateHead->start        = 0;
			}
			while (relocateHead != relocateLast);
			whereNext = next;
		}
        newRange->size         = size;
        newRange->proposedSize = size;
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
		oldRange->size = 0;
	}

    return (range != 0);
}

bool IOPCIRangeAppendSubRange(IOPCIRange ** list,
                            IOPCIRange * newRange )
{
    bool result = false;

    IOPCIRange ** prev;
    IOPCIRange *  range;

    prev = list;
    do
    {
        range = *prev;
        if (!range)
            break;
        if (range->start && ((range->start < newRange->start) || !newRange->start))
            continue;
        if (newRange->start && !range->start)
            break;
        if (newRange->alignment > range->alignment)
            break;
    }
    while (prev = &range->nextSubRange, true);

    *prev = newRange;
    newRange->nextSubRange = range;
    result = true;

    return (result);
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
    }


    IOPCIRangeListAddRange(&head, 0, 0x80000000, 1024*1024, 1024*1024);

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
