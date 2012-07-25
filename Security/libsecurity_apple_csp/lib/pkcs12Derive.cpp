/*
 * Copyright (c) 2003 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please 
 * obtain a copy of the License at http://www.apple.com/publicsource and 
 * read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. 
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 */
/*
 * pkcs12Derive.cpp - PKCS12 PBE routine
 *
 * Created 2/28/03 by Doug Mitchell.
 */

#include <Security/cssmapple.h>
#include <openssl/bn.h>
#include <pbkdDigest.h>

#include "pkcs12Derive.h"
#include "AppleCSPUtils.h"
#include "AppleCSPContext.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <security_asn1/SecNssCoder.h>

#include <CoreFoundation/CoreFoundation.h>

/* specify which flavor of bits to generate */
typedef enum {
	PBE_ID_Key 	= 1,
	PBE_ID_IV  	= 2,
	PBE_ID_MAC	= 3
} P12_PBE_ID;	
  
/*
 * Create a "string" (in the loose p12 notation) of specified length 
 * from the concatention of copies of the specified input string.
 */
static unsigned char *p12StrCat(
	const unsigned char *inStr,
	unsigned inStrLen,
	SecNssCoder &coder,
	unsigned outLen,
	unsigned char *outStr = NULL)	// if not present, we malloc
{
	if(outStr == NULL) {
		outStr = (unsigned char *)coder.malloc(outLen);
	}
	unsigned toMove = outLen;
	unsigned char *outp = outStr;
	while(toMove) {
		unsigned thisMove = inStrLen;
		if(thisMove > toMove) {
			thisMove = toMove;
		}
		memmove(outp, inStr, thisMove);
		toMove -= thisMove;
		outp   += thisMove;
	}
	return outStr;
}

/*
 * PBE generator per PKCS12 v.1 section B.2.
 */
static CSSM_RETURN p12PbeGen(
	const CSSM_DATA		&pwd,			// unicode, double null terminated
	const uint8 		*salt,
	unsigned 			saltLen,
	unsigned 			iterCount,
	P12_PBE_ID 			pbeId,
	CSSM_ALGORITHMS 	hashAlg,		// MS5 or SHA1 only
	SecNssCoder			&coder,			// for temp allocs
	/* result goes here, mallocd by caller */
	uint8 				*outbuf,	
	unsigned 			outbufLen)
{
	CSSM_RETURN ourRtn = CSSM_OK;
	unsigned unipassLen = pwd.Length;
	unsigned char *unipass = pwd.Data;
	int irtn;
	
	/*
	 * all variables of the form p12_<XXX> represent <XXX> from the 
	 * PKCS12 spec. E.g., p12_u is u, the length of the digest output.
	 * Only difference here is: all of our sizes are in BYTES, not
	 * bits. 
	 */
	unsigned p12_r = iterCount;
	unsigned p12_n = outbufLen;

	unsigned p12_u;					// hash output size
	unsigned p12_v;					// hash block size
	unsigned char *p12_P = NULL;	// catted passwords
	unsigned char *p12_S = NULL;	// catted salts
	
	switch(hashAlg) {
		case CSSM_ALGID_MD5:
			p12_u = kMD5DigestSize;
			p12_v = kMD5BlockSize;
			break;
		case CSSM_ALGID_SHA1:
			p12_u = kSHA1DigestSize;
			p12_v = kSHA1BlockSize;
			break;
		default:
			return CSSMERR_CSP_INVALID_ALGORITHM;
	}
	
	/* 
	 * 1. Construct a string, D (the diversifier), by 
	 *    concatenating v/8 copies of ID.
	 */
	unsigned char *p12_D = NULL;		// diversifier
	p12_D = (unsigned char *)coder.malloc(p12_v);
	for(unsigned dex=0; dex<p12_v; dex++) {
		p12_D[dex] = (unsigned char)pbeId;
	}
	
	/*
	 * 2. Concatenate copies of the salt together to create 
	 *    a string S of length v * ceil(s/v) bits (the final copy 
	 *    of the salt may be truncated to create S). Note that if 
	 *    the salt is the empty string, then so is S.
	 */
	unsigned p12_Slen = p12_v * ((saltLen + p12_v - 1) / p12_v);
	if(p12_Slen) {
		p12_S = p12StrCat(salt, saltLen, coder, p12_Slen);
	}
	
	 
	/* 
	 * 3. Concatenate copies of the password together to create 
	 *    a string P of length v * ceil(p/v) bits (the final copy of 
	 *    the password may be truncated to create P). Note that 
	 *    if the password is the empty string, then so is P.
	 */
	unsigned p12_Plen = p12_v * ((unipassLen + p12_v - 1) / p12_v);
	if(p12_Plen) {
		p12_P = p12StrCat(unipass, unipassLen, coder, p12_Plen);
	}
	
	/*
	 * 4. Set I= S||P to be the concatenation of S and P.
	 */
	unsigned char *p12_I = 
		(unsigned char *)coder.malloc(p12_Slen + p12_Plen);
	memmove(p12_I, p12_S, p12_Slen);
	if(p12_Plen) {
		memmove(p12_I + p12_Slen, p12_P, p12_Plen);
	}
	
	/*
	 * 5. Set c = ceil(n/u). 
	 */
	unsigned p12_c = (p12_n + p12_u - 1) / p12_u;
	
	/* allocate c hash-output-size bufs */
	unsigned char *p12_A = (unsigned char *)coder.malloc(p12_c * p12_u);

	/* one reusable hash object */
	DigestCtx ourDigest;
	DigestCtx *hashHand = &ourDigest;
	memset(hashHand, 0, sizeof(hashHand));
	
	/* reused inside the loop */
	unsigned char *p12_B = (unsigned char *)coder.malloc(p12_v + 1);
	BIGNUM *Ij = BN_new();
	BIGNUM *Bpl1 = BN_new();
	
	/*
	 * 6. For i=1, 2, ..., p12_c, do the following:
	 */
	for(unsigned p12_i=0; p12_i<p12_c; p12_i++) {
		unsigned char *p12_AsubI = p12_A + (p12_i * p12_u);
		
		/* 
		 * a) Set A[i] = H**r(D||I). (i.e. the rth hash of D||I, 
		 *    H(H(H(...H(D||I))))
		 */
		irtn = DigestCtxInit(hashHand, hashAlg); 
		if(!irtn) {
			ourRtn = CSSMERR_CSP_INTERNAL_ERROR;
			break;
		}
		DigestCtxUpdate(hashHand, p12_D, p12_v); 
		DigestCtxUpdate(hashHand, p12_I, p12_Slen + p12_Plen);
		DigestCtxFinal(hashHand, p12_AsubI); 

		for(unsigned iter=1; iter<p12_r; iter++) {
			irtn = DigestCtxInit(hashHand, hashAlg);
			if(!irtn) {
				ourRtn = CSSMERR_CSP_INTERNAL_ERROR;
				break;
			}
			DigestCtxUpdate(hashHand, p12_AsubI, p12_u);
			DigestCtxFinal(hashHand, p12_AsubI);
		}
		
		/*
		 * b) Concatenate copies of A[i] to create a string B of 
		 *    length v bits (the final copy of A[i]i may be truncated 
		 *    to create B).
		 */
		p12StrCat(p12_AsubI, p12_u, coder, p12_v, p12_B);
		
		/*
		 * c) Treating I as a concatenation I[0], I[1], ..., 
		 *    I[k-1] of v-bit blocks, where k = ceil(s/v) + ceil(p/v),
		 *    modify I by setting I[j]=(I[j]+B+1) mod (2 ** v)
		 *    for each j.
		 *
		 * Copied from PKCS12_key_gen_uni() from openssl...
		 */
		/* Work out B + 1 first then can use B as tmp space */
		BN_bin2bn (p12_B, p12_v, Bpl1);
		BN_add_word (Bpl1, 1);
		unsigned Ilen = p12_Slen + p12_Plen;
		
		for (unsigned j = 0; j < Ilen; j+=p12_v) {
			BN_bin2bn (p12_I + j, p12_v, Ij);
			BN_add (Ij, Ij, Bpl1);
			BN_bn2bin (Ij, p12_B);
			unsigned Ijlen = BN_num_bytes (Ij);
			/* If more than 2^(v*8) - 1 cut off MSB */
			if (Ijlen > p12_v) {
				BN_bn2bin (Ij, p12_B);
				memcpy (p12_I + j, p12_B + 1, p12_v);
			/* If less than v bytes pad with zeroes */
			} else if (Ijlen < p12_v) {
				memset(p12_I + j, 0, p12_v - Ijlen);
				BN_bn2bin(Ij, p12_I + j + p12_v - Ijlen); 
			} else BN_bn2bin (Ij, p12_I + j);
		}
	}	
	 
	if(ourRtn == CSSM_OK) {	
		/*
		 * 7. Concatenate A[1], A[2], ..., A[c] together to form a 
		 *    pseudo-random bit string, A.
		 *
		 * 8. Use the first n bits of A as the output of this entire 
		 *    process.
		 */
		memmove(outbuf, p12_A, outbufLen);
	}
	
	/* clear all these strings */
	if(p12_D) {
		memset(p12_D, 0, p12_v);
	}
	if(p12_S) {
		memset(p12_S, 0, p12_Slen);
	}
	if(p12_P) {
		memset(p12_P, 0, p12_Plen);
	}
	if(p12_I) {
		memset(p12_I, 0, p12_Slen + p12_Plen);
	}
	if(p12_A) {
		memset(p12_A, 0, p12_c * p12_u);
	}
	if(p12_B) {
		memset(p12_B, 0, p12_v);
	}
	if(hashHand) {
		DigestCtxFree(hashHand);
	}
	BN_free(Bpl1);
	BN_free(Ij);
	return ourRtn;
}

/*
 * Public P12 derive key function, called out from 
 * AppleCSPSession::DeriveKey()
 *
 * On input:
 * ---------
 * Context parameters:
 *    	Salt
 *		Iteration Count
 *		CSSM_CRYPTO_DATA.Param - Unicode passphrase, double-NULL terminated
 *		Algorithm - CSSM_ALGID_PKCS12_PBE_{ENCR,MAC}
 * Passed explicitly from DeriveKey():
 *		CSSM_DATA Param - IN/OUT - optional IV - caller mallocs space to
 *			tell us to generate an IV. The param itself is not 
 *			optional; the presence or absence of allocated data in it
 *			is our IV indicator (present/absent as well as size)
 *		KeyData - mallocd by caller, we fill in keyData->Length bytes
 */
void DeriveKey_PKCS12 (
	const Context &context,
	AppleCSPSession	&session,
	const CssmData &Param,			// other's public key
	CSSM_DATA *keyData)				// mallocd by caller
									// we fill in keyData->Length bytes
{
	SecNssCoder tmpCoder;
	
	/*
	 * According to the spec, both passphrase and salt are optional.
	 * Get them from context if they're present. In practical terms
	 * the user really should supply a passphrase either in the 
	 * seed attribute (as a Unicode passphrase) or as the BaseKey
	 * as a CSSM_ALGID_SECURE_PASSPHRASE key). 
	 */
	CSSM_DATA pwd = {0, NULL};
	CSSM_DATA appPwd = {0, NULL};
	CssmCryptoData *cryptData = 
		context.get<CssmCryptoData>(CSSM_ATTRIBUTE_SEED);
	if((cryptData != NULL) && (cryptData->Param.Length != 0)) {
		appPwd = cryptData->Param;
	}
	else {
		/* Get pwd from base key */
		CssmKey *passKey = context.get<CssmKey>(CSSM_ATTRIBUTE_KEY);
		if (passKey != NULL) {
			AppleCSPContext::symmetricKeyBits(context, session,
				CSSM_ALGID_SECURE_PASSPHRASE, CSSM_KEYUSE_DERIVE, 
				appPwd.Data, appPwd.Length);
		}
	}
	if(appPwd.Data) {
		/*
		 * The incoming passphrase is a UTF8 encoded enternal representation
		 * of a CFString. Convert to CFString and obtain the unicode characters
		 * from the string.
		 */
		CFDataRef cfData = CFDataCreate(NULL, appPwd.Data, appPwd.Length);
		CFStringRef cfStr = CFStringCreateFromExternalRepresentation(NULL,
			cfData, kCFStringEncodingUTF8);
        if (cfData)
            CFRelease(cfData);
		if(cfStr == NULL) {
			CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_SEED);
		}
		
		/* convert unicode to chars with an extra double-NULL */
		unsigned len = CFStringGetLength(cfStr);
		tmpCoder.allocItem(pwd, sizeof(UniChar) * (len + 1));
		unsigned char *cp = pwd.Data;
		UniChar uc = 0;
		for(unsigned dex=0; dex<len; dex++) {
			uc = CFStringGetCharacterAtIndex(cfStr, dex);
			*cp++ = uc >> 8;
			*cp++ = uc & 0xff;
		}
		/* CFString tends to include a NULL at the end; add it if it's not there */
		if(uc == 0) {
			if(pwd.Length < 2) {
				CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_SEED);
			}
			pwd.Length -= 2;
		}
		else {
			*cp++ = 0;
			*cp++ = 0;
		}
        if (cfStr)
            CFRelease(cfStr);
	}

	/* salt from context */
	uint32 saltLen = 0;
	uint8  *salt = NULL;
	CssmData *csalt = context.get<CssmData>(CSSM_ATTRIBUTE_SALT);
	if(csalt) {
		salt = csalt->Data;
		saltLen = csalt->Length;
	}
	
	/* 
	 * Iteration count, from context, required.
	 * The spec's ASN1 definition says this is optional with a default
	 * of one but that's a BER encode/decode issue. Here we require
	 * a nonzero value.
	 */
	uint32 iterCount = context.getInt(CSSM_ATTRIBUTE_ITERATION_COUNT,
		CSSMERR_CSP_MISSING_ATTR_ITERATION_COUNT);
	if(iterCount == 0) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_ITERATION_COUNT);
	}

	/* 
	 * Algorithm determines which of {PBE_ID_Key,PBE_ID_MAC} we now
	 * generate. We'll also do an optional PBE_ID_IV later.
	 */
	P12_PBE_ID pbeId = PBE_ID_Key;
	switch(context.algorithm()) {
		case CSSM_ALGID_PKCS12_PBE_ENCR:
			pbeId = PBE_ID_Key;
			break;
		case CSSM_ALGID_PKCS12_PBE_MAC:
			pbeId = PBE_ID_MAC;
			break;
		default:
			/* really should not be here */
			assert(0);
			CssmError::throwMe(CSSMERR_CSP_INTERNAL_ERROR);
	}
	
	/* Go */
	CSSM_RETURN crtn = p12PbeGen(pwd,
		salt, saltLen,
		iterCount,
		pbeId,
		CSSM_ALGID_SHA1,			// all we support for now
		tmpCoder,
		keyData->Data,
		keyData->Length);
	if(crtn) {
		CssmError::throwMe(crtn);
	}
	
	/* 
	 * Optional IV - makes no sense if we just did PBE_ID_MAC, but why
	 * bother restricting?
	 */
	if(Param.Data) {
		crtn = p12PbeGen(pwd,
			salt, saltLen,
			iterCount,
			PBE_ID_IV,
			CSSM_ALGID_SHA1,			// all we support for now
			tmpCoder,
			Param.Data,
			Param.Length);
		if(crtn) {
			CssmError::throwMe(crtn);
		}
	}
}

