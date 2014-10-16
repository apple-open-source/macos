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
 * desContext.h - glue between BlockCrytpor and DES/3DES implementations
 */
#ifndef _DES_CONTEXT_H_
#define _DES_CONTEXT_H_

#include <security_cdsa_plugin/CSPsession.h>
#include "AppleCSP.h"
#include "AppleCSPContext.h"
#include "AppleCSPSession.h"
#include "BlockCryptor.h"
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonCryptorSPI.h>

#define DES_KEY_SIZE_BITS_EXTERNAL		(kCCKeySizeDES * 8)
#define DES_BLOCK_SIZE_BYTES			kCCBlockSizeDES

/* DES Symmetric encryption context */
class DESContext : public BlockCryptor {
public:
	DESContext(AppleCSPSession &session);
	virtual ~DESContext();
	
	// called by CSPFullPluginSession
	void init(const Context &context, bool encoding = true);

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
        CCCryptorRef	DesInst;	
};	/* DESContext */

/* Triple-DES (EDE, 24 byte key) Symmetric encryption context */

#define DES3_KEY_SIZE_BYTES		(3 * (DES_KEY_SIZE_BITS_EXTERNAL / 8))
#define DES3_BLOCK_SIZE_BYTES	kCCBlockSize3DES

class DES3Context : public BlockCryptor {
public:
	DES3Context(AppleCSPSession &session);
	~DES3Context();
	
	// called by CSPFullPluginSession
	void init(const Context &context, bool encoding = true);

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
    CCCryptorRef	DesInst;		
};	/* DES3Context */

#endif //_DES_CONTEXT_H_
