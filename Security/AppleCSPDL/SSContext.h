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
// SSContext.h - Security Server contexts 
//
#ifndef _H_SS_CONTEXT
#define _H_SS_CONTEXT

#include <Security/CSPsession.h>
#include <Security/SecurityServerClient.h>

//
// Parent class for all CSPContexts implemented in this CSP.  Currently the
// only thing we add is a reference to our creator's session.
//
class SSCSPSession;
class SSKey;

class SSContext : public CSPFullPluginSession::CSPContext
{
public:
	SSContext(SSCSPSession &session);
	virtual void init(const Context &context, bool encoding);

protected:
	SecurityServer::ClientSession &clientSession();
	SSCSPSession &mSession;

	// We remeber a pointer to the passed in context and assume it will
	// remain a valid from init(), update() all the way though the call to
	// final().
	const Context *mContext;
};

// SSSignContext -- Context for Sign, and GenerateMac operations
class SSSignContext : public SSContext
{
public:
	SSSignContext(SSCSPSession &session);
	virtual void update(const CssmData &data);
	virtual size_t outputSize(bool final, size_t inSize);
	virtual void final(CssmData &out);
};

// SSVerifyContext -- Context for Verify, and VerifyMac operations
class SSVerifyContext : public SSContext
{
public:
	SSVerifyContext(SSCSPSession &session);
	virtual void update(const CssmData &data);
	virtual void final(const CssmData &in);
};

// Context for GenerateRandom operations
class SSRandomContext : public SSContext
{
public:
	SSRandomContext(SSCSPSession &session);
	virtual void init(const Context &context, bool);
	virtual size_t outputSize(bool final, size_t inSize);
	virtual void final(CssmData &out);

private:
	uint32 mOutSize;
};

// Context for Encrypt and Decrypt operations
class SSCryptContext : public SSContext
{
public:
	SSCryptContext(SSCSPSession &session);
	~SSCryptContext();
	virtual void init(const Context &context, bool encoding);
	virtual size_t inputSize(size_t outSize);
	virtual size_t outputSize(bool final, size_t inSize);
	virtual void minimumProgress(size_t &in, size_t &out);
	virtual void update(void *inp, size_t &inSize, void *outp,
						size_t &outSize);
	virtual void final(CssmData &out);

private:
	void freeBuffer();

	SecurityServer::KeyHandle mKeyHandle;
	uint32 mCurrent;
	uint32 mCapacity;
	void *mBuffer;
};

#if 0
// Context for key (pair) generation
class SSKeyGenContext : public SSContext
{
public:
	SSKeyGenContext(SSCSPSession &session);

	// Subclass implements generate(const Context &, CssmKey &,
	// CssmKey &). That method allocates two subclass-specific 
	// SSKeys and calls this method. This will call down to 
	// generate(const Context &, SSKey &, SSKey &)
	// and optionally to SSKey::generateKeyBlob.
	void generate(const Context &context, 
				  CssmKey &pubKey,
				  SSKey *pubBinKey,
				  CssmKey &privKey,
				  SSKey *privBinKey);

protected:
	// @@@ Subclasses must implement this. It cooks up a key pair.
	virtual void generate(const Context &context,
						  SSKey &pubBinKey,		// valid on successful return
						  SSKey &privBinKey, 	// ditto
						  uint32 &keySize);	// ditto

public:
	void generateSymKey(const Context &context, CssmKey &outCssmKey); 
};
#endif // 0


#endif // _H_SS_CONTEXT
