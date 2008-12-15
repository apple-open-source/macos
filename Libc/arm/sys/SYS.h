/*
 * Copyright (c) 1999-2006 Apple Computer, Inc. All rights reserved.
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
 * Header files.
 */
#import	<sys/syscall.h>
#define SWI_SYSCALL 0x80	// from <mach/vm_param.h>

/*
 * ARM system call interface:
 *
 * swi 0x80
 * args: r0-r6
 * return code: r0
 * on error, carry bit is set in the psr, otherwise carry bit is cleared.
 */

/*
 * Macros.
 */

/*
 * until we update the architecture project, these live here
 */

#if defined(__DYNAMIC__)
#define MI_GET_ADDRESS(reg,var)  \
	ldr	reg, 4f					;\
3:	ldr	reg, [pc, reg]				;\
	b	5f					;\
4:	.long	6f - (3b + 8)				;\
5:							;\
	.non_lazy_symbol_pointer			;\
6:							;\
	.indirect_symbol var				;\
	.long 0						;\
	.text						;\
	.align 2
#else
#define MI_GET_ADDRESS(reg,var)  \
	ldr	reg, 3f	;\
	b	4f	;\
3:	.long var	;\
4:
#endif

#if defined(__DYNAMIC__)
#define MI_BRANCH_EXTERNAL(var)				\
	.globl	var								;\
	MI_GET_ADDRESS(ip, var)				;\
 	bx	ip
#else
#define MI_BRANCH_EXTERNAL(var)				;\
	.globl	var								;\
 	b	var
#endif

#if defined(__DYNAMIC__)
#define MI_CALL_EXTERNAL(var)    \
	.globl	var				;\
	MI_GET_ADDRESS(ip,var)	;\
	mov	lr, pc		;\
	bx	ip
#else
#define MI_CALL_EXTERNAL(var)				\
	.globl	var								;\
 	bl	var
#endif

#define MI_ENTRY_POINT(name)				\
	.align 2	;\
	.globl  name							;\
	.text									;\
name:

/* load the syscall number into r12 and trap */
#define DO_SYSCALL(num)		\
	.if (((num) & 0xff) == (num)) 	       				;\
	mov		r12, #(num)		       			;\
	.elseif (((num) & 0x3fc) == (num))				;\
	mov		r12, #(num)					;\
	.else								;\
	mov		r12, #((num) & 0xffffff00)	/* top half of the syscall number */ ;\
	orr		r12, r12, #((num) & 0xff)	/* bottom half */ ;\
	.endif								;\
	swi		#SWI_SYSCALL

/* simple syscalls (0 to 4 args) */
#define	SYSCALL_0to4(name)					\
	MI_ENTRY_POINT(_##name)					;\
	DO_SYSCALL(SYS_##name)					;\
	bxcc	lr								/* return if carry is clear (no error) */ ; \
1:	MI_BRANCH_EXTERNAL(cerror)

/* syscalls with 5 args is different, because of the single arg register load */
#define	SYSCALL_5(name)						\
	MI_ENTRY_POINT(_##name)					;\
	mov		ip, sp							/* save a pointer to the args */ ; \
	stmfd	sp!, { r4-r5 }					/* save r4-r5 */ ;\
	ldr		r4, [ip]						/* load 5th arg */ ; \
	DO_SYSCALL(SYS_##name)					;\
	ldmfd	sp!, { r4-r5 }					/* restore r4-r5 */ ; \
	bxcc	lr								/* return if carry is clear (no error) */ ; \
1:	MI_BRANCH_EXTERNAL(cerror)

/* syscalls with 6 to 8 args */
#define SYSCALL_6to8(name, save_regs, arg_regs) \
	MI_ENTRY_POINT(_##name)					;\
	mov		ip, sp							/* save a pointer to the args */ ; \
	stmfd	sp!, { save_regs }				/* callee saved regs */ ;\
	ldmia	ip, { arg_regs }				/* load arg regs */ ; \
	DO_SYSCALL(SYS_##name)					;\
	ldmfd	sp!, { save_regs }				/* restore callee saved regs */ ; \
	bxcc	lr								/* return if carry is clear (no error) */ ; \
1:	MI_BRANCH_EXTERNAL(cerror)

#define COMMA ,

#define SYSCALL_0(name)						SYSCALL_0to4(name)
#define SYSCALL_1(name)						SYSCALL_0to4(name)
#define SYSCALL_2(name)						SYSCALL_0to4(name)
#define SYSCALL_3(name)						SYSCALL_0to4(name)
#define SYSCALL_4(name)						SYSCALL_0to4(name)
/* SYSCALL_5 declared above */
#define SYSCALL_6(name)						SYSCALL_6to8(name, r4-r5, r4-r5)
#define SYSCALL_7(name)						SYSCALL_6to8(name, r4-r6 COMMA r8, r4-r6)
/* there are no 8-argument syscalls currently defined */

/* select the appropriate syscall code, based on the number of arguments */
#define SYSCALL(name, nargs)	SYSCALL_##nargs(name)

#define	SYSCALL_NONAME_0to4(name)			\
	DO_SYSCALL(SYS_##name)					;\
	bcc		1f								/* branch if carry bit is clear (no error) */ ; \
	MI_BRANCH_EXTERNAL(cerror)				/* call cerror */ ; \
1:

#define	SYSCALL_NONAME_5(name)				\
	mov		ip, sp 							/* save a pointer to the args */ ; \
	stmfd	sp!, { r4-r5 }					/* save r4-r5 */ ;\
	ldr		r4, [ip]						/* load 5th arg */ ; \
	DO_SYSCALL(SYS_##name)					;\
	ldmfd	sp!, { r4-r5 }					/* restore r4-r7 */ ; \
	bcc		1f								/* branch if carry bit is clear (no error) */ ; \
	MI_BRANCH_EXTERNAL(cerror)				/* call cerror */ ; \
1:

#define	SYSCALL_NONAME_6to8(name, save_regs, arg_regs)	\
	mov		ip, sp 							/* save a pointer to the args */ ; \
	stmfd	sp!, { save_regs }				/* callee save regs */ ;\
	ldmia	ip, { arg_regs }				/* load arguments */ ; \
	DO_SYSCALL(SYS_##name)					;\
	ldmfd	sp!, { save_regs }				/* restore callee saved regs */ ; \
	bcc		1f								/* branch if carry bit is clear (no error) */ ; \
	MI_BRANCH_EXTERNAL(cerror)				/* call cerror */ ; \
1:

#define SYSCALL_NONAME_0(name)				SYSCALL_NONAME_0to4(name)
#define SYSCALL_NONAME_1(name)				SYSCALL_NONAME_0to4(name)
#define SYSCALL_NONAME_2(name)				SYSCALL_NONAME_0to4(name)
#define SYSCALL_NONAME_3(name)				SYSCALL_NONAME_0to4(name)
#define SYSCALL_NONAME_4(name)				SYSCALL_NONAME_0to4(name)
/* SYSCALL_NONAME_5 declared above */
#define SYSCALL_NONAME_6(name)				SYSCALL_NONAME_6to8(name, r4-r5, r4-r5)
#define SYSCALL_NONAME_7(name)				SYSCALL_NONAME_6to8(name, r4-r6 COMMA r8, r4-r6)
/* there are no 8-argument syscalls currently defined */

/* select the appropriate syscall code, based on the number of arguments */
#define SYSCALL_NONAME(name, nargs)	SYSCALL_NONAME_##nargs(name)

#define	PSEUDO(pseudo, name, nargs)			\
	.globl	_##pseudo						;\
	.text									;\
	.align  2								;\
_##pseudo:									;\
	SYSCALL_NONAME(name, nargs)

#undef END
#import	<mach/arm/syscall_sw.h>
 
#if !defined(SYS___pthread_canceled)
#define SYS___pthread_markcancel	332
#define SYS___pthread_canceled		333
#define SYS___semwait_signal		334
#endif
