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
	File:		cssmdatetime.h

	Contains:	defines for the CSSM date and time utilities for the Mac

	Written by:	The Hindsight team

	Copyright:	© 1997-2000 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

	To Do:
*/

#ifndef _CSSM_DATE_TIME_UTILS
#define _CSSM_DATE_TIME_UTILS

#include <Security/cssm.h>

#ifdef _CPP_CSSM_DATE_TIME_UTILS
# pragma export on
#endif

namespace Security
{

namespace CSSMDateTimeUtils
{

// Get the current time.
extern void GetCurrentMacLongDateTime(SInt64 &outMacDate);

extern void TimeStringToMacSeconds(const CSSM_DATA &inUTCTime, UInt32 &ioMacDate);
extern void TimeStringToMacLongDateTime(const CSSM_DATA &inUTCTime, SInt64 &outMacDate);

// Length of inLength is an input parameter and must be 14 or 16.
// The outData parameter must point to a buffer of at least inLength bytes.
extern void MacSecondsToTimeString(UInt32 inMacDate, UInt32 inLength, void *outData);
extern void MacLongDateTimeToTimeString(const SInt64 &inMacDate,
                                        UInt32 inLength, void *outData);
}; // end namespace CSSMDateTimeUtils

} // end namespace Security

#ifdef _CPP_CSSM_DATE_TIME_UTILS
# pragma export off
#endif

#endif //_CSSM_DATE_TIME_UTILS
