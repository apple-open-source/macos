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
 * gladmanContext.cpp - glue between BlockCryptor and Gladman AES implementation
 * Written by Doug Mitchell 12/12/2001
 */
 
#include "gladmanContext.h"
#include "cspdebugging.h"

/* 
 * Global singleton to perform one-time-only init of AES tables.
 */
class GladmanInit
{
public:
	GladmanInit() :  mTablesGenerated(false) { }
	void genTables();
private:
	bool mTablesGenerated;
	Mutex mLock;
};

void GladmanInit::genTables()
{
	StLock<Mutex> _(mLock);
	if(mTablesGenerated) {
		return;
	}
	
	/* allocate the tables */
	CssmAllocator &alloc = CssmAllocator::standard(CssmAllocator::sensitive);
	pow_tab = (u1byte *)alloc.malloc(POW_TAB_SIZE * sizeof(u1byte));
	log_tab = (u1byte *)alloc.malloc(LOG_TAB_SIZE * sizeof(u1byte));
	sbx_tab = (u1byte *)alloc.malloc(SBX_TAB_SIZE * sizeof(u1byte));
	isb_tab = (u1byte *)alloc.malloc(ISB_TAB_SIZE * sizeof(u1byte));
	rco_tab = (u4byte *)alloc.malloc(RCO_TAB_SIZE * sizeof(u4byte));
	ft_tab  = (u4byte (*)[FT_TAB_SIZE_LS])alloc.malloc(
		FT_TAB_SIZE_LS * FT_TAB_SIZE_MS * sizeof(u4byte));
	it_tab  = (u4byte (*)[IT_TAB_SIZE_LS])alloc.malloc(
		IT_TAB_SIZE_LS * IT_TAB_SIZE_MS * sizeof(u4byte));
	#ifdef  LARGE_TABLES
	fl_tab  = (u4byte (*)[FL_TAB_SIZE_LS])alloc.malloc(
		FL_TAB_SIZE_LS * FL_TAB_SIZE_MS * sizeof(u4byte));
	il_tab  = (u4byte (*)[IL_TAB_SIZE_LS])alloc.malloc(
		IL_TAB_SIZE_LS * IL_TAB_SIZE_MS * sizeof(u4byte));
	#endif
	
	/* now fill them */
	gen_tabs();
	mTablesGenerated = true;
}

static ModuleNexus<GladmanInit> gladmanInit;

/*
 * AES encrypt/decrypt.
 */
GAESContext::GAESContext(AppleCSPSession &session) :
	BlockCryptor(session),
	mKeyValid(false),
	mInitFlag(false),
	mRawKeySize(0)	
{ 
	/* one-time only init */
	gladmanInit().genTables();
}

GAESContext::~GAESContext()
{
	deleteKey();
	memset(mRawKey, 0, MAX_AES_KEY_BITS / 8);
	mInitFlag = false;
}
	
void GAESContext::deleteKey()
{
	memset(&mAesKey, 0, sizeof(GAesKey));
	mKeyValid = false;
}

/* 
 * Standard CSPContext init, called from CSPFullPluginSession::init().
 * Reusable, e.g., query followed by en/decrypt. Even reusable after context
 * changed (i.e., new IV in Encrypted File System). 
 */
void GAESContext::init( 
	const Context &context, 
	bool encrypting)
{
	if(mInitFlag && !opStarted()) {
		return;
	}
	
	UInt32 		keyLen;
	UInt8 		*keyData = NULL;
	bool		sameKeySize = false;
	
	/* obtain key from context */
	symmetricKeyBits(context, CSSM_ALGID_AES, 
		encrypting ? CSSM_KEYUSE_ENCRYPT : CSSM_KEYUSE_DECRYPT,
		keyData, keyLen);
	
	/*
	 * Delete existing key if key size changed
	 */
	if(mRawKeySize == keyLen) {
		sameKeySize = true;
	}
	else {
		deleteKey();
	}
	
	/* init key only if key size or key bits have changed */
	if(!sameKeySize || memcmp(mRawKey, keyData, mRawKeySize)) {
		set_key((u4byte *)keyData, keyLen * 8, &mAesKey);

		/* save this raw key data */
		memmove(mRawKey, keyData, keyLen); 
		mRawKeySize = keyLen;
	}

	/* Finally, have BlockCryptor do its setup */
	setup(GLADMAN_BLOCK_SIZE_BYTES, context);
	mInitFlag = true;
}	

/*
 * Functions called by BlockCryptor
 */
void GAESContext::encryptBlock(
	const void		*plainText,			// length implied (one block)
	size_t			plainTextLen,
	void 			*cipherText,	
	size_t			&cipherTextLen,		// in/out, throws on overflow
	bool			final)				// ignored
{
	if(plainTextLen != GLADMAN_BLOCK_SIZE_BYTES) {
		CssmError::throwMe(CSSMERR_CSP_INPUT_LENGTH_ERROR);
	}
	if(cipherTextLen < GLADMAN_BLOCK_SIZE_BYTES) {
		CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
	}
	rEncrypt((u4byte *)plainText, (u4byte *)cipherText, &mAesKey);
	cipherTextLen = GLADMAN_BLOCK_SIZE_BYTES;
}

void GAESContext::decryptBlock(
	const void		*cipherText,		// length implied (one cipher block)
	void			*plainText,	
	size_t			&plainTextLen,		// in/out, throws on overflow
	bool			final)				// ignored
{
	if(plainTextLen < GLADMAN_BLOCK_SIZE_BYTES) {
		CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
	}
	rDecrypt((u4byte *)cipherText, (u4byte *)plainText, &mAesKey);
	plainTextLen = GLADMAN_BLOCK_SIZE_BYTES;
}

