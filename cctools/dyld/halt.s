/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
/*
 * halt() function causes a breakpoint trap.  This is used when available and
 * is called before exit() to cause a crash that maybe the debugger or the
 * crashcatcher app will catch.  This allows the programmer to see the stack
 * trace that caused the problem.
 */
	.text
	.align 2
	.globl _halt
_halt:

#ifdef __ppc__
	trap
	blr
#endif /* __ppc__ */

#ifdef __i386__
	int3
#endif /* __i386__ */

#ifdef m68k
	rts
#endif

#ifdef hppa
	bv,n	0(%r2)
#endif

#ifdef sparc
	retl
#endif
