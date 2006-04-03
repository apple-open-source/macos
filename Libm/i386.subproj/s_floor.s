/*
 * Written by Ian Ollmann
 * Copyright © 2005, Apple Computer Inc. All rights reserved.
 */

#include <machine/asm.h>
#include "abi.h"


ENTRY( floorl )
	XMM_ONE_ARG_LONG_DOUBLE_PROLOGUE
	fldt		ARG_LONG_DOUBLE_ONE			//{ f }
	frndint									//{ rounded }
	fldt		ARG_LONG_DOUBLE_ONE			//{ f, rounded }
	fucomi		%ST(1), %ST					//  test for f > rounded
	fldz									//{ 0, f, rounded } 
	fld1									//{ 1, 0, f, rounded }
	fcmovnb		%ST(1), %ST(0)				//{ 0 or 1, 0, f, rounded }
	fsubrp		%ST(0), %ST(3)				//{ 0, f, rounded - (0 or 1) }
	fucomip		%ST(1), %ST					//{ f, rounded - (0 or 1) }
	fcmovne		%ST(1), %ST(0)				//{ correct, rounded - (0 or 1)}
	fstp		%ST(1)

	XMM_LONG_DOUBLE_EPILOGUE
	ret



