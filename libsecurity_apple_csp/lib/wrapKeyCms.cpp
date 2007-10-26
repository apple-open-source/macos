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


//
// wrapKeyCms.cpp - wrap/unwrap key, CMS format 
//

#include "AppleCSPSession.h"
#include "AppleCSPUtils.h"
#include "AppleCSPKeys.h"
#include "cspdebugging.h"

/*
 *
 * Here is the algorithm implemented in this module:
 *
 * Note that DEK is the wrapping key,
 *
 * 1. PRIVATE_KEY_BYTES is the private data to be wrapped. It consists of the 
 *    following concatenation:
 *
 *    4-byte length of Descriptive Data, big-endian  |
 *    Descriptive Data |
 *    rawBlob.Data bytes
 * 
 * 2. Encrypt PRIVATE_KEY_BYTES using DEK (3DES) and IV in CBC mode with
 *    PKCS1 padding.  Call the ciphertext TEMP1
 * 
 * 3. Let TEMP2 = IV || TEMP1.
 * 
 * 4. Reverse the order of the octets in TEMP2 call the result TEMP3.
 * 
 * 5. Encrypt TEMP3 using DEK with an IV of 0x4adda22c79e82105 in CBC mode
 *    with PKCS1 padding call the result TEMP4.
 * 
 *    TEMP4 is wrappedKey.KeyData.
 */

/* true: cook up second CCHandle via a new HandleObject
 * false - OK to reuse a CCHandle */
#define USE_SECOND_CCHAND	0

/* false : make copy of incoming context before changing IV
 * true  : resuse OK */
#define REUSE_CONTEXT		1

/* lots'o'printfs in lieu of a debugger which works */
#define VERBOSE_DEBUG		0

static const uint8 magicCmsIv[] = 
	{ 0x4a, 0xdd, 0xa2, 0x2c, 0x79, 0xe8, 0x21, 0x05 };

#if		VERBOSE_DEBUG
static void dumpBuf(
	char				*title,
	const CSSM_DATA		*d,
	uint32 				maxLen)
{
	unsigned i;
	uint32 len;
	
	if(title) {
		printf("%s:  ", title);
	}
	if(d == NULL) {
		printf("NO DATA\n");
		return;
	}
	printf("Total Length: %d\n   ", d->Length);
	len = maxLen;
	if(d->Length < len) {
		len = d->Length;
	}
	for(i=0; i<len; i++) {
		printf("%02X ", d->Data[i]);
		if((i % 16) == 15) {
			printf("\n   ");
		}
	}
	printf("\n");
}
#else
#define dumpBuf(t, d, m)
#endif	/* VERBOSE_DEBUG */


/* serialize/deserialize uint32, big-endian. */
static void serializeUint32(uint32 i, uint8 *buf)
{
	*buf++ = (uint8)(i >> 24);
	*buf++ = (uint8)(i >> 16);
	*buf++ = (uint8)(i >> 8);
	*buf   = (uint8)i;
}

static uint32 deserializeUint32(const uint8 *buf) {
    	uint32 result;

    	result = ((uint32)buf[0] << 24) |
				 ((uint32)buf[1] << 16) |
				 ((uint32)buf[2] << 8)  |
				  (uint32)buf[3];
    	return result;
}

void AppleCSPSession::WrapKeyCms(
	CSSM_CC_HANDLE CCHandle,
	const Context &context,
	const AccessCredentials &AccessCred,
	const CssmKey &UnwrappedKey,
	CssmData &rawBlob,
	bool allocdRawBlob,			// callee has to free rawBlob
	const CssmData *DescriptiveData,
	CssmKey &WrappedKey,
	CSSM_PRIVILEGE Privilege)
{
	uint32 ddLen;
	CssmData PRIVATE_KEY_BYTES;
	#if		!REUSE_CONTEXT
	Context secondCtx(context.ContextType, context.AlgorithmType);
	secondCtx.copyFrom(context, privAllocator);
	#endif	/* REUSE_CONTEXT */
	
	/*
	 * 1. PRIVATE_KEY_BYTES is the private data to be wrapped. It consists of the 
	 *    following concatenation:
	 *
	 *    4-byte length of Descriptive Data, big-endian  |
	 *    Descriptive Data | 
	 *    rawBlob.Data bytes
	 */
	dumpBuf("wrap rawBlob", &rawBlob, 24);
	dumpBuf("wrap DescriptiveData", DescriptiveData, 24);
	
	if(DescriptiveData == NULL) {
		ddLen = 0;
	}
	else {
		ddLen = DescriptiveData->Length;
	}
	uint32 pkbLen = 4 +  ddLen + rawBlob.Length;
	setUpCssmData(PRIVATE_KEY_BYTES, pkbLen, privAllocator);
	uint8 *cp = PRIVATE_KEY_BYTES.Data;
	serializeUint32(ddLen, cp);
	cp += 4;
	if(ddLen != 0) {
		memcpy(cp, DescriptiveData->Data, ddLen);
		cp += ddLen;
	}
	memcpy(cp, rawBlob.Data, rawBlob.Length);
	dumpBuf("wrap PRIVATE_KEY_BYTES", &PRIVATE_KEY_BYTES, 48);
	
	/* 2. Encrypt PRIVATE_KEY_BYTES using DEK (3DES) and IV in CBC mode with
     *    PKCS1 padding.  Call the ciphertext TEMP1
	 *
	 * We'll just use the caller's context for this. Maybe we should 
	 * validate mode, padding, IV?
	 */
	CssmData TEMP1;
	CSSM_SIZE bytesEncrypted;
	CssmData remData;
	EncryptData(CCHandle,
		context,
		&PRIVATE_KEY_BYTES,	// ClearBufs[]
		1,					// ClearBufCount
		&TEMP1,				// CipherBufs[],
		1,					// CipherBufCount,
		bytesEncrypted,
		remData,
		Privilege);
	
	// I'm not 100% sure about this....
	assert(remData.Length == 0);
	TEMP1.Length = bytesEncrypted;
	dumpBuf("wrap TEMP1", &TEMP1, 48);
	
	/* 
	 * 3. Let TEMP2 = IV || TEMP1.
	 */
	CssmData TEMP2;
	CssmData &IV = context.get<CssmData>(CSSM_ATTRIBUTE_INIT_VECTOR, 
				CSSMERR_CSP_MISSING_ATTR_INIT_VECTOR);
	setUpCssmData(TEMP2, IV.Length + TEMP1.Length, privAllocator);
	memcpy(TEMP2.Data, IV.Data, IV.Length);
	memcpy(TEMP2.Data + IV.Length, TEMP1.Data, TEMP1.Length);
	dumpBuf("wrap TEMP2", &TEMP2, 56);
	
	
	/* 
	 * 4. Reverse the order of the octets in TEMP2 call the result 
	 *    TEMP3.
	 */
	CssmData TEMP3;
	setUpCssmData(TEMP3, TEMP2.Length, privAllocator);
	uint8 *cp2 = TEMP2.Data + TEMP2.Length - 1;
	cp = TEMP3.Data;
	for(uint32 i=0; i<TEMP2.Length; i++) {
		*cp++ = *cp2--;
	} 
	dumpBuf("wrap TEMP3", &TEMP3, 64);

	/* 
     * 5. Encrypt TEMP3 using DEK with an IV of 0x4adda22c79e82105 in CBC mode
	 *    with PKCS1 padding call the result TEMP4.
	 * 
	 *    TEMP4 is wrappedKey.KeyData.
	 *
	 * This is the tricky part - we're going to use the caller's context
	 * again, but we're going to modify the IV. 
	 * We're assuming here that the IV we got via context.get<CssmData>
	 * actually is in the context and not a copy!
	 */
	#if		REUSE_CONTEXT
	CssmData &IV2 = context.get<CssmData>(CSSM_ATTRIBUTE_INIT_VECTOR, 
				CSSMERR_CSP_MISSING_ATTR_INIT_VECTOR);
	#else
	CssmData &IV2 = secondCtx.get<CssmData>(CSSM_ATTRIBUTE_INIT_VECTOR, 
				CSSMERR_CSP_MISSING_ATTR_INIT_VECTOR);
	#endif	/* REUSE_CONTEXT */
	
	uint8 *savedIV = IV2.Data;
	uint32 savedIVLen = IV2.Length;
	IV2.Data = (uint8 *)magicCmsIv;
	IV2.Length = 8;
	CssmData &outBlob = CssmData::overlay(WrappedKey.KeyData);
	outBlob.Length = 0;
	outBlob.Data = NULL;
	try {
		EncryptData(CCHandle,
			#if		REUSE_CONTEXT
			context,
			#else
			secondCtx,
			#endif	/* REUSE_CONTEXT */
			
			&TEMP3,	// ClearBufs[]
			1,					// ClearBufCount
			&outBlob,			// CipherBufs[],
			1,					// CipherBufCount,
			bytesEncrypted,
			remData,
			Privilege);
	}
	catch (...) {
		IV2.Data = savedIV;
		IV2.Length = savedIVLen;
		throw;		// and leak
	}
	IV2.Data = savedIV;
	IV2.Length = savedIVLen;

	// I'm not 100% sure about this....
	assert(remData.Length == 0);
	outBlob.Length = bytesEncrypted;
	dumpBuf("wrap outBlob", &outBlob, 64);
	
	/* outgoing header */
	WrappedKey.KeyHeader.BlobType = CSSM_KEYBLOB_WRAPPED;
	// OK to be zero or not present 
	WrappedKey.KeyHeader.WrapMode = context.getInt(CSSM_ATTRIBUTE_MODE);
	WrappedKey.KeyHeader.Format = CSSM_KEYBLOB_WRAPPED_FORMAT_APPLE_CUSTOM;
	
	/* free resources */
	freeCssmData(PRIVATE_KEY_BYTES, privAllocator);
	freeCssmData(TEMP1, normAllocator);		// alloc via encrypt
	freeCssmData(TEMP2, privAllocator); 
	freeCssmData(TEMP3, privAllocator); 
	if(allocdRawBlob) {
		/* our caller mallocd this when dereferencing a ref key */
		freeCssmData(rawBlob, privAllocator);
	}
}

/* note we expect an IV present in the context though we don't use it
 * FIXME - we should figure out how to add this attribute at this level
 */
 
/* safety trap - don't try to malloc anything bigger than this - we get 
 * sizes from the processed bit stream.... */
#define MAX_MALLOC_SIZE		0x10000

void AppleCSPSession::UnwrapKeyCms(
	CSSM_CC_HANDLE CCHandle,
	const Context &Context,
	const CssmKey &WrappedKey,
	const CSSM_RESOURCE_CONTROL_CONTEXT *CredAndAclEntry,
	CssmKey &UnwrappedKey,
	CssmData &DescriptiveData,
	CSSM_PRIVILEGE Privilege,
	cspKeyStorage keyStorage)
{
	/*
	 * In reverse order, the steps from wrap...
	 * 
     * 5. Encrypt TEMP3 using DEK with an IV of 0x4adda22c79e82105 in CBC mode
	 *    with PKCS1 padding call the result TEMP4.
	 * 
	 *    TEMP4 is wrappedKey.KeyData.
	 */
	const CssmData &wrappedBlob = CssmData::overlay(WrappedKey.KeyData);
	dumpBuf("unwrap inBlob", &wrappedBlob, 64);
	CssmData &IV1 = Context.get<CssmData>(CSSM_ATTRIBUTE_INIT_VECTOR, 
				CSSMERR_CSP_MISSING_ATTR_INIT_VECTOR);
	uint8 *savedIV = IV1.Data;
	uint32 savedIvLen = IV1.Length;
	IV1.Data = (uint8 *)magicCmsIv;
	IV1.Length = 8;
	CssmData TEMP3;
	CSSM_SIZE bytesDecrypted;
	CssmData remData;
	
	try {
		DecryptData(CCHandle,
			Context,
			&wrappedBlob,		// CipherBufs[],
			1,					// CipherBufCount,
			&TEMP3,				// ClearBufs[]
			1,					// ClearBufCount
			bytesDecrypted,
			remData,
			Privilege);
	}
	catch(...) {
		IV1.Data = savedIV;
		IV1.Length = savedIvLen;
		throw;
	}
	IV1.Data = savedIV;
	IV1.Length = savedIvLen;
	// I'm not 100% sure about this....
	assert(remData.Length == 0);
	TEMP3.Length = bytesDecrypted;
	dumpBuf("unwrap TEMP3", &TEMP3, 64);
	
	/* 
	 * 4. Reverse the order of the octets in TEMP2 call the result 
	 *    TEMP3.
	 *
	 * i.e., TEMP2 := reverse(TEMP3)
	 */
	CssmData TEMP2;
	setUpCssmData(TEMP2, TEMP3.Length, privAllocator);
	uint8 *src = TEMP3.Data + TEMP3.Length - 1;
	uint8 *dst = TEMP2.Data;
	for(uint32 i=0; i<TEMP2.Length; i++) {
		*dst++ = *src--;
	} 
	dumpBuf("unwrap TEMP2", &TEMP2, 64);

	/* 
	 * 3. Let TEMP2 = IV || TEMP1.
	 *
	 * IV2 is first 8 bytes of TEMP2, remainder is TEMP1 
	 */
	if(TEMP2.Length <= 8) {
		dprintf0("UnwrapKeyCms: short TEMP2\n");
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
	}
	CssmData IV2;
	CssmData TEMP1;
	setUpCssmData(IV2, 8, privAllocator);
	setUpCssmData(TEMP1, TEMP2.Length - 8, privAllocator);
	memcpy(IV2.Data, TEMP2.Data, 8);
	memcpy(TEMP1.Data, TEMP2.Data + 8, TEMP1.Length);
	dumpBuf("unwrap TEMP1", &TEMP1, 48);

	/* 
	 * 2. Encrypt PRIVATE_KEY_BYTES using DEK (3DES) and IV in CBC mode with
     *    PKCS1 padding.  Call the ciphertext TEMP1
	 *
	 * i.e., decrypt TEMP1 to get PRIVATE_KEY_BYTES. Use IV2, not caller's 
	 * IV. We already saved caller's IV in savediV and savedIvLen.
	 */
	IV1 = IV2;
	CssmData PRIVATE_KEY_BYTES;
	try {
		DecryptData(CCHandle,
			Context,
			&TEMP1,				// CipherBufs[],
			1,					// CipherBufCount,
			&PRIVATE_KEY_BYTES,	// ClearBufs[]
			1,					// ClearBufCount
			bytesDecrypted,
			remData,
			Privilege);
	}
	catch(...) {
		IV1.Data = savedIV;
		IV1.Length = savedIvLen;
		throw;
	}
	IV1.Data = savedIV;
	// I'm not 100% sure about this....
	assert(remData.Length == 0);
	PRIVATE_KEY_BYTES.Length = bytesDecrypted;
	dumpBuf("unwrap PRIVATE_KEY_BYTES", &PRIVATE_KEY_BYTES, 64);

	/*
	 * 1. PRIVATE_KEY_BYTES is the private data to be wrapped. It consists of the 
	 *    following concatenation:
	 *
	 *    4-byte length of Descriptive Data, big-endian  |
	 *    Descriptive Data | 
	 *    rawBlob.Data bytes
	 */
	if(PRIVATE_KEY_BYTES.Length < 4) {
		dprintf0("UnwrapKeyCms: short PRIVATE_KEY_BYTES\n");
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
	}
	uint8 *cp1 = PRIVATE_KEY_BYTES.Data;
	uint32 ddLen = deserializeUint32(cp1);
	cp1 += 4;
	if(ddLen > MAX_MALLOC_SIZE) {
		dprintf0("UnwrapKeyCms: preposterous ddLen in PRIVATE_KEY_BYTES\n");
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
	}
	setUpCssmData(DescriptiveData, ddLen, normAllocator);
	memcpy(DescriptiveData.Data, cp1, ddLen);
	cp1 += ddLen;
	uint32 outBlobLen = PRIVATE_KEY_BYTES.Length - ddLen - 4;
	if(ddLen > MAX_MALLOC_SIZE) {
		dprintf0("UnwrapKeyCms: preposterous outBlobLen in PRIVATE_KEY_BYTES\n");
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
	}
	CssmData &outBlob = CssmData::overlay(UnwrappedKey.KeyData);
	setUpCssmData(outBlob, outBlobLen, normAllocator);
	memcpy(outBlob.Data, cp1, outBlobLen);

	/* set up outgoing header */
	UnwrappedKey.KeyHeader.BlobType = CSSM_KEYBLOB_RAW;
	UnwrappedKey.KeyHeader.Format   = inferFormat(UnwrappedKey);

	/* 
	 * Cook up a BinaryKey if caller wants a reference key.
	 */
	if(keyStorage == CKS_Ref) {
		BinaryKey *binKey = NULL;
		CSPKeyInfoProvider *provider = infoProvider(UnwrappedKey);
		/* optional parameter-bearing key */
		CssmKey *paramKey = Context.get<CssmKey>(CSSM_ATTRIBUTE_PARAM_KEY);
		provider->CssmKeyToBinary(paramKey, UnwrappedKey.KeyHeader.KeyAttr, &binKey);
		addRefKey(*binKey, UnwrappedKey);
		delete provider;
	}
	/* free resources */
	freeCssmData(PRIVATE_KEY_BYTES, normAllocator);	// alloc via decrypt
	freeCssmData(TEMP1, privAllocator);		
	freeCssmData(IV2, privAllocator);		
	freeCssmData(TEMP2, privAllocator); 
	freeCssmData(TEMP3, normAllocator);		// via decrypt 

}

