/*
 * Copyright (c) 2004,2011-2012,2014 Apple Inc. All Rights Reserved.
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
// SDKey.h - CSP-wide SDKey base class
//
#ifndef	_H_SDKEY_
#define _H_SDKEY_

#include <securityd_client/ssclient.h>
#include <security_cdsa_plugin/CSPsession.h>

namespace Security
{

class CssmKey;

} // end namespace Security

class SDCSPSession;
class SDCSPDLSession;
class SDDLSession;

class SDKey : public ReferencedKey
{
public:
	SDKey(SDCSPSession &session, SecurityServer::KeyHandle keyHandle,
		  CssmKey &ioKey, CSSM_DB_HANDLE inDBHandle, uint32 inKeyAttr,
		  const CssmData *inKeyLabel);
	SDKey(SDDLSession &session, CssmKey &ioKey, SecurityServer::KeyHandle hKey, CSSM_DB_HANDLE inDBHandle,
		  SecurityServer::RecordHandle record, CSSM_DB_RECORDTYPE recordType,
		  CssmData &keyBlob);
	
	virtual ~SDKey();
	void free(const AccessCredentials *accessCred, CssmKey &ioKey,
			  CSSM_BOOL deleteKey);

	SecurityServer::ClientSession &clientSession();

	/* Might return SecurityServer::noKey if the key has not yet been instantiated. */
	SecurityServer::KeyHandle optionalKeyHandle() const;

	/* Will instantiate the key if needed. */
	SecurityServer::KeyHandle keyHandle();

    // ACL retrieval and change operations
	void getOwner(CSSM_ACL_OWNER_PROTOTYPE &owner, Allocator &allocator);
	void changeOwner(const AccessCredentials &accessCred,
					 const AclOwnerPrototype &newOwner);
	void getAcl(const char *selectionTag, uint32 &numberOfAclInfos,
				AclEntryInfo *&aclInfos, Allocator &allocator);
	void changeAcl(const AccessCredentials &accessCred,
				   const AclEdit &aclEdit);

private:
	Allocator &mAllocator;
	SecurityServer::KeyHandle mKeyHandle;
	CSSM_DB_HANDLE mDatabase;
	SecurityServer::RecordHandle mRecord;
	SecurityServer::ClientSession &mClientSession;
};


#endif	// _H_SDKEY_
