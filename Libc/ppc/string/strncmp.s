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

// *****************
// * S T R N C M P *
// *****************
//
// int	strncmp(const char *s1, const char *s2, size_t len);
//
// We optimize the compare by doing it word parallel.  This introduces
// a complication: if we blindly did word loads from both sides until
// finding a difference (or 0), we might get a spurious page fault by
// reading bytes past the difference.  To avoid this, we never do a "lwz"
// that crosses a page boundary.
//
// The test for 0s relies on the following inobvious but very efficient
// word-parallel test:
//		x =  dataWord + 0xFEFEFEFF
//		y = ~dataWord & 0x80808080
//		if (x & y) == 0 then no zero found
// The test maps any non-zero byte to zero, and any zero byte to 0x80,
// with one exception: 0x01 bytes preceeding the first zero are also
// mapped to 0x80.

        .text
        .globl EXT(strncmp)

        .align 	5
LEXT(strncmp)							// int strncmp(const char *s1,const char *s2,size_t len);
        cmplwi	cr1,r5,8			// is buffer too short to bother with word compares?
        andi.	r0,r3,3				// is LHS aligned?
        dcbt	0,r3				// touch in LHS
        subi	r3,r3,4				// we use "lwzu" in the word inner loop
        subi	r4,r4,4
        blt		cr1,Lshort			// short buffer, just compare a byte at a time
        lis		r2,hi16(0xFEFEFEFF)	// start to load magic constants
        lis		r6,hi16(0x80808080)
        ori		r2,r2,lo16(0xFEFEFEFF)
        ori		r6,r6,lo16(0x80808080)
        beq		Laligned			// LHS is aligned
        subfic	r0,r0,4				// r0 <- #bytes to word align LHS
        mtctr	r0					// set up for byte loop
        sub		r5,r5,r0			// adjust length
        b		Lbyteloop
        
// Handle short operands or end-of-buffer.
//		r3 = LHS ptr - 4 (unaligned)
//		r4 = RHS ptr - 4 (unaligned)
//		r5 = length remaining in buffer (0..7)
//	   cr1 = blt set

Lshort:
        cmpwi	r5,0				// buffer null?
        mtctr	r5					// assume not null, set up for loop
        bne		Lbyteloop			// buffer not null
        li		r3,0				// if buffer null, say "equal"
        blr

// We're at a RHS page boundary.  Compare 4 bytes in order to cross the page
// but still keep the LHS ptr word-aligned.
//		r2 = 0xFEFEFEFF
//		r3 = LHS ptr - 4 (aligned)
//		r4 = RHS ptr - 4 (unaligned)
//		r5 = length remaining in buffer (may be 0)
//		r6 = 0x80808080

Lcrosspage:
        cmplwi	cr1,r5,8			// not enough left in buffer for word compares?
        li		r0,4				// get #bytes to cross RHS page
        blt		cr1,Lshort			// buffer is about to end
        mtctr	r0					// set up to compare 4 bytes
        sub		r5,r5,r0			// adjust length
        b		Lbyteloop
        
// Compare bytes, until 0-byte or difference found.
//		r2 = 0xFEFEFEFF (if cr1 bge)
//		r3 = LHS ptr - 4 (unaligned)
//		r4 = RHS ptr - 4 (unaligned)
//		r5 = length remaining in buffer (may be 0)
//		r6 = 0x80808080 (if cr1 bge)
//	   cr1 = blt if this is end of buffer

        .align	5					// align inner loop, which is 8 words long
Lbyteloop:
        lbz		r7,4(r3)			// next LHS byte
        addi	r3,r3,1
        lbz		r8,4(r4)			// next RHS byte
        addi	r4,r4,1
        cmpwi	cr0,r7,0			// zero?
        cmpw	cr7,r7,r8			// equal?
        crandc	cr0_eq,cr7_eq,cr0_eq// set cr0_eq if equal and not 0
        bdnzt	eq,Lbyteloop		// loop until different, 0, or (ctr==0)
                
        bne		Ldifferent			// done if bytes differ or are 0
        blt		cr1,Ldifferent		// done if buffer end (ie, if r5==0)
        
// LHS is now word aligned.  Loop over words until end of RHS page or buffer.
// When we get to the end of the page, we compare 4 bytes, so that we keep
// the LHS word aligned.
//		r2 = 0xFEFEFEFF
//		r3 = LHS ptr - 4 (aligned)
//		r4 = RHS ptr - 4 (unaligned)
//		r5 = length remaining in buffer (may be 0)
//		r6 = 0x80808080

Laligned:
        addi	r9,r4,4				// restore true address of next RHS byte
        rlwinm	r9,r9,0,0xFFF		// get RHS offset in page
        subfic	r0,r9,4096			// get #bytes left in RHS page
        subfc	r7,r0,r5			// ***
        subfe	r8,r5,r5			// * r9 <- min(r0,r5),
        and		r7,r7,r8			// * using algorithm in Compiler Writer's Guide
        add		r9,r0,r7			// ***
        srwi.	r8,r9,2				// get #words we can compare
        beq--	Lcrosspage			// no words so advance to next RHS page
        slwi	r9,r8,2				// convert #words to #bytes
        mtctr	r8					// set up loop count
        sub		r5,r5,r9			// decrement length remaining
        b		Lwordloop
        
// Inner loop: compare a word at a time, until one of three conditions:
//		- a difference is found
//		- a zero byte is found
//		- end of count (ie, end of buffer or RHS page, whichever is first)
// At this point, registers are as follows:
//		r2 = 0xFEFEFEFF
//		r3 = LHS ptr - 4 (aligned)
//		r4 = RHS ptr - 4 (unaligned)
//		r5 = length remaining in buffer (may be 0)
//		r6 = 0x80808080
//     ctr = count of words until end of buffer or RHS page

        .align	5					// align inner loop, which is 8 words long
Lwordloop:
        lwzu	r7,4(r3)			// r7 <- next 4 LHS bytes
        lwzu	r8,4(r4)			// r8 <- next 4 RHS bytes
        add		r10,r7,r2			// r10 <- LHS + 0xFEFEFEFF
        andc	r12,r6,r7			// r12 <- ~LHS & 0x80808080
        xor		r11,r7,r8			// r11 <- compare the words
        and		r9,r10,r12			// r9 <- nonzero iff LHS has a 0-byte
        or.		r12,r9,r11			// combine difference and 0-test vectors
        bdnzt	eq,Lwordloop		// loop if ctr!=0 and cr0_eq
        
        beq--	Lcrosspage			// skip if buffer or page end reached
        
// Found differing bytes and/or a 0-byte.  Determine which comes first, and
// subtract the bytes to compute the return value.  We also need to mask out the
// false hits in the 0-byte test, which consist of 0x01 bytes that preceed
// the 0-byte.

        slwi	r0,r7,7				// move 0x01 bits in LHS into position 0x80
        andc	r9,r9,r0			// mask out the false 0-hits from 0x01 bytes
        or		r11,r11,r9			// recompute difference vector
        cntlzw	r0,r11				// find 1st difference (r0 = 0..31)
        rlwinm	r9,r0,0,0x18		// byte align bit offset (r9 = 0,8,16, or 24)
        addi	r0,r9,8				// now, r0 = 8, 16, 24, or 32
        rlwnm	r7,r7,r0,24,31		// right justify differing bytes and mask off rest
        rlwnm	r8,r8,r0,24,31

Ldifferent:							// bytes in r7 and r8 differ or are 0
        sub		r3,r7,r8			// compute return value
        blr
        
