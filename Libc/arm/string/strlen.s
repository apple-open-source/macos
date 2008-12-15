/*
 * Copyright (c) 2006 Apple Computer, Inc. All rights reserved.
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

.text
	.align 2

	.globl _strlen
/* size_t strlen(const char *s); */
_strlen:
	/* save the original pointer */
	mov		r12, r0

	/* see if the string is aligned */
	ands	r3, r0, #3

	/* load the first word, address rounded down */
	bic		r0, r0, #3
	ldr		r2, [r0], #4

	/* skip the next part if the string is already aligned */
	beq		Laligned

Lunaligned:
	/* unaligned pointer, mask out the bytes that we've read that we should be ignoring */
	cmp		r3, #2
	orr		r2, r2, #0x000000ff
	orrge	r2, r2, #0x0000ff00
	orrgt	r2, r2, #0x00ff0000

Laligned:
	/* load 0x01010101 into r1 */
	mov		r1, #0x01
	orr		r1, r1, r1, lsl #8
	orr		r1, r1, r1, lsl #16

Laligned_loop:
	/* ((x - 0x01010101) & ~x & 0x80808080) == hasnull(word) */
	sub		r3, r2, r1		/* x - 0x01010101 */
	bic		r3, r3, r2		/* above & ~x */
	tst		r3, r1, lsl #7	/* above & 0x80808080 */
	ldreq	r2, [r0], #4	/* load next word */
	beq		Laligned_loop

	/* we found a nullbyte */
	/* r0 (ptr) has overshot by up to 4 bytes, so subtract off until we find a nullbyte */
	sub		r0, r0, #1
	tst		r2, #0x000000ff
	subeq	r0, r0, #1
	tstne	r2, #0x0000ff00
	subeq	r0, r0, #1
	tstne	r2, #0x00ff0000
	subeq	r0, r0, #1

Lexit:
	/* len = ptr - original pointer */
	sub		r0, r0, r12
	bx		lr

