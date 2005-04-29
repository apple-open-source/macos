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
// * S T R L C A T *
// *****************
//
// size_t	strlcat(char *dst, const char *src, size_t count);
//
// We optimize the move by doing it word parallel.  This introduces
// a complication: if we blindly did word load/stores until finding
// a 0, we might get a spurious page fault by touching bytes past it.
// We are allowed to touch the "count" bytes starting at "dst", but
// when appending the "src", we must not do a "lwz" that crosses a page
// boundary, or store past "count".
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
// Note that "count" is the total buffer length, including the length
// of the "dst" string.  This is different than strncat().
//
// In 64-bit mode, this algorithm is doubleword parallel.

        .text
        .globl EXT(strlcat)

        .align 	5
LEXT(strlcat)                       // size_t strlcat(char *dst, const char *src, size_t count);
        srgi.	r0,r5,LOG2_GPR_BYTES// get #words or doublewords to scan
#if defined(__ppc__)
        lis		r6,hi16(0xFEFEFEFF)	// start to generate 32-bit magic constants
        lis		r7,hi16(0x80808080)
        ori		r6,r6,lo16(0xFEFEFEFF)
        ori		r7,r7,lo16(0x80808080)
#else
        ld		r6,_COMM_PAGE_MAGIC_FE(0)	// get 0xFEFEFEFE FEFEFEFF from commpage
        ld		r7,_COMM_PAGE_MAGIC_80(0)	// get 0x80808080 80808080 from commpage
#endif
        mr		r9,r3				// use r9 for dest ptr (r3 remembers dst start)
        beq--	L0bytes				// buffer length <4
        mtctr	r0 					// set up loop
        b		L0words				// enter word loop
        
// Loop over words looking for 0.
//		r3 = original start of buffer
//		r4 = source ptr (unaligned)
//		r5 = original buffer size
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r9 = dest ptr (unaligned)
//     ctr = #words or doublewords remaining in buffer

        .align	5					// align inner loops for speed
L0words:
        lg		r8,0(r9)			// r8 <- next dest word or doubleword
        addi	r9,r9,GPR_BYTES
        add		r10,r8,r6			// r10 <-  word + 0xFEFEFEFF
        andc	r12,r7,r8			// r12 <- ~word & 0x80808080
        and.	r11,r10,r12			// r11 <- nonzero iff word has a 0-byte
        bdnzt	eq,L0words			// loop until 0 found or buffer end
       
        beq--	L0bytes				// skip if 0 not found
        
        slgi	r0,r8,7				// move 0x01 bits (false hits) into 0x80 position
        subi	r9,r9,GPR_BYTES     // back up r9 to the start of the word
        andc	r11,r11,r0			// mask out false hits
        cntlzg	r0,r11				// find 0 byte (r0 = 0, 8, 16, or 24)
        srwi	r0,r0,3				// now r0 = 0, 1, 2, or 3
        add		r9,r9,r0			// now r9 points to the 0-byte in dest
        b		L0found				// start to append source
        
// Loop over bytes looking for 0.
//		r3 = original start of buffer
//		r4 = source ptr (unaligned)
//		r5 = original buffer size
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r9 = dest ptr (unaligned)

L0bytes:
        andi.	r0,r5,GPR_BYTES-1   // get #bytes remaining in buffer
        mtctr	r0					// set up byte loop
        beq--	L0notfound			// skip if 0 not found in buffer (error)
L0byteloop:
        lbz		r8,0(r9)			// r8 <- next dest byte
        addi	r9,r9,1
        cmpwi	r8,0				// 0 ?
        bdnzf	eq,L0byteloop		// loop until 0 found or buffer end
        
        bne--	L0notfound			// skip if 0 not found (error)
        subi	r9,r9,1				// back up, so r9 points to the 0
        
// End of dest found, so we can start appending source.  First, align the source,
// in order to avoid spurious page faults.
//		r3 = original start of buffer
//		r4 = original source ptr (unaligned)
//		r5 = original buffer size
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r9 = ptr to 0-byte in dest (unaligned)

L0found:
        andi.	r0,r4,GPR_BYTES-1   // is source aligned?
        add		r5,r5,r3			// get ptr to end of buffer
        sub		r5,r5,r9			// get #bytes remaining in buffer, counting the 0 (r5>0)
        beq		Laligned			// skip if source already word aligned
        subfic	r0,r0,GPR_BYTES     // not aligned, get #bytes to align r4
        b		Lbyteloop1			// r5!=0, so skip check
        
// Copy min(r0,r5) bytes, until 0-byte.
//		r0 = #bytes we propose to copy (NOTE: must be >0)
//		r4 = source ptr (unaligned)
//		r5 = length remaining in buffer (may be 0)
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r9 = dest ptr (unaligned)

Lbyteloop:
        cmpgi	r5,0				// buffer empty? (note: length is unsigned)
        beq--	Loverrun			// buffer filled before end of source reached
Lbyteloop1:							// entry when we know r5!=0
        lbz		r8,0(r4)			// r8 <- next source byte
        subic.	r0,r0,1				// decrement count of bytes to move
        addi	r4,r4,1
        subi	r5,r5,1				// decrement buffer length remaining
        stb		r8,0(r9)			// pack into dest
        cmpwi	cr1,r8,0			// 0-byte?
        addi	r9,r9,1
        beq		cr1,L0stored		// byte was 0, so done
        bne		Lbyteloop			// r0!=0, source not yet aligned
        
// Source is aligned.  Loop over words or doublewords until 0-byte found or end
// of buffer.
//		r3 = original start of buffer
//		r4 = source ptr (aligned)
//		r5 = length remaining in buffer
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r9 = dest ptr (unaligned)

Laligned:
        srgi.	r8,r5,LOG2_GPR_BYTES// get #words or doublewords in buffer
        addi	r0,r5,1				// if no words...
        beq--	Lbyteloop			// ...copy to end of buffer
        mtctr	r8					// set up loop count
        rlwinm	r5,r5,0,GPR_BYTES-1 // mask buffer length down to leftover bytes
        b		LwordloopEnter
        
// Inner loop: move a word or doubleword at a time, until one of two conditions:
//		- a zero byte is found
//		- end of buffer
// At this point, registers are as follows:
//		r3 = original start of buffer
//		r4 = source ptr (aligned)
//		r5 = bytes leftover in buffer (0..GPR_BYTES-1)
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r9 = dest ptr (unaligned)
//     ctr = loop count

        .align	5					// align inner loop, which is 8 words long
Lwordloop:
        stg		r8,0(r9)			// pack word into destination
        addi	r9,r9,GPR_BYTES
LwordloopEnter:
        lg		r8,0(r4)			// r8 <- next 4 or 8 source bytes
        addi	r4,r4,GPR_BYTES
        add		r10,r8,r6			// r10 <-  word + 0xFEFEFEFF
        andc	r12,r7,r8			// r12 <- ~word & 0x80808080
        and.	r11,r10,r12			// r11 <- nonzero iff word has a 0-byte
        bdnzt	eq,Lwordloop		// loop if ctr!=0 and cr0_eq
        
        beq--	Lleftovers			// skip if no 0-byte found, copy leftovers

// Found a 0-byte.  Store last word up to and including the 0, a byte at a time.
//		r3 = original start of buffer
//		r8 = last word, known to have a 0-byte
//		r9 = dest ptr (one past 0)

Lstorelastbytes:
        srgi.	r0,r8,GPR_BYTES*8-8 // right justify next byte and test for 0
        slgi	r8,r8,8				// shift next byte into position
        stb		r0,0(r9)			// pack into dest
        addi	r9,r9,1
        bne		Lstorelastbytes		// loop until 0 stored

// Append op successful, O stored into buffer.  Return total length.
//		r3 = original start of buffer
//		r9 = dest ptr (one past 0)
        
L0stored:
        sub		r3,r9,r3			// get (length+1) of string in buffer
        subi	r3,r3,1				// return length
        blr
        
// 0-byte not found in aligned source words.  There are up to GPR_BYTES-1 leftover 
// source bytes, hopefully the 0-byte is among them.
//		r4 = source ptr (aligned)
//		r5 = leftover bytes in buffer (0..GPR_BYTES-1)
//		r6 = 0xFEFEFEFF
//		r7 = 0x80808080
//		r8 = last full word or doubleword of source
//		r9 = dest ptr (unaligned)

Lleftovers:
        stg		r8,0(r9)			// store last word
        addi	r9,r9,GPR_BYTES
        addi	r0,r5,1				// make sure r5 terminates byte loop (not r0)
        b		Lbyteloop
        
// Buffer filled during append without finding the end of source.  Overwrite the
// last byte in buffer with a 0, and compute how long the concatenated string would
// have been, if the buffer had been large enough.  
//		r3 = original start of buffer
//		r4 = source ptr (1st byte not copied into buffer)
//		r9 = dest ptr (one past end of buffer)

Loverrun:
        sub.	r3,r9,r3			// compute #bytes stored in buffer
        li		r0,0				// get a 0
        beq--	Lskip				// buffer was 0-length
        stb		r0,-1(r9)			// jam in delimiting 0
        
// Buffer full, check to see how much longer source is.  We don't optimize this,
// since overruns are an error.

Lskip:
        lbz		r8,0(r4)			// get next source byte
        addi	r4,r4,1
        addi	r3,r3,1				// increment length of "ideal" string
        cmpwi	r8,0				// 0?
        bne		Lskip
        
        subi	r3,r3,1				// don't count 0 in length
        blr							// return length of string we "wanted" to create
        
// 0 not found in buffer (append not yet begun.)  We don't store a delimiting 0,
// but do compute how long the concatenated string would have been, assuming the length
// of "dst" is the length of the buffer.
//		r3 = original start of buffer
//		r4 = original source ptr
//		r9 = dest ptr (one past end of buffer)

L0notfound:
        sub		r3,r9,r3			// compute #bytes in buffer
        b		Lskip				// add strlen(source) to r3
        
