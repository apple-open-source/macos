/*
 * Copyright (c) 2008-2009,2012-2018 Apple Inc. All Rights Reserved.
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
 * SecOCSPResponse.c - Wrapper to decode ocsp responses.
 */

#include "trust/trustd/SecOCSPResponse.h"

#include <asl.h>
#include <AssertMacros.h>
#include <CommonCrypto/CommonDigest.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecFramework.h>
#include <Security/SecKeyPriv.h>
#include <security_asn1/SecAsn1Coder.h>
#include <security_asn1/SecAsn1Templates.h>
#include <security_asn1/ocspTemplates.h>
#include <security_asn1/oidsocsp.h>
#include <stdlib.h>
#include "SecInternal.h"
#include <utilities/SecCFWrappers.h>
#include <utilities/SecSCTUtils.h>

#include <libDER/libDER.h>
#include <libDER/asn1Types.h>
#include <libDER/DER_Decode.h>
#include <libDER/oids.h>
#include <libDER/DER_CertCrl.h>
#include <libDER/DER_Encode.h>

#define ocspdErrorLog(args, ...)     secerror(args, ## __VA_ARGS__)
#define ocspdHttpDebug(args...)     secdebug("ocspdHttp", ## args)
#define ocspdDebug(args...)     secdebug("ocsp", ## args)


/*
 OCSPResponse ::= SEQUENCE {
    responseStatus         OCSPResponseStatus,
    responseBytes          [0] EXPLICIT ResponseBytes OPTIONAL }
*/

typedef struct {
    DERItem responseStatus;
    DERItem responseBytes;
} DER_OCSPResponse;

const DERItemSpec DER_OCSPResponseItemSpecs[] =
{
    { DER_OFFSET(DER_OCSPResponse, responseStatus),
        ASN1_ENUMERATED,
        DER_DEC_NO_OPTS },
    { DER_OFFSET(DER_OCSPResponse, responseBytes),
        ASN1_CONSTRUCTED | ASN1_CONTEXT_SPECIFIC | 0,
        DER_DEC_OPTIONAL }
};

const DERSize DERNumOCSPResponseItemSpecs =
    sizeof(DER_OCSPResponseItemSpecs) / sizeof(DERItemSpec);

/*
 RESPONSE ::= TYPE-IDENTIFIER

 ResponseSet RESPONSE ::= {basicResponse, ...}

 ResponseBytes ::=       SEQUENCE {
     responseType        RESPONSE.
                             &id ({ResponseSet}),
     response            OCTET STRING (CONTAINING RESPONSE.
                             &Type({ResponseSet}{@responseType}))}
 */
typedef struct {
    DERItem responseType;
    DERItem response;
} DER_OCSPResponseBytes;

const DERItemSpec DER_OCSPResponseBytesItemSpecs[] =
{
    { DER_OFFSET(DER_OCSPResponseBytes, responseType),
        ASN1_OBJECT_ID,
        DER_DEC_NO_OPTS },
    { DER_OFFSET(DER_OCSPResponseBytes, response),
        ASN1_OCTET_STRING,
        DER_DEC_NO_OPTS }
};

const DERSize DERNumOCSPResponseBytesItemSpecs =
    sizeof(DER_OCSPResponseBytesItemSpecs) / sizeof(DERItemSpec);

/*
 basicResponse RESPONSE ::=
     { BasicOCSPResponse IDENTIFIED BY id-pkix-ocsp-basic }
 id-pkix-ocsp                 OBJECT IDENTIFIER ::= id-ad-ocsp
 id-pkix-ocsp-basic           OBJECT IDENTIFIER ::= { id-pkix-ocsp 1 }
 */
const DERByte _basicOCSPResponse[] = { 43, 6, 1, 5, 5, 7, 48, 1 , 1 };
const DERItem BasicOCSPResponse = { (DERByte *)_basicOCSPResponse,
                                    sizeof(_basicOCSPResponse) };

/*
 BasicOCSPResponse       ::= SEQUENCE {
    tbsResponseData      ResponseData,
    signatureAlgorithm   AlgorithmIdentifier{SIGNATURE-ALGORITHM,
                             {sa-dsaWithSHA1 | sa-rsaWithSHA1 |
                                  sa-rsaWithMD5 | sa-rsaWithMD2, ...}},
    signature            BIT STRING,
    certs            [0] EXPLICIT SEQUENCE OF Certificate OPTIONAL }
 */
const DERItemSpec DERBasicOCSPResponseItemSpecs[] =
{
    { DER_OFFSET(DERBasicOCSPResponse, responseData),
        ASN1_CONSTR_SEQUENCE,
        DER_DEC_NO_OPTS | DER_DEC_SAVE_DER },
    { DER_OFFSET(DERBasicOCSPResponse, signatureAlgorithm),
        ASN1_CONSTR_SEQUENCE,
        DER_DEC_NO_OPTS },
    { DER_OFFSET(DERBasicOCSPResponse, signature),
        ASN1_BIT_STRING,
        DER_DEC_NO_OPTS },
    { DER_OFFSET(DERBasicOCSPResponse, certs),
        ASN1_CONSTRUCTED | ASN1_CONTEXT_SPECIFIC | 0,
        DER_DEC_OPTIONAL },
};

const DERSize DERNumBasicOCSPResponseItemSpecs =
    sizeof(DERBasicOCSPResponseItemSpecs) / sizeof(DERItemSpec);

/*
 Version ::= INTEGER { v1(0) }

 ResponseData ::= SEQUENCE {
    version              [0] EXPLICIT Version DEFAULT v1,
    responderID              ResponderID,
    producedAt               GeneralizedTime,
    responses                SEQUENCE OF SingleResponse,
    responseExtensions   [1] EXPLICIT Extensions
                                {{re-ocsp-nonce, ...,
                                  re-ocsp-extended-revoke}} OPTIONAL }
*/

const DERItemSpec DER_OCSPResponseDataItemSpecs[] =
{
    { DER_OFFSET(DER_OCSPResponseData, version),
        ASN1_CONSTRUCTED | ASN1_CONTEXT_SPECIFIC | 0,
        DER_DEC_OPTIONAL },
    { DER_OFFSET(DER_OCSPResponseData, responderId),
        0,
        DER_DEC_ASN_ANY | DER_DEC_SAVE_DER },
    { DER_OFFSET(DER_OCSPResponseData, producedAt),
        ASN1_GENERALIZED_TIME,
        DER_DEC_NO_OPTS },
    { DER_OFFSET(DER_OCSPResponseData, responses),
        ASN1_CONSTR_SEQUENCE,
        DER_DEC_NO_OPTS },
    { DER_OFFSET(DER_OCSPResponseData, extensions),
        ASN1_CONSTRUCTED | ASN1_CONTEXT_SPECIFIC | 1,
        DER_DEC_OPTIONAL },
};

const DERSize DERNumOCSPResponseDataItemSpecs =
    sizeof(DER_OCSPResponseDataItemSpecs) / sizeof(DERItemSpec);


/*
 SingleResponse ::= SEQUENCE {
    certID                       CertID,
    certStatus                   CertStatus,
    thisUpdate                   GeneralizedTime,
    nextUpdate           [0]     EXPLICIT GeneralizedTime OPTIONAL,
    singleExtensions     [1]     EXPLICIT Extensions{{re-ocsp-crl |
                                              re-ocsp-archive-cutoff |
                                              CrlEntryExtensions, ...}
                                              } OPTIONAL }
 */
typedef struct {
    DERItem certId;
    DERItem certStatus;
    DERItem thisUpdate;
    DERItem nextUpdate;
    DERItem singleExtensions;
} DER_OCSPSingleResponse;

const DERItemSpec DER_OCSPSingleResponseItemSpecs[] =
{
    { DER_OFFSET(DER_OCSPSingleResponse, certId),
        ASN1_CONSTR_SEQUENCE,
        DER_DEC_NO_OPTS },
    { DER_OFFSET(DER_OCSPSingleResponse, certStatus),
        0,
        DER_DEC_ASN_ANY | DER_DEC_SAVE_DER },
    { DER_OFFSET(DER_OCSPSingleResponse, thisUpdate),
        ASN1_GENERALIZED_TIME,
        DER_DEC_NO_OPTS },
    { DER_OFFSET(DER_OCSPSingleResponse, nextUpdate),
        ASN1_CONSTRUCTED | ASN1_CONTEXT_SPECIFIC | 0,
        DER_DEC_OPTIONAL },
    { DER_OFFSET(DER_OCSPSingleResponse, singleExtensions),
        ASN1_CONSTRUCTED | ASN1_CONTEXT_SPECIFIC | 1,
        DER_DEC_OPTIONAL },
};

const DERSize DERNumOCSPSingleResponseItemSpecs =
    sizeof(DER_OCSPSingleResponseItemSpecs) / sizeof(DERItemSpec);

/*
 RevokedInfo ::= SEQUENCE {
     revocationTime              GeneralizedTime,
     revocationReason    [0]     EXPLICIT CRLReason OPTIONAL }
 */
typedef struct {
    DERItem revocationTime;
    DERItem revocationReason;
} DERRevokedInfo;

const DERItemSpec DERRevokedInfoItemSpecs[] =
{
    { DER_OFFSET(DERRevokedInfo, revocationTime),
        ASN1_GENERALIZED_TIME,
        DER_DEC_NO_OPTS },
    { DER_OFFSET(DERRevokedInfo, revocationReason),
        ASN1_CONSTRUCTED | ASN1_CONTEXT_SPECIFIC | 0,
        DER_DEC_OPTIONAL }
};

const DERSize DERNumRevokedInfoItemSpecs =
    sizeof(DERRevokedInfoItemSpecs) / sizeof(DERItemSpec);

void SecOCSPSingleResponseDestroy(SecOCSPSingleResponseRef this) {
    if (!this) { return; }
    CFReleaseSafe(this->scts);
    free(this);
}

static bool SecOCSPRevokedInfoParse(DERItem *revokedInfoBytes, SecOCSPSingleResponseRef singleResponseObj)
{
    bool result = false;
    DERReturn drtn = DR_GenericErr;
    DERRevokedInfo revokedInfo;

    drtn = DERParseSequenceContentToObject(revokedInfoBytes, DERNumRevokedInfoItemSpecs, DERRevokedInfoItemSpecs, &revokedInfo, sizeof(revokedInfo), sizeof(revokedInfo));
    require_noerr_action(drtn, badRevokedInfo, ocspdErrorLog("failed to parse RevokedInfo"));

    require_action(revokedInfo.revocationTime.data && revokedInfo.revocationTime.length > 0, badRevokedInfo,
                   ocspdErrorLog("RevokedInfo missing revocationTime"));
    CFErrorRef dateError = NULL;
    singleResponseObj->revokedTime = SecAbsoluteTimeFromDateContentWithError(ASN1_GENERALIZED_TIME, revokedInfo.revocationTime.data, revokedInfo.revocationTime.length, &dateError);
    require_action(dateError == NULL, badRevokedInfo, ocspdErrorLog("failed to decode revocationTime: %@", dateError));

    if (revokedInfo.revocationReason.data && revokedInfo.revocationReason.length > 0) {
        DERDecodedInfo revocationReason;
        drtn = DERDecodeItem(&revokedInfo.revocationReason, &revocationReason);
        require_noerr_action(drtn, badRevokedInfo, ocspdErrorLog("failed to parse revocation reason"));
        require_action(revocationReason.tag == ASN1_ENUMERATED && revocationReason.content.length == 1, badRevokedInfo,
                       ocspdErrorLog("failed to parse revocation reason"));
        singleResponseObj->crlReason = revocationReason.content.data[0];
    }

    result = true;

badRevokedInfo:
    return result;
}

/*
 CertStatus ::= CHOICE {
     good                [0]     IMPLICIT NULL,
     revoked             [1]     IMPLICIT RevokedInfo,
     unknown             [2]     IMPLICIT UnknownInfo }

 UnknownInfo ::= NULL
 */
static bool SecOCSPCertStatusParse(DERItem *certStatusBytes, SecOCSPSingleResponseRef singleResponseObj)
{
    bool result = false;
    require_action(certStatusBytes->data && certStatusBytes->length > 0, badCertStatus, ocspdErrorLog("missing certStatus in SingleResponse"));

    DERDecodedInfo decodedCertStatus;
    DERReturn drtn = DERDecodeItem(certStatusBytes, &decodedCertStatus);
    require_noerr_action(drtn, badCertStatus, ocspdErrorLog("failed to decode certStatus in SingleResponse"));
    switch (decodedCertStatus.tag) {
        case ASN1_CONTEXT_SPECIFIC | 0:
            singleResponseObj->certStatus = OCSPCertStatusGood;
            require_action(decodedCertStatus.content.length == 0, badCertStatus, ocspdErrorLog("invalid Good certStatus content"));
            break;
        case ASN1_CONSTRUCTED | ASN1_CONTEXT_SPECIFIC | 1:
            singleResponseObj->certStatus = OCSPCertStatusRevoked;
            require(SecOCSPRevokedInfoParse(&decodedCertStatus.content, singleResponseObj), badCertStatus);
            break;
        case ASN1_CONTEXT_SPECIFIC | 2:
            singleResponseObj->certStatus = OCSPCertStatusUnknown;
            require_action(decodedCertStatus.content.length == 0, badCertStatus, ocspdErrorLog("invalid Unknown certStatus content"));
            break;
        default:
            ocspdErrorLog("Unknown cert status: %llu", decodedCertStatus.tag);
            goto badCertStatus;
    }

    result = true;

badCertStatus:
    return result;
}

static bool SecOCSPNextUpdateParse(DERItem *nextUpdateBytes, SecOCSPSingleResponseRef singleResponseObj)
{
    bool result = false;
    DERDecodedInfo decodedNextUpdate;
    CFErrorRef dateError = NULL;
    DERReturn drtn = DR_GenericErr;

    drtn = DERDecodeItem(nextUpdateBytes, &decodedNextUpdate);
    require_noerr(drtn, badNextUpdate);
    require(decodedNextUpdate.tag == ASN1_GENERALIZED_TIME, badNextUpdate);
    singleResponseObj->nextUpdate = SecAbsoluteTimeFromDateContentWithError(ASN1_GENERALIZED_TIME, decodedNextUpdate.content.data, decodedNextUpdate.content.length, &dateError);
    require(dateError == NULL, badNextUpdate);
    return true;

badNextUpdate:
    ocspdErrorLog("failed to decode nextUpdate: %@", dateError);
    return result;
}

static bool SecOCSPSingleExtensionsParse(DERItem *singleExtensionsBytes, SecOCSPSingleResponseRef singleResponseObj)
{
    DERDecodedInfo extensions;
    if (DERDecodeItem(singleExtensionsBytes, &extensions) != DR_Success || extensions.tag != ASN1_CONSTR_SEQUENCE) {
        return false;
    }
    DERReturn extnStatus = DERDecodeSequenceWithBlock(singleExtensionsBytes, ^DERReturn(DERDecodedInfo *content, bool *stop) {
        if (content->tag != ASN1_CONSTR_SEQUENCE) {
            return DR_UnexpectedTag;
        }
        DERReturn drtn = DR_GenericErr;
        DERExtension extension;
        drtn = DERParseSequenceContentToObject(&content->content, DERNumExtensionItemSpecs, DERExtensionItemSpecs, &extension, sizeof(extension), sizeof(extension));
        if (drtn != DR_Success) {
            return drtn;
        }

        // We only examine the SCTs extension
        if (DEROidCompare(&extension.extnID, &oidGoogleOCSPSignedCertificateTimestamp)) {
            DERDecodedInfo decodedSct;
            drtn = DERDecodeItem(&extension.extnValue, &decodedSct);
            if (drtn != DR_Success) {
                return drtn;
            }
            if (decodedSct.tag != ASN1_OCTET_STRING) {
                return DR_UnexpectedTag;
            }
            if (singleResponseObj && !singleResponseObj->scts) {
                singleResponseObj->scts = SecCreateSignedCertificateTimestampsArrayFromSerializedSCTList(decodedSct.content.data, decodedSct.content.length);
            }
        }
        return DR_Success;
    });
    if (extnStatus != DR_Success) {
        return false;
    }
    return true;
}

static SecOCSPSingleResponseRef SecOCSPSingleResponseCreate(DER_OCSPSingleResponse *resp) {
	assert(resp != NULL);
    SecOCSPSingleResponseRef this;
    require(this = (SecOCSPSingleResponseRef)
        calloc(1, sizeof(struct __SecOCSPSingleResponse)), badSingleResponse);
    this->certStatus = CS_NotParsed;
	this->thisUpdate = NULL_TIME;
	this->nextUpdate = NULL_TIME;
	this->revokedTime = NULL_TIME;
	this->crlReason = kSecRevocationReasonUndetermined;
    this->scts = NULL;

    require(SecOCSPCertStatusParse(&resp->certStatus, this), badSingleResponse);

    require_action(resp->thisUpdate.data && resp->thisUpdate.length > 0, badSingleResponse,
                   ocspdErrorLog("SingleResponse missing thisUpdate"));
    CFErrorRef dateError = NULL;
    this->thisUpdate = SecAbsoluteTimeFromDateContentWithError(ASN1_GENERALIZED_TIME, resp->thisUpdate.data, resp->thisUpdate.length, &dateError);
    require_action(dateError == NULL, badSingleResponse, ocspdErrorLog("failed to decode thisUpdate: %@", dateError));

    if (resp->nextUpdate.data && resp->nextUpdate.length > 0) {
        require(SecOCSPNextUpdateParse(&resp->nextUpdate, this), badSingleResponse);
    }

    if (resp->singleExtensions.data && resp->singleExtensions.length > 0) {
        require(SecOCSPSingleExtensionsParse(&resp->singleExtensions, this), badSingleResponse);
    }

    ocspdDebug("status %d reason %d", (int)this->certStatus,
        (int)this->crlReason);
    return this;

badSingleResponse:
    if (this) {
        SecOCSPSingleResponseDestroy(this);
    }
    return NULL;
}

static bool SecOCSPResponseForDERSingleResponse(SecOCSPResponseRef response, DERReturn (^operation)(DER_OCSPSingleResponse *singleResponse, bool *stop))
{
    DERReturn drtn = DERDecodeSequenceContentWithBlock(&response->responseData.responses, ^DERReturn(DERDecodedInfo *content, bool *stop) {
        if (content->tag != ASN1_CONSTR_SEQUENCE) {
            return DR_UnexpectedTag;
        }
        DER_OCSPSingleResponse singleResponse;
        DERReturn innerDrtn = DERParseSequenceContentToObject(&content->content, DERNumOCSPSingleResponseItemSpecs, DER_OCSPSingleResponseItemSpecs, &singleResponse, sizeof(singleResponse), sizeof(singleResponse));
        if (innerDrtn != DR_Success) {
            ocspdErrorLog("failed to parse single response");
            return innerDrtn;
        }
        return operation(&singleResponse, stop);
    });
    if (drtn != DR_Success) {
        return false;
    }
    return true;
}

bool SecOCSPResponseForSingleResponse(SecOCSPResponseRef response, DERReturn (^operation)(SecOCSPSingleResponseRef singleResponse, DER_OCSPCertID *certId, DERAlgorithmId *hashAlgorithm, bool *stop))
{
    return SecOCSPResponseForDERSingleResponse(response, ^DERReturn(DER_OCSPSingleResponse *singleResponse, bool *stop) {
        DER_OCSPCertID certId;
        DERAlgorithmId algId;
        SecOCSPSingleResponseRef sr = SecOCSPSingleResponseCreate(singleResponse);
        if (!sr) {
            return DR_DecodeError;
        }

        DERReturn innerDrtn = DERParseSequenceContentToObject(&singleResponse->certId,
                                                              DERNumOCSPCertIDItemSpecs, DER_OCSPCertIDItemSpecs,
                                                              &certId, sizeof(certId), sizeof(certId));
        require_noerr_action(innerDrtn, singleResponseCleanup, ocspdErrorLog("failed to parse certId in single response"));

        innerDrtn = DERParseSequenceContent(&certId.hashAlgorithm,
                                     DERNumAlgorithmIdItemSpecs, DERAlgorithmIdItemSpecs,
                                     &algId, sizeof(algId));
        require_noerr_action(innerDrtn, singleResponseCleanup, ocspdErrorLog("failed to parse certId hash algorithm"));

        if (singleResponse->singleExtensions.data && singleResponse->singleExtensions.length > 0) {
            require_action(SecOCSPSingleExtensionsParse(&singleResponse->singleExtensions, sr), singleResponseCleanup, innerDrtn = DR_DecodeError; ocspdErrorLog("failed to parse single extensions"));
        }

        innerDrtn = operation(sr, &certId, &algId, stop);

    singleResponseCleanup:
        if (sr) {
            SecOCSPSingleResponseDestroy(sr);
        }
        return innerDrtn;
    });
}

/* Calculate temporal validity; set latestNextUpdate and expireTime.
   Returns true if valid, else returns false. */
bool SecOCSPResponseCalculateValidity(SecOCSPResponseRef this,
    CFTimeInterval maxAge, CFTimeInterval defaultTTL, CFAbsoluteTime verifyTime)
{
    bool ok = false;
	this->latestNextUpdate = NULL_TIME;

    if (this->producedAt > verifyTime + TRUST_TIME_LEEWAY) {
        secnotice("ocsp", "OCSPResponse: producedAt more than 1:15 from now");
        goto exit;
    }

    /* Make this->latestNextUpdate be the date farthest in the future
       of any of the singleResponses nextUpdate fields. */
    ok = SecOCSPResponseForDERSingleResponse(this, ^DERReturn(DER_OCSPSingleResponse *singleResponse, bool *stop) {
        DERReturn innerDrtn = DR_GenericErr;
        SecOCSPSingleResponseRef sr = SecOCSPSingleResponseCreate(singleResponse);
        if (!sr) {
            return DR_GenericErr;
        }

        if (sr->thisUpdate > verifyTime + TRUST_TIME_LEEWAY) {
            secnotice("ocsp","OCSPResponse: thisUpdate more than 1:15 from now");
            goto singleResponseCleanup;
        }
        if (singleResponse->nextUpdate.data && singleResponse->nextUpdate.length > 0) {
            if (sr->nextUpdate > this->latestNextUpdate) {
                this->latestNextUpdate = sr->nextUpdate;
            }
        }
        innerDrtn = DR_Success;

    singleResponseCleanup:
        if (sr) {
            SecOCSPSingleResponseDestroy(sr);
        }
        return innerDrtn;
    });
    require_action(ok, exit, ocspdErrorLog("failed to parse single responses"));
    ok = false;

    /* Now that we have this->latestNextUpdate, we figure out the latest
       date at which we will expire this response from our cache.  To comply
       with rfc5019s:

6.1.  Caching at the Client

   To minimize bandwidth usage, clients MUST locally cache authoritative
   OCSP responses (i.e., a response with a signature that has been
   successfully validated and that indicate an OCSPResponseStatus of
   'successful').

   Most OCSP clients will send OCSPRequests at or near the nextUpdate
   time (when a cached response expires).  To avoid large spikes in
   responder load that might occur when many clients refresh cached
   responses for a popular certificate, responders MAY indicate when the
   client should fetch an updated OCSP response by using the cache-
   control:max-age directive.  Clients SHOULD fetch the updated OCSP
   Response on or after the max-age time.  To ensure that clients
   receive an updated OCSP response, OCSP responders MUST refresh the
   OCSP response before the max-age time.

6.2 [...]

       we need to take the cache-control:max-age directive into account.

       The way the code below is written we ignore a max-age=0 in the
       http header.  Since a value of 0 (NULL_TIME) also means there
       was no max-age in the header. This seems ok since that would imply
       no-cache so we also ignore negative values for the same reason,
       instead we'll expire whenever this->latestNextUpdate tells us to,
       which is the signed value if max-age is too low, since we don't
       want to refetch multilple times for a single page load in a browser. */
	if (this->latestNextUpdate == NULL_TIME) {
        /* See comment above on RFC 5019 section 2.2.4. */
		/* Absolute expire time = current time plus defaultTTL */
		this->expireTime = verifyTime + defaultTTL;
	} else if (this->latestNextUpdate < verifyTime - TRUST_TIME_LEEWAY) {
        secnotice("ocsp", "OCSPResponse: latestNextUpdate more than 1:15 ago");
        goto exit;
    } else if (maxAge > 0) {
        /* Beware of double overflows such as:

               now + maxAge < this->latestNextUpdate

           in the math below since an attacker could create any positive
           value for maxAge. */
        if (maxAge < this->latestNextUpdate - verifyTime) {
            /* maxAge header wants us to expire the cache entry sooner than
               nextUpdate would allow, to balance server load. */
            this->expireTime = verifyTime + maxAge;
        } else {
            /* maxAge http header attempting to make us cache the response
               longer than it's valid for, bad http header! Ignoring you. */
#ifdef DEBUG
            CFStringRef hexResp = CFDataCopyHexString(this->data);
            ocspdDebug("OCSPResponse: now + maxAge > latestNextUpdate,"
                " using latestNextUpdate %@", hexResp);
            CFReleaseSafe(hexResp);
#endif
            this->expireTime = this->latestNextUpdate;
        }
	} else {
        /* No maxAge provided, just use latestNextUpdate. */
		this->expireTime = this->latestNextUpdate;
    }

    ok = true;
exit:
	return ok;
}

/*
 ResponderID ::= CHOICE {
    byName   [1] Name,
    byKey    [2] KeyHash }

 KeyHash ::= OCTET STRING -- SHA-1 hash of responder's public key
                          -- (excluding the tag and length fields)
 */
static bool SecOCSPResponseDataParseResponderId(DERItem *responderIdBytes, SecOCSPResponseRef responseObj)
{
    if (!responderIdBytes->data || responderIdBytes->length <= 0) {
        ocspdErrorLog("ResponseData missing responderId");
        return false;
    }
    bool result = false;
    DERReturn drtn = DR_GenericErr;
    DERDecodedInfo responderId;

    drtn = DERDecodeItem(responderIdBytes, &responderId);
    require_noerr_action(drtn, badResponse, ocspdErrorLog("failed to parse ResponderId"));
    switch (responderId.tag) {
        case ASN1_CONSTRUCTED | ASN1_CONTEXT_SPECIFIC | 1: // byName
            responseObj->responderId.tag = responderId.tag;
            responseObj->responderId.content.data = responderId.content.data;
            responseObj->responderId.content.length = responderId.content.length;
            result = true;
            break;
        case ASN1_CONSTRUCTED | ASN1_CONTEXT_SPECIFIC | 2: // byKey
            responseObj->responderId.tag = responderId.tag;
            DERDecodedInfo key;
            drtn = DERDecodeItem(&responderId.content, &key);
            require_noerr_action(drtn, badResponse, ocspdErrorLog("failed to parse ResponderId byKey"));
            require_action(key.tag == ASN1_OCTET_STRING, badResponse, ocspdErrorLog("failed to parse ResponderId byKey, wrong type"));
            require_action(DERLengthOfItem(key.tag, key.content.length) == responderId.content.length, badResponse, ocspdErrorLog("failed to parse ResponderId byKey, extra data"));
            responseObj->responderId.content.data = key.content.data;
            responseObj->responderId.content.length = key.content.length;
            result = true;
            break;
        default:
            ocspdErrorLog("unknown responderId choice: %llu", responseObj->responderId.tag);
    }

badResponse:
    return result;
}

static bool SecOCSPResponseDataParse(DERItem *responseDataBytes, SecOCSPResponseRef responseObj)
{
    bool result = false;
    DERReturn drtn = DR_GenericErr;
    DER_OCSPResponseData *responseData = &responseObj->responseData;

    drtn = DERParseSequence(responseDataBytes, DERNumOCSPResponseDataItemSpecs, DER_OCSPResponseDataItemSpecs, responseData, sizeof(*responseData));
    require_noerr_action(drtn, badResponse, ocspdErrorLog("failed to parse ResponseData: %d", drtn));

    // Version should not be present, but if present must be v1
    if (responseData->version.data && responseData->version.length > 0) {
        uint64_t version = 0;
        DERDecodedInfo decodedVersion;
        drtn = DERDecodeItem(&responseData->version, &decodedVersion);
        require_noerr_action(drtn, badResponse, ocspdErrorLog("failed to parse version from ResponseData: %d", drtn));
        require_action(decodedVersion.tag == ASN1_INTEGER, badResponse, ocspdErrorLog("failed to parse version from ResponseData: %d", DR_UnexpectedTag));
        drtn = DERParseInteger64(&decodedVersion.content, &version);
        require_noerr_action(drtn, badResponse, ocspdErrorLog("failed to parse version from ResponseData: %d", drtn));
        require_action(version == 0, badResponse, ocspdErrorLog("ResponseData has unknown version: %llu", version));
    }

    require(SecOCSPResponseDataParseResponderId(&responseData->responderId, responseObj), badResponse);

    require_action(responseData->producedAt.data && responseData->producedAt.length > 0, badResponse,
                   ocspdErrorLog("ResponseData with missing producedAt"));
    CFErrorRef dateError = NULL;
    responseObj->producedAt = SecAbsoluteTimeFromDateContentWithError(ASN1_GENERALIZED_TIME, responseData->producedAt.data, responseData->producedAt.length, &dateError);
    require_action(dateError == NULL, badResponse, ocspdErrorLog("failed to decode producedAt time: %@", dateError));

    result = SecOCSPResponseForSingleResponse(responseObj, ^DERReturn(SecOCSPSingleResponseRef singleResponse, DER_OCSPCertID *certId, DERAlgorithmId *hashAlgorithm, bool *stop) {
        // Let the wrapper do the parsing and return a failure
        return DR_Success;
    });
    require(result, badResponse);

    if (responseData->extensions.data && responseData->extensions.length > 0) {
        // We can use the single response extensions parser because we only care that a generic seq of extensions correctly parses
        result = SecOCSPSingleExtensionsParse(&responseData->extensions, NULL);
    }

badResponse:
    return result;
}

static bool SecBasicOCSPResponseParse(DERItem *basicResponseBytes, SecOCSPResponseRef responseObj)
{
    bool result = false;
    DERReturn drtn = DR_GenericErr;
    DERBasicOCSPResponse *basicResponse = &responseObj->basicResponse;

    drtn = DERParseSequence(basicResponseBytes, DERNumBasicOCSPResponseItemSpecs, DERBasicOCSPResponseItemSpecs, basicResponse, sizeof(*basicResponse));
    require_noerr_action(drtn, badResponse, ocspdErrorLog("failed to parse BasicOCSPResponse: %d", drtn));

    /* responseData */
    require_action(basicResponse->responseData.data && basicResponse->responseData.length > 0, badResponse, ocspdErrorLog("BasicOCSPResponse missing/bad responseData"));
    result = SecOCSPResponseDataParse(&responseObj->basicResponse.responseData, responseObj);
    require(result, badResponse);

    /* signatureAlgorithm */
    result = false;
    require_action(basicResponse->signatureAlgorithm.data && basicResponse->signatureAlgorithm.length > 0, badResponse, ocspdErrorLog("BasicOCSPResponse missing/bad signatureAlgorithm"));
    DERAlgorithmId algorithm;
    drtn = DERParseSequenceContent(&basicResponse->signatureAlgorithm,
                                   DERNumAlgorithmIdItemSpecs, DERAlgorithmIdItemSpecs,
                                   &algorithm, sizeof(algorithm));
    require_noerr_action(drtn, badResponse, ocspdErrorLog("failed to parse BasicOCSPResponse signatureAlgorithm: %d", drtn));

    /* Signature */
    require_action(basicResponse->signature.data && basicResponse->signature.length > 0, badResponse, ocspdErrorLog("BasicOCSPResponse missing/bad signature"));
    DERItem signature;
    DERByte numUnusedBits;
    drtn = DERParseBitString(&basicResponse->signature, &signature, &numUnusedBits);
    require_noerr_action(drtn, badResponse, ocspdErrorLog("failed to parse BasicOCSPResponse signature: %d", drtn));

    /* Certs */
    if (basicResponse->certs.data && basicResponse->certs.length > 0) {
        CFArrayRef certs = SecOCSPResponseCopySigners(responseObj);
        require_action(certs, badResponse, ocspdErrorLog("failed to parse BasicOCSPResponse certs"));
        CFReleaseNull(certs);
    }

    result = true;

badResponse:
    return result;
}

static bool SecOCSPResponseBytesParse(DERItem *responseBytes, SecOCSPResponseRef responseObj)
{
    bool result = false;
    DER_OCSPResponseBytes responseBytesStr;
    DERReturn drtn = DR_GenericErr;

    drtn = DERParseSequence(responseBytes, DERNumOCSPResponseBytesItemSpecs, DER_OCSPResponseBytesItemSpecs, &responseBytesStr, sizeof(responseBytesStr));
    require_noerr_action(drtn, badResponse, ocspdErrorLog("failed to parse OCSPResponseBytes: %d", drtn));
    require_action(DEROidCompare(&responseBytesStr.responseType, &BasicOCSPResponse), badResponse,
                   ocspdErrorLog("unknown responseType"));
    require_action(responseBytesStr.response.data && responseBytesStr.response.length > 0, badResponse,
                   ocspdErrorLog("OCSPResponseBytes with missing response"));
    result = SecBasicOCSPResponseParse(&responseBytesStr.response, responseObj);

badResponse:
    return result;
}

static bool SecOCSPResponseParse(CFDataRef ocspResponse, SecOCSPResponseRef responseObj)
{
    bool result = false;
    DER_OCSPResponse responseStr;
    DERReturn drtn = DR_GenericErr;

    DERItem derResponse = {
        .data = (DERByte *)CFDataGetBytePtr(ocspResponse),
        .length = (DERSize)CFDataGetLength(ocspResponse)
    };
    drtn = DERParseSequence(&derResponse, DERNumOCSPResponseItemSpecs, DER_OCSPResponseItemSpecs, &responseStr, sizeof(responseStr));
    require_noerr_action(drtn, badResponse, ocspdErrorLog("failed to parse OCSPResponse: %d", drtn));
    require_action(responseStr.responseStatus.data && responseStr.responseStatus.length == 1, badResponse, ocspdErrorLog("OCSPResponse has missing/bad responseStatus"));

    responseObj->responseStatus = responseStr.responseStatus.data[0];
    if (responseObj->responseStatus == OCSPResponseStatusSuccessful) {
        require_action(responseStr.responseBytes.data && responseStr.responseBytes.length > 0, badResponse, ocspdErrorLog("Successful OCSPResponse has missing/bad responseBytes"));
        result = SecOCSPResponseBytesParse(&responseStr.responseBytes, responseObj);
    } else {
        // This is a useful object but only for the top-level status
        secdebug("ocsp", "OCSPResponse with unsuccessful status: %d", responseObj->responseStatus);
        result = true;
    }

badResponse:
    return result;
}

SecOCSPResponseRef SecOCSPResponseCreateWithID(CFDataRef ocspResponse, int64_t responseID) {
    SecOCSPResponseRef this = NULL;

    require(ocspResponse, errOut);
    require(CFDataGetLength(ocspResponse) > 0, errOut);
    require(this = (SecOCSPResponseRef)calloc(1, sizeof(struct __SecOCSPResponse)),
        errOut);

    this->data = CFRetainSafe(ocspResponse);
    this->responseID = responseID;

    require(SecOCSPResponseParse(ocspResponse, this), errOut);
    return this;

errOut:
#ifdef DEBUG
    {
        CFStringRef hexResp = (this) ? CFDataCopyHexString(this->data) : NULL;
        secdebug("ocsp", "bad ocsp response: %@", hexResp);
        CFReleaseSafe(hexResp);
    }
#endif
    if (this) {
        SecOCSPResponseFinalize(this);
    }
    return NULL;
}

SecOCSPResponseRef SecOCSPResponseCreate(CFDataRef this) {
    return SecOCSPResponseCreateWithID(this, -1);
}

int64_t SecOCSPResponseGetID(SecOCSPResponseRef this) {
    return this->responseID;
}

CFDataRef SecOCSPResponseGetData(SecOCSPResponseRef this) {
    return this->data;
}

OCSPResponseStatus SecOCSPGetResponseStatus(SecOCSPResponseRef this) {
    return this->responseStatus;
}

CFAbsoluteTime SecOCSPResponseGetExpirationTime(SecOCSPResponseRef this) {
    return this->expireTime;
}

CFAbsoluteTime SecOCSPResponseProducedAt(SecOCSPResponseRef this) {
    return this->producedAt;
}

CFArrayRef SecOCSPResponseCopySigners(SecOCSPResponseRef response) {
    __block CFMutableArrayRef result = NULL;
    result = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (!result) {
        return NULL;
    }
    if (!response->basicResponse.certs.data || response->basicResponse.certs.length == 0) {
        return result;
    }

    DERDecodedInfo certsSeq;
    DERReturn drtn = DERDecodeItem(&response->basicResponse.certs, &certsSeq);
    require_noerr_action(drtn, errOut, CFReleaseNull(result));
    require_action(certsSeq.tag == ASN1_CONSTR_SEQUENCE, errOut, CFReleaseNull(result));
    if (certsSeq.content.length == 0) {
        // return an empty array if we received an empty sequence
        return result;
    }
    while (certsSeq.content.length > 0 && certsSeq.content.length <= LONG_MAX) {
        DERDecodedInfo certSeq;
        size_t certLen = 0;
        drtn = DERDecodeItemPartialBufferGetLength(&certsSeq.content, &certSeq, &certLen);
        require_noerr_action(drtn, errOut, CFReleaseNull(result));
        require_action(certSeq.tag == ASN1_CONSTR_SEQUENCE, errOut, CFReleaseNull(result));
        require_action(certsSeq.content.length >= DERLengthOfItem(ASN1_CONSTR_SEQUENCE, certLen), errOut, CFReleaseNull(result));
        SecCertificateRef cert = SecCertificateCreateWithBytes(NULL, certsSeq.content.data, (CFIndex)DERLengthOfItem(ASN1_CONSTR_SEQUENCE, certLen));
        if (!cert) {
            CFReleaseNull(result);
            break; // we couldn't parse the cert so we can't tell where the next one starts
        }
        CFArrayAppendValue(result, cert);
        certsSeq.content.data += SecCertificateGetLength(cert);
        certsSeq.content.length -= (size_t)SecCertificateGetLength(cert);
        CFReleaseNull(cert);
    }

errOut:
    if (result && CFArrayGetCount(result) == 0) {
        CFReleaseNull(result);
    }
    return result;
}

void SecOCSPResponseFinalize(SecOCSPResponseRef this) {
    if (!this) { return; }
    CFReleaseSafe(this->data);
    free(this);
}

static CFAbsoluteTime SecOCSPSingleResponseComputedNextUpdate(SecOCSPSingleResponseRef this, CFTimeInterval defaultTTL) {
    /* rfc2560 section 2.4 states: "If nextUpdate is not set, the
     responder is indicating that newer revocation information
     is available all the time".
     Let's ensure that thisUpdate isn't more than defaultTTL in
     the past then. */
    return this->nextUpdate == NULL_TIME ? this->thisUpdate + defaultTTL : this->nextUpdate;
}

bool SecOCSPSingleResponseCalculateValidity(SecOCSPSingleResponseRef this, CFTimeInterval defaultTTL, CFAbsoluteTime verifyTime) {
    if (this->thisUpdate > verifyTime + TRUST_TIME_LEEWAY) {
        ocspdErrorLog("OCSPSingleResponse: thisUpdate more than 1:15 from now");
        return false;
    }

    CFAbsoluteTime cnu = SecOCSPSingleResponseComputedNextUpdate(this, defaultTTL);
    if (verifyTime - TRUST_TIME_LEEWAY > cnu) {
        ocspdErrorLog("OCSPSingleResponse: %s %.2f days ago", this->nextUpdate ? "nextUpdate" : "thisUpdate + defaultTTL", (verifyTime - cnu) / 86400);
        return false;
    }

    return true;
}

CFArrayRef SecOCSPSingleResponseCopySCTs(SecOCSPSingleResponseRef this)
{
    if (!this) { return NULL; }
    return CFRetainSafe(this->scts);
}

static CF_RETURNS_RETAINED CFDataRef digestForOid(const uint8_t *data, CFIndex dataLength, DERItem *oid)
{
    if (!data || !oid || dataLength < 0 || dataLength > INT32_MAX) {
        return NULL;
    }
    unsigned char *(*digestFcn)(const void *data, CC_LONG len, unsigned char *md);
    CFIndex digestLen = 0;
    if (DEROidCompare(oid, &oidSha1)) {
        digestFcn = CC_SHA1;
        digestLen = CC_SHA1_DIGEST_LENGTH;
    } else if (DEROidCompare(oid, &oidSha224)) {
        digestFcn = CC_SHA224;
        digestLen = CC_SHA224_DIGEST_LENGTH;
    } else if (DEROidCompare(oid, &oidSha256)) {
        digestFcn = CC_SHA256;
        digestLen = CC_SHA256_DIGEST_LENGTH;
    } else if (DEROidCompare(oid, &oidSha384)) {
        digestFcn = CC_SHA384;
        digestLen = CC_SHA384_DIGEST_LENGTH;
    } else if (DEROidCompare(oid, &oidSha512)) {
        digestFcn = CC_SHA512;
        digestLen = CC_SHA512_DIGEST_LENGTH;
    } else {
        return NULL;
    }

    CFMutableDataRef digest = CFDataCreateMutable(NULL, digestLen);
    CFDataSetLength(digest, digestLen);

    digestFcn(data, (CC_LONG)dataLength, CFDataGetMutableBytePtr(digest));
    return digest;
}


SecOCSPSingleResponseRef SecOCSPResponseCopySingleResponse(
    SecOCSPResponseRef this, SecOCSPRequestRef request) {
    __block SecOCSPSingleResponseRef sr = NULL;

    if (!request) { return sr; }
    bool ok = SecOCSPResponseForDERSingleResponse(this, ^DERReturn(DER_OCSPSingleResponse *singleResponse, bool *stop) {
        DER_OCSPCertID certId;
        DERAlgorithmId algId;
        CFDataRef issuer = NULL;
        const DERItem *publicKey = NULL;
        CFDataRef serial = NULL;
        CFDataRef issuerNameHash = NULL;
        CFDataRef issuerPubKeyHash = NULL;
        DERItem *algorithm = NULL;
        if (request->certificate && request->issuer) {
            publicKey = SecCertificateGetPublicKeyData(request->issuer);
            if (publicKey->length > LONG_MAX) { return DR_BufOverflow; }
            issuer = SecCertificateCopyIssuerSequence(request->certificate);
            serial = SecCertificateCopySerialNumberData(request->certificate, NULL);
        } else {
            /* In testing, where we have the request but not the certs, prepopulate fields */
            algorithm = &request->certIdHash;
            issuerNameHash = CFRetainSafe(request->issuerNameDigest);
            issuerPubKeyHash = CFRetainSafe(request->issuerPubKeyDigest);
            serial = CFRetainSafe(request->serial);
        }

        DERReturn innerDrtn = DERParseSequenceContentToObject(&singleResponse->certId,
                                                              DERNumOCSPCertIDItemSpecs, DER_OCSPCertIDItemSpecs,
                                                              &certId, sizeof(certId), sizeof(certId));
        require_noerr_action(innerDrtn, singleResponseCleanup, ocspdErrorLog("failed to parse certId in single response"));

        if (!serial || certId.serialNumber.length != (size_t)CFDataGetLength(serial) ||
            memcmp(CFDataGetBytePtr(serial), certId.serialNumber.data,
                   certId.serialNumber.length)) {
            innerDrtn = DR_Success; // continue to next single response
            goto singleResponseCleanup;
        }

        /* Calcluate the issuerKey and issuerName digests using the
           hashAlgorithm and parameters specified in the certId, if
           they differ from the ones we already computed. */
        if (issuer && publicKey) {
            innerDrtn = DERParseSequenceContent(&certId.hashAlgorithm,
                                         DERNumAlgorithmIdItemSpecs, DERAlgorithmIdItemSpecs,
                                         &algId, sizeof(algId));
            require_noerr_action(innerDrtn, singleResponseCleanup, ocspdErrorLog("failed to parse certId hash algorithm"));

            if (!DEROidCompare(&algId.oid, algorithm)) {
                algorithm = &algId.oid;
                CFReleaseSafe(issuerNameHash);
                CFReleaseSafe(issuerPubKeyHash);
                issuerNameHash = digestForOid(CFDataGetBytePtr(issuer), CFDataGetLength(issuer), algorithm);
                issuerPubKeyHash = digestForOid(publicKey->data, (CFIndex)publicKey->length, algorithm);
            }
        }

        /* This can happen when the hash algorithm is not supported, should be really rare */
        /* See also: <rdar://problem/21908655> CrashTracer: securityd at securityd: SecOCSPResponseCopySingleResponse */
        if (!issuerNameHash || !issuerPubKeyHash) {
            ocspdErrorLog("Unknown hash algorithm in singleResponse");
            innerDrtn = DR_Success; // continue to next single response
            goto singleResponseCleanup;
        }

        // Compare hashes
        if (certId.issuerNameHash.length == (size_t)CFDataGetLength(issuerNameHash)
            && !memcmp(CFDataGetBytePtr(issuerNameHash),
                       certId.issuerNameHash.data, certId.issuerNameHash.length)
            && certId.issuerKeyHash.length == (size_t)CFDataGetLength(issuerPubKeyHash)
            && !memcmp(CFDataGetBytePtr(issuerPubKeyHash),
                       certId.issuerKeyHash.data, certId.issuerKeyHash.length)) {

            /* resp matches the certificate in request, so let's use it. */
            sr = SecOCSPSingleResponseCreate(singleResponse);
            if (sr) {
                ocspdDebug("found matching singleResponse");
                innerDrtn = DR_Success;
                *stop = true; // No need to look at more single responses
            }
        }

    singleResponseCleanup:
        CFReleaseSafe(issuerPubKeyHash);
        CFReleaseSafe(issuerNameHash);
        CFReleaseSafe(serial);
        CFReleaseSafe(issuer);
        return innerDrtn;
    });

    if (!ok) {
        ocspdErrorLog("failed to parse single responses");
    }
    if (!sr) {
        ocspdDebug("certID not found");
    }
	return sr;
}

static bool SecOCSPResponseVerifySignature(SecOCSPResponseRef this,
    SecKeyRef key) {
    DERAlgorithmId algorithm;
    DERReturn drtn = DERParseSequenceContent(&this->basicResponse.signatureAlgorithm,
                                 DERNumAlgorithmIdItemSpecs, DERAlgorithmIdItemSpecs,
                                 &algorithm, sizeof(algorithm));
    if (drtn != DR_Success) {
        return false;
    }
    DERByte numUnusedBits;
    DERItem signature;
    drtn = DERParseBitString(&this->basicResponse.signature, &signature, &numUnusedBits);
    if (drtn != DR_Success) {
        return false;
    }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    /* Setup algId in SecAsn1AlgId format. */
    SecAsn1AlgId algId;
#pragma clang diagnostic pop
    algId.algorithm.Length = algorithm.oid.length;
    algId.algorithm.Data = algorithm.oid.data;
    algId.parameters.Length = algorithm.params.length;
    algId.parameters.Data = algorithm.params.data;

    return SecKeyDigestAndVerify(key, &algId,
        this->basicResponse.responseData.data,
        this->basicResponse.responseData.length,
        signature.data,
        signature.length) == errSecSuccess;
}

static bool SecOCSPResponseIsIssuer(SecOCSPResponseRef this,
    SecCertificateRef issuer) {
    bool shouldBeSigner = false;
	if (this->responderId.tag == (ASN1_CONSTRUCTED | ASN1_CONTEXT_SPECIFIC | 1)) {
		/* Name inside response must == signer's SubjectName. */
        CFDataRef subject = SecCertificateCopySubjectSequence(issuer);
        if (!subject) {
			ocspdDebug("error on SecCertificateCopySubjectSequence");
			return false;
		}
        if ((size_t)CFDataGetLength(subject) == this->responderId.content.length &&
            !memcmp(this->responderId.content.data, CFDataGetBytePtr(subject),
                this->responderId.content.length)) {
			ocspdDebug("good ResponderID.byName");
			shouldBeSigner = true;
        } else {
			ocspdDebug("BAD ResponderID.byName");
		}
        CFRelease(subject);
    } else if (this->responderId.tag == (ASN1_CONSTRUCTED | ASN1_CONTEXT_SPECIFIC | 2)) {
		/* ResponderID.byKey must == SHA1(signer's public key) */
        CFDataRef pubKeyDigest = SecCertificateCopyPublicKeySHA1Digest(issuer);
        if ((size_t)CFDataGetLength(pubKeyDigest) == this->responderId.content.length &&
            !memcmp(this->responderId.content.data, CFDataGetBytePtr(pubKeyDigest),
                this->responderId.content.length)) {
			ocspdDebug("good ResponderID.byKey");
			shouldBeSigner = true;
		} else {
			ocspdDebug("BAD ResponderID.byKey");
		}
        CFRelease(pubKeyDigest);
    } else {
        // Unknown responderID tag
        return false;
    }

    if (shouldBeSigner) {
        SecKeyRef key = SecCertificateCopyKey(issuer);
        if (key) {
            shouldBeSigner = SecOCSPResponseVerifySignature(this, key);
            ocspdDebug("ocsp response signature %sok", shouldBeSigner ? "" : "not ");
            CFRelease(key);
        } else {
			ocspdDebug("Failed to extract key from leaf certificate");
            shouldBeSigner = false;
        }
    }

    return shouldBeSigner;
}

/* Returns the SecCertificateRef of the cert that signed this ocspResponse if
 we can find one and NULL if we can't find a valid signer. */
SecCertificateRef SecOCSPResponseCopySigner(SecOCSPResponseRef this, SecCertificateRef issuer) {
    /* Look though any certs that came with the response to find
     * which one signed the response. */
    __block SecCertificateRef signer = NULL;
    CFArrayRef certs = SecOCSPResponseCopySigners(this);
    if (certs) {
        CFArrayForEach(certs, ^(const void *value) {
            if (SecOCSPResponseIsIssuer(this, (SecCertificateRef)value)) {
                signer = CFRetainSafe((SecCertificateRef)value);
            }
        });
    }
    CFReleaseNull(certs);

    if (!signer && issuer && SecOCSPResponseIsIssuer(this, issuer)) {
        signer = CFRetainSafe(issuer);
    }
    return signer;
}

bool SecOCSPResponseIsWeakHash(SecOCSPResponseRef response) {
    DERAlgorithmId algId;
    DERReturn drtn = DERParseSequenceContent(&response->basicResponse.signatureAlgorithm,
                                 DERNumAlgorithmIdItemSpecs, DERAlgorithmIdItemSpecs,
                                 &algId, sizeof(algId));
    if (drtn != DR_Success) {
        return true;
    }
    SecSignatureHashAlgorithm algorithm = SecSignatureHashAlgorithmForAlgorithmOid(&algId.oid);
    if (algorithm == kSecSignatureHashAlgorithmUnknown ||
        algorithm == kSecSignatureHashAlgorithmMD2 ||
        algorithm == kSecSignatureHashAlgorithmMD4 ||
        algorithm == kSecSignatureHashAlgorithmMD5 ||
        algorithm == kSecSignatureHashAlgorithmSHA1) {
        return true;
    }
    return false;
}
