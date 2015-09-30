/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
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
 * FEEKeys.h - FEE-related asymmetric key pair classes. 
 *
 */

#ifdef	CRYPTKIT_CSP_ENABLE

#ifndef	_FEE_KEYS_H_
#define _FEE_KEYS_H_

#include "AppleCSPContext.h"
#include "AppleCSPSession.h"
#include "AppleCSPKeys.h"
#include "cryptkitcsp.h"
#include <security_cryptkit/feeTypes.h>

namespace CryptKit {

/*
 * FEE/ECDSA version of a BinaryKey.
 */
class FEEBinaryKey : public BinaryKey {
public:
	FEEBinaryKey(feePubKey feeKey = NULL);
	~FEEBinaryKey();
	void generateKeyBlob(
		Allocator 		&allocator,
		CssmData			&blob,
		CSSM_KEYBLOB_FORMAT	&format,
		AppleCSPSession		&session,
		const CssmKey		*paramKey,		/* optional, unused here */
		CSSM_KEYATTR_FLAGS 	&attrFlags);	/* IN/OUT */
		
	feePubKey				feeKey() { return mFeeKey; }
private:
	feePubKey				mFeeKey;
};

class FEEKeyPairGenContext : 
	public AppleCSPContext, private AppleKeyPairGenContext  {
public:
	FEEKeyPairGenContext(
		AppleCSPSession &session,
		const Context &) :
			AppleCSPContext(session) {}

	~FEEKeyPairGenContext() { }
	
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
 * CSPKeyInfoProvider for FEE and ECDSA keys
 */
class FEEKeyInfoProvider : public CSPKeyInfoProvider 
{
private:
	FEEKeyInfoProvider(
		const CssmKey		&cssmKey,
		AppleCSPSession		&session);
public:
	static CSPKeyInfoProvider *provider(
		const CssmKey 		&cssmKey,
		AppleCSPSession		&session);
		
	~FEEKeyInfoProvider() { }
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

} /* namespace CryptKit */

#endif	/* _FEE_KEYS_H_ */
#endif	/* CRYPTKIT_CSP_ENABLE */
