/*******************************************************************************
*                                                                              *
*      File nan.c,                                                             *
*      Function nan.                                                           *
*                                                                              *
*      Copyright © 1991 Apple Computer, Inc.  All rights reserved.             *
*                                                                              *
*      Written by Ali Sazegari, started on October 1991.                       *
*                                                                              *
*      January  6  1993: changed the value of EPSILON to avoid the denormal    *
*                        trap on the 68040.                                    *
*      September24 1993: took the ³#include support.h² out.                    *
*      May      17 1993: changed the routine nan with kurt's help.  nan now    *
*                        conforms to the nceg specification with some          *
*                        restrictions. see below.                              *
*      July     28 1993: fixed the problem with nan("+n"), where n „ 0.        *
*      July     29 1993: completely replaced the program with a new outlook    *
*                        using bin2dec functions.                              *
*      August   25 1993: added implementation of nanf and nanl.                *
*                                                                              *
*      W A R N I N G:  This routine expects a 64 bit double model.             *
*                                                                              *
*******************************************************************************/

#include      <fenv.h>
#include      "fp_private.h"
//#include <CarbonCore/fp.h>

typedef union
      {
       long double ldbl;
       struct
            {
            double msd;
            double lsd;
            } headtail;
      } doubledouble;

/*******************************************************************************
********************************************************************************
*                                    N  A  N                                   *
********************************************************************************
*                                                                              *
*      Return a NaN with the appropriate nan code.                             *
*                                                                              *
*      what does it do?  it returns back a nan with the code in the lower half *
*      of the higher long word.                                                *
*      if the string is empty, the code is zero;                               *
*      else if the string is a negative number, then the code is zero;         *
*      else if the string is zero, then the code is zero;                      *
*      else if the string is larger than 255, then 255 is returned;            *
*      else, the numerical content of the string is returned.                  *
*                                                                              *
*******************************************************************************/

double nan ( const char *string )
      {
      short int NaNCode, vp, ix = 0;
      decimal dc;
      union
            {
            dHexParts hex;
            double dbl;
            } QNaN;

      QNaN.hex.high = 0x7FF80000;
      QNaN.hex.low  = 0x00000000;
      
      str2dec ( string, &ix, &dc, &vp );
      NaNCode = dec2s ( &dc );

      if ( 0 <= NaNCode && NaNCode <= 255 ) 
            QNaN.hex.high += ( NaNCode << 5 );
	else if ( NaNCode > 255 )
		{
		NaNCode = 255;
            QNaN.hex.high += ( NaNCode << 5 );
		}

      return QNaN.dbl;
      }

float nanf ( const char *string )
	{
	return ( ( float ) nan ( string ) );
	}

#if GCC_SUPPORTS_LONG_DOUBLE_AS_128_BITS

long double nanl ( const char *string )
	{
	doubledouble arg;
	arg.headtail.msd = nan ( string );
	arg.headtail.lsd = 0.0;
	return arg.ldbl;
	}

#endif  /* GCC_SUPPORTS_LONG_DOUBLE_AS_128_BITS */

