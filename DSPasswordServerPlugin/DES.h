/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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


#ifndef __DEStypesLocal__
#define __DEStypesLocal__

#if defined(MACINTOSH)
	#include <CarbonCore/MacTypes.h>
#endif

typedef long KeysArray[32];			// Encryption Key array type

// Use a version number for the keyschedule routine
// All new code should use version two.  This contains a bug fix for version 1

#define kDESVersion1	1 //  PPCToolbox, AFP 2.0 for one way random number exchange.
#define kDESVersion2 	2 //  AFP 2.1 in FileShare and AppleShare for two way random number exchange.

#define kFixedDESChunk			8

typedef struct EncryptBlk
{
	unsigned long 	keyHi;			
	unsigned long 	keyLo;			

} EncryptBlk;

#if defined(__cplusplus)
	extern "C" {
#endif

void KeySched( const EncryptBlk *Key, long*	keysArrayPtr, short version );
void Encode( long*	keysArrayPtr, long Count, char * encryptData );
void Decode( long*	keysArrayPtr, long Count, char * encryptedData );

#if defined(__cplusplus)
}
#endif

#endif

