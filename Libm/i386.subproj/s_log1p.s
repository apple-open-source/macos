/*
 *	log1pl  by Ian Ollmann
 *
 *	Copyright © 2005 Apple Computer, Inc. All Rights Reserved
 *
 */

#include <machine/asm.h>
#include "abi.h"

#if defined( __LP64__ )
	#error not 64-bit ready
#endif

ENTRY(log1pl)
	pushl		$0x3e95f619			// 1- sqrt(0.5f)

	//test to see if |x| < 1-sqrt(2)/2
	fldln2							//	{ ln2 }
	fld1							//	{ 1, ln2 }
	fchs							//	{ -1, ln2 }
	fldt		8(%esp)				//	{ x, -1, ln2 }
	fucomi		%st(1), %st(0)		//  { x, -1, ln2 }
	jb			log1pl_negnan
	
	fld			%st(0)				//	{ x, x, -1, ln2 }
	fabs							//	{ |x|, x, -1, ln2 }
	flds		(%esp)				//	{ 1- sqrt(0.5f), |x|, x, -1, ln2 }
	fucomip		%st(1), %st(0)		//	{ |x|, x, -1, ln2 }
	fstp		%st(0)				//	{ x, -1, ln2 }
	jb			log1pl_large		//  handle  (1-sqrt(2)/2) <= |x|  

	//we assume the common case is the one where the precision is needed
	fstp		%st(1)				//	{ x, ln2 }
	fyl2xp1							//	{ result }
	addl		$4, %esp
	ret

log1pl_negnan:						//  { x, -1, ln2 }
	//we assume the common case is the one where the precision is needed
	fstp		%st(1)				//	{ x, ln2 }
	fyl2x							//	{ result }
	addl		$4, %esp
	ret


//handles inputs outside of +-{ 1 - sqrt(2)/2 } and NaN	
log1pl_large:						//  { x+1, ln2 }
	fsubp							//	{ x+1, ln2 }
	fyl2x							//	{ result }

	addl		$4, %esp
	ret


