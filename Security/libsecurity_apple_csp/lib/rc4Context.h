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
 * rc4Context.h - glue between BlockCrytpor and ssleay RC4 implementation
 * Written by Doug Mitchell 4/3/2001
 */
#ifndef _RC4_CONTEXT_H_
#define _RC4_CONTEXT_H_

#include "AppleCSPContext.h"
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonCryptorSPI.h>

class RC4Context : public AppleCSPContext {
public:
	RC4Context(AppleCSPSession &session) :
		AppleCSPContext(session)	{ }
	virtual ~RC4Context();
	
	// called by CSPFullPluginSession
	void init(
		const Context 	&context, 
		bool encoding = true);
	void update(
		void 			*inp, 
		size_t 			&inSize, 			// in/out
		void 			*outp, 
		size_t 			&outSize);			// in/out
	void final(
		CssmData 		&out);

 	size_t inputSize(
		size_t 			outSize);			// input for given output size
	size_t outputSize(
		bool 			final = false, 
		size_t 			inSize = 0); 		// output for given input size
	void minimumProgress(
		size_t 			&in, 
		size_t 			&out); 				// minimum progress chunks
	
private:
    CCCryptorRef    rc4Key;
	
};	/* RC4Context */

#endif //_RC4_CONTEXT_H_
