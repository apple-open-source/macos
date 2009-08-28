/*
 *  Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
 *  CACError.cpp
 *  TokendMuscle
 */

#include "CACError.h"

#include <Security/cssmerr.h>

//
// CACError exceptions
//
CACError::CACError(uint16_t sw) : SCardError(sw)
{
#if MAX_OS_X_VERSION_MIN_REQUIRED <= MAX_OS_X_VERSION_10_5
	IFDEBUG(debugDiagnose(this));
#else
	SECURITY_EXCEPTION_THROW_OTHER(this, sw, (char *)"CAC");
#endif
}

CACError::~CACError() throw ()
{
}

const char *CACError::what() const throw ()
{ return "CAC error"; }

OSStatus CACError::osStatus() const
{
    switch (statusWord)
    {
	case CAC_AUTHENTICATION_FAILED_0:
	case CAC_AUTHENTICATION_FAILED_1:
	case CAC_AUTHENTICATION_FAILED_2:
	case CAC_AUTHENTICATION_FAILED_3:
        return CSSM_ERRCODE_OPERATION_AUTH_DENIED;
    default:
        return SCardError::osStatus();
    }
}

void CACError::throwMe(uint16_t sw)
{ throw CACError(sw); }

#if !defined(NDEBUG)

#if MAX_OS_X_VERSION_MIN_REQUIRED <= MAX_OS_X_VERSION_10_5

void CACError::debugDiagnose(const void *id) const
{
    secdebug("exception", "%p CACError %s (%04hX)",
             id, errorstr(statusWord), statusWord);
}

#endif // MAX_OS_X_VERSION_MIN_REQUIRED <= MAX_OS_X_VERSION_10_5

const char *CACError::errorstr(uint16_t sw) const
{
	switch (sw)
	{
	case CAC_AUTHENTICATION_FAILED_0:
		return "Authentication failed, 0 retries left.";
	case CAC_AUTHENTICATION_FAILED_1:
		return "Authentication failed, 1 retry left.";
	case CAC_AUTHENTICATION_FAILED_2:
		return "Authentication failed, 2 retries left.";
	case CAC_AUTHENTICATION_FAILED_3:
		return "Authentication failed, 3 retries left.";
	default:
		return SCardError::errorstr(sw);
	}
}

#endif //NDEBUG

