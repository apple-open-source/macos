/*
 * Copyright (c) 2004,2011,2014 Apple Inc. All Rights Reserved.
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
// CSPDLCSPDL.h - File Based CSP/DL plug-in module.
//
#ifndef _H_SD_CSPDLPLUGIN
#define _H_SD_CSPDLPLUGIN

#include "SDCSPDLSession.h"
#include "SDCSPDLDatabase.h"
#include "SDFactory.h"
#include <security_cdsa_client/cspclient.h>
#include <security_cdsa_plugin/cssmplugin.h>
#include <securityd_client/eventlistener.h>


class SDCSPSession;

class SDCSPDLPlugin : public CssmPlugin, private SecurityServer::EventListener
{
	NOCOPY(SDCSPDLPlugin)
public:
    SDCSPDLPlugin();
    ~SDCSPDLPlugin();

    PluginSession *makeSession(CSSM_MODULE_HANDLE handle,
                               const CSSM_VERSION &version,
                               uint32 subserviceId,
                               CSSM_SERVICE_TYPE subserviceType,
                               CSSM_ATTACH_FLAGS attachFlags,
                               const CSSM_UPCALLS &upcalls);

private:
	void consume(SecurityServer::NotificationDomain domain,
		SecurityServer::NotificationEvent event, const CssmData &data);
    bool initialized() { return mInitialized; }

private:
	friend class SDCSPSession;
	friend class SDCSPDLSession;
	SDCSPDLSession mSDCSPDLSession;
    SDCSPDLDatabaseManager mDatabaseManager;
    SDFactory mSDFactory;
	CssmClient::CSP mRawCsp;		// raw (nonsecure) CSP connection
};


#endif //_H_SD_CSPDLPLUGIN
