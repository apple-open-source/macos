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
 * rc4Context.cpp - glue between BlockCrytpor and ssleay RC4 implementation
 * Written by Doug Mitchell 04/03/2001
 */
 
#include <openssl/rc4.h>
#include "rc4Context.h"

RC4Context::~RC4Context()
{
	memset(&rc4Key, 0, sizeof(RC4_KEY));
}
	
/* 
 * Standard CSPContext init, called from CSPFullPluginSession::init().
 * Reusable, e.g., query followed by en/decrypt.
 */
void RC4Context::init( 
	const Context &context, 
	bool encrypting)
{
	UInt32 		keyLen;
	UInt8 		*keyData 	= NULL;
	
	/* obtain key from context */
	symmetricKeyBits(context, CSSM_ALGID_RC4, 
		encrypting ? CSSM_KEYUSE_ENCRYPT : CSSM_KEYUSE_DECRYPT,
		keyData, keyLen);
	if((keyLen < RC4_MIN_KEY_SIZE_BYTES) || (keyLen > RC4_MAX_KEY_SIZE_BYTES)) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_KEY);
	}
	
	/* All other context attributes ignored */
	/* init the low-level state */
	RC4_set_key(&rc4Key, keyLen, keyData);
}	

/*
 * All of these functions are called by CSPFullPluginSession.
 */
void RC4Context::update(
	void 			*inp, 
	size_t 			&inSize, 			// in/out
	void 			*outp, 
	size_t 			&outSize)			// in/out
{
	RC4(&rc4Key, inSize, (unsigned char *)inp, (unsigned char *)outp);
	outSize = inSize;
}

/* remainding functions are trivial for any stream cipher */
void RC4Context::final(
	CssmData 		&out)	
{
	out.length(0);
}

size_t RC4Context::inputSize(
	size_t 			outSize)			// input for given output size
{
	return outSize;
}

size_t RC4Context::outputSize(
	bool 			final /*= false*/, 
	size_t 			inSize /*= 0*/) 	// output for given input size
{
	return inSize;
}

void RC4Context::minimumProgress(
	size_t 			&in, 
	size_t 			&out) 				// minimum progress chunks
{
	in  = 1;
	out = 1;
}
