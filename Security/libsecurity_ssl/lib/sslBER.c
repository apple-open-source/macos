/*
 * Copyright (c) 1999-2001,2005-2007,2010-2012 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * sslBER.c - BER routines
 */

#if USE_CDSA_CRYPTO

#include "ssl.h"
#include "sslMemory.h"
#include "sslDebug.h"
#include "sslBER.h"
#include "sslCrypto.h"

#include <string.h>

#include "appleCdsa.h"
#include "SecureTransportPriv.h"

#include <string.h>
#include <Security/SecAsn1Coder.h>
#include <Security/keyTemplates.h>
#include <security_asn1/nssUtils.h>
#include <Security/oidsattr.h>
#include <Security/oidsalg.h>

/* we should get rid of this low level stuff and use SecAsn1Coder throughout... */
#include <security_asn1/secasn1.h>

#define SSLBUF_TO_SECITEM(sb, cd)  {		\
	(cd)->Length = (sb)->length; 		\
	(cd)->Data   = (sb)->data;			\
}

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
	SSLBUF_TO_SECITEM(modulus, &nssPubKey.modulus);
	SSLBUF_TO_SECITEM(exponent, &nssPubKey.publicExponent);

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

#if APPLE_DH
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
	SSLBUF_TO_SECITEM(prime, &dhParams.prime);
	SSLBUF_TO_SECITEM(generator, &dhParams.base);
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
#endif /* APPLE_DH */

/*
 * Given an ECDSA key in CSSM format, extract the SSL_ECDSA_NamedCurve
 * from its algorithm parameters.
 */
OSStatus sslEcdsaPeerCurve(
	CSSM_KEY_PTR pubKey,
	SSL_ECDSA_NamedCurve *namedCurve)
{
	SecAsn1CoderRef coder = NULL;
	CSSM_X509_SUBJECT_PUBLIC_KEY_INFO subjPubKeyInfo;
	CSSM_X509_ALGORITHM_IDENTIFIER *algId = &subjPubKeyInfo.algorithm;
	CSSM_OID curveOid;
	OSStatus ortn;

	CSSM_KEYHEADER *hdr = &pubKey->KeyHeader;
	if(hdr->AlgorithmId != CSSM_ALGID_ECDSA) {
	   sslErrorLog("sslEcdsaPeerCurve: bad peer key algorithm\n");
	   return errSSLProtocol;
	}
	if(hdr->BlobType != CSSM_KEYBLOB_RAW) {
		/* No can do - this must be raw format, it came from the CL */
	   sslErrorLog("sslEcdsaPeerCurve: bad peer key algorithm\n");
	   return errSSLProtocol;
	}
	if(hdr->Format != CSSM_KEYBLOB_RAW_FORMAT_X509) {
	   sslErrorLog("sslEcdsaPeerCurve: bad peer key format\n");
	   return errSSLProtocol;
	}

	/* KeyData is an encoded CSSM_X509_SUBJECT_PUBLIC_KEY_INFO */
	ortn = SecAsn1CoderCreate(&coder);
	if(ortn) {
		return errSSLInternal;
	}
	/* subsequent errors to errOut: */

	memset(&subjPubKeyInfo, 0, sizeof(subjPubKeyInfo));
	ortn = SecAsn1DecodeData(coder, &pubKey->KeyData, kSecAsn1SubjectPublicKeyInfoTemplate,
		&subjPubKeyInfo);
	if(ortn) {
		printf("sslEcdsaPeerCurve: error decoding public key\n");
		goto errOut;
	}

	if(!nssCompareCssmData(&algId->algorithm, &CSSMOID_ecPublicKey)) {
		printf("sslEcdsaPeerCurve: unexpected algorithm ID in public key\n");
		ortn = errSSLProtocol;
		goto errOut;
	}
	if((algId->parameters.Data[0] != BER_TAG_OID) ||
	   (algId->parameters.Length < 2)) {
		printf("sslEcdsaPeerCurve: missing algorithm parameters in public key\n");
		ortn = errSSLProtocol;
		goto errOut;
	}

	/*
	 * The curve OID is DER-encoded since the parameters are ASN_ANY.
	 * Quickie decode for further processing...
	 */
	curveOid.Data = algId->parameters.Data + 2;
	curveOid.Length = algId->parameters.Length - 2;

	/* algId->parameters is the curve OID */
	if(nssCompareCssmData(&curveOid, &CSSMOID_secp256r1)) {
		*namedCurve = SSL_Curve_secp256r1;
	}
	else if(nssCompareCssmData(&curveOid, &CSSMOID_secp384r1)) {
		*namedCurve = SSL_Curve_secp384r1;
	}
	else if(nssCompareCssmData(&curveOid, &CSSMOID_secp521r1)) {
		*namedCurve = SSL_Curve_secp521r1;
	}
	/* Others? Later. That's all we support for now. */
	else {
		printf("sslEcdsaPeerCurve: missing algorithm parameters in public key\n");
		ortn = errSSLProtocol;
	}

errOut:
	SecAsn1CoderRelease(coder);
	return ortn;
}

/*
 * Given an ECDSA public key in X509 format, extract the raw public key
 * bits in ECPOint format.
 */
OSStatus sslEcdsaPubKeyBits(
	CSSM_KEY_PTR	pubKey,
	SSLBuffer		*pubBits)		/* data mallocd and RETURNED */
{
	SecAsn1CoderRef coder = NULL;
	CSSM_X509_SUBJECT_PUBLIC_KEY_INFO subjPubKeyInfo;
	OSStatus ortn = noErr;

	CSSM_KEYHEADER *hdr = &pubKey->KeyHeader;
	if(hdr->AlgorithmId != CSSM_ALGID_ECDSA) {
	   sslErrorLog("sslEcdsaPubKeyBits: bad peer key algorithm\n");
	   return errSSLProtocol;
	}
	if(hdr->BlobType != CSSM_KEYBLOB_RAW) {
		/* No can do - this must be raw format, it came from the CL */
	   sslErrorLog("sslEcdsaPubKeyBits: bad peer key algorithm\n");
	   return errSSLProtocol;
	}
	if(hdr->Format != CSSM_KEYBLOB_RAW_FORMAT_X509) {
	   sslErrorLog("sslEcdsaPubKeyBits: bad peer key format\n");
	   return errSSLProtocol;
	}

	/* KeyData is an encoded CSSM_X509_SUBJECT_PUBLIC_KEY_INFO */
	ortn = SecAsn1CoderCreate(&coder);
	if(ortn) {
		return errSSLInternal;
	}
	/* subsequent errors to errOut: */

	memset(&subjPubKeyInfo, 0, sizeof(subjPubKeyInfo));
	ortn = SecAsn1DecodeData(coder, &pubKey->KeyData, kSecAsn1SubjectPublicKeyInfoTemplate,
		&subjPubKeyInfo);
	if(ortn) {
		printf("sslEcdsaPubKeyBits: error decoding public key\n");
		goto errOut;
	}
	/* that key data is a BITSTRING */
	ortn = SSLCopyBufferFromData(subjPubKeyInfo.subjectPublicKey.Data,
		subjPubKeyInfo.subjectPublicKey.Length >> 3, pubBits);
errOut:
	SecAsn1CoderRelease(coder);
	return ortn;
}

#endif /* USE_CDSA_CRYPTO */
