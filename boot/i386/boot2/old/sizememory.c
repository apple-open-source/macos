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
#import <mach/i386/vm_types.h>

#define KB(x)		(1024*(x))
#define MB(x)		(1024*KB(x))

unsigned int
sizememory(
    unsigned int	cnvmem
)
{
    vm_offset_t		end_of_memory;
#define	SCAN_INCR	KB(64)
#define	SCAN_LEN	8
#define SCAN_LIM	MB(512)

    printf("\nSizing memory... ");
    
    if (readKeyboardShiftFlags() & 0x2) { /* left SHIFT key depressed */
    	printf("[aborted]");
	end_of_memory = KB(memsize(1)) + MB(1);
    }
    else {
	/*
	 * First scan beginning at start of
	 * extended memory using a reasonably
	 * large segment size.
	 */
	end_of_memory = scan_memory(
					KB(1024),
					KB(cnvmem),
					SCAN_INCR,
					SCAN_LEN,
					SCAN_LIM);

	/*
	 * Now scan the top segment a page at
	 * a time to find the actual end of
	 * extended memory.
	 */
	if (end_of_memory > KB(1024))
	    end_of_memory = scan_memory(
					    end_of_memory - SCAN_INCR,
					    KB(cnvmem),
					    KB(4),
					    SCAN_LEN,
					    end_of_memory);
    }
   
    return (end_of_memory /  1024);
}
