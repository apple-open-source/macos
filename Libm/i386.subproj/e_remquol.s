
/*
 *  by Ian Ollmann
 *
 *	Copyright © 2005 Apple Computer Inc. All Rights Reserved.
 *
 */


#include <machine/asm.h>
#include "abi.h"

#if defined( __LP64__ )
	#error not 64-bit ready
#endif

PRIVATE_ENTRY(__remquol)			//private interface used by single and double precision remquo
#if ! defined( BUILDING_FOR_CARBONCORE_LEGACY )
ENTRY(remquol)
#endif
	//load data
	fldt	20(%esp)			//	{ d }
	fldt	4(%esp)				//	{ n, d }

1:	fprem1						//	{ r, d }
	fstsw	%ax
	btw		$10,%ax
	jc		1b
	fstp	%st(1)

	//Calculate quo. 
	//Alas, the bits in fstat are all scrambled up.
	//It seems like there should be an easy way to do this,
	//but I don't see the magic instruction. So....
	movl	%eax,	%ecx
	movl	%eax,	%edx
	ror		$6,		%eax
	ror		$9,		%ecx
	ror		$13,	%edx
	and		$0x4,	%eax
	and		$0x1,	%ecx
	and		$0x2,	%edx
	or		%ecx,	%eax
	or		%eax,	%edx

	//set the sign appropriately according to the sign of n/d
	movw	12(%esp),	%cx
	movw	28(%esp),	%ax
	xor		%ecx,		%eax
	movl	36(%esp),	%ecx
	cwde	
	sar		$15,		%eax
	xor		%eax,		%edx
	sub		%eax,		%edx

	//store out quo and return
	movl	%edx,	(%ecx)
	ret
	
