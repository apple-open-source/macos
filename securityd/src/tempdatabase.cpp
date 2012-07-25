/*
 * Copyright (c) 2004,2008 Apple Inc. All Rights Reserved.
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
// tempdatabase - temporary (scratch) storage for keys
//
#include <security_cdsa_utilities/cssmdata.h>
#include <security_cdsa_utilities/cssmkey.h>
#include <security_cdsa_client/wrapkey.h>
#include "tempdatabase.h"
#include "localkey.h"
#include "server.h"
#include "session.h"
#include "agentquery.h"


//
// Temporary-space Key objects are almost normal LocalKeys, with the key
// matter always preloaded (and thus no deferral of instantiation).
// A TempKey bears its own ACL.
//
class TempKey : public LocalKey, public SecurityServerAcl {
public:
	TempKey(Database &db, const CssmKey &newKey, uint32 moreAttributes,
		const AclEntryPrototype *owner = NULL);
	
	Database *relatedDatabase();
	
	SecurityServerAcl &acl()	{ return *this; }

public:
	// SecurityServerAcl personality
	AclKind aclKind() const;
};


TempKey::TempKey(Database &db, const CssmKey &newKey, uint32 moreAttributes,
		const AclEntryPrototype *owner)
	: LocalKey(db, newKey, moreAttributes)
{
	setOwner(owner);
	db.addReference(*this);
}


AclKind TempKey::aclKind() const
{
	return keyAcl;
}


Database *TempKey::relatedDatabase()
{
	return NULL;
}


//
// Create a Database object from initial parameters (create operation)
//
TempDatabase::TempDatabase(Process &proc)
	: LocalDatabase(proc)
{
	proc.addReference(*this);
}


//
// A LocalDatabase itself doesn't really have a database name,
// but here's an innocent placeholder.
//
const char *TempDatabase::dbName() const
{
	return "(transient)";
}

bool TempDatabase::transient() const
{
	return true;
}


//
// Invoke the Security Agent to get a passphrase (other than for a Keychain)
//
void TempDatabase::getSecurePassphrase(const Context &context,
									   string &passphrase)
{
    uint32 verify = context.getInt(CSSM_ATTRIBUTE_VERIFY_PASSPHRASE, CSSMERR_CSSM_ATTRIBUTE_NOT_IN_CONTEXT);
    
    CssmData *promptData = context.get<CssmData>(CSSM_ATTRIBUTE_PROMPT);
	
    QueryGenericPassphrase agentQuery;
    agentQuery.inferHints(Server::process());
    agentQuery(promptData, verify, passphrase);
}


void TempDatabase::makeSecurePassphraseKey(const Context &context,
										   const AccessCredentials *cred, 
										   const AclEntryPrototype *owner, 
										   uint32 usage, uint32 attrs, 
										   RefPointer<Key> &newKey)
{
	secdebug("SSdb", "requesting secure passphrase");
	
	string passphrase;
	getSecurePassphrase(context, passphrase);
	
	secdebug("SSdb", "wrapping securely-obtained passphrase as key");
	
	// CssmKey rawKey(StringData(passphrase)) confuses gcc
	StringData passphraseData(passphrase);
	CssmKey rawKey(passphraseData);
	rawKey.algorithm(context.algorithm());
	rawKey.blobType(CSSM_KEYBLOB_RAW);
	rawKey.blobFormat(CSSM_KEYBLOB_WRAPPED_FORMAT_NONE);
	rawKey.keyClass(CSSM_KEYCLASS_SESSION_KEY);
	
	CssmClient::UnwrapKey unwrap(Server::csp(), CSSM_ALGID_NONE);
	CssmKey cspKey;
	unwrap(rawKey, TempKey::KeySpec(usage, attrs), cspKey);
	
	newKey = makeKey(cspKey, attrs & TempKey::managedAttributes, owner);
}


//
// Obtain "secure passphrases" for the CSP.  Useful for PKCS 12.  
// 
void TempDatabase::generateKey(const Context &context,
							   const AccessCredentials *cred, 
							   const AclEntryPrototype *owner, 
							   uint32 usage, uint32 attrs, 
							   RefPointer<Key> &newKey)
{
	switch (context.algorithm())
	{
		case CSSM_ALGID_SECURE_PASSPHRASE:
			makeSecurePassphraseKey(context, cred, owner, usage, attrs, newKey);
			break;
		default:
			LocalDatabase::generateKey(context, cred, owner, usage, attrs, newKey);
			return;
	}
}


//
// Make a new TempKey
//
RefPointer<Key> TempDatabase::makeKey(const CssmKey &newKey,
	uint32 moreAttributes, const AclEntryPrototype *owner)
{
	assert(!newKey.attribute(CSSM_KEYATTR_PERMANENT));
	return new TempKey(*this, newKey, moreAttributes, owner);
}
