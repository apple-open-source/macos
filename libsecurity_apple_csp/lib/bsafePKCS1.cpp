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

#ifdef	BSAFE_CSP_ENABLE


/*
 * bsafePKCS1.cpp - support for PKCS1 format RSA public key blobs, which for some
 * 					reason, BSAFE doesn't know about.
 */

#include "bsafePKCS1.h"
#include "bsafecspi.h"
#include "cspdebugging.h"
#include "bsobjects.h"
#include <Security/pkcs1oids.h>		/* for RSAPublicKey */
#include <Security/cdsaUtils.h>
#include <Security/cssmerrno.h>

/*
 * Simple conversion between BSAFE ITEM and snacc BigIntegerStr
 */
static void BS_ItemToSnaccBigInt(
	const ITEM		&item,
	BigIntegerStr	&snaccInt)
{
	snaccInt.Set(reinterpret_cast<const char *>(item.data), item.len);
}

/*  
 * This one doesn't do a malloc - the ITEM is only valid as long as
 * snaccInt is!
 */
static void BS_snaccBigIntToItem(
	BigIntegerStr 		&snaccInt,	// not const - we're passing a ptr
	ITEM				&item)
{
	char *cp = snaccInt;
	item.data = reinterpret_cast<unsigned char *>(cp);
	item.len = snaccInt.Len();
}

/*
 * Given a PKCS1-formatted key blob, decode the blob into components and do 
 * a B_SetKeyInfo on the specified BSAFE key.
 */
void BS_setKeyPkcs1(
	const CssmData &pkcs1Blob, 
	B_KEY_OBJ bsKey)
{
	/* DER-decode the blob */
	RSAPublicKey snaccPubKey;
	
	try {
		SC_decodeAsnObj(pkcs1Blob, snaccPubKey);
	}
	catch(const CssmError &cerror) {
		CSSM_RETURN crtn = cerror.cssmError();
		
		errorLog1("BS_setKeyPkcs1: SC_decodeAsnObj returned %s\n",
			cssmErrorString(crtn).c_str());
		switch(crtn) {
			case CSSMERR_CSSM_MEMORY_ERROR:
				crtn = CSSMERR_CSP_MEMORY_ERROR;
				break;
			case CSSMERR_CSSM_INVALID_INPUT_POINTER:
				crtn = CSSMERR_CSP_INVALID_KEY;
			default:
				break;
		}
		CssmError::throwMe(crtn);
	}
	
	/* 
	 * Convert BigIntegerStr modulus, publicExponent into
	 * ITEMS in an A_RSA_KEY.
	 */
	A_RSA_KEY	rsaKey;
	BS_snaccBigIntToItem(snaccPubKey.modulus, rsaKey.modulus);
	BS_snaccBigIntToItem(snaccPubKey.publicExponent, rsaKey.exponent);
	
	BSafe::check(
		B_SetKeyInfo(bsKey, KI_RSAPublic, POINTER(&rsaKey)), true);
}

/*
 * Obtain public key blob info, PKCS1 format. 
 */
void BS_GetKeyPkcs1(
	const B_KEY_OBJ bsKey, 
	CssmOwnedData &pkcs1Blob)
{
	/* get modulus/exponent info from BSAFE */
	A_RSA_KEY *rsaKey;
	BSafe::check(
		B_GetKeyInfo((POINTER *)&rsaKey, bsKey, KI_RSAPublic), true);
		
	/* Cook up a snacc-style RSAPublic key */
	RSAPublicKey snaccPubKey;
	BS_ItemToSnaccBigInt(rsaKey->modulus, snaccPubKey.modulus);
	BS_ItemToSnaccBigInt(rsaKey->exponent, snaccPubKey.publicExponent);
		
	/* estimate max size, BER-encode */
	size_t maxSize = 2 * (rsaKey->modulus.len + rsaKey->exponent.len);
	try {
		SC_encodeAsnObj(snaccPubKey, pkcs1Blob, maxSize);
	}
	catch(const CssmError &cerror) {
		CSSM_RETURN crtn = cerror.cssmError();

		errorLog1("BS_GetKeyPkcs1: SC_encodeAsnObj returned %s\n",
			cssmErrorString(crtn).c_str());
		switch(crtn) {
			case CSSMERR_CSSM_MEMORY_ERROR:
				crtn = CSSMERR_CSP_MEMORY_ERROR;
				break;
			default:
				break;
		}
		CssmError::throwMe(crtn);
	}
}
#endif	/* BSAFE_CSP_ENABLE */
