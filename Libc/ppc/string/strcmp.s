/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
#define	ASSEMBLER
#include <mach/ppc/asm.h>
#undef	ASSEMBLER

#define	__APPLE_API_PRIVATE
#include <machine/cpu_capabilities.h>
#undef	__APPLE_API_PRIVATE

/* We use mode-independent "g" opcodes such as "srgi".  These expand
 * into word operations when targeting __ppc__, and into doubleword
 * operations when targeting __ppc64__.
 */
#include <architecture/ppc/mode_independent_asm.h>


// ***************
// * S T R C M P *
// ***************
//
// int	strcmp(const char *s1, const char *s2);
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
//
// In 64-bit mode, the algorithm is doubleword parallel.
	
        .text
        .globl EXT(strcmp)

        .align 	5
LEXT(strcmp)                        // int	strcmp(const char *s1, const char *s2);
        andi.	r0,r3,GPR_BYTES-1   // is LHS aligned?
#if defined(__ppc__)
        lis		r5,hi16(0xFEFEFEFF)	// start to generate 32-bit magic constants
        lis		r6,hi16(0x80808080)
        ori		r5,r5,lo16(0xFEFEFEFF)
        ori		r6,r6,lo16(0x80808080)
#else
        ld		r5,_COMM_PAGE_MAGIC_FE(0)	// get 0xFEFEFEFE FEFEFEFF from commpage
        ld		r6,_COMM_PAGE_MAGIC_80(0)	// get 0x80808080 80808080 from commpage
#endif
        subi	r3,r3,GPR_BYTES     // we use "lgu" in the inner loops
        subi	r4,r4,GPR_BYTES
        beq		Laligned			// LHS is aligned
        subfic	r0,r0,GPR_BYTES     // r0 <- #bytes to align LHS
        mtctr	r0
        
// Loop over bytes.

Lbyteloop:
        lbz		r7,GPR_BYTES(r3)    // r7 <- next LHS byte
        addi	r3,r3,1
        lbz		r8,GPR_BYTES(r4)    // r8 <- next RHS byte
        addi	r4,r4,1
        cntlzw	r9,r7				// is r7 zero?
        sub		r0,r7,r8			// different?
        srwi	r9,r9,5				// r9 <- (r7==0) ? 1 : 0
        or.		r9,r9,r0			// r9 is nonzero if either different or 0
        bdnzt	eq,Lbyteloop		// loop until different, 0, or buf end
        
        bne		Ldone				// done if different or 0
        
// LHS is aligned.  If RHS also is, we need not worry about page
// crossing.  Otherwise, we must stop the loop before page is crossed.

Laligned:
        andi.	r0,r4,GPR_BYTES-1   // is RHS now aligned too?
        addi	r9,r4,GPR_BYTES     // restore true address of next RHS byte
        rlwinm	r9,r9,0,0xFFF		// get RHS offset in page
        beq		Lalignedloop		// RHS also aligned, use simple loop
        subfic	r9,r9,4096			// get #bytes left in RHS page
        srwi.	r0,r9,LOG2_GPR_BYTES// get #words or doublewords left in RHS page
        mtctr	r0					// set up loop count
        bne++	Lunalignedloop		// at least one word left in RHS page
        li		r0,GPR_BYTES        // must check GPR_BYTES, a byte at a time...
        mtctr	r0					// ...in order to keep LHS aligned
        b		Lbyteloop			// go cross RHS page
        
// Unaligned inner loop: compare a word or doubleword at a time, until one of
// three conditions:
//		- a difference is found
//		- a zero byte is found
//		- end of RHS page (we dare not touch next page until we must)
// At this point, registers are as follows:
//		r3 = LHS ptr - GPR_BYTES (aligned)
//		r4 = RHS ptr - GPR_BYTES (not aligned)
//		r5 = 0xFEFEFEFF
//		r6 = 0x80808080
//     ctr = words or doublewords left in RHS page

        .align	5					// align inner loop, which is 8 words long
Lunalignedloop:
        lgu     r7,GPR_BYTES(r3)    // r7 <- next LHS bytes
        lgu     r8,GPR_BYTES(r4)    // r8 <- next RHS bytes
        add		r10,r7,r5			// r10 <- LHS + 0xFEFEFEFF
        andc	r12,r6,r7			// r12 <- ~LHS & 0x80808080
        xor		r11,r7,r8			// r11 <- compare the words
        and		r0,r10,r12			// r0 <- nonzero iff LHS has a 0-byte
        or.		r12,r0,r11			// combine difference and 0-test vectors
        bdnzt	eq,Lunalignedloop	// loop if ctr!=0 and cr0_eq
        
        bne++	Ldifferent			// done if we found a 0 or difference
        li		r0,GPR_BYTES        // must check GPR_BYTES, a byte at a time...
        mtctr	r0					// ...in order to keep LHS word aligned
        b		Lbyteloop			// cross RHS page, then resume word loop
        
// Aligned inner loop: compare a word at a time, until one of two conditions:
//		- a difference is found
//		- a zero byte is found
// At this point, registers are as follows:
//		r3 = LHS ptr - 4 (word aligned)
//		r4 = RHS ptr - 4 (word aligned)
//		r5 = 0xFEFEFEFF
//		r6 = 0x80808080

        .align	5					// align inner loop, which is 8 words ling
Lalignedloop:
        lgu     r7,GPR_BYTES(r3)    // r7 <- next LHS bytes
        lgu     r8,GPR_BYTES(r4)    // r8 <- next RHS bytes
        add		r10,r7,r5			// r10 <- LHS + 0xFEFEFEFF
        andc	r12,r6,r7			// r12 <- ~LHS & 0x80808080
        xor		r11,r7,r8			// r11 <- compare the words
        and		r0,r10,r12			// r0 <- nonzero iff LHS has a 0-byte
        or.		r12,r0,r11			// combine difference and 0-test vectors
        beq		Lalignedloop		// loop if neither found
        
// Found differing bytes and/or a 0-byte.  Determine which comes first, and
// subtract the bytes to compute the return value.  We also need to mask out the
// false hits in the 0-byte test, which consist of 0x01 bytes that preceed
// the 0-byte.

Ldifferent:							// r0 == 0-test vector (with 0x01 false hits)
        slgi	r9,r7,7				// move 0x01 bits in LHS into position 0x80
        andc	r0,r0,r9			// mask out the false 0-hits from 0x01 bytes
        or		r11,r11,r0			// recompute difference vector
        cntlzg	r9,r11				// find 1st difference (r9 = 0..31 or 63)
        rlwinm	r9,r9,0,0x38		// byte align bit offset (now, r9 = 0,8,16, or 24 etc)
        addi	r0,r9,8				// now, r0 = 8, 16, 24, or 32
#if defined(__ppc__)
        rlwnm	r7,r7,r0,24,31		// right justify differing bytes and mask off rest
        rlwnm	r8,r8,r0,24,31
#else
        rldcl   r7,r7,r0,56         // right justify differing bytes and mask off rest
        rldcl   r8,r8,r0,56
#endif

Ldone:                              // differing bytes are in r7 and r8
        sub		r3,r7,r8			// compute difference (0, +, or -)
        blr
        
