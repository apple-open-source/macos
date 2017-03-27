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
 * rc2Context.h - glue between BlockCrytpor and ssleay RC2 implementation
 */
#ifndef _RC2_CONTEXT_H_
#define _RC2_CONTEXT_H_

#include <BlockCryptor.h>
#include <openssl/rc2_legacy.h>

/* RC2 Symmetric encryption context */
class RC2Context : public BlockCryptor {
public:
	RC2Context(AppleCSPSession &session) :
		BlockCryptor(session)	{ }
	~RC2Context();
	
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
	RC2_KEY				rc2Key;
	
};	/* RC2Context */

#endif //_RC2_CONTEXT_H_
