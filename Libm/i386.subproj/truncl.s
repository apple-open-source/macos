/*
 *  truncl.s
 *  LibmV5
 *
 *  Created by Ian Ollmann on 9/1/05.
 *  Copyright 2005 Apple Computer. All rights reserved.
 *
 */

#include "machine/asm.h"
#include "abi.h"

#if defined( __LP64__ )
	#error not 64-bit ready
#endif

ENTRY(truncl)
    pushl       $0x5f000000             //0x1.0p63 
    subl        $8,     %esp

//convert |f| to an int
    //check to see if f is already an int or zero:      |f| >= 2**63 is an int
	fldz							//	{0}
    fldt        16(%esp)            //  {f, 0}
	fld			%st(0)				//	{f, f, 0 }
    fabs                            //  {|f|, f 0}
    flds        8(%esp)             //  {limit,     |f|, f, 0}
    fucomip     %ST(1), %ST         //  {|f|, f, 0}       0x1.0p63 <= |f| or NaN
	fld			%st(0)				//	{|f|, |f|, f, 0 }

    //if it is a large int, NaN, replace it with 0. This avoids spurious overflows, illegal, and inexact
    fcmovbe     %ST(3), %ST(0)      //  { 0 or |f|, |f|, f, 0  }

    //then convert to int with truncation to zero
    fisttpll    (%esp)              //  {|f|, f, 0}   ***USES SSE3***

    //generate NaN result
    fadd		%st(2)				//  {|f|+0, f, 0}   NaN silenced
    
    //load the integer result back in
    fildll      (%esp)              //  {|intf|,  |f|+0, f, 0}

    //if 2**63 <= |f| or NaN, use f+0 instead
    fcmovbe      %ST(1), %ST(0)     //  {|intf| or |f|+0, |f|+0, f, 0 }
	fstp		%ST(1)				//	{|intf| or |f|+0, f, 0 }
	fld			%st(0)				//	{|intf| or |f|+0, |intf| or |f|+0, f, 0 }
	fchs							//  {-(|intf| or |f|+0), |intf| or |f|+0, f, 0 }
        
    //return f, if f == 0
	fxch		%ST(3)				//	{ 0, |intf| or |f|+0, f, -(|intf| or |f|+0) }
    fucomip     %ST(2), %ST         //  { |intf| or |f|+0, f, -(|intf| or |f|+0) }           0 < f or f is NaN 
    fcmovnbe    %ST(2), %ST(0)		//  { +-(|intf| or |f|+0), f, -(|intf| or |f|+0) }
    fcmove		%st(1), %st(0)		//	{ result, f, -(|intf| or |f|+0) }
	
    //clean up the stack
    fstp        %ST(2)              //  { f, result }
	fstp		%st(0)				//	{ result }

    addl        $12,    %esp
    ret
