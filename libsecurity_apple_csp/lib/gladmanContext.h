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
// gladmanContext.h - Gladman AES context class
//
#ifndef _H_GLADMAN_CONTEXT
#define _H_GLADMAN_CONTEXT

#include <security_cdsa_plugin/CSPsession.h>
#include "AppleCSP.h"
#include "AppleCSPContext.h"
#include "AppleCSPSession.h"
#include "BlockCryptor.h"
#include <CommonCrypto/CommonCryptor.h>
#include "/usr/local/include/CommonCrypto/CommonCryptorSPI.h"
#include "aesCommon.h"

#define GLADMAN_BLOCK_SIZE_BYTES	DEFAULT_AES_BLOCK_BYTES

/* Symmetric encryption context */
class GAESContext : public BlockCryptor {
public:
	GAESContext(AppleCSPSession &session);
	virtual ~GAESContext();
	
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
	void deleteKey();
	
	/* scheduled key */
    CCCryptorRef	mAesKey;	
	bool				mInitFlag;			// for easy reuse
	
	/* 
	 * Raw key bits saved here and checked on re-init to avoid extra key 
	 * schedule on re-init. We also have to do a new key schedule if 
	 * changing between encrypting and decrypting since the key schedules
	 * differ for the two.
	 */
	uint8				mRawKey[MAX_AES_KEY_BITS / 8];
	uint32				mRawKeySize;
	bool				mWasEncrypting;
};	/* AESContext */

#endif //_H_GLADMAN_CONTEXT
