/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

RCSID("$NetBSD: e_atan2.S,v 1.5 2001/06/19 00:26:29 fvdl Exp $")


ENTRY(atan2l)
	XMM_TWO_ARG_LONG_DOUBLE_PROLOGUE
	fldt	ARG_LONG_DOUBLE_ONE
	fldt	ARG_LONG_DOUBLE_TWO
	fpatan
	XMM_LONG_DOUBLE_EPILOGUE
	ret
