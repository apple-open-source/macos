/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*******************************************************************************
*                                                                              *
*      File nan.c,                                                             *
*      Function nan.                                                           *
*                                                                              *
*      Copyright © 1991-2001 Apple Computer, Inc.  All rights reserved.        *
*                                                                              *
*      Written by A. Sazegari, started on October 1991.                        *
*      Modified and ported by Robert A. Murley (ram) for Mac OS X.             *
*                                                                              *
*      A MathLib v4 file.                                                      *
*                                                                              *
*      November 20 2001: <scp> made nan and nanf publically visible again      *
*      November  8 2001: made nan and nanf __private_extern__ to prevent       *
*                        conflict with CarbonCore.                             *
*      November  6 2001: commented out warning about Intel architectures.      *
*                        changed i386 stubs to call abort().                   *
*      November  2 2001: added stubs for i386 routines.                        *
*      January   6 1993: changed the value of EPSILON to avoid the denormal    *
*                        trap on the 68040.                                    *
*      September24 1993: took the Ò#include support.hÓ out.                    *
*      May      17 1993: changed the routine nan with kurt's help.  nan now    *
*                        conforms to the nceg specification with some          *
*                        restrictions. see below.                              *
*      July     28 1993: fixed the problem with nan("+n"), where n ³ 0.        *
*      July     29 1993: completely replaced the program with a new outlook    *
*                        using bin2dec functions.                              *
*      August   25 1993: added implementation of nanf and nanl.                *
*      April     7 1997: deleted nanl and deferred to AuxiliaryDD.c            *
*      September20 2001: removed calls to binary-decimal converter.            *
*      September25 2001: added ram's name.                                     *
*     October   08 2001: changed compiler errors to warnings.                  *
*                                                                              *
*     A version of gcc higher than 932 is required.                            *
*                                                                              *
*      GCC compiler options:                                                   *
*            optimization level 3 (-O3)                                        *
*            -fschedule-insns -finline-functions -funroll-all-loops            *
*                                                                              *
*******************************************************************************/

#ifdef      __APPLE_CC__
#if         __APPLE_CC__ > 930

#include    "fp_private.h"
#define     NULL       0

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
      hexdouble QNaN;
      int       zero = 0x30;               // ASCII zero
      int       n, next;

      QNaN.i.lo = 0x00000000;
      QNaN.i.hi = 0x7ff80000;
      
      if ( string == NULL )
            return QNaN.d;
      n = *(string++);
      if ( n != 0 )                              // if string is not null
      {
            n -= zero;                           // remove ASCII bias
            next = *string;
            if ( next > 0 )                      // if string has more than 1 character
            {
                  n *= 10;                       // make first digit tens digit
                  n += *(string++) - zero;
                  next = *string;
                  if ( next > 0 )                // if string has more than 2 characters
                  {
                        n *= 10;                 // first two digits are hundreds and tens
                        n += *string - zero;
                        if ( n > 255 )
                            n = 255;
                  }
            }
      }
      
      QNaN.i.hi |= n << 5;
      
      return QNaN.d;
      }

float nanf ( const char *string )
      {
      return ( ( float ) nan ( string ) );
      }

#else       /* __APPLE_CC__ version */
#warning A higher version than gcc-932 is required.
#endif      /* __APPLE_CC__ version */
#endif      /* __APPLE_CC__ */

