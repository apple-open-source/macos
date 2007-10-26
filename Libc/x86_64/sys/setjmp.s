/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
 * Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved
 */
/*
 * NeXT 386 setjmp/longjmp
 *
 * Written by Bruce Martin, NeXT Inc. 4/9/92
 */

/*
 * C library -- setjmp, longjmp
 *
 *	longjmp(a,v)
 * will generate a "return(v)" from
 * the last call to
 *	setjmp(a)
 * by restoring registers from the stack,
 * The previous value of the signal mask is
 * restored.
 *
 */

#include <architecture/i386/asm_help.h>
#include <SYS.h>

#define JB_RBX			0
#define JB_RBP			8
#define JB_RSP			16
#define JB_R12			24
#define JB_R13			32
#define JB_R14			40
#define JB_R15			48
#define JB_RIP			56
#define JB_RFLAGS		64
#define JB_MXCSR		72
#define JB_FPCONTROL	76
#define JB_MASK			80
#define JB_SAVEMASK		84		// sigsetjmp/siglongjmp only


LEAF(_sigsetjmp, 0)
	// %rdi is sigjmp_buf * jmpbuf;
	// %esi is int savemask
	movl	%esi, JB_SAVEMASK(%rdi) // jmpbuf[_JBLEN] = savemask;
	cmpl	$0, %esi		// if savemask != 0
	jne	_setjmp			 // setjmp(jmpbuf);
	jmp	L_do__setjmp		// else _setjmp(jmpbuf);

LEAF(_setjmp, 0)
	pushq	%rdi			// Preserve the jmp_buf across the call
	movl	$1, %edi		// how = SIG_BLOCK
	xorq	%rsi, %rsi	      // set = NULL
	subq	$16, %rsp		// Allocate space for the return from sigprocmask + 8 to align stack
	movq	%rsp, %rdx		// oset = allocated space
	CALL_EXTERN(_sigprocmask)
	popq	%rax			// Save the mask
	addq	$8, %rsp		// Restore the stack to before we align it
	popq	%rdi			// jmp_buf (struct sigcontext *)
	movq	%rax, JB_MASK(%rdi)
L_do__setjmp:
	BRANCH_EXTERN(__setjmp)

LEAF(_siglongjmp, 0)
	// %rdi is sigjmp_buf * jmpbuf;
	cmpl	$0, JB_SAVEMASK(%rdi)      // if jmpbuf[_JBLEN] != 0
	jne	_longjmp		//     longjmp(jmpbuf, var);
	jmp	L_do__longjmp	       // else _longjmp(jmpbuf, var);

LEAF(_longjmp, 0)
	// %rdi is address of jmp_buf (saved context)
	pushq	%rdi				// Preserve the jmp_buf across the call
	pushq	%rsi				// Preserve the value across the call
	pushq	JB_MASK(%rdi)		// Put the mask on the stack
	movq	$3, %rdi			// how = SIG_SETMASK
	movq	%rsp, %rsi			// set = address where we stored the mask
	xorq	%rdx, %rdx			// oset = NULL
	CALL_EXTERN_AGAIN(_sigprocmask)
	addq	$8, %rsp
	popq	%rsi				// Restore the value
	popq	%rdi				// Restore the jmp_buf
L_do__longjmp:
	BRANCH_EXTERN(__longjmp)	// else
END(_longjmp)
