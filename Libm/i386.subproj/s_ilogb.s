/*
 * Written by Ian Ollmann
 *
 * Copyright © 2005, Apple Computer. All rights reserved.
 */

#include <machine/asm.h>


#define LOCAL_STACK_SIZE		4

#if defined( __LP64__ )
	#define FRAME_SIZE			8		/* sizeof( void*) */
	#define STACKP				%rsp
	#define SIGN_EXTEND_EAX		cdqe
#else	
	#define FRAME_SIZE			4		/* sizeof( void*) */
	#define STACKP				%esp
	#define SIGN_EXTEND_EAX		
#endif

#define FIRST_ARG_OFFSET		(FRAME_SIZE + LOCAL_STACK_SIZE)

ENTRY(ilogbl)
	pushl		$0x4f000000						//  0x1.0p31
	flds		(STACKP)						//  { 1.0p31 }
	fldt		FIRST_ARG_OFFSET(STACKP)		//	{ x, 0x1.0p31 }
	fldz										//	{ 0, x, 0x1.0p31 }
	fucomip		%st(1), %st(0)					//	{ x, 0x1.0p31 }							if( x == 0 or NaN )
	fcmove		%st(1), %st(0)					//	{ x or 0x1.0p31, 0x1.0p31 }				then x = 0x1.0p31			//avoids div/0 for fxtract(0)
	fxtract										//	{ sig, exp, 0x1.0p31 }
	fstp		%st(0)							//	{ exp, 0x1.0p31 }
	fld			%st(0)							//	{ exp, exp, 0x1.0p31 }
	fcmove		%st(2), %st(0)					//	{ exp or 0x1.0p31, exp, 0x1.0p31 }		if( x == 0 or NaN )	then exp = 0x1.0p31		//set to overflow
	fld1										//	{ 1, exp or 0x1.0p31, exp, 0x1.0p31 }
	fsubrp		%st(3), %st(0)					//	{ exp or 0x1.0p31, exp, 0x1.0p31 - 1}
	fxch										//	{ exp, exp or 0x1.0p31, 0x1.0p31 - 1} 
	fucomip		%st(2), %st(0)					//	{ exp or 0x1.0p31, 0x1.0p31 - 1}
	fcmovnbe	%st(1), %st(0)					//	{ exp or 0x1.0p31 or 0x1.0p31 - 1, 0x1.0p31 - 1 }
	fstp		%st(1)							//	{ exp or 0x1.0p31 - 1 }
	fistpl		(STACKP)						//	{}
	movl		(STACKP), %eax
	SIGN_EXTEND_EAX
	addl		$LOCAL_STACK_SIZE, %esp
	ret
