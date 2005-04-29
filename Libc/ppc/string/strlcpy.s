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


// *****************
// * S T R L C P Y *
// *****************
//
// size_t strlcpy(char *dst, const char *src, size_t size);
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
//
// This algorithm is doubleword parallel in 64-bit mode.

        .text
        .globl EXT(strlcpy)

        .align 	5
LEXT(strlcpy)                       // size_t strlcpy(char *dst, const char *src, size_t size);
        andi.	r0,r4,GPR_BYTES-1   // is source aligned?
#if defined(__ppc__)
        lis		r6,hi16(0xFEFEFEFF)	// start to generate 32-bit magic constants
        lis		r7,hi16(0x80808080)
        ori		r6,r6,lo16(0xFEFEFEFF)
        ori		r7,r7,lo16(0x80808080)
#else
        ld		r6,_COMM_PAGE_MAGIC_FE(0)	// get 0xFEFEFEFE FEFEFEFF from commpage
        ld		r7,_COMM_PAGE_MAGIC_80(0)	// get 0x80808080 80808080 from commpage
#endif
        mr		r9,r3				// use r9 for dest ptr (must return r3 intact)
        beq		Laligned			// source is aligned
        subfic	r0,r0,GPR_BYTES     // r0 <- #bytes to align source
        
// Copy min(r0,r5) bytes, until 0-byte found.
//		r0 = #bytes we propose to copy (NOTE: must be >0)
//		r4 = source ptr (unaligned)
//		r5 = length remaining in buffer (may be 0)
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r9 = dest ptr (unaligned)

Lbyteloop:
        cmpwi	r5,0				// buffer empty?
        beq--	L0notfound			// buffer full but 0 not found
        lbz		r8,0(r4)			// r8 <- next source byte
        subic.	r0,r0,1				// decrement count of bytes to move
        addi	r4,r4,1
        subi	r5,r5,1				// decrement buffer length remaining
        stb		r8,0(r9)			// pack into dest
        cmpwi	cr1,r8,0			// 0-byte?
        addi	r9,r9,1
        beq		cr1,L0found			// byte was 0
        bne		Lbyteloop			// r0!=0, source not yet aligned
        
// Source is aligned.  Loop over words or doublewords until end of buffer.  We
// align the source, rather than the dest, to avoid getting spurious page faults.
//		r4 = source ptr (aligned)
//		r5 = length remaining in buffer
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r9 = dest ptr (unaligned)

Laligned:
        srgi.	r8,r5,LOG2_GPR_BYTES// get #words or doublewords in buffer
        addi	r0,r5,1				// if no words, compare rest of buffer
        beq--	Lbyteloop			// r8==0, no words
        mtctr	r8					// set up word loop count
        rlwinm	r5,r5,0,GPR_BYTES-1 // mask buffer length down to leftover bytes
        b		LwordloopEnter
        
// Move a word or doubleword at a time, until one of two conditions:
//		- a zero byte is found
//		- end of buffer
// At this point, registers are as follows:
//		r4 = source ptr (aligned)
//		r5 = leftover bytes in buffer (0..GPR_BYTES-1)
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r9 = dest ptr (unaligned)
//     ctr = whole words or doublewords left in buffer

        .align	5					// align inner loop, which is 8 words long
Lwordloop:
        stg		r8,0(r9)			// pack word or doubleword into destination
        addi	r9,r9,GPR_BYTES
LwordloopEnter:
        lg		r8,0(r4)			// r8 <- next 4 or 8 source bytes
        addi	r4,r4,GPR_BYTES
        add		r10,r8,r6			// r10 <-  word + 0xFEFEFEFF
        andc	r12,r7,r8			// r12 <- ~word & 0x80808080
        and.	r11,r10,r12			// r11 <- nonzero iff word has a 0-byte
        bdnzt	eq,Lwordloop		// loop if ctr!=0 and cr0_eq
        
        beq		Lleftovers			// 0-byte not found in aligned words

// Found a 0-byte.  Store last word up to and including the 0, a byte at a time.
//		r8 = last word or doubleword, known to have a 0-byte
//		r9 = dest ptr

Lstorelastbytes:
        srgi.	r0,r8,GPR_BYTES*8-8 // right justify next byte and test for 0
        slgi	r8,r8,8				// shift next byte into position
        stb		r0,0(r9)			// pack into dest
        addi	r9,r9,1
        bne		Lstorelastbytes		// loop until 0 stored
        
L0found:
        sub		r3,r9,r3			// get #bytes stored, including 0
        subi	r3,r3,1				// don't count the 0
        blr							// return strlen(src)
        
// 0-byte not found in aligned source words.  There are up to GPR_BYTES-1 leftover 
// source bytes, hopefully the 0-byte is among them.
//		r4 = source ptr (aligned)
//		r5 = leftover bytes in buffer (0..GPR_BYTES-1)
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r8 = last full word or doubleword of source
//		r9 = dest ptr (unaligned)

Lleftovers:
        stg		r8,0(r9)			// store last word or doubleword
        addi	r9,r9,GPR_BYTES
        addi	r0,r5,1				// make sure r5 terminates byte loop (not r0)
        b		Lbyteloop

// Buffer full but 0-byte not found.  Stuff a 0 into last byte of buffer.
//		r3 = start of buffer
//		r4 = ptr to next byte in source
//		r9 = ptr to first byte past end of buffer

L0notfound:
        sub.	r3,r9,r3			// get #bytes stored, ie original buffer length
        beq		Lfind0				// skip if buffer 0-length
        li		r0,0				// get a 0
        stb		r0,-1(r9)			// always store 0-byte unless buffer was 0-length
        
// Keep searching for 0-byte ending source, so we can return strlen(source).
// Not optimized, since this is an error condition.
//		r3 = number of bytes already copied
//		r4 = ptr to next byte in source

Lfind0:
        lbz		r0,0(r4)			// get next byte
        addi	r4,r4,1
        addi	r3,r3,1				// increment strlen
        cmpwi	r0,0
        bne		Lfind0				// loop if not 0
        
        subi	r3,r3,1				// don't count the 0-byte
        blr							// return strlen(source)
