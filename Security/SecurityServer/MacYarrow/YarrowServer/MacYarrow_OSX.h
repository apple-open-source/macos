/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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


/*
	File:		MacYarrow_OSX.h

	Contains:	Yarrow RNG, OS X version

	Written by:	Doug Mitchell

	Copyright: (c) 2000 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

		02/29/00	dpm		Created. 
 
*/

#ifndef	_MAC_YARROW_OSX_H_
#define _MAC_YARROW_OSX_H_

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacTypes.h>

#ifdef	__cplusplus
extern "C" {
#endif

/*
 * Resusable init. entropyFilePath is optional; if NULL, we'll use our
 * own hard-coded default.
 */
OSStatus yarrowServerInit(
	const char *entropyFilePath,
	unsigned *firstTimeout);			// RETURNED, first timeout in milliseconds

void yarrowServerFini();

/* 
 * Add some entropy to the pool. The only "known" failure here is a 
 * result of a failure of this library's early init.
 */
OSStatus yarrowAddEntropy(
	UInt8	*bytes,
	UInt32	numBytes,
	UInt32	bitsOfEntropy,
	unsigned *nextTimeout);		// RETURNED, next timeout in ms,  0 means none (leave
								//   timer alone)
					
/* 
 * Get some random data. Caller mallocs the memory.
 */
OSStatus yarrowGetRandomBytes(
	UInt8	*bytes,	
	UInt32	numBytes);

/* 
 * Handle timer event. Returns next timeout in milliseconds.
 */
unsigned yarrowTimerEvent();

#ifdef	__cplusplus
}
#endif

#endif	/* _MAC_YARROW_OSX_H_*/

