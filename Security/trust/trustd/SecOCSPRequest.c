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
#include <CommonCrypto/CommonDigest.h>
#include <stdlib.h>
#include "SecInternal.h"

#include <libDER/libDER.h>
#include <libDER/asn1Types.h>
#include <libDER/DER_Decode.h>
#include <libDER/DER_Encode.h>
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
        DER_ENC_WRITE_DER },
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
        ASN1_CONSTRUCTED | ASN1_CONTEXT_SPECIFIC | 2,
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
        DER_ENC_WRITE_DER},
    { DER_OFFSET(DER_OCSPSingleRequest, singleRequestExtensions),
        ASN1_CONSTRUCTED | ASN1_CONTEXT_SPECIFIC | 0,
        DER_DEC_OPTIONAL }
};

const DERSize DERNumOCSPSingleRequestItemSpecs =
    sizeof(DER_OCSPSingleRequestItemSpecs) / sizeof(DERItemSpec);

const DERItemSpec DER_OCSPCertIDItemSpecs[] =
{
    { DER_OFFSET(DER_OCSPCertID, hashAlgorithm),
        ASN1_CONSTR_SEQUENCE,
        DER_ENC_WRITE_DER},
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

        request->certIdHash.data = algId.oid.data;
        request->certIdHash.length = algId.oid.length;
        request->issuerNameDigest = CFDataCreate(NULL, certId.issuerNameHash.data, (CFIndex)certId.issuerNameHash.length);
        request->issuerPubKeyDigest = CFDataCreate(NULL, certId.issuerKeyHash.data, (CFIndex)certId.issuerKeyHash.length);
        request->serial = CFDataCreate(NULL, certId.serialNumber.data, (CFIndex)certId.serialNumber.length);

        /* The hash algorithms supported are supposed to use absent parameters */
        if (strict) {
            require_action(!algId.params.data && algId.params.length == 0, badSingleRequest, blockDrtn = DR_DecodeError);
        }

        /* Disallow extensions for now */
        require_action(!singleRequest.singleRequestExtensions.data &&
                      singleRequest.singleRequestExtensions.length == 0,
                      badSingleRequest, blockDrtn = DR_Unimplemented);

    badSingleRequest:
        return blockDrtn;
    });
    require_noerr(drtn, badRequest);

    // Skip extensions

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
static CF_RETURNS_RETAINED CFDataRef _SecOCSPRequestEncodeHashAlgorithm(void) {
    CFMutableDataRef result = NULL;
    DERAlgorithmId hashAlg;
    memset(&hashAlg, 0, sizeof(hashAlg));

    hashAlg.oid.data = oidSha1.data;
    hashAlg.oid.length = oidSha1.length;
    size_t hashAlgSize = 0;
    require_noerr(DERLengthOfEncodedSequenceFromObject(ASN1_CONSTR_SEQUENCE, &hashAlg, sizeof(hashAlg),
                                                       (DERShort)DERNumAlgorithmIdItemSpecs, DERAlgorithmIdItemSpecs,
                                                       &hashAlgSize),
                  errOut);
    require(hashAlgSize < LONG_MAX, errOut);
    result = CFDataCreateMutable(NULL, (CFIndex)hashAlgSize);
    require(result, errOut);
    CFDataSetLength(result, (CFIndex)hashAlgSize);
    require_noerr(DEREncodeSequenceFromObject(ASN1_CONSTR_SEQUENCE, &hashAlg, sizeof(hashAlg),
                                              (DERShort)DERNumAlgorithmIdItemSpecs, DERAlgorithmIdItemSpecs,
                                              CFDataGetMutableBytePtr(result), (size_t)CFDataGetLength(result), &hashAlgSize),
                  errOut);

    return result;

errOut:
    CFReleaseNull(result);
    return NULL;
}

static CF_RETURNS_RETAINED CFDataRef _SecOCSPRequestEncodeCertId(SecOCSPRequestRef request) {
    CFMutableDataRef result = NULL;
    CFDataRef hashAlg = NULL;
    CFDataRef issuerNameDigest = NULL;
    CFDataRef issuerPubKeyDigest = NULL;
    CFDataRef serial = NULL;
    DER_OCSPCertID certId;
    memset(&certId, 0, sizeof(certId));

    hashAlg = _SecOCSPRequestEncodeHashAlgorithm();
    require(hashAlg && (CFDataGetLength(hashAlg) > 0), errOut);
    issuerNameDigest = SecCertificateCopyIssuerSHA1Digest(request->certificate);
    require(issuerNameDigest && (CFDataGetLength(issuerNameDigest) > 0), errOut);
    issuerPubKeyDigest = SecCertificateCopyPublicKeySHA1Digest(request->issuer);
    require(issuerPubKeyDigest && (CFDataGetLength(issuerPubKeyDigest) > 0), errOut);
    serial = SecCertificateCopySerialNumberData(request->certificate, NULL);
    require(serial && CFDataGetLength(serial) > 0, errOut);

    request->issuerNameDigest = CFRetainSafe(issuerNameDigest);
    request->issuerPubKeyDigest = CFRetainSafe(issuerPubKeyDigest);
    request->serial = CFRetainSafe(serial);

    certId.hashAlgorithm.data = (DERByte *)CFDataGetBytePtr(hashAlg);
    certId.hashAlgorithm.length = (size_t)CFDataGetLength(hashAlg);
    certId.issuerNameHash.data = (DERByte *)CFDataGetBytePtr(issuerNameDigest);
    certId.issuerNameHash.length = (size_t)CFDataGetLength(issuerNameDigest);
    certId.issuerKeyHash.data = (DERByte *)CFDataGetBytePtr(issuerPubKeyDigest);
    certId.issuerKeyHash.length = (size_t)CFDataGetLength(issuerPubKeyDigest);
    certId.serialNumber.data = (DERByte *)CFDataGetBytePtr(serial);
    certId.serialNumber.length = (size_t)CFDataGetLength(serial);

    size_t certIdLen = 0;
    require_noerr(DERLengthOfEncodedSequenceFromObject(ASN1_CONSTR_SEQUENCE, &certId, sizeof(certId), (DERShort)DERNumOCSPCertIDItemSpecs, DER_OCSPCertIDItemSpecs, &certIdLen), errOut);
    require(certIdLen < LONG_MAX, errOut);
    result = CFDataCreateMutable(NULL, (CFIndex)certIdLen);
    require(result, errOut);
    CFDataSetLength(result, (CFIndex)certIdLen);
    require_noerr_action(DEREncodeSequenceFromObject(ASN1_CONSTR_SEQUENCE, &certId, sizeof(certId),
                                                     (DERShort)DERNumOCSPCertIDItemSpecs, DER_OCSPCertIDItemSpecs,
                                                     CFDataGetMutableBytePtr(result), (size_t)CFDataGetLength(result), &certIdLen),
                         errOut, CFReleaseNull(result));

errOut:
    CFReleaseNull(issuerNameDigest);
    CFReleaseNull(issuerPubKeyDigest);
    CFReleaseNull(serial);
    CFReleaseNull(hashAlg);
    return result;
}

static CF_RETURNS_RETAINED CFDataRef _SecOCSPRequestEncodeSingleRequest(SecOCSPRequestRef request) {
    CFMutableDataRef result = NULL;
    DER_OCSPSingleRequest singleRequest;
    memset(&singleRequest, 0, sizeof(singleRequest));

    CFDataRef certId = _SecOCSPRequestEncodeCertId(request);
    require(certId && (CFDataGetLength(certId) > 0), errOut);
    singleRequest.reqCert.data = (DERByte *)CFDataGetBytePtr(certId);
    singleRequest.reqCert.length = (size_t)CFDataGetLength(certId);

    size_t reqLen = 0;
    require_noerr(DERLengthOfEncodedSequenceFromObject(ASN1_CONSTR_SEQUENCE, &singleRequest, sizeof(singleRequest), (DERShort)DERNumOCSPSingleRequestItemSpecs, DER_OCSPSingleRequestItemSpecs, &reqLen), errOut);
    require(reqLen < LONG_MAX, errOut);
    result = CFDataCreateMutable(NULL, (CFIndex)reqLen);
    require(result, errOut);
    CFDataSetLength(result, (CFIndex)reqLen);
    require_noerr_action(DEREncodeSequenceFromObject(ASN1_CONSTR_SEQUENCE, &singleRequest, sizeof(singleRequest),
                                                     (DERShort)DERNumOCSPSingleRequestItemSpecs, DER_OCSPSingleRequestItemSpecs,
                                                     CFDataGetMutableBytePtr(result), (size_t)CFDataGetLength(result), &reqLen),
                         errOut, CFReleaseNull(result));

errOut:
    CFReleaseNull(certId);
    return result;
}


static CF_RETURNS_RETAINED CFDataRef _SecOCSPRequestEncodeTBSRequest(SecOCSPRequestRef request) {
    CFMutableDataRef result = NULL;
    DER_TBS_OCSPRequest tbsRequest;
    memset(&tbsRequest, 0, sizeof(tbsRequest));

    CFDataRef requestList = _SecOCSPRequestEncodeSingleRequest(request);
    require(requestList && (CFDataGetLength(requestList) > 0), errOut);
    tbsRequest.requestList.data = (DERByte *)CFDataGetBytePtr(requestList);
    tbsRequest.requestList.length = (size_t)CFDataGetLength(requestList);

    size_t tbsLen = 0;
    require_noerr(DERLengthOfEncodedSequenceFromObject(ASN1_CONSTR_SEQUENCE, &tbsRequest, sizeof(tbsRequest), (DERShort)DERNumTBS_OCSPRequestItemSpecs, DER_TBS_OCSPRequestItemSpecs, &tbsLen), errOut);
    require(tbsLen < LONG_MAX, errOut);
    result = CFDataCreateMutable(NULL, (CFIndex)tbsLen);
    require(result, errOut);
    CFDataSetLength(result, (CFIndex)tbsLen);
    require_noerr_action(DEREncodeSequenceFromObject(ASN1_CONSTR_SEQUENCE, &tbsRequest, sizeof(tbsRequest),
                                                        (DERShort)DERNumTBS_OCSPRequestItemSpecs, DER_TBS_OCSPRequestItemSpecs,
                                                     CFDataGetMutableBytePtr(result), (size_t)CFDataGetLength(result), &tbsLen),
                         errOut, CFReleaseNull(result));

errOut:
    CFReleaseNull(requestList);
    return result;
}

static CFDataRef _SecOCSPRequestCopyDEREncoding(SecOCSPRequestRef request) {
    CFMutableDataRef result = NULL;
    DER_OCSPRequest ocspRequestStr;
    memset(&ocspRequestStr, 0, sizeof(ocspRequestStr));

    CFDataRef tbsRequest = _SecOCSPRequestEncodeTBSRequest(request);
    require(tbsRequest && CFDataGetLength(tbsRequest) > 0, errOut);
    ocspRequestStr.tbs.data = (DERByte *)CFDataGetBytePtr(tbsRequest);
    ocspRequestStr.tbs.length = (size_t)CFDataGetLength(tbsRequest);

    size_t reqLen = 0;
    require_noerr(DERLengthOfEncodedSequenceFromObject(ASN1_CONSTR_SEQUENCE, &ocspRequestStr, sizeof(ocspRequestStr), (DERShort)DERNumOCSPRequestItemSpecs, DER_OCSPRequestItemSpecs, &reqLen), errOut);
    require(reqLen < LONG_MAX, errOut);
    result = CFDataCreateMutable(NULL, (CFIndex)reqLen);
    require(result, errOut);
    CFDataSetLength(result, (CFIndex)reqLen);
    require_noerr_action(DEREncodeSequenceFromObject(ASN1_CONSTR_SEQUENCE, &ocspRequestStr, sizeof(ocspRequestStr),
                                                     (DERShort)DERNumOCSPRequestItemSpecs, DER_OCSPRequestItemSpecs,
                                                     CFDataGetMutableBytePtr(result), (size_t)CFDataGetLength(result), &reqLen),
                         errOut, CFReleaseNull(result));

errOut:
    CFReleaseNull(tbsRequest);
    CFDataRef immutableResult = NULL;
    if (result) {
        immutableResult = CFDataCreateCopy(NULL, result);
        CFReleaseNull(result);
    }
    return immutableResult;
}

SecOCSPRequestRef SecOCSPRequestCreate(SecCertificateRef certificate,
    SecCertificateRef issuer) {
    SecOCSPRequestRef this;
    require(this = (SecOCSPRequestRef)calloc(1, sizeof(struct __SecOCSPRequest)),
        errOut);
    this->certificate = CFRetainSafe(certificate);
    this->issuer = CFRetainSafe(issuer);

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
    if (!this) { return; }
    CFReleaseNull(this->issuerNameDigest);
    CFReleaseNull(this->issuerPubKeyDigest);
    CFReleaseNull(this->serial);
    CFReleaseNull(this->certificate);
    CFReleaseNull(this->issuer);
    CFReleaseNull(this->der);
    free(this);
}

