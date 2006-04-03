/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>
#include "abi.h"

RCSID("$NetBSD: e_sqrt.S,v 1.5 2001/06/19 00:26:30 fvdl Exp $")

ENTRY(sqrtl)
	fldt	ARG_LONG_DOUBLE_ONE
	fsqrt
	ret
