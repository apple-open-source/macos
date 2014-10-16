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
 * p12pbe.h - PKCS12 PBE routine. App space reference version.
 *
 * Created 2/28/03 by Doug Mitchell.
*/

#include "p12pbe.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <Security/cssm.h>
#include <openssl/bn.h>
/*
 * For development outside the CSP, malloc using stdlib.
 * Inside the CSP we'll use CssmAllocator.
 */
#define PBE_MALLOC	malloc
#define PBE_FREE	free

/* 
 * implementation dependent hash object
 * for now just a Digest context handle 
 */
typedef CSSM_CC_HANDLE HashHand;
static HashHand hashCreate(CSSM_CSP_HANDLE cspHand,
	CSSM_ALGORITHMS alg)
{
	CSSM_CC_HANDLE hashHand;
	CSSM_RETURN crtn = CSSM_CSP_CreateDigestContext(cspHand,
		alg,
		&hashHand);
	if(crtn) {
		printf("CSSM_CSP_CreateDigestContext error\n");
		return 0;
	}
	return hashHand;
}

static CSSM_RETURN hashInit(HashHand hand)
{
	return CSSM_DigestDataInit(hand);
}

static CSSM_RETURN hashUpdate(HashHand hand,
	const unsigned char *buf,
	unsigned bufLen)
{
	const CSSM_DATA cdata = {bufLen, (uint8 *)buf};
	return CSSM_DigestDataUpdate(hand, &cdata, 1);
}

static CSSM_RETURN hashFinal(HashHand hand,
	unsigned char *digest,		// mallocd by caller
	unsigned *digestLen)		// IN/OUT
{
	CSSM_DATA cdata = {(uint32)digestLen, digest};
	return CSSM_DigestDataFinal(hand, &cdata);
}

static CSSM_RETURN hashDone(HashHand hand)
{
	return CSSM_DeleteContext(hand);
}
  
/*
 * Create a "string" (in the loose p12 notation) of specified length 
 * from the concatention of copies of the specified input string.
 */
static unsigned char *p12StrCat(
	const unsigned char *inStr,
	unsigned inStrLen,
	unsigned outLen,
	unsigned char *outStr = NULL)	// if not present, we malloc
{
	if(outStr == NULL) {
		outStr = (unsigned char *)PBE_MALLOC(outLen);
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
CSSM_RETURN p12PbeGen_app(
	const CSSM_DATA	&pwd,		// unicode, double null terminated
	const unsigned char *salt,
	unsigned saltLen,
	unsigned iterCount,
	P12_PBE_ID pbeId,
	CSSM_ALGORITHMS hashAlg,	// MS5 or SHA1 only
	CSSM_CSP_HANDLE cspHand,
	/* result goes here, mallocd by caller */
	unsigned char *outbuf,	
	unsigned outbufLen)
{
	CSSM_RETURN ourRtn;
	unsigned unipassLen = pwd.Length;
	unsigned char *unipass = pwd.Data;
	
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
			p12_u = 16;
			p12_v = 64;
			break;
		case CSSM_ALGID_SHA1:
			p12_u = 20;
			p12_v = 64;
			break;
		default:
			return CSSMERR_CSP_INVALID_ALGORITHM;
	}
	
	/* 
	 * 1. Construct a string, D (the “diversifier”), by 
	 *    concatenating v/8 copies of ID.
	 */
	unsigned char *p12_D = NULL;		// diversifier
	p12_D = (unsigned char *)PBE_MALLOC(p12_v);
	/* subsequent errors to errOut: */
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
		p12_S = p12StrCat(salt, saltLen, p12_Slen);
	}
	
	 
	/* 
	 * 3. Concatenate copies of the password together to create 
	 *    a string P of length v * ceil(p/v) bits (the final copy of 
	 *    the password may be truncated to create P). Note that 
	 *    if the password is the empty string, then so is P.
	 */
	unsigned p12_Plen = p12_v * ((unipassLen + p12_v - 1) / p12_v);
	if(p12_Plen) {
		p12_P = p12StrCat(unipass, unipassLen, p12_Plen);
	}
	
	/*
	 * 4. Set I= S||P to be the concatenation of S and P.
	 */
	unsigned char *p12_I = 
		(unsigned char *)PBE_MALLOC(p12_Slen + p12_Plen);
	memmove(p12_I, p12_S, p12_Slen);
	if(p12_Plen) {
		memmove(p12_I + p12_Slen, p12_P, p12_Plen);
	}
	
	/*
	 * 5. Set c = ceil(n/u). 
	 */
	unsigned p12_c = (p12_n + p12_u - 1) / p12_u;
	
	/* allocate c hash-output-size bufs */
	unsigned char *p12_A = (unsigned char *)PBE_MALLOC(p12_c * p12_u);

	/* one reusable hash object */
	HashHand hashHand = hashCreate(cspHand, hashAlg);
	if(!hashHand) {
		return CSSMERR_CSP_INVALID_CONTEXT_HANDLE;	// XXX
	}
	
	/* reused inside the loop */
	unsigned char *p12_B = (unsigned char *)PBE_MALLOC(p12_v + sizeof(int));
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
		ourRtn = hashInit(hashHand); 
		if(ourRtn) break;
		ourRtn = hashUpdate(hashHand, p12_D, p12_v); 
		if(ourRtn) break;
		ourRtn = hashUpdate(hashHand, p12_I, p12_Slen + p12_Plen);
		if(ourRtn) break;
		unsigned outLen = p12_u;
		ourRtn = hashFinal(hashHand, p12_AsubI, &outLen); 
		if(ourRtn) break;

		for(unsigned iter=1; iter<p12_r; iter++) {
			ourRtn = hashInit(hashHand);
			if(ourRtn) break;
			ourRtn = hashUpdate(hashHand, p12_AsubI, p12_u);
			if(ourRtn) break;
			ourRtn = hashFinal(hashHand, p12_AsubI, &outLen);
			if(ourRtn) break;
		}
		
		/*
		 * b) Concatenate copies of A[i] to create a string B of 
		 *    length v bits (the final copy of A[i]i may be truncated 
		 *    to create B).
		 */
		p12StrCat(p12_AsubI, p12_u, p12_v, p12_B);
		
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
	 
	if(ourRtn) {
		goto errOut;
	}
	
	/*
	 * 7. Concatenate A[1], A[2], ..., A[c] together to form a 
	 *    pseudo-random bit string, A.
	 *
	 * 8. Use the first n bits of A as the output of this entire 
	 *    process.
	 */
	memmove(outbuf, p12_A, outbufLen);
	ourRtn = CSSM_OK;
	
errOut:
	/* FIXME clear all these strings */
	if(p12_D) {
		PBE_FREE(p12_D);
	}
	if(p12_S) {
		PBE_FREE(p12_S);
	}
	if(p12_P) {
		PBE_FREE(p12_P);
	}
	if(p12_I) {
		PBE_FREE(p12_I);
	}
	if(p12_A) {
		PBE_FREE(p12_A);
	}
	if(p12_B) {
		PBE_FREE(p12_B);
	}
	if(hashHand) {
		hashDone(hashHand);
	}
	BN_free(Bpl1);
	BN_free(Ij);
	return ourRtn;
}

