/*
 * Written by Ian Ollmann.
 * Copyright © 2005 Apple Computer Inc.
 */

#include <machine/asm.h>

#include "abi.h"


#if defined (__LP64__ )
	#error	this code not LP64 aware
#endif

ENTRY(rintl)
	fldt	4(%esp)
	frndint
	ret

ENTRY( nearbyintl )
	subl	$28, %esp
	
	fldt	32( %esp )				//{f}
	
	//read fpcw + fpsw
	fnstenv	(%esp)
	movw	(%esp),	%ax

	//or it with 0x20 
	movl	%eax, %edx
	orl		$0x20,  %eax

	//stick it back int the fpcw
	movw	%ax, (%esp)
	fldenv	(%esp)
	
	//round
	frndint							//{ result }
		
	//reset fpsw and fpcw
	movw	%dx, (%esp)
	fldenv	(%esp)
	
	addl	$28, %esp
	ret
	
//a short routine to get the local address
local_addr:
	movl    (%esp), %ebx
	ret


ENTRY( llrintl )
	pushl		$0x5f000000						//0x1.0p63f
	subl		$8, %esp
	xor			%edx,		%edx

	flds		8(%esp)							//{0x1.0p63 }
	fldt		16( %esp )						//{f, 0x1.0p63}
	fucomi 		%ST(1), %ST						//{f, 0x1.0p63}		f>=0x1.0p63
	fistpll		(%esp)							//{0x1.0p63}
	fstp		%ST(0)							//{}

	setnb		%dl
	negl		%edx
	movl		(%esp),		%eax
	xorl		%edx,		%eax
	xorl		4(%esp),	%edx
	
	addl		$12,		%esp
	ret
	
ENTRY( lrintl )
	pushl		$0x4f000000						//0x1.0p31f
	subl		$8, %esp
	xor			%edx,		%edx

	flds		8(%esp)							//{0x1.0p31f }
	fldt		16( %esp )						//{f, 0x1.0p31f}
	fucomi 		%ST(1), %ST						//{f, 0x1.0p31f}		f>=0x1.0p31f   test for overflow
	fistpl		(%esp)							//{0x1.0p31f}
	fstp		%ST(0)							//{}

	setnb		%dl
	negl		%edx
	movl		(%esp),		%eax
	xorl		%edx,		%eax
	
	addl		$12,		%esp
	ret

