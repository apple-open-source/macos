/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
#include "SYS.h"

#define JB_ONSTACK	0
#define JB_MASK		4
#define JB_EAX		8
#define JB_EBX		12
#define JB_ECX		16
#define JB_EDX		20
#define JB_EDI		24
#define JB_ESI		28
#define JB_EBP		32
#define JB_ESP		36
#define JB_SS		40
#define JB_EFLAGS	44
#define JB_EIP		48
#define JB_CS		52
#define JB_DS		56
#define JB_ES		60
#define JB_FS		64
#define JB_GS		68
#define JB_SAVEMASK	72	// sigsetjmp/siglongjmp only

LEAF(_sigsetjmp, 0)
	movl	4(%esp), %eax 		// sigjmp_buf * jmpbuf; 
	movl	8(%esp), %ecx		// int savemask;
	movl	%ecx, JB_SAVEMASK(%eax)	// jmpbuf[_JBLEN] = savemask;
	cmpl	$0, %ecx		// if savemask != 0
	jne	_setjmp			//     setjmp(jmpbuf); 
	BRANCH_EXTERN(__setjmp)		// else
					//     _setjmp(jmpbuf); 
	
LEAF(_setjmp, 0)
	movl	4(%esp), %ecx		// jmp_buf (struct sigcontext *)
	pushl	%ecx			// save ecx

	// call sigstack to get the current signal stack
	subl	$12, %esp		// space for return structure
	pushl	%esp
	pushl	$0
	CALL_EXTERN(_sigaltstack)
	movl	12(%esp), %eax		// save stack pointer
	movl	%eax, JB_ONSTACK(%ecx)
	addl	$20, %esp

	// call sigblock to get signal mask
	pushl	$0
	CALL_EXTERN(_sigblock)
	addl	$4, %esp
	popl	%ecx			// restore ecx
	movl	%eax, JB_MASK(%ecx)

	// now build sigcontext
	movl	%ebx, JB_EBX(%ecx)
	movl	%edi, JB_EDI(%ecx)
	movl	%esi, JB_ESI(%ecx)
	movl	%ebp, JB_EBP(%ecx)

	// EIP is set to the frame return address value
	movl	(%esp), %eax
	movl	%eax, JB_EIP(%ecx)
	// ESP is set to the frame return address plus 4
	movl	%esp, %eax
	addl	$4, %eax
	movl	%eax, JB_ESP(%ecx)

	// segment registers
	movl	$0,  JB_SS(%ecx)
	mov	%ss, JB_SS(%ecx)
	movl	$0,  JB_CS(%ecx)
	mov	%cs, JB_CS(%ecx)
	movl	$0,  JB_DS(%ecx)
	mov	%ds, JB_DS(%ecx)
	movl	$0,  JB_ES(%ecx)
	mov	%es, JB_ES(%ecx)
	movl	$0,  JB_FS(%ecx)
	mov	%fs, JB_FS(%ecx)
	movl	$0,  JB_GS(%ecx)
	mov	%gs, JB_GS(%ecx)

	// save eflags - you can't use movl
	pushf
	popl	%eax
	movl	%eax, JB_EFLAGS(%ecx)

	// return 0
	xorl	%eax, %eax
	ret

LEAF(_siglongjmp, 0)
	movl 4(%esp), %eax		// sigjmp_buf * jmpbuf; 
	cmpl $0, JB_SAVEMASK(%eax)	// if jmpbuf[_JBLEN] != 0
	jne 	_longjmp		//     longjmp(jmpbuf, var); 
	BRANCH_EXTERN(__longjmp)	// else
					//     _longjmp(jmpbuf, var); 
	
LEAF(_longjmp, 0)
	subl	$2,%esp
	fnstcw	(%esp)			// save FP control word
	fninit				// reset FP coprocessor
	fldcw	(%esp)			// restore FP control word
	addl	$2,%esp
	movl	4(%esp), %eax		// address of jmp_buf (saved context)
	movl	8(%esp), %edx		// return value
	movl	%edx, JB_EAX(%eax)	// return value into saved context
	movl	$ SYS_sigreturn, %eax	// sigreturn system call
	UNIX_SYSCALL_TRAP
	addl	$8, %esp
	CALL_EXTERN(_longjmperror)
	CALL_EXTERN(_abort)
END(_longjmp)
