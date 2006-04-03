/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

RCSID("$NetBSD: e_remainder.S,v 1.6 2001/06/25 16:47:05 fvdl Exp $")

ENTRY(remainderl)
	XMM_TWO_ARG_LONG_DOUBLE_PROLOGUE
	fldt	ARG_LONG_DOUBLE_TWO
	fldt	ARG_LONG_DOUBLE_ONE
1:	fprem1
	fstsw	%ax
	btw	$10,%ax
	jc	1b
	fstp	%st(1)
	XMM_LONG_DOUBLE_EPILOGUE
	ret
