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

// *****************
// * S T R N C A T *
// *****************
//
// char*	strncat(char *dst, const char *src, size_t count);
//
// We optimize the move by doing it word parallel.  This introduces
// a complication: if we blindly did word load/stores until finding
// a 0, we might get a spurious page fault by touching bytes past it.
// To avoid this, we never do a "lwz" that crosses a page boundary,
// or store extra bytes.
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
// Note that "count" refers to the max number of bytes to _append_.
// There is no limit to the number of bytes we will scan looking for
// the end of the "dst" string.

        .text
        .globl EXT(strncat)

        .align 	5
LEXT(strncat)
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
        mtctr	r0					// set up byte loop
        
// Loop over bytes looking for 0-byte marking end of dest, until dest is
// word aligned.
//		r4 = source ptr (unaligned)
//		r5 = count (unchanged so far)
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r9 = dest ptr (unaligned)
//	   ctr = byte count

Lbyte0loop:
        lbz		r8,0(r9)			// r8 <- next dest byte
        addi	r9,r9,1
        cmpwi	r8,0				// test for 0
        bdnzf	eq,Lbyte0loop		// loop until (ctr==0) | (r8==0)
        
        bne		Lword0loop			// haven't found 0, so enter word-aligned loop
        andi.	r0,r4,3				// is source aligned?
        subi	r9,r9,1				// point to the 0-byte we just stored
        beq		Laligned			// source is already aligned
        subfic	r0,r0,4				// r0 <- #bytes to word align source
        b		Lbyteloop			// must align source
        
// Loop over words looking for 0-byte marking end of dest.
//		r4 = source ptr (unaligned)
//		r5 = count (unchanged so far)
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
       
        slwi	r10,r8,7			// move 0x01 bits (false hits) into 0x80 position
        andi.	r0,r4,3				// is source aligned?
        andc	r11,r11,r10			// mask out false hits
        subi	r9,r9,4				// back up r9 to the start of the word
        cntlzw	r10,r11				// find 0 byte (r0 = 0, 8, 16, or 24)
        srwi	r10,r10,3			// now r10 = 0, 1, 2, or 3
        add		r9,r9,r10			// now r9 points to the 0-byte in dest
        beq		Laligned			// skip if source already aligned
        subfic	r0,r0,4				// r0 <- #bytes to word align source
        
// Copy min(r0,r5) bytes, until 0-byte.
//		r0 = #bytes we propose to copy (NOTE: must be >0)
//		r4 = source ptr (unaligned)
//		r5 = length remaining in buffer (may be 0)
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r9 = dest ptr (unaligned)

Lbyteloop:
        cmpwi	r5,0				// buffer empty? (note: unsigned)
        beq--	L0notfound			// buffer full but 0 not found
        lbz		r8,0(r4)			// r8 <- next source byte
        subic.	r0,r0,1				// decrement count of bytes to move
        addi	r4,r4,1
        subi	r5,r5,1				// decrement buffer length remaining
        stb		r8,0(r9)			// pack into dest
        cmpwi	cr1,r8,0			// 0-byte?
        addi	r9,r9,1
        beqlr	cr1					// byte was 0, so done
        bne		Lbyteloop			// r0!=0, source not yet aligned
        
// Source is word aligned.  Loop over words until 0-byte found or end
// of buffer.
//		r4 = source ptr (word aligned)
//		r5 = length remaining in buffer
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r9 = dest ptr (unaligned)

Laligned:
        srwi.	r8,r5,2				// get #words in buffer
        addi	r0,r5,1				// if no words, copy rest of buffer
        beq--	Lbyteloop			// fewer than 4 bytes in buffer
        mtctr	r8					// set up word loop count
        rlwinm	r5,r5,0,0x3			// mask buffer length down to leftover bytes
        b		LwordloopEnter
        
// Inner loop: move a word at a time, until one of two conditions:
//		- a zero byte is found
//		- end of buffer
// At this point, registers are as follows:
//		r4 = source ptr (word aligned)
//		r5 = bytes leftover in buffer (0..3)
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
        
        beq--	LcheckLeftovers		// skip if 0-byte not found

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
        
// 0-byte not found while appending words to source.  There might be up to
// 3 "leftover" bytes to append, hopefully the 0-byte is in there.
//		r4 = source ptr (past word in r8)
//		r5 = bytes leftover in buffer (0..3)
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r8 = last word of source, with no 0-byte
//		r9 = dest ptr (unaligned)

LcheckLeftovers:
        stw		r8,0(r9)			// store last whole word of source
        addi	r9,r9,4
        addi	r0,r5,1				// let r5 (not r0) terminate byte loop
        b		Lbyteloop			// append last few bytes

// 0-byte not found in source.  We append a 0 anyway, even though it will
// be past the end of the buffer.  That's the way it's defined.
//		r9 = dest ptr

L0notfound:
        li		r0,0
        stb		r0,0(r9)			// add a 0, past end of buffer
        blr

