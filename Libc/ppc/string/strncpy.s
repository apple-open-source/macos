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

// *****************
// * S T R N C P Y *
// *****************
//
// char*	strncpy(const char *dst, const char *src, size_t len));
//
// We optimize the move by doing it word parallel.  This introduces
// a complication: if we blindly did word load/stores until finding
// a 0, we might get a spurious page fault by touching bytes past it.
// To avoid this, we never do a "lwz" that crosses a page boundary,
// or store unnecessary bytes.
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
        .globl EXT(strncpy)

        .align 	5
LEXT(strncpy)
        andi.	r0,r4,3				// is source aligned?
        dcbt	0,r4				// touch in source
        lis		r6,hi16(0xFEFEFEFF)	// start to load magic constants
        lis		r7,hi16(0x80808080)
        dcbtst	0,r3				// touch in dst
        ori		r6,r6,lo16(0xFEFEFEFF)
        ori		r7,r7,lo16(0x80808080)
        mr		r9,r3				// use r9 for dest ptr (must return r3 intact)
        add		r2,r3,r5			// remember where end of buffer is
        beq		Laligned			// source is aligned
        subfic	r0,r0,4				// r0 <- #bytes to word align source
        
// Copy min(r0,r5) bytes, until 0-byte.
//		r0 = #bytes we propose to copy (NOTE: must be >0)
//		r2 = ptr to 1st byte not in buffer
//		r4 = source ptr (unaligned)
//		r5 = length remaining in buffer (may be 0)
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r9 = dest ptr (unaligned)

Lbyteloop:
        cmpwi	r5,0				// buffer empty? (note: unsigned)
        beqlr--						// buffer full but 0 not found
        lbz		r8,0(r4)			// r8 <- next source byte
        subic.	r0,r0,1				// decrement count of bytes to move
        addi	r4,r4,1
        subi	r5,r5,1				// decrement buffer length remaining
        stb		r8,0(r9)			// pack into dest
        cmpwi	cr1,r8,0			// 0-byte?
        addi	r9,r9,1
        beq		cr1,L0found			// byte was 0
        bne		Lbyteloop			// r0!=0, source not yet aligned
        
// Source is word aligned.  Loop over words until end of buffer.  Note that we
// have aligned the source, rather than the dest, in order to avoid spurious
// page faults.
//		r2 = ptr to 1st byte not in buffer
//		r4 = source ptr (word aligned)
//		r5 = length remaining in buffer
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r9 = dest ptr (unaligned)

Laligned:
        srwi.	r8,r5,2				// get #words in buffer
        addi	r0,r5,1				// if no words, compare rest of buffer
        beq--	Lbyteloop			// r8==0, no words
        mtctr	r8					// set up word loop count
        rlwinm	r5,r5,0,0x3			// mask buffer length down to leftover bytes
        b		LwordloopEnter
        
// Move a word at a time, until one of two conditions:
//		- a zero byte is found
//		- end of buffer
// At this point, registers are as follows:
//		r2 = ptr to 1st byte not in buffer
//		r4 = source ptr (word aligned)
//		r5 = leftover bytes in buffer (0..3)
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r9 = dest ptr (unaligned)
//     ctr = whole words left in buffer

        .align	5					// align inner loop, which is 8 words long
Lwordloop:
        stw		r8,0(r9)			// pack word into destination
        addi	r9,r9,4
LwordloopEnter:
        lwz		r8,0(r4)			// r8 <- next 4 source bytes
        addi	r4,r4,4
        add		r10,r8,r6			// r10 <-  word + 0xFEFEFEFF
        andc	r12,r7,r8			// r12 <- ~word & 0x80808080
        and.	r11,r10,r12			// r11 <- nonzero iff word has a 0-byte
        bdnzt	eq,Lwordloop		// loop if ctr!=0 and cr0_eq
        
        stw		r8,0(r9)			// pack in last word
        addi	r9,r9,4
        addi	r0,r5,1				// if no 0-byte found...
        beq--	Lbyteloop			// ...fill rest of buffer a byte at a time

// Found a 0-byte, point to following byte with r9.
        
        slwi	r0,r8,7				// move 0x01 false hit bits to 0x80 position
        andc	r11,r11,r0			// mask out false hits
        cntlzw	r0,r11				// find the 0-byte (r0 = 0,8,16, or 24)
        srwi	r0,r0,3				// now r0 = 0, 1, 2, or 3
        subfic	r0,r0,3				// now r0 = 3, 2, 1, or 0
        sub		r9,r9,r0			// now r9 points one past the 0-byte
        
// Zero rest of buffer, if any.  We don't simply branch to bzero or memset, because
// r3 is set up incorrectly, and there is a fair amt of overhead involved in using them.
// Instead we use a simpler routine, which will nonetheless be faster unless the number
// of bytes to 0 is large and we're on a 64-bit machine.
//		r2 = ptr to 1st byte not in buffer
//		r9 = ptr to 1st byte to zero

L0found:
        sub		r5,r2,r9			// r5 <- #bytes to zero (ie, rest of buffer)
        cmplwi	r5,32				// how many?
        neg		r8,r9				// start to compute #bytes to align ptr
        li		r0,0				// get a 0
        blt		Ltail				// skip if <32 bytes
        andi.	r10,r8,31			// get #bytes to 32-byte align
        sub		r5,r5,r10			// adjust buffer length
        srwi	r11,r5,5			// get #32-byte chunks
        cmpwi	cr1,r11,0			// any chunks?
        mtctr	r11					// set up dcbz loop count
        beq		1f					// skip if already 32-byte aligned
        
// 32-byte align.  We just store 32 0s, rather than test and use conditional
// branches.

        stw		r0,0(r9)			// zero next 32 bytes
        stw		r0,4(r9)
        stw		r0,8(r9)
        stw		r0,12(r9)
        stw		r0,16(r9)
        stw		r0,20(r9)
        stw		r0,24(r9)
        stw		r0,28(r9)
        add		r9,r9,r10			// now r9 is 32-byte aligned
        beq		cr1,Ltail			// skip if no 32-byte chunks
        b		1f

// Loop doing 32-byte version of DCBZ instruction.

        .align	4					// align the inner loop
1:
        dcbz	0,r9				// zero another 32 bytes
        addi	r9,r9,32
        bdnz	1b

// Store trailing bytes.
//		r0 = 0
//		r5 = #bytes to store (<32)
//		r9 = address

Ltail:
        mtcrf	0x02,r5				// remaining byte count to cr6 and cr7
        mtcrf	0x01,r5
        bf		27,2f				// 16-byte chunk?
        stw		r0,0(r9)
        stw		r0,4(r9)
        stw		r0,8(r9)
        stw		r0,12(r9)
        addi	r9,r9,16
2:
        bf		28,4f				// 8-byte chunk?
        stw		r0,0(r9)
        stw		r0,4(r9)
        addi	r9,r9,8
4:
        bf		29,5f				// word?
        stw		r0,0(r9)
        addi	r9,r9,4
5:
        bf		30,6f				// halfword?
        sth		r0,0(r9)
        addi	r9,r9,2
6:
        bflr	31					// byte?
        stb		r0,0(r9)
        blr

        
        
