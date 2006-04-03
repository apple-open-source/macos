/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

RCSID("$NetBSD: s_ilogbf.S,v 1.5 2001/06/19 00:26:30 fvdl Exp $")

ENTRY(ilogbf)
#ifdef __i386__
	flds	4(%esp)
	fxtract
	fstp	%st

	fistpl	4(%esp)
	movl	4(%esp),%eax

#else
	movss	%xmm0,-4(%rsp)
	flds	-4(%rsp)
	fxtract
	fstp	%st
	fistpl	-4(%rsp)
	movl	-4(%rsp),%eax
#endif
	ret
