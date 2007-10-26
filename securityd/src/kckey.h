/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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
// key - representation of SecurityServer key objects
//
#ifndef _H_KCKEY
#define _H_KCKEY

#include "localkey.h"
#include <security_cdsa_utilities/handleobject.h>
#include <security_cdsa_client/keyclient.h>


class KeychainDatabase;


//
// A KeychainKey object represents a CssmKey that is stored in a KeychainDatabase.
//
// This is a LocalKey with deferred instantiation. A KeychainKey always exists in one of
// two states:
//  (*) Decoded: The CssmKey is valid; the blob may or may not be.
//  (*) Encoded: The blob is valid, the CssmKey may or may not be.
// One of (blob, CssmKey) is always valid. The process of decoding the CssmKey from the
// blob (and vice versa) requires keychain cryptography, which unlocks the keychain
// (implicitly as needed).
// Other than that, this is just a LocalKey.
//
class KeychainKey : public LocalKey, public SecurityServerAcl {
public:
	KeychainKey(Database &db, const KeyBlob *blob);
	KeychainKey(Database &db, const CssmKey &newKey, uint32 moreAttributes,
		const AclEntryPrototype *owner = NULL);
	virtual ~KeychainKey();
    
	KeychainDatabase &database() const;
    
    // we can also yield an encoded KeyBlob
	KeyBlob *blob();
	
	void invalidateBlob();
    
    // ACL state management hooks
	void instantiateAcl();
	void changedAcl();
    Database *relatedDatabase();
	void validate(AclAuthorization auth, const AccessCredentials *cred, Database *relatedDatabase);

public:
	// SecurityServerAcl personality
	AclKind aclKind() const;
	
	SecurityServerAcl &acl();
	
private:
    void decode();
	void getKey();
	virtual void getHeader(CssmKey::Header &hdr); // get header (only) without mKey

private:
	CssmKey::Header mHeaderCache; // cached, cleaned blob header cache

	KeyBlob *mBlob;			// key blob encoded by mDatabase
	bool mValidBlob;		// mBlob is valid key encoding
};


#endif //_H_KCKEY
