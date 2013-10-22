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
 * BlockCryptor.h - common context for block-oriented encryption algorithms
 *
 * Created March 5 2001 by dmitch
 */

#ifndef	_BLOCK_CRYPTOR_H_
#define _BLOCK_CRYPTOR_H_

#include "AppleCSPContext.h"

/*
 * Base class for AppleCSPContexts associated with BlockCryptObjects. 
 * The main purpose of this class is to abstract out the very common work
 * of buffering incoming data (per CSSM-style update, ..., final) and 
 * doing single-block ops on the underlying encrypt/decrypt algorithm
 * objects. Standard PKSC5 padding is handled here. All other chaining,
 * padding, IV, et al, logic is handled by subclasses.
 */
class BlockCryptor : public AppleCSPContext
{
public:
	BlockCryptor(
		AppleCSPSession &session) : 
			AppleCSPContext(session), 
			mOpStarted(false),
			mCbcCapable(false),
			mMultiBlockCapable(false),
			mInBuf(NULL),
			mChainBuf(NULL) { }
	virtual ~BlockCryptor();
	
	/* 
	 * Note standard init(const Context &context, bool encoding) is totally 
	 * subclass-specific. 
	 *
	 * These are implemented here using the subclass's {en,de}cryptBlock functions.
	 * Note PKCS5 padding is implemented here if mPkcs5Padding is true. PKCS5
	 * padding can only be accomplished if the result of decrypting 
	 * cipherBlockSize() bytes of ciphertext yields exactly plainBlockSize()
	 * bytes of plaintext. (Sound odd? FEED does not meet that restriction...)
	 */
	void update(
		void 			*inp, 
		size_t 			&inSize, 			// in/out
		void 			*outp, 
		size_t 			&outSize);			// in/out
		
	void final(
		CssmData 		&out);

	/* 
	 * Our implementation of these three query functions are only valid 
	 * for algorithms for which encrypting one block of plaintext always 
	 * yields exactly one block of ciphertext, and vice versa for decrypt.
	 * The block sizes for plaintext and ciphertext do NOT have to be the same. 
	 * Subclasses (e.g. FEED) which do not meet this criterion will have to override.
	 */
 	virtual size_t inputSize(
		size_t 			outSize);			// input for given output size
	virtual size_t outputSize(
		bool 			final = false, 
		size_t 			inSize = 0); 		// output for given input size
	virtual void minimumProgress(
		size_t 			&in, 
		size_t 			&out); 				// minimum progress chunks

protected:
	typedef enum {
		BCM_ECB,			// no chaining
		BCM_CBC				// requires inBlockSize == outBlockSize
	} BC_Mode;
	
	/* accessors (see comments below re: the member variables) */
	bool	pkcs5Padding()		{ return mPkcsPadding; }
	bool	needFinalData()		{ return mNeedFinalData; }
	void	*inBuf()			{ return mInBuf; }
	size_t	inBufSize()			{ return mInBufSize; }
	void	*chainBuf()			{ return mChainBuf; }
	size_t	inBlockSize()		{ return mInBlockSize; }
	size_t	outBlockSize()		{ return mOutBlockSize; }
	BC_Mode	mode()				{ return mMode; }
	bool	opStarted()			{ return mOpStarted; }
	bool	cbcCapable()		{ return mCbcCapable; }
	void	cbcCapable(bool c)	{ mCbcCapable = c; }
	bool	multiBlockCapable()	{ return mMultiBlockCapable; }
	void	multiBlockCapable(bool c)	{ mMultiBlockCapable = c; }
	
	/* 
	 * Reusable setup functions called from subclass's init.
	 * This is the general purpose one....
	 */
	void	setup(
		size_t			blockSizeIn,	// block size of input in bytes
		size_t			blockSizeOut,	// block size of output in bytes
		bool			pkcsPad,		// this class performs PKCS{5,7} padding
		bool			needsFinal,		// needs final update with valid data
		BC_Mode			mode,			// ECB, CBC
		const CssmData	*iv);			// init vector, required for CBC
										//Ê  must be at least blockSizeIn bytes
		
	/*
	 * This one is used by simple, well-behaved algorithms which don't do their own
	 * padding and which rely on us to do everything but one-block-at-a-time
	 * encrypt and decrypt.
	 */
	void setup(
		size_t			blockSize,		// block size of input and output
		const Context 	&context);

	/***
	 *** Routines to be implemented by subclass.
	 ***/
	 
	/*
	virtual void init(const Context &context, bool encoding = true);
	*/

	/* 
	 * encrypt/decrypt exactly one block. Output buffers mallocd by caller.
	 * On encrypt, it may be acceptable for plainTextLen to be less than
	 * one plainBlockSize() if:
	 *   -- final is true, and
	 *   -- the subclass permits this. That is generally only true
	 *      when the subclass implements some padding other than our
	 *      standard PKCS5.
	 *
	 * The subclass throws CSSMERR_CSP_INPUT_LENGTH_ERROR if the above
	 * conditions are not met.  
	 */
	virtual void encryptBlock(
		const void		*plainText,			// length implied (one block)
		size_t			plainTextLen,
		void			*cipherText,	
		size_t			&cipherTextLen,		// in/out, subclass throws on overflow
		bool			final) = 0;
		
	/*
	 * Decrypt one block. Incoming cipherText length is ALWAYS cipherBlockSize(). 
	 */
	virtual void decryptBlock(
		const void		*cipherText,		// length implied (one cipher block)
		size_t			cipherTextLen,
		void			*plainText,	
		size_t			&plainTextLen,		// in/out, subclass throws on overflow
		bool			final) = 0;

private:
	bool				mOpStarted;			// for optional use by subclasses when 
											//   resuing context after encrypt/decrypt 
											//   ops occur
	bool				mCbcCapable;		// when true, algorithm can do its own CBC
	bool				mMultiBlockCapable;	// when true, algorithm can do multi-block ops
	
	/* these are all init'd via setup(), called from subclass-specific init */
	bool				mPkcsPadding;		// PKCS{5,7} padding enabled
	bool				mNeedFinalData;		// subclass needs an update(final) with
											//   valid data; if true we always keep
											//   some data in mInBuf after an update.
											//   Mutually exclusive with mPkcsPadding. 
	uint8 				*mInBuf;			// for buffering input
	size_t				mInBufSize;			// valid bytes in mInBuf
	uint8				*mChainBuf;			// for CBC, decrypting only
	size_t				mInBlockSize;		// block size of input in bytes; also
											//    mallocd size of mInBuf
	size_t				mOutBlockSize;		// block size of output in bytes
	BC_Mode				mMode;				// ECB, CBC
		
};

#endif	/* _BLOCK_CRYPTOR_H_ */
