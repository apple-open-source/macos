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
 * RSA_asymmetric.h - CSPContext for RSA asymmetric encryption
 */

#ifndef _RSA_ASYMMETRIC_H_
#define _RSA_ASYMMETRIC_H_

#include <Security/CSPsession.h>
#include <AppleCSP/AppleCSP.h>
#include <AppleCSP/AppleCSPContext.h>
#include <AppleCSP/AppleCSPSession.h>
#include <AppleCSP/BlockCryptor.h>
#include <openssl/rsa.h>

#define RSA_ASYM_PADDING_DEFAULT		RSA_PKCS1_PADDING

class RSA_CryptContext : public BlockCryptor {
public:
	RSA_CryptContext(AppleCSPSession &session) :
		BlockCryptor(session),
		mRsaKey(NULL),
		mAllocdRsaKey(false),
		mInitFlag(false),
		mPadding(RSA_ASYM_PADDING_DEFAULT) 	{ }
		
	~RSA_CryptContext();
	
	/* called by CSPFullPluginSession */
	void init(const Context &context, bool encoding = true);

	/* called by BlockCryptor */
	void encryptBlock(
		const void		*plainText,			// length implied (one block)
		size_t			plainTextLen,
		void			*cipherText,	
		size_t			&cipherTextLen,		// in/out, throws on overflow
		bool			final);
	void decryptBlock(
		const void		*cipherText,		// length implied (one cipher block)
		void			*plainText,	
		size_t			&plainTextLen,		// in/out, throws on overflow
		bool			final);

	size_t RSA_CryptContext::outputSize(
		bool 			final,				// ignored
		size_t 			inSize = 0); 		// output for given input size

private:
		RSA				*mRsaKey;
		bool			mAllocdRsaKey;
		bool			mInitFlag;			// allows easy reuse
	int					mPadding;			// RSA_NO_PADDING, RSA_PKCS1_PADDING
};	/* RSA_CryptContext */


#endif // _RSA_ASYMMETRIC_H_
