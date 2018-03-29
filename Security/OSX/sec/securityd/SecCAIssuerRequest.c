/*
 * Copyright (c) 2009-2018 Apple Inc. All Rights Reserved.
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
 * SecCAIssuerRequest.c - asynchronous CAIssuer request fetching engine.
 */


#include "SecCAIssuerRequest.h"
#include "SecCAIssuerCache.h"

#include <Security/SecInternal.h>
#include <Security/SecCMS.h>
#include <CoreFoundation/CFURL.h>
#include <CFNetwork/CFHTTPMessage.h>
#include <utilities/debugging.h>
#include <Security/SecCertificateInternal.h>
#include <securityd/asynchttp.h>
#include <securityd/SecTrustServer.h>
#include <stdlib.h>
#include <mach/mach_time.h>

#define MAX_CA_ISSUERS 3
#define CA_ISSUERS_REQUEST_THRESHOLD 10


/* CA Issuer lookup code. */

typedef struct SecCAIssuerRequest *SecCAIssuerRequestRef;
struct SecCAIssuerRequest {
    asynchttp_t http;   /* Must be first field. */
    SecCertificateRef certificate;
    CFArrayRef issuers; /* NONRETAINED */
    CFIndex issuerIX;
    void *context;
    void (*callback)(void *, CFArrayRef);
};

static void SecCAIssuerRequestRelease(SecCAIssuerRequestRef request) {
    CFRelease(request->certificate);
    asynchttp_free(&request->http);
    free(request);
}

static bool SecCAIssuerRequestIssue(SecCAIssuerRequestRef request) {
    CFIndex count = CFArrayGetCount(request->issuers);
    if (count >= CA_ISSUERS_REQUEST_THRESHOLD) {
        secnotice("caissuer", "too many caIssuer entries (%ld)", (long)count);
        request->callback(request->context, NULL);
        SecCAIssuerRequestRelease(request);
        return true;
    }

    SecPathBuilderRef builder = (SecPathBuilderRef)request->context;
    TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(builder);

    while (request->issuerIX < count && request->issuerIX < MAX_CA_ISSUERS) {
        CFURLRef issuer = CFArrayGetValueAtIndex(request->issuers,
                                                 request->issuerIX++);
        CFStringRef scheme = CFURLCopyScheme(issuer);
        if (scheme) {
            if (CFEqual(CFSTR("http"), scheme)) {
                CFHTTPMessageRef msg = CFHTTPMessageCreateRequest(kCFAllocatorDefault,
                                                                  CFSTR("GET"), issuer, kCFHTTPVersion1_1);
                if (msg) {
                    secinfo("caissuer", "%@", msg);
                    bool done = asynchttp_request(msg, 0, &request->http);
                    if (analytics) {
                        /* Count each http request we made */
                        analytics->ca_issuer_fetches++;
                    }
                    CFRelease(msg);
                    if (done == false) {
                        CFRelease(scheme);
                        return done;
                    }
                }
                secdebug("caissuer", "failed to get %@", issuer);
            } else {
                secnotice("caissuer", "skipping unsupported uri %@", issuer);
            }
            CFRelease(scheme);
        }
    }

    /* No more issuers left to try, we're done. */
    secdebug("caissuer", "no request issued");
    request->callback(request->context, NULL);
    SecCAIssuerRequestRelease(request);
    return true;
}

/* Releases parent unconditionally, and return a CFArrayRef containing
   parent if the normalized subject of parent matches the normalized issuer
   of certificate. */
static CF_RETURNS_RETAINED CFArrayRef SecCAIssuerConvertToParents(SecCertificateRef certificate,
    SecCertificateRef CF_CONSUMED parent) {
    CFDataRef nic = SecCertificateGetNormalizedIssuerContent(certificate);
    CFArrayRef parents = NULL;
    if (parent) {
        CFDataRef parent_nic = SecCertificateGetNormalizedSubjectContent(parent);
        if (nic && parent_nic && CFEqual(nic, parent_nic)) {
            const void *ventry = parent;
            parents = CFArrayCreate(NULL, &ventry, 1, &kCFTypeArrayCallBacks);
        }
        CFRelease(parent);
    }
    return parents;
}

#define SECONDS_PER_DAY (86400.0)
static void SecCAIssuerRequestCompleted(asynchttp_t *http,
    CFTimeInterval maxAge) {
    /* Cast depends on http being first field in struct SecCAIssuerRequest. */
    SecCAIssuerRequestRef request = (SecCAIssuerRequestRef)http;

    SecPathBuilderRef builder = (SecPathBuilderRef)request->context;
    TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(builder);
    if (analytics) {
        /* Add the time this fetch took to complete to the total time */
        analytics->ca_issuer_fetch_time += (mach_absolute_time() - http->start_time);
    }

    CFDataRef data = (request->http.response ?
        CFHTTPMessageCopyBody(request->http.response) : NULL);
    if (data) {
        /* RFC5280 4.2.2.1:
        "accessLocation MUST be a uniformResourceIdentifier and the URI
        MUST point to either a single DER encoded certificate as speci-
        fied in [RFC2585] or a collection of certificates in a BER or
        DER encoded "certs-only" CMS message as specified in [RFC2797]." */

        /* DER-encoded certificate */
        SecCertificateRef parent = SecCertificateCreateWithData(NULL, data);

        /* "certs-only" CMS Message */
        if (!parent) {
            CFArrayRef certificates = NULL;
            certificates = SecCMSCertificatesOnlyMessageCopyCertificates(data);
            /* @@@ Technically these can have more than one certificate */
            if (certificates && CFArrayGetCount(certificates) >= 1) {
                parent = CFRetainSafe((SecCertificateRef)CFArrayGetValueAtIndex(certificates, 0));
            } else if (certificates && CFArrayGetCount(certificates) > 1) {
                if (analytics) {
                    /* Indicate that this trust evaluation encountered a CAIssuer fetch with multiple certs */
                    analytics->ca_issuer_multiple_certs = true;
                }
            }
            CFReleaseNull(certificates);
        }

        /* Retry in case the certificate is in PEM format. Some CAs
         incorrectly return a PEM encoded cert, despite RFC 5280 4.2.2.1 */
        if (!parent) {
            parent = SecCertificateCreateWithPEM(NULL, data);
        }
        CFRelease(data);
        if (parent) {
            /* We keep responses in the cache for at least 7 days, or longer
             if the http response tells us to keep it around for more. */
            if (maxAge < SECONDS_PER_DAY * 7)
                maxAge = SECONDS_PER_DAY * 7;
            CFAbsoluteTime expires = CFAbsoluteTimeGetCurrent() + maxAge;
            CFURLRef issuer = CFArrayGetValueAtIndex(request->issuers,
                                                     request->issuerIX - 1);
            SecCAIssuerCacheAddCertificate(parent, issuer, expires);
            CFArrayRef parents = SecCAIssuerConvertToParents(
                request->certificate, parent); /* note: this releases parent */
            if (parents) {
                secdebug("caissuer", "response: %@ good", http->response);
                request->callback(request->context, parents);
                CFRelease(parents);
                SecCAIssuerRequestRelease(request);
                return;
            }
        } else if (analytics) {
            /* We failed to create a SecCertificateRef from the data we got */
            analytics->ca_issuer_unsupported_data = true;
        }
    } else if (analytics) {
        /* We didn't get any data back, so the fetch failed */
        analytics->ca_issuer_fetch_failed++;
    }

    secdebug("caissuer", "response: %@ not parent, trying next caissuer",
        http->response);
    /* We're re-using this http object, so we need to free all the old memory. */
    asynchttp_free(&request->http);
    SecCAIssuerRequestIssue(request);
}

static CFArrayRef SecCAIssuerRequestCacheCopyParents(SecCertificateRef cert,
    CFArrayRef issuers) {
    CFIndex ix = 0, ex = CFArrayGetCount(issuers);
    for (;ix < ex; ++ix) {
        CFURLRef issuer = CFArrayGetValueAtIndex(issuers, ix);
        CFStringRef scheme = CFURLCopyScheme(issuer);
        if (scheme) {
            if (CFEqual(CFSTR("http"), scheme)) {
                CFArrayRef parents = SecCAIssuerConvertToParents(cert,
                    SecCAIssuerCacheCopyMatching(issuer));
                if (parents) {
                    secdebug("caissuer", "cache hit, for %@ no request issued", issuer);
                    CFRelease(scheme);
                    return parents;
                }
            }
            CFRelease(scheme);
        }
    }
    return NULL;
}

bool SecCAIssuerCopyParents(SecCertificateRef certificate, dispatch_queue_t queue,
    void *context, void (*callback)(void *, CFArrayRef)) {
    CFArrayRef issuers = SecCertificateGetCAIssuers(certificate);
    if (!issuers) {
        /* certificate has no caissuer urls, we're done. */
        callback(context, NULL);
        return true;
    }

    SecPathBuilderRef builder = (SecPathBuilderRef)context;
    TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(builder);
    CFArrayRef parents = SecCAIssuerRequestCacheCopyParents(certificate, issuers);
    if (parents) {
        if (analytics) {
            /* We found parents in the cache */
            analytics->ca_issuer_cache_hit = true;
        }
        callback(context, parents);
        CFReleaseSafe(parents);
        return true;
    }
    if (analytics) {
        /* We're going to have to make a network call */
        analytics->ca_issuer_network = true;
    }

    /* Cache miss, let's issue a network request. */
    SecCAIssuerRequestRef request =
        (SecCAIssuerRequestRef)calloc(1, sizeof(*request));
    request->http.queue = queue;
    request->http.completed = SecCAIssuerRequestCompleted;
    CFRetain(certificate);
    request->certificate = certificate;
    request->issuers = issuers;
    request->issuerIX = 0;
    request->context = context;
    request->callback = callback;

    return SecCAIssuerRequestIssue(request);
}

