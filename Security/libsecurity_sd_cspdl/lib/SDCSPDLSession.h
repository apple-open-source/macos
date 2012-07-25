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
// SDCSPDLSession.h - File Based CSP/DL plug-in module.
//
#ifndef _H_SDCSPDLSESSION
#define _H_SDCSPDLSESSION

#include <security_cdsa_plugin/CSPsession.h>
#include <securityd_client/ssclient.h>


class SDCSPDLPlugin;
class SDFactory;
class SDCSPSession;
class SDKey;

class SDCSPDLSession: public KeyPool
{
public:
	SDCSPDLSession();

	void makeReferenceKey(SDCSPSession &session,
						  SecurityServer::KeyHandle inKeyHandle,
						  CssmKey &outKey, CSSM_DB_HANDLE inDBHandle,
						  uint32 inKeyAttr, const CssmData *inKeyLabel);
	SDKey &lookupKey(const CssmKey &inKey);
};


#endif // _H_SDCSPDLSESSION
