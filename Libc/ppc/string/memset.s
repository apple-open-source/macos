/*
 * Copyright (c) 2000-2004 Apple Computer, Inc. All rights reserved.
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

#include <machine/cpu_capabilities.h>

/* We use mode-independent "g" opcodes such as "srgi".  These expand
 * into word operations when targeting __ppc__, and into doubleword
 * operations when targeting __ppc64__.
 */
#include <architecture/ppc/mode_independent_asm.h>


        .text
#define kShort  128             // threshold for calling commpage


/* ***************
 * * M E M S E T *
 * ***************
 *
 * Registers we use:
 *      r3  = original ptr, not changed since memset returns it
 *      r4  = count of bytes to set
 *      r7  = value to set
 *      r8  = working operand ptr
 */
 
        .globl	_memset
        .align	5
_memset:                        // void *   memset(void *b, int c, size_t len);
        andi.	r7,r4,0xFF      // copy value to working register, test for 0
        mr		r4,r5           // move length to working register
        cmplgi	cr1,r5,kShort	// long enough to bother with _COMM_PAGE_MEMSET_PATTERN?
        beqa++	_COMM_PAGE_BZERO    // if (c==0), map to bzero()
        rlwimi	r7,r7,8,16,23	// replicate nonzero value to low 2 bytes
        neg		r5,r3           // start to compute #bytes to align
        mr		r8,r3           // make working copy of operand ptr
        rlwimi	r7,r7,16,0,15	// value now in all 4 bytes
        blt		cr1,Lmemset3    // too short to use commpage
        
        // TEMPORARY HACK
        // Operand is long enough to use _COMM_PAGE_MEMSET_PATTERN.  During Tiger
        // development, B&I uses Panther kernels on their builders but runs Tiger
        // apps on it.  So _COMM_PAGE_MEMSET_PATTERN may not be on this machine.
        // Rather than patch build fleet kernels, we just test to see if it is there
        // and use the short-operand case if not.  We can remove the hack when Tiger ships.
        
        lhz     r10,_COMM_PAGE_VERSION(0) // REMOVE THIS LINE WHEN TIGER SHIPS
        andi.	r0,r5,0xF       // r0 <- #bytes to align on quadword
        
        // Align ptr and store enough so that we have an aligned 16-byte pattern.

        stw     r7,0(r8)
        stw     r7,4(r8)
        stw     r7,8(r8)
        stw     r7,12(r8)
        cmpwi   cr1,r10,1       // REMOVE THIS LINE WHEN TIGER SHIPS
        beq     Lmemset1        // skip if (r0==0), ie if r8 is 16-byte aligned
        add     r8,r8,r0        // 16-byte align ptr
        sub     r4,r4,r0        // adjust length
        stw     r7,0(r8)        // now we can store an aligned 16-byte pattern
        stw     r7,4(r8)
        stw     r7,8(r8)
        stw     r7,12(r8)

        // Call machine-specific commpage routine, which expects:
        //      r4 = count (>=32)
        //      r8 = ptr (16-byte aligned) to memory to store
        //      r9 = ptr (16-byte aligned) to 16-byte pattern to store
        // When it returns:
        //      r3, r7, and r12 are preserved
        //      r4 and r8 are updated to reflect a residual count of from 0..31 bytes
        
Lmemset1:
        mflr    r12             // save return address
        mr      r9,r8           // point to 16-byte-aligned 16-byte pattern
        addi    r8,r8,16        // point to first unstored byte
        subi    r4,r4,16        // account for the aligned bytes we have stored
        bnela++ cr1,_COMM_PAGE_MEMSET_PATTERN   // CHANGE THIS LINE WHEN TIGER SHIPS
        mtlr    r12

        // Here for short nonzero memset.
        //  r4 = count (<= kShort bytes)
        //  r7 = pattern in all four bytes
        //  r8 = ptr
Lmemset3:
        srgi.   r0,r4,4         // any 16-byte chunks?
        mtcrf   0x01,r4         // move length remaining to cr7 so we can test bits
        beq     Lmemset5        // fewer than 16 bytes
        mtctr   r0
        b       Lmemset4        // enter loop
        
        .align  5
Lmemset4:                       // loop over 16-byte chunks
        stw     r7,0(r8)
        stw     r7,4(r8)
        stw     r7,8(r8)
        stw     r7,12(r8)
        addi    r8,r8,16
        bdnz++  Lmemset4
        
        // Handle last 0..15 bytes.
Lmemset5:
        bf      28,2f
        stw     r7,0(r8)
        stw     r7,4(r8)
        addi    r8,r8,8
2:
        bf      29,3f
        stw     r7,0(r8)
        addi    r8,r8,4
3:
        bf      30,4f
        sth     r7,0(r8)
        addi    r8,r8,2
4:
        bflr    31
        stb     r7,0(r8)
        blr
        

/* *************************************
 * * _ M E M S E T _ P A T T E R N 1 6 *
 * *************************************
 *
 * Used to store a 16-byte pattern in memory:
 *
 *  void    _memset_pattern16(void *b, const void *c16, size_t len);
 *
 * Where c16 points to the 16-byte pattern.  None of the parameters need be aligned.
 */

        .globl	__memset_pattern16
        .align	5
__memset_pattern16:
        cmplgi  cr1,r5,kShort   // check length
        lwz     r7,0(r4)        // load pattern into (these remain lwz in 64-bit mode)
        lwz     r9,4(r4)
        neg     r6,r3           // start to compute ptr alignment
        lwz     r10,8(r4)
        lwz     r11,12(r4)
        b       __memset_pattern_common
        

/* ***********************************
 * * _ M E M S E T _ P A T T E R N 8 *
 * ***********************************
 *
 * Used to store an 8-byte pattern in memory:
 *
 *  void    _memset_pattern8(void *b, const void *c8, size_t len);
 *
 * Where c8 points to the 8-byte pattern.  None of the parameters need be aligned.
 */

        .globl	__memset_pattern8
        .align	5
__memset_pattern8:
        lwz     r7,0(r4)        // load pattern (these remain lwz in 64-bit mode)
        lwz     r9,4(r4)
        cmplgi  cr1,r5,kShort   // check length
        neg     r6,r3           // start to compute ptr alignment
        mr      r10,r7          // replicate into 16-byte pattern
        mr      r11,r9
        b       __memset_pattern_common
        

/* ***********************************
 * * _ M E M S E T _ P A T T E R N 4 *
 * ***********************************
 *
 * Used to store a 4-byte pattern in memory:
 *
 *  void    _memset_pattern4(void *b, const void *c4, size_t len);
 *
 * Where c4 points to the 4-byte pattern.  None of the parameters need be aligned.
 */

        .globl	__memset_pattern4
        .align	5
__memset_pattern4:
        lwz     r7,0(r4)        // load pattern
        cmplgi  cr1,r5,kShort   // check length
        neg     r6,r3           // start to compute ptr alignment
        mr      r9,r7           // replicate into 16-byte pattern
        mr      r10,r7
        mr      r11,r7
        b       __memset_pattern_common // don't fall through because of scatter-loading
        
        
/* ***********************************************
 * * _ M E M S E T _ P A T T E R N _ C O M M O N *
 * ***********************************************
 *
 * This is the common code used by _memset_patter16, 8, and 4.  They all get here via
 * long branch (ie, "b") in case the routines are re-ordered, with:
 *      r3 = ptr to memory to store pattern into (unaligned)
 *      r5 = length in bytes
 *      r6 = neg(r3), used to compute #bytes to align
 *      r7, r9, r10, r11 = 16-byte pattern to store
 *      cr1= ble if (r5 <= kShort)
 */

        .globl	__memset_pattern_common
        .align	5
__memset_pattern_common:
        andi.   r0,r6,0xF       // get #bytes to 16-byte align ptr
        ble--   cr1,LShort      // if short operand skip out

        // Align ptr and store enough of pattern so we have an aligned
        // 16-byte chunk of it (this effectively rotates incoming pattern
        // if the original ptr was not aligned.)
        
        stw     r7,0(r3)
        stw     r9,4(r3)
        stw     r10,8(r3)
        stw     r11,12(r3)
        beq     Laligned        // skip if (r0==0), ie if r3 is 16-byte aligned
        stw     r7,16(r3)
        stw     r9,20(r3)
        stw     r10,24(r3)
        stw     r11,28(r3)
        add     r3,r3,r0        // 16-byte align ptr
        sub     r5,r5,r0        // adjust length
        
        // We're ready to call the machine-specific commpage routine
        // to do the heavy lifting.  When called, _COMM_PAGE_MEMSET_PATTERN expects:
        //      r4 = length (>= 32)
        //      r8 = ptr (16-byte aligned)
        //      r9 = ptr to 16-byte pattern (16-byte aligned)
        // When it returns:
        //      r3, r7, and r12 are preserved
        //      r4 and r8 are updated to reflect a residual count of from 0..31 bytes

Laligned:
        mflr    r12             // save return across commpage call
        mr      r9,r3           // point to 16-byte aligned 16-byte pattern
        addi    r8,r3,16        // point to first unstored byte (r8 is 16-byte aligned)
        subi    r4,r5,16        // account for the aligned bytes we have stored
        bla     _COMM_PAGE_MEMSET_PATTERN
        mr.     r5,r4           // move length (0..31) back to original reg and test for 0
        mtlr    r12
        beqlr                   // done if residual length == 0
        lwz     r7,-16(r8)      // load aligned pattern into r7,r9,r10, and r11
        lwz     r9,-12(r8)
        mr      r3,r8           // move destination ptr back
        lwz     r10,-8(r8)
        lwz     r11,-4(r8)
        
        // Handle short operands and leftovers.
        //      r3 = dest
        //      r5 = length
        //      r7,r9,r10,r11 = pattern
LShort:
        srgi.   r0,r5,4         // at least 16 bytes?
        mtcrf   0x01,r5         // move leftover count to cr7
        beq     Lleftovers
        mtctr   r0
LShortLoop:
        stw     r7,0(r3)        // replicate the pattern
        stw     r9,4(r3)
        stw     r10,8(r3)
        stw     r11,12(r3)
        addi    r3,r3,16
        bdnz    LShortLoop      // store 16 more bytes
        
        // Fewer than 16 bytes remaining.
Lleftovers:        
        bf      28,1f
        stw     r7,0(r3)        // store next 8 bytes
        stw     r9,4(r3)
        addi    r3,r3,8
        mr      r7,r10          // shift pattern over
        mr      r9,r11
1:
        bf      29,2f
        stw     r7,0(r3)
        addi    r3,r3,4
        mr      r7,r9
2:
        bf      30,3f
        rlwinm  r7,r7,16,0,31   // position leftmost 2 bytes for store
        sth     r7,0(r3)
        addi    r3,r3,2
3:
        bflr    31
        srwi    r7,r7,24        // position leftmost byte for store
        stb     r7,0(r3)
        blr
