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
#define	ASSEMBLER
#include <mach/ppc/asm.h>
#undef	ASSEMBLER

// ***************
// * S T R C A T *
// ***************
//
// char*	strcat(const char *dst, const char *src);
//
// We optimize the move by doing it word parallel.  This introduces
// a complication: if we blindly did word load/stores until finding
// a 0, we might get a spurious page fault by touching bytes past it.
// To avoid this, we never do a "lwz" that crosses a page boundary,
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

        .text
        .globl	EXT(strcat)
        
        .align 	5
LEXT(strcat)					// char*	strcat(const char *s, const char *append);
        andi.	r0,r3,3				// is dst aligned?
        dcbtst	0,r3				// touch in dst
        lis		r6,hi16(0xFEFEFEFF)	// start to load magic constants
        lis		r7,hi16(0x80808080)
        dcbt	0,r4				// touch in source
        ori		r6,r6,lo16(0xFEFEFEFF)
        ori		r7,r7,lo16(0x80808080)
        mr		r9,r3				// use r9 for dest ptr (must return r3 intact)
        beq		Lword0loop			// dest is aligned
        subfic	r0,r0,4				// r0 <- #bytes to word align dest
        mtctr	r0
        
// Loop over bytes looking for 0-byte marking end of dest.
//		r4 = source ptr (unalaigned)
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r9 = dest ptr (unaligned)
//	   ctr = byte count

Lbyte0loop:
        lbz		r8,0(r9)			// r8 <- next dest byte
        addi	r9,r9,1
        cmpwi	r8,0				// test for 0
        bdnzf	eq,Lbyte0loop		// loop until (ctr==0) | (r8==0)
        
        bne		Lword0loop			// enter word loop if we haven't found the 0-byte
        subi	r9,r9,1				// point to 0-byte
        b		L0found				// start to append the source
        
// Loop over words looking for 0-byte marking end of dest.
//		r4 = source ptr (unalaigned)
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r9 = dest ptr (word aligned)

        .align	5					// align inner loops for speed
Lword0loop:
        lwz		r8,0(r9)			// r8 <- next dest word
        addi	r9,r9,4
        add		r10,r8,r6			// r10 <-  word + 0xFEFEFEFF
        andc	r12,r7,r8			// r12 <- ~word & 0x80808080
        and.	r11,r10,r12			// r11 <- nonzero iff word has a 0-byte
        beq		Lword0loop			// loop until 0 found
       
        slwi	r0,r8,7				// move 0x01 bits (false hits) into 0x80 position
        subi	r9,r9,4				// back r9 up to beginning of word
        andc	r11,r11,r0			// mask out false hits
        cntlzw	r0,r11				// find 0 byte (r0 = 0, 8, 16, or 24)
        srwi	r0,r0,3				// now r0 = 0, 1, 2, or 3
        add		r9,r9,r0			// now r9 points to the 0-byte in dest
        
// End of dest found, so we can start appending source.
// We align the _source_, which allows us to avoid all worries about
// spurious page faults.  Doing so is faster than aligning the dest.
//		r4 = source ptr (unaligned)
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r9 = ptr to 0-byte (unaligned)

L0found:
        andi.	r0,r4,3				// is source aligned?
        beq		LwordloopEnter		// skip if so
        subfic	r0,r0,4				// not aligned, get #bytes to align r4
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
        
// Word loop: move a word at a time until 0-byte found.
//		r4 = source ptr (word aligned)
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r9 = dest ptr (unaligned)

        .align	5					// align inner loop, which is 8 words ling
Lwordloop:
        stw		r8,0(r9)			// pack word into destination
        addi	r9,r9,4
LwordloopEnter:
        lwz		r8,0(r4)			// r8 <- next 4 source bytes
        addi	r4,r4,4
        add		r10,r8,r6			// r10 <-  word + 0xFEFEFEFF
        andc	r12,r7,r8			// r12 <- ~word & 0x80808080
        and.	r0,r10,r12			// r0 <- nonzero iff word has a 0-byte
        beq		Lwordloop			// loop if ctr!=0 and cr0_eq
        
// Found a 0-byte.  Store last word up to and including the 0, a byte at a time.
//		r8 = last word, known to have a 0-byte
//		r9 = dest ptr

Lstorelastbytes:
        srwi.	r0,r8,24			// right justify next byte and test for 0
        slwi	r8,r8,8				// shift next byte into position
        stb		r0,0(r9)			// pack into dest
        addi	r9,r9,1
        bne		Lstorelastbytes		// loop until 0 stored
        
        blr
                
