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
 * SecCAIssuerRequest.m - asynchronous CAIssuer request fetching engine.
 */


#include "SecCAIssuerRequest.h"
#include "SecCAIssuerCache.h"

#import <Foundation/Foundation.h>
#import <CFNetwork/CFNSURLConnection.h>
#include <Security/SecInternal.h>
#include <Security/SecCMS.h>
#include <CoreFoundation/CFURL.h>
#include <CFNetwork/CFHTTPMessage.h>
#include <utilities/debugging.h>
#include <Security/SecCertificateInternal.h>
#include <securityd/SecTrustServer.h>
#include <securityd/TrustURLSessionDelegate.h>
#include <stdlib.h>
#include <mach/mach_time.h>

#define MAX_CA_ISSUERS 3
#define CA_ISSUERS_REQUEST_THRESHOLD 10

typedef void (*CompletionHandler)(void *context, CFArrayRef parents);

/* CA Issuer lookup code. */
@interface CAIssuerDelegate: TrustURLSessionDelegate
@property (assign) CompletionHandler callback;
@end

static NSArray *certificatesFromData(NSData *data) {
    /* RFC5280 4.2.2.1:
     "accessLocation MUST be a uniformResourceIdentifier and the URI
     MUST point to either a single DER encoded certificate as speci-
     fied in [RFC2585] or a collection of certificates in a BER or
     DER encoded "certs-only" CMS message as specified in [RFC2797]." */

    /* DER-encoded certificate */
    SecCertificateRef parent = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)data);
    if (parent) {
        NSArray *result = @[(__bridge id)parent];
        CFReleaseNull(parent);
        return result;
    }

    /* "certs-only" CMS Message */
    CFArrayRef certificates = SecCMSCertificatesOnlyMessageCopyCertificates((__bridge CFDataRef)data);
    if (certificates) {
        return CFBridgingRelease(certificates);
    }

    /* Retry in case the certificate is in PEM format. Some CAs
     incorrectly return a PEM encoded cert, despite RFC 5280 4.2.2.1 */
    parent = SecCertificateCreateWithPEM(NULL, (__bridge CFDataRef)data);
    if (parent) {
        NSArray *result = @[(__bridge id)parent];
        CFReleaseNull(parent);
        return result;
    }
    return nil;
}

@implementation CAIssuerDelegate
- (BOOL)fetchNext:(NSURLSession *)session {
    SecPathBuilderRef builder = (SecPathBuilderRef)self.context;
    TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(builder);

    BOOL result = false;
    if (!(result = [super fetchNext:session]) && analytics) {
        analytics->ca_issuer_fetches++;
    }
    return result;
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didCompleteWithError:(NSError *)error {
    /* call the superclass's method to set taskTime and expiration */
    [super URLSession:session task:task didCompleteWithError:error];

    __block SecPathBuilderRef builder =(SecPathBuilderRef)self.context;
    if (!builder) {
        /* We already returned to the PathBuilder state machine. */
        return;
    }

    TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(builder);
    __block NSArray *parents = nil;
    if (error) {
        /* Log the error */
        secnotice("caissuer", "Failed to download issuer %@, with error %@", task.originalRequest.URL, error);
        if (analytics) {
            analytics->ca_issuer_fetch_failed++;
        }
    } else if (self.response) {
        /* Get the parent cert from the data */
        parents = certificatesFromData(self.response);
        if (analytics && !parents) {
            analytics->ca_issuer_unsupported_data = true;
        } else if (analytics && [parents count] > 1) {
            analytics->ca_issuer_multiple_certs = true;
        }
    }

    if (parents)  {
        /* Found some parents, add to cache, close session, and return to SecPathBuilder */
        secdebug("caissuer", "found parents for %@", task.originalRequest.URL);
        SecCAIssuerCacheAddCertificates((__bridge CFArrayRef)parents, (__bridge CFURLRef)task.originalRequest.URL, self.expiration);
        self.context = nil; // set the context to NULL before we call back because the callback may free the builder
        [session invalidateAndCancel];
        dispatch_async(SecPathBuilderGetQueue(builder), ^{
            self.callback(builder, (__bridge CFArrayRef)parents);
        });
    } else {
        secdebug("caissuer", "no parents for %@", task.originalRequest.URL);
        if ([self fetchNext:session]) { // Try the next CAIssuer URI
            /* no fetch scheduled, close this session and jump back into the state machine on the builder's queue */
            secdebug("caissuer", "no more fetches. returning to builder");
            self.context = nil; // set the context to NULL before we call back because the callback may free the builder
            [session invalidateAndCancel];
            dispatch_async(SecPathBuilderGetQueue(builder), ^{
                self.callback(builder, NULL);
            });
        }
    }
}

- (void)URLSession:(NSURLSession *)session task:(NSURLSessionTask *)task didFinishCollectingMetrics:(NSURLSessionTaskMetrics *)taskMetrics {
    secdebug("caissuer", "got metrics with task interval %f", taskMetrics.taskInterval.duration);
    SecPathBuilderRef builder =(SecPathBuilderRef)self.context;
    if (builder) {
        TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(builder);
        if (analytics) {
            analytics->ca_issuer_fetch_time += (uint64_t)(taskMetrics.taskInterval.duration * NSEC_PER_SEC);
        }
    }
}
@end

/* Releases parent unconditionally, and return a CFArrayRef containing
 parent if the normalized subject of parent matches the normalized issuer
 of certificate. */
static CF_RETURNS_RETAINED CFArrayRef SecCAIssuerConvertToParents(SecCertificateRef certificate, CFArrayRef CF_CONSUMED putativeParents) {
    CFDataRef nic = SecCertificateGetNormalizedIssuerContent(certificate);
    NSMutableArray *parents = [NSMutableArray array];
    NSArray *possibleParents = CFBridgingRelease(putativeParents);
    for (id parent in possibleParents) {
        CFDataRef parent_nic = SecCertificateGetNormalizedSubjectContent((__bridge SecCertificateRef)parent);
        if (nic && parent_nic && CFEqual(nic, parent_nic)) {
            [parents addObject:parent];
        }
    }

    if ([parents count] > 0) {
        return CFBridgingRetain(parents);
    } else {
        return nil;
    }
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
                    secdebug("caissuer", "cache hit, for %@. no request issued", issuer);
                    CFRelease(scheme);
                    return parents;
                }
            }
            CFRelease(scheme);
        }
    }
    return NULL;
}

bool SecCAIssuerCopyParents(SecCertificateRef certificate, void *context, void (*callback)(void *, CFArrayRef)) {
    @autoreleasepool {
        CFArrayRef issuers = CFRetainSafe(SecCertificateGetCAIssuers(certificate));
        NSArray *nsIssuers = CFBridgingRelease(issuers);
        if (!issuers) {
            /* certificate has no caissuer urls, we're done. */
            callback(context, NULL);
            return true;
        }

        SecPathBuilderRef builder = (SecPathBuilderRef)context;
        TrustAnalyticsBuilder *analytics = SecPathBuilderGetAnalyticsData(builder);
        CFArrayRef parents = SecCAIssuerRequestCacheCopyParents(certificate, (__bridge CFArrayRef)nsIssuers);
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

        NSInteger count = [nsIssuers count];
        if (count >= CA_ISSUERS_REQUEST_THRESHOLD) {
            secnotice("caissuer", "too many caIssuer entries (%ld)", (long)count);
            callback(context, NULL);
            return true;
        }

        NSURLSessionConfiguration *config = [NSURLSessionConfiguration ephemeralSessionConfiguration];
        config.timeoutIntervalForResource = TrustURLSessionGetResourceTimeout();
        config.HTTPAdditionalHeaders = @{@"User-Agent" : @"com.apple.trustd/2.0"};

        NSData *auditToken = CFBridgingRelease(SecPathBuilderCopyClientAuditToken(builder));
        if (auditToken) {
            config._sourceApplicationAuditTokenData = auditToken;
        }

        CAIssuerDelegate *delegate = [[CAIssuerDelegate alloc] init];
        delegate.context = context;
        delegate.callback = callback;
        delegate.URIs = nsIssuers;
        delegate.URIix = 0;

        NSOperationQueue *queue = [[NSOperationQueue alloc] init];

        NSURLSession *session = [NSURLSession sessionWithConfiguration:config delegate:delegate delegateQueue:queue];
        secdebug("caissuer", "created URLSession for %@", certificate);

        bool result = false;
        if ((result = [delegate fetchNext:session])) {
            /* no fetch scheduled, close the session */
            [session invalidateAndCancel];
        }
        return result;
    }
}

