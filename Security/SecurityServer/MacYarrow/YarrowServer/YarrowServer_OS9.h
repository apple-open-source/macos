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
	File:		YarrowServer_OS9.h

	Contains:	Yarrow Server interface, OS 9 version.

	Written by:	Doug Mitchell

	Copyright: (c) 2000 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

		02/29/00	dpm		Created. 
 
*/

/*
 * This interface is only used by the YarrowClient class. It's basically
 * just a 1-to-1 map of YarrowClient's own public member functions. The
 * functions declared here are the only exported symbols from this shared
 * library. 
 */
#ifndef	_YARROW_SERVER_OS9_H_
#define _YARROW_SERVER_OS9_H_

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacTypes.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* 
 * Add some entropy to the pool. The only "known" failure here is a 
 * result of a failure of this library'e early init.
 */
OSErr yarrowAddEntropy(
	UInt8	*bytes,
	UInt32	numBytes,
	UInt32	bitsOfEntropy);
					
/* 
 * Get some random data. Caller mallocs the memory.
 */
OSErr yarrowGetRandomBytes(
	UInt8	*bytes,	
	UInt32	numBytes);

#ifdef	__cplusplus
}
#endif

#endif	/* _YARROW_SERVER_OS9_H_*/

