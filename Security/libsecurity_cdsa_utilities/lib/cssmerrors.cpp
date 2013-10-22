/*
 * Copyright (c) 2000-2004,2006 Apple Computer, Inc. All Rights Reserved.
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


//
// cssmerrors
//
#include <security_cdsa_utilities/cssmerrors.h>
#include <security_utilities/mach++.h>
#include <Security/cssmapple.h>
#include <Security/SecBase.h>

namespace Security {


CssmError::CssmError(CSSM_RETURN err) : error(err)
{
    SECURITY_EXCEPTION_THROW_CSSM(this, err);
}


const char *CssmError::what() const throw ()
{
	return "CSSM exception";
}


OSStatus CssmError::osStatus() const
{
	if (error == CSSM_ERRCODE_INVALID_POINTER)
	{
		return errSecParam;
	}
	
	return error;
}


int CssmError::unixError() const
{
	OSStatus err = osStatus();
	
	// embedded UNIX errno values are returned verbatim
	if (err >= errSecErrnoBase && err <= errSecErrnoLimit)
		return err - errSecErrnoBase;
	
	// re-map certain CSSM errors
    switch (err) {
	case CSSM_ERRCODE_MEMORY_ERROR:
		return ENOMEM;
	case CSSMERR_APPLEDL_DISK_FULL:
		return ENOSPC;
	case CSSMERR_APPLEDL_QUOTA_EXCEEDED:
		return EDQUOT;
	case CSSMERR_APPLEDL_FILE_TOO_BIG:
		return EFBIG;
	default:
		// cannot map this to errno space
		return -1;
    }
}


void CssmError::throwMe(CSSM_RETURN err)
{
	throw CssmError(err);
}


CSSM_RETURN CssmError::merge(CSSM_RETURN error, CSSM_RETURN base)
{
	if (0 < error && error < CSSM_ERRORCODE_COMMON_EXTENT) {
		return base + error;
	} else {
		return error;
	}
}

//
// Get a CSSM_RETURN from a CommonError
//
CSSM_RETURN CssmError::cssmError(const CommonError &error, CSSM_RETURN base)
{
	if (const CssmError *cssm = dynamic_cast<const CssmError *>(&error)) {
		return cssmError(cssm->error, base);
	} else if (const MachPlusPlus::Error *mach = dynamic_cast<const MachPlusPlus::Error *>(&error)) {
		switch (mach->error) {
		case BOOTSTRAP_UNKNOWN_SERVICE:
		case MIG_SERVER_DIED:
			return CSSM_ERRCODE_SERVICE_NOT_AVAILABLE;
		default: 
			return CSSM_ERRCODE_INTERNAL_ERROR;
		}
	} else {
		return error.osStatus();
	}
}

CSSM_RETURN CssmError::cssmError(CSSM_RETURN error, CSSM_RETURN base)
{
    if (0 < error && error < CSSM_ERRORCODE_COMMON_EXTENT) {
        return base + error;
    } else {
        return error;
    }
}


}   // namespace Security
