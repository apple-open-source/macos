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
// cryptoclient - client interface to CSSM CSP encryption/decryption operations
//
#ifndef _H_CDSA_CLIENT_CRYPTOCLIENT
#define _H_CDSA_CLIENT_CRYPTOCLIENT  1

#include <Security/cspclient.h>
#include <Security/keyclient.h>

namespace Security
{

namespace CssmClient
{

class Crypt : public Context
{
public:
	Crypt(const CSP &csp, CSSM_ALGORITHMS alg);
	
public:
    // Context attributes
	CSSM_ENCRYPT_MODE mode() const			{ return mMode; }
	void mode(CSSM_ENCRYPT_MODE m)			{ mMode = m; set(CSSM_ATTRIBUTE_MODE, m); }
	const AccessCredentials *cred() const	{ return mCred; }
	void cred(const AccessCredentials *c);
	Key key() const							{ return mKey; }
	void key(const Key &k)				   	{ mKey = k; set(CSSM_ATTRIBUTE_KEY, k); }
	const CssmData &initVector() const		{ return *mInitVector; }
	void initVector(const CssmData &v)		{ mInitVector = &v; set(CSSM_ATTRIBUTE_INIT_VECTOR, v); }
	CSSM_PADDING padding() const			{ return mPadding; }
	void padding(CSSM_PADDING p)			{ mPadding = p; set(CSSM_ATTRIBUTE_PADDING, p); }

    // Other attributes
	AclEntryInput aclEntry() const			{ return mAclEntry; }
	void aclEntry(AclEntryInput &aclEntry)	{ mAclEntry = aclEntry; }

protected:
	void activate();
	
protected:
	CSSM_ENCRYPT_MODE mMode;
	Key mKey;
	const CssmData *mInitVector;
	CSSM_PADDING mPadding;
	
protected:
	const AccessCredentials *mCred;
	AclEntryInput mAclEntry;
};



//
// An encryption context
//
class Encrypt : public Crypt
{
public:
	Encrypt(const CSP &csp, CSSM_ALGORITHMS alg) : Crypt(csp, alg) {};
	

public:
	// integrated
	uint32 encrypt(const CssmData *in, uint32 inCount, CssmData *out, uint32 outCount,
		CssmData &remData);
	uint32 encrypt(const CssmData &in, CssmData &out, CssmData &remData)
	{ return encrypt(&in, 1, &out, 1, remData); }
	
	// staged update
	void init(); // Optional
	uint32 encrypt(const CssmData *in, uint32 inCount, CssmData *out, uint32 outCount);
	uint32 encrypt(const CssmData &in, CssmData &out)
	{ return encrypt(&in, 1, &out, 1); }
	// staged final
	void final(CssmData &remData);

};

//
// An Decryption context
//
class Decrypt : public Crypt
{
public:
	Decrypt(const CSP &csp, CSSM_ALGORITHMS alg) : Crypt(csp, alg) {};
	
public:
	// integrated
	uint32 decrypt(const CssmData *in, uint32 inCount, CssmData *out, uint32 outCount,
		CssmData &remData);
	uint32 decrypt(const CssmData &in, CssmData &out, CssmData &remData)
	{ return decrypt(&in, 1, &out, 1, remData); }

	// staged update
	void init(); // Optional
	uint32 decrypt(const CssmData *in, uint32 inCount, CssmData *out, uint32 outCount);
	uint32 decrypt(const CssmData &in, CssmData &out)
	{ return decrypt(&in, 1, &out, 1); }
	// staged final
	void final(CssmData &remData);
};


} // end namespace CssmClient

} // end namespace Security

#endif // _H_CDSA_CLIENT_CRYPTOCLIENT
