/* 
 * Mach Operating System
 * Copyright (c) 1989 Carnegie-Mellon University
 * All rights reserved.  The CMU software License Agreement specifies
 * the terms and conditions for use and redistribution.
 */
/* 
 * HISTORY
 * $Log: asm.h,v $
 * Revision 1.2  2000/03/07 06:10:59  wsanchez
 * Make main branch like boot-3, which builds.
 *
 * Revision 1.1.2.1  2000/02/29 00:04:34  jliu
 * Added legacy header files.
 *
 * Revision 1.1.1.1  1997/09/30 02:45:05  wsanchez
 * Import of kernel from umeshv/kernel
 *
 * Revision 2.1.1.6  90/03/29  20:45:08  rvb
 * 	Typo on ENTRY if gprof
 * 	[90/03/29            rvb]
 * 
 * Revision 2.1.1.5  90/02/28  15:47:31  rvb
 * 	fix SVC for "ifdef wheeze" [kupfer]
 * 
 * Revision 2.1.1.4  90/02/27  08:47:30  rvb
 * 	Fix the GPROF definitions.
 * 	 ENTRY(x) gets profiled iffdef GPROF.
 * 	 Entry(x) (and DATA(x)) is NEVER profiled.
 * 	 MCOUNT can be used by asm that intends to build a frame,
 * 	 after the frame is built.
 * 	[90/02/26            rvb]
 * 
 * Revision 2.1.1.3  90/02/09  17:23:23  rvb
 * 	Add #define addr16 .byte 0x67
 * 	[90/02/09            rvb]
 * 
 * Revision 2.1.1.2  89/11/10  09:51:33  rvb
 * 	Added LBi, SVC and ENTRY
 * 
 * Revision 2.1.1.1  89/10/22  11:29:38  rvb
 * 	New a.out and coff compatible .s files.
 * 	[89/10/16            rvb]
 * 
 */


#define S_ARG0	 4(%esp)
#define S_ARG1	 8(%esp)
#define S_ARG2	12(%esp)
#define S_ARG3	16(%esp)

#define FRAME	pushl %ebp; movl %esp, %ebp
#define EMARF	leave

#define B_ARG0	 8(%ebp)
#define B_ARG1	12(%ebp)
#define B_ARG2	16(%ebp)
#define B_ARG3	20(%ebp)

#define EXT(x)		_##x
#define LBb(x,n)	n##b
#define LBf(x,n)	n##f

#define ALIGN 2
#define	LCL(x)	x

#define LB(x,n) n

#define SVC .byte 0x9a; .long 0; .word 0x7
#define String	.ascii
#define Value	.word

#define Times(a,b) (a*b)
#define Divide(a,b) (a/b)

#define INB	inb	%dx, %al
#define OUTB	outb	%al, %dx
#define INL	inl	%dx, %eax
#define OUTL	outl	%eax, %dx

#define data16	.byte 0x66
#define addr16	.byte 0x67



#ifdef GPROF
#define MCOUNT		.data; LB(x, 9): .long 0; .text; lea LBb(x, 9),%edx; call mcount
#define	ENTRY(x)	.globl EXT(x); .align ALIGN; EXT(x): ; \
			pushl %ebp; movl %esp, %ebp; MCOUNT; popl %ebp;
#define	ASENTRY(x) 	.globl x; .align ALIGN; x: ; \
			pushl %ebp; movl %esp, %ebp; MCOUNT; popl %ebp;
#else	/* GPROF */
#define MCOUNT
#define	ENTRY(x)	.globl EXT(x); .align ALIGN; EXT(x):
#define	ASENTRY(x)	.globl x; .align ALIGN; x:
#endif	/* GPROF */

#define	Entry(x)	.globl EXT(x); .align ALIGN; EXT(x):
#define	DATA(x)		.globl EXT(x); .align ALIGN; EXT(x):

