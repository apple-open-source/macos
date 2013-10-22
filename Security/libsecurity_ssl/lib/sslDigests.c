/*
 * Copyright (c) 2000-2001,2005-2007,2010-2012 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

/*
 * sslDigests.c - Interface between SSL and SHA, MD5 digest implementations
 */

#include "sslMemory.h"
#include "sslDigests.h"
#include <Security/SecBase.h>

#define DIGEST_PRINT		0
#if		DIGEST_PRINT
#define dgprintf(s)	printf s
#else
#define dgprintf(s)
#endif


/*
 * Public general hash functions
 */

/*
 * A convenience wrapper for HashReference.clone, which has the added benefit of
 * allocating the state buffer for the caller.
 */
OSStatus
CloneHashState(
	const HashReference *ref, 
	const SSLBuffer *state, 
	SSLBuffer *newState)
{   
	OSStatus      err;
    if ((err = SSLAllocBuffer(newState, ref->contextSize)))
        return err;
	return ref->clone(state, newState);
}

/*
 * Wrapper for HashReference.init.
 */
OSStatus
ReadyHash(const HashReference *ref, SSLBuffer *state)
{   
	OSStatus      err;
    if ((err = SSLAllocBuffer(state, ref->contextSize)))
        return err;
    return ref->init(state);
}

/*
 * Wrapper for HashReference.close. Tolerates NULL state and frees it if it's
 * there.
 */
OSStatus CloseHash(const HashReference *ref, SSLBuffer *state)
{
	OSStatus serr;

	if(state->data == NULL) {
		return errSecSuccess;
	}
	serr = ref->close(state);
	if(serr) {
		return serr;
	}
	return SSLFreeBuffer(state);
}
