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
 	File:		systemEntropy.h
	
 	Contains:	System entropy collector, using 
				sysctl(CTL_KERN:KERN_KDEBUG) trace info

 	Copyright:	(C) 2000 by Apple Computer, Inc., all rights reserved

 	Written by:	Doug Mitchell <dmitch@apple.com>	
*/

#ifndef	_YARROW_SYSTEM_ENTROPY_H_
#define _YARROW_SYSTEM_ENTROPY_H_

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacTypes.h>

#ifdef	__cplusplus
extern "C" {
#endif

/* start collecting system entropy */
int systemEntropyBegin(
	UInt32 bufSize);		// desired number of bytes to collect


/* gather system entropy in caller-supplied buffer */
int systemEntropyCollect(
	UInt8 *buf,
	UInt32 bufSize,
	UInt32 *numBytes,		// RETURNED - number of bytes obtained
	UInt32 *bitsOfEntropy);	// RETURNED - est. amount of entropy

/* minimum number of milliseconds between calling systemEntropyBegin() and
 * systemEntropyCollect() */
#define SYSTEM_ENTROPY_COLLECT_TIME		100
//#define SYSTEM_ENTROPY_COLLECT_TIME		5000

#ifdef	__cplusplus
}
#endif

#endif	/* _YARROW_SYSTEM_ENTROPY_H_*/
