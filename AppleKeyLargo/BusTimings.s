/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All rights reserved.
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
;		 DRI: Dave Radcliffe
;
;			More accurately calculate bus timings to minimize drift.
;
;			Borrows from Bob Bradleys version in Mac OS 9 (BPPC:FCodeSrcs:BusTiming.s)
;
;
#include <ppc/asm.h>

#define   kKeyLargoCounterLoOffset	0x15038
#define   kKeyLargoCounterHiOffset	0x1503C

;
; TimeSystemBusKeyLargo(inKeyLargoBaseAddress)
;
; TimeSystemBusKeyLargo - Times how long it takes the PowerPC decrementer to count down
; 1,048,575 ticks.
;
; returns, in r3, the number of KeyLargo timer ticks per 1,048,575 PowerPC decrementer ticks.
;
; trashes r3 - r10
;
; NOTE - interrupts should be disabled when calling this code
;



ENTRY(TimeSystemBusKeyLargo, TAG_NO_FRAME_USED)
			
			lis		r4, 0x000F
			ori		r4, r4, 0xFFFF		; Load decrementer tick count (1,048,575)
			li		r8, 0				; Load KeyLargo start time (zero)
			lis		r6, kKeyLargoCounterLoOffset >> 16
			ori		r6, r6, kKeyLargoCounterLoOffset & 0xFFFF ; Counter lo offset
			lis		r7, kKeyLargoCounterHiOffset >> 16
			ori		r7, r7, kKeyLargoCounterHiOffset & 0xFFFF ; Counter hi offset
			stwbrx	r8, r6, r3			; Set low 32-bits to zero (latches all 64 bits)
			eieio
			stwbrx	r8, r7, r3			; Set high 32-bits to zero
			eieio
			
			; Set up decrementer and wait for it to tick down
			
			mtdec	r4					; Set decrementer to 1,048,575
			isync
			
DecrementerLoop:
			mfdec	r5					; Read current decrementer value
			cmpwi	r5, 0				; Check if decrementer is zero
			bgt+	DecrementerLoop		; If not yet to zero, keep looping
			sync
			
			; Read current value of KeyLargo to get delta time
			
			lwbrx	r5, r6, r3			; Load low 32-bits of timer (latches all 64 bits)
			lwbrx	r10, r7, r3			; Load high 32-bits of timer
			mr		r3, r5				; Copy timing result to output
			
			blr							; Return
