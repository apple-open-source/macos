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


/*
 * RSA_DSA_keys.h - key pair support for RSA/DSA
 */
 
#ifndef	_RSA_DSA_KEYS_H_
#define _RSA_DSA_KEYS_H_

#include <AppleCSP/AppleCSPContext.h>
#include <AppleCSP/AppleCSPSession.h>
#include <RSA_DSA/RSA_DSA_csp.h>
#include "AppleCSPKeys.h"
#include <opensslUtils/osKeyTemplates.h>
#include <openssl/rsa.h>
#include <openssl/dsa.h>
#include <Security/context.h>
#include <SecurityNssAsn1/SecNssCoder.h>

#define RSA_PUB_KEY_FORMAT		CSSM_KEYBLOB_RAW_FORMAT_PKCS1
#define RSA_PRIV_KEY_FORMAT		CSSM_KEYBLOB_RAW_FORMAT_PKCS8

#define DSA_PUB_KEY_FORMAT		CSSM_KEYBLOB_RAW_FORMAT_X509
#define DSA_PRIV_KEY_FORMAT		CSSM_KEYBLOB_RAW_FORMAT_FIPS186

#define	DSA_MIN_KEY_SIZE		512
#define DSA_MAX_KEY_SIZE		1024
#define DSA_KEY_BITS_MASK		(64 - 1)	/* these bits must be zero */
											/* i.e., aligned to 64 bits */
/*
 * RSA version of a BinaryKey.
 */
class RSABinaryKey : public BinaryKey {
public:
	RSABinaryKey(RSA *rsaKey = NULL);
	~RSABinaryKey();
	void generateKeyBlob(
		CssmAllocator 		&allocator,
		CssmData			&blob,
		CSSM_KEYBLOB_FORMAT	&format,
		AppleCSPSession		&session,
		const CssmKey		*paramKey,		/* optional, unused here */
		CSSM_KEYATTR_FLAGS	&attrFlags);	/* IN/OUT */

	RSA						*mRsaKey;
};

class RSAKeyPairGenContext : 
	public AppleCSPContext, private AppleKeyPairGenContext  {
public:
	RSAKeyPairGenContext(
		AppleCSPSession &session,
		const Context &) :
			AppleCSPContext(session) {}

	~RSAKeyPairGenContext() { }
	
	/* no init functionality, but we need to implement it */
	void init(
		const Context &, 
		bool) { }
		
	// this one is specified in, and called from, CSPFullPluginSession
	void generate(
		const Context 	&context, 
		CssmKey 		&pubKey, 
		CssmKey 		&privKey);
		
	// this one is specified in, and called from, AppleKeyPairGenContext
	void generate(
		const Context 	&context,
		BinaryKey		&pubBinKey,	
		BinaryKey		&privBinKey,
		uint32			&keySize);
	
};	/* KeyPairGenContext */

/*
 * CSPKeyInfoProvider for RSA keys
 */
class RSAKeyInfoProvider : public CSPKeyInfoProvider 
{
private:
	RSAKeyInfoProvider(
		const CssmKey		&cssmKey,
		AppleCSPSession		&session);
public:
	static CSPKeyInfoProvider *provider(
		const CssmKey 		&cssmKey,
		AppleCSPSession		&session);

	~RSAKeyInfoProvider() { }
	void CssmKeyToBinary(
		CssmKey				*paramKey,	// optional
		CSSM_KEYATTR_FLAGS	&attrFlags,	// IN/OUT
		BinaryKey			**binKey);	// RETURNED
	void QueryKeySizeInBits(
		CSSM_KEY_SIZE		&keySize);	// RETURNED
	bool getHashableBlob(
		CssmAllocator 		&allocator,
		CssmData			&hashBlob);
};

/*
 * DSA version of a BinaryKey.
 */
class DSABinaryKey : public BinaryKey {
public:
	DSABinaryKey(DSA *dsaKey = NULL);
	~DSABinaryKey();
	void generateKeyBlob(
		CssmAllocator 		&allocator,
		CssmData			&blob,
		CSSM_KEYBLOB_FORMAT	&format,
		AppleCSPSession		&session,
		const CssmKey		*paramKey,		/* optional */
		CSSM_KEYATTR_FLAGS	&attrFlags);	/* IN/OUT */

	DSA						*mDsaKey;
};

class DSAKeyPairGenContext : 
	public AppleCSPContext, private AppleKeyPairGenContext  {
public:
	DSAKeyPairGenContext(
		AppleCSPSession &session,
		const Context &) :
			AppleCSPContext(session), mGenAttrs(NULL) {}

	~DSAKeyPairGenContext() { freeGenAttrs(); }
	
	/* no init functionality, but we need to implement it */
	void init(
		const Context &, 
		bool) { }
		
	// this one is specified in, and called from, CSPFullPluginSession
	void generate(
		const Context 	&context, 
		CssmKey 		&pubKey, 
		CssmKey 		&privKey);
		
	// this one is specified in, and called from, AppleKeyPairGenContext
	void generate(
		const Context 	&context,
		BinaryKey		&pubBinKey,	
		BinaryKey		&privBinKey,
		uint32			&keySize);

	// specified in, and called from, CSPFullPluginSession - generate parameters
	void generate(
		const Context 	&context, 
		uint32 			bitSize,
		CssmData 		&params,
		uint32 			&attrCount, 
		Context::Attr * &attrs);
	
	/*
	 * Necessary to handle and deflect "context changed" notification which occurs
	 * after the strange return from "generate parameters", when the plugin adds
	 * the "returned" values to the Context.
	 */
	bool changed(const Context &context) { return true; }

	void dsaGenParams(
		uint32			keySizeInBits,
		const void		*inSeed,			// optional
		unsigned		inSeedLen,
		NSS_DSAAlgParams &algParams,
		SecNssCoder		&coder);
	
private:
	/* gross hack to store attributes "returned" from GenParams */
	Context::Attr		*mGenAttrs;
	void				freeGenAttrs();
};	/* KeyPairGenContext */

/*
 * CSPKeyInfoProvider for DSA keys
 */
class DSAKeyInfoProvider : public CSPKeyInfoProvider 
{
private:
	DSAKeyInfoProvider(
		const CssmKey		&cssmKey,
		AppleCSPSession		&session);
public:
	static CSPKeyInfoProvider *provider(
		const CssmKey 		&cssmKey,
		AppleCSPSession		&session);
		
	~DSAKeyInfoProvider() { }
	void CssmKeyToBinary(
		CssmKey				*paramKey,	// optional
		CSSM_KEYATTR_FLAGS	&attrFlags,	// IN/OUT
		BinaryKey			**binKey);	// RETURNED
	void QueryKeySizeInBits(
		CSSM_KEY_SIZE		&keySize);	// RETURNED
	bool getHashableBlob(
		CssmAllocator 	&allocator,
		CssmData		&hashBlob);
};

#endif	/* _RSA_DSA_KEYS_H_ */
