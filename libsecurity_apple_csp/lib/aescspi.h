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
// aescspi.h - AES context class
//
#ifndef _H_AESCSPI
#define _H_AESCSPI

#include <security_cdsa_plugin/CSPsession.h>
#include "AppleCSP.h"
#include "AppleCSPContext.h"
#include "AppleCSPSession.h"
#include "BlockCryptor.h"
#include "rijndaelApi.h"

/* Symmetric encryption context */
class AESContext : public BlockCryptor {
public:
	AESContext(AppleCSPSession &session) :
		BlockCryptor(session),
		mAesKey(NULL),
		mBlockSize(0),
		mInitFlag(false),
		mRawKeySize(0)	{ }
	~AESContext();
	
	// called by CSPFullPluginSession
	void init(const Context &context, bool encoding = true);

	// As an optimization, we allow reuse of a modified context. The main thing
	// we avoid is a redundant key scheduling. We save the current raw keys bits
	// in mRawKey and compare on re-init.
	bool changed(const Context &context)	 { return true; }

	// called by BlockCryptor
	void encryptBlock(
		const void		*plainText,			// length implied (one block)
		size_t			plainTextLen,
		void			*cipherText,	
		size_t			&cipherTextLen,		// in/out, throws on overflow
		bool			final);
	void decryptBlock(
		const void		*cipherText,		// length implied (one cipher block)
		size_t			cipherTextLen,	
		void			*plainText,	
		size_t			&plainTextLen,		// in/out, throws on overflow
		bool			final);
	
private:
	void aesError(
		int artn, 
		const char *errStr);
	void deleteKey();
	
	keyInstance			*mAesKey;			// keyInstance or keyInstance128
	unsigned			mBlockSize;			// in BYTES
	bool				mInitFlag;			// for easy reuse
	aesCryptFcn			mEncryptFcn;
	aesCryptFcn			mDecryptFcn;	
	
	/* raw key bits saved here and checked on re-init to avoid extra key schedule */
	uint8				mRawKey[MAX_AES_KEY_BITS / 8];
	uint32				mRawKeySize;
};	/* AESContext */

// symmetric key generation context
class AESKeyGenContext : public AppleCSPContext, private AppleSymmKeyGenContext {
public:
	AESKeyGenContext(
		AppleCSPSession &session) :
			AppleCSPContext(session),
			AppleSymmKeyGenContext(
				MIN_AES_KEY_BITS,
				MAX_AES_KEY_BITS,
				true) { }
	
	void init(const Context &context, bool encoding = true) { }
			
	void generate(
		const Context 	&context, 
		CssmKey 		&symKey, 
		CssmKey 		&dummyKey);

};

#endif //_H_AESCSPI
