/*
 * Copyright (c) 2000-2001,2004-2006,2008-2009 Apple Inc. All Rights Reserved.
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
// key - representation of securityd key objects
//
#include "kckey.h"
#include "server.h"
#include "database.h"
#include <security_cdsa_utilities/acl_any.h>
#include <security_cdsa_utilities/cssmendian.h>


//
// Create a Key object from a database-encoded blob.
// Note that this doesn't decode the blob (yet).
//
KeychainKey::KeychainKey(Database &db, const KeyBlob *blob)
	: LocalKey(db, n2h(blob->header.attributes()))
{
    // perform basic validation on the incoming blob
	assert(blob);
    blob->validate(CSSMERR_APPLEDL_INVALID_KEY_BLOB);
    switch (blob->version()) {
#if defined(COMPAT_OSX_10_0)
    case KeyBlob::version_MacOS_10_0:
        break;
#endif
    case KeyBlob::version_MacOS_10_1:
        break;
    default:
        CssmError::throwMe(CSSMERR_APPLEDL_INCOMPATIBLE_KEY_BLOB);
    }

    // set it up
    mBlob = blob->copy(Allocator::standard());
	mValidBlob = true;
	db.addReference(*this);
    secdebug("SSkey", "%p (handle %#x) created from blob version %x",
		this, handle(), blob->version());
}


//
// Create a Key from an explicit CssmKey.
//
KeychainKey::KeychainKey(Database &db, const CssmKey &newKey, uint32 moreAttributes,
	const AclEntryPrototype *owner)
	: LocalKey(db, newKey, moreAttributes)
{
	assert(moreAttributes & CSSM_KEYATTR_PERMANENT);
	setOwner(owner);
    mBlob = NULL;
	mValidBlob = false;
	db.addReference(*this);
}


KeychainKey::~KeychainKey()
{
    Allocator::standard().free(mBlob);
    secdebug("SSkey", "%p destroyed", this);
}


KeychainDatabase &KeychainKey::database() const
{
	return referent<KeychainDatabase>();
}


//
// Retrieve the actual CssmKey value for the key object.
// This will decode its blob if needed (and appropriate).
//
void KeychainKey::getKey()
{
    decode();
}

void KeychainKey::getHeader(CssmKey::Header &hdr)
{
	assert(mValidBlob);
	hdr = mBlob->header;
	n2hi(hdr);	// correct for endian-ness
}


//
// Ensure that a key is fully decoded.
// This makes the mKey key value available for use, as well as its ACL.
// Caller must hold the key object lock.
//
void KeychainKey::decode()
{
	if (!mValidKey) {
		assert(mValidBlob);	// must have a blob to decode
        
        // decode the key
        void *publicAcl, *privateAcl;
		CssmKey key;
        database().decodeKey(mBlob, key, publicAcl, privateAcl);
		mKey = CssmClient::Key(Server::csp(), key);
        acl().importBlob(publicAcl, privateAcl);
        // publicAcl points into the blob; privateAcl was allocated for us
        Allocator::standard().free(privateAcl);
        
        // extract managed attribute bits
        mAttributes = mKey.header().attributes() & managedAttributes;
        mKey.header().clearAttribute(managedAttributes);
		mKey.header().setAttribute(forcedAttributes);

        // key is valid now
		mValidKey = true;
	}
}


//
// Encode a key into a blob.
// We'll have to ask our Database to do this - we don't have its keys.
// Note that this returns memory we own and keep.
//
KeyBlob *KeychainKey::blob()
{
	if (!mValidBlob) {
        assert(mValidKey);		// must have valid key to encode

        // export Key ACL to blob form
        CssmData pubAcl, privAcl;
		acl().exportBlob(pubAcl, privAcl);
        
        // assemble external key form
        CssmKey externalKey = mKey;
		externalKey.clearAttribute(forcedAttributes);
        externalKey.setAttribute(mAttributes);

        // encode the key and replace blob
        KeyBlob *newBlob = database().encodeKey(externalKey, pubAcl, privAcl);
        Allocator::standard().free(mBlob);
        mBlob = newBlob;
        mValidBlob = true;
    
        // clean up and go
        acl().allocator.free(pubAcl);
        acl().allocator.free(privAcl);
	}
	return mBlob;
}

void KeychainKey::invalidateBlob()
{
	mValidBlob = false;
}


//
// Override ACL-related methods and events.
// Decode the key before ACL activity; invalidate the stored blob on ACL edits;
// and return the key's database as "related".
//
void KeychainKey::instantiateAcl()
{
	StLock<Mutex> _(*this);
	decode();
}

void KeychainKey::changedAcl()
{
	invalidateBlob();
}


//
// Intercept Key validation and double-check that the keychain is (still) unlocked
//
void KeychainKey::validate(AclAuthorization auth, const AccessCredentials *cred,
	Database *relatedDatabase)
{
	if(!mBlob->isClearText()) {
		/* unlock not needed for cleartext keys */
		if (KeychainDatabase *db = dynamic_cast<KeychainDatabase *>(relatedDatabase))
			db->unlockDb();
	}
	SecurityServerAcl::validate(auth, cred, relatedDatabase);
	database().activity();		// upon successful validation
}


//
// We're a key (duh)
//
AclKind KeychainKey::aclKind() const
{
	return keyAcl;
}


Database *KeychainKey::relatedDatabase()
{
	return &database();
}

SecurityServerAcl &KeychainKey::acl()
{
	return *this;
}
