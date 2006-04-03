/*
 *  roundl.s
 *  LibmV5
 *
 *  Created by Ian Ollmann on 8/19/05.
 *  Copyright 2005 Apple Computer. All rights reserved.
 *
 */

#include "machine/asm.h"
#include "abi.h"

#if defined( __LP64__ )
	#error not 64-bit ready
#endif


ENTRY(roundl)
    pushl       $0x3f000000             //0.5f
    pushl       $0x5f000000             //0x1.0p63 
    subl        $4,     %esp

//convert |f| to an int
    //check to see if f is already an int or zero:      |f| >= 2**63 is an int
    flds        8(%esp)             //  {0.5}
    fldt        16(%esp)            //  {f, 0.5}
    fabs                            //  {|f|, 0.5}
    flds        4(%esp)             //  {limit,     |f|, 0.5}
    fucomip     %ST(1), %ST         //  {|f|, 0.5}       0x1.0p63 <= |f| or NaN

    //if it is a large int, NaN, replace it with 0.5. This avoids spurious overflows, illegal, and inexact
    fld         %ST(0)              //  {|f|,   |f|,    0.5  }
    fcmovbe     %ST(2), %ST(0)      //  {.5or|f|, |f|,  0.5  }

    //since we have removed large integers, inf and NaN from being used, we can now round safely
    //add 0.5 (with same sign as f) to f, or to the 0.5 we just put there. 
    faddp       %ST(0), %ST(2)      //  {|f|, .5or|f|+.5 }
    fxch                            //  {.5or|f|+.5,    |f|} 

    //then convert to int with truncation to zero
    fisttpll    (%esp)              //  {|f|}   ***USES SSE3***

    //generate NaN result
    fldz                            //  {0, |f|}
    faddp                           //  {|f|}   NaN silenced
    
    //load the integer result back in
    fildll      (%esp)              //  {|intf|,  |f|}

    //if 2**63 <= |f| or NaN, use |f| instead
    fcmovbe      %ST(1), %ST(0)     //  {|intf| or |f|,   |f|}
    fstp         %ST(1)             //  {|intf| or |f|}
    
    //deal with the sign
    fldt        16(%esp)            //  { f, |intf| or |f|} 
    fldz                            //  { 0, f, |intf| or |f| }
    fucomip     %ST(1), %ST         //  { f, |intf| or |f| }           0 < f or f is NaN 
    fld         %ST(1)              //  { |intf| or |f|,  f, |intf| or |f| }  
    fchs                            //  { -(intf or f+0 or f), f, intf or f+0 or f }
    fcmovb      %ST(2), %ST(0)      //  { result, f, intf or f+0 or f}
    
    //return f, if f == 0
    fcmove      %ST(1), %ST(0)      //  { result, f, intf or f+0 or f}
    
    //clean up the stack
    fstp        %ST(2)              //  { intf or f+0 or f, result }
    fstp        %ST(0)              //  { result }

    addl        $12,    %esp
    ret


ENTRY(llroundl)
    pushl       $0x3f000000             //0.5f
	subl		$8, %esp

	//calculate floor(|x|)
	fldt		16(%esp)			//	{f}
	fld			%st(0)				//	{ f, f }
	fabs							//	{|f|, f}
	fld			%st(0)				//	{|f|, |f|, f}
	frndint							//	{rint(|f|), |f|, f}	
	fucomi		%st(1), %st(0)		//	{rint(|f|), |f|, f}
	fldz							//	{0, rint(|f|), |f|, f}			//select between 0 and 1 here to avoid inexact on large inputs
	fld1							//	{1, 0, rint(|f|), |f|, f}		
	fcmove		%st(1), %st(0)		//	{1 or 0, 0, rint(|f|), |f|, f}
	fstp		%st(1)				//	{1 or 0, rint( |f| ), |f|, f }
	fsubr		%st(1), %st(0)		//	{rint(|f|-1or0, rint(|f|), |f|, f }
	fxch							//	{rint(|f|, rint(|f|)-1or0, |f|, f }
	fcmovnbe	%st(1), %st(0)		//	{floor(|f|), rint(|f|)-1, |f|, f}
	fstp		%st(1)				//	{floor(|f|), |f|, f}
	
	//calculate the difference between floor(|f|) and |f|
	fsubr		%st(1), %st(0)		//	{ |f| - floor(|f|), |f|, f }

    //if( |f| - floor(|f|) >= 0.5 )	add 0.5, otherwise add |f| - floor(|f|) (which gives similar results to adding zero)
    flds        8(%esp)				//  { 0.5, |f| - floor(|f|), |f|, f }
	fucomi		%st(1), %st(0)		//	{ 0.5, |f| - floor(|f|), |f|, f }
	fcmovnb		%st(1), %st(0)		//	{0.5 or |f| - floor(|f|), |f| - floor(|f|), |f|, f }
	fstp		%st(1)				//	{0.5 or |f| - floor(|f|), |f|, f }
	faddp							//	{ |f| + 0.5 or |f| - floor(|f|), f }

	//set the sign properly
	fldz							//	{ 0, a, f }
	fucomip		%st(2), %st(0)		//	{ a, f }
	fld			%st(0)				//	{ a, a, f }
	fchs							//	{ -a, a, f }
	fcmovb		%st(1), %st(0)		//	{ +-a, a, f }
	fstp		%st(2)				//	{ a, +-a }
	fstp		%st(0)				//	{ +-a }

    //convert to long long
    fisttpll    (%esp)              //  {}

    //load into return registers
#if defined( __LP64__ ) 
    movll       (%esp),     %eax
#else
    movl        (%esp),     %eax    
    movl        4(%esp),    %edx
#endif
    
    addl        $12,    %esp
    ret
    
ENTRY(lroundl)
    pushl       $0x3f000000             //0.5f
    pushl       $0x4f000000             //0x1.0p31f
	subl		$4, %esp

	//calculate floor(|x|)
	fldt		16(%esp)			//	{f}
	fld			%st(0)				//	{ f, f }
	fabs							//	{|f|, f}
	fld			%st(0)				//	{|f|, |f|, f}
	frndint							//	{rint(|f|), |f|, f}	
	fucomi		%st(1), %st(0)		//	{rint(|f|), |f|, f}
	fldz							//	{0, rint(|f|), |f|, f}			//select between 0 and 1 here to avoid inexact on large inputs
	fld1							//	{1, 0, rint(|f|), |f|, f}		
	fcmove		%st(1), %st(0)		//	{1 or 0, 0, rint(|f|), |f|, f}
	fstp		%st(1)				//	{1 or 0, rint( |f| ), |f|, f }
	fsubr		%st(1), %st(0)		//	{rint(|f|-1or0, rint(|f|), |f|, f }
	fxch							//	{rint(|f|, rint(|f|)-1or0, |f|, f }
	fcmovnbe	%st(1), %st(0)		//	{floor(|f|), rint(|f|)-1, |f|, f}
	fstp		%st(1)				//	{floor(|f|), |f|, f}
	
	//calculate the difference between floor(|f|) and |f|
	fsubr		%st(1), %st(0)		//	{ |f| - floor(|f|), |f|, f }

    //if( |f| - floor(|f|) >= 0.5 )	add 0.5, otherwise add |f| - floor(|f|) (which gives similar results to adding zero)
    flds        8(%esp)				//  { 0.5, |f| - floor(|f|), |f|, f }
	fucomi		%st(1), %st(0)		//	{ 0.5, |f| - floor(|f|), |f|, f }
	fcmovnb		%st(1), %st(0)		//	{0.5 or |f| - floor(|f|), |f| - floor(|f|), |f|, f }
	fstp		%st(1)				//	{0.5 or |f| - floor(|f|), |f|, f }
	faddp							//	{ |f| + 0.5 or |f| - floor(|f|), f }

	//set the sign properly
	fldz							//	{ 0, a, f }
	fucomip		%st(2), %st(0)		//	{ a, f }
	fld			%st(0)				//	{ a, a, f }
	fchs							//	{ -a, a, f }
	fcmovb		%st(1), %st(0)		//	{ +-a, a, f }
	fstp		%st(2)				//	{ a, +-a }
	fstp		%st(0)				//	{ +-a }
	
	//check for overflow
	xorl		%edx,	%edx		// zero edz
	flds		4(%esp)				//	{ 0x1.0p31, +-a }
	fxch							//  { +-a, 0x1.0p31 }
	fucomi		%st(1), %st(0)		//	{ +-a, 0x1.0p31 }
	setnb		%dl
	negl		%edx

#if defined( __LP64__ ) 
    //convert to long long
    fisttpll    (%esp)              //  {0x1.0p31}
	fstp		%st(0)

    //load into return registers
    movll       (%esp),     %eax
	xorll		%edx,		%eax
#else   
    //convert to long
    fisttpl     (%esp)              //  {0x1.0p31}
	fstp		%st(0)

    //load into return registers
    movl        (%esp),     %eax
	xorl		%edx,		%eax
#endif

    addl        $12,		%esp
    ret
	
ENTRY(round)
    pushl       $0x3f000000             //0.5f
    pushl       $0x5f000000             //0x1.0p63 
    subl        $4,     %esp

//convert |f| to an int
    //check to see if f is already an int or zero:      |f| >= 2**63 is an int
    flds        8(%esp)             //  {0.5}
    fldl        16(%esp)            //  {f, 0.5}
    fabs                            //  {|f|, 0.5}
    flds        4(%esp)             //  {limit,     |f|, 0.5}
    fucomip     %ST(1), %ST         //  {|f|, 0.5}       0x1.0p63 <= |f| or NaN

    //if it is a large int, NaN, replace it with 0.5. This avoids spurious overflows, illegal, and inexact
    fld         %ST(0)              //  {|f|,   |f|,    0.5  }
    fcmovbe     %ST(2), %ST(0)      //  {.5or|f|, |f|,  0.5  }

    //since we have removed large integers, inf and NaN from being used, we can now round safely
    //add 0.5 (with same sign as f) to f, or to the 0.5 we just put there. 
    faddp       %ST(0), %ST(2)      //  {|f|, .5or|f|+.5 }
    fxch                            //  {.5or|f|+.5,    |f|} 

    //then convert to int with truncation to zero
    fisttpll    (%esp)              //  {|f|}   ***USES SSE3***

    //generate NaN result
    fldz                            //  {0, |f|}
    faddp                           //  {|f|}   NaN silenced
    
    //load the integer result back in
    fildll      (%esp)              //  {|intf|,  |f|}

    //if 2**63 <= |f| or NaN, use |f| instead
    fcmovbe      %ST(1), %ST(0)     //  {|intf| or |f|,   |f|}
    fstp         %ST(1)             //  {|intf| or |f|}
    
    //deal with the sign
    fldl        16(%esp)            //  { f, |intf| or |f|} 
    fldz                            //  { 0, f, |intf| or |f| }
    fucomip     %ST(1), %ST         //  { f, |intf| or |f| }           0 < f or f is NaN 
    fld         %ST(1)              //  { |intf| or |f|,  f, |intf| or |f| }  
    fchs                            //  { -(intf or f+0 or f), f, intf or f+0 or f }
    fcmovb      %ST(2), %ST(0)      //  { result, f, intf or f+0 or f}
    
    //return f, if f == 0
    fcmove      %ST(1), %ST(0)      //  { result, f, intf or f+0 or f}
    
    //clean up the stack
    fstp        %ST(2)              //  { intf or f+0 or f, result }
    fstp        %ST(0)              //  { result }

#if 0
	fstpl		(%esp)
	movsd		(%esp), %xmm0
#endif

    addl        $12,    %esp
    ret

	
ENTRY(roundf)
    pushl       $0x3f000000             //0.5f
    pushl       $0x5f000000             //0x1.0p63 
    subl        $4,     %esp

//convert |f| to an int
    //check to see if f is already an int or zero:      |f| >= 2**63 is an int
    flds        8(%esp)             //  {0.5}
    flds        16(%esp)            //  {f, 0.5}
    fabs                            //  {|f|, 0.5}
    flds        4(%esp)             //  {limit,     |f|, 0.5}
    fucomip     %ST(1), %ST         //  {|f|, 0.5}       0x1.0p63 <= |f| or NaN

    //if it is a large int, NaN, replace it with 0.5. This avoids spurious overflows, illegal, and inexact
    fld         %ST(0)              //  {|f|,   |f|,    0.5  }
    fcmovbe     %ST(2), %ST(0)      //  {.5or|f|, |f|,  0.5  }

    //since we have removed large integers, inf and NaN from being used, we can now round safely
    //add 0.5 (with same sign as f) to f, or to the 0.5 we just put there. 
    faddp       %ST(0), %ST(2)      //  {|f|, .5or|f|+.5 }
    fxch                            //  {.5or|f|+.5,    |f|} 

    //then convert to int with truncation to zero
    fisttpll    (%esp)              //  {|f|}   ***USES SSE3***

    //generate NaN result
    fldz                            //  {0, |f|}
    faddp                           //  {|f|}   NaN silenced
    
    //load the integer result back in
    fildll      (%esp)              //  {|intf|,  |f|}

    //if 2**63 <= |f| or NaN, use |f| instead
    fcmovbe      %ST(1), %ST(0)     //  {|intf| or |f|,   |f|}
    fstp         %ST(1)             //  {|intf| or |f|}
    
    //deal with the sign
    flds        16(%esp)            //  { f, |intf| or |f|} 
    fldz                            //  { 0, f, |intf| or |f| }
    fucomip     %ST(1), %ST         //  { f, |intf| or |f| }           0 < f or f is NaN 
    fld         %ST(1)              //  { |intf| or |f|,  f, |intf| or |f| }  
    fchs                            //  { -(intf or f+0 or f), f, intf or f+0 or f }
    fcmovb      %ST(2), %ST(0)      //  { result, f, intf or f+0 or f}
    
    //return f, if f == 0
    fcmove      %ST(1), %ST(0)      //  { result, f, intf or f+0 or f}
    
    //clean up the stack
    fstp        %ST(2)              //  { intf or f+0 or f, result }
    fstp        %ST(0)              //  { result }

#if 0
	fstps		(%esp)
	movss		(%esp), %xmm0
#endif

    addl        $12,    %esp
    ret

