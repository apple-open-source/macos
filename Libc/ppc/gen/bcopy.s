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
/* =======================================
 * BCOPY, MEMCPY, and MEMMOVE for Mac OS X
 * =======================================
 *
 * Version of 6/17/2002, for G3, G4, and G4+.
 *
 * There are many paths through this code, depending on length, reverse/forward,
 * processor type, and alignment.  We use reverse paths only when the operands
 * overlap and the destination is higher than the source.  They are not quite as
 * fast as the forward paths.
 *
 * Judicious use of DCBTs, just far enough ahead to minimize waiting, is critical in
 * the inner loops for long operands.  DST is less effective than DCBT, because it
 * can get out of sync with the inner loop.  DCBTST is usually not a win, so we
 * don't use it except during initialization when we're not using the LSU.
 * We don't DCBT on G3, which only handles one load miss at a time.
 *
 * We don't use DCBZ, because it takes an alignment exception on uncached memory
 * like frame buffers.  Bcopy to frame buffers must work.  This hurts G3 in the
 * cold-cache case, but G4 can use DCBA (which does not take alignment exceptions.)
 *
 * Using DCBA on G4 is a tradeoff.  For the cold-cache case it can be a big win, 
 * since it avoids the read of destination cache lines.  But for the hot-cache case 
 * it is always slower, because of the cycles spent needlessly zeroing data.  Some 
 * machines store-gather and can cancel the read if all bytes of a line are stored,
 * others cannot.  Unless explicitly told which is better, we time loops with and 
 * without DCBA and use the fastest.  Note that we never DCBA in reverse loops,
 * since by definition they are overlapped so dest lines will be in the cache.
 *
 * For longer operands we use an 8-element branch table, based on the CPU type,
 * to select the appropriate inner loop.  The branch table is indexed as follows:
 *
 *   bit 10000 set if a Reverse move is required
 *  bits 01100 set on the relative operand alignment: 0=unaligned, 1=word,
 *             2=doubleword, and 3=quadword.
 *
 * By "relatively" n-byte aligned, we mean the source and destination are a multiple
 * of n bytes apart (they need not be absolutely aligned.)
 *
 * The branch table for the running CPU type is pointed to by LBranchTablePtr.
 * Initially, LBranchtablePtr points to G3's table, since that is the lowest
 * common denominator that will run on any CPU.  Later, pthread initialization
 * sets up the _cpu_capabilities vector and calls _bcopy_initialize, which sets
 * up the correct pointer for the running CPU.
 *
 * We distinguish between "short", "medium", and "long" operands:
 *  short     (<= 32 bytes)    most common case, minimum path length is important
 *  medium    (> 32, < kLong)  too short for Altivec or use of cache ops like DCBA
 *  long      (>= kLong)       long enough for cache ops and to amortize use of Altivec
 *
 * WARNING:  kLong must be >=96, due to implicit assumptions about operand length.
 */
#define	kLong		96

/* Register usage.  Note we use R2, so this code will not run in a PEF/CFM
 * environment.  Note also the rather delicate way we assign multiple uses
 * to the same register.  Beware.
 *
 *   r0  = "w7" or "r0" (NB: cannot use r0 for any constant such as "c16")
 *   r2  = "w8" or VRSave ("rv")
 *   r3  = not used, as memcpy and memmove return 1st parameter as a value
 *   r4  = source ptr ("rs")
 *   r5  = count of bytes to move ("rc")
 *   r6  = "w1", "c16", or "cm17"
 *   r7  = "w2", "c32", or "cm33"
 *   r8  = "w3", "c48", or "cm49"
 *   r9  = "w4", "c64", or "cm1"
 *   r10 = "w5", "c96", or "cm97"
 *   r11 = "w6", "c128", "cm129", or return address ("ra")
 *   r12 = destination ptr ("rd")
 * f0-f8 = used for moving 8-byte aligned data
 *   v0  = permute vector ("vp") 
 * v1-v4 = qw's loaded from source ("v1", "v2", "v3", and "v4")
 * v5-v7 = permuted qw's ("vx", "vy", and "vz")
 */
#define rs	r4
#define rd	r12
#define rc	r5
#define ra	r11
#define	rv	r2

#define w1	r6
#define w2	r7
#define w3	r8
#define	w4	r9
#define w5	r10
#define w6	r11
#define w7	r0
#define w8	r2

#define c16		r6
#define cm17	r6
#define c32		r7
#define cm33	r7
#define c48		r8
#define cm49	r8
#define c64		r9
#define cm1		r9
#define c96		r10
#define cm97	r10
#define c128	r11
#define cm129	r11

#define	vp	v0
#define	vx	v5
#define	vy	v6
#define	vz	v7

#define	VRSave	256

#include <architecture/ppc/asm_help.h>

// The branch tables, 8 entries per CPU type.
// NB: we depend on 5 low-order 0s in the address of branch tables.

    .data
    .align	5						// must be 32-byte aligned

    // G3 (the default CPU type)
      
LG3:
    .long	LForwardWord			// 000: forward,       unaligned
    .long	LForwardFloat			// 001: forward,  4-byte aligned
    .long	LForwardFloat			// 010: forward,  8-byte aligned
    .long	LForwardFloat			// 011: forward, 16-byte aligned
    .long	LReverseWord			// 100: reverse,       unaligned
    .long	LReverseFloat			// 101: reverse,  4-byte aligned
    .long	LReverseFloat			// 110: reverse,  8-byte aligned
    .long	LReverseFloat			// 111: reverse, 16-byte aligned
    
    // G4s that benefit from DCBA.
        
LG4UseDcba:
    .long	LForwardVecUnal32Dcba	// 000: forward,       unaligned
    .long	LForwardVecUnal32Dcba	// 001: forward,  4-byte aligned
    .long	LForwardVecUnal32Dcba	// 010: forward,  8-byte aligned
    .long	LForwardVecAlig32Dcba	// 011: forward, 16-byte aligned
    .long	LReverseVectorUnal32	// 100: reverse,       unaligned
    .long	LReverseVectorUnal32	// 101: reverse,  4-byte aligned
    .long	LReverseVectorUnal32	// 110: reverse,  8-byte aligned
    .long	LReverseVectorAligned32	// 111: reverse, 16-byte aligned

    // G4s that should not use DCBA.

LG4NoDcba:    
    .long	LForwardVecUnal32NoDcba	// 000: forward,       unaligned
    .long	LForwardVecUnal32NoDcba	// 001: forward,  4-byte aligned
    .long	LForwardVecUnal32NoDcba	// 010: forward,  8-byte aligned
    .long	LForwardVecAlig32NoDcba	// 011: forward, 16-byte aligned
    .long	LReverseVectorUnal32	// 100: reverse,       unaligned
    .long	LReverseVectorUnal32	// 101: reverse,  4-byte aligned
    .long	LReverseVectorUnal32	// 110: reverse,  8-byte aligned
    .long	LReverseVectorAligned32	// 111: reverse, 16-byte aligned
    
        
// Pointer to the 8-element branch table for running CPU type:

LBranchTablePtr:
    .long	LG3						// default to G3 until "bcopy_initialize" called


// The CPU capability vector, initialized in pthread_init().
// "_bcopy_initialize" uses this to set up LBranchTablePtr:

    .globl __cpu_capabilities
__cpu_capabilities:
    .long 0
        
// Bit definitions for _cpu_capabilities:

#define	kHasAltivec		0x01
#define	k64Bit			0x02
#define	kCache32		0x04
#define	kCache64		0x08
#define	kCache128		0x10
#define	kUseDcba		0x20
#define	kNoDcba			0x40


.text
.globl _bcopy
.globl _memcpy
.globl _memmove
.globl __bcopy_initialize


// Main entry points.

        .align 	5
_bcopy:								// void bcopy(const void *src, void *dst, size_t len)
        mr		r10,r3				// reverse source and dest ptrs, to be like memcpy
        mr		r3,r4
        mr		r4,r10
_memcpy:							// void* memcpy(void *dst, void *src, size_t len)
_memmove:							// void* memmove(void *dst, const void *src, size_t len)
        cmplwi	cr7,rc,32			// length <= 32 bytes?
        sub.	w1,r3,rs			// must move in reverse if (rd-rs)<rc, set cr0 on sou==dst
        dcbt	0,rs				// touch in the first line of source
        cmplw	cr6,w1,rc			// set cr6 blt iff we must move reverse
        cmplwi	cr1,rc,kLong-1		// set cr1 bgt if long
        mr		rd,r3				// must leave r3 alone, it is return value for memcpy etc
        bgt-	cr7,LMedium			// longer than 32 bytes
        dcbtst	0,rd				// touch in destination
        beq-	cr7,LMove32			// special case moves of 32 bytes
        blt-	cr6,LShortReverse0
        
// Forward short operands.  This is the most frequent case, so it is inline.
// We also end up here to xfer the last 0-31 bytes of longer operands.

LShort:								// WARNING: can fall into this routine
        andi.	r0,rc,0x10			// test bit 27 separately (sometimes faster than a mtcrf)
        mtcrf	0x01,rc				// move rest of length to cr7
        beq		1f					// quadword to move?
        lwz		w1,0(rs)
        lwz		w2,4(rs)
        lwz		w3,8(rs)
        lwz		w4,12(rs)
        addi	rs,rs,16
        stw		w1,0(rd)
        stw		w2,4(rd)
        stw		w3,8(rd)
        stw		w4,12(rd)
        addi	rd,rd,16
1:
LShort16:							// join here to xfer 0-15 bytes
        bf		28,2f				// doubleword?
        lwz		w1,0(rs)
        lwz		w2,4(rs)
        addi	rs,rs,8
        stw		w1,0(rd)
        stw		w2,4(rd)
        addi	rd,rd,8
2:
        bf		29,3f				// word?
        lwz		w1,0(rs)
        addi	rs,rs,4
        stw		w1,0(rd)
        addi	rd,rd,4
3:
        bf		30,4f				// halfword to move?
        lhz		w1,0(rs)
        addi	rs,rs,2
        sth		w1,0(rd)
        addi	rd,rd,2
4:
        bflr	31					// skip if no odd byte
        lbz		w1,0(rs)
        stb		w1,0(rd)
        blr
        
        
// Handle short reverse operands, up to kShort in length.        
// This is also used to transfer the last 0-31 bytes of longer operands.

LShortReverse0:
        add		rs,rs,rc			// adjust ptrs for reverse move
        add		rd,rd,rc
LShortReverse:
        andi.	r0,rc,0x10			// test bit 27 separately (sometimes faster than a mtcrf)
        mtcrf	0x01,rc				// move rest of length to cr7
        beq		1f					// quadword to move?
        lwz		w1,-4(rs)
        lwz		w2,-8(rs)
        lwz		w3,-12(rs)
        lwzu	w4,-16(rs)
        stw		w1,-4(rd)
        stw		w2,-8(rd)
        stw		w3,-12(rd)
        stwu	w4,-16(rd)
1:
LShortReverse16:					// join here to xfer 0-15 bytes and return
        bf		28,2f				// doubleword?
        lwz		w1,-4(rs)
        lwzu	w2,-8(rs)
        stw		w1,-4(rd)
        stwu	w2,-8(rd
2:
        bf		29,3f				// word?
        lwzu	w1,-4(rs)
        stwu	w1,-4(rd)
3:
        bf		30,4f				// halfword to move?
        lhzu	w1,-2(rs)
        sthu	w1,-2(rd)
4:
        bflr	31					// done if no odd byte
        lbz 	w1,-1(rs)			// no update
        stb 	w1,-1(rd)
        blr


// Special case for 32-byte moves.  Too long for LShort, too common for LMedium.

LMove32:
        lwz		w1,0(rs)
        lwz		w2,4(rs)
        lwz		w3,8(rs)
        lwz		w4,12(rs)
        lwz		w5,16(rs)
        lwz		w6,20(rs)
        lwz		w7,24(rs)
        lwz		w8,28(rs)
        stw		w1,0(rd)
        stw		w2,4(rd)
        stw		w3,8(rd)
        stw		w4,12(rd)
        stw		w5,16(rd)
        stw		w6,20(rd)
        stw		w7,24(rd)
        stw		w8,28(rd)
LExit:
        blr


// Medium length operands (32 < rc < kLong.)  These loops run on all CPUs, as the
// operands are not long enough to bother with the branch table, using cache ops, or
// Altivec.  We word align the source, not the dest as we do for long operands,
// since doing so is faster on G4+ and probably beyond, we never DCBA on medium-length
// operands, and the opportunity to cancel reads of dest cache lines is limited.
//		w1  = (rd-rs), used to check for alignment
//		cr0 = set on (rd-rs)
//		cr1 = bgt if long operand
//		cr6 = blt if reverse move

LMedium:
        dcbtst	0,rd				// touch in 1st line of destination
        rlwinm	r0,w1,0,29,31		// r0 <- ((rd-rs) & 7), ie 0 if doubleword aligned
        beq-	LExit				// early exit if (rs==rd), avoiding use of "beqlr"
        neg		w2,rs				// we align source, not dest, and assume forward
        cmpwi	cr5,r0,0			// set cr5 beq if doubleword aligned
        bgt-	cr1,LLong			// handle long operands
        andi.	w3,w2,3				// W3 <- #bytes to word-align source
        blt-	cr6,LMediumReverse	// handle reverse move
        lwz		w1,0(rs)			// pre-fetch first 4 bytes of source
        beq-	cr5,LMediumAligned	// operands are doubleword aligned
        sub		rc,rc,w3			// adjust count for alignment
        mtcrf	0x01,rc				// remaining byte count (0-15) to cr7 for LShort16
        srwi	w4,rc,4				// w4 <- number of 16-byte chunks to xfer (>=1)
        mtctr	w4					// prepare loop count
        beq+	2f					// source already aligned
        
        lwzx	w2,w3,rs			// get 1st aligned word (which we might partially overwrite)
        add		rs,rs,w3			// word-align source ptr
        stw		w1,0(rd)			// store all (w3) bytes at once to avoid a loop
        add		rd,rd,w3
        mr		w1,w2				// first aligned word to w1
        b		2f
        
        .align	4					// align inner loops
1:									// loop over 16-byte chunks
        lwz		w1,0(rs)
2:
        lwz		w2,4(rs)
        lwz		w3,8(rs)
        lwz		w4,12(rs)
        addi	rs,rs,16
        stw		w1,0(rd)
        stw		w2,4(rd)
        stw		w3,8(rd)
        stw		w4,12(rd)
        addi	rd,rd,16
        bdnz	1b
        
        b		LShort16

        
// Medium, doubleword aligned.  We use floating point.  Note that G4+ has bigger latencies
// and reduced throughput for floating pt loads and stores; future processors will probably
// have even worse lfd/stfd performance.  We use it here because it is so important for G3,
// and not slower for G4+.  But we only do so for doubleword aligned operands, whereas the
// G3-only long operand loops use floating pt even for word-aligned operands.
//		w2 = neg(rs)
//		w1 = first 4 bytes of source

LMediumAligned:
        andi.	w3,w2,7				// already aligned?
        sub		rc,rc,w3			// adjust count by 0-7 bytes
        lfdx	f0,rs,w3			// pre-fetch first aligned source doubleword
        srwi	w4,rc,5				// get count of 32-byte chunks (might be 0 if unaligned)
        mtctr	w4
        beq-	LForwardFloatLoop1	// already aligned
        
        cmpwi	w4,0				// are there any 32-byte chunks to xfer?
        lwz		w2,4(rs)			// get 2nd (unaligned) source word
        add		rs,rs,w3			// doubleword align source pointer
        stw		w1,0(rd)			// store first 8 bytes of source to align...
        stw		w2,4(rd)			// ...which could overwrite source
        add		rd,rd,w3			// doubleword align destination
        bne+	LForwardFloatLoop1	// at least 1 chunk, so enter loop
        
        subi	rc,rc,8				// unfortunate degenerate case: no chunks to xfer
        stfd	f0,0(rd)			// must store f1 since source might have been overwriten
        addi	rs,rs,8
        addi	rd,rd,8
        b		LShort
        

// Medium reverse moves.  This loop runs on all processors.

LMediumReverse:
        add		rs,rs,rc			// point to other end of operands when in reverse
        add		rd,rd,rc
        andi.	w3,rs,3				// w3 <- #bytes to word align source
        lwz		w1,-4(rs)			// pre-fetch 1st 4 bytes of source
        sub		rc,rc,w3			// adjust count
        srwi	w4,rc,4				// get count of 16-byte chunks (>=1)
        mtcrf	0x01,rc				// remaining byte count (0-15) to cr7 for LShortReverse16
        mtctr	w4					// prepare loop count
        beq+	2f					// source already aligned
        
        sub		rs,rs,w3			// word-align source ptr
        lwz		w2,-4(rs)			// get 1st aligned word which we may overwrite
        stw		w1,-4(rd)			// store all 4 bytes to align without a loop
        sub		rd,rd,w3
        mr		w1,w2				// shift 1st aligned source word to w1
        b		2f

1:
        lwz		w1,-4(rs)
2:
        lwz		w2,-8(rs)
        lwz		w3,-12(rs)
        lwzu	w4,-16(rs)
        stw		w1,-4(rd)
        stw		w2,-8(rd)
        stw		w3,-12(rd)
        stwu	w4,-16(rd)
        bdnz	1b
        
        b		LShortReverse16

                                
// Long operands.  Use branch table to decide which loop to use.
//		w1  = (rd-rs), used to determine alignment

LLong:
        xor		w4,w1,rc			// we must move reverse if (rd-rs)<rc
        mflr	ra					// save return address
        rlwinm	w5,w1,1,27,30		// w5 <- ((w1 & 0xF) << 1)
        bcl		20,31,1f			// use reserved form to get our location
1:
        mflr	w3					// w3 == addr(1b)
        lis		w8,0x0408			// load a 16 element, 2-bit array into w8...
        cntlzw	w4,w4				// find first difference between (rd-rs) and rc
        addis	w2,w3,ha16(LBranchTablePtr-1b)
        ori		w8,w8,0x040C		// ...used to map w5 to alignment encoding (ie, to 0-3)
        lwz		w2,lo16(LBranchTablePtr-1b)(w2)	// w2 <- branch table address
        slw		w4,rc,w4			// bit 0 of w4 set iff (rd-rs)<rc
        rlwnm	w5,w8,w5,28,29		// put alignment encoding in bits 01100 of w5
        rlwimi	w2,w4,5,27,27		// put reverse bit in bit 10000 of branch table address
        lwzx	w3,w2,w5			// w3 <- load loop address from branch table
        neg		w1,rd				// start to compute destination alignment
        mtctr	w3
        andi.	r0,w1,0x1F			// r0 <- bytes req'd to 32-byte align dest (if forward move)
        bctr						// NB: r0/cr0 and w1 are passed as parameters
        
        
// G3, forward, long, unaligned.
//		w1 = neg(rd)

LForwardWord:
        andi.	w3,w1,3				// W3 <- #bytes to word-align destination
        mtlr	ra					// restore return address
        sub		rc,rc,w3			// adjust count for alignment
        srwi	r0,rc,5				// number of 32-byte chunks to xfer (>=1)
        mtctr	r0					// prepare loop count
        beq+	1f					// dest already aligned
        
        lwz		w2,0(rs)			// get first 4 bytes of source
        lwzx	w1,w3,rs			// get source bytes we might overwrite
        add		rs,rs,w3			// adjust source ptr
        stw		w2,0(rd)			// store all 4 bytes to avoid a loop
        add		rd,rd,w3			// word-align destination
        b		2f
1:
        lwz		w1,0(rs)
2:
        lwz		w2,4(rs)
        lwz		w3,8(rs)
        lwz		w4,12(rs)
        lwz		w5,16(rs)
        lwz		w6,20(rs)
        lwz		w7,24(rs)
        lwz		w8,28(rs)
        addi	rs,rs,32
        stw		w1,0(rd)
        stw		w2,4(rd)
        stw		w3,8(rd)
        stw		w4,12(rd)
        stw		w5,16(rd)
        stw		w6,20(rd)
        stw		w7,24(rd)
        stw		w8,28(rd)
        addi	rd,rd,32
        bdnz	1b
        
        b		LShort        


// G3, forward, long, word aligned.  We use floating pt even when only word aligned.
//		w1 = neg(rd)

LForwardFloat:
        andi.	w3,w1,7				// W3 <- #bytes to doubleword-align destination
        mtlr	ra					// restore return address
        sub		rc,rc,w3			// adjust count for alignment
        srwi	r0,rc,5				// number of 32-byte chunks to xfer (>=1)
        mtctr	r0					// prepare loop count
        beq		LForwardFloatLoop	// dest already aligned
        
        lwz		w1,0(rs)			// get first 8 bytes of source
        lwz		w2,4(rs)
        lfdx	f0,w3,rs			// get source bytes we might overwrite
        add		rs,rs,w3			// word-align source ptr
        stw		w1,0(rd)			// store all 8 bytes to avoid a loop
        stw		w2,4(rd)
        add		rd,rd,w3
        b		LForwardFloatLoop1
        
        .align	4					// align since this loop is executed by G4s too
LForwardFloatLoop:
        lfd		f0,0(rs)
LForwardFloatLoop1:					// enter here from LMediumAligned and above
        lfd		f1,8(rs)
        lfd		f2,16(rs)
        lfd		f3,24(rs)
        addi	rs,rs,32
        stfd	f0,0(rd)
        stfd	f1,8(rd)
        stfd	f2,16(rd)
        stfd	f3,24(rd)
        addi	rd,rd,32
        bdnz	LForwardFloatLoop
        
        b		LShort
        
        
// G4 Forward, long, 16-byte aligned, 32-byte cache ops, use DCBA and DCBT.
//		r0/cr0 = #bytes to 32-byte align

LForwardVecAlig32Dcba:
        bnel+	LAlign32			// align destination iff necessary
        bl		LPrepareForwardVectors
        mtlr	ra					// restore return address before loading c128
        li		c128,128
        b		1f					// enter aligned loop
        
        .align	5					// long loop heads should be at least 16-byte aligned
1:        							// loop over aligned 64-byte chunks
        dcbt	c96,rs				// pre-fetch three cache lines ahead
        dcbt	c128,rs				// and four
        lvx		v1,0,rs
        lvx		v2,c16,rs
        lvx		v3,c32,rs
        lvx		v4,c48,rs
        addi	rs,rs,64
        dcba	0,rd				// avoid read of destination cache lines
        stvx	v1,0,rd
        stvx	v2,c16,rd
        dcba	c32,rd
        stvx	v3,c32,rd
        stvx	v4,c48,rd
        addi	rd,rd,64
        bdnz	1b
        
LForwardVectorAlignedEnd:			// r0/cr0=#quadwords, rv=VRSave, cr7=low 4 bits of rc, cr6 set on cr7       
        beq-	3f					// no leftover quadwords
        mtctr	r0
2:									// loop over remaining quadwords (1-7)
        lvx		v1,0,rs
        addi	rs,rs,16
        stvx	v1,0,rd
        addi	rd,rd,16
        bdnz	2b
3:
        mtspr	VRSave,rv			// restore bitmap of live vr's
        bne		cr6,LShort16		// handle last 0-15 bytes if any
        blr


// G4 Forward, long, 16-byte aligned, 32-byte cache, use DCBT but not DCBA.
//		r0/cr0 = #bytes to 32-byte align

LForwardVecAlig32NoDcba:
        bnel+	LAlign32			// align destination iff necessary
        bl		LPrepareForwardVectors
        mtlr	ra					// restore return address before loading c128
        li		c128,128
        b		1f					// enter aligned loop
        
        .align	4					// balance 13-word loop between QWs...
        nop							// ...which improves performance 5% +/-
        nop
1:        							// loop over aligned 64-byte chunks
        dcbt	c96,rs				// pre-fetch three cache lines ahead
        dcbt	c128,rs				// and four
        lvx		v1,0,rs
        lvx		v2,c16,rs
        lvx		v3,c32,rs
        lvx		v4,c48,rs
        addi	rs,rs,64
        stvx	v1,0,rd
        stvx	v2,c16,rd
        stvx	v3,c32,rd
        stvx	v4,c48,rd
        addi	rd,rd,64
        bdnz	1b
        
        b		LForwardVectorAlignedEnd


// G4 Forward, long, unaligned, 32-byte cache ops, use DCBT and DCBA.  At least on
// some CPUs, this routine is no slower than the simpler aligned version that does
// not use permutes.  But it cannot be used with aligned operands, because of the
// way it prefetches source QWs.
//		r0/cr0 = #bytes to 32-byte align

LForwardVecUnal32Dcba:
        bnel+	LAlign32			// align destination iff necessary
        bl		LPrepareForwardVectors
        lvx		v1,0,rs				// prime loop
        mtlr	ra					// restore return address before loading c128
        lvsl	vp,0,rs				// get permute vector to shift left
        li		c128,128
        b		1f					// enter aligned loop
        
        .align	4					// long loop heads should be at least 16-byte aligned
1:        							// loop over aligned 64-byte destination chunks
        lvx		v2,c16,rs
        dcbt	c96,rs				// touch 3rd cache line ahead
        lvx		v3,c32,rs
        dcbt	c128,rs				// touch 4th cache line ahead
        lvx		v4,c48,rs
        addi	rs,rs,64
        vperm	vx,v1,v2,vp
        lvx		v1,0,rs
        vperm	vy,v2,v3,vp
        dcba	0,rd				// avoid read of destination lines
        stvx	vx,0,rd
        vperm	vz,v3,v4,vp
        stvx	vy,c16,rd
        dcba	c32,rd
        vperm	vx,v4,v1,vp
        stvx	vz,c32,rd
        stvx	vx,c48,rd
        addi	rd,rd,64
        bdnz	1b

LForwardVectorUnalignedEnd:			// r0/cr0=#QWs, rv=VRSave, v1=next QW, cr7=(rc & F), cr6 set on cr7
        beq-	3f					// no leftover quadwords
        mtctr	r0
2:									// loop over remaining quadwords
        lvx		v2,c16,rs
        addi	rs,rs,16
        vperm	vx,v1,v2,vp
        vor		v1,v2,v2			// v1 <- v2
        stvx	vx,0,rd
        addi	rd,rd,16
        bdnz	2b
3:
        mtspr	VRSave,rv			// restore bitmap of live vr's
        bne		cr6,LShort16		// handle last 0-15 bytes if any
        blr


// G4 Forward, long, unaligned, 32-byte cache ops, use DCBT but not DCBA.
//		r0/cr0 = #bytes to 32-byte align

LForwardVecUnal32NoDcba:
        bnel+	LAlign32			// align destination iff necessary
        bl		LPrepareForwardVectors
        lvx		v1,0,rs				// prime loop
        mtlr	ra					// restore return address before loading c128
        lvsl	vp,0,rs				// get permute vector to shift left
        li		c128,128
        b		1f					// enter aligned loop
        
        .align	4
        nop							// balance 17-word loop between QWs
        nop
1:        							// loop over aligned 64-byte destination chunks
        lvx		v2,c16,rs
        dcbt	c96,rs				// touch 3rd cache line ahead
        lvx		v3,c32,rs
        dcbt	c128,rs				// touch 4th cache line ahead
        lvx		v4,c48,rs
        addi	rs,rs,64
        vperm	vx,v1,v2,vp
        lvx		v1,0,rs
        vperm	vy,v2,v3,vp
        stvx	vx,0,rd
        vperm	vz,v3,v4,vp
        stvx	vy,c16,rd
        vperm	vx,v4,v1,vp
        stvx	vz,c32,rd
        stvx	vx,c48,rd
        addi	rd,rd,64
        bdnz	1b
        
        b		LForwardVectorUnalignedEnd


// G3 Reverse, long, unaligned.

LReverseWord:
        bl		LAlign8Reverse		// 8-byte align destination
        mtlr	ra					// restore return address
        srwi	r0,rc,5				// get count of 32-byte chunks to xfer (> 1)
        mtctr	r0
1:
        lwz		w1,-4(rs)
        lwz		w2,-8(rs)
        lwz		w3,-12(rs)
        lwz		w4,-16(rs)
        stw		w1,-4(rd)
        lwz		w5,-20(rs)
        stw		w2,-8(rd)
        lwz		w6,-24(rs)
        stw		w3,-12(rd)
        lwz		w7,-28(rs)
        stw		w4,-16(rd)
        lwzu	w8,-32(rs)
        stw		w5,-20(rd)
        stw		w6,-24(rd)
        stw		w7,-28(rd)
        stwu	w8,-32(rd)
        bdnz	1b

        b		LShortReverse        


// G3 Reverse, long, word aligned.

LReverseFloat:
        bl		LAlign8Reverse		// 8-byte align
        mtlr	ra					// restore return address
        srwi	r0,rc,5				// get count of 32-byte chunks to xfer (> 1)
        mtctr	r0
1:
        lfd		f0,-8(rs)
        lfd		f1,-16(rs)
        lfd		f2,-24(rs)
        lfdu	f3,-32(rs)
        stfd	f0,-8(rd)
        stfd	f1,-16(rd)
        stfd	f2,-24(rd)
        stfdu	f3,-32(rd)
        bdnz	1b
        
        b		LShortReverse    
        
        
// G4 Reverse, long, 16-byte aligned, 32-byte DCBT but no DCBA.

LReverseVectorAligned32:
        bl		LAlign32Reverse		// 32-byte align destination iff necessary
        bl		LPrepareReverseVectors
        mtlr	ra					// restore return address before loading cm129
        li		cm129,-129
        b		1f					// enter aligned loop
        
        .align	4
        nop							// must start in 3rd word of QW...
        nop							// ...to keep balanced
1:        							// loop over aligned 64-byte chunks
        dcbt	cm97,rs				// pre-fetch three cache lines ahead
        dcbt	cm129,rs			// and four
        lvx		v1,cm1,rs
        lvx		v2,cm17,rs
        lvx		v3,cm33,rs
        lvx		v4,cm49,rs
        subi	rs,rs,64
        stvx	v1,cm1,rd
        stvx	v2,cm17,rd
        stvx	v3,cm33,rd
        stvx	v4,cm49,rd
        subi	rd,rd,64
        bdnz	1b
        
LReverseVectorAlignedEnd:			// cr0/r0=#quadwords, rv=VRSave, cr7=low 4 bits of rc, cr6 set on cr7
        beq		3f					// no leftover quadwords
        mtctr	r0
2:									// loop over 1-3 quadwords
        lvx		v1,cm1,rs
        subi	rs,rs,16
        stvx	v1,cm1,rd
        subi	rd,rd,16
        bdnz	2b
3:
        mtspr	VRSave,rv			// restore bitmap of live vr's
        bne		cr6,LShortReverse16	// handle last 0-15 bytes iff any
        blr


// G4 Reverse, long, unaligned, 32-byte DCBT. 

LReverseVectorUnal32:
        bl		LAlign32Reverse		// align destination iff necessary
        bl		LPrepareReverseVectors
        lvx		v1,cm1,rs			// prime loop
        mtlr	ra					// restore return address before loading cm129
        lvsl	vp,0,rs				// get permute vector to shift left
        li		cm129,-129
        b		1f					// enter aligned loop
        
        .align	4
        nop							// start loop in 3rd word on QW to balance
        nop
1:        							// loop over aligned 64-byte destination chunks
        lvx		v2,cm17,rs
        dcbt	cm97,rs				// touch in 3rd source block
        lvx		v3,cm33,rs
        dcbt	cm129,rs			// touch in 4th
        lvx		v4,cm49,rs
        subi	rs,rs,64
        vperm	vx,v2,v1,vp
        lvx		v1,cm1,rs
        vperm	vy,v3,v2,vp
        stvx	vx,cm1,rd
        vperm	vz,v4,v3,vp
        stvx	vy,cm17,rd
        vperm	vx,v1,v4,vp
        stvx	vz,cm33,rd
        stvx	vx,cm49,rd
        subi	rd,rd,64
        bdnz	1b
        
LReverseVectorUnalignedEnd:			// r0/cr0=#QWs, rv=VRSave, v1=source QW, cr7=low 4 bits of rc, cr6 set on cr7
        beq		3f					// no leftover quadwords
        mtctr	r0
2:									// loop over 1-3 quadwords
        lvx		v2,cm17,rs
        subi	rs,rs,16
        vperm	vx,v2,v1,vp
        vor		v1,v2,v2			// v1 <- v2
        stvx	vx,cm1,rd
        subi	rd,rd,16
        bdnz	2b
3:
        mtspr	VRSave,rv			// restore bitmap of live vr's
        bne		cr6,LShortReverse16	// handle last 0-15 bytes iff any
        blr


// Subroutine to prepare for 64-byte forward vector loops.
//		Returns many things:
//			ctr = number of 64-byte chunks to move
//			r0/cr0 = leftover QWs to move
//			cr7 = low 4 bits of rc (ie, leftover byte count 0-15)
//			cr6 = beq if leftover byte count is 0
//			c16..c96 loaded
//			rv = original value of VRSave
//		NB: c128 not set (if needed), since it is still "ra"

LPrepareForwardVectors:
        mfspr	rv,VRSave			// get bitmap of live vector registers
        srwi	r0,rc,6				// get count of 64-byte chunks to move (>=1)
        oris	w1,rv,0xFF00		// we use v0-v7
        mtcrf	0x01,rc				// prepare for moving last 0-15 bytes in LShort16
        rlwinm	w3,rc,0,28,31		// move last 0-15 byte count to w3 too
        mtspr	VRSave,w1			// update mask
        li		c16,16				// get constants used in ldvx/stvx
        li		c32,32
        mtctr	r0					// set up loop count
        cmpwi	cr6,w3,0			// set cr6 on leftover byte count
        li		c48,48
        li		c96,96
        rlwinm.	r0,rc,28,30,31		// get number of quadword leftovers (0-3) and set cr0
        blr


// Subroutine to prepare for 64-byte reverse vector loops.
//		Returns many things:
//			ctr = number of 64-byte chunks to move
//			r0/cr0 = leftover QWs to move
//			cr7 = low 4 bits of rc (ie, leftover byte count 0-15)
//			cr6 = beq if leftover byte count is 0
//			cm1..cm97 loaded
//			rv = original value of VRSave
//		NB: cm129 not set (if needed), since it is still "ra"

LPrepareReverseVectors:
        mfspr	rv,VRSave			// get bitmap of live vector registers
        srwi	r0,rc,6				// get count of 64-byte chunks to move (>=1)
        oris	w1,rv,0xFF00		// we use v0-v7
        mtcrf	0x01,rc				// prepare for moving last 0-15 bytes in LShortReverse16
        rlwinm	w3,rc,0,28,31		// move last 0-15 byte count to w3 too
        mtspr	VRSave,w1			// update mask
        li		cm1,-1				// get constants used in ldvx/stvx
        li		cm17,-17
        mtctr	r0					// set up loop count
        cmpwi	cr6,w3,0			// set cr6 on leftover byte count
        li		cm33,-33
        li		cm49,-49
        rlwinm.	r0,rc,28,30,31		// get number of quadword leftovers (0-3) and set cr0
        li		cm97,-97
        blr


// Subroutine to align destination on a 32-byte boundary.
//	r0 = number of bytes to xfer (0-31)

LAlign32:
        mtcrf	0x01,r0				// length to cr (faster to change 1 CR at a time)
        mtcrf	0x02,r0
        sub		rc,rc,r0			// adjust length
        bf		31,1f				// skip if no odd bit
        lbz		w1,0(rs)
        addi	rs,rs,1
        stb		w1,0(rd)
        addi	rd,rd,1
1:
        bf		30,2f				// halfword to move?
        lhz		w1,0(rs)
        addi	rs,rs,2
        sth		w1,0(rd)
        addi	rd,rd,2
2:
        bf		29,3f				// word?
        lwz		w1,0(rs)
        addi	rs,rs,4
        stw		w1,0(rd)
        addi	rd,rd,4
3:
        bf		28,4f				// doubleword?
        lwz		w1,0(rs)
        lwz		w2,4(rs)
        addi	rs,rs,8
        stw		w1,0(rd)
        stw		w2,4(rd)
        addi	rd,rd,8
4:
        bflr	27					// done if no quadword to move
        lwz		w1,0(rs)
        lwz		w2,4(rs)
        lwz		w3,8(rs)
        lwz		w4,12(rs)
        addi	rs,rs,16
        stw		w1,0(rd)
        stw		w2,4(rd)
        stw		w3,8(rd)
        stw		w4,12(rd)
        addi	rd,rd,16
        blr

// Subroutine to align destination if necessary on a 32-byte boundary for reverse moves.
//   rs and rd still point to low end of operands
//	 we adjust rs and rd to point to last byte moved

LAlign32Reverse:
        add		rd,rd,rc			// point to last byte moved (ie, 1 past end of operands)
        add		rs,rs,rc
        andi.	r0,rd,0x1F			// r0 <- #bytes that must be moved to align destination
        mtcrf	0x01,r0				// length to cr (faster to change 1 CR at a time)
        mtcrf	0x02,r0
        sub		rc,rc,r0			// update length
        beqlr-						// destination already 32-byte aligned
        
        bf		31,1f				// odd byte?
        lbzu 	w1,-1(rs)
        stbu 	w1,-1(rd)
1:
        bf		30,2f				// halfword to move?
        lhzu	w1,-2(rs)
        sthu	w1,-2(rd)
2:        
        bf		29,3f				// word?
        lwzu	w1,-4(rs)
        stwu	w1,-4(rd)
3:
        bf		28,4f				// doubleword?
        lwz		w1,-4(rs)
        lwzu	w2,-8(rs)
        stw		w1,-4(rd)
        stwu	w2,-8(rd
4:        
        bflr	27					// done if no quadwords
        lwz		w1,-4(rs)
        lwz		w2,-8(rs)
        lwz		w3,-12(rs)
        lwzu	w4,-16(rs)
        stw		w1,-4(rd)
        stw		w2,-8(rd)
        stw		w3,-12(rd)
        stwu	w4,-16(rd)
        blr


// Subroutine to align destination on an 8-byte boundary for reverse moves.
//   rs and rd still point to low end of operands
//	 we adjust rs and rd to point to last byte moved

LAlign8Reverse:
        add		rd,rd,rc			// point to last byte moved (ie, 1 past end of operands)
        add		rs,rs,rc
        andi.	r0,rd,0x7			// r0 <- #bytes that must be moved to align destination
        beqlr-						// destination already 8-byte aligned
        mtctr	r0					// set up for loop
        sub		rc,rc,r0			// update length
1:
        lbzu	w1,-1(rs)
        stbu	w1,-1(rd)
        bdnz	1b
        
        blr
        
        
// Called by pthread initialization to set up the branch table pointer based on
// the CPU capability vector.  This routine may be called more than once (for
// example, during testing.)

// Size of the buffer we use to do DCBA timing on G4:
#define	kBufSiz	1024

// Stack frame size, which contains the 128-byte-aligned buffer:
#define	kSFSize	(kBufSiz+128+16)

// Iterations of the timing loop:
#define	kLoopCnt	5

// Bit in cr5 used as a flag in timing loop:
#define	kDCBA		22

__bcopy_initialize:					// int _bcopy_initialize(void)
        mflr	ra					// get return
        stw		ra,8(r1)			// save
        stwu	r1,-kSFSize(r1)		// carve our temp buffer from the stack
        addi	w6,r1,127+16		// get base address...
        rlwinm	w6,w6,0,0,24		// ...of our buffer, 128-byte aligned
        bcl		20,31,1f			// get our PIC base
1:
        mflr	w1
        addis	w2,w1,ha16(__cpu_capabilities - 1b)
        lwz		w3,lo16(__cpu_capabilities - 1b)(w2)
        andi.	r0,w3,kUseDcba+kNoDcba+kCache32+k64Bit+kHasAltivec
        cmpwi	r0,kCache32+kHasAltivec	// untyped G4?
        li		w8,0				// assume no need to test
        bne		2f					// not an untyped G4, so do not test
        
        // G4, but neither kUseDcba or kNoDcba are set.  Time and select fastest.
        
        crset	kDCBA				// first, use DCBA
        bl		LTest32				// time it
        mr		w8,w4				// w8 <- best time using DCBA
        srwi	r0,w8,3				// bias 12 pct in favor of not using DCBA...
        add		w8,w8,r0			// ...because DCBA is always slower with warm cache
        crclr	kDCBA
        bl		LTest32				// w4 <- best time without DCBA
        cmplw	w8,w4				// which is better?
        li		w8,kUseDcba			// assume using DCBA is faster
        blt		2f
        li		w8,kNoDcba			// no DCBA is faster
        
        // What branch table to use?

2:									// here with w8 = 0, kUseDcba, or kNoDcba
        bcl		20,31,4f			// get our PIC base again
4:
        mflr	w1
        addis	w2,w1,ha16(__cpu_capabilities - 4b)
        lwz		w3,lo16(__cpu_capabilities - 4b)(w2)
        or		w3,w3,w8			// add in kUseDcba or kNoDcba if untyped G4
        mr		r3,w8				// return dynamic selection, if any (used in testing)
        
        andi.	r0,w3,kHasAltivec+k64Bit+kCache128+kCache64+kCache32+kUseDcba+kNoDcba
        cmpwi	r0,kHasAltivec+kCache32+kUseDcba	// G4 with DCBA?
        addis	w4,w1,ha16(LG4UseDcba - 4b)
        addi	w4,w4,lo16(LG4UseDcba - 4b)
        beq		5f
        
        andi.	r0,w3,kHasAltivec+k64Bit+kCache128+kCache64+kCache32+kUseDcba+kNoDcba
        cmpwi	r0,kHasAltivec+kCache32+kNoDcba		// G4 without DCBA?
        addis	w4,w1,ha16(LG4NoDcba - 4b)
        addi	w4,w4,lo16(LG4NoDcba - 4b)
        beq		5f
        
        andi.	r0,w3,kHasAltivec+k64Bit+kCache128+kCache64+kCache32
        cmpwi	r0,kCache32							// G3?
        addis	w4,w1,ha16(LG3 - 4b)
        addi	w4,w4,lo16(LG3 - 4b)
        beq		5f
        
        // Map unrecognized CPU types to G3 (lowest common denominator)
        
5:									// w4 <- branch table pointer
        addis	w5,w1,ha16(LBranchTablePtr - 4b)
        stw		w4,lo16(LBranchTablePtr - 4b)(w5)
        lwz		ra,kSFSize+8(r1)	// recover return address
        mtlr	ra					// restore it
        lwz		r1,0(r1)			// pop off our stack frame
        blr							// return dynamic selection (or 0) in r3
        
        
// Subroutine to time a 32-byte cache.
//		kDCBA = set if we should use DCBA
//		w6 = base of buffer to use for test (kBufSiz bytes)
//		w4 = we return time of fastest loop in w4

LTest32:
        li		w1,kLoopCnt			// number of times to loop
        li		w4,-1				// initialize fastest time
1:
        mr		rd,w6				// initialize buffer ptr
        li		r0,kBufSiz/32		// r0 <- cache blocks to test
        mtctr	r0
2:
        dcbf	0,rd				// first, force the blocks out of the cache
        addi	rd,rd,32
        bdnz	2b
        sync						// make sure all the flushes take
        mr		rd,w6				// re-initialize buffer ptr
        mtctr	r0					// reset cache-block count
        mftbu	w5					// remember upper half so we can check for carry
        mftb	w2					// start the timer
3:									// loop over cache blocks
        bf		kDCBA,4f			// should we DCBA?
        dcba	0,rd
4:
        stfd	f1,0(rd)			// store the entire cache block
        stfd	f1,8(rd)
        stfd	f1,16(rd)
        stfd	f1,24(rd)
        addi	rd,rd,32
        bdnz	3b
        mftb	w3
        mftbu	r0
        cmpw	r0,w5				// did timebase carry?
        bne		1b					// yes, retest rather than fuss
        sub		w3,w3,w2			// w3 <- time for this loop
        cmplw	w3,w4				// faster than current best?
        bge		5f					// no
        mr		w4,w3				// remember fastest time through loop
5:
        subi	w1,w1,1				// decrement outer loop count
        cmpwi	w1,0				// more to go?
        bne		1b					// loop if so
        blr
        