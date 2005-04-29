/*
 * ascContext.h - glue between BlockCrytpor and ComCryption (a.k.a. Apple
 *				  Secure Compression). 
 * Written by Doug Mitchell 4/4/2001
 */

#ifdef	ASC_CSP_ENABLE

#ifndef _ASC_CONTEXT_H_
#define _ASC_CONTEXT_H_

#include "AppleCSPContext.h"
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacTypes.h>
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
