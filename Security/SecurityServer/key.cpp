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
#include "key.h"
#include "server.h"
#include "xdatabase.h"
#include <Security/acl_any.h>


//
// Create a Key object from a database-encoded blob.
// Note that this doesn't decode the blob (yet).
//
Key::Key(Database &db, const KeyBlob *blob)
: SecurityServerAcl(keyAcl, CssmAllocator::standard())
{
    // perform basic validation on the incoming blob
	assert(blob);
    blob->validate(CSSMERR_APPLEDL_INVALID_KEY_BLOB);
    switch (blob->version) {
#if defined(COMPAT_OSX_10_0)
    case blob->version_MacOS_10_0:
        break;
#endif
    case blob->version_MacOS_10_1:
        break;
    default:
        CssmError::throwMe(CSSMERR_APPLEDL_INCOMPATIBLE_KEY_BLOB);
    }

    // set it up
	mDatabase = &db;
    mBlob = blob->copy(CssmAllocator::standard());
    mAttributes = 0;
	mValidBlob = true;
	mValidKey = false;
    mValidUID = false;
    debug("SSkey", "%p created from blob version %lx", this, blob->version);
}


//
// Create a Key from an explicit CssmKey.
//
Key::Key(Database *db, const CssmKey &newKey, uint32 moreAttributes,
	const AclEntryPrototype *owner)
: SecurityServerAcl(keyAcl, CssmAllocator::standard())
{
	if (moreAttributes & CSSM_KEYATTR_PERMANENT) {
		// better have a database to make it permanent in...
		if (!db)
			CssmError::throwMe(CSSMERR_CSP_MISSING_ATTR_DL_DB_HANDLE);
	} else {
		// non-permanent; ignore database
		db = NULL;
	}

	mDatabase = db;
	mValidKey = true;
    mBlob = NULL;
	mValidBlob = false;
    mValidUID = false;
	setup(newKey, moreAttributes);
	
	// establish initial ACL; reinterpret empty (null-list) owner as NULL for resilence's sake
	if (owner && !owner->subject().empty())
		cssmSetInitial(*owner);					// specified
	else
		cssmSetInitial(new AnyAclSubject());	// defaulted
    debug("SSkey", "%p created from key alg=%ld use=0x%lx attr=0x%lx db=%p",
        this, mKey.algorithm(), mKey.usage(), mAttributes, db);
}


//
// Set up the CssmKey part of this Key according to instructions.
//
void Key::setup(const CssmKey &newKey, uint32 moreAttributes)
{
    CssmKey::Header &header = mKey.header();
    
	// copy key header
	header = newKey.header();
    mAttributes = header.attributes() | moreAttributes;
	
	// apply initial values of derived attributes (these are all in managedAttributes)
    if (!(mAttributes & CSSM_KEYATTR_EXTRACTABLE))
        mAttributes |= CSSM_KEYATTR_NEVER_EXTRACTABLE;
    if (mAttributes & CSSM_KEYATTR_SENSITIVE)
        mAttributes |= CSSM_KEYATTR_ALWAYS_SENSITIVE;

    // verify internal/external attribute separation
    assert(!(header.attributes() & managedAttributes));

	// copy key data field, using the CSP's allocator (so the release operation works later)
	mKey.KeyData = CssmAutoData(Server::csp().allocator(), newKey).release();
}


Key::~Key()
{
    CssmAllocator::standard().free(mBlob);
    if (mValidKey)
        Server::csp()->freeKey(mKey);
    debug("SSkey", "%p destroyed", this);
}


//
// Form a KeySpec with checking and masking
//
Key::KeySpec::KeySpec(uint32 usage, uint32 attrs)
	: CssmClient::KeySpec(usage, attrs & ~managedAttributes)
{
	if (attrs & generatedAttributes)
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEYATTR_MASK);
}

Key::KeySpec::KeySpec(uint32 usage, uint32 attrs, const CssmData &label)
	: CssmClient::KeySpec(usage, attrs & ~managedAttributes, label)
{
	if (attrs & generatedAttributes)
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEYATTR_MASK);
}


//
// Retrieve the actual CssmKey value for the key object.
// This will decode its blob if needed (and appropriate).
//
CssmKey &Key::keyValue()
{
    decode();
    return mKey;
}


//
// Ensure that a key is fully decoded.
// This makes the mKey key value available for use, as well as its ACL.
//
void Key::decode()
{
	if (!mValidKey) {
		assert(mDatabase);	// have to have a database (to decode the blob)
		assert(mValidBlob);	// must have a blob to decode
        
        // decode the key
        void *publicAcl, *privateAcl;
        database()->decodeKey(mBlob, mKey, publicAcl, privateAcl);
        importBlob(publicAcl, privateAcl);
        // publicAcl points into the blob; privateAcl was allocated for us
        CssmAllocator::standard().free(privateAcl);
        
        // extract managed attribute bits
        mAttributes = mKey.attributes() & managedAttributes;
        mKey.clearAttribute(managedAttributes);

        // key is valid now
		mValidKey = true;
	}
}


//
// Retrieve the header (only) of a key.
// This is taking the clear header from the blob *without* verifying it.
//
CssmKey::Header &Key::keyHeader()
{
    if (mValidKey) {
        return mKey.header();
    } else {
        assert(mValidBlob);
        return mBlob->header;
    }
}


//
// Return a key's handle and header in external form
//
void Key::returnKey(Handle &h, CssmKey::Header &hdr)
{
    // return handle
    h = handle();
    
    // return header with external attributes merged
    hdr = keyHeader();
    hdr.setAttribute(mAttributes);
}


//
// Encode a key into a blob.
// We'll have to ask our Database to do this - we don't have its keys.
// Note that this returns memory we own and keep.
//
KeyBlob *Key::blob()
{
	if (mDatabase == NULL)	// can't encode independent keys
		CssmError::throwMe(CSSMERR_DL_INVALID_DB_HANDLE);
	if (!mValidBlob) {
        assert(mValidKey);		// must have valid key to encode
		//@@@ release mBlob memory here

        // export Key ACL to blob form
        CssmData pubAcl, privAcl;
        exportBlob(pubAcl, privAcl);
        
        // assemble external key form
        CssmKey externalKey = mKey;
        externalKey.setAttribute(mAttributes);

        // encode the key and replace blob
        KeyBlob *newBlob = database()->encodeKey(externalKey, pubAcl, privAcl);
        CssmAllocator::standard().free(mBlob);
        mBlob = newBlob;
        mValidBlob = true;
    
        // clean up and go
        database()->allocator.free(pubAcl);
        database()->allocator.free(privAcl);
	}
	return mBlob;
}


//
// Return the UID of a key (the hash of its bits)
//
KeyUID &Key::uid()
{
    if (!mValidUID) {
        //@@@ calculate UID here
        memset(&mUID, 0, sizeof(mUID));
        mValidUID = true;
    }
    return mUID;
}


//
// Intercept ACL change requests and reset blob validity
//
void Key::instantiateAcl()
{
	decode();
}

void Key::noticeAclChange()
{
	mValidBlob = false;
}

const Database *Key::relatedDatabase() const
{ return database(); }
