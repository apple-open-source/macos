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
: SecurityServerAcl(keyAcl, CssmAllocator::standard()), mDigest(Server::csp().allocator())
{
    // perform basic validation on the incoming blob
	assert(blob);
    blob->validate(CSSMERR_APPLEDL_INVALID_KEY_BLOB);
    switch (blob->version()) {
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
    secdebug("SSkey", "%p (handle 0x%lx) created from blob version %lx",
		this, handle(), blob->version());
}


//
// Create a Key from an explicit CssmKey.
//
Key::Key(Database *db, const CssmKey &newKey, uint32 moreAttributes,
	const AclEntryPrototype *owner)
: SecurityServerAcl(keyAcl, CssmAllocator::standard()), mDigest(Server::csp().allocator())
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
    secdebug("SSkey", "%p (handle 0x%lx) created from key alg=%ld use=0x%lx attr=0x%lx db=%p",
        this, handle(), mKey.header().algorithm(), mKey.header().usage(), mAttributes, db);
}


//
// Set up the CssmKey part of this Key according to instructions.
//
void Key::setup(const CssmKey &newKey, uint32 moreAttributes)
{
	mKey = CssmClient::Key(Server::csp(), newKey, false);
    CssmKey::Header &header = mKey->header();
    
	// copy key header
	header = newKey.header();
    mAttributes = (header.attributes() & ~forcedAttributes) | moreAttributes;
	
	// apply initial values of derived attributes (these are all in managedAttributes)
    if (!(mAttributes & CSSM_KEYATTR_EXTRACTABLE))
        mAttributes |= CSSM_KEYATTR_NEVER_EXTRACTABLE;
    if (mAttributes & CSSM_KEYATTR_SENSITIVE)
        mAttributes |= CSSM_KEYATTR_ALWAYS_SENSITIVE;

    // verify internal/external attribute separation
    assert((header.attributes() & managedAttributes) == forcedAttributes);
}


Key::~Key()
{
    CssmAllocator::standard().free(mBlob);
    secdebug("SSkey", "%p destroyed", this);
}


//
// Form a KeySpec with checking and masking
//
Key::KeySpec::KeySpec(uint32 usage, uint32 attrs)
	: CssmClient::KeySpec(usage, (attrs & ~managedAttributes) | forcedAttributes)
{
	if (attrs & generatedAttributes)
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEYATTR_MASK);
}

Key::KeySpec::KeySpec(uint32 usage, uint32 attrs, const CssmData &label)
	: CssmClient::KeySpec(usage, (attrs & ~managedAttributes) | forcedAttributes, label)
{
	if (attrs & generatedAttributes)
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEYATTR_MASK);
}


//
// Retrieve the actual CssmKey value for the key object.
// This will decode its blob if needed (and appropriate).
//
CssmClient::Key Key::keyValue()
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
		CssmKey key;
        database()->decodeKey(mBlob, key, publicAcl, privateAcl);
		mKey = CssmClient::Key(Server::csp(), key);
        importBlob(publicAcl, privateAcl);
        // publicAcl points into the blob; privateAcl was allocated for us
        CssmAllocator::standard().free(privateAcl);
        
        // extract managed attribute bits
        mAttributes = mKey.header().attributes() & managedAttributes;
        mKey.header().clearAttribute(managedAttributes);
		mKey.header().setAttribute(forcedAttributes);

        // key is valid now
		mValidKey = true;
	}
}


//
// Return a key's handle and header in external form
//
void Key::returnKey(Handle &h, CssmKey::Header &hdr)
{
    // return handle
    h = handle();
	
	// obtain the key header, from the valid key or the blob if no valid key
	if (mValidKey) {
		hdr = mKey.header();
	} else {
		assert(mValidBlob);
		hdr = mBlob->header;
		n2hi(hdr);	// correct for endian-ness
	}
    
    // adjust for external attributes
	hdr.clearAttribute(forcedAttributes);
    hdr.setAttribute(mAttributes);
}


//
// Generate the canonical key digest.
// This is defined by a CSP feature that we invoke here.
//
const CssmData &Key::canonicalDigest()
{
	if (!mDigest) {
		CssmClient::PassThrough ctx(Server::csp());
		ctx.key(keyValue());
		CssmData *digest = NULL;
		ctx(CSSM_APPLECSP_KEYDIGEST, (const void *)NULL, &digest);
		assert(digest);
		mDigest.set(*digest);	// takes ownership of digest data
		Server::csp().allocator().free(digest);	// the CssmData itself
	}
	return mDigest.get();
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
		externalKey.clearAttribute(forcedAttributes);
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

void Key::changedAcl()
{
	mValidBlob = false;
}

const Database *Key::relatedDatabase() const
{ return database(); }
