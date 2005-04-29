/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
#ifndef _H_TOKENKEY
#define _H_TOKENKEY


//
// tokenkey - remote reference key on an attached hardware token
//
#include "key.h"
#include "tokenacl.h"

class TokenDatabase;


//
// The token-specific instance of a Key
//
class TokenKey : public Key, public TokenAcl {
public:
	TokenKey(TokenDatabase &db, KeyHandle hKey, const CssmKey::Header &hdr);
	~TokenKey();
	
	TokenDatabase &database() const;
	Token &token();
	const CssmKey::Header &header() const { return mHeader; }
	KeyHandle tokenHandle() const;
	
	CSSM_KEYATTR_FLAGS attributes();
	void returnKey(Handle &h, CssmKey::Header &hdr);
	const CssmData &canonicalDigest();
	
	SecurityServerAcl &acl();
	Database *relatedDatabase();

public:
	// SecurityServerAcl personality
	AclKind aclKind() const;

private:
	KeyHandle mKey;			// tokend reference handle
	CssmKey::Header mHeader; // key header as maintained by tokend
};

#endif //_H_TOKENKEY
