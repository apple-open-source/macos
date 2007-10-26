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
	File:		sslBER.c

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
//#include <security_asn1/SecNssCoder.h>
#include <Security/keyTemplates.h>
#include <security_asn1/secasn1.h>

/*
 * Given a PKCS-1 encoded RSA public key, extract the 
 * modulus and public exponent.
 *
 * RSAPublicKey ::= SEQUENCE {
 *		modulus INTEGER, -- n
 *		publicExponent INTEGER -- e }
 */

/* 
 * Default chunk size for new arena pool.
 * FIXME: analyze & measure different defaults here. I'm pretty sure
 * that only performance - not correct behavior - is affected by
 * an arena pool's chunk size.
 */
#define CHUNKSIZE_DEF		1024		

OSStatus sslDecodeRsaBlob(
	const SSLBuffer	*blob,			/* PKCS-1 encoded */
	SSLBuffer		*modulus,		/* data mallocd and RETURNED */
	SSLBuffer		*exponent)		/* data mallocd and RETURNED */
{
    SECStatus rv;
	OSStatus srtn;
	NSS_RSAPublicKeyPKCS1 nssPubKey = {};
    PLArenaPool *pool;

	assert(blob != NULL);
	assert(modulus != NULL);
	assert(exponent != NULL);

	/* DER-decode the blob */
    pool = PORT_NewArena(CHUNKSIZE_DEF);
    rv = SEC_ASN1Decode(pool, &nssPubKey,
        kSecAsn1RSAPublicKeyPKCS1Template, (const char *)blob->data, blob->length);
    if (rv != SECSuccess)
		srtn = errSSLBadCert;
    else {
        /* malloc & copy components */
        srtn = SSLCopyBufferFromData(nssPubKey.modulus.Data,
            nssPubKey.modulus.Length, modulus);
        if(!srtn) {
            srtn = SSLCopyBufferFromData(nssPubKey.publicExponent.Data,
                nssPubKey.publicExponent.Length, exponent);
        }
    }
    PORT_FreeArena(pool, PR_TRUE);
    return srtn;
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
    PLArenaPool *pool;
	OSStatus srtn;
    SECItem *encBlob, dest = {};
	NSS_RSAPublicKeyPKCS1 nssPubKey;

	assert((modulus != NULL) && (exponent != NULL));

	/* convert to NSS_RSAPublicKeyPKCS1 */
	SSLBUF_TO_CSSM(modulus, &nssPubKey.modulus);
	SSLBUF_TO_CSSM(exponent, &nssPubKey.publicExponent);

	/* DER encode */
    pool = PORT_NewArena(CHUNKSIZE_DEF);
    encBlob = SEC_ASN1EncodeItem(pool, &dest, &nssPubKey,
        kSecAsn1RSAPublicKeyPKCS1Template);
	if (!encBlob)
		srtn = memFullErr;
    else {
        /* copy out to caller */
        srtn = SSLCopyBufferFromData(encBlob->Data, encBlob->Length, blob);
    }

    PORT_FreeArena(pool, PR_TRUE);
    return srtn;
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
    SECStatus rv;
	OSStatus srtn;
	NSS_DHParameterBlock paramBlock = {};
    PLArenaPool *pool;

	assert(blob != NULL);
	assert(prime != NULL);
	assert(generator != NULL);

    pool = PORT_NewArena(CHUNKSIZE_DEF);
	/*
	 * Since the common case here is to decode a parameter block coming
	 * over the wire, which is in openssl format, let's try that format first.
	 */
    rv = SEC_ASN1Decode(pool, &paramBlock.params,
        kSecAsn1DHParameterTemplate, (const char *)blob->data, blob->length);
    if (rv != SECSuccess) {
		/*
		 * OK, that failed when trying as a CDSA_formatted parameter
		 * block DHParameterBlock). Openssl uses a subset of that,
		 * a DHParameter. Try that instead.
		 */
		memset(&paramBlock, 0, sizeof(paramBlock));
        rv = SEC_ASN1Decode(pool, &paramBlock,
            kSecAsn1DHParameterBlockTemplate,
            (const char *)blob->data, blob->length);
	}

    if (rv != SECSuccess) {
        /* Ah well, we tried. */
        sslErrorLog("sslDecodeDhParams: both CDSA and openssl format"
            "failed\n");
        srtn = errSSLCrypto;
    }
    else {
        /* copy out components */
        srtn = SSLCopyBufferFromData(paramBlock.params.prime.Data,
            paramBlock.params.prime.Length, prime);
        if(!srtn) {
            srtn = SSLCopyBufferFromData(paramBlock.params.base.Data,
                paramBlock.params.base.Length, generator);
        }
    }

    PORT_FreeArena(pool, PR_TRUE);
    return srtn;
}

/*
 * Given a prime and generator, cook up a BER-encoded DHParameter blob.
 */
OSStatus sslEncodeDhParams(
	const SSLBuffer	*prime,		
	const SSLBuffer	*generator,		
	SSLBuffer		*blob)			/* data mallocd and RETURNED */
{
    PLArenaPool *pool;
	OSStatus srtn;
    SECItem *encBlob, dest = {};
	NSS_DHParameter dhParams;

	assert((prime != NULL) && (generator != NULL));

	/* convert to NSS_DHParameter */
	SSLBUF_TO_CSSM(prime, &dhParams.prime);
	SSLBUF_TO_CSSM(generator, &dhParams.base);
	dhParams.privateValueLength.Data = NULL;
	dhParams.privateValueLength.Length = 0;

	/* DER encode */
    pool = PORT_NewArena(CHUNKSIZE_DEF);
    encBlob = SEC_ASN1EncodeItem(pool, &dest, &dhParams,
        kSecAsn1DHParameterTemplate);
	if (!encBlob)
		srtn = memFullErr;
    else {
        /* copy out to caller */
        srtn = SSLCopyBufferFromData(encBlob->Data, encBlob->Length, blob);
    }

    PORT_FreeArena(pool, PR_TRUE);
    return srtn;
}

