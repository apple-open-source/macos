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
 * FEEAsymmetricContext.h - CSPContexts for FEE asymmetric encryption
 *
 * Created March 8 2001 by dmitch.
 */

#ifdef	CRYPTKIT_CSP_ENABLE

#ifndef _FEE_ASYMMETRIC_CONTEXT_H_
#define _FEE_ASYMMETRIC_CONTEXT_H_

#include <Security/CSPsession.h>
#include "AppleCSP.h"
#include "AppleCSPContext.h"
#include "AppleCSPSession.h"
#include "BlockCryptor.h"
#include <CryptKit/feeFEED.h>
#include <CryptKit/feeFEEDExp.h>

namespace CryptKit {

class FEEDContext : public BlockCryptor {
public:
	FEEDContext(AppleCSPSession &session) :
		BlockCryptor(session),
		mFeeFeed(NULL),
		mPrivKey(NULL),
		mPubKey(NULL),
		mInitFlag(false) 	{ }
	~FEEDContext();
	
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
	
	/*
 	 * Additional query size support, necessary because we don't conform to 
	 * BlockCryptor's standard one-to-one block scheme
	 */
 	size_t inputSize(
		size_t 			outSize);			// input for given output size
	size_t outputSize(
		bool 			final = false, 
		size_t 			inSize = 0); 		// output for given input size
	void minimumProgress(
		size_t 			&in, 
		size_t 			&out); 				// minimum progress chunks


private:
		feeFEED			mFeeFeed;
		feePubKey		mPrivKey;
		bool			mAllocdPrivKey;
		feePubKey		mPubKey;
		bool			mAllocdPubKey;
		bool			mInitFlag;			// allows easy reuse
};	/* FEEDContext */


class FEEDExpContext : public BlockCryptor {
public:
	FEEDExpContext(AppleCSPSession &session) :
		BlockCryptor(session),
		mFeeFeedExp(NULL),
		mFeeKey(NULL),
		mInitFlag(false) 	{ }

	~FEEDExpContext();
	
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
	
private:
		feeFEEDExp		mFeeFeedExp;
		feePubKey		mFeeKey;
		bool			mAllocdFeeKey;
		bool			mInitFlag;			// allows easy reuse
};	/* FEEDExpContext */

} /* namespace CryptKit */

#endif 	/* _FEE_ASYMMETRIC_CONTEXT_H_ */
#endif	/* CRYPTKIT_CSP_ENABLE */
