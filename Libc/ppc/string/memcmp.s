/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
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
#define	ASSEMBLER		// we need the defs for cr7_eq etc
#include <mach/ppc/asm.h>
#undef	ASSEMBLER

// ***************     ***********
// * M E M C M P * and * B C M P *
// ***************     ***********
//
// int	memcmp(const char *s1, const char *s2, size_t len);
// int	  bcmp(const char *s1, const char *s2, size_t len);
//
// Bcmp returns (+,0,-), whereas memcmp returns the true difference
// between the first differing bytes, but we treat them identically.
//
// We optimize the compare by doing it word parallel.  This introduces
// a complication: if we blindly did word loads from both sides until
// finding a difference, we might get a spurious page fault by
// reading bytes past the difference.  To avoid this, we never do a "lwz"
// that crosses a page boundary.

        .text
        .globl EXT(memcmp)
        .globl EXT(bcmp)

        .align 	5
LEXT(memcmp)							// int memcmp(const char *s1,const char *s2,size_t len);
LEXT(bcmp)							// int   bcmp(const char *s1,const char *s2,size_t len);
        cmplwi	cr1,r5,8			// is buffer too short to bother with word compares?
        andi.	r0,r3,3				// is LHS word aligned?
        blt		cr1,Lshort			// short buffer, so just compare byte-by-byte
        beq		Laligned			// skip if aligned
        subfic	r0,r0,4				// r0 <- #bytes to word align LHS
        mtctr	r0					// set up for byte loop
        b		Lbyteloop
        
// Handle short buffer or end-of-buffer.
//		r3 = LHS ptr (unaligned)
//		r4 = RHS ptr (unaligned)
//		r5 = length remaining in buffer (0..7)

Lshort:
        cmpwi	r5,0				// null buffer?
        mtctr	r5					// assume not null, and set up for loop
        bne		Lshortloop			// buffer not null
        li		r3,0				// say "equal"
        blr
        
        .align	5
Lshortloop:
        lbz		r7,0(r3)			// next LHS byte
        addi	r3,r3,1
        lbz		r8,0(r4)			// next RHS byte
        addi	r4,r4,1
        cmpw	r7,r8				// compare the bytes
        bdnzt	eq,Lshortloop		// loop if more to go and bytes are equal

        sub		r3,r7,r8			// generate return value
        blr 

// We're at a RHS page boundary.  Compare 4 bytes in order to cross the
// page but still keep the LHS ptr word-aligned.

Lcrosspage:
        cmplwi	r5,8				// enough bytes left to use word compares?
        li		r0,4				// get #bytes to cross RHS page
        blt		Lshort				// buffer is about to end
        mtctr	r0					// set up to compare 4 bytes
        b		Lbyteloop
        
// Compare byte-by-byte.
//		r3 = LHS ptr (unaligned)
//		r4 = RHS ptr (unaligned)
//		r5 = length remaining in buffer (must be >0)
//	   ctr = bytes to compare

        .align	5
Lbyteloop:
        lbz		r7,0(r3)			// next LHS byte
        addi	r3,r3,1
        lbz		r8,0(r4)			// next RHS byte
        addi	r4,r4,1
        subi	r5,r5,1				// decrement bytes remaining in buffer
        cmpw	r7,r8				// compare the bytes
        bdnzt	eq,Lbyteloop		// loop if more to go and bytes are equal
        
        bne		Ldifferent			// done if we found differing bytes
                
// LHS is now word aligned.  Loop over words until end of RHS page or buffer.
// When we get to the end of the page, we compare 4 bytes, so that we keep
// the LHS word aligned.
//		r3 = LHS ptr (aligned)
//		r4 = RHS ptr (unaligned)
//		r5 = length remaining in buffer (>= 4 bytes)

Laligned:
        rlwinm	r9,r4,0,0xFFF		// get RHS offset in page
        subfic	r0,r9,4096			// get #bytes left in RHS page
        subfc	r7,r0,r5			// ***
        subfe	r8,r5,r5			// * r9 <- min(r0,r5),
        and		r7,r7,r8			// * using algorithm in Compiler Writer's Guide
        add		r9,r0,r7			// ***
        srwi.	r8,r9,2				// get #words we can compare
        rlwinm	r9,r9,0,0,29		// get #bytes we will compare word-parallel
        beq--	Lcrosspage			// we're at a RHS page boundary
        mtctr	r8					// set up loop count
        sub		r5,r5,r9			// decrement length remaining
        b		Lwordloop
        
// Compare a word at a time, until one of two conditions:
//		- a difference is found
//		- end of count (ie, end of buffer or RHS page, whichever is first)
// At this point, registers are as follows:
//		r3 = LHS ptr (aligned)
//		r4 = RHS ptr (unaligned)
//		r5 = length remaining in buffer (may be 0)
//     ctr = count of words until end of buffer or RHS page

        .align	5					// align inner loop, which is 8 words long
Lwordloop:
        lwz		r7,0(r3)			// r7 <- next 4 LHS bytes
        addi	r3,r3,4
        lwz		r8,0(r4)			// r8 <- next 4 RHS bytes
        addi	r4,r4,4
        xor.	r11,r7,r8			// compare the words
        bdnzt	eq,Lwordloop		// loop if ctr!=0 and cr0_eq
        
        beq--	Lcrosspage			// skip if buffer or page end reached
        
// Found differing bytes.

        cntlzw	r0,r11				// find 1st difference (r0 = 0..31)
        rlwinm	r9,r0,0,0x18		// byte align bit offset (r9 = 0,8,16, or 24)
        addi	r0,r9,8				// now, r0 = 8, 16, 24, or 32
        rlwnm	r7,r7,r0,24,31		// right justify differing bytes and mask off rest
        rlwnm	r8,r8,r0,24,31

Ldifferent:							// bytes in r7 and r8 differ
        sub		r3,r7,r8			// compute return value
        blr
                
