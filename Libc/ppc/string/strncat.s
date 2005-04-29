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

/* We use mode-independent "g" opcodes such as "srgi".  These expand
 * into word operations when targeting __ppc__, and into doubleword
 * operations when targeting __ppc64__.
 */
#include <architecture/ppc/mode_independent_asm.h>

#define	__APPLE_API_PRIVATE
#include <machine/cpu_capabilities.h>
#undef	__APPLE_API_PRIVATE


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
//
// In 64-bit mode, this algorithm is doubleword parallel.

        .text
        .globl EXT(strncat)

        .align 	5
LEXT(strncat)                       // char* strncat(char *dst, const char *src, size_t count);
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
//		r5 = count (unchanged so far)
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r9 = dest ptr (aligned)

        .align	5					// align inner loops for speed
Lword0loop:
        lgu		r8,GPR_BYTES(r9)    // r8 <- next dest word or doubleword
Lword0loopEnter:
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
//		r5 = count (unchanged so far)
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//      r8 = word or doubleword with a 0-byte
//		r9 = ptr to the word or doubleword in r8 (aligned)
//     r11 = mapped word or doubleword
       
        slgi	r10,r8,7			// move 0x01 bits (false hits) into 0x80 position
        andi.	r0,r4,GPR_BYTES-1   // is source aligned?
        andc	r11,r11,r10			// mask out false hits
        cntlzg	r10,r11				// find 0 byte (r0 = 0, 8, 16, or 24)
        subfic	r0,r0,GPR_BYTES     // get #bytes to align r4
        srwi	r10,r10,3			// now r10 = 0, 1, 2, or 3
        add		r9,r9,r10			// now r9 points to the 0-byte in dest
        beq		Laligned			// skip if source already aligned
        
// Copy min(r0,r5) bytes, until 0-byte.
//		r0 = #bytes we propose to copy (NOTE: must be >0)
//		r4 = source ptr (unaligned)
//		r5 = length remaining in buffer (may be 0)
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r9 = dest ptr (unaligned)

Lbyteloop:
        cmpgi	r5,0				// buffer empty? (note: count is unsigned)
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
        
// Source is aligned.  Loop over words or doublewords until 0-byte found
// or end of buffer.
//		r4 = source ptr (aligned)
//		r5 = length remaining in buffer
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r9 = dest ptr (unaligned)

Laligned:
        srgi.	r8,r5,LOG2_GPR_BYTES// get #words or doublewords in buffer
        addi	r0,r5,1				// if no words, copy rest of buffer
        beq--	Lbyteloop			// fewer than 4 bytes in buffer
        mtctr	r8					// set up word loop count
        rlwinm	r5,r5,0,GPR_BYTES-1 // mask buffer length down to leftover bytes
        b		LwordloopEnter
        
// Inner loop: move a word or doubleword at a time, until one of two conditions:
//		- a zero byte is found
//		- end of buffer
// At this point, registers are as follows:
//		r4 = source ptr (aligned)
//		r5 = bytes leftover in buffer (0..GPR_BYTES-1)
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r9 = dest ptr (unaligned)
//     ctr = whole words or doublewords left in buffer

        .align	5					// align inner loop, which is 8 words long
Lwordloop:
        stg		r8,0(r9)			// pack word or doubleword into destination
        addi	r9,r9,GPR_BYTES
LwordloopEnter:
        lg		r8,0(r4)			// r8 <- next GPR_BYTES source bytes
        addi	r4,r4,GPR_BYTES
        add		r10,r8,r6			// r10 <-  word + 0xFEFEFEFF
        andc	r12,r7,r8			// r12 <- ~word & 0x80808080
        and.	r11,r10,r12			// r11 <- nonzero iff word has a 0-byte
        bdnzt	eq,Lwordloop		// loop if ctr!=0 and cr0_eq
        
        beq--	LcheckLeftovers		// skip if 0-byte not found

// Found a 0-byte.  Store last word up to and including the 0, a byte at a time.
//		r8 = last word or doubleword, known to have a 0-byte
//		r9 = dest ptr

Lstorelastbytes:
        srgi.	r0,r8,GPR_BYTES*8-8 // right justify next byte and test for 0
        slgi	r8,r8,8				// shift next byte into position
        stb		r0,0(r9)			// pack into dest
        addi	r9,r9,1
        bne		Lstorelastbytes		// loop until 0 stored
        
        blr
        
// 0-byte not found while appending words to source.  There might be up to
// GPR_BYTES-1 "leftover" bytes to append, hopefully the 0-byte is in there.
//		r4 = source ptr (past word in r8)
//		r5 = bytes leftover in buffer (0..GPR_BYTES-1)
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r8 = last word or doubleword of source, with no 0-byte
//		r9 = dest ptr (unaligned)

LcheckLeftovers:
        stg		r8,0(r9)			// store last whole word or doubleword of source
        addi	r9,r9,GPR_BYTES
        addi	r0,r5,1				// let r5 (not r0) terminate byte loop
        b		Lbyteloop			// append last few bytes

// 0-byte not found in source.  We append a 0 anyway, even though it will
// be past the end of the buffer.  That's the way it's defined.
//		r9 = dest ptr

L0notfound:
        li		r0,0
        stb		r0,0(r9)			// add a 0, past end of buffer
        blr

