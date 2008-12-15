/*
 *  Copyright (c) 2004-2007 Apple Inc. All Rights Reserved.
 * 
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */

/*
 *  PIVError.cpp
 *  TokendPIV
 */

/* ---------------------------------------------------------------------------
 *
 *		MODIFY
 *		- Fill in your token specific error codes below
 *
 * ---------------------------------------------------------------------------
*/

/*
	Errors:
	card blocked: shall not be made and the PIV Card Application shall return the status word '69 83'. 
*/

#include "PIVError.h"

#include <Security/cssmerr.h>

//
// PIVError exceptions
//
PIVError::PIVError(uint16_t sw) : SCardError(sw)
{
	IFDEBUG(debugDiagnose(this));
}

PIVError::~PIVError() throw ()
{
}

const char *PIVError::what() const throw ()
{ return "PIV error"; }

OSStatus PIVError::osStatus() const
{
    switch (statusWord)
    {
	case PIV_AUTHENTICATION_FAILED_0:
	case PIV_AUTHENTICATION_FAILED_1:
	case PIV_AUTHENTICATION_FAILED_2:
	case PIV_AUTHENTICATION_FAILED_3:
        return CSSM_ERRCODE_OPERATION_AUTH_DENIED;
	// At least leave the default case
    default:
        return SCardError::osStatus();
    }
}

void PIVError::throwMe(uint16_t sw)
{ throw PIVError(sw); }

#if !defined(NDEBUG)

void PIVError::debugDiagnose(const void *id) const
{
    secdebug("exception", "%p PIVError %s (%04hX)",
             id, errorstr(statusWord), statusWord);
}

const char *PIVError::errorstr(uint16_t sw) const
{
	switch (sw)
	{
	case PIV_AUTHENTICATION_FAILED_0:
		return "Authentication failed, 0 retries left.";
	case PIV_AUTHENTICATION_FAILED_1:
		return "Authentication failed, 1 retry left.";
	case PIV_AUTHENTICATION_FAILED_2:
		return "Authentication failed, 2 retries left.";
	case PIV_AUTHENTICATION_FAILED_3:
		return "Authentication failed, 3 retries left.";
	// At least leave the default case
	default:
		return SCardError::errorstr(sw);
	}
}

#endif //NDEBUG

