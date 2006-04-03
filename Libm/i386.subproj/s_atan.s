/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

RCSID("$NetBSD: s_atan.S,v 1.5 2001/06/19 00:26:30 fvdl Exp $")

ENTRY(atanl)
	XMM_ONE_ARG_LONG_DOUBLE_PROLOGUE
	fldt	ARG_LONG_DOUBLE_ONE
	fld1
	fpatan
	XMM_LONG_DOUBLE_EPILOGUE
	ret
