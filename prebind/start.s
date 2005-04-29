/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * To avoid "relocation overflow" errors from the static link editor we want
 * to make sure that the .text section and the stub sections don't have any 
 * sections between them (like the .cstring section).  Since this is the first
 * file loaded we can force the order by ordering the section directives.
 */
.text
#ifdef CRT1
.picsymbol_stub
.symbol_stub 	/* only in executables with -mdynamic-no-pic */
#endif

#if defined(__m68k__) || defined(__i386__) || defined(__ppc__) || defined(__ppc64__)
/*
 * C runtime startup for m68k, i386 & ppc
 *
 * Kernel or dyld sets up stack frame to look like:
 *
 *	| STRING AREA |
 *	+-------------+
#if defined(__MACH30__) && (defined(__ppc__) || defined(__ppc64__) || defined(__i386__))
 *	+-------------+
 *	|      0      |	In MacOS X PR2 Beaker2E the path passed to exec() is
 *	+-------------+	passed on the stack just after the trailing 0 of the
 *	|  exec_path  | the envp[] array as a pointer to a string.
 *	+-------------+
#endif
 *	+-------------+
 *	|      0      |
 *	+-------------+
 *	|    env[n]   |
 *	+-------------+
 *	       :
 *	       :
 *	+-------------+
 *	|    env[0]   |
 *	+-------------+
 *	|      0      |
 *	+-------------+
 *	| arg[argc-1] |
 *	+-------------+
 *	       :
 *	       :
 *	+-------------+
 *	|    arg[0]   |
 *	+-------------+
 * sp->	|     argc    | argc is always 4 bytes long, even in __ppc64__.
 *	+-------------+
 *
 *	Where arg[i] and env[i] point into the STRING AREA
 */
#endif /* defined(__m68k__) || defined(__i386__) || defined(__ppc__) || defined(__ppc64__) */

#ifdef __m68k__
	.text
	.align 1
L_start:
	.stabs "start.s",100,0,0,L_start
	.stabs "int:t1=r1;-2147483648;2147483647;",128,0,0,0
	.stabs "char:t2=r2;0;127;",128,0,0,0

	.globl	start
start:
	movl	sp,a0		| pointer to base of kernel frame
	subw	#12,sp		| room for new argc, argv, & environ
	movl	a0@+,d0		| pickup argc and bump a0 to pt to arg[0]
	movl	d0,sp@		| argc to reserved stack word
	movl	a0,sp@(4)	| argv to reserved stack word
	addql	#1,d0		| argc + 1 for zero word
	asll	#2,d0		| * sizeof(char *)
	addl	d0,a0		| addr of env[0]
	movl	a0,sp@(8)	| environ to reserved stack word
	jbsr	__start		| call _start(argc, argv, envp)
	illegal			| should never return

	.stabs "",100,0,0,L_end
L_end:
#endif /* __m68k__ */

#ifdef __i386__
	.text
	.align	2, 0x90
L_start:
	.stabs "start.s",100,0,0,L_start
	.stabs "int:t1=r1;-2147483648;2147483647;",128,0,0,0
	.stabs "char:t2=r2;0;127;",128,0,0,0

	.globl	start
start:
	pushl	$0		# push a zero for debugger end of frames marker
	movl	%esp,%ebp	# pointer to base of kernel frame
	andl    $-16,%esp       # force SSE alignment
	subl    $16,%esp        # room for new argc, argv, & envp, SSE aligned
	movl	4(%ebp),%ebx	# pickup argc in %ebx
	movl	%ebx,0(%esp)	# argc to reserved stack word
	lea	8(%ebp),%ecx	# addr of arg[0], argv, into %ecx
	movl	%ecx,4(%esp)	# argv to reserved stack word
	addl	$1,%ebx		# argc + 1 for zero word
	sall	$2,%ebx		# * sizeof(char *)
	addl	%ecx,%ebx	# addr of env[0], envp, into %ebx
	movl	%ebx,8(%esp)	# envp to reserved stack word
	call	__start		# call _start(argc, argv, envp)
	hlt			# should never return

	.stabs "",100,0,0,L_end
L_end:
#endif /* __i386__ */

#if defined(__ppc__) || defined(__ppc64__)
#include <architecture/ppc/mode_independent_asm.h>
	.text
	.align 2
L_start:
	.stabs "start.s",100,0,0,L_start
	.stabs "int:t1=r1;-2147483648;2147483647;",128,0,0,0
	.stabs "char:t2=r2;0;127;",128,0,0,0

	.globl start
start:
	mr      r26,r1              ; save original stack pointer into r26
	subi	r1,r1,GPR_BYTES     ; make space for linkage
	clrrgi	r1,r1,5             ; align to 32 bytes (good enough for 32- and 64-bit APIs)

	li      r0,0                ; load 0 into r0
	stg     r0,0(r1)            ; terminate initial stack frame
	stgu	r1,-SF_MINSIZE(r1)  ; allocate minimal stack frame

	lwz     r3,0(r26)           ; get argc into r3
       addi    r4,r26,GPR_BYTES    ; get argv into r4
	addi	r27,r3,1            ; calculate argc + 1 into r27
	slgi	r27,r27,LOG2_GPR_BYTES ; calculate (argc + 1) * sizeof(char *) into r27
	add     r5,r4,r27           ; get address of env[0] into r5
#ifdef __ppc64__
	bl	__start		    ; 24-bt branch to __start.  ld64 will make a branch island if needed
	trap                        ; should never return
#else /* __ppc__ */
	lis     r11,hi16(__start)   ; far call to _start
	ori     r11,r11,lo16(__start)
	mtctr   r11
	bctrl                       ; call _start(argc, argv, envp)
	trap                        ; should never return
#endif

	.stabs "",100,0,0,L_end
L_end:
#endif /* __ppc__ || __ppc64__ */

#ifdef __hppa__
/*
 * C runtime startup for hppa 
 *
 * Kernel or dyld sets up stack frame to look like:
 *
 *	+-------------+
 * sp->	|     ap      |        HIGH ADDRESS
 *	+-------------+
 *	|      0      | <-xxx (I don't know what this value is called
 *	+-------------+	       but it is the first thing on the stack see
 *	| STRING AREA |	       below at the lowest address).
 *	+-------------+
 *	|      0      |
 *	+-------------+
 *	|    env[n]   |
 *	+-------------+
 *	       :
 *	       :
 *	+-------------+
 *	|    env[0]   |
 *	+-------------+
 *	|      0      |
 *	+-------------+
 *	| arg[argc-1] |
 *	+-------------+
 *	       :
 *	       :
 *	+-------------+
 *	|    arg[0]   |
 *	+-------------+
 * ap->	|     argc    |
 *	+-------------+
 *	|     xxx     |        LOW ADDRESS (first word on the stack)
 *	+-------------+
 *
 *	Where arg[i] and env[i] point into the STRING AREA
 *
 *	Initial sp points to a pointer to argc.  Stack then
 *	grows upward from sp.  [Special calling convention for
 *	PA-RISC and other architectures which grow upward].
 */
	.text
	.align	2
L_start:
	.stabs "start.s",100,0,0,L_start
	.stabs "int:t1=r1;-2147483648;2147483647;",128,0,0,0
	.stabs "char:t2=r2;0;127;",128,0,0,0

	.globl start
start:
	; adjust and align the stack and create the first frame area
	copy	%r30,%r3	; save stack pointer value in r3
	ldo	127(%r30),%r30	; setup stack pointer on the next 64
	depi	0,31,6,%r30	;  byte boundary for calls

	; load the arguments to call _start(argc, argv, envp)
	ldw	0(%r3),%r19	; load the value of ap in r19
	ldw	0(%r19),%r26	; load the value of argc in arg0
	ldo	4(%r19),%r25	; load the address of arg[0] in arg2
	; calculate the value for envp by walking the arg[] pointers
	copy	%r25,%r24	; start with the address of arg[0] in arg3
	ldw	0(%r24),%r20	; load the value of the arg[0] pointer
	comib,=	0,%r20,L2	; if 0 go on to call _start()
	ldws,mb 4(0,%r24),%r20	; load next pointer value and advance r24
L1:
	comib,<>,n 0,%r20,L1	; if next pointer is not 0 loop
	ldws,mb 4(0,%r24),%r20	; load next pointer value and advance r24
L2:
	bl	__start,%r2	; _start(argc, argv, envp)
	ldo	4(%r24),%r24	; load address of the next pointer after the 0

	break	0,0		; should never return

	.stabs "",100,0,0,L_end
L_end:
#endif /* __hppa__ */

#ifdef __sparc__
/*
 *
 * C runtime startup for sparc 
 * 
 * Kernel or dyld sets up stack frame to look like:
 *
 * On entry the stack frame looks like:
 *
 *       _______________________  <- USRSTACK
 *      |           :           |
 *      |  arg and env strings  |
 *      |           :           |
 *      |-----------------------|
 *      |           0           |
 *      |-----------------------|
 *      |           :           |
 *      |  ptrs to env strings  |
 *      |-----------------------|
 *      |           0           |
 *      |-----------------------|
 *      |           :           |
 *      |  ptrs to arg strings  |
 *      |   (argc = # of wds)   |
 *      |-----------------------|
 *      |          argc         |
 *      |-----------------------|
 *	|           mh  	| <- mach header arg for crt1.o ONLY
 *	|-----------------------|
 *      |    window save area   |
 *      |        (16 wds)       |
 *      |_______________________| <- %sp
 */


	.text
	.align	2
L_start:
	.stabs "start.s",100,0,0,L_start
	.stabs "int:t1=r1;-2147483648;2147483647;",128,0,0,0
	.stabs "char:t2=r2;0;127;",128,0,0,0

	.globl start
start:
	mov	%sp, %fp	! save initial stack pointer
	add	%sp, -96, %sp	! allocate first frame on the stack

	! build arguments to _start(argc, argv, envp)
	mov	%fp,%l0
	add	%l0,16*4,%l0	! skip over window save area
#ifdef CRT1
	add	%l0,1*4,%l0	! skip over mach_header on the stack
#endif
	ld	[%l0],%o0	! 1st arg: argc
	mov	%l0,%o1
	inc	4,%o1		! 2nd arg: argv ptr
	! now compute the envp ptr
	add	%o0,1,%o2	! account for null ptr
	sll	%o2,2,%o2	! adjust for ptrs
	call	__start		! call _start(argc, argv, envp)
	add	%o1,%o2,%o2	! 3rd arg: envp ptr	

	ta	9		! should never return

	.stabs "",100,0,0,L_end
L_end:
#endif /* __sparc__ */
