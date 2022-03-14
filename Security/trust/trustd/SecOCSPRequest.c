/*
 * Copyright (c) 2008-2021 Apple Inc. All Rights Reserved.
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
 * SecOCSPRequest.c - Trust policies dealing with certificate revocation.
 */

#include "trust/trustd/SecOCSPRequest.h"
#include <Security/SecCertificateInternal.h>
#include <AssertMacros.h>
#include <utilities/debugging.h>
#include <security_asn1/SecAsn1Coder.h>
#include <security_asn1/ocspTemplates.h>
#include <security_asn1/oidsalg.h>
#include <CommonCrypto/CommonDigest.h>
#include <stdlib.h>
#include "SecInternal.h"

#include <libDER/libDER.h>
#include <libDER/asn1Types.h>
#include <libDER/DER_Decode.h>
#include <libDER/oids.h>

/* MARK: libDER templates */

/*
   OCSPRequest     ::=     SEQUENCE {
       tbsRequest                  TBSRequest,
       optionalSignature   [0]     EXPLICIT Signature OPTIONAL }
 */
typedef struct {
    DERItem        tbs;
    DERItem        sig;
} DER_OCSPRequest;

const DERItemSpec DER_OCSPRequestItemSpecs[] =
{
    { DER_OFFSET(DER_OCSPRequest, tbs),
        ASN1_CONSTR_SEQUENCE,
        DER_DEC_NO_OPTS },
    { DER_OFFSET(DER_OCSPRequest, sig),
        ASN1_BIT_STRING,
        DER_DEC_OPTIONAL }
};

const DERSize DERNumOCSPRequestItemSpecs =
    sizeof(DER_OCSPRequestItemSpecs) / sizeof(DERItemSpec);

/*
 TBSRequest      ::=     SEQUENCE {
     version             [0]     EXPLICIT Version DEFAULT v1,
     requestorName       [1]     EXPLICIT GeneralName OPTIONAL,
     requestList                 SEQUENCE OF Request,
     requestExtensions   [2]     EXPLICIT Extensions OPTIONAL }
 */
typedef struct {
    DERItem        version;
    DERItem        requestorName;
    DERItem        requestList;
    DERItem        requestExtensions;
} DER_TBS_OCSPRequest;

const DERItemSpec DER_TBS_OCSPRequestItemSpecs[] =
{
    { DER_OFFSET(DER_TBS_OCSPRequest, version),
        ASN1_CONSTRUCTED | ASN1_CONTEXT_SPECIFIC | 0,
        DER_DEC_OPTIONAL },
    { DER_OFFSET(DER_TBS_OCSPRequest, requestorName),
        ASN1_CONSTRUCTED | ASN1_CONTEXT_SPECIFIC | 1,
        DER_DEC_OPTIONAL },
    { DER_OFFSET(DER_TBS_OCSPRequest, requestList),
        ASN1_CONSTR_SEQUENCE,
        DER_DEC_NO_OPTS  },
    { DER_OFFSET(DER_TBS_OCSPRequest, requestExtensions),
        ASN1_CONSTRUCTED | ASN1_CONTEXT_SPECIFIC | 3,
        DER_DEC_OPTIONAL }
};
const DERSize DERNumTBS_OCSPRequestItemSpecs = sizeof(DER_TBS_OCSPRequestItemSpecs) / sizeof(DERItemSpec);

/*
 Signature       ::=     SEQUENCE {
     signatureAlgorithm      AlgorithmIdentifier,
     signature               BIT STRING,
     certs               [0] EXPLICIT SEQUENCE OF Certificate OPTIONAL}
 */
typedef struct {
    DERItem        signatureAlgorithm;
    DERItem        signature;
    DERItem        certs;
} DER_OCSPSignature;

const DERItemSpec DER_OCSPSignatureItemSpecs[] =
{
    { DER_OFFSET(DER_OCSPSignature, signatureAlgorithm),
        ASN1_CONSTR_SEQUENCE,
        DER_DEC_NO_OPTS},
    { DER_OFFSET(DER_OCSPSignature, signature),
        ASN1_BIT_STRING,
        DER_DEC_NO_OPTS },
    { DER_OFFSET(DER_OCSPSignature, certs),
        ASN1_CONSTRUCTED | ASN1_CONTEXT_SPECIFIC | 0,
        DER_DEC_OPTIONAL }
};

const DERSize DERNumOCSPSignatureItemSpecs =
    sizeof(DER_OCSPSignatureItemSpecs) / sizeof(DERItemSpec);

/*
 Request         ::=     SEQUENCE {
     reqCert                     CertID,
     singleRequestExtensions     [0] EXPLICIT Extensions OPTIONAL }
 */
typedef struct {
    DERItem        reqCert;
    DERItem        singleRequestExtensions;
} DER_OCSPSingleRequest;

const DERItemSpec DER_OCSPSingleRequestItemSpecs[] =
{
    { DER_OFFSET(DER_OCSPSingleRequest, reqCert),
        ASN1_CONSTR_SEQUENCE,
        DER_DEC_NO_OPTS},
    { DER_OFFSET(DER_OCSPSingleRequest, singleRequestExtensions),
        ASN1_CONSTRUCTED | ASN1_CONTEXT_SPECIFIC | 0,
        DER_DEC_OPTIONAL }
};

const DERSize DERNumOCSPSingleRequestItemSpecs =
    sizeof(DER_OCSPSingleRequestItemSpecs) / sizeof(DERItemSpec);

/*
 CertID          ::=     SEQUENCE {
     hashAlgorithm       AlgorithmIdentifier,
     issuerNameHash      OCTET STRING, -- Hash of Issuer's DN
     issuerKeyHash       OCTET STRING, -- Hash of Issuers public key
     serialNumber        CertificateSerialNumber }
 */
typedef struct {
    DERItem        hashAlgorithm;
    DERItem        issuerNameHash;
    DERItem        issuerKeyHash;
    DERItem        serialNumber;
} DER_OCSPCertID;

const DERItemSpec DER_OCSPCertIDItemSpecs[] =
{
    { DER_OFFSET(DER_OCSPCertID, hashAlgorithm),
        ASN1_CONSTR_SEQUENCE,
        DER_DEC_NO_OPTS},
    { DER_OFFSET(DER_OCSPCertID, issuerNameHash),
        ASN1_OCTET_STRING,
        DER_DEC_NO_OPTS},
    { DER_OFFSET(DER_OCSPCertID, issuerKeyHash),
        ASN1_OCTET_STRING,
        DER_DEC_NO_OPTS},
    { DER_OFFSET(DER_OCSPCertID, serialNumber),
        ASN1_INTEGER,
        DER_DEC_NO_OPTS},
};

const DERSize DERNumOCSPCertIDItemSpecs =
    sizeof(DER_OCSPCertIDItemSpecs) / sizeof(DERItemSpec);

/* MARK: SecOCSPRequest Decoder
 * For testing purposes */
static DERReturn SecOCSPRequestParse(SecOCSPRequestRef request, bool strict) {
    DERReturn drtn = DR_GenericErr;
    if (!request->der) {
        return DR_ParamErr;
    }

    DERItem derRequest = { .data = (DERByte*)CFDataGetBytePtr(request->der),
                           .length = (DERSize)CFDataGetLength(request->der), };

    DER_OCSPRequest signedRequest;
    drtn = DERParseSequence(&derRequest,
                            DERNumOCSPRequestItemSpecs, DER_OCSPRequestItemSpecs,
                            &signedRequest, sizeof(signedRequest));
    require_noerr(drtn, badRequest);

    /* Explicitly disallow signatures, since adding these would cause Internet chaos. */
    require_action(!signedRequest.sig.data && signedRequest.sig.length == 0, badRequest, drtn = DR_Unimplemented);

    DER_TBS_OCSPRequest tbsRequest;
    drtn = DERParseSequenceContent(&signedRequest.tbs,
                                   DERNumTBS_OCSPRequestItemSpecs, DER_TBS_OCSPRequestItemSpecs,
                                   &tbsRequest, sizeof(tbsRequest));
    require_noerr(drtn, badRequest);

    /* DEFAULT 0, so should not be specified, but if specified (non-strict mode), must be 0 */
    if (strict) {
        require_action(tbsRequest.version.length == 0, badRequest, drtn = DR_DecodeError);
    } else if (tbsRequest.version.length) {
        DERLong version = 0;
        drtn = DERParseInteger64(&tbsRequest.version, &version);
        require_noerr(drtn, badRequest);
        require_action(version == 0, badRequest, drtn = DR_Unimplemented);
    }

    /* Explicitly disallow requestorName */
    require_action(!tbsRequest.requestorName.data && tbsRequest.requestorName.length == 0, badRequest, drtn = DR_Unimplemented);

    /* Decode the list of requests */
    drtn = DERDecodeSequenceContentWithBlock(&tbsRequest.requestList, ^DERReturn(DERDecodedInfo *content, bool *stop) {
        DERReturn blockDrtn = DR_UnexpectedTag;

        require(content->tag == ASN1_CONSTR_SEQUENCE, badSingleRequest);
        DER_OCSPSingleRequest singleRequest;
        blockDrtn = DERParseSequenceContent(&content->content,
                                            DERNumOCSPSingleRequestItemSpecs, DER_OCSPSingleRequestItemSpecs,
                                            &singleRequest, sizeof(singleRequest));
        require_noerr(blockDrtn, badSingleRequest);

        DER_OCSPCertID certId;
        blockDrtn = DERParseSequenceContent(&singleRequest.reqCert,
                                            DERNumOCSPCertIDItemSpecs, DER_OCSPCertIDItemSpecs,
                                            &certId, sizeof(certId));
        require_noerr(blockDrtn, badSingleRequest);

        DERAlgorithmId algId;
        blockDrtn = DERParseSequenceContent(&certId.hashAlgorithm,
                                     DERNumAlgorithmIdItemSpecs, DERAlgorithmIdItemSpecs,
                                     &algId, sizeof(algId));
        require_noerr(blockDrtn, badSingleRequest);

        if (DEROidCompare(&oidSha1, &algId.oid)) {
            require_action(certId.issuerNameHash.length == CC_SHA1_DIGEST_LENGTH, badSingleRequest, blockDrtn = DR_GenericErr);
            require_action(certId.issuerKeyHash.length == CC_SHA1_DIGEST_LENGTH, badSingleRequest, blockDrtn = DR_GenericErr);
        } else if (DEROidCompare(&oidSha256, &algId.oid)) {
            require_action(certId.issuerNameHash.length == CC_SHA256_DIGEST_LENGTH, badSingleRequest, blockDrtn = DR_GenericErr);
            require_action(certId.issuerKeyHash.length == CC_SHA256_DIGEST_LENGTH, badSingleRequest, blockDrtn = DR_GenericErr);
        } else {
            return DR_Unimplemented;
        }

        /* The hash algorithms supported are supposed to use absent parameters */
        require_action(!algId.params.data && algId.params.length == 0, badSingleRequest, blockDrtn = DR_DecodeError);

        /* Disallow extensions for now */
        require_action(!singleRequest.singleRequestExtensions.data &&
                      singleRequest.singleRequestExtensions.length == 0,
                      badSingleRequest, blockDrtn = DR_Unimplemented);

    badSingleRequest:
        return blockDrtn;
    });
    require_noerr(drtn, badRequest);

    /* Disallow extensions for now */
    require_action(!tbsRequest.requestExtensions.data && tbsRequest.requestExtensions.length == 0,
                   badRequest, drtn = DR_Unimplemented);

badRequest:
    return drtn;
}

SecOCSPRequestRef SecOCSPRequestCreateWithData(CFDataRef der_ocsp_request) {
    SecOCSPRequestRef result = SecOCSPRequestCreate(NULL, NULL);
    result->der = CFRetainSafe(der_ocsp_request);

    if (DR_Success != SecOCSPRequestParse(result, false)) {
        SecOCSPRequestFinalize(result);
        return NULL;
    }

    return result;
}

SecOCSPRequestRef SecOCSPRequestCreateWithDataStrict(CFDataRef der_ocsp_request) {
    SecOCSPRequestRef result = SecOCSPRequestCreate(NULL, NULL);
    result->der = CFRetainSafe(der_ocsp_request);

    if (DR_Success != SecOCSPRequestParse(result, true)) {
        SecOCSPRequestFinalize(result);
        return NULL;
    }

    return result;
}

/* MARK: SecOCSPRequest Encoder */

static CFDataRef _SecOCSPRequestCopyDEREncoding(SecOCSPRequestRef this) {
	/* fields obtained from issuer */
	SecAsn1OCSPSignedRequest	signedReq = {};
	SecAsn1OCSPTbsRequest		*tbs = &signedReq.tbsRequest;
	SecAsn1OCSPRequest			singleReq = {};
	SecAsn1OCSPCertID			*certId = &singleReq.reqCert;
	SecAsn1OCSPRequest			*reqArray[2] = { &singleReq, NULL };
    CFDataRef                   der = NULL;
    SecAsn1CoderRef             coder = NULL;
    CFDataRef                   issuerNameDigest;
    CFDataRef                   serial;
    CFDataRef                   issuerPubKeyDigest;


    /* algId refers to the hash we'll perform in issuer name and key */
    certId->algId.algorithm = CSSMOID_SHA1;

    /* @@@ Change this from using SecCertificateCopyIssuerSHA1Digest() /
       SecCertificateCopyPublicKeySHA1Digest() to
       SecCertificateCopyIssuerSequence() / SecCertificateGetPublicKeyData()
       and call SecDigestCreate here instead. */
    issuerNameDigest = SecCertificateCopyIssuerSHA1Digest(this->certificate);
    issuerPubKeyDigest = SecCertificateCopyPublicKeySHA1Digest(this->issuer);
    serial = SecCertificateCopySerialNumberData(this->certificate, NULL);
    require(CFDataGetLength(serial) > 0, errOut);

	/* build the CertID from those components */
	certId->issuerNameHash.Length = CC_SHA1_DIGEST_LENGTH;
	certId->issuerNameHash.Data = (uint8_t *)CFDataGetBytePtr(issuerNameDigest);
	certId->issuerPubKeyHash.Length = CC_SHA1_DIGEST_LENGTH;
	certId->issuerPubKeyHash.Data = (uint8_t *)CFDataGetBytePtr(issuerPubKeyDigest);
	certId->serialNumber.Length = (size_t)CFDataGetLength(serial);
	certId->serialNumber.Data = (uint8_t *)CFDataGetBytePtr(serial);

	/* Build top level request with one entry in requestList, no signature,
       and no optional extensions. */
	tbs->version = NULL;
	tbs->requestList = reqArray;

	/* Encode the request. */
    require_noerr(SecAsn1CoderCreate(&coder), errOut);
    SecAsn1Item encoded;
	require_noerr(SecAsn1EncodeItem(coder, &signedReq,
        kSecAsn1OCSPSignedRequestTemplate, &encoded), errOut);
    require(encoded.Length < LONG_MAX, errOut);
    der = CFDataCreate(kCFAllocatorDefault, encoded.Data, (CFIndex)encoded.Length);

errOut:
    if (coder)
        SecAsn1CoderRelease(coder);
    CFReleaseSafe(issuerNameDigest);
    CFReleaseSafe(serial);
    CFReleaseSafe(issuerPubKeyDigest);

    return der;
}

SecOCSPRequestRef SecOCSPRequestCreate(SecCertificateRef certificate,
    SecCertificateRef issuer) {
    SecOCSPRequestRef this;
    require(this = (SecOCSPRequestRef)calloc(1, sizeof(struct __SecOCSPRequest)),
        errOut);
    this->certificate = certificate;
    this->issuer = issuer;

    return this;
errOut:
    if (this) {
        SecOCSPRequestFinalize(this);
    }
    return NULL;
}

CFDataRef SecOCSPRequestGetDER(SecOCSPRequestRef this) {
    if (!this) { return NULL; }
    CFDataRef der = this->der;
    if (!der) {
        this->der = der = _SecOCSPRequestCopyDEREncoding(this);
    }
    return der;
}

void SecOCSPRequestFinalize(SecOCSPRequestRef this) {
    CFReleaseSafe(this->der);
    free(this);
}

