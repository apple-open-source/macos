/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
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
 * C runtime startup for m68k, i386 and ppc interface to the dynamic linker.
 * This is the same as the entry point in crt0.o with the addition of the
 * address of the mach header passed as the an extra first argument.
 *
 * Kernel sets up stack frame to look like:
 *
 *	| STRING AREA |
 *	+-------------+
#if defined(__MACH30__) && (defined(__ppc__) || defined(__i386__))
 *	+-------------+
 *	|      0      |	In MacOS X PR2 Beaker2E the path passed to exec() is
 *	+-------------+	passed on the stack just after the trailing 0 of the
 *	|  exec_path  | the envp[] array as a pointer to a string.  See the
 *	+-------------+ code at the end of pickup_environment_variables().
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
 *	|     argc    |
 *	+-------------+
 * sp->	|      mh     | address of where the a.out's file offset 0 is in memory
 *	+-------------+
 *
 *	Where arg[i] and env[i] point into the STRING AREA
 */
#ifdef m68k
	.text
	.even
L_start:
	.stabs "dyld_start.s",100,0,0,L_start
	.stabs "int:t1=r1;-2147483648;2147483647;",128,0,0,0
	.stabs "char:t2=r2;0;127;",128,0,0,0

	.globl __dyld_start
__dyld_start:
	lea	pc@(-2),a1	| Set d1 to the difference between where we
	movel	a1,d1		|  started executing at and __dyld_start to know
	subl	#__dyld_start,d1|  if the kernel slid this code.

	movl	sp,a0		| pointer to base of kernel frame
	subw	#16,sp		| room for new mh, argc, argv, & envp
	movl	a0@+,sp@	| mh to reserved stack word and a0 to pt to argc
	movl	a0@+,d0		| pickup argc and bump a0 to pt to arg[0]
	movl	d0,sp@(4)	| argc to reserved stack word
	movl	a0,sp@(8)	| argv to reserved stack word
	addql	#1,d0		| argc + 1 for zero word
	asll	#2,d0		| * sizeof(char *)
	addl	d0,a0		| addr of env[0]
	movl	a0,sp@(12)	| envp to reserved stack word

	cmpl	#0,d1		| If this code was slid we have to relocate
	beq	1f		|  ourself before we get going.
	movel	d1,sp@-
	bsr	__dyld_reloc	| _dyld_reloc(slide_amount)
	addw	#4,sp
1:
	bsr	_mach_init	| call mach_init() so we can make mach calls

	bsr	__dyld_init	| _dyld_init(mh, argc, argv, envp)
	movl	d0,a0		| put entry point in a0
	addw	#20,sp		| deallocate room for new mh, argc, argv, & envp
				|  and remove the mh argument
	jmp	a0@		| jump to the entry point

	.globl dyld_stub_binding_helper
dyld_stub_binding_helper:
	trapt.l	#0xfeadface

	.stabs "",100,0,0,L_end
L_end:
#endif /* m68k */

#ifdef __i386__
	.text
	.align	4, 0x90
L_start:
	.stabs "dyld_start.s",100,0,0,L_start
	.stabs "int:t1=r1;-2147483648;2147483647;",128,0,0,0
	.stabs "char:t2=r2;0;127;",128,0,0,0

	.globl __dyld_start
__dyld_start:
	call	L1			# Set %eax to the difference between
L1:	popl	%eax			#  where we started executing at and
	subl	$L1-__dyld_start,%eax	#  __dyld_start to know if the kernel
	subl	$__dyld_start,%eax	#  slid this code.

	pushl	$0		# push a zero for debugger end of frames marker
	movl	%esp,%ebp	# pointer to base of kernel frame
	subl	$16,%esp	# room for new mh, argc, argv, & envp
	movl	4(%ebp),%ebx	# pickup mh in %ebx
	movl	%ebx,0(%esp)	# mh to reserved stack word
	movl	8(%ebp),%ebx	# pickup argc in %ebx
	movl	%ebx,4(%esp)	# argc to reserved stack word
	lea	12(%ebp),%ecx	# addr of arg[0], argv, into %ecx
	movl	%ecx,8(%esp)	# argv to reserved stack word
	addl	$1,%ebx		# argc + 1 for zero word
	sall	$2,%ebx		# * sizeof(char *)
	addl	%ecx,%ebx	# addr of env[0], envp, into %ebx
	movl	%ebx,12(%esp)	# envp to reserved stack word

	cmpl	$0,%eax		# If this code was slid we have to relocate
	je	1f		#  ourself before we get going.
	pushl	%eax
	call	__dyld_reloc	# _dyld_reloc(slide_amount)
	addl	$4,%esp
1:
	call	_mach_init	# call mach_init() so we can make mach calls

	call	__dyld_init	# _dyld_init(mh, argc, argv, envp)
	addl	$24,%esp	# deallocate room for new mh, argc, argv, & envp
				#  and remove the mh argument, and debugger end
				#  frame marker
	movl	$0,%ebp		# restore ebp back to zero
	jmp	%eax		# jump to the entry point

	.globl dyld_stub_binding_helper
dyld_stub_binding_helper:
	hlt

	.stabs "",100,0,0,L_end
L_end:
#endif /* __i386__ */

#ifdef __ppc__
	.text
	.align 2
L_start:
	.stabs "dyld_start.s",100,0,0,L_start
	.stabs "int:t1=r1;-2147483648;2147483647;",128,0,0,0
	.stabs "char:t2=r2;0;127;",128,0,0,0

	.globl __dyld_start
__dyld_start:
	bcl	20,31,L1	; put address of L1 in r11
L1:	mflr	r11		; Calculate the difference
	lis	r12,hi16(L1)	;  between where this code was
	ori	r12,r12,lo16(L1);  loaded and where it was
	sub	r11,r11,r12	;  staticly linked as the slide into r11

	mr	r26,r1		; save original stack pointer into r26
	mr	r3,r1		; get pointer to base of kernel frame into r3
	subi	r1,r1,64	; minimum space on stack for arguments
	clrrwi	r1,r1,5		; align to 32 bytes
	lwz	r30,0(r3)	; get mach_header into r30
	lwz	r29,4(r3)	; get argc into r29
	addi	r28,r3,8	; get argv into r28
	addi	r4,r29,1	; calculate argc + 1 into r4
	slwi	r4,r4,2		; calculate (argc + 1) * sizeof(char *) into r4
	add	r27,r28,r4	; get address of env[0] into r27

	cmpwi	cr1,r11,0	; If this code was slid we have to relocate
	beq+    cr1,L2		;  ourself before we get going.
	mr	r3,r11		; slide_amount was calculate into r11 above
	bl	__dyld_reloc    ; _dyld_reloc(slide_amount)
L2:
	bl	_mach_init	; call mach_init() so we can make mach calls

	mr	r3,r30		; move mach_header into r3
	mr	r4,r29		; move argc into r4
	mr	r5,r28		; move argv into r5
	mr	r6,r27		; move envp into r6
	bl	__dyld_init	; _dyld_init(mh, argc, argv, envp)
	mtctr	r3		; Put entry point in count register
	mr	r12,r3		;  also put in r12 for ABI convention.
	addi	r1,r26,4	; Restore the stack pointer and remove the
				;  mach_header argument.
	bctr			; jump to the entry point

	.globl dyld_stub_binding_helper
dyld_stub_binding_helper:
	trap

	.stabs "",100,0,0,L_end
L_end:
#endif /* __ppc__ */

#ifdef hppa
/*
 * C runtime startup for hppa interface to the dynamic linker.  This is the same
 * as the entry point in crt0.o with the addition of the address of the mach
 * header (mh) passed as the an extra first argument.
 *
 * Kernel sets up stack frame to look like:
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
 *	|      mh     |
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
	.stabs "dyld_start.s",100,0,0,L_start
	.stabs "int:t1=r1;-2147483648;2147483647;",128,0,0,0
	.stabs "char:t2=r2;0;127;",128,0,0,0

	.globl __dyld_start
__dyld_start:
	bl	L1,%r26			; put address of L1 in r26
	nop
L1:	depi	0,31,2,%r26		; clear protection bits
	ldil	L`L1-__dyld_start,%r1	; Calculate the difference between where
	ldo	R`L1-__dyld_start(%r1),%r1 ;  this code was loaded and where it
	sub	%r26,%r1,%r26		;  was staticly linked as the slide
	ldil	L`__dyld_start,%r2	;  amount and call _dyld_reloc() if the
	ldo	R`__dyld_start(%r2),%r2	;  slide amount is not zero.
	sub	%r26,%r2,%r26

	; adjust and align the stack and create the first frame area
	copy	%r30,%r3	  	; save stack pointer value in r3
	ldo	127(%r30),%r30		; setup stack pointer on the next 64
	depi	0,31,6,%r30		;  byte boundary for calls

	combt,=,n %r26,%r0,L2		; if slide amount is not zero call
	bl	__dyld_reloc,%r2	;  _dyld_reloc(slide_amount)
	nop
L2:
	bl	_mach_init,%r2	; call mach_init() so mach calls work
	nop

	; load the arguments to call _dyld_init(mh, argc, argv, envp)
	ldw	0(%r3),%r19	; load the value of ap in r19
	ldw	-4(%r19),%r26	; load the value of mh in arg0
	ldw	0(%r19),%r25	; load the value of argc in arg1
	ldo	4(%r19),%r24	; load the address of arg[0] in arg2
	; calculate the value for envp by walking the arg[] pointers
	copy	%r24,%r23	; start with the address of arg[0] in arg3
	ldw	0(%r23),%r20	; load the value of the arg[0] pointer
	comib,=	0,%r20,L4	; if 0 go on to call _dyld_init()

	ldws,mb 4(0,%r23),%r20	; load next pointer value and advance r23
L3:
	comib,<>,n 0,%r20,L3	; if next pointer is not 0 loop
	ldws,mb 4(0,%r23),%r20	; load next pointer value and advance r23
	
L4:
	bl	__dyld_init,%r2	; _dyld_init(mh, argc, argv, envp)
	ldo	4(%r23),%r23	; load address of the next pointer after the 0

	; adjust the stack back the way we came in
	bv	0(%r28)		; jump to the entry point
	copy	%r3,%r30  	; put back the value of the stack pointer

	.globl dyld_stub_binding_helper
dyld_stub_binding_helper:
	break	0,0

	.stabs "",100,0,0,L_end
L_end:
#endif /* hppa */

#ifdef sparc
/*
 *
 * Kernel sets up stack frame to look like:
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
 *	|           mh  	| <- new mach header arg for dyld
 *	|-----------------------|
 *      |    window save area   |
 *      |        (16 wds)       |
 *      |_______________________| <- %sp
 */


	.text
	.align	2
L_start:
	.stabs "dyld_start.s",100,0,0,L_start
	.stabs "int:t1=r1;-2147483648;2147483647;",128,0,0,0
	.stabs "char:t2=r2;0;127;",128,0,0,0

	.globl __dyld_start
__dyld_start:
	! determine our location and slide code if necessary
	save	%sp, -96, %sp		! get ready to make calls
	call	L1
	dec	4,%o7
L1:
	sethi	%hi(__dyld_start),%l0	! get our static address
	or	%l0,%lo(__dyld_start),%l0
	sub	%o7,%l0,%l0		! and compare with entry pc

	tst	%l0
	bz	1f			! not slid
	nop
	call	__dyld_reloc		! relocate ourself
	mov	%l0,%o0

1:
	call	_mach_init	! allows us to make mach calls
	nop
	! build arguments to dyld_init (mh, argc, argv, envp)
	mov	%fp,%l0
	add	%l0,16*4,%l0	! skip over window save area
	ld	[%l0],%o0	! 1st arg: mh ptr
	ld	[%l0+4],%o1	! 2nd arg: argc
	mov	%l0,%o2
	inc	8,%o2		! 3rd arg: argv ptr
	! now compute the envp ptr
	add	%o1,1,%o3	! account for null ptr
	sll	%o3,2,%o3	! adjust for ptrs
	
	call	__dyld_init	! dyld_init returns our entry point
	add	%o2,%o3,%o3	! 4th arg: envp ptr	

	jmp	%o0		! call the entry point
	restore
	
	.globl dyld_stub_binding_helper
dyld_stub_binding_helper:

	ta	9

	.stabs "",100,0,0,L_end
L_end:
#endif /* sparc */
