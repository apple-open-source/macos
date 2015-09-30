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
 * NullCryptor.h - null symmetric encryptor for measurement only
 */
#ifndef _NULL_CRYPTOR_H_
#define _NULL_CRYPTOR_H_

/*
 * DO NOT DEFINE THIS SYMBOL TRUE FOR CODE CHECKED IN TO CVS
 */
#define NULL_CRYPT_ENABLE			0

#if		NULL_CRYPT_ENABLE

#include <security_cdsa_plugin/CSPsession.h>
#include "AppleCSP.h"
#include "AppleCSPContext.h"
#include "AppleCSPSession.h"
#include "BlockCryptor.h"

#define NULL_CRYPT_BLOCK_SIZE		16

class NullCryptor : public BlockCryptor {
public:
	NullCryptor(AppleCSPSession &session) :
		BlockCryptor(session),
		mInitFlag(false)	{ }
	~NullCryptor() { }
	
	// called by CSPFullPluginSession
	void init(const Context &context, bool encoding = true)
	{
		if(mInitFlag && !opStarted()) {
			return;
		}
		/* Just have BlockCryptor do its setup */
		setup(NULL_CRYPT_BLOCK_SIZE, context);
		mInitFlag = true;
	}

	// called by BlockCryptor
	void encryptBlock(
		const void		*plainText,			// length implied (one block)
		size_t			plainTextLen,
		void			*cipherText,	
		size_t			&cipherTextLen,		// in/out, throws on overflow
		bool			final)
	{
		memmove(cipherText, plainText, NULL_CRYPT_BLOCK_SIZE);
		cipherTextLen = NULL_CRYPT_BLOCK_SIZE;
	}
	
	void decryptBlock(
		const void		*cipherText,		// length implied (one cipher block)
		size_t			cipherTextLen,
		void			*plainText,	
		size_t			&plainTextLen,		// in/out, throws on overflow
		bool			final)
	{
		memmove(plainText, cipherText, NULL_CRYPT_BLOCK_SIZE);
		plainTextLen = NULL_CRYPT_BLOCK_SIZE;
	}
		
private:
	bool				mInitFlag;			// for easy reuse

};	/* NullCryptor */

#endif	/* NULL_CRYPT_ENABLE */

#endif //_NULL_CRYPTOR_H_
