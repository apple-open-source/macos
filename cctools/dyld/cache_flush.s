/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
#ifdef sparc
/*
 * sparc_cache_flush() take a double word address and flushes the cache line
 * for that address.
 *
 * extern void sparc_cache_flush(
 *    unsigned long double_word_address);
 */
        .text
	.align 2
.globl _sparc_cache_flush
_sparc_cache_flush:
        retl
        flush %o0               /* flush the address in the first paramter */
#endif /* sparc */

#ifdef __ppc__
/*
 * ppc_cache_flush() takes a byte address and a size and invalidates the cache 
 * blocks for those addresses.
 *
 * extern void ppc_cache_flush(
 *    unsigned long byte_address,
 *    unsigned long size);
 */
kCacheLineSize = 32
kLog2CacheLineSize = 5
        .text
	.align 2
.globl _ppc_cache_flush
_ppc_cache_flush:
	cmpwi	cr0,r4,0		; is this zero length?
	add	r4,r3,r4		; calculate last byte + 1
	subi	r4,r4,1			; calculate last byte
		
	srwi	r5,r3,kLog2CacheLineSize ; calculate first cache line index
	srwi	r4,r4,kLog2CacheLineSize ; calculate last cache line index
	beq	cr0,dataToCodeDone	; done if zero length

	subf	r4,r5,r4	; calculate difference (number of lines minus 1)
	addi	r4,r4,1			; number of cache lines to flush
	slwi	r5,r5,kLog2CacheLineSize ; calculate address of first cache line

; flush the data cache lines		
	mr	r3,r5			; starting address for loop
	mtctr	r4			; loop count
dataToCodeFlushLoop:
	dcbf	0, r3			; flush the data cache line
	addi	r3,r3,kCacheLineSize	; advance to next cache line
	bdnz	dataToCodeFlushLoop	; loop until count is zero
	sync				; wait until RAM is valid

; invalidate the code cache lines	
	mr	r3,r5			; starting address for loop
	mtctr	r4			; loop count
dataToCodeInvalidateLoop:
	icbi	0, r3			; invalidate the code cache line
	addi	r3,r3,kCacheLineSize	; advance to next cache line
	bdnz	dataToCodeInvalidateLoop ; loop until count is zero
	sync				; wait until last icbi completes
	isync				; discard prefetched instructions, too

dataToCodeDone:
	blr
#endif /* __ppc__ */
