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
// key - representation of SecurityServer key objects
//
#ifndef _H_KEY
#define _H_KEY

#include "securityserver.h"
#include "acls.h"
#include <Security/utilities.h>
#include <Security/handleobject.h>
#include <Security/keyclient.h>


class Database;

//
// A Key object represents a CSSM_KEY known to the SecurityServer.
// We give each Key a handle that allows our clients to access it, while we use
// the Key's ACL to control such accesses.
// A Key can be used by multiple Connections. Whether more than one Key can represent
// the same actual key object is up to the CSP we use, so let's be tolerant about that.
//
// A note on key attributes: We keep two sets of attribute bits. The internal bits are used
// when talking to our CSP; the external bits are used when negotiating with our client(s).
// The difference is the bits in managedAttributes, which relate to persistent key storage
// and are not digestible by our CSP. The internal attributes are kept in mKey. The external
// ones are kept in mAttributes.
//
class Key : public HandleObject, public SecurityServerAcl {
public:
	Key(Database &db, const KeyBlob *blob);
	Key(Database *db, const CssmKey &newKey, uint32 moreAttributes,
		const AclEntryPrototype *owner = NULL);
	virtual ~Key();
    
    Database *database() const { return mDatabase; }
    bool hasDatabase() const { return mDatabase != NULL; }
	
    // yield the decoded internal key -- internal attributes
	CssmClient::Key key()		{ return keyValue(); }
	const CssmKey &cssmKey()	{ return keyValue(); }
	operator CssmClient::Key ()	{ return keyValue(); }
	operator const CssmKey &()	{ return keyValue(); }
    operator const CSSM_KEY & () { return keyValue(); }
    
    // yield the approximate external key header -- external attributes
    void returnKey(Handle &h, CssmKey::Header &hdr);
	
	// generate the canonical key digest
	const CssmData &canonicalDigest();
    
    // we can also yield an encoded KeyBlob *if* we belong to a database	
	KeyBlob *blob();
    
    // calculate the UID value for this key (if possible)
    KeyUID &uid();
    
    // ACL state management hooks
	void instantiateAcl();
	void changedAcl();
    const Database *relatedDatabase() const;
    
    // key attributes that should not be passed on to the CSP
    static const uint32 managedAttributes = KeyBlob::managedAttributes;
	// these attributes are "forced on" in internal keys (but not always in external attributes)
	static const uint32 forcedAttributes = KeyBlob::forcedAttributes;
	// these attributes are internally generated, and invalid on input
	static const uint32 generatedAttributes =
		CSSM_KEYATTR_ALWAYS_SENSITIVE | CSSM_KEYATTR_NEVER_EXTRACTABLE;
	
	// a version of KeySpec that self-checks and masks for CSP operation
	class KeySpec : public CssmClient::KeySpec {
	public:
		KeySpec(uint32 usage, uint32 attrs);
		KeySpec(uint32 usage, uint32 attrs, const CssmData &label);
	};
	CSSM_KEYATTR_FLAGS attributes() { return mAttributes; }
	
private:
	void setup(const CssmKey &newKey, uint32 attrs);
    void decode();
	CssmClient::Key keyValue();

private:
	CssmClient::Key mKey;	// clear form CssmKey (attributes modified)
	CssmKey::Header mHeaderCache; // cached, cleaned blob header cache
    CSSM_KEYATTR_FLAGS mAttributes; // full attributes (external form)
	bool mValidKey;			// CssmKey form is valid
	CssmAutoData mDigest;	// computed key digest (cached)

	Database *mDatabase;	// the database we belong to, NULL if independent

	KeyBlob *mBlob;			// key blob encoded by mDatabase
	bool mValidBlob;		// mBlob is valid key encoding
    
    KeyUID mUID;			// cached UID
    bool mValidUID;			// UID has been calculated
};


#endif //_H_KEY
