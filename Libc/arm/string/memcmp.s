/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
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

// ARM Assembly implementation of memcmp( ) from <string.h>
// Uses Thumb2 if it is available, otherwise generates ARM code.
//
// -- Stephen Canon, August 2009
//
// The basic idea is to use word compares instead of byte compares as long as
// at least four bytes remain to be compared.  However, because memcmp( )
// compares the buffers as though they were big-endian unsigned integers, we
// need to byte-reverse each word before comparing them.
//
// If the buffers are not word aligned, or they are shorter than four bytes,
// we just use a simple byte comparison loop instead.
//
// int   bcmp(void *src1, void *src2, size_t length);
// int memcmp(void *src1, void *src2, size_t length);

#include <arm/arch.h>

    .text
    .syntax unified
#if defined __thumb2__
    .code 16
    .thumb_func _bcmp
    .thumb_func _memcmp
#else
    .code 32
#endif
    .globl _bcmp
    .globl _memcmp
    .align 3
_bcmp:
_memcmp:

#ifdef _ARM_ARCH_6
    subs    ip,     r2,  #4     // if length < 4
    bmi     L_useByteCompares   // jump to the byte comparison loop
    
    orr     r3,     r0,  r1     // if the buffers are
    tst     r3,          #3     // not word aligned
    bne     L_useByteCompares   // jump to the byte comparison loop

.align 3
L_wordCompare:                  // Here we know that both buffers are word
    ldr     r2,    [r0], #4     // aligned, and (length - 4) > 0, so at least
    ldr     r3,    [r1], #4     // four bytes remain to be compared.  We load
    subs    ip,          #4     // a word from each buffer, and byte reverse
    bmi     L_lastWord          // the loaded words.  We also decrement the
    rev     r2,     r2          // length by four and jump out of this loop if
    rev     r3,     r3          // the result is negative.  Then we compare the
    cmp     r2,     r3          // reversed words, and continue the loop only
    beq     L_wordCompare       // if they are equal.
L_wordsUnequal:
    ite     hi                  // If the words compared unequal, return +/- 1
    movhi   r0,     #1          // according to the result of the comparison.
    movls   r0,     #-1         //
    bx      lr                  //
L_lastWord:
    rev     r2,     r2          // If we just loaded the last complete words
    rev     r3,     r3          // from the buffers, byte-reverse them and
    cmp     r2,     r3          // compare.  If they are unequal, jump to the
    bne     L_wordsUnequal      // return path.
    add     r2,     ip,  #4     // Otherwise, fall into the cleanup code.
#endif // _ARM_ARCH_6

L_useByteCompares:
    tst     r2,     r2          // If the length is exactly zero
    beq     L_returnZero        // avoid doing any loads and return zero.
    mov     r3,     r0
.align 3
L_byteCompareLoop:
    ldrb    r0,    [r3], #1     // Load a byte from each buffer, and decrement
    ldrb    ip,    [r1], #1     // the length by one.  If the decremented
    subs    r2,     #1          // length is zero, exit the loop.  Otherwise
    beq     L_lastByte          // subtract the loaded bytes; if their
    subs    r0,     ip          // difference is zero, continue the comparison
    beq     L_byteCompareLoop   // loop.  Otherwise, return their difference.
    bx      lr
L_returnZero:
    mov     r0,     ip
L_lastByte:
    sub     r0,     ip          // Return the difference of the final bytes
    bx      lr
