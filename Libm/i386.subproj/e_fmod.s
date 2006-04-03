/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>
#include "abi.h"

RCSID("$NetBSD: e_fmod.S,v 1.6 2001/06/25 16:44:34 fvdl Exp $")

PRIVATE_ENTRY(__fmodl)							//private interface for single and double precision fmod
ENTRY(fmodl)
	XMM_TWO_ARG_LONG_DOUBLE_PROLOGUE
	fldt	ARG_LONG_DOUBLE_TWO
	fldt	ARG_LONG_DOUBLE_ONE
1:	fprem
	fstsw	%ax
	btw	$10,%ax
	jc	1b
	fstp	%st(1)
	XMM_LONG_DOUBLE_EPILOGUE
	ret

