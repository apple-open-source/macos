/*
 * Copyright (c) 2000-2002,2011,2014 Apple Inc. All Rights Reserved.
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
 * DH_keys.h - Diffie-Hellman key pair support
 */
 
#ifndef	_DH_KEYS_H_
#define _DH_KEYS_H_

#include <AppleCSPContext.h>
#include <AppleCSPSession.h>
#include "AppleCSPKeys.h"
#include <DH_csp.h>
#include <openssl/dh.h>
#include <security_cdsa_utilities/context.h>
#include <security_utilities/debugging.h>
#include <security_asn1/SecNssCoder.h>
#include <Security/osKeyTemplates.h>

#define DH_PUB_KEY_FORMAT		CSSM_KEYBLOB_RAW_FORMAT_PKCS3
#define DH_PRIV_KEY_FORMAT		CSSM_KEYBLOB_RAW_FORMAT_PKCS3

#define	DH_MIN_KEY_SIZE			512			/* FIXME */
#define DH_MAX_KEY_SIZE			2048

#define cspDhDebug(args...)		secdebug("dhDebug", ## args)

/*
 * Diffie-Hellman version of a BinaryKey.
 */
class DHBinaryKey : public BinaryKey {
public:
	DHBinaryKey(DH *dhKey = NULL);
	~DHBinaryKey();
	void generateKeyBlob(
		Allocator 		&allocator,
		CssmData			&blob,
		CSSM_KEYBLOB_FORMAT	&format,
		AppleCSPSession		&session,
		const CssmKey		*paramKey,		/* optional, unused here */
		CSSM_KEYATTR_FLAGS 	&attrFlags);	/* IN/OUT */
		
	/*
	 * This may contain a fully-capable private key, or a public
	 * key with as little as the pub_key field set. 
	 */
	DH						*mDhKey;
};

class DHKeyPairGenContext : 
	public AppleCSPContext, private AppleKeyPairGenContext  {
public:
	DHKeyPairGenContext(
		AppleCSPSession &session,
		const Context &) :
			AppleCSPContext(session),
			mGenAttrs(NULL) {}

	~DHKeyPairGenContext() { freeGenAttrs(); }
	
	// no init functionality, but we need to implement it
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

	void dhGenParams(
		uint32			keySizeInBits,
		unsigned		g,					// probably should be BIGNUM
		int				privValueLength, 	// optional
		NSS_DHParameter	&algParams,
		SecNssCoder		&coder);			// for temp contents of algParams
	
private:
	/* gross hack to store attributes "returned" from GenParams */
	Context::Attr		*mGenAttrs;
	void				freeGenAttrs();
};	/* DHKeyPairGenContext */

/*
 * CSPKeyInfoProvider for Diffie-Hellman keys
 */
class DHKeyInfoProvider : public CSPKeyInfoProvider 
{
private:
	DHKeyInfoProvider(
		const CssmKey		&cssmKey,
		AppleCSPSession		&session);
public:
	static CSPKeyInfoProvider *provider(
		const CssmKey 		&cssmKey,
		AppleCSPSession		&session);

	~DHKeyInfoProvider() { }
	void CssmKeyToBinary(
		CssmKey				*paramKey,	// optional, ignored here
		CSSM_KEYATTR_FLAGS	&attrFlags,	// IN/OUT
		BinaryKey			**binKey);	// RETURNED
	void QueryKeySizeInBits(
		CSSM_KEY_SIZE		&keySize);	// RETURNED
	bool getHashableBlob(
		Allocator 		&allocator,
		CssmData			&hashBlob);
};

#endif	/* _DH_KEYS_H_ */
