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
// tokenkey - remote reference key on an attached hardware token
//
#include "tokenkey.h"
#include "tokendatabase.h"


//
// Construct a TokenKey from a reference handle and key header
//
TokenKey::TokenKey(TokenDatabase &db, KeyHandle tokenKey, const CssmKey::Header &hdr)
	: Key(db), mKey(tokenKey), mHeader(hdr)
{
	db.addReference(*this);
}


//
// Destruction of a TokenKey releases the reference from tokend
//
TokenKey::~TokenKey()
{
	try {
		database().token().tokend().releaseKey(mKey);
	} catch (...) {
		secinfo("tokendb", "%p release key handle %u threw (ignored)",
			this, mKey);
	}
}


//
// Links through the object mesh
//
TokenDatabase &TokenKey::database() const
{
	return referent<TokenDatabase>();
}

Token &TokenKey::token()
{
	return database().token();
}

GenericHandle TokenKey::tokenHandle() const
{
	return mKey;	// tokend-side handle
}


//
// Canonical external attributes (taken directly from the key header)
//
CSSM_KEYATTR_FLAGS TokenKey::attributes()
{
	return mHeader.attributes();
}


//
// Return-to-caller processing (trivial in this case)
//
void TokenKey::returnKey(Handle &h, CssmKey::Header &hdr)
{
	h = this->handle();
	hdr = mHeader;
}


//
// We're a key (duh)
//
AclKind TokenKey::aclKind() const
{
	return keyAcl;
}


//
// Right now, key ACLs are at the process level
//
SecurityServerAcl &TokenKey::acl()
{
	return *this;
}


//
// The related database is, naturally enough, the TokenDatabase we're in
//
Database *TokenKey::relatedDatabase()
{
	return &database();
}


//
// Generate the canonical key digest.
// This is not currently supported through tokend. If we need it,
// we'll have to force unlock and fake it (in tokend, most likely).
//
const CssmData &TokenKey::canonicalDigest()
{
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}
