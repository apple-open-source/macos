/*
 * Copyright (c) 1992-2001 Apple Computer, Inc. All rights reserved.
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
#include <architecture/ppc/asm_help.h>

// =================================================================================================
// *** The easiest way to assemble things on Mac OS X is via "cc", so this uses #defines and such.
// =================================================================================================

// Keep track of whether we have Altivec 
// This gets set in pthread_init()

.data
.align 2
.globl __cpu_has_altivec
__cpu_has_altivec:
.long 0

.text
.align 2
.globl _bcopy
.globl _memcpy
.globl _memmove

_bcopy:
	mr	r2,r4	// Since bcopy uses (src,dest,count), swap r3,r4
	mr	r4,r3
	mr	r3,r2	
_memcpy:
_memmove:
	mr	r2,r3	// Store dest ptr in r2 to preserve r3 on return

// ------------------
// Standard registers

#define rs	r4
#define rd	r2
#define rc	r5

// Should we bother using Altivec?

	cmpwi	r5, 128
	blt+	LScalar

// Determine whether we have Altivec enabled

	mflr    r0
	bcl	20,31,1f
1:
	mflr    r6
	mtlr    r0
	addis   r6, r6, ha16(__cpu_has_altivec - 1b)
	lwz     r6, lo16(__cpu_has_altivec - 1b)(r6)
	cmpwi	r6, 0
	bne+	LAltivec
	
// =================================================================================================

//  *****************************************
//  * S c a l a r B l o c k M o o f D a t a *
//  *****************************************
// 
//  This is the scalar (non-AltiVec) version of BlockMoofData.
// 
//		void ScalarBlockMoofData			(ptr sou, ptr dest, long len)
//		void ScalarBlockMoofDataUncached	(ptr sou, ptr dest, long len)
// 
// 
//  Calling Sequence: 	r3 = source pointer
// 						r4 = destination pointer
// 						r5 = length in bytes
// 
//  Uses: all volatile registers.

LScalar:
		cmplwi	cr7,rc,32				//  length <= 32 bytes?
		cmplw	cr6,rd,rs				//  up or down?
		mr.		r0,rc					//  copy to r0 for MoveShort, and test for negative
		bgt		cr7,Lbm1				//  skip if count > 32
		
//  Handle short moves (<=32 bytes.)

		beq		cr7,LMove32				//  special case 32-byte blocks
		blt		cr6,LMoveDownShort		//  move down in memory and return
		add		rs,rs,rc				//  moving up (right-to-left), so adjust pointers
		add		rd,rd,rc
		b		LMoveUpShort			//  move up in memory and return

//  Handle long moves (>32 bytes.)

Lbm1:
		beqlr	cr6						//  rs==rd, so nothing to move
		bltlr	cr0						//  length<0, so ignore call and return
		mflr	r12						//  save return address
		bge		cr6,Lbm2				//  rd>=rs, so move up

//  Long moves down (left-to-right.)

		neg		r6,rd					//  start to 32-byte-align destination
		andi.	r0,r6,0x1F				//  r0 <- bytes to move to align destination
		bnel	LMoveDownShort			//  align destination if necessary
		bl		LMoveDownLong			//  move 32-byte chunks down
		andi.	r0,rc,0x1F				//  done?
		mtlr	r12						//  restore caller's return address
		bne		LMoveDownShort			//  move trailing leftover bytes and done
		blr								//  no leftovers, so done
		
//  Long moves up (right-to-left.)

Lbm2:
		add		rs,rs,rc				//  moving up (right-to-left), so adjust pointers
		add		rd,rd,rc
		andi.	r0,rd,0x1F				//  r0 <- bytes to move to align destination
		bnel	LMoveUpShort			//  align destination if necessary
		bl		LMoveUpLong				//  move 32-byte chunks up
		andi.	r0,rc,0x1F				//  done?
		mtlr	r12						//  restore caller's return address
		bne		LMoveUpShort			//  move trailing leftover bytes and done
		blr								//  no leftovers, so done

//  ***************
//  * M O V E 3 2 *
//  ***************
// 
//  Special case subroutine to move a 32-byte block.  MoveDownShort and
//  MoveUpShort only handle 0..31 bytes, and we believe 32 bytes is too
//  common a case to send it through the general purpose long-block code.
//  Since it moves both up and down, we must load all 32 bytes before
//  storing any.
// 
//  Calling Sequence:  rs = source ptr
// 					 rd = destination ptr
// 
//  Uses: r0,r5-r11.
// 

LMove32:
		lwz		r0,0(rs)
		lwz		r5,4(rs)
		lwz		r6,8(rs)
		lwz		r7,12(rs)
		lwz		r8,16(rs)
		lwz		r9,20(rs)
		lwz		r10,24(rs)
		lwz		r11,28(rs)
		stw		r0,0(rd)
		stw		r5,4(rd)
		stw		r6,8(rd)
		stw		r7,12(rd)
		stw		r8,16(rd)
		stw		r9,20(rd)
		stw		r10,24(rd)
		stw		r11,28(rd)
		blr
		

//  *************************
//  * M o v e U p S h o r t *
//  *************************
// 
//  Subroutine called to move <32 bytes up in memory (ie, right-to-left).
// 
//  Entry conditions: rs = last byte moved from source (right-to-left)
// 					rd = last byte moved into destination
//					r0 = #bytes to move (0..31)
// 
//  Exit conditions:  rs = updated source ptr
// 					rd = updated destination ptr
//					rc = decremented by #bytes moved
// 
//  Uses: r0,r6,r7,r8,cr7.
// 

LMoveUpShort:
		andi.	r6,r0,0x10				//  test 0x10 bit in length
		mtcrf	0x1,r0					//  move count to cr7 so we can test bits
		sub		rc,rc,r0				//  decrement count of bytes remaining to be moved
		beq		Lmus1					//  skip if 0x10 bit in length is 0
		lwzu	r0,-16(rs)				//  set, so copy up 16 bytes
		lwz		r6,4(rs)
		lwz		r7,8(rs)
		lwz		r8,12(rs)
		stwu	r0,-16(rd)
		stw		r6,4(rd)
		stw		r7,8(rd)
		stw		r8,12(rd)

Lmus1:
		bf		28,Lmus2				//  test 0x08 bit
		lwzu	r0,-8(rs)
		lwz		r6,4(rs)
		stwu	r0,-8(rd)
		stw		r6,4(rd)

Lmus2:
		bf		29,Lmus3				//  test 0x4 bit
		lwzu	r0,-4(rs)
		stwu	r0,-4(rd)

Lmus3:
		bf		30,Lmus4				//  test 0x2 bit
		lhzu	r0,-2(rs)
		sthu	r0,-2(rd)

Lmus4:
		bflr	31						//  test 0x1 bit, return if 0
		lbzu	r0,-1(rs)
		stbu	r0,-1(rd)
		blr


//  *****************************
//  * M o v e D o w n S h o r t *
//  *****************************
// 
//  Subroutine called to move <32 bytes down in memory (ie, left-to-right).
// 
//  Entry conditions: rs = source pointer
// 					rd = destination pointer
//					r0 = #bytes to move (0..31)
// 
//  Exit conditions:  rs = ptr to 1st byte not moved
// 					rd = ptr to 1st byte not moved
//					rc = decremented by #bytes moved
// 
//  Uses: r0,r6,r7,r8,cr7.
// 

LMoveDownShort:
		andi.	r6,r0,0x10				//  test 0x10 bit in length
		mtcrf	0x1,r0					//  move count to cr7 so we can test bits
		sub		rc,rc,r0				//  decrement count of bytes remaining to be moved
		beq		Lmds1					//  skip if 0x10 bit in length is 0
		lwz		r0,0(rs)				//  set, so copy up 16 bytes
		lwz		r6,4(rs)
		lwz		r7,8(rs)
		lwz		r8,12(rs)
		addi	rs,rs,16
		stw		r0,0(rd)
		stw		r6,4(rd)
		stw		r7,8(rd)
		stw		r8,12(rd)
		addi	rd,rd,16

Lmds1:
		bf		28,Lmds2				//  test 0x08 bit
		lwz		r0,0(rs)
		lwz		r6,4(rs)
		addi	rs,rs,8
		stw		r0,0(rd)
		stw		r6,4(rd)
		addi	rd,rd,8

Lmds2:
		bf		29,Lmds3				//  test 0x4 bit
		lwz		r0,0(rs)
		addi	rs,rs,4
		stw		r0,0(rd)
		addi	rd,rd,4

Lmds3:
		bf		30,Lmds4				//  test 0x2 bit
		lhz		r0,0(rs)
		addi	rs,rs,2
		sth		r0,0(rd)
		addi	rd,rd,2

Lmds4:
		bflr	31						//  test 0x1 bit, return if 0
		lbz		r0,0(rs)
		addi	rs,rs,1
		stb		r0,0(rd)
		addi	rd,rd,1
		blr


//  ***********************
//  * M o v e U p L o n g *
//  ***********************
// 
//  Subroutine to move 32-byte chunks of memory up (ie, right-to-left.)
//  The destination is known to be 32-byte aligned, but the source is
//  *not* necessarily aligned.
// 
//  Entry conditions: rs = last byte moved from source (right-to-left)
// 					rd = last byte moved into destination
// 					rc = count of bytes to move
// 					cr = crCached set iff destination is cacheable
// 
//  Exit conditions:  rs = updated source ptr
// 					rd = updated destination ptr
// 					rc = low order 8 bits of count of bytes to move
// 
//  Uses: r0,r5-r11,fr0-fr3,ctr,cr0,cr6,cr7.
// 

LMoveUpLong:
		srwi.	r11,rc,5				// r11 <- #32 byte chunks to move
		mtctr	r11						//  prepare loop count
		beqlr							//  return if no chunks to move
		andi.	r0,rs,7					//  is source at least doubleword aligned?
		beq		Lmup3					//  yes, can optimize this case
		mtcrf	0x1,rc					//  save low bits of count
		mtcrf	0x2,rc					//  (one cr at a time, as 604 prefers)

Lmup1:									//  loop over each 32-byte-chunk
		lwzu	r0,-32(rs)
		subi	rd,rd,32				//  prepare destination address for 'dcbz'
		lwz		r5,4(rs)
		lwz		r6,8(rs)
		lwz		r7,12(rs)
		lwz		r8,16(rs)
		lwz		r9,20(rs)
		lwz		r10,24(rs)
		lwz		r11,28(rs)
		stw		r0,0(rd)
		stw		r5,4(rd)
		stw		r6,8(rd)
		stw		r7,12(rd)
		stw		r8,16(rd)
		stw		r9,20(rd)
		stw		r10,24(rd)
		stw		r11,28(rd)
		bdnz	Lmup1
		mfcr	rc						//  restore low bits of count
		blr								//  return to caller

//  Aligned operands, so use d.p. floating point registers to move data.

Lmup3:
		lfdu	f0,-32(rs)
		subi	rd,rd,32				//  prepare destination address for 'dcbz'
		lfd		f1,8(rs)
		lfd		f2,16(rs)
		lfd		f3,24(rs)
		stfd	f0,0(rd)
		stfd	f1,8(rd)
		stfd	f2,16(rd)
		stfd	f3,24(rd)
		bdnz	Lmup3
		blr								//  return to caller
		

//  ***************************
//  * M o v e D o w n L o n g *
//  ***************************
// 
//  Subroutine to move 32-byte chunks of memory down (ie, left-to-right.)
//  The destination is known to be 32-byte aligned, but the source is
//  *not* necessarily aligned.
// 
//  Entry conditions: rs = source ptr (next byte to move)
// 					rd = dest ptr (next byte to move into)
// 					rc = count of bytes to move
// 					cr = crCached set iff destination is cacheable
// 
//  Exit conditions:  rs = updated source ptr
// 					rd = updated destination ptr
// 					rc = low order 8 bits of count of bytes to move
// 
//  Uses: r0,r5-r11,fr0-fr3,ctr,cr0,cr6,cr7.
// 

LMoveDownLong:
		srwi.	r11,rc,5				// r11 <- #32 byte chunks to move
		mtctr	r11						//  prepare loop count
		beqlr							//  return if no chunks to move
		andi.	r0,rs,7					//  is source at least doubleword aligned?
		beq		Lmdown3					//  yes, can optimize this case
		mtcrf	0x1,rc					//  save low 8 bits of count
		mtcrf	0x2,rc					//  (one cr at a time, as 604 prefers)

Lmdown1:									//  loop over each 32-byte-chunk
		lwz		r0,0(rs)
		lwz		r5,4(rs)
		lwz		r6,8(rs)
		lwz		r7,12(rs)
		lwz		r8,16(rs)
		lwz		r9,20(rs)
		lwz		r10,24(rs)
		lwz		r11,28(rs)
		stw		r0,0(rd)
		stw		r5,4(rd)
		stw		r6,8(rd)
		stw		r7,12(rd)
		stw		r8,16(rd)
		stw		r9,20(rd)
		addi	rs,rs,32
		stw		r10,24(rd)
		stw		r11,28(rd)
		addi	rd,rd,32
		bdnz	Lmdown1
		mfcr	rc						//  restore low bits of count
		blr								//  return to caller

//  Aligned operands, so use d.p. floating point registers to move data.

Lmdown3:
		lfd		f0,0(rs)
		lfd		f1,8(rs)
		lfd		f2,16(rs)
		lfd		f3,24(rs)
		addi	rs,rs,32
		stfd	f0,0(rd)
		stfd	f1,8(rd)
		stfd	f2,16(rd)
		stfd	f3,24(rd)
		addi	rd,rd,32
		bdnz	Lmdown3
		blr								//  return to caller

//
// Register use conventions are as follows:
//
// r0 - temp
// r6 - copy of VMX SPR at entry
// r7 - temp
// r8 - constant -1 (also temp and a string op buffer)
// r9 - constant 16 or -17 (also temp and a string op buffer)
// r10- constant 32 or -33 (also temp and a string op buffer)
// r11- constant 48 or -49 (also temp and a string op buffer)
// r12- chunk count ("c") in long moves
//
// v0 - vp - permute vector
// v1 - va - 1st quadword of source
// v2 - vb - 2nd quadword of source
// v3 - vc - 3rd quadword of source
// v4 - vd - 4th quadword of source
// v5 - vx - temp
// v6 - vy - temp
// v7 - vz - temp

#define vp	v0
#define va	v1
#define vb	v2
#define vc	v3
#define vd	v4
#define vx	v5
#define vy	v6
#define vz	v7

#define VRSave	256

// kShort should be the crossover point where the long algorithm is faster than the short.
// WARNING: kShort must be >= 64

// Yes, I know, we just checked rc > 128 to get here...

#define kShort	128
LAltivec:
		cmpwi	cr1,rc,kShort		//(1) too short to bother using vector regs?
		sub.	r0,rd,rs			//(1) must move reverse if (rd-rs)<rc
		dcbt	0,rs				//(2) prefetch first source block
		cmplw	cr6,r0,rc			//(2) set cr6 blt iff we must move reverse
		beqlr-						//(2) done if src==dest
		srawi.	r9,rc,4				//(3) r9 <- quadwords to move, test for zero
		or		r8,rs,rd			//(3) start to check for word alignment
		dcbtst	0,rd				//(4) prefetch first destination block
		rlwinm	r8,r8,0,30,31		//(4) r8 is zero if word aligned
		bgt-	cr1,LMoveLong		//(4) handle long operands
		cmpwi	cr1,r8,0			//(5) word aligned?
		rlwinm	r7,rc,0,28,31		//(5) r7 <- leftover bytes to move after quadwords
		bltlr-						//(5) done if negative count
		blt-	cr6,LShortReverse 	//(5) handle reverse moves
		cmpwi	cr7,r7,0			//(6) leftover bytes?
		beq-	Leftovers			//(6) r9==0, so no quadwords to move
		mtctr	r9					//(7) set up for quadword loop
		bne-	cr1,LUnalignedLoop	//(7) not word aligned (less common than word aligned)

		
// <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>
// <><>                         S H O R T   O P E R A N D S                        <><> 
// <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>
		
LAlignedLoop:						// word aligned operands (the common case)
		lfd		f0,0(rs)			//(1)
		lfd		f1,8(rs)			//(2)
		addi	rs,rs,16			//(2)
		stfd	f0,0(rd)			//(3)
		stfd	f1,8(rd)			//(4)
		addi	rd,rd,16			//(4)
		bdnz	LAlignedLoop		//(4)
		
Leftovers:
		beqlr-	cr7					//(8) done if r7==0, ie no leftover bytes
		mtxer	r7					//(9) count of bytes to move (1-15)
		lswx	r8,0,rs
		stswx	r8,0,rd
 		blr							//(17)

LUnalignedLoop:						// not word aligned, cannot use lfd/stfd
		lwz		r8,0(rs)			//(1)
		lwz		r9,4(rs)			//(2)
		lwz		r10,8(rs)			//(3)
		lwz		r11,12(rs)			//(4)
		addi	rs,rs,16			//(4)
		stw		r8,0(rd)			//(5)
		stw		r9,4(rd)			//(6)
		stw		r10,8(rd)			//(7)
		stw		r11,12(rd)			//(8)
		addi	rd,rd,16			//(8)
		bdnz	LUnalignedLoop		//(8)
		
		b		Leftovers
		
		
// <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>
// <><>                   S H O R T   R E V E R S E   M O V E S                    <><> 
// <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>
		
		// cr0 & r9 <- #doublewords to move (>=0)
		// cr1      <- beq if word aligned
		//       r7 <- #leftover bytes to move (0-15)
		
LShortReverse:
		cmpwi	cr7,r7,0			// leftover bytes?
		add		rs,rs,rc			// point 1 past end of string for reverse moves
		add		rd,rd,rc
		beq-	LeftoversReverse 	// r9==0, ie no words to move
		mtctr	r9					// set up for quadword loop
		bne-	cr1,LUnalignedLoopReverse
		
LAlignedLoopReverse:					// word aligned, so use lfd/stfd
		lfd		f0,-8(rs)
		lfdu	f1,-16(rs)
		stfd	f0,-8(rd)
		stfdu	f1,-16(rd)
		bdnz	LAlignedLoopReverse
		
LeftoversReverse:
		beqlr-	cr7					// done if r7==0, ie no leftover bytes
		mtxer	r7					// count of bytes to move (1-15)
		neg		r7,r7				// index back by #bytes
		lswx	r8,r7,rs
		stswx	r8,r7,rd
		blr
		
LUnalignedLoopReverse:				// not word aligned, cannot use lfd/stfd
		lwz		r8,-4(rs)
		lwz 	r9,-8(rs)
		lwz		r10,-12(rs)
		lwzu	r11,-16(rs)
		stw		r8,-4(rd)
		stw		r9,-8(rd)
		stw		r10,-12(rd)
		stwu	r11,-16(rd)
		bdnz	LUnalignedLoopReverse
		
		b		LeftoversReverse
		
// <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>
// <><>                          L O N G   O P E R A N D S                         <><> 
// <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>

		// cr6 set (blt) if must move reverse
		// r0 <- (rd - rs)
			
LMoveLong:				
		mfspr	r6,VRSave			//(5) save caller's VMX mask register
		stw		r6,-4(r1)			// use CR save area so we can use r6 later
		neg		r8,rd				//(5) start to compute #bytes to fill in 1st dest quadword
		rlwinm	r0,r0,0,28,31		//(6) start to determine relative alignment
		andi.	r7,r8,0xF			//(6) r7 <- #bytes to fill in 1st dest quadword
		cmpwi	cr7,r0,0			//(7) relatively aligned? (ie, 16 bytes apart?)
		oris	r9,r6,0xFF00		//(7) light bits for regs we use (v0-v7)
		mtspr	VRSave,r9			//(8) update live register bitmask
		blt-	cr6,LongReverse		//(8) must move reverse direction
		sub		rc,rc,r7			//(9) adjust length while we wait
		beq-	LDest16Aligned		//(9) r7==0, ie destination already quadword aligned
		
		// Align destination on a quadword.
		
		mtxer	r7					//(10) set up byte count (1-15)
		lswx	r8,0,rs				// load into r8-r11
		stswx	r8,0,rd				// store r8-r11 (measured latency on arthur is 7.2 cycles)
		add		rd,rd,r7			//(18) adjust ptrs
		add		rs,rs,r7			//(18)
		
		// Begin preparation for inner loop and "dst" stream.
		
LDest16Aligned:
        andi.	r0,rd,0x10          //(19) is destination cache-block aligned?
		li		r9,16				//(19) r9 <- constant used to access 2nd quadword
		li		r10,32				//(20) r10<- constant used to access 3rd quadword
		beq-	cr7,LAligned		//(20) handle relatively aligned operands
		lvx		va,0,rs				//(20) prefetch 1st source quadword
		li		r11,48				//(21) r11<- constant used to access 4th quadword
		lvsl	vp,0,rs				//(21) get permute vector to left shift
		beq		LDest32Aligned		//(22) destination already cache-block aligned
		
		// Copy 16 bytes to align destination on 32-byte (cache block) boundary
		// to maximize store gathering.
		
		lvx		vb,r9,rs			//(23) get 2nd source qw
		subi	rc,rc,16			//(23) adjust count
		addi	rs,rs,16			//(24) adjust source ptr
		vperm	vx,va,vb,vp			//(25) vx <- 1st destination qw
		vor		va,vb,vb			//(25) va <- vb
		stvx	vx,0,rd				//(26) assuming store Q deep enough to avoid latency
		addi	rd,rd,16			//(26) adjust dest ptr
		
		// Destination 32-byte aligned, source alignment unknown.

LDest32Aligned:
		srwi.	r12,rc,6			//(27) r12<- count of 64-byte chunks to move
		rlwinm	r7,rc,28,30,31		//(27) r7 <- count of 16-byte chunks to move
		cmpwi	cr1,r7,0			//(28) remember if any 16-byte chunks
		rlwinm	r8,r12,0,26,31		//(29) mask chunk count down to 0-63
		subi	r0,r8,1				//(30) r8==0?
		beq-	LNoChunks			//(30) r12==0, ie no chunks to move
		rlwimi	r8,r0,0,25,25		//(31) if r8==0, then r8 <- 64
		li		r0,64				//(31) r0 <- used to get 1st quadword of next chunk
		sub.	r12,r12,r8			//(32) adjust chunk count, set cr0
		mtctr	r8					//(32) set up loop count
		li		r8,96				//SKP
		li		r6,128				//SKP
		// Inner loop for unaligned sources.  We copy 64 bytes per iteration.
		// We loop at most 64 times, then reprime the "dst" and loop again for
		// the next 4KB.  This loop is tuned to keep the CPU flat out, which
		// means we need to execute a lvx or stvx every cycle.
		
LoopBy64:
		dcbt	rs,r8				//SKP
		dcbt	rs,r6				//SKP
		lvx		vb,r9,rs			//(1) 2nd source quadword (1st already in va)
		lvx		vc,r10,rs			//(2) 3rd
		lvx		vd,r11,rs			//(3) 4th
		vperm	vx,va,vb,vp			//(3) vx <- 1st destination quadword
		lvx		va,rs,r0			//(4) get 1st qw of next 64-byte chunk (r0 must be RB!)
		vperm	vy,vb,vc,vp			//(4) vy <- 2nd dest qw
		stvx	vx,0,rd				//(5)
		vperm	vz,vc,vd,vp			//(5) vz <- 3rd dest qw
		stvx	vy,r9,rd			//(6)
		vperm	vx,vd,va,vp			//(6) vx <- 4th
		stvx	vz,r10,rd			//(7)
		addi	rs,rs,64			//(7)
		stvx	vx,r11,rd			//(8)
		addi	rd,rd,64			//(8)
		bdnz	LoopBy64			//(8)
		
		// End of inner loop.  Should we reprime dst stream and restart loop?
		// This block is only executed when we're moving more than 4KB.
		// It is usually folded out because cr0 is set in the loop prologue.
		
		beq+	LNoChunks			// r12==0, ie no more chunks to move
		sub.	r12,r12,r0			// set cr0 if more than 4KB remain to xfer
		mtctr	r0					// initialize loop count to 64
		b		LoopBy64			// restart inner loop, xfer another 4KB
		
		// Fewer than 64 bytes remain to be moved.
		
LNoChunks:							// r7 and cr1 are set with the number of QWs
		andi.	rc,rc,0xF			//(33) rc <- leftover bytes
		beq-	cr1,LCleanup		//(33) r7==0, ie fewer than 16 bytes remaining
		mtctr	r7					//(34) we will loop over 1-3 QWs

LoopBy16:
		lvx		vb,r9,rs			//(1) vb <- 2nd source quadword
		addi	rs,rs,16			//(1)
		vperm	vx,va,vb,vp			//(3) vx <- next destination quadword
		vor		va,vb,vb			//(3) va <- vb
		stvx	vx,0,rd				//(4) assuming store Q is deep enough to mask latency
		addi	rd,rd,16			//(4)
		bdnz	LoopBy16			//(4)
		
		// Move remaining bytes in last quadword.  rc and cr0 have the count.
		
LCleanup:
		lwz		r6,-4(r1)		    // load VRSave from CR save area
		mtspr	VRSave,r6			//(35) restore caller's live-register bitmask
		beqlr						//(36) rc==0, ie no leftovers, so done
		mtxer	rc					//(37) load byte count (1-15)
		lswx	r8,0,rs
		stswx	r8,0,rd
		blr							//(45)
		
		
// <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>
// <><>              L O N G   A L I G N E D   M O V E S                           <><> 
// <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>

		// rs, rd <- both quadword aligned
		// cr0 <- beq if dest is cache block (32-byte) aligned
		// r9  <- 16
		// r10 <- 32
		
LAligned:
		lvx		va,0,rs				// prefetch 1st source quadword
		li		r11,48				// r11<- constant used to access 4th quadword
		beq		LAligned32			// destination already cache-block aligned
		
		// Copy 16 bytes to align destination on 32-byte (cache block) boundary
		// to maximize store gathering.
		
		subi	rc,rc,16			// adjust count
		addi	rs,rs,16			// adjust source ptr
		stvx	va,0,rd				// assuming store Q deep enough to avoid latency
		addi	rd,rd,16			// adjust dest ptr
		
		// Destination 32-byte aligned, source 16-byte aligned.  Set up for inner loop.

LAligned32:
		srwi.	r12,rc,6			// r12<- count of 64-byte chunks to move
		rlwinm	r7,rc,28,30,31		// r7 <- count of 16-byte chunks to move
		cmpwi	cr1,r7,0			// remember if any 16-byte chunks
		rlwinm	r8,r12,0,26,31		// mask chunk count down to 0-63
		subi	r0,r8,1				// r8==0?
		beq-	LAlignedNoChunks	// r12==0, ie no chunks to move
		rlwimi	r8,r0,0,25,25		// if r8==0, then r8 <- 64
		li		r0,64				// r0 <- used at end of loop
		sub.	r12,r12,r8			// adjust chunk count, set cr0
		mtctr	r8					// set up loop count
		li		r8,96				//SKP
		li		r6,128				//SKP
		
		// Inner loop for aligned sources.  We copy 64 bytes per iteration.
		
LAlignedLoopBy64:
		dcbt	rs,r8				//SKP
		dcbt	rs,r6				//SKP
		lvx		va,0,rs				//(1)
		lvx		vb,r9,rs			//(2)
		lvx		vc,r10,rs			//(3)
		lvx		vd,r11,rs			//(4)
		addi	rs,rs,64			//(4)
		stvx	va,0,rd				//(5)
		stvx	vb,r9,rd			//(6)
		stvx	vc,r10,rd			//(7)
		stvx	vd,r11,rd			//(8)
		addi	rd,rd,64			//(8)
		bdnz	LAlignedLoopBy64	//(8)
		
		// End of inner loop.  Loop again for next 4KB iff any.
		
		beq+	LAlignedNoChunks	// r12==0, ie no more chunks to move
		sub.	r12,r12,r0			// set cr0 if more than 4KB remain to xfer
		mtctr	r0					// reinitialize loop count to 64
		b		LAlignedLoopBy64	// restart inner loop, xfer another 4KB
		
		// Fewer than 64 bytes remain to be moved.
		
LAlignedNoChunks:					// r7 and cr1 are set with the number of QWs
		andi.	rc,rc,0xF			// rc <- leftover bytes
		beq-	cr1,LCleanup		// r7==0, ie fewer than 16 bytes remaining
		mtctr	r7					// we will loop over 1-3 QWs

LAlignedLoopBy16:
		lvx		va,0,rs				// get next quadword
		addi	rs,rs,16
		stvx	va,0,rd
		addi	rd,rd,16
		bdnz	LAlignedLoopBy16
		
		b		LCleanup			// handle last 0-15 bytes, if any

		
// <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>
// <><>              L O N G   R E V E R S E   M O V E S                           <><> 
// <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>

		// Reverse moves.  These involve overlapping operands, with the source
		// lower in memory (lower addresses) than the destination.  They must be
		// done right-to-left, ie from high addresses down to low addresses.
		// Throughout this code, we maintain rs and rd as pointers one byte past
		// the end of the untransferred operands.
		//
		// The byte count is >=kShort and the following registers are already loaded:
		//
		//	r6  - VMX mask at entry
		//	cr7 - beq if relatively aligned
		//
		
LongReverse:
		add		rd,rd,rc			// update source/dest ptrs to be 1 byte past end
		add		rs,rs,rc
		andi.	r7,rd,0xF			// r7 <- #bytes needed to move to align destination
		sub		rc,rc,r7			// adjust length while we wait
		sub		rs,rs,r7			// adjust ptrs by #bytes to xfer, also while we wait
		sub		rd,rd,r7
		beq-	LDest16AlignedReverse
		
		// Align destination on a quadword.  Note that we do NOT align on a cache
		// block boundary for store gathering etc// since all these operands overlap
		// many dest cache blocks will already be in the L1, so its not clear that
		// this would be a win.
		
		mtxer	r7					// load byte count
		lswx	r8,0,rs
		stswx	r8,0,rd
		
		// Prepare for inner loop and start "dstst" stream.  Frankly, its not
		// clear whether "dst" or "dstst" would be better// somebody should
		// measure.  We use "dstst" because, being overlapped, at least some
		// source cache blocks will also be stored into.
		
LDest16AlignedReverse:
		srwi.	r12,rc,6			// r12 <- count of 64-byte chunks to move
		rlwinm	r0,rc,11,9,15		// position quadword count for dst
		rlwinm	r11,r12,0,26,31		// mask chunk count down to 0-63
		li		r9,-17				// r9 <- constant used to access 2nd quadword
		oris	r0,r0,0x0100		// set dst block size to 1 qw
		li		r10,-33				// r10<- constant used to access 3rd quadword
		ori		r0,r0,0xFFE0		// set dst stride to -16 bytes
		li		r8,-1				// r8<- constant used to access 1st quadword
		dstst	rs,r0,3				// start stream 0
		subi	r0,r11,1			// r11==0 ?
		lvx		va,r8,rs			// prefetch 1st source quadword
		rlwinm	r7,rc,28,30,31		// r7 <- count of 16-byte chunks to move
		lvsl	vp,0,rs				// get permute vector to right shift
		cmpwi	cr1,r7,0			// remember if any 16-byte chunks
		beq-	LNoChunksReverse	// r12==0, so skip inner loop
		rlwimi	r11,r0,0,25,25		// if r11==0, then r11 <- 64
		sub.	r12,r12,r11			// adjust chunk count, set cr0
		mtctr	r11					// set up loop count
		li		r11,-49				// r11<- constant used to access 4th quadword
		li		r0,-64				// r0 <- used for several purposes
		beq-	cr7,LAlignedLoopBy64Reverse
		
		// Inner loop for unaligned sources.  We copy 64 bytes per iteration.

LoopBy64Reverse:
		lvx		vb,r9,rs			//(1) 2nd source quadword (1st already in va)
		lvx		vc,r10,rs			//(2) 3rd quadword
		lvx		vd,r11,rs			//(3) 4th
		vperm	vx,vb,va,vp			//(3) vx <- 1st destination quadword
		lvx		va,rs,r0			//(4) get 1st qw of next 64-byte chunk (note r0 must be RB)
		vperm	vy,vc,vb,vp			//(4) vy <- 2nd dest qw
		stvx	vx,r8,rd			//(5)
		vperm	vz,vd,vc,vp			//(5) vz <- 3rd destination quadword
		stvx	vy,r9,rd			//(6)
		vperm	vx,va,vd,vp			//(6) vx <- 4th qw
		stvx	vz,r10,rd			//(7)
		subi	rs,rs,64			//(7)
		stvx	vx,r11,rd			//(8)
		subi	rd,rd,64			//(8)
		bdnz	LoopBy64Reverse		//(8)
		
		// End of inner loop.  Should we reprime dst stream and restart loop?
		// This block is only executed when we're moving more than 4KB.
		// It is usually folded out because cr0 is set in the loop prologue.
		
		beq+	LNoChunksReverse	// r12==0, ie no more chunks to move
		lis		r8,0x0440			// dst control: 64 4-qw blocks
		add.	r12,r12,r0			// set cr0 if more than 4KB remain to xfer
		ori		r8,r8,0xFFC0		// stride is -64 bytes
		dstst	rs,r8,3				// restart the prefetch stream
		li		r8,64				// inner loop count
		mtctr	r8					// initialize loop count to 64
		li		r8,-1				// restore qw1 offset for inner loop
		b		LoopBy64Reverse		// restart inner loop, xfer another 4KB
		
		// Fewer than 64 bytes remain to be moved.
		
LNoChunksReverse:					// r7 and cr1 are set with the number of QWs
		andi.	rc,rc,0xF			// rc <- leftover bytes
		beq-	cr1,LCleanupReverse	// r7==0, ie fewer than 16 bytes left
		mtctr	r7
		beq-	cr7,LAlignedLoopBy16Reverse

LoopBy16Reverse:
		lvx		vb,r9,rs			// vb <- 2nd source quadword
		subi	rs,rs,16
		vperm	vx,vb,va,vp			// vx <- next destination quadword
		vor		va,vb,vb			// va <- vb
		stvx	vx,r8,rd
		subi	rd,rd,16
		bdnz	LoopBy16Reverse
		
		// Fewer that 16 bytes remain to be moved.
		
LCleanupReverse:					// rc and cr0 set with remaining byte count
		lwz		r6,-4(r1)			// load VRSave from CR save area
		mtspr	VRSave,r6			// restore caller's live-register bitmask
		beqlr						// rc==0, ie no leftovers so done
		neg		r7,rc				// get -(#bytes)
		mtxer	rc					// byte count
		lswx	r8,r7,rs
		stswx	r8,r7,rd
		blr

		
// <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>
// <><>        A L I G N E D   L O N G   R E V E R S E   M O V E S                 <><> 
// <><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><><>

		// Inner loop.  We copy 64 bytes per iteration.

LAlignedLoopBy64Reverse:
		lvx		va,r8,rs			//(1)
		lvx		vb,r9,rs			//(2)
		lvx		vc,r10,rs			//(3)
		lvx		vd,r11,rs			//(4) 
		subi	rs,rs,64			//(4)
		stvx	va,r8,rd			//(5)
		stvx	vb,r9,rd			//(6)
		stvx	vc,r10,rd			//(7)
		stvx	vd,r11,rd			//(8)
		subi	rd,rd,64			//(8)
		bdnz	LAlignedLoopBy64Reverse //(8)
		
		// End of inner loop.  Loop for next 4KB iff any.
		
		beq+	LNoChunksReverse	// r12==0, ie no more chunks to move
		lis		r8,0x0440			// dst control: 64 4-qw blocks
		add.	r12,r12,r0			// r12 <- r12 - 64, set cr0
		ori		r8,r8,0xFFC0		// stride is -64 bytes
		dstst	rs,r8,3				// restart the prefetch stream
		li		r8,64				// inner loop count
		mtctr	r8					// initialize loop count to 64
		li		r8,-1				// restore qw1 offset for inner loop
		b		LAlignedLoopBy64Reverse

		// Loop to copy leftover quadwords (1-3).
		
LAlignedLoopBy16Reverse:
		lvx		va,r8,rs			// get next qw
		subi	rs,rs,16
		stvx	va,r8,rd
		subi	rd,rd,16
		bdnz	LAlignedLoopBy16Reverse
		
		b		LCleanupReverse		// handle up to 15 bytes in last qw
