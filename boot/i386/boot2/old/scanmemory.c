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
#import <mach/boolean.h>

#import <mach/i386/vm_types.h>
#import <architecture/i386/cpu.h>

/*
 * Primatives for manipulating the
 * cpu cache(s).
 */

static __inline__
void
enable_cache(void)
{
    cr0_t	cr0;
    
    asm volatile(
    	"mov %%cr0,%0"
	    : "=r" (cr0));
    
    cr0.cd = cr0.nw = 0;

    asm volatile(
    	"wbinvd; mov %0,%%cr0"
	    :
	    : "r" (cr0));
}

static __inline__
void
flush_cache(void)
{
    asm volatile("wbinvd");
}

/*
 * Memory sizing code.
 *
 * Tunable Parameters:
 *	SCAN_INCR	Size of memory scanning segment.
 *	SCAN_LEN	Length to actually test, per segment,
 *			starting at the front of the segment.
 *	SCAN_LIM	Highest address to test for existence of memory.
 *
 * Assumptions:
 *	Primary assumption is that SCAN_INCR and SCAN_LEN are chosen
 *	and the stack is positioned in its segment such that the tested
 *	portion of the segment does not overlap with any portion of the
 *	stack's	location in that segment.  The best way to acomplish this
 *	is to position the stack at the high end of a segment, and to make
 *	the segment size large enough to prevent it from overlapping the
 *	test area.  The stack needs to be large enough to contain the low 
 *	memory save area as well as the code for the scan function.
 */
#define KB(x)		(1024*(x))
#define MB(x)		(1024*KB(x))

#define SCAN_PAT0	0x76543210
#define SCAN_PAT1	0x89abcdef

struct test_datum {
    unsigned int	word0;
    unsigned int	word1;
};

vm_offset_t
scan_memory(
    vm_offset_t			end_of_memory,
    vm_offset_t			end_of_cnvmem,
    unsigned int		SCAN_INCR,
    unsigned int		SCAN_LEN,
    unsigned int		SCAN_LIM
)
{
    struct test_datum		zero_pat = { 0, 0 };
    vm_offset_t			memory;
    
    /*
     * Make sure that the cache(s) are flushed
     * and enabled.
     */
    enable_cache();

    /*
     * Round the starting address to the next
     * segment boundary.  This is where we will
     * begin testing.
     */
    end_of_memory = (end_of_memory + (SCAN_INCR - 1) & ~(SCAN_INCR - 1));

    /*
     * Zero out the test area of each segent
     * which is located in extended memory.
     */
    memory = KB(1024);
    
    while (memory < end_of_memory) {
	struct test_datum	*memory_ptr;
	
	(vm_offset_t)memory_ptr = memory;
	
	while ((vm_offset_t)memory_ptr < memory + SCAN_LEN)
	    *memory_ptr++ = zero_pat;
	    
	memory += SCAN_INCR;
    }

    {
	/*
	 * Code for segment scanning function.
	 */
	extern unsigned int	Scan_segment_code[],
				Scan_segment_code_end[];
	/*
	 * Location on the stack to where this
	 * function is copied and then executed
	 * from!!  N.B. This code must be position
	 * independent. (duh)
	 */
	unsigned int	scan_func[
				Scan_segment_code_end -
					    Scan_segment_code];

	/*
	 * Copy the scan function onto the stack.
	 */
	memcpy(scan_func, Scan_segment_code, sizeof (scan_func));

	while (end_of_memory < SCAN_LIM) {
	    display_kbytes(end_of_memory);
	    if (!((vm_offset_t (*)())scan_func)(
					    end_of_memory,
					    end_of_cnvmem,
					    SCAN_INCR,
					    SCAN_LEN))
		break;
	    
	    end_of_memory += SCAN_INCR;
	}
    }
    
    display_kbytes(end_of_memory);

    return (end_of_memory);
}

static
void
display_kbytes(
    vm_offset_t		address
)
{
    unsigned int	quant, dig, done = 0, mag = 1000000000;
    int			places = 1;

    quant = address / 1024;
    
    while (mag > 0) {
    	done *= 10;
    	dig = (quant / mag) - done;
	done += dig;
	if (done > 0 || mag == 1) {
	    putc(dig + '0');
	    places++;
	}
	mag /= 10;
    }
    
    putc('K');
    
    while (places-- > 0)
    	putc('\b');
}

/*
 * Memory scan function, which tests one segment of memory.
 * This code is copied onto the stack and executed there to
 * avoid problems when memory aliasing occurs.  If it detects
 * problems due to aliasing to low memory, it restores the
 * low memory segments before returning.
 *
 * Parameters:
 *	end_of_memory		Address to start testing at,
 *				this is rounded to the start of
 *				the next segment internally.
 *	end_of_cnvmem		Address where conventional
 *				memory ends.
 *	SCAN_INCR		Size of each segment.
 *	SCAN_LEN		Size of per segment test area,
 *				located at the front of the segment.
 *	SCAN_LIM		Address next segment after highest
 *				to test. 
 */
static
boolean_t
scan_segment(
    vm_offset_t			start_of_segment,
    vm_offset_t			end_of_cnvmem,
    unsigned int		SCAN_INCR,
    unsigned int		SCAN_LEN
)
{
    /*
     * Location on the stack where the test
     * area of each segment of low memory is
     * saved, appended together.  The copy is
     * used to detect memory aliasing and to
     * restore memory on that occasion.
     */
    unsigned int	copy_area[
				    ((KB(640) / SCAN_INCR) * SCAN_LEN)
					    / sizeof (unsigned int)];
    struct test_datum	*test_ptr,
			test_pat = { SCAN_PAT0, SCAN_PAT1 },
			zero_pat = { 0, 0 };
    vm_offset_t		memory, copy;

    /*
     * Copy the test area of each low memory
     * segment to the save area.  Low memory
     * begins at zero, and runs to the end of
     * conventional memory.
     */
    copy = (vm_offset_t)copy_area;
    memory = 0;

    while (memory < KB(640)) {
	unsigned int	*memory_ptr, *copy_ptr;

	if (memory <= (end_of_cnvmem - SCAN_LEN)) {
	    (vm_offset_t)memory_ptr = memory;
	    (vm_offset_t)copy_ptr = copy;

	    while ((vm_offset_t)memory_ptr < memory + SCAN_LEN)
		*copy_ptr++ = *memory_ptr++;
	}
	
	memory += SCAN_INCR; copy += SCAN_LEN;
    }

    /*
     * Write the test pattern in the test
     * area of the current segment.
     */
    (vm_offset_t)test_ptr = start_of_segment;

    while ((vm_offset_t)test_ptr < start_of_segment + SCAN_LEN)
	*test_ptr++ = test_pat;

    /*
     * Flush the data cache to insure that the
     * data actually gets written to main memory.
     * This will provoke aliasing to occur if
     * it is in fact present.
     */
    flush_cache();

    /*
     * Compare low memory against the save
     * area, breaking out immediately if
     * an inconsistency is observed.
     */
    copy = (vm_offset_t)copy_area;
    memory = 0;

    while (memory < KB(640)) {
	struct test_datum	*memory_ptr, *copy_ptr;
	
	if (memory <= (end_of_cnvmem - SCAN_LEN)) {
	    (vm_offset_t)memory_ptr = memory;
	    (vm_offset_t)copy_ptr = copy;

	    while ((vm_offset_t)memory_ptr < memory + SCAN_LEN) {
		if (	memory_ptr->word0 != copy_ptr->word0	||
			memory_ptr->word1 != copy_ptr->word1	)
		    break;
		    
		memory_ptr++; copy_ptr++;
	    }

	    if ((vm_offset_t)memory_ptr < memory + SCAN_LEN)
		break;
	}

	memory += SCAN_INCR; copy += SCAN_LEN;
    }

    /*
     * If an inconsistency was found in low
     * memory, restore the entire region from
     * the save area and return a failure.
     */
    if (memory < KB(640)) {
	copy = (vm_offset_t)copy_area;
	memory = 0;
	
	while (memory < KB(640)) {
	    unsigned int	*memory_ptr, *copy_ptr;
	
	    if (memory <= (end_of_cnvmem - SCAN_LEN)) {
		(vm_offset_t)memory_ptr = memory;
		(vm_offset_t)copy_ptr = copy;
		
		while ((vm_offset_t)memory_ptr < memory + SCAN_LEN)
		    *memory_ptr++ = *copy_ptr++;
	    }

	    memory += SCAN_INCR; copy += SCAN_LEN;
	}
	
	return (FALSE);
    }

    /*
     * Check the memory we have already scanned
     * to see whether aliasing occurred there.
     * The test area of each segment should contain
     * zeros.
     */
    memory = KB(1024);
    
    while (memory < start_of_segment) {
	struct test_datum	*memory_ptr;
	
	(vm_offset_t)memory_ptr = memory;
	
	while ((vm_offset_t)memory_ptr < memory + SCAN_LEN) {
	    if (	memory_ptr->word0 != zero_pat.word0	||
			memory_ptr->word1 != zero_pat.word1	)
		break;

	    memory_ptr++;
	}
	
	if ((vm_offset_t)memory_ptr < memory + SCAN_LEN)
	    break;
	    
	memory += SCAN_INCR;
    }

    if (memory < start_of_segment)
	return (FALSE);

    /*
     * Now check the current segment to see
     * whether the test patten was correctly
     * written out.
     */
    (vm_offset_t)test_ptr = start_of_segment;
    
    while ((vm_offset_t)test_ptr < start_of_segment + SCAN_LEN) {
	if (	test_ptr->word0 != test_pat.word0	||
		test_ptr->word1 != test_pat.word1	)
	    break;

	test_ptr++;
    }
    
    if ((vm_offset_t)test_ptr < start_of_segment + SCAN_LEN)
	return (FALSE);

    /*
     * Zero the current segment, which has now
     * passed the test!!
     */
    (vm_offset_t)test_ptr = start_of_segment;

    while ((vm_offset_t)test_ptr < start_of_segment + SCAN_LEN)
	*test_ptr++ = zero_pat;
	
    return (TRUE);
}
