/*
 * Copyright 1996 1995 by Open Software Foundation, Inc. 1997 1996 1995 1994 1993 1992 1991  
 *              All Rights Reserved 
 *  
 * Permission to use, copy, modify, and distribute this software and 
 * its documentation for any purpose and without fee is hereby granted, 
 * provided that the above copyright notice appears in all copies and 
 * that both the copyright notice and this permission notice appear in 
 * supporting documentation. 
 *  
 * OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE. 
 *  
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR 
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM 
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT, 
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION 
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. 
 * 
 */
/*
 * MkLinux
 */

.text

#if defined(__ppc__)
#import	<architecture/ppc/asm_help.h>
#import	<architecture/ppc/pseudo_inst.h>

/*	
 * void *_sp(void)
 */

LEAF(__sp)
	mr	r3,r1
	blr

/*
 * void *_adjust_sp(void *sp)
 */
LEAF(__adjust_sp)
	subi	r3,r3,0x100		/* Stack frame padding */
	blr	

#elif defined(__i386__)

#import	<architecture/i386/asm_help.h>
/*	
 * void *_sp(void)
 */

LEAF(__sp,0)
	movl	%esp,%eax
	ret

/*
 * void *_adjust_sp(void *sp)
 */
LEAF(__adjust_sp,0)
	movl	4(%esp),%eax
	subl	$0x100,%eax
	ret

#else
#error sp functions not defined for this architecture
#endif

