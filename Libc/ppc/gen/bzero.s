/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
//
// =============================
// BZERO and MEMSET FOR Mac OS X
// =============================
//
// We use DCBZ, and therefore are dependent on the cache block size (32.)
// Bzero and memset need to be in the same file since they are tightly
// coupled, so we can use bzero for memset of 0 without incurring extra
// overhead.  (The issue is that bzero must preserve r3 for memset.)
//
// Registers we use:
//	r3  = original ptr, not changed since memset returns it
//	r4  = count of bytes to set ("rc")
//	r11 = working operand ptr ("rp")
//	r10 = value to set ("rv")

#define	rc	r4
#define	rp	r11
#define	rv	r10

#include <architecture/ppc/asm_help.h>

        .text
        .align	5
        .globl	_bzero
        .globl	_memset

// *************
// * B Z E R O *
// *************

_bzero:						// void	bzero(void *b, size_t len);
        cmplwi	cr1,rc,32	// too short for DCBZ?
        li		rv,0		// get a 0
Lbzero1:					// enter from memset with cr1 and rv set up
        neg		r5,r3		// start to compute bytes to align
        mr		rp,r3		// make copy of operand ptr
        andi.	r6,r5,0x1F	// r6 <- bytes to align on cache block
        blt-	cr1,Ltail	// <32, so skip DCBZs
        beq-	cr0,Ldcbz	// already aligned
        
        // align on 32-byte boundary
        
        mtcrf	0x01,r6		// move length to cr7 (faster if only 1 cr)
        andi.	r7,r6,16	// test bit 27 by hand
        sub		rc,rc,r6	// adjust length
        bf		31,1f		// test bits of count
        stb		rv,0(rp)
        addi	rp,rp,1
1:
        bf		30,2f
        sth		rv,0(rp)
        addi	rp,rp,2
2:
        bf		29,3f
        stw		rv,0(rp)
        addi	rp,rp,4
3:
        bf		28,4f
        stw		rv,0(rp)
        stw		rv,4(rp)
        addi	rp,rp,8
4:
        beq		Ldcbz
        stw		rv,0(rp)
        stw		rv,4(rp)
        stw		rv,8(rp)
        stw		rv,12(rp)
        addi	rp,rp,16
        
        // DCBZ 32-byte cache blocks
Ldcbz:
        srwi.	r5,rc,5		// r5 <- number of cache blocks to zero
        beq		Ltail		// none
        mtctr	r5			// set up loop count
        andi.	rc,rc,0x1F	// will there be leftovers?
1:
        dcbz	0,rp		// zero 32 bytes
        addi	rp,rp,32
        bdnz	1b
        beqlr				// no leftovers so done
        
        // store up to 31 trailing bytes
        //	rv = value to store (in all 4 bytes)
        //	rc = #bytes to store (0..31)
Ltail:
        andi.	r5,rc,16	// bit 27 set in length?
        mtcrf	0x01,rc		// low 4 bits of length to cr7
        beq		1f			// test bits of length
        stw		rv,0(rp)
        stw		rv,4(rp)
        stw		rv,8(rp)
        stw		rv,12(rp)
        addi	rp,rp,16
1:
        bf		28,2f
        stw		rv,0(rp)
        stw		rv,4(rp)
        addi	rp,rp,8
2:
        bf		29,3f
        stw		rv,0(rp)
        addi	rp,rp,4
3:
        bf		30,4f
        sth		rv,0(rp)
        addi	rp,rp,2
4:
        bflr	31
        stb		rv,0(rp)
        blr


// ***************
// * M E M S E T *
// ***************

        .align	5
_memset:					// void *   memset(void *b, int c, size_t len);
        andi.	rv,r4,0xFF	// copy value to working register, test for 0
        mr		rc,r5		// move length to working register
        cmplwi	cr1,r5,32	// length < 32 ?
        beq		Lbzero1		// memset of 0 is just a bzero
        rlwimi	rv,rv,8,16,23	// replicate value to low 2 bytes
        mr		rp,r3		// make working copy of operand ptr
        rlwimi	rv,rv,16,0,15	// value now in all 4 bytes
        blt		cr1,Ltail	// length<32, so use common tail routine
        neg		r5,rp		// start to compute #bytes to align
        andi.	r6,r5,0x7	// r6 <- #bytes to align on dw
        beq-	Lmemset1	// already aligned
        
        ; align on 8-byte boundary
        
        mtcrf	0x01,r6		// move count to cr7 (faster if only 1 cr)
        sub		rc,rc,r6	// adjust length
        bf		31,1f
        stb		rv,0(rp)
        addi	rp,rp,1
1:
        bf		30,2f
        sth		rv,0(rp)
        addi	rp,rp,2
2:
        bf		29,Lmemset1
        stw		rv,0(rp)
        addi	rp,rp,4
        
       // loop on 16-byte blocks
Lmemset1:
        stw		rv,0(rp)	// store first 8 bytes from rv
        stw		rv,4(rp)
        srwi	r5,rc,4		// r5 <- #blocks (>=1)
        mtcrf	0x01,rc		// leftover length to cr7
        mtctr	r5			// set up loop count
        lfd		f0,0(rp)	// pick up in a fp register
        b		2f			// enter loop in middle
        .align	4
1:							// loop on 16-byte blocks
        stfd	f0,0(rp)
2:
        stfd	f0,8(rp)
        addi	rp,rp,16
        bdnz	1b
        
        // store up to 16 trailing bytes (count in cr7)
        
        bf		28,3f
        stfd	f0,0(rp)
        addi	rp,rp,8
3:
        bf		29,4f
        stw		rv,0(rp)
        addi	rp,rp,4
4:
        bf		30,5f
        sth		rv,0(rp)
        addi	rp,rp,2
5:
        bflr	31
        stb		rv,0(rp)
        blr
