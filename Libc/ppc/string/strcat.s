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
// * S T R C A T *
// ***************
//
// char*	strcat(const char *dst, const char *src);
//
// We optimize the move by doing it word parallel.  This introduces
// a complication: if we blindly did word load/stores until finding
// a 0, we might get a spurious page fault by touching bytes past it.
// To avoid this, we never do a load that crosses a page boundary,
// and never store a byte we don't have to.
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
// In 64-bit mode, this algorithm is doubleword parallel.

        .text
        .globl	EXT(strcat)
        
        .align 	5
LEXT(strcat)                        // char*	strcat(const char *s, const char *append);
        clrrgi  r9,r3,LOG2_GPR_BYTES// align pointer by zeroing right LOG2_GPR_BYTES bits
        li		r10,-1				// get 0xFFs
        lg		r8,0(r9)			// get word or doubleword with 1st operand byte
        rlwinm  r11,r3,3,(GPR_BYTES-1)*8 // get starting bit position of operand
#if defined(__ppc__)
        lis		r6,hi16(0xFEFEFEFF)	// start to generate 32-bit magic constants
        lis		r7,hi16(0x80808080)
        srw		r10,r10,r11			// create a mask of 0xFF bytes for operand in r8
        ori		r6,r6,lo16(0xFEFEFEFF)
        ori		r7,r7,lo16(0x80808080)
#else
        ld		r6,_COMM_PAGE_MAGIC_FE(0)	// get 0xFEFEFEFE FEFEFEFF from commpage
        ld		r7,_COMM_PAGE_MAGIC_80(0)	// get 0x80808080 80808080 from commpage
        srd		r10,r10,r11			// create a mask of 0xFF bytes for operand in r8
#endif
        orc     r8,r8,r10           // make sure bytes preceeding operand are nonzero
        b       Lword0loopEnter
                
// Loop over words or doublewords looking for 0-byte marking end of dest.
//		r4 = source ptr (unaligned)
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r9 = dest ptr (aligned)

        .align	5					// align inner loops for speed
Lword0loop:
        lgu		r8,GPR_BYTES(r9)    // r8 <- next dest word or doubleword
Lword0loopEnter:                    // initial entry
        add		r10,r8,r6			// r10 <-  word + 0xFEFEFEFF
        andc	r12,r7,r8			// r12 <- ~word & 0x80808080
        and.	r11,r10,r12			// r11 <- nonzero iff word has a 0-byte
        beq		Lword0loop			// loop until 0 found

// Now we know one of the bytes in r8 is zero, we just have to figure out which one. 
// We have mapped 0 bytes to 0x80, and nonzero bytes to 0x00, with one exception:
// 0x01 bytes preceeding the first zero are also mapped to 0x80.   So we have to mask
// out the 0x80s caused by 0x01s before searching for the 0x80 byte.  Once the 0 is
// found, we can start appending source.  We align the source, which allows us to
// avoid worrying about spurious page faults.
//		r4 = source ptr (unaligned)
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//      r8 = word or doubleword with a 0-byte
//		r9 = ptr to the word or doubleword in r8 (aligned)
//     r11 = mapped word or doubleword

        slgi	r10,r8,7            // move 0x01 bits (false hits) into 0x80 position
        andi.	r0,r4,GPR_BYTES-1   // is source aligned?
        andc	r11,r11,r10			// mask out false hits
        cntlzg	r10,r11				// find 0 byte (r0 = 0, 8, 16, or 24)
        subfic	r0,r0,GPR_BYTES     // get #bytes to align r4
        srwi	r10,r10,3           // now r0 = 0, 1, 2, or 3
        add		r9,r9,r10			// now r9 points to the 0-byte in dest
        beq		LwordloopEnter		// skip if source is already aligned
        
        mtctr	r0					// set up loop
        
// Loop over bytes.
//		r4 = source ptr (unaligned)
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r9 = dest ptr (unaligned)
//	   ctr = byte count

Lbyteloop:
        lbz		r8,0(r4)			// r8 <- next source byte
        addi	r4,r4,1
        cmpwi	r8,0				// 0 ?
        stb		r8,0(r9)			// pack into dest
        addi	r9,r9,1
        bdnzf	eq,Lbyteloop		// loop until (ctr==0) | (r8==0)
        
        bne		LwordloopEnter		// 0-byte not found, so enter word loop
        blr							// 0-byte found, done
        
// Word loop: move a word or doubleword at a time until 0-byte found.
//		r4 = source ptr (aligned)
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r9 = dest ptr (unaligned)

        .align	5					// align inner loop, which is 8 words ling
Lwordloop:
        stg		r8,0(r9)			// pack word or doubleword into destination
        addi	r9,r9,GPR_BYTES
LwordloopEnter:
        lg		r8,0(r4)			// r8 <- next 4 or 8 source bytes
        addi	r4,r4,GPR_BYTES
        add		r10,r8,r6			// r10 <-  word + 0xFEFEFEFF
        andc	r12,r7,r8			// r12 <- ~word & 0x80808080
        and.	r0,r10,r12			// r0 <- nonzero iff word has a 0-byte
        beq		Lwordloop			// loop if no 0-byte
        
// Found a 0-byte.  Store last word up to and including the 0, a byte at a time.
//		r8 = last word or doubleword, known to have a 0-byte
//		r9 = dest ptr

Lstorelastbytes:
        srgi.   r0,r8,GPR_BYTES*8-8 // shift leftmost byte into bottom so we can "stb"
        slgi    r8,r8,8             // move on to next
        stb		r0,0(r9)			// pack into dest
        addi	r9,r9,1
        bne		Lstorelastbytes		// loop until 0 stored
        
        blr
                
