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
/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 * Adapted for exp2 by Stephen C. Peters
 */

#include <machine/asm.h>

#include "abi.h"
#warning scp: edge case +-Inf

ENTRY(exp2)
#ifdef __i386__
	pushl	%ebp
	movl	%esp,%ebp
	subl	$8,%esp

	fstcw	-12(%ebp)		/* store fpu control word */
	movw	-12(%ebp),%dx
	orw	$0x0180,%dx
	movw	%dx,-16(%ebp)
	fldcw	-16(%ebp)		/* load modfied control word */
	fldl	8(%ebp)
#else
	fstcw	-12(%rsp)
	movw	-12(%rsp),%dx
	orw	$0x0180,%dx
	movw	%dx,-16(%rsp)
	fldcw	-16(%rsp)
	movsd	%xmm0,-8(%rsp)
	fldl	-8(%rsp)
#endif

	fld	%st(0)
	frndint				/* int(x) */
	fxch	%st(1)
	fsub	%st(1),%st		/* fract(x) */
	f2xm1				/* 2^(fract(x)) - 1 */
	fld1
	faddp				/* 2^(fract(x)) */
	fscale				/* 2^x */
	fstp	%st(1)

#ifdef __i386__
	fldcw	-12(%ebp)		/* restore original control word */
	leave
#else
	fstpl	-8(%rsp)
	movsd	-8(%rsp),%xmm0
	fldcw	-12(%rsp)
#endif
	ret
