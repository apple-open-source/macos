/*
 * Copyright (c) 2000-2001,2011,2013-2014 Apple Inc. All Rights Reserved.
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
 * castContext.h - glue between BlockCrytpor and ssleay CAST-128 (CAST5)
 *				 implementation
 *
 *
 * Here's what RFC 2144 has to say about CAST128 and CAST5 nomenclature:
 *
 *    In order to avoid confusion when variable keysize operation is 
 *    used, the name CAST-128 is to be considered synonymous with the 
 *    name CAST5; this allows a keysize to be appended without ambiguity.  
 *    Thus, for example, CAST-128 with a 40-bit key is to be referred to 
 *    as CAST5-40; where a 128-bit key is explicitly intended, the 
 *    name CAST5-128 should be used. 
 *
 * This module implements a variable key length, from 40 bits to 128 bits,
 * and can thus be said to implement both CAST-128 and CAST5.
 */
 
#ifndef _CAST_CONTEXT_H_
#define _CAST_CONTEXT_H_

#include "AppleCSPContext.h"
#include "BlockCryptor.h"
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonCryptorSPI.h>

class CastContext : public BlockCryptor {
public:
	CastContext(AppleCSPSession &session);
	virtual ~CastContext();
	
	// called by CSPFullPluginSession
	void init(const Context &context, bool encoding = true);

	// As an optimization, we allow reuse of a modified context. 
	// The main thing we avoid is a redundant key scheduling. We 
	// save the current raw keys bits in mRawKey and compare on 
	// re-init.
	bool changed(const Context &context)	 { return true; }

	// called by BlockCryptor
	void encryptBlock(
		const void		*plainText,		// length implied (one block)
		size_t			plainTextLen,
		void			*cipherText,	
		size_t			&cipherTextLen,	// in/out, throws on overflow
		bool			final);
	void decryptBlock(
		const void		*cipherText,	// length implied (one cipher block)
		size_t			cipherTextLen,
		void			*plainText,	
		size_t			&plainTextLen,	// in/out, throws on overflow
		bool			final);
	
private:
	void deleteKey();

	/* scheduled key */
    CCCryptorRef	mCastKey;		

	bool				mInitFlag;			// for easy reuse

	/* 
	 * Raw key bits saved here and checked on re-init to avoid 
	 * extra key schedule 
	 */
	uint8				mRawKey[kCCKeySizeMaxCAST];
	uint32				mRawKeySize;

	
};	/* CastContext */

#endif //_CAST_CONTEXT_H_
