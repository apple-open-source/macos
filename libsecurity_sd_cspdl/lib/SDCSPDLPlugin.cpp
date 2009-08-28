/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
// SDCSPDLPlugin.cpp - Securityd-based CSP/DL plug-in module.
//
#include "SDCSPDLPlugin.h"

#include "SDCSPSession.h"
#include "SDDLSession.h"
#include <securityd_client/dictionary.h>

using namespace SecurityServer;


//
// Make and break the plugin object
//
SDCSPDLPlugin::SDCSPDLPlugin()
	: EventListener(kNotificationDomainCDSA, kNotificationAllEvents),
	  mRawCsp(gGuidAppleCSP)
{
}

SDCSPDLPlugin::~SDCSPDLPlugin()
{
}


//
// Create a new plugin session, our way
//
PluginSession *
SDCSPDLPlugin::makeSession(CSSM_MODULE_HANDLE handle,
						 const CSSM_VERSION &version,
						 uint32 subserviceId,
						 CSSM_SERVICE_TYPE subserviceType,
						 CSSM_ATTACH_FLAGS attachFlags,
						 const CSSM_UPCALLS &upcalls)
{
    switch (subserviceType)
	{
        case CSSM_SERVICE_CSP:
            return new SDCSPSession(handle,
									*this,
									version,
									subserviceId,
									subserviceType,
									attachFlags,
									upcalls,
									mSDCSPDLSession,
									mRawCsp);
        case CSSM_SERVICE_DL:
            return new SDDLSession(handle,
								   *this,
								   version,
								   subserviceId,
								   subserviceType,
								   attachFlags,
								   upcalls,
								   mDatabaseManager,
								   mSDCSPDLSession);
        default:
            CssmError::throwMe(CSSMERR_CSSM_INVALID_SERVICE_MASK);
//            return 0;	// placebo
    }
}


//
// Accept callback notifications from securityd and dispatch them
// upstream through CSSM.
//
void SDCSPDLPlugin::consume(NotificationDomain domain, NotificationEvent event,
	const CssmData &data)
{
	NameValueDictionary nvd(data);
	assert(domain == kNotificationDomainCDSA);
	if (const NameValuePair *uidp = nvd.FindByName(SSUID_KEY)) {
		CssmSubserviceUid *uid = (CssmSubserviceUid *)uidp->Value().data();
		assert(uid);
		secdebug("sdcspdl", "sending callback %d upstream", event);
		sendCallback(event, n2h (uid->subserviceId()), CSSM_SERVICE_DL | CSSM_SERVICE_CSP);
	} else
		secdebug("sdcspdl", "callback event %d has no SSUID data", event);
}
