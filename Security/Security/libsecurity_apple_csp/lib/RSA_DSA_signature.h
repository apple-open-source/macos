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
 * RSA_DSA_signature.h - openssl-based signature classes.  
 */

#ifndef	_RSA_DSA_SIGNATURE_H_
#define _RSA_DSA_SIGNATURE_H_

#include <openssl/rsa.h>
#include <openssl/dsa.h>
#include <RawSigner.h>
#include <AppleCSPSession.h>

#define RSA_SIG_PADDING_DEFAULT		RSA_PKCS1_PADDING

class RSASigner : public RawSigner {
public:
	RSASigner(
		Allocator	&alloc,
		AppleCSPSession &session,
		CSSM_ALGORITHMS	digestAlg) : 
			RawSigner(alloc, digestAlg),
			mRsaKey(NULL),
			mWeMallocdRsaKey(false),
			mSession(session),
			mPadding(RSA_SIG_PADDING_DEFAULT) { }
			
	~RSASigner();
	
	/* reusable init */
	void signerInit(
		const Context 	&context,
		bool			isSigning);
	

	/* sign */
	void sign(
		const void 		*data, 
		size_t 			dataLen,
		void			*sig,	
		size_t			*sigLen);	/* IN/OUT */
		
	/* verify */
	void verify(
		const void 	*data, 
		size_t 			dataLen,
		const void		*sig,			
		size_t			sigLen);	
		
	/* works for both, but only used for signing */
	size_t maxSigSize();

private:

	/* 
	 * obtain key from context, validate, convert to RSA key
	 */
	void keyFromContext(
		const Context 	&context);

	RSA					*mRsaKey;
	bool				mWeMallocdRsaKey;
	AppleCSPSession		&mSession;
	int					mPadding;		// RSA_NO_PADDING, RSA_PKCS1_PADDING
};

class DSASigner : public RawSigner {
public:
	DSASigner(
		Allocator	&alloc,
		AppleCSPSession &session,
		CSSM_ALGORITHMS	digestAlg) : 
			RawSigner(alloc, digestAlg),
			mDsaKey(NULL),
			mWeMallocdDsaKey(false),
			mSession(session) { }
			
	~DSASigner();
	
	/* reusable init */
	void signerInit(
		const Context 	&context,
		bool			isSigning);
	

	/* sign */
	void sign(
		const void 		*data, 
		size_t 			dataLen,
		void			*sig,	
		size_t			*sigLen);	/* IN/OUT */
		
	/* verify */
	void verify(
		const void 	*data, 
		size_t 			dataLen,
		const void		*sig,			
		size_t			sigLen);	
		
	/* works for both, but only used for signing */
	size_t maxSigSize();

private:

	/* 
	 * obtain key from context, validate, convert to DSA key
	 */
	void keyFromContext(
		const Context 	&context);

	DSA					*mDsaKey;
	bool				mWeMallocdDsaKey;
	AppleCSPSession		&mSession;
};


#endif	/* _RSA_DSA_SIGNATURE_H_ */
