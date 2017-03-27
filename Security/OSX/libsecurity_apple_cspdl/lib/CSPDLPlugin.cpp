/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
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
// CSPDLPlugin.cpp - File Based CSP/DL plug-in module.
//
#include "CSPDLPlugin.h"

#include "SSCSPSession.h"
#include "SSDLSession.h"


//
// Make and break the plugin object
//
CSPDLPlugin::CSPDLPlugin()
	: mRawCsp(gGuidAppleCSP)
{
}

CSPDLPlugin::~CSPDLPlugin()
{
}


//
// Create a new plugin session, our way
//
PluginSession *
CSPDLPlugin::makeSession(CSSM_MODULE_HANDLE handle,
						 const CSSM_VERSION &version,
						 uint32 subserviceId,
						 CSSM_SERVICE_TYPE subserviceType,
						 CSSM_ATTACH_FLAGS attachFlags,
						 const CSSM_UPCALLS &upcalls)
{
    switch (subserviceType)
	{
        case CSSM_SERVICE_CSP:
            return new SSCSPSession(handle,
									*this,
									version,
									subserviceId,
									subserviceType,
									attachFlags,
									upcalls,
									mSSCSPDLSession,
									mRawCsp);
        case CSSM_SERVICE_DL:
            return new SSDLSession(handle,
								   *this,
								   version,
								   subserviceId,
								   subserviceType,
								   attachFlags,
								   upcalls,
								   mDatabaseManager,
								   mSSCSPDLSession);
        default:
            CssmError::throwMe(CSSMERR_CSSM_INVALID_SERVICE_MASK);
    }
}
