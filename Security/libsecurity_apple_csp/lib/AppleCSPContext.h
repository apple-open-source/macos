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
// AppleCSPContext.h - CSP-wide contexts 
//
#ifndef _H_APPLE_CSP_CONTEXT
#define _H_APPLE_CSP_CONTEXT

#include <security_cdsa_plugin/CSPsession.h>
#include "BinaryKey.h"

//
// Parent class for all CSPContexts implemented in this CSP.
// Currently the only thing we add is a reference to our
// creator's session.
//
class AppleCSPSession;

class AppleCSPContext : public CSPFullPluginSession::CSPContext
{
public:
	AppleCSPContext(AppleCSPSession &session)
		: mSession(session) {}
    
    ~AppleCSPContext();
	
	/* 
	 * get symmetric key bits - context.key can be either ref or raw.
	 * A convenience routine typically used by symmetric contexts' 
	 * init() routines. 
	 */
	static void symmetricKeyBits(
		const Context 	&context,
		AppleCSPSession &session,
		CSSM_ALGORITHMS	requiredAlg,	// throws if this doesn't match key alg
		CSSM_KEYUSE 	intendedUse,	// throws if key usage doesn't match this
		uint8			*&keyBits,		// RETURNED (not mallocd or copied)
		CSSM_SIZE		&keyLen);		// RETURNED
		
protected:	
	AppleCSPSession	&session() { return mSession; }

private:
	AppleCSPSession	&mSession;
};

//
// Context for CSSM_ALGID_APPLE_YARROW.
//
class YarrowContext : public AppleCSPContext
{
public:
	YarrowContext(AppleCSPSession &session);
	virtual ~YarrowContext();
	virtual void init(const Context &context, bool encoding = true);
	void final(CssmData &out);
	size_t outputSize(bool final, size_t inSize) { return outSize; }

private:
	uint32	outSize;
};

//
// Classes which inherit from AppleCSPContext and which also perform
// key pair generation inherit from this class as well.
//
class AppleKeyPairGenContext  {
public:
	virtual ~AppleKeyPairGenContext();
	
	//
	// Subclass implements generate(const Context &, CssmKey &,
	// CssmKey &). That method (called from CSPFullPluginSession)
	// allocates two subclass-specific BinaryKeys and calls this
	// method. This will eventually call down to generate(const Context &,
	// BinaryKey &, BinaryKey &) and optionally to 
	// BinaryKey::generateKeyBlob.
	//
	void generate(
		const Context 	&context, 
		AppleCSPSession	&session,		// for ref keys
		CssmKey 		&pubKey, 
		BinaryKey 		*pubBinKey,
		CssmKey 		&privKey,
		BinaryKey		*privBinKey);

protected:
	// Subclasses must implement this. It cooks up a key pair.
	virtual void generate(
		const Context 	&context,
		BinaryKey		&pubBinKey,		// valid on successful return
		BinaryKey		&privBinKey, 	// ditto
		uint32 			&keySize) = 0;	// ditto
};

//
// Classes which inherit from AppleCSPContext and which also perform
// symmetric key generation inherit from this class as well.
//
class AppleSymmKeyGenContext  {
public:
	//
	// Subclass implements generate(const Context &, CssmKey &, 
	// CssmKey &). Note that the second CssmKey is a dummy 
	// argument. That method merely calls generateSymKey, allowing us 
	// to get to the associated AppleCSPSession if we need to 
	// store reference keys. We take care of all attribute and 
	// usage validation and of header formatting. Parameters for
	// validation typlically specified in constructor via an
	// algorithm factory. 
	//
	AppleSymmKeyGenContext(
		uint32			minSize,	// in bits
		uint32			maxSize,	// ditto
		bool			byteSized)	// true --> key size must
									//   be multiple of 8 bits
		:	minSizeInBits(minSize),
			maxSizeInBits(maxSize),
			mustBeByteSized(byteSized)  {}
			
	void generateSymKey(
		const Context 	&context, 
		AppleCSPSession	&session,		// for ref keys
		CssmKey 		&cssmKey);		// RETURNED 

private:
	uint32			minSizeInBits;
	uint32			maxSizeInBits;
	bool			mustBeByteSized;

};

/*
 * Generic symmetric key generation context, for algorithms whose
 * requirements can be expressed in min/max key size and 
 * mustBeByteSized. Such algorithms just need create one of these
 * from an algorithm factory.
 */
class AppleSymmKeyGenerator : public AppleCSPContext, private AppleSymmKeyGenContext {
public:
	AppleSymmKeyGenerator(
		AppleCSPSession &session,
		uint32			minSize,		// in bits
		uint32			maxSize,		// ditto
		bool			byteSized) :	// true --> key size must
										//   be multiple of 8 bits
			AppleCSPContext(session),
			AppleSymmKeyGenContext(minSize, maxSize, byteSized) { }
	
	void init(const Context &context, bool encoding = true) { }
			
	/* this just passes the request up to AppleSymmKeyGenContext */
	void generate(
		const Context 	&context, 
		CssmKey 		&symKey, 
		CssmKey 		&dummyKey) {
			AppleSymmKeyGenContext::generateSymKey(
					context, 
					session(),
					symKey);
		}

};

#endif	/* _H_APPLE_CSP_CONTEXT */
