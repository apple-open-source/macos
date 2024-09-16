/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*
 * @OSF_COPYRIGHT@
 */
/* 
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

#ifndef	_ARM_ASM_H_
#define	_ARM_ASM_H_

#ifdef __arm64__

#include <arm/arch.h>
#if XNU_KERNEL_PRIVATE
#include <pexpert/arm64/board_config.h>
#endif /* XNU_KERNEL_PRIVATE */

/* There is another definition of ALIGN for .c sources */
#ifdef __ASSEMBLER__
#define ALIGN 2
#endif /* ASSEMBLER */

#ifndef FALIGN
#define FALIGN ALIGN
#endif

#define LB(x,n) n
#if	__STDC__
#ifndef __NO_UNDERSCORES__
#define	LCL(x)	L ## x
#define EXT(x) _ ## x
#define LEXT(x) _ ## x ## :
#else
#define	LCL(x)	.L ## x
#define EXT(x) x
#define LEXT(x) x ## :
#endif
#define LBc(x,n) n ## :
#define LBb(x,n) n ## b
#define LBf(x,n) n ## f
#else /* __STDC__ */
#ifndef __NO_UNDERSCORES__
#define LCL(x) L/**/x
#define EXT(x) _/**/x
#define LEXT(x) _/**/x/**/:
#else /* __NO_UNDERSCORES__ */
#define	LCL(x)	.L/**/x
#define EXT(x) x
#define LEXT(x) x/**/:
#endif /* __NO_UNDERSCORES__ */
#define LBc(x,n) n/**/:
#define LBb(x,n) n/**/b
#define LBf(x,n) n/**/f
#endif /* __STDC__ */

#define String	.asciz
#define Value	.word
#define Times(a,b) (a*b)
#define Divide(a,b) (a/b)

#ifdef __ASSEMBLER__
#if	MACH_KDB
#include <ddb/stab.h>
/*
 * This pseudo-assembler line is added so that there will be at least
 *	one N_SO entry in the symbol stable to define the current file name.
 */
#endif	/* MACH_KDB */

/*
 * Multiline macros must use .macro syntax for now,
 * as there is no ARM64 statement separator.
 */
.macro ENTRY 
	.align FALIGN
	.globl _$0 
	_$0 : 
.endmacro

.macro ENTRY2
	.align FALIGN
	.globl _$0 
	.globl _$1 
	_$0 :
	_$1 :
.endmacro

.macro READ_THREAD
	mrs $0, TPIDR_EL1
.endmacro

.macro BRANCH_EXTERN
	b _$0 
.endmacro

.macro CALL_EXTERN
	bl _$0 
.endmacro

.macro MOV64
	movk $0, #((($1) >> 48) & 0x000000000000FFFF), lsl #48
	movk $0, #((($1) >> 32) & 0x000000000000FFFF), lsl #32
	movk $0, #((($1) >> 16) & 0x000000000000FFFF), lsl #16
	movk $0, #((($1) >> 00) & 0x000000000000FFFF), lsl #00
.endmacro

.macro MOV32
	movz $0, #((($1) >> 16) & 0x000000000000FFFF), lsl #16
	movk $0, #((($1) >> 00) & 0x000000000000FFFF), lsl #00
.endmacro

.macro ARM64_STACK_PROLOG
#if __has_feature(ptrauth_returns)
	pacibsp
#endif
.endmacro

.macro ARM64_STACK_EPILOG
#if __has_feature(ptrauth_returns)
	retab
#else
	ret
#endif
.endmacro

/**
 * Push a stack frame.
 *
 * Most callers should invoke ARM64_STACK_PROLOG first, since otherwise this will
 * push an unsigned return address onto the stack.
 */
#define PUSH_FRAME			\
	stp fp, lr, [sp, #-16]!		%% \
	mov fp, sp			%%

/**
 * Pop the most recent stack frame.
 *
 * Note: if the complementary PUSH_FRAME was not preceded by ARM64_STACK_PROLOG,
 * then this operation could load an attacker-controlled return address from
 * memory!  Either add ARM64_STACK_PROLOG, or use POP_FRAME_WITHOUT_LR if there
 * are no plans to ever use the popped LR.
 */
#define POP_FRAME			\
	mov sp, fp			%% \
	ldp fp, lr, [sp], #16		%%

/**
 * Pop the most recent stack frame, but do not update LR.
 *
 * This macro is intended for situations like kernel entry, where the caller
 * doesn't actually need to preserve LR, but wants to push a stack frame
 * anyway for the benefit of unwinders.
 */
#define POP_FRAME_WITHOUT_LR		\
	mov sp, fp			%% \
	ldp fp, xzr, [sp], #16		%%

#define EXT(x) _ ## x

#ifdef  XNU_KERNEL_PRIVATE
.macro PANIC_UNIMPLEMENTED
	bl EXT(panic_unimplemented)
.endmacro

/**
 * Marks the first instruction of an externally callable assembly routine.
 *
 * Callable functions which might be executed via indirect branches (i.e. any 
 * function which could be invoked from C/C++) MUST use either this macro or 
 * ARM64_STACK_PROLOG, depending on whether LR will be spilled to the stack. 
 * Failure to use one of these macros may cause unexpected runtime BTI 
 * exceptions when invoking a function.
 */
.macro ARM64_PROLOG
#if BTI_ENFORCED
    bti c
#endif /* BTI_ENFORCED */
.endmacro

/**
 * Marks the first instruction of an indirect branch target.
 *
 * This macro is not required if the location is only ever branched to by direct
 * branches (as is most common). This macro is necessary for computed
 * branches (i.e. switch cases) and other forms of dynamic dispatch.
 */
.macro ARM64_JUMP_TARGET
#if BTI_ENFORCED
    bti j
#endif /* BTI_ENFORCED */
.endmacro
#endif /* XNU_KERNEL_PRIVATE */
#else /* NOT __ASSEMBLER__ */

/* These defines are here for .c files that wish to reference global symbols
 * within __asm__ statements. 
 */
#ifndef __NO_UNDERSCORES__
#define CC_SYM_PREFIX "_"
#else
#define CC_SYM_PREFIX ""
#endif /* __NO_UNDERSCORES__ */
#endif /* __ASSEMBLER__ */

#ifdef __ASSEMBLER__

# define BRANCH_EXTERN(x)	b	EXT(x)

#endif /* __ASSEMBLER__ */

#endif /* __arm64__ */

#endif /* _ARM_ASM_H_ */
