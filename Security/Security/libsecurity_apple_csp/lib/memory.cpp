/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */

#ifdef	BSAFE_CSP_ENABLE


//
// memory - memory functions for BSafe
//
#include <bsafe.h>
#include <aglobal.h>
#include "bsafecspi.h"


// declared in bsafecspi.h....
Allocator *BSafe::normAllocator;
Allocator *BSafe::privAllocator;	

// We use the private allocator for all BSAFE-alalocated memory.
// Memory allocated my BSAFE should never be visible by apps. 

POINTER CALL_CONV T_malloc (unsigned int size)
{
    return reinterpret_cast<POINTER>(BSafe::privAllocator->malloc(size));
}

POINTER CALL_CONV T_realloc (POINTER p, unsigned int size)
{
    POINTER result;
    if ((result = (POINTER)BSafe::privAllocator->realloc(p, size)) == NULL_PTR)
    free (p);
    return (result);
}

void CALL_CONV T_free (POINTER p)
{
    BSafe::privAllocator->free(p);
}
#endif	/* BSAFE_CSP_ENABLE */
