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
	File:		entropyFile.h

	Contains:	Module to maintain MacYarrow's entropy file.

	Written by:	Doug Mitchell

	Copyright: (c) 2000 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

		02/29/00	dpm		Created.
 
*/

#ifndef	_ENTROPY_FILE_H_
#define _ENTROPY_FILE_H_

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacTypes.h>

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Specify optional entropy file path. If this is never called,
 * this module will use its own default path. 
 */
OSErr setEntropyFilePath(
	const char *path);
	
/*
 * Write specified data to entropy file. A new file will be created
 * if none exists. Existing file's data is replaced with caller's data.
 */
OSErr writeEntropyFile(
	UInt8		*bytes,
	UInt32		numBytes);
	
/*
 * Read data from entropy file.
 */
OSErr readEntropyFile(
	UInt8		*bytes,
	UInt32		numBytes,		// max # of bytes to read
	UInt32		*actualBytes);	// RETURNED - number of bytes actually read
	
#if defined(__cplusplus)
}
#endif

#endif	/* _ENTROPY_FILE_H_*/
