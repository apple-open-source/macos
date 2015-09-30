/*
 * Copyright (c) 2001,2011,2013-2014 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */


#ifdef	ASC_CSP_ENABLE

#ifndef _ASC_CONTEXT_H_
#define _ASC_CONTEXT_H_

#include "AppleCSPContext.h"
#include <security_comcryption/comcryption.h>

/* symmetric encrypt/decrypt context */
class ASCContext : public AppleCSPContext {
public:
	ASCContext(AppleCSPSession &session) :
		AppleCSPContext(session),
		mCcObj(NULL)	{ }
	~ASCContext();
	
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
	comcryptObj			mCcObj;
	
	/*
	 * For first implementation, we have to cope with the fact that the final
	 * decrypt call down to the comcryption engine requires *some* ciphertext.
	 * On decrypt, we'll just save one byte on each update in preparation for
	 * the final call. Hopefull we'll have time to fix deComcryptData() so this
	 * is unneccesary.
	 */
	unsigned char		mDecryptBuf;
	bool				mDecryptBufValid;
	
};	/* RC4Context */

#endif 	/*_ASC_CONTEXT_H_ */
#endif	/* ASC_CSP_ENABLE */
