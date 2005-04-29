/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/* We use mode-independent "g" opcodes such as "srgi".  These expand
 * into word operations when targeting __ppc__, and into doubleword
 * operations when targeting __ppc64__.
 */
#include <architecture/ppc/mode_independent_asm.h>

#include <mach/ppc/asm.h>

#define	__APPLE_API_PRIVATE
#include <machine/cpu_capabilities.h>
#undef	__APPLE_API_PRIVATE


// Strlen, optimized for PPC.  We use an inobvious but very efficient
// word-parallel test for 0-bytes: 
// 
//	y =  dataWord + 0xFEFEFEFF
//	z = ~dataWord & 0x80808080
//	if ( y & z ) = 0 then all bytes in dataWord are non-zero
//
// The test maps any non-zero byte to zeros and any zero byte to 0x80,
// with one exception: 0x01 bytes preceeding the first zero are also
// mapped to 0x80.  Using altivec is another possibility, but it turns
// out that the overhead of maintaining VRSAVE and dealing with edge
// cases pushes the crossover point out to around 30 bytes... longer
// the the "typical" operand length.
//
// In 64-bit mode, the algorithm is doubleword parallel.

        .text
        .align	5
        .globl	EXT(strlen)
LEXT(strlen)                        // int	strlen(ptr)
        clrrgi  r9,r3,LOG2_GPR_BYTES// align pointer by zeroing right LOG2_GPR_BYTES bits
        li		r7,-1				// get 0xFFs
        lg		r8,0(r9)			// get word or doubleword with 1st operand byte
        rlwinm  r4,r3,3,(GPR_BYTES-1)*8 // get starting bit position of operand
#if defined(__ppc__)
        lis		r5,hi16(0xFEFEFEFF)	// start to generate 32-bit magic constants
        lis		r6,hi16(0x80808080)
        srw		r7,r7,r4			// create a mask of 0xFF bytes for operand in r8
        ori		r5,r5,lo16(0xFEFEFEFF)
        ori		r6,r6,lo16(0x80808080)
#else
        ld		r5,_COMM_PAGE_MAGIC_FE(0)	// get 0xFEFEFEFE FEFEFEFF from commpage
        ld		r6,_COMM_PAGE_MAGIC_80(0)	// get 0x80808080 80808080 from commpage
        srd		r7,r7,r4			// create a mask of 0xFF bytes for operand in r8
#endif
        orc		r8,r8,r7			// make sure bytes preceeding operand are 0xFF
        b		Lloop1				// enter loop
        
// Loop over words or doublewords.
//		r3 = original address
//		r5 = 0xFEFEFEFE FEFEFEFF
//		r6 = 0x80808080 80808080
//		r9 = address (aligned)

        .align	5
Lloop:
        lgu		r8,GPR_BYTES(r9)    // get next word or doubleword
Lloop1:								// initial entry
        add		r4,r5,r8			// r4 =  data + 0xFEFEFEFF
        andc	r7,r6,r8			// r7 = ~data & 0x80808080
        and.	r4,r4,r7			// r4 = r4 & r7
        beq		Lloop				// if r4 is zero, then all bytes are non-zero

// Now we know one of the bytes in r8 is zero, we just have to figure out which one. 
// We have mapped 0 bytes to 0x80, and nonzero bytes to 0x00, with one exception:
// 0x01 bytes preceeding the first zero are also mapped to 0x80.   So we have to mask
// out the 0x80s caused by 0x01s before searching for the 0x80 byte.

        slgi	r5,r8,7				// move 0x01 bits to 0x80 position
        sub		r3,r9,r3			// start to compute string length
        andc	r4,r4,r5			// turn off false hits from 0x0100 worst case
        cntlzg	r7,r4				// now we can count leading 0s
        srwi	r7,r7,3				// convert 0,8,16,24 to 0,1,2,3, etc
        add		r3,r3,r7			// add in nonzero bytes in last word
        blr
