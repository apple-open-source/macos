/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*******************************************************************************
*                                                                              *
*     File:  gamma9.c                                                          *
*                                                                              *
*     Contains: Legacy gamma(). 		       			       *
*                                                                              *
*     Copyright © 2001 Apple Computer, Inc.  All rights reserved.              *
*                                                                              *
*     Written by Stephen C. Peters, started in November 2001.                  *
*                                                                              *
*     A MathLib v5 file.                                                       *
*                                                                              *
*     Change History (most recent first):                                      *
*                                                                              *
*     12 Dec 01   scp   First created.                                         *
*                                                                              *
*     A version of gcc higher than 932 is required.                            *
*                                                                              *
*     GCC compiler options:                                                    *
*           optimization level 3 (-O3)                                         *
*           -fschedule-insns -finline-functions -funroll-all-loops             *
*                                                                              *
*******************************************************************************/
#include "math.h"

int signgam;

double gamma ( double x )
{
    double g = tgamma ( x ); // return *True* gamma a la MacOS9 Mathlib
    
    signgam = (g < 0.0 ? -1 : 1); // set signgam as a courtesy.
    
    return g;
}

float gammaf ( float x )
{
    float g = tgammaf ( x ); // return *True* gamma a la MacOS9 Mathlib
    
    signgam = (g < 0.0 ? -1 : 1); // set signgam as a courtesy.
    
    return g;
}

double gamma_r ( double x, int *psigngam )
{
    double g = tgamma ( x ); // return *True* gamma a la MacOS9 Mathlib
    
    *psigngam = (g < 0.0 ? -1 : 1); 
        
    return g;
}

float gammaf_r ( float x, int *psigngam )
{
    float g = tgammaf ( x ); // return *True* gamma a la MacOS9 Mathlib
    
    *psigngam = (g < 0.0 ? -1 : 1); 
        
    return g;
}

