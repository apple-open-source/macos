/*
 *  e_scalbn.s
 *
 *	by Ian Ollmann
 *
 *	Copyright © 2005 Apple Computer. All Rights Reserved.
 */
 
#include <machine/asm.h>
#include "abi.h"

#if defined( __LP64__ )
	#error not 64-bit ready
#endif

ENTRY( scalbnl )
	fildl	20(%esp )		//{ scale }
	fldt	4(%esp )		//{ f, scale }
	fscale
	fstp	%st(1)
	ret
	
