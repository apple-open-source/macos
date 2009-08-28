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
// localkey - Key objects that store a local CSSM key object
//
#include "localkey.h"
#include "server.h"
#include "database.h"
#include <security_cdsa_utilities/acl_any.h>


//
// Create a Key from an explicit CssmKey.
//
LocalKey::LocalKey(Database &db, const CssmKey &newKey, CSSM_KEYATTR_FLAGS moreAttributes)
	: Key(db), mDigest(Server::csp().allocator())
{
	mValidKey = true;
	setup(newKey, moreAttributes);
    secdebug("SSkey", "%p (handle %#x) created from key alg=%u use=0x%x attr=0x%x db=%p",
        this, handle(), mKey.header().algorithm(), mKey.header().usage(), mAttributes, &db);
}


LocalKey::LocalKey(Database &db, CSSM_KEYATTR_FLAGS attributes)
	: Key(db), mValidKey(false), mAttributes(attributes), mDigest(Server::csp().allocator())
{
}


//
// Set up the CssmKey part of this Key according to instructions.
//
void LocalKey::setup(const CssmKey &newKey, CSSM_KEYATTR_FLAGS moreAttributes)
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


LocalKey::~LocalKey()
{
    secdebug("SSkey", "%p destroyed", this);
}


void LocalKey::setOwner(const AclEntryPrototype *owner)
{
	// establish initial ACL; reinterpret empty (null-list) owner as NULL for resilence's sake
	if (owner && !owner->subject().empty())
		acl().cssmSetInitial(*owner);					// specified
	else
		acl().cssmSetInitial(new AnyAclSubject());		// defaulted
}


LocalDatabase &LocalKey::database() const
{
	return referent<LocalDatabase>();
}


//
// Retrieve the actual CssmKey value for the key object.
// This will decode its blob if needed (and appropriate).
//
CssmClient::Key LocalKey::keyValue()
{
	StLock<Mutex> _(*this);
    if (!mValidKey) {
		getKey();
		mValidKey = true;
	}
    return mKey;
}


//
// Return external key attributees
//
CSSM_KEYATTR_FLAGS LocalKey::attributes()
{
	return mAttributes;
}


//
// Return a key's handle and header in external form
//
void LocalKey::returnKey(U32HandleObject::Handle &h, CssmKey::Header &hdr)
{
	StLock<Mutex> _(*this);

    // return handle
    h = this->handle();
	
	// obtain the key header, from the valid key or the blob if no valid key
	if (mValidKey) {
		hdr = mKey.header();
	} else {
		getHeader(hdr);
	}
    
    // adjust for external attributes
	hdr.clearAttribute(forcedAttributes);
    hdr.setAttribute(mAttributes);
}


//
// Generate the canonical key digest.
// This is defined by a CSP feature that we invoke here.
//
const CssmData &LocalKey::canonicalDigest()
{
	StLock<Mutex> _(*this);
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
// Default getKey/getHeader calls - should never be called
//
void LocalKey::getKey()
{
	assert(false);
}

void LocalKey::getHeader(CssmKey::Header &)
{
	assert(false);
}


//
// Form a KeySpec with checking and masking
//
LocalKey::KeySpec::KeySpec(CSSM_KEYUSE usage, CSSM_KEYATTR_FLAGS attrs)
	: CssmClient::KeySpec(usage, (attrs & ~managedAttributes) | forcedAttributes)
{
	if (attrs & generatedAttributes)
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEYATTR_MASK);
}

LocalKey::KeySpec::KeySpec(CSSM_KEYUSE usage, CSSM_KEYATTR_FLAGS attrs, const CssmData &label)
	: CssmClient::KeySpec(usage, (attrs & ~managedAttributes) | forcedAttributes, label)
{
	if (attrs & generatedAttributes)
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEYATTR_MASK);
}
