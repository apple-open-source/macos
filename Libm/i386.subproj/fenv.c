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
*     File:  fenv.c                                                             *
*                                                                              *
*     Contains:   C source code for PowerPC implementations of floating-point  *
*     environmental functions defined in C99.                                  *
*                                                                              *
*     Copyright © 1992-2001 Apple Computer, Inc.  All rights reserved.         *
*                                                                              *
*     Written by Stephen C. Peters, started in November 2001.                  *
*                                                                              *
*     A MathLib v5 file.                                                       *
*                                                                              *
*     Change History (most recent first):                                      *
*                                                                              *
*     21 Nov 01   scp   First derived from v3MathLib's fenv.c.                 *
*                                                                              *
*     A version of gcc higher than 932 is required.                            *
*                                                                              *
*     GCC compiler options:                                                    *
*           optimization level 3 (-O3)                                         *
*           -fschedule-insns -finline-functions -funroll-all-loops             *
*                                                                              *
*******************************************************************************/
#ifdef      __APPLE_CC__
#if         __APPLE_CC__ > 930

#include "fenv.h"

/*  rounding direction mode bits  */
#define      FE_ALL_RND           0x00000c00

/* iapx-v1 Figure 7-13 */
typedef struct {
    unsigned short int __control;
    unsigned short int __reserved1;
    unsigned short int __status;
    unsigned short int __reserved2;
    unsigned int __private3;
    unsigned int __private4;
    unsigned int __private5;
    unsigned int __private6;
    unsigned int __private7;
} __fpustate_t;

/******************************************************************************
*   Implementations of functions which provide access to the exception flags. *
*   The "int" input argument is constructed by bitwise ORs of the exception   *
*   macros defined in fenv.h:  for example, FE_OVERFLOW | FE_INEXACT.         *
******************************************************************************/

static void _fegetexceptflag ( fexcept_t *flagp, int excepts )
{
    int state;
    
    asm volatile ("fnstsw %0" : "=m" (state));
    
    *flagp = state & excepts & FE_ALL_EXCEPT;
}

static void _fesetexceptflag ( const fexcept_t *flagp, int excepts )
{
    int state;
    __fpustate_t currfpu;
    
    asm volatile ("fnstenv %0" : "=m" (currfpu));

    state = currfpu.__status;
    state &= ~( excepts & FE_ALL_EXCEPT ); 	   // clear just the bits indicated
    state |= ( *flagp & excepts & FE_ALL_EXCEPT ); // latch the specified bits
    currfpu.__status = state; 

    asm volatile ("fldenv %0" : : "m" (currfpu));
}

#if defined(BUILDING_FOR_CARBONCORE_LEGACY)
   
/* default environment object        */
const fenv_t _FE_DFL_ENV = (const fenv_t) { FE_TONEAREST | FE_ALL_EXCEPT , 0 }; 

/****************************************************************************
   the "feclearexcept" function clears the exceptions represented by its
   argument. 
****************************************************************************/

void feclearexcept ( int excepts )
{
    fexcept_t zero = 0;
    _fesetexceptflag ( &zero, excepts );
}

/****************************************************************************
   the "feraiseexcept" function raises the exceptions represented by its
   argument.
****************************************************************************/

void feraiseexcept ( int excepts )
{
    fexcept_t t = excepts;
    
    _fesetexceptflag ( &t, excepts );
    asm volatile ("fwait"); 			// and raise the exception(s)
}
      
/****************************************************************************
   The function "fetestexcept" determines which of the specified subset of
   the exception flags are currently set.  It returns the bitwise OR of a
   subset of the exception macros included in "excepts".
****************************************************************************/

int fetestexcept ( int excepts )
{
    int state;
    
    asm volatile ("fnstsw %0" : "=m" (state));
    
    return (state & excepts & FE_ALL_EXCEPT);
}


/*  The following functions provide control of rounding direction modes. */

/****************************************************************************
   The function "fegetround" returns the value of the rounding direction
   macro which represents the current rounding direction.
****************************************************************************/
int
fegetround (void)
{
    int state;
    
    asm volatile ("fnstcw %0" : "=m" (state));
    
    // FE_* rounding enums conveniently mapped to HW bits
    return (state & FE_ALL_RND);
}

/****************************************************************************
  The function "fesetround" establishes the rounding direction
   represented by its argument.  It returns zero if and only if
   the argument matches a rounding direction macro.  If not, the 
   rounding direction is not changed.
****************************************************************************/

int
fesetround (int round)
{    
    if ((round & ~FE_ALL_RND))
        return 1;
    else
    {
        int state;
        
        asm volatile ("fnstcw %0" : "=m" (state));
        
        state &= ~FE_ALL_RND;
        state |= ( round & FE_ALL_RND ); 
    
        asm volatile ("fldcw %0" : : "m" (state));
        return 0;
    }
}

/*    The following functions manage the floating-point environment---
      exception flags and dynamic modes---as one entity.                  */

/****************************************************************************
   The function "fgetenv" stores the current environment in the
   object pointed to by its pointer argument "envp".
****************************************************************************/
   
void fegetenv ( fenv_t *envp )
{
    __fpustate_t currfpu;
    
    asm volatile ("fnstenv %0" : "=m" (currfpu));
    
    envp->__control = currfpu.__control;
    envp->__status = currfpu.__status;
}


/****************************************************************************
   The function "feholdexcept" saves the current environment in
   the object pointed to by its argument "envp" and clears the
   exception flags.  It returns zero.  This function supersedes
   the SANE function "procentry".
****************************************************************************/
   
int feholdexcept ( fenv_t *envp )
{
    int state;
    
    fegetenv ( envp );
    
    state = envp->__control;
    state |= FE_ALL_EXCEPT; // FPU shall handle all exceptions

    asm volatile ("fldcw %0" : : "m" (state));
    
    return 0;
}


/****************************************************************************
   The function "fesetenv" establishes the floating-point environment
   represented by the object pointed to by its argument "envp".  The
   value of "*env_p" must be set by a call to "fegetenv" or
   "feholdexcept", by the macro "FE_DFL_ENV", or by an implementation-
   defined macro of type "fenv_t".
****************************************************************************/
   
void fesetenv ( const fenv_t *envp )
{
    __fpustate_t currfpu;
    asm volatile ("fnstenv %0" : "=m" (currfpu));

    currfpu.__control &= ~( FE_ALL_RND | FE_ALL_EXCEPT );
    currfpu.__control |= ( envp->__control & ( FE_ALL_RND | FE_ALL_EXCEPT ) );
    
    currfpu.__status &= ~FE_ALL_EXCEPT;
    currfpu.__status |= ( envp->__status & FE_ALL_EXCEPT );
    
    asm volatile ("fldenv %0" : : "m" (currfpu));
}

/****************************************************************************
   The function "feupdateenv" saves the current exceptions in its
   automatic storage, installs the environment pointed to by its
   pointer argument "envp", and then re-raises the saved exceptions.
   This function, which supersedes the SANE function "procexit", can
   be used in conjunction with "feholdexcept" to write routines which
   hide spurious exceptions from their callers.
****************************************************************************/
   
void feupdateenv ( const fenv_t *envp )
{
    __fpustate_t currfpu;
    asm volatile ("fnstenv %0" : "=m" (currfpu));
    
    currfpu.__control &= ~( FE_ALL_RND | FE_ALL_EXCEPT );
    currfpu.__control |= ( envp->__control & ( FE_ALL_RND | FE_ALL_EXCEPT ) );
    
    currfpu.__status |= ( envp->__status & FE_ALL_EXCEPT ); // add envp's to current excepts
    
    asm volatile ("fldenv %0" : : "m" (currfpu)); // install envp's control word, preserving status word
    asm volatile ("fwait"); 			  // and raise the exception(s)
}


/* Legacy entry point */
void fegetexcept ( fexcept_t *flagp, int excepts )
{
    _fegetexceptflag (flagp, excepts );
}

/* Legacy entry point */
void fesetexcept ( fexcept_t *flagp, int excepts )
{
    _fesetexceptflag ( flagp, excepts );
}

#else /* !BUILDING_FOR_CARBONCORE_LEGACY */

/****************************************************************************
   "fegetexceptflag" stores a representation of the exception flags indicated
   by the argument "excepts" through the pointer argument "flagp".
****************************************************************************/

void fegetexceptflag ( fexcept_t *flagp, int excepts )
{
    _fegetexceptflag (flagp, excepts );
}

/****************************************************************************
   "fesetexceptflag" sets the exception flags indicated by the argument "excepts",
   to the corresponding state represented in the object pointed to by "flagp".
   This function does not raise exceptions, but only sets the state of the
   flags.
****************************************************************************/

void fesetexceptflag ( const fexcept_t *flagp, int excepts )
{
    _fesetexceptflag ( flagp, excepts );
}

/****************************************************************************
    The float.h macro FLT_ROUNDS has a value derived from fegetround() --
    Addition rounds to 0: zero, 1: nearest, 2: +inf, 3: -inf, -1: unknown 
****************************************************************************/

int __fegetfltrounds( void ) 
{
    switch ( fegetround() )
    {
    case FE_TONEAREST:
        return 1;
    case FE_TOWARDZERO:
        return 0;
    case FE_UPWARD:
        return 2;
    case FE_DOWNWARD:
        return 3;
    default:
        return -1;
    }
}

#endif /* !BUILDING_FOR_CARBONCORE_LEGACY */

#else       /* __APPLE_CC__ version */
#warning A higher version than gcc-932 is required.
#endif      /* __APPLE_CC__ version */
#endif      /* __APPLE_CC__ */
