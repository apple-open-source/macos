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
	File:		sslBER.cpp

	Contains:	BER routines

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

#include "ssl.h"
#include "sslalloc.h"
#include "sslDebug.h"
#include "sslBER.h"

#include <Security/asn-incl.h>
#include <Security/sm_vdatypes.h>
#include <Security/asn-type.h>
#include <Security/pkcs1oids.h>		/* for RSAPublicKey */
#include <Security/cdsaUtils.h>
#include <string.h>
#include <Security/cssmdata.h>

/* convert between SSLBuffer and snacc-style BigIntegerStr */

static void snaccIntToData(
	const BigIntegerStr			&snaccInt,
	SSLBuffer					*outData)		// already mallocd
{
	const char *scp = snaccInt;
	uint8 *cp = (uint8 *)scp;
	uint32 len = snaccInt.Len();

	if (*cp == 0x00) {
		/* skip over this place-holding m.s. byte */
		cp++;
		len--;
	}

	memmove(outData->data, cp, len);
	outData->length = len;
}

static void dataToSnaccInt(
	const SSLBuffer		*inData,
	BigIntegerStr 		&snaccInt)
{
	uint8 *cp;
	int msbIsSet = 0;
	
	if (inData->data[0] & 0x80) {
		/* m.s. bit of BER data must be zero! */ 
		cp = (uint8 *)malloc(inData->length + 1);
		*cp = 0;
		memmove(cp+1, inData->data, inData->length);
		msbIsSet = 1;
	}
	else {
		cp = inData->data;
	}
	snaccInt.Set(reinterpret_cast<const char *>(cp), 
		inData->length + msbIsSet);
	if(msbIsSet) {
		free(cp);
	}
}

/*
 * Given a PKCS-1 encoded RSA public key, extract the 
 * modulus and public exponent.
 *
 * RSAPublicKey ::= SEQUENCE {
 *		modulus INTEGER, -- n
 *		publicExponent INTEGER -- e }
 */
 
SSLErr sslDecodeRsaBlob(
	const SSLBuffer	*blob,			/* PKCS-1 encoded */
	SSLBuffer		*modulus,		/* data mallocd and RETURNED */
	SSLBuffer		*exponent)		/* data mallocd and RETURNED */
{
	SSLErr				srtn;

	CASSERT(blob != NULL);
	CASSERT(modulus != NULL);
	CASSERT(exponent != NULL);
	
	/* DER-decode the blob */
	RSAPublicKey snaccPubKey;
	CssmData cssmBlob(blob->data, blob->length);
	try {
		SC_decodeAsnObj(cssmBlob, snaccPubKey);
	}
	catch(...) {
		return SSLBadCert;
	}
	
	/* malloc & convert components */
	srtn = SSLAllocBuffer(modulus, snaccPubKey.modulus.Len(), NULL);
	if(srtn) {
		return srtn;
	}
	snaccIntToData(snaccPubKey.modulus, modulus);
	srtn = SSLAllocBuffer(exponent, snaccPubKey.publicExponent.Len(), 
		NULL);
	if(srtn) {
		return srtn;
	}
	snaccIntToData(snaccPubKey.publicExponent, exponent);
	return SSLNoErr;
}

/*
 * Given a raw modulus and exponent, cook up a
 * BER-encoded RSA public key blob.
 */
SSLErr sslEncodeRsaBlob(
	const SSLBuffer	*modulus,		
	const SSLBuffer	*exponent,		
	SSLBuffer		*blob)			/* data mallocd and RETURNED */
{
	CASSERT((modulus != NULL) && (exponent != NULL));
	blob->data = NULL;
	blob->length = 0;

	/* Cook up a snacc-style RSAPublic key */
	RSAPublicKey snaccPubKey;
	dataToSnaccInt(modulus, snaccPubKey.modulus);
	dataToSnaccInt(exponent, snaccPubKey.publicExponent);
		
	/* estimate max size, BER-encode */
	size_t maxSize = 2 * (modulus->length + exponent->length);
	CssmAllocator &alloc  = CssmAllocator::standard();
	CssmAutoData cblob(alloc);
	try {
		SC_encodeAsnObj(snaccPubKey, cblob, maxSize);
	}
	catch(...) {
		/* right...? */
		return SSLMemoryErr;
	}
	
	/* copy to caller's SSLBuffer */
	SSLErr srtn = SSLAllocBuffer(blob, cblob.length(), NULL);
	if(srtn) {
		return srtn;
	}
	memmove(blob->data, cblob.data(), cblob.length());
	return SSLNoErr;
}

