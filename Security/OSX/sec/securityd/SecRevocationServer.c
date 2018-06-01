/*
 * Copyright (c) 2008-2018 Apple Inc. All Rights Reserved.
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
 * SecRevocationServer.c - Engine for evaluating certificate revocation.
 */

#include <AssertMacros.h>

#include <mach/mach_time.h>

#include <Security/SecCertificatePriv.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecInternal.h>

#include <utilities/debugging.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecIOFormat.h>

#include <securityd/SecTrustServer.h>
#include <securityd/SecOCSPRequest.h>
#include <securityd/SecOCSPResponse.h>
#include <securityd/asynchttp.h>
#include <securityd/SecOCSPCache.h>
#include <securityd/SecRevocationDb.h>
#include <securityd/SecCertificateServer.h>
#include <securityd/SecPolicyServer.h>

#include <securityd/SecRevocationServer.h>

// MARK: SecORVCRef
/********************************************************
 ****************** OCSP RVC Functions ******************
 ********************************************************/
const CFAbsoluteTime kSecDefaultOCSPResponseTTL = 24.0 * 60.0 * 60.0;
const CFAbsoluteTime kSecOCSPResponseOnlineTTL = 5.0 * 60.0;
#define OCSP_RESPONSE_TIMEOUT       (3 * NSEC_PER_SEC)

/* OCSP Revocation verification context. */
struct OpaqueSecORVC {
    /* Will contain the response data. */
    asynchttp_t http;

    /* Pointer to the builder for this revocation check. */
    SecPathBuilderRef builder;

    /* Pointer to the generic rvc for this revocation check */
    SecRVCRef rvc;

    /* The ocsp request we send to each responder. */
    SecOCSPRequestRef ocspRequest;

    /* The freshest response we received so far, from stapling or cache or responder. */
    SecOCSPResponseRef ocspResponse;

    /* The best validated candidate single response we received so far, from stapling or cache or responder. */
    SecOCSPSingleResponseRef ocspSingleResponse;

    /* Index of cert in builder that this RVC is for 0 = leaf, etc. */
    CFIndex certIX;

    /* Index in array returned by SecCertificateGetOCSPResponders() for current
     responder. */
    CFIndex responderIX;

    /* URL of current responder. */
    CFURLRef responder;

    /* Date until which this revocation status is valid. */
    CFAbsoluteTime nextUpdate;

    bool done;
};

static void SecORVCFinish(SecORVCRef orvc) {
    secdebug("alloc", "%p", orvc);
    asynchttp_free(&orvc->http);
    if (orvc->ocspRequest) {
        SecOCSPRequestFinalize(orvc->ocspRequest);
        orvc->ocspRequest = NULL;
    }
    if (orvc->ocspResponse) {
        SecOCSPResponseFinalize(orvc->ocspResponse);
        orvc->ocspResponse = NULL;
        if (orvc->ocspSingleResponse) {
            SecOCSPSingleResponseDestroy(orvc->ocspSingleResponse);
            orvc->ocspSingleResponse = NULL;
        }
    }
}

#define MAX_OCSP_RESPONDERS 3
#define OCSP_REQUEST_THRESHOLD 10

/* Return the next responder we should contact for this rvc or NULL if we
 exhausted them all. */
static CFURLRef SecORVCGetNextResponder(SecORVCRef rvc) {
    SecCertificateRef cert = SecPathBuilderGetCertificateAtIndex(rvc->builder, rvc->certIX);
    CFArrayRef ocspResponders = SecCertificateGetOCSPResponders(cert);
    if (ocspResponders) {
        CFIndex responderCount = CFArrayGetCount(ocspResponders);
        if (responderCount >= OCSP_REQUEST_THRESHOLD) {
            secnotice("rvc", "too many ocsp responders (%ld)", (long)responderCount);
            return NULL;
        }
        while (rvc->responderIX < responderCount && rvc->responderIX < MAX_OCSP_RESPONDERS) {
            CFURLRef responder = CFArrayGetValueAtIndex(ocspResponders, rvc->responderIX);
            rvc->responderIX++;
            CFStringRef scheme = CFURLCopyScheme(responder);
            if (scheme) {
                /* We only support http and https responders currently. */
                bool valid_responder = (CFEqual(CFSTR("http"), scheme) ||
                                        CFEqual(CFSTR("https"), scheme));
                CFRelease(scheme);
                if (valid_responder)
                    return responder;
            }
        }
    }
    return NULL;
}

/* Fire off an async http request for this certs revocation status, return
 false if request was queued, true if we're done. */
static bool SecORVCFetchNext(SecORVCRef rvc) {
    while ((rvc->responder = SecORVCGetNextResponder(rvc))) {
        CFDataRef request = SecOCSPRequestGetDER(rvc->ocspRequest);
        if (!request)
            goto errOut;

        secinfo("rvc", "Sending http ocsp request for cert %ld", rvc->certIX);
        if (!asyncHttpPost(rvc->responder, request, OCSP_RESPONSE_TIMEOUT, &rvc->http)) {
            /* Async request was posted, wait for reply. */
            return false;
        }
    }

errOut:
    rvc->done = true;
    return true;
}

/* Process a verified ocsp response for a given cert. Return true if the
 certificate status was obtained. */
static bool SecOCSPSingleResponseProcess(SecOCSPSingleResponseRef this,
                                         SecORVCRef rvc) {
    bool processed;
    switch (this->certStatus) {
        case CS_Good:
            secdebug("ocsp", "CS_Good for cert %" PRIdCFIndex, rvc->certIX);
            /* @@@ Mark cert as valid until a given date (nextUpdate if we have one)
             in the info dictionary. */
            //cert.revokeCheckGood(true);
            rvc->nextUpdate = this->nextUpdate == NULL_TIME ? this->thisUpdate + kSecDefaultOCSPResponseTTL : this->nextUpdate;
            processed = true;
            break;
        case CS_Revoked:
            secdebug("ocsp", "CS_Revoked for cert %" PRIdCFIndex, rvc->certIX);
            /* @@@ Mark cert as revoked (with reason) at revocation date in
             the info dictionary, or perhaps we should use a different key per
             reason?   That way a client using exceptions can ignore some but
             not all reasons. */
            SInt32 reason = this->crlReason;
            CFNumberRef cfreason = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &reason);
            SecPathBuilderSetResultInPVCs(rvc->builder, kSecPolicyCheckRevocation, rvc->certIX,
                                  cfreason, true);
            if (rvc->builder) {
                CFMutableDictionaryRef info = SecPathBuilderGetInfo(rvc->builder);
                if (info) {
                    /* make the revocation reason available in the trust result */
                    CFDictionarySetValue(info, kSecTrustRevocationReason, cfreason);
                }
            }
            CFRelease(cfreason);
            processed = true;
            break;
        case CS_Unknown:
            /* not an error, no per-cert status, nothing here */
            secdebug("ocsp", "CS_Unknown for cert %" PRIdCFIndex, rvc->certIX);
            processed = false;
            break;
        default:
            secnotice("ocsp", "BAD certStatus (%d) for cert %" PRIdCFIndex,
                      (int)this->certStatus, rvc->certIX);
            processed = false;
            break;
    }

    return processed;
}

static void SecORVCUpdatePVC(SecORVCRef rvc) {
    if (rvc->ocspSingleResponse) {
        SecOCSPSingleResponseProcess(rvc->ocspSingleResponse, rvc);
    }
    if (rvc->ocspResponse) {
        rvc->nextUpdate = SecOCSPResponseGetExpirationTime(rvc->ocspResponse);
    }
}

typedef void (^SecOCSPEvaluationCompleted)(SecTrustResultType tr);

static void
SecOCSPEvaluateCompleted(const void *userData,
                         CFArrayRef chain, CFArrayRef details, CFDictionaryRef info,
                         SecTrustResultType result) {
    SecOCSPEvaluationCompleted evaluated = (SecOCSPEvaluationCompleted)userData;
    evaluated(result);
    Block_release(evaluated);

}

static bool SecOCSPResponseEvaluateSigner(SecORVCRef rvc, CFArrayRef signers, CFArrayRef issuers, CFAbsoluteTime verifyTime) {
    __block bool evaluated = false;
    bool trusted = false;
    if (!signers || !issuers) {
        return trusted;
    }

    /* Verify the signer chain against the OCSPSigner policy, using the issuer chain as anchors. */
    const void *ocspSigner = SecPolicyCreateOCSPSigner();
    CFArrayRef policies = CFArrayCreate(kCFAllocatorDefault,
                                        &ocspSigner, 1, &kCFTypeArrayCallBacks);
    CFRelease(ocspSigner);

    SecOCSPEvaluationCompleted completed = Block_copy(^(SecTrustResultType result) {
        if (result == kSecTrustResultProceed || result == kSecTrustResultUnspecified) {
            evaluated = true;
        }
    });

    CFDataRef clientAuditToken = SecPathBuilderCopyClientAuditToken(rvc->builder);
    SecPathBuilderRef oBuilder = SecPathBuilderCreate(clientAuditToken,
                                                      signers, issuers, true, false,
                                                      policies, NULL, NULL,  NULL,
                                                      verifyTime, NULL, NULL,
                                                      SecOCSPEvaluateCompleted, completed);
    /* Build the chain(s), evaluate them, call the completed block, free the block and builder */
    SecPathBuilderStep(oBuilder);
    CFReleaseNull(clientAuditToken);
    CFReleaseNull(policies);

    /* verify the public key of the issuer signed the OCSP signer */
    if (evaluated) {
        SecCertificateRef issuer = NULL, signer = NULL;
        SecKeyRef issuerPubKey = NULL;

        issuer = (SecCertificateRef)CFArrayGetValueAtIndex(issuers, 0);
        signer = (SecCertificateRef)CFArrayGetValueAtIndex(signers, 0);

        if (issuer) {
#if TARGET_OS_IPHONE
            issuerPubKey = SecCertificateCopyPublicKey(issuer);
#else
            issuerPubKey = SecCertificateCopyPublicKey_ios(issuer);
#endif
        }
        if (signer && issuerPubKey && (errSecSuccess == SecCertificateIsSignedBy(signer, issuerPubKey))) {
            trusted = true;
        } else {
            secnotice("ocsp", "ocsp signer cert not signed by issuer");
        }
        CFReleaseNull(issuerPubKey);
    }

    return trusted;
}

static bool SecOCSPResponseVerify(SecOCSPResponseRef ocspResponse, SecORVCRef rvc, CFAbsoluteTime verifyTime) {
    bool trusted;
    SecCertificatePathVCRef issuers = SecCertificatePathVCCopyFromParent(SecPathBuilderGetPath(rvc->builder), rvc->certIX + 1);
    SecCertificateRef issuer = issuers ? CFRetainSafe(SecCertificatePathVCGetCertificateAtIndex(issuers, 0)) : NULL;
    CFArrayRef signers = SecOCSPResponseCopySigners(ocspResponse);
    SecCertificateRef signer = SecOCSPResponseCopySigner(ocspResponse, issuer);

    if (signer && signers) {
        if (issuer && CFEqual(signer, issuer)) {
            /* We already know we trust issuer since it's the issuer of the
             * cert we are verifying. */
            secinfo("ocsp", "ocsp responder: %@ response signed by issuer",
                    rvc->responder);
            trusted = true;
        } else {
            secinfo("ocsp", "ocsp responder: %@ response signed by cert issued by issuer",
                    rvc->responder);
            CFMutableArrayRef signerCerts = NULL;
            CFArrayRef issuerCerts = NULL;

            /* Ensure the signer cert is the 0th cert for trust evaluation */
            signerCerts = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
            CFArrayAppendValue(signerCerts, signer);
            CFArrayAppendArray(signerCerts, signers, CFRangeMake(0, CFArrayGetCount(signers)));

            if (issuers) {
                issuerCerts = SecCertificatePathVCCopyCertificates(issuers);
            }

            if (SecOCSPResponseEvaluateSigner(rvc, signerCerts, issuerCerts, verifyTime)) {
                secdebug("ocsp", "response satisfies ocspSigner policy (%@)",
                         rvc->responder);
                trusted = true;
            } else {
                /* @@@ We don't trust the cert so don't use this response. */
                secnotice("ocsp", "ocsp response signed by certificate which "
                          "does not satisfy ocspSigner policy");
                trusted = false;
            }
            CFReleaseNull(signerCerts);
            CFReleaseNull(issuerCerts);
        }
    } else {
        /* @@@ No signer found for this ocsp response, discard it. */
        secnotice("ocsp", "ocsp responder: %@ no signer found for response",
                  rvc->responder);
        trusted = false;
    }

#if DUMP_OCSPRESPONSES
    char buf[40];
    snprintf(buf, 40, "/tmp/ocspresponse%ld%s.der",
             rvc->certIX, (trusted ? "t" : "u"));
    secdumpdata(ocspResponse->data, buf);
#endif
    CFReleaseNull(issuers);
    CFReleaseNull(issuer);
    CFReleaseNull(signers);
    CFReleaseNull(signer);
    return trusted;
}

static void SecORVCConsumeOCSPResponse(SecORVCRef rvc, SecOCSPResponseRef ocspResponse /*CF_CONSUMED*/, CFTimeInterval maxAge, bool updateCache) {
    SecOCSPSingleResponseRef sr = NULL;
    require_quiet(ocspResponse, errOut);
    SecOCSPResponseStatus orStatus = SecOCSPGetResponseStatus(ocspResponse);
    require_action_quiet(orStatus == kSecOCSPSuccess, errOut,
                         secnotice("ocsp", "responder: %@ returned status: %d",  rvc->responder, orStatus));
    require_action_quiet(sr = SecOCSPResponseCopySingleResponse(ocspResponse, rvc->ocspRequest), errOut,
                         secnotice("ocsp",  "ocsp responder: %@ did not include status of requested cert", rvc->responder));
    // Check if this response is fresher than any (cached) response we might still have in the rvc.
    require_quiet(!rvc->ocspSingleResponse || rvc->ocspSingleResponse->thisUpdate < sr->thisUpdate, errOut);

    CFAbsoluteTime verifyTime = CFAbsoluteTimeGetCurrent();
    /* Check the OCSP response signature and verify the response. */
    require_quiet(SecOCSPResponseVerify(ocspResponse, rvc,
                                        sr->certStatus == CS_Revoked ? SecOCSPResponseProducedAt(ocspResponse) : verifyTime), errOut);

    // If we get here, we have a properly signed ocsp response
    // but we haven't checked dates yet.

    bool sr_valid = SecOCSPSingleResponseCalculateValidity(sr, kSecDefaultOCSPResponseTTL, verifyTime);
    if (sr->certStatus == CS_Good) {
        // Side effect of SecOCSPResponseCalculateValidity sets ocspResponse->expireTime
        require_quiet(sr_valid && SecOCSPResponseCalculateValidity(ocspResponse, maxAge, kSecDefaultOCSPResponseTTL, verifyTime), errOut);
    } else if (sr->certStatus == CS_Revoked) {
        // Expire revoked responses when the subject certificate itself expires.
        ocspResponse->expireTime = SecCertificateNotValidAfter(SecPathBuilderGetCertificateAtIndex(rvc->builder, rvc->certIX));
    }

    // Ok we like the new response, let's toss the old one.
    if (updateCache)
        SecOCSPCacheReplaceResponse(rvc->ocspResponse, ocspResponse, rvc->responder, verifyTime);

    if (rvc->ocspResponse) SecOCSPResponseFinalize(rvc->ocspResponse);
    rvc->ocspResponse = ocspResponse;
    ocspResponse = NULL;

    if (rvc->ocspSingleResponse) SecOCSPSingleResponseDestroy(rvc->ocspSingleResponse);
    rvc->ocspSingleResponse = sr;
    sr = NULL;

    rvc->done = sr_valid;

errOut:
    if (sr) SecOCSPSingleResponseDestroy(sr);
    if (ocspResponse) SecOCSPResponseFinalize(ocspResponse);
}

/* Callback from async http code after an ocsp response has been received. */
static void SecOCSPFetchCompleted(asynchttp_t *http, CFTimeInterval maxAge) {
    SecORVCRef rvc = (SecORVCRef)http->info;
    SecPathBuilderRef builder = rvc->builder;
    TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(builder);
    if (analytics) {
        /* Add the time this fetch took to complete to the total time */
        analytics->ocsp_fetch_time += (mach_absolute_time() - http->start_time);
    }
    SecOCSPResponseRef ocspResponse = NULL;
    if (http->response) {
        CFDataRef data = CFHTTPMessageCopyBody(http->response);
        if (data) {
            /* Parse the returned data as if it's an ocspResponse. */
            ocspResponse = SecOCSPResponseCreate(data);
            CFRelease(data);
        }
    }

    if ((!http->response || !ocspResponse) && analytics) {
        /* We didn't get any data back, so the fetch failed */
        analytics->ocsp_fetch_failed++;
    }

    SecORVCConsumeOCSPResponse(rvc, ocspResponse, maxAge, true);
    // TODO: maybe we should set the cache-control: false in the http header and try again if the response is stale

    if (!rvc->done) {
        if (analytics && ocspResponse) {
            /* We got an OCSP response that didn't pass validation */
            analytics-> ocsp_validation_failed = true;
        }
        /* Clear the data for the next response. */
        asynchttp_free(http);
        SecORVCFetchNext(rvc);
    }

    if (rvc->done) {
        secdebug("rvc", "got OCSP response for cert: %ld", rvc->certIX);
        SecORVCUpdatePVC(rvc);
        if (!SecPathBuilderDecrementAsyncJobCount(builder)) {
            secdebug("rvc", "done with all async jobs");
            SecPathBuilderStep(builder);
        }
    }
}

static SecORVCRef SecORVCCreate(SecRVCRef rvc, SecPathBuilderRef builder, CFIndex certIX) {
    SecORVCRef orvc = NULL;
    orvc = malloc(sizeof(struct OpaqueSecORVC));
    if (orvc) {
        memset(orvc, 0, sizeof(struct OpaqueSecORVC));
        orvc->builder = builder;
        orvc->rvc = rvc;
        orvc->certIX = certIX;
        orvc->http.queue = SecPathBuilderGetQueue(builder);
        orvc->http.token = SecPathBuilderCopyClientAuditToken(builder);
        orvc->http.completed = SecOCSPFetchCompleted;
        orvc->http.info = orvc;
        orvc->ocspRequest = NULL;
        orvc->responderIX = 0;
        orvc->responder = NULL;
        orvc->nextUpdate = NULL_TIME;
        orvc->ocspResponse = NULL;
        orvc->ocspSingleResponse = NULL;
        orvc->done = false;

        SecCertificateRef cert = SecPathBuilderGetCertificateAtIndex(builder, certIX);
        if (SecPathBuilderGetCertificateCount(builder) > (certIX + 1)) {
            SecCertificateRef issuer = SecPathBuilderGetCertificateAtIndex(builder, certIX + 1);
            orvc->ocspRequest = SecOCSPRequestCreate(cert, issuer);
        }
    }
    return orvc;
}

static void SecORVCProcessStapledResponses(SecORVCRef rvc) {
    /* Get stapled OCSP responses */
    CFArrayRef ocspResponsesData = SecPathBuilderCopyOCSPResponses(rvc->builder);

    if(ocspResponsesData) {
        secdebug("rvc", "Checking stapled responses for cert %ld", rvc->certIX);
        CFArrayForEach(ocspResponsesData, ^(const void *value) {
            SecOCSPResponseRef ocspResponse = SecOCSPResponseCreate(value);
            SecORVCConsumeOCSPResponse(rvc, ocspResponse, NULL_TIME, false);
        });
        CFRelease(ocspResponsesData);
    }
}

// MARK: SecCRVCRef
/********************************************************
 ******************* CRL RVC Functions ******************
 ********************************************************/
#if ENABLE_CRLS
#include <../trustd/macOS/SecTrustOSXEntryPoints.h>
#define kSecDefaultCRLTTL kSecDefaultOCSPResponseTTL

/* CRL Revocation verification context. */
struct OpaqueSecCRVC {
    /* Response data from ocspd. Yes, ocspd does CRLs, but not OCSP... */
    async_ocspd_t async_ocspd;

    /* Pointer to the builder for this revocation check. */
    SecPathBuilderRef builder;

    /* Pointer to the generic rvc for this revocation check */
    SecRVCRef rvc;

    /* The current CRL status from ocspd. */
    OSStatus status;

    /* Index of cert in builder that this RVC is for 0 = leaf, etc. */
    CFIndex certIX;

    /* Index in array returned by SecCertificateGetCRLDistributionPoints() for
     current distribution point. */
    CFIndex distributionPointIX;

    /* URL of current distribution point. */
    CFURLRef distributionPoint;

    /* Date until which this revocation status is valid. */
    CFAbsoluteTime nextUpdate;

    bool done;
};

static void SecCRVCFinish(SecCRVCRef crvc) {
    // nothing yet
}

#define MAX_CRL_DPS 3
#define CRL_REQUEST_THRESHOLD 10

static CFURLRef SecCRVCGetNextDistributionPoint(SecCRVCRef rvc) {
    SecCertificateRef cert = SecPathBuilderGetCertificateAtIndex(rvc->builder, rvc->certIX);
    CFArrayRef crlDPs = SecCertificateGetCRLDistributionPoints(cert);
    if (crlDPs) {
        CFIndex crlDPCount = CFArrayGetCount(crlDPs);
        if (crlDPCount >= CRL_REQUEST_THRESHOLD) {
            secnotice("rvc", "too many CRL DP entries (%ld)", (long)crlDPCount);
            return NULL;
        }
        while (rvc->distributionPointIX < crlDPCount && rvc->distributionPointIX < MAX_CRL_DPS) {
            CFURLRef distributionPoint = CFArrayGetValueAtIndex(crlDPs, rvc->distributionPointIX);
            rvc->distributionPointIX++;
            CFStringRef scheme = CFURLCopyScheme(distributionPoint);
            if (scheme) {
                /* We only support http and https responders currently. */
                bool valid_DP = (CFEqual(CFSTR("http"), scheme) ||
                                 CFEqual(CFSTR("https"), scheme) ||
                                 CFEqual(CFSTR("ldap"), scheme));
                CFRelease(scheme);
                if (valid_DP)
                    return distributionPoint;
            }
        }
    }
    return NULL;
}

static void SecCRVCGetCRLStatus(SecCRVCRef rvc) {
    SecCertificateRef cert = SecPathBuilderGetCertificateAtIndex(rvc->builder, rvc->certIX);
    SecCertificatePathVCRef path = SecPathBuilderGetPath(rvc->builder);
    CFArrayRef serializedCertPath = SecCertificatePathVCCreateSerialized(path);
    secdebug("rvc", "searching CRL cache for cert: %ld", rvc->certIX);
    rvc->status = SecTrustLegacyCRLStatus(cert, serializedCertPath, rvc->distributionPoint);
    CFReleaseNull(serializedCertPath);
    /* we got a response indicating that the CRL was checked */
    if (rvc->status == errSecSuccess || rvc->status == errSecCertificateRevoked) {
        rvc->done = true;
        /* ocspd doesn't give us the nextUpdate time, so set to default */
        rvc->nextUpdate = SecPathBuilderGetVerifyTime(rvc->builder) + kSecDefaultCRLTTL;
    }
}

static void SecCRVCCheckRevocationCache(SecCRVCRef rvc) {
    while ((rvc->distributionPoint = SecCRVCGetNextDistributionPoint(rvc))) {
        SecCRVCGetCRLStatus(rvc);
        if (rvc->status == errSecCertificateRevoked) {
            return;
        }
    }
}

/* Fire off an async http request for this certs revocation status, return
 false if request was queued, true if we're done. */
static bool SecCRVCFetchNext(SecCRVCRef rvc) {
    while ((rvc->distributionPoint = SecCRVCGetNextDistributionPoint(rvc))) {
        SecCertificateRef cert = SecPathBuilderGetCertificateAtIndex(rvc->builder, rvc->certIX);
        SecCertificatePathVCRef path = SecPathBuilderGetPath(rvc->builder);
        CFArrayRef serializedCertPath = SecCertificatePathVCCreateSerialized(path);
        secinfo("rvc", "fetching CRL for cert: %ld", rvc->certIX);
        if (!SecTrustLegacyCRLFetch(&rvc->async_ocspd, rvc->distributionPoint,
                                    CFAbsoluteTimeGetCurrent(), cert, serializedCertPath)) {
            CFDataRef clientAuditToken = NULL;
            SecTaskRef task = NULL;
            audit_token_t auditToken = {};
            clientAuditToken = SecPathBuilderCopyClientAuditToken(rvc->builder);
            require(clientAuditToken, out);
            require(sizeof(auditToken) == CFDataGetLength(clientAuditToken), out);
            CFDataGetBytes(clientAuditToken, CFRangeMake(0, sizeof(auditToken)), (uint8_t *)&auditToken);
            require(task = SecTaskCreateWithAuditToken(NULL, auditToken), out);
            secnotice("rvc", "asynchronously fetching CRL (%@) for client (%@)",
                      rvc->distributionPoint, task);

        out:
            CFReleaseNull(clientAuditToken);
            CFReleaseNull(task);
            /* Async request was posted, wait for reply. */
            return false;
        }
    }
    rvc->done = true;
    return true;
}

static void SecCRVCUpdatePVC(SecCRVCRef rvc) {
    if (rvc->status == errSecCertificateRevoked) {
        secdebug("rvc", "CRL revoked cert %" PRIdCFIndex, rvc->certIX);
        SInt32 reason = 0; // unspecified, since ocspd didn't tell us
        CFNumberRef cfreason = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &reason);
        SecPathBuilderSetResultInPVCs(rvc->builder, kSecPolicyCheckRevocation, rvc->certIX,
                              cfreason, true);
        if (rvc->builder) {
            CFMutableDictionaryRef info = SecPathBuilderGetInfo(rvc->builder);
            if (info) {
                /* make the revocation reason available in the trust result */
                CFDictionarySetValue(info, kSecTrustRevocationReason, cfreason);
            }
        }
        CFReleaseNull(cfreason);
    }
}

static void SecCRVCFetchCompleted(async_ocspd_t *ocspd) {
    SecCRVCRef rvc = ocspd->info;
    SecPathBuilderRef builder = rvc->builder;
    TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(builder);
    if (analytics) {
        /* Add the time this fetch took to complete to the total time */
        analytics->crl_fetch_time += (mach_absolute_time() - ocspd->start_time);
    }
    /* we got a response indicating that the CRL was checked */
    if (ocspd->response == errSecSuccess || ocspd->response == errSecCertificateRevoked) {
        rvc->status = ocspd->response;
        rvc->done = true;
        /* ocspd doesn't give us the nextUpdate time, so set to default */
        rvc->nextUpdate = SecPathBuilderGetVerifyTime(rvc->builder) + kSecDefaultCRLTTL;
        secdebug("rvc", "got CRL response for cert: %ld", rvc->certIX);
        SecCRVCUpdatePVC(rvc);
        if (!SecPathBuilderDecrementAsyncJobCount(builder)) {
            secdebug("rvc", "done with all async jobs");
            SecPathBuilderStep(builder);
        }
    } else {
        if (analytics) {
            /* We didn't get any data back, so the fetch failed */
            analytics->crl_fetch_failed++;
        }
        if(SecCRVCFetchNext(rvc)) {
            if (!SecPathBuilderDecrementAsyncJobCount(builder)) {
                secdebug("rvc", "done with all async jobs");
                SecPathBuilderStep(builder);
            }
        }
    }
}

static SecCRVCRef SecCRVCCreate(SecRVCRef rvc, SecPathBuilderRef builder, CFIndex certIX) {
    SecCRVCRef crvc = NULL;
    crvc = malloc(sizeof(struct OpaqueSecCRVC));
    if (crvc) {
        memset(crvc, 0, sizeof(struct OpaqueSecCRVC));
        crvc->builder = builder;
        crvc->rvc = rvc;
        crvc->certIX = certIX;
        crvc->status = errSecInternal;
        crvc->distributionPointIX = 0;
        crvc->distributionPoint = NULL;
        crvc->nextUpdate = NULL_TIME;
        crvc->async_ocspd.queue = SecPathBuilderGetQueue(builder);
        crvc->async_ocspd.completed = SecCRVCFetchCompleted;
        crvc->async_ocspd.response = errSecInternal;
        crvc->async_ocspd.info = crvc;
        crvc->done = false;
    }
    return crvc;
}

static bool SecRVCShouldCheckCRL(SecRVCRef rvc) {
    CFStringRef revocation_method = SecPathBuilderGetRevocationMethod(rvc->builder);
    TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(rvc->builder);
    if (revocation_method &&
        CFEqual(kSecPolicyCheckRevocationCRL, revocation_method)) {
        /* Our client insists on CRLs */
        secinfo("rvc", "client told us to check CRL");
        if (analytics) {
            analytics->crl_client = true;
        }
        return true;
    }
    SecCertificateRef cert = SecPathBuilderGetCertificateAtIndex(rvc->builder, rvc->certIX);
    CFArrayRef ocspResponders = SecCertificateGetOCSPResponders(cert);
    if ((!ocspResponders || CFArrayGetCount(ocspResponders) == 0) &&
        (revocation_method && !CFEqual(kSecPolicyCheckRevocationOCSP, revocation_method))) {
        /* The cert doesn't have OCSP responders and the client didn't specifically ask for OCSP.
         * This logic will skip the CRL cache check if the client didn't ask for revocation checking */
        secinfo("rvc", "client told us to check revocation and CRL is only option for cert: %ld", rvc->certIX);
        if (analytics) {
            analytics->crl_cert = true;
        }
        return true;
    }
    return false;
}
#endif /* ENABLE_CRLS */

void SecRVCDelete(SecRVCRef rvc) {
    if (rvc->orvc) {
        SecORVCFinish(rvc->orvc);
        free(rvc->orvc);
        rvc->orvc = NULL;
    }
#if ENABLE_CRLS
    if (rvc->crvc) {
        SecCRVCFinish(rvc->crvc);
        free(rvc->crvc);
        rvc->crvc = NULL;
    }
#endif
    if (rvc->valid_info) {
        SecValidInfoRelease(rvc->valid_info);
        rvc->valid_info = NULL;
    }
}

static void SecRVCInit(SecRVCRef rvc, SecPathBuilderRef builder, CFIndex certIX) {
    secdebug("alloc", "%p", rvc);
    rvc->builder = builder;
    rvc->certIX = certIX;
    rvc->orvc = SecORVCCreate(rvc, builder, certIX);
#if ENABLE_CRLS
    rvc->crvc = SecCRVCCreate(rvc, builder, certIX);
#endif
    if (!rvc->orvc
#if ENABLE_CRLS
        || !rvc->crvc
#endif
        ) {
        SecRVCDelete(rvc);
        rvc->done = true;
    } else {
        rvc->done = false;
    }
}

#if ENABLE_CRLS
static bool SecRVCShouldCheckOCSP(SecRVCRef rvc) {
    CFStringRef revocation_method = SecPathBuilderGetRevocationMethod(rvc->builder);
    if (!revocation_method
        || !CFEqual(revocation_method, kSecPolicyCheckRevocationCRL)) {
        return true;
    }
    return false;
}
#else
static bool SecRVCShouldCheckOCSP(SecRVCRef rvc) {
    return true;
}
#endif

static void SecRVCProcessValidDateConstraints(SecRVCRef rvc) {
    if (!rvc || !rvc->valid_info || !rvc->builder) {
        return;
    }
    if (!rvc->valid_info->hasDateConstraints) {
        return;
    }
    SecCertificateRef certificate = SecPathBuilderGetCertificateAtIndex(rvc->builder, rvc->certIX);
    if (!certificate) {
        return;
    }
    CFAbsoluteTime certIssued = SecCertificateNotValidBefore(certificate);
    CFAbsoluteTime caNotBefore = -3155760000.0; /* default: 1901-01-01 00:00:00-0000 */
    CFAbsoluteTime caNotAfter = 31556908800.0;  /* default: 3001-01-01 00:00:00-0000 */
    if (rvc->valid_info->notBeforeDate) {
        caNotBefore = CFDateGetAbsoluteTime(rvc->valid_info->notBeforeDate);
    }
    if (rvc->valid_info->notAfterDate) {
        caNotAfter = CFDateGetAbsoluteTime(rvc->valid_info->notAfterDate);
        /* per the Valid specification, if this date is in the past, we need to check CT. */
        CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
        if (caNotAfter < now) {
            rvc->valid_info->requireCT = true;
        }
    }
    if ((certIssued < caNotBefore) && (rvc->certIX > 0)) {
        /* not-before constraint is only applied to leaf certificate, for now. */
        return;
    }

    TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(rvc->builder);
    if ((certIssued < caNotBefore) || (certIssued > caNotAfter)) {
        /* We are outside the constrained validity period. */
        secnotice("rvc", "certificate issuance date not within the allowed range for this CA%s",
                  (rvc->valid_info->overridable) ? "" : " (non-recoverable error)");
        if (analytics) {
            analytics->valid_status |= TAValidDateContrainedRevoked;
        }
        if (rvc->valid_info->overridable) {
            /* error is recoverable, treat certificate as untrusted
               (note this date check is different from kSecPolicyCheckTemporalValidity) */
            SecPathBuilderSetResultInPVCs(rvc->builder, kSecPolicyCheckGrayListedKey, rvc->certIX,
                                          kCFBooleanFalse, true);
        } else {
            /* error is non-overridable, treat certificate as revoked */
            SInt32 reason = 0; /* unspecified reason code */
            CFNumberRef cfreason = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &reason);
            SecPathBuilderSetResultInPVCs(rvc->builder, kSecPolicyCheckRevocation, rvc->certIX,
                                          cfreason, true);
            CFMutableDictionaryRef info = SecPathBuilderGetInfo(rvc->builder);
            if (info) {
                /* make the revocation reason available in the trust result */
                CFDictionarySetValue(info, kSecTrustRevocationReason, cfreason);
            }
            CFReleaseNull(cfreason);
        }
    } else if (analytics) {
        analytics->valid_status |= TAValidDateConstrainedOK;
    }
}

bool SecRVCHasDefinitiveValidInfo(SecRVCRef rvc) {
    if (!rvc || !rvc->valid_info) {
        return false;
    }
    SecValidInfoRef info = rvc->valid_info;
    /* outcomes as defined in Valid server specification */
    if (info->format == kSecValidInfoFormatSerial ||
        info->format == kSecValidInfoFormatSHA256) {
        if (info->noCACheck || info->complete || info->isOnList) {
            return true;
        }
    } else { /* info->format == kSecValidInfoFormatNto1 */
        if (info->noCACheck || (info->complete && !info->isOnList)) {
            return true;
        }
    }
    return false;
}

bool SecRVCHasRevokedValidInfo(SecRVCRef rvc) {
    if (!rvc || !rvc->valid_info) {
        return false;
    }
    SecValidInfoRef info = rvc->valid_info;
    /* either not present on an allowlist, or present on a blocklist */
    return (!info->isOnList && info->valid) || (info->isOnList && !info->valid);
}

void SecRVCSetRevokedResult(SecRVCRef rvc) {
    if (!rvc || !rvc->valid_info || !rvc->builder) {
        return;
    }
    if (rvc->valid_info->overridable) {
        /* error is recoverable, treat certificate as untrusted */
        SecPathBuilderSetResultInPVCs(rvc->builder, kSecPolicyCheckGrayListedKey, rvc->certIX,
                                      kCFBooleanFalse, true);
        return;
    }
    /* error is fatal, treat certificate as revoked */
    SInt32 reason = 0; /* unspecified, since the Valid db doesn't tell us */
    CFNumberRef cfreason = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &reason);
    SecPathBuilderSetResultInPVCs(rvc->builder, kSecPolicyCheckRevocation, rvc->certIX,
                                  cfreason, true);
    CFMutableDictionaryRef info = SecPathBuilderGetInfo(rvc->builder);
    if (info) {
        /* make the revocation reason available in the trust result */
        CFDictionarySetValue(info, kSecTrustRevocationReason, cfreason);
    }
    CFReleaseNull(cfreason);
}

static void SecRVCProcessValidInfoResults(SecRVCRef rvc) {
    if (!rvc || !rvc->valid_info || !rvc->builder) {
        return;
    }
    SecCertificatePathVCRef path = SecPathBuilderGetPath(rvc->builder);
    SecValidInfoRef info = rvc->valid_info;

    bool definitive = SecRVCHasDefinitiveValidInfo(rvc);
    bool revoked = SecRVCHasRevokedValidInfo(rvc);

    /* set analytics */
    TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(rvc->builder);
    if (analytics) {
        if (revoked) {
            analytics->valid_status |= definitive ? TAValidDefinitelyRevoked : TAValidProbablyRevoked;
        } else {
            analytics->valid_status |= definitive ? TAValidDefinitelyOK : TAValidProbablyOK;
        }
    }

    /* Handle no-ca cases */
    if (info->noCACheck) {
        bool allowed = (info->valid && info->complete && info->isOnList);
        if (revoked) {
            /* definitely revoked */
            SecRVCSetRevokedResult(rvc);
        } else if (allowed) {
            /* definitely not revoked (allowlisted) */
            SecCertificatePathVCSetIsAllowlisted(path, true);
        }
        /* no-ca is definitive; no need to check further. */
        secdebug("validupdate", "rvc: definitely %s cert %" PRIdCFIndex,
                 (allowed) ? "allowed" : "revoked", rvc->certIX);
        rvc->done = true;
        return;
    }

    /* Handle date constraints, if present.
     * Note: a not-after date may set the CT requirement,
     * so check requireCT after this function is called. */
    SecRVCProcessValidDateConstraints(rvc);

    /* Set CT requirement on path, if present. */
    if (info->requireCT) {
        if (analytics) {
            analytics->valid_require_ct |= info->requireCT;
        }
        SecPathCTPolicy ctp = kSecPathCTRequired;
        if (info->overridable) {
            ctp = kSecPathCTRequiredOverridable;
        }
        SecCertificatePathVCSetRequiresCT(path, ctp);
    }

    /* Trigger OCSP for any non-definitive or revoked cases */
    if (!definitive || revoked) {
        info->checkOCSP = true;
    }

    if (info->checkOCSP) {
        if (analytics) {
            /* Valid DB results caused us to do OCSP */
            analytics->valid_trigger_ocsp = true;
        }
        CFIndex count = SecPathBuilderGetCertificateCount(rvc->builder);
        CFIndex issuerIX = rvc->certIX + 1;
        if (issuerIX >= count) {
            /* cannot perform a revocation check on the last cert in the
               chain, since we don't have its issuer. */
            return;
        }
        CFIndex pvcIX;
        for (pvcIX = 0; pvcIX < SecPathBuilderGetPVCCount(rvc->builder); pvcIX++) {
            SecPVCRef pvc = SecPathBuilderGetPVCAtIndex(rvc->builder, pvcIX);
            if (!pvc) { continue; }
            SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(pvc->policies, 0);
            CFStringRef policyName = (policy) ? SecPolicyGetName(policy) : NULL;
            if (policyName && CFEqual(CFSTR("sslServer"), policyName)) {
                /* perform revocation check for SSL policy;
                 require for leaf if an OCSP responder is present. */
                if (0 == rvc->certIX) {
                    SecCertificateRef cert = SecPathBuilderGetCertificateAtIndex(rvc->builder, rvc->certIX);
                    CFArrayRef resps = (cert) ? SecCertificateGetOCSPResponders(cert) : NULL;
                    CFIndex rcount = (resps) ? CFArrayGetCount(resps) : 0;
                    if (rcount > 0) {
                        // %%% rdar://31279923
                        // This currently requires a valid revocation response for each cert,
                        // but we only want to require a leaf check. For now, do not require.
                        //SecPathBuilderSetRevocationResponseRequired(rvc->builder);
                    }
                }
                secdebug("validupdate", "rvc: %s%s cert %" PRIdCFIndex " (will check OCSP)",
                         (info->complete) ? "" : "possibly ", (info->valid) ? "allowed" : "revoked",
                         rvc->certIX);
                SecPathBuilderSetRevocationMethod(rvc->builder, kSecPolicyCheckRevocationAny);
            }
        }
    }
}

static bool SecRVCCheckValidInfoDatabase(SecRVCRef rvc) {
    /* Skip checking for OCSP Signer verification */
    if (SecPathBuilderGetPVCCount(rvc->builder) == 1) {
        SecPVCRef pvc = SecPathBuilderGetPVCAtIndex(rvc->builder, 0);
        if (!pvc) { return false; }
        SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(pvc->policies, 0);
        CFStringRef policyName = (policy) ? SecPolicyGetName(policy) : NULL;
        if (policyName && CFEqual(policyName, CFSTR("OCSPSigner"))) {
            return false;
        }
    }

    /* Make sure revocation db info is up-to-date.
     * We don't care if the builder is allowed to access the network because
     * the network fetching does not block the trust evaluation. */
    SecRevocationDbCheckNextUpdate();

    /* Check whether we have valid db info for this cert,
     given the cert and its issuer */
    SecValidInfoRef info = NULL;
    CFIndex count = SecPathBuilderGetCertificateCount(rvc->builder);
    if (count) {
        bool isSelfSigned = false;
        SecCertificateRef cert = NULL;
        SecCertificateRef issuer = NULL;
        CFIndex issuerIX = rvc->certIX + 1;
        if (count > issuerIX) {
            issuer = SecPathBuilderGetCertificateAtIndex(rvc->builder, issuerIX);
        } else if (count == issuerIX) {
            CFIndex rootIX = SecCertificatePathVCSelfSignedIndex(SecPathBuilderGetPath(rvc->builder));
            if (rootIX == rvc->certIX) {
                issuer = SecPathBuilderGetCertificateAtIndex(rvc->builder, rootIX);
                isSelfSigned = true;
            }
        }
        cert = SecPathBuilderGetCertificateAtIndex(rvc->builder, rvc->certIX);
        if (!isSelfSigned) {
            /* skip revocation db check for self-signed certificates [33137065] */
            info = SecRevocationDbCopyMatching(cert, issuer);
        }
        SecValidInfoSetAnchor(info, SecPathBuilderGetCertificateAtIndex(rvc->builder, count-1));
    }
    if (info) {
        SecValidInfoRef old_info = rvc->valid_info;
        rvc->valid_info = info;
        if (old_info) {
            SecValidInfoRelease(old_info);
        }
        return true;
    }
    return false;
}

static void SecRVCCheckRevocationCaches(SecRVCRef rvc) {
    /* Don't check OCSP cache if CRLs enabled and policy requested CRL only */
    if (SecRVCShouldCheckOCSP(rvc) && (rvc->orvc->ocspRequest)) {
        secdebug("ocsp", "Checking cached responses for cert %ld", rvc->certIX);
        SecOCSPResponseRef response = NULL;
        if (SecPathBuilderGetCheckRevocationOnline(rvc->builder)) {
            CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
            response = SecOCSPCacheCopyMatchingWithMinInsertTime(rvc->orvc->ocspRequest, NULL, now - kSecOCSPResponseOnlineTTL);
        } else {
            response = SecOCSPCacheCopyMatching(rvc->orvc->ocspRequest, NULL);
        }
        SecORVCConsumeOCSPResponse(rvc->orvc,
                                   response,
                                   NULL_TIME, false);
        TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(rvc->builder);
        if (rvc->orvc->done && analytics) {
            /* We found a valid OCSP response in the cache */
            analytics->ocsp_cache_hit = true;
        }
    }
#if ENABLE_CRLS
    /* Don't check CRL cache if policy requested OCSP only */
    if (SecRVCShouldCheckCRL(rvc)) {
        SecCRVCCheckRevocationCache(rvc->crvc);
    }
#endif
}

static void SecRVCUpdatePVC(SecRVCRef rvc) {
    SecRVCProcessValidInfoResults(rvc); /* restore the results we got from Valid */
    if (rvc->orvc) { SecORVCUpdatePVC(rvc->orvc); }
#if ENABLE_CRLS
    if (rvc->crvc) { SecCRVCUpdatePVC(rvc->crvc); }
#endif
}

static bool SecRVCFetchNext(SecRVCRef rvc) {
    bool OCSP_fetch_finished = true;
    TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(rvc->builder);
    /* Don't send OCSP request only if CRLs enabled and policy requested CRL only */
    if (SecRVCShouldCheckOCSP(rvc)) {
        OCSP_fetch_finished &= SecORVCFetchNext(rvc->orvc);
    }
    if (OCSP_fetch_finished) {
        /* we didn't start an OCSP background job for this cert */
        (void)SecPathBuilderDecrementAsyncJobCount(rvc->builder);
    } else if (analytics) {
        /* We did a network OCSP fetch, set report appropriately */
        analytics->ocsp_network = true;
        analytics->ocsp_fetches++;
    }

#if ENABLE_CRLS
    bool CRL_fetch_finished = true;
    /* Don't check CRL cache if policy requested OCSP only */
    if (SecRVCShouldCheckCRL(rvc)) {
        /* reset the distributionPointIX because we already iterated through the CRLDPs
         * in SecCRVCCheckRevocationCache */
        rvc->crvc->distributionPointIX = 0;
        CRL_fetch_finished &= SecCRVCFetchNext(rvc->crvc);
    }
    if (CRL_fetch_finished) {
        /* we didn't start a CRL background job for this cert */
        (void)SecPathBuilderDecrementAsyncJobCount(rvc->builder);
    } else if (analytics) {
        /* We did a CRL fetch */
        analytics->crl_fetches++;
    }
    OCSP_fetch_finished &= CRL_fetch_finished;
#endif

    return OCSP_fetch_finished;
}

/* The SecPathBuilder state machine calls SecPathBuilderCheckRevocation twice --
 * once in the ValidatePath state, and again in the ComputeDetails state. In the
 * ValidatePath state we've not yet run the path checks, so for callers who set
 * kSecRevocationCheckIfTrusted, we don't do any networking on that first call.
 * Here, if we've already done revocation before (so we're in ComputeDetails now),
 * we need to recheck (and enable networking) for trusted chains and
 * kSecRevocationCheckIfTrusted. Otherwise, we skip the checks to save on the processing
 * but update the PVCs with the revocation results from the last check. */
static bool SecRevocationDidCheckRevocation(SecPathBuilderRef builder, bool *first_check_done) {
    SecCertificatePathVCRef path = SecPathBuilderGetPath(builder);
    if (!SecCertificatePathVCIsRevocationDone(path)) {
        return false;
    }
    if (first_check_done) {
        *first_check_done = true;
    }

    SecPVCRef resultPVC = SecPathBuilderGetResultPVC(builder);
    bool recheck = false;
    if (SecPathBuilderGetCheckRevocationIfTrusted(builder) && SecPVCIsOkResult(resultPVC)) {
        recheck = true;
        secdebug("rvc", "Rechecking revocation because network now allowed");
    } else {
        secdebug("rvc", "Not rechecking revocation");
    }

    CFIndex certIX, certCount = SecPathBuilderGetCertificateCount(builder);
    for (certIX = 0; certIX < certCount; ++certIX) {
        SecRVCRef rvc = SecCertificatePathVCGetRVCAtIndex(path, certIX);
        if (rvc && !recheck) {
            SecRVCUpdatePVC(rvc);
        } else if (rvc) {
            SecRVCDelete(rvc); // reset the RVC for the second pass
        }
    }
    return !recheck;
}

static bool SecRevocationCanAccessNetwork(SecPathBuilderRef builder, bool first_check_done) {
    /* CheckRevocationIfTrusted overrides NoNetworkAccess for revocation */
    if (SecPathBuilderGetCheckRevocationIfTrusted(builder)) {
        if (first_check_done) {
            /* We're on the second pass. We need to now allow networking for revocation.
             * SecRevocationDidCheckRevocation takes care of not running a second pass
             * if the chain isn't trusted. */
            return true;
        } else {
            /* We're on the first pass of the revocation checks, where we aren't
             * supposed to do networking because we don't know if the chain
             * is trusted yet. */
            return false;
        }
    }
    return SecPathBuilderCanAccessNetwork(builder);
}

bool SecPathBuilderCheckRevocation(SecPathBuilderRef builder) {
    secdebug("rvc", "checking revocation");
    CFIndex certIX, certCount = SecPathBuilderGetCertificateCount(builder);
    SecCertificatePathVCRef path = SecPathBuilderGetPath(builder);
    bool completed = true;
    if (certCount <= 1) {
        /* Can't verify without an issuer; we're done */
        return completed;
    }

    bool first_check_done = false;
    if (SecRevocationDidCheckRevocation(builder, &first_check_done)) {
        return completed;
    }

    /* Setup things so we check revocation status of all certs. */
    SecCertificatePathVCAllocateRVCs(path, certCount);

    /* Note that if we are multi threaded and a job completes after it
     is started but before we return from this function, we don't want
     a callback to decrement asyncJobCount to zero before we finish issuing
     all the jobs. To avoid this we pretend we issued certCount-1 async jobs,
     and decrement pvc->asyncJobCount for each cert that we don't start a
     background fetch for. (We will never start an async job for the final
     cert in the chain.) */
#if !ENABLE_CRLS
    SecPathBuilderSetAsyncJobCount(builder, (unsigned int)(certCount-1));
#else
    /* If we enable CRLS, we may end up with two async jobs per cert: one
     * for OCSP and one for fetching the CRL */
    SecPathBuilderSetAsyncJobCount(builder, 2 * (unsigned int)(certCount-1));
#endif

    /* Loop though certificates again and issue an ocsp fetch if the
     revocation status checking isn't done yet (and we have an issuer!) */
    for (certIX = 0; certIX < certCount; ++certIX) {
        secdebug("rvc", "checking revocation for cert: %ld", certIX);
        SecRVCRef rvc = SecCertificatePathVCGetRVCAtIndex(path, certIX);
        if (!rvc) {
            continue;
        }

        SecRVCInit(rvc, builder, certIX);

        /* RFC 6960: OCSP No-Check extension says that we shouldn't check revocation. */
        if (SecCertificateHasMarkerExtension(SecCertificatePathVCGetCertificateAtIndex(path, certIX),
                                             CFSTR("1.3.6.1.5.5.7.48.1.5"))) // id-pkix-ocsp-nocheck
        {
            secdebug("rvc", "skipping revocation checks for no-check cert: %ld", certIX);
            TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(builder);
            if (analytics) {
                /* This certificate has OCSP No-Check, so add to reporting analytics */
                analytics->ocsp_no_check = true;
            }
            rvc->done = true;
        }

        if (rvc->done) {
            continue;
        }

#if !TARGET_OS_BRIDGE
        /* Check valid database first (separate from OCSP response cache) */
        if (SecRVCCheckValidInfoDatabase(rvc)) {
            SecRVCProcessValidInfoResults(rvc);
        }
#endif
        /* Any other revocation method requires an issuer certificate to verify the response;
         * skip the last cert in the chain since it doesn't have one. */
        if (certIX+1 >= certCount) {
            continue;
        }

        /* Ignore stapled OCSP responses only if CRLs are enabled and the
         * policy specifically requested CRLs only. */
        if (SecRVCShouldCheckOCSP(rvc)) {
            /*  If we have any OCSP stapled responses, check those first */
            SecORVCProcessStapledResponses(rvc->orvc);
        }

#if TARGET_OS_BRIDGE
        /* The bridge has no writeable storage and no network. Nothing else we can
         * do here. */
        rvc->done = true;
        return completed;
#endif

        /* Then check the caches for revocation results. */
        SecRVCCheckRevocationCaches(rvc);

        /* The check is done if we found cached responses from either method. */
        if (rvc->orvc->done
#if ENABLE_CRLS
            || rvc->crvc->done
#endif
            ) {
            secdebug("rvc", "found cached response for cert: %ld", certIX);
            rvc->done = true;
        }

        /* If we got a cached response that is no longer valid (which can only be true for
         * revoked responses), let's try to get a fresher response even if no one asked.
         * This check resolves unrevocation events after the nextUpdate time. */
        bool old_cached_response = (!rvc->done && rvc->orvc->ocspResponse);

        /* If the cert is EV or if revocation checking was explicitly enabled, attempt to fire off an
         async http request for this cert's revocation status, unless we already successfully checked
         the revocation status of this cert based on the cache or stapled responses.  */
        bool allow_fetch = SecRevocationCanAccessNetwork(builder, first_check_done) &&
            (SecCertificatePathVCIsEV(path) || SecCertificatePathVCIsOptionallyEV(path) ||
             SecPathBuilderGetRevocationMethod(builder) || old_cached_response);
        bool fetch_done = true;
        if (rvc->done || !allow_fetch) {
            /* We got a cache hit or we aren't allowed to access the network */
            SecRVCUpdatePVC(rvc);
            /* We didn't really start any background jobs for this cert. */
            (void)SecPathBuilderDecrementAsyncJobCount(builder);
#if ENABLE_CRLS
            (void)SecPathBuilderDecrementAsyncJobCount(builder);
#endif
        } else {
            fetch_done = SecRVCFetchNext(rvc);
        }
        if (!fetch_done) {
            /* We started at least one background fetch. */
            secdebug("rvc", "waiting on background fetch for cert %ld", certIX);
            completed = false;
        }
    }

    /* Return false if we started any background jobs. */
    /* We can't just return !builder->asyncJobCount here, since if we started any
     jobs the completion callback will be called eventually and it will call
     SecPathBuilderStep(). If for some reason everything completed before we
     get here we still want the outer SecPathBuilderStep() to terminate so we
     keep track of whether we started any jobs and return false if so. */
    return completed;
}

CFAbsoluteTime SecRVCGetEarliestNextUpdate(SecRVCRef rvc) {
    CFAbsoluteTime enu = NULL_TIME;
    if (!rvc || !rvc->orvc) { return enu; }
    enu = rvc->orvc->nextUpdate;
#if ENABLE_CRLS
    CFAbsoluteTime crlNextUpdate = rvc->crvc->nextUpdate;
    if (enu == NULL_TIME ||
        ((crlNextUpdate > NULL_TIME) && (enu > crlNextUpdate))) {
        /* We didn't check OCSP or CRL next update time was sooner */
        enu = crlNextUpdate;
    }
#endif
    return enu;
}
