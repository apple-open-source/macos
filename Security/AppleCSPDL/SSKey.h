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
// SSKey.h - CSP-wide SSKey base class
//
#ifndef	_H_SSKEY_
#define _H_SSKEY_

#include <Security/CSPsession.h>

#include "SSDatabase.h"

#include <Security/dlclient.h>
#include <Security/SecurityServerClient.h>

namespace Security
{

class CssmKey;

} // end namespace Security

class SSCSPSession;
class SSCSPDLSession;
class SSDLSession;

class SSKey : public ReferencedKey
{
public:
	SSKey(SSCSPSession &session, SecurityServer::KeyHandle keyHandle,
		  CssmKey &ioKey, SSDatabase &inSSDatabase, uint32 inKeyAttr,
		  const CssmData *inKeyLabel);
	SSKey(SSDLSession &session, CssmKey &ioKey, SSDatabase &inSSDatabase,
		  const SSUniqueRecord &uniqueId, CSSM_DB_RECORDTYPE recordType,
		  CssmData &keyBlob);

	virtual ~SSKey();
	void free(const AccessCredentials *accessCred, CssmKey &ioKey,
			  CSSM_BOOL deleteKey);

	SecurityServer::ClientSession &clientSession();
	SecurityServer::KeyHandle keyHandle();

    // ACL retrieval and change operations
	void getOwner(CSSM_ACL_OWNER_PROTOTYPE &owner, CssmAllocator &allocator);
	void changeOwner(const AccessCredentials &accessCred,
					 const AclOwnerPrototype &newOwner);
	void getAcl(const char *selectionTag, uint32 &numberOfAclInfos,
				AclEntryInfo *&aclInfos, CssmAllocator &allocator);
	void changeAcl(const AccessCredentials &accessCred,
				   const AclEdit &aclEdit);

private:
	CssmAllocator &mAllocator;
	SecurityServer::KeyHandle mKeyHandle;
	SSDatabase mSSDatabase;
	SSUniqueRecord mUniqueId;
	CSSM_DB_RECORDTYPE mRecordType;
};


#endif	// _H_SSKEY_
