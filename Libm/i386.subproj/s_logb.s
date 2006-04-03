/*
 * Written by J.T. Conklin <jtc@netbsd.org>.
 * Public domain.
 */

#include <machine/asm.h>

#include "abi.h"

RCSID("$NetBSD: s_logb.S,v 1.5 2001/06/19 00:26:30 fvdl Exp $")

ENTRY(logbl)
	XMM_ONE_ARG_LONG_DOUBLE_PROLOGUE
	fldt	ARG_LONG_DOUBLE_ONE
	fxtract
	fstp	%st
	XMM_LONG_DOUBLE_EPILOGUE
	ret
