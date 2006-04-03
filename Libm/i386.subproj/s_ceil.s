/*
 * Written by Ian Ollmann
 *
 * Copyright © 2005, Apple Computer.  All Rights Reserved.
 */

#include <machine/asm.h>

#include "abi.h"

//We play a few games with the sign here to get the sign of ceil( -1 < x < 0 ) to come out right

ENTRY(ceill)
	XMM_ONE_ARG_LONG_DOUBLE_PROLOGUE
	fldt		ARG_LONG_DOUBLE_ONE			//{ f }
	fld			%ST(0)						//{ f, f }
	frndint									//{ rounded, f }
	fucomi		%ST(1), %ST					//  test for rounded > f
	fldz									//{ 0, rounded, f } 
	fld1									//{ 1, 0, rounded, f }
	fchs									//{ -1, 0, rounded, f }
	fcmovnb		%ST(1), %ST(0)				//{ 0 or -1, 0, rounded, f }
	fsubp		%ST(0), %ST(2)				//{ 0, (0 or -1) - rounded, f }
	fucomip		%ST(2), %ST					//{ (0 or -1) - rounded, f }
	fchs									//{ -((0 or -1) - rounded), f }
	fxch									//{ f, rounded - (0 or 1) }
	fcmovne		%ST(1), %ST(0)				//{ correct, rounded - (0 or 1)}
	fstp		%ST(1)

	XMM_LONG_DOUBLE_EPILOGUE
	ret

