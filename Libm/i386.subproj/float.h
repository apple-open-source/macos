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
*     File:  float.h	                                                       *
*                                                                              *
*     Contains: macros defining parameters of the C99 floating point model.    *
*                                                                              *
*******************************************************************************/

#ifndef __FLOAT__
#define __FLOAT__
   
#define FLT_ROUNDS 	   (__fegetfltrounds()) 
#define FLT_EVAL_METHOD    2 /* evaluate all operations and constants to the range of float double type */
#define FLT_RADIX          2
#define DECIMAL_DIG        21

#define FLT_MANT_DIG       24
#define FLT_DIG		   6
#define FLT_MIN_EXP        (-125)
#define FLT_MIN_10_EXP     (-37)
#define FLT_MAX_EXP        128
#define FLT_MAX_10_EXP     38
#define FLT_MAX		   3.40282347e38F
#define FLT_EPSILON        1.1920928955078125e-7F
#define FLT_MIN		   1.1754943508222875e-38F

#define DBL_MANT_DIG       53
#define DBL_DIG		   15
#define DBL_MIN_EXP        (-1021)
#define DBL_MIN_10_EXP     (-307)
#define DBL_MAX_EXP        1024
#define DBL_MAX_10_EXP     308
#define DBL_MAX		   1.7976931348623157e308L
#define DBL_EPSILON        2.2204460492503131e-16L
#define DBL_MIN		   2.2250738585072014e-308L

#define LDBL_MANT_DIG       64
#define LDBL_DIG	    18
#define LDBL_MIN_EXP        (-16381)
#define LDBL_MIN_10_EXP     (-4931)
#define LDBL_MAX_EXP        16384
#define LDBL_MAX_10_EXP     4932
#define LDBL_MAX	    1.18973149535723176502e+4932L
#define LDBL_EPSILON        1.08420217248550443401e-19L
#define LDBL_MIN	    3.36210314311209350626e-4932L

#ifdef __cplusplus
extern "C" {
#endif

extern int __fegetfltrounds( void ); 

#ifdef __cplusplus
}
#endif

#endif /* __FLOAT__ */
