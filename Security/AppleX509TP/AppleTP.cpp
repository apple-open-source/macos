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


//
// AppleTP.cpp 
//
#include "AppleTP.h"
#include "AppleTPSession.h"


//
// Make and break the plugin object
//
AppleTP::AppleTP()
{
}

AppleTP::~AppleTP()
{
}


//
// Create a new plugin session, our way
//
PluginSession *AppleTP::makeSession(
	CSSM_MODULE_HANDLE handle,
	const CSSM_VERSION &version,
	uint32 subserviceId,
	CSSM_SERVICE_TYPE subserviceType,
	CSSM_ATTACH_FLAGS attachFlags,
	const CSSM_UPCALLS &upcalls)
{
    switch (subserviceType) {
        case CSSM_SERVICE_TP:
            return new AppleTPSession(handle,
                                       *this,
                                       version,
                                       subserviceId,
                                       subserviceType,
                                       attachFlags,
                                       upcalls);
        default:
            CssmError::throwMe(CSSMERR_CSSM_INVALID_SERVICE_MASK);
            return 0;	// placebo
    }
}
