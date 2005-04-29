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
// AppleTP.h - TP module for X509, SSL, iSign
//
#ifndef _H_APPLETP
#define _H_APPLETP

#include <security_cdsa_plugin/cssmplugin.h>
#include <security_cdsa_plugin/pluginsession.h>

class AppleTP : public CssmPlugin
{
public:
    AppleTP();
    ~AppleTP();

    PluginSession *makeSession(
			CSSM_MODULE_HANDLE handle,
			const CSSM_VERSION &version,
			uint32 subserviceId,
			CSSM_SERVICE_TYPE subserviceType,
			CSSM_ATTACH_FLAGS attachFlags,
			const CSSM_UPCALLS &upcalls);
private:
    // Don't copy AppleTP
    AppleTP(const AppleTP&);
    void operator=(const AppleTP&);
};


#endif //_H_APPLETP
