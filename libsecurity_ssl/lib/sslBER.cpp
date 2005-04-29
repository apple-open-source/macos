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
#include "sslMemory.h"
#include "sslDebug.h"
#include "sslBER.h"
#include "appleCdsa.h"

#include <string.h>
#include <security_cdsa_utilities/cssmdata.h>
#include <security_asn1/SecNssCoder.h>
#include <Security/keyTemplates.h>

/*
 * Given a PKCS-1 encoded RSA public key, extract the 
 * modulus and public exponent.
 *
 * RSAPublicKey ::= SEQUENCE {
 *		modulus INTEGER, -- n
 *		publicExponent INTEGER -- e }
 */
 
OSStatus sslDecodeRsaBlob(
	const SSLBuffer	*blob,			/* PKCS-1 encoded */
	SSLBuffer		*modulus,		/* data mallocd and RETURNED */
	SSLBuffer		*exponent)		/* data mallocd and RETURNED */
{
	OSStatus srtn;

	assert(blob != NULL);
	assert(modulus != NULL);
	assert(exponent != NULL);
	
	/* DER-decode the blob */
	NSS_RSAPublicKeyPKCS1 nssPubKey;
	SecNssCoder coder;
	
	memset(&nssPubKey, 0, sizeof(nssPubKey));
	PRErrorCode perr = coder.decode(blob->data, blob->length, 
		kSecAsn1RSAPublicKeyPKCS1Template, &nssPubKey);
	if(perr) {
		return errSSLBadCert;
	}

	/* malloc & copy components */
	srtn = SSLCopyBufferFromData(nssPubKey.modulus.Data,
		nssPubKey.modulus.Length, *modulus);
	if(srtn) {
		return srtn;
	}
	return SSLCopyBufferFromData(nssPubKey.publicExponent.Data,
		nssPubKey.publicExponent.Length, *exponent);
}

/*
 * Given a raw modulus and exponent, cook up a
 * BER-encoded RSA public key blob.
 */
OSStatus sslEncodeRsaBlob(
	const SSLBuffer	*modulus,		
	const SSLBuffer	*exponent,		
	SSLBuffer		*blob)			/* data mallocd and RETURNED */
{
	assert((modulus != NULL) && (exponent != NULL));
	blob->data = NULL;
	blob->length = 0;

	/* convert to NSS_RSAPublicKeyPKCS1 */
	NSS_RSAPublicKeyPKCS1 nssPubKey;
	SSLBUF_TO_CSSM(modulus, &nssPubKey.modulus);
	SSLBUF_TO_CSSM(exponent, &nssPubKey.publicExponent);
	
	/* DER encode */
	SecNssCoder coder;
	CSSM_DATA encBlob;
	PRErrorCode perr;
	perr = coder.encodeItem(&nssPubKey, kSecAsn1RSAPublicKeyPKCS1Template, encBlob);
	if(perr) {
		return memFullErr;
		
	}
	/* copy out to caller */
	return SSLCopyBufferFromData(encBlob.Data, encBlob.Length, *blob);
}

/*
 * Given a DER encoded DHParameterBlock, extract the prime and generator. 
 * modulus and public exponent.
 * This will work with either PKCS-1 encoded DHParameterBlock or
 * openssl-style DHParameter.
 */
OSStatus sslDecodeDhParams(
	const SSLBuffer	*blob,			/* PKCS-1 encoded */
	SSLBuffer		*prime,			/* data mallocd and RETURNED */
	SSLBuffer		*generator)		/* data mallocd and RETURNED */
{
	assert(blob != NULL);
	assert(prime != NULL);
	assert(generator != NULL);
	
	PRErrorCode perr;
	NSS_DHParameterBlock paramBlock;
	SecNssCoder coder;
	CSSM_DATA cblob;
	
	memset(&paramBlock, 0, sizeof(paramBlock));
	SSLBUF_TO_CSSM(blob, &cblob);
	
	/*
	 * Since the common case here is to decode a parameter block coming
	 * over the wire, which is in openssl format, let's try that format first.
	 */
	perr = coder.decodeItem(cblob, kSecAsn1DHParameterTemplate, 
		&paramBlock.params);
	if(perr) {
		/*
		 * OK, that failed when trying as a CDSA_formatted parameter
		 * block DHParameterBlock). Openssl uses a subset of that,
		 * a DHParameter. Try that instead.
		 */
		memset(&paramBlock, 0, sizeof(paramBlock));
		perr = coder.decodeItem(cblob, kSecAsn1DHParameterBlockTemplate, 
			&paramBlock);
		if(perr) {
			/* Ah well, we tried. */
			sslErrorLog("sslDecodeDhParams: both CDSA and openssl format"
				"failed\n");
			return errSSLCrypto;
		}
	}

	/* copy out components */
	NSS_DHParameter &param = paramBlock.params;
	OSStatus ortn = SSLCopyBufferFromData(param.prime.Data,
		param.prime.Length, *prime);
	if(ortn) {
		return ortn;
	}
	return SSLCopyBufferFromData(param.base.Data,
		param.base.Length, *generator);
}

/*
 * Given a prime and generator, cook up a BER-encoded DHParameter blob.
 */
OSStatus sslEncodeDhParams(
	const SSLBuffer	*prime,		
	const SSLBuffer	*generator,		
	SSLBuffer		*blob)			/* data mallocd and RETURNED */
{
	assert((prime != NULL) && (generator != NULL));
	blob->data = NULL;
	blob->length = 0;

	/* convert to NSS_DHParameter */
	NSS_DHParameter dhParams;
	SSLBUF_TO_CSSM(prime, &dhParams.prime);
	SSLBUF_TO_CSSM(generator, &dhParams.base);
	dhParams.privateValueLength.Data = NULL;
	dhParams.privateValueLength.Length = 0;
	
	/* DER encode */
	SecNssCoder coder;
	CSSM_DATA encBlob;
	PRErrorCode perr;
	perr = coder.encodeItem(&dhParams, kSecAsn1DHParameterTemplate, encBlob);
	if(perr) {
		return memFullErr;
		
	}
	/* copy out to caller */
	return SSLCopyBufferFromData(encBlob.Data, encBlob.Length, *blob);
}

