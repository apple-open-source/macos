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
// SSCSPDLSession.h - File Based CSP/DL plug-in module.
//
#ifndef _H_SSCSPDLSESSION
#define _H_SSCSPDLSESSION

#include <security_cdsa_plugin/CSPsession.h>
#include <securityd_client/ssclient.h>


class CSPDLPlugin;
class SSFactory;
class SSCSPSession;
class SSDatabase;
class SSKey;

class SSCSPDLSession: public KeyPool
{
public:
	SSCSPDLSession();

	void makeReferenceKey(SSCSPSession &session,
						  SecurityServer::KeyHandle inKeyHandle,
						  CssmKey &outKey, SSDatabase &inSSDatabase,
						  uint32 inKeyAttr, const CssmData *inKeyLabel);
	SSKey &lookupKey(const CssmKey &inKey);

	/* Notification we receive when a key's acl has been modified. */
	void didChangeKeyAcl(SecurityServer::ClientSession &clientSession,
		SecurityServer::KeyHandle keyHandle, CSSM_ACL_AUTHORIZATION_TAG tag);

	static void didChangeKeyAclCallback(void *context, SecurityServer::ClientSession &clientSession,
		SecurityServer::KeyHandle keyHandle, CSSM_ACL_AUTHORIZATION_TAG tag);
};


#endif // _H_SSCSPDLSESSION
