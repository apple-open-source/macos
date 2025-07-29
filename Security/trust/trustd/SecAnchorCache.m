/*
 * Copyright (c) 2024 Apple Inc. All Rights Reserved.
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
 *
 */

/*
 *  SecAnchorCache.m
 */

#include "trust/trustd/SecAnchorCache.h"
#include "trust/trustd/SecTrustLoggingServer.h"
#include "trust/trustd/OTATrustUtilities.h"
#include "trust/trustd/trustdFileLocations.h"
#include "trust/trustd/trustdVariants.h"
#include "trust/trustd/trustd_objc_helpers.h"
#include "featureflags/featureflags.h"
#include <utilities/debugging.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecFramework.h>
#include <Security/SecInternal.h>
#include <Security/SecPolicyPriv.h>
#include <AssertMacros.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <dispatch/dispatch.h>
#include <asl.h>
#include <CoreFoundation/CFUtilities.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFRelease.h>
#include <utilities/SecCFError.h>
#include <utilities/SecAppleAnchorPriv.h>

CFStringRef kSecAnchorTypeUndefined = CFSTR("none");
CFStringRef kSecAnchorTypeSystem = CFSTR("system");
CFStringRef kSecAnchorTypePlatform = CFSTR("platform");
CFStringRef kSecAnchorTypeCustom = CFSTR("custom");

// MARK: -
// MARK: SecAnchorCache
/*
 ========================================================================
   SecAnchorCache
 ========================================================================
*/
#define kSecAnchorCacheSize 30

@interface SecAnchorCache()
@property (strong) NSDictionary* anchor_table;
@property (strong) NSMutableDictionary* cache;
@property (strong) NSMutableArray* cache_list;
@property (assign) os_unfair_lock cache_lock;
@end

@implementation SecAnchorCache

- (instancetype)init {
    self = [super init];
    if (self) {
        self.anchor_table = CFBridgingRelease(SecOTAPKICopyConstrainedAnchorLookupTable());
        self.cache = [NSMutableDictionary dictionary];
        self.cache_list = [NSMutableArray array];
        self.cache_lock = OS_UNFAIR_LOCK_INIT;
    }
    return self;
}

- (void)preheatCache {
    SecOTAPKIRef otapkiRef = SecOTAPKICopyCurrentOTAPKIRef();

    //%%% read persistent MRU cache entries from disk here (rdar://139730485)

    CFReleaseSafe(otapkiRef);
}

// return SecCertificateRef for anchor
- (SecCertificateRef _Nullable)copyAnchorAssetForKey:(NSString*)anchorHash {
    SecOTAPKIRef otapkiRef = SecOTAPKICopyCurrentOTAPKIRef();
    if (!otapkiRef) { return NULL; }
    CFDataRef data = SecOTAPKICopyConstrainedAnchorData(otapkiRef, (__bridge CFStringRef)anchorHash);
    //%%% report TrustAnalytics error here since it is never expected to fail
    SecCertificateRef cert = SecCertificateCreateWithData(NULL, data);
    CFReleaseSafe(data);
    CFReleaseSafe(otapkiRef);
    return cert;
}

// return array of SecCertificateRef, from cache if possible
- (NSArray*)anchorsForKey:(NSString*)anchorLookupKey {
    NSMutableArray* result = [NSMutableArray array];
    NSArray *records = [_anchor_table objectForKey:anchorLookupKey];
    if (!isArray((__bridge CFArrayRef)records)) {
        secerror("Malformed anchor records, not an array");
        return result;
    }
    NSUInteger idx, count = [records count];

    os_unfair_lock_lock(&_cache_lock); // grab the cache lock before using the cache

    // iterate over records for normalized issuer hash lookup
    // (normally there is only 1 cert per record, but can be more)
    for (idx = 0; idx < count; idx++) {
        NSDictionary* record = [records objectAtIndex:idx];
        if (!isDictionary((__bridge CFDictionaryRef)record)) {
            secerror("Malformed anchor record, not a dictionary: %{public}@", record);
            continue;
        }
        NSString* certHash = [record objectForKey:@"sha2"];
        if (!isString((__bridge CFStringRef)certHash)) {
            secerror("Malformed anchor record, cert hash not a string: %{public}@", certHash);
            continue;
        }
        NSUInteger index = [_cache_list indexOfObjectPassingTest:^BOOL(NSString* obj, NSUInteger ix, BOOL * stop) {
            if ([obj isEqualToString:certHash]) {
                *stop = YES;
                return YES;
            }
            return NO;
        }];
        if (index != NSNotFound) {
            // Cache hit. Move the entry to the bottom of the list.
            [_cache_list removeObjectAtIndex:index];
            [_cache_list addObject:certHash];
            SecCertificateRef cert = (__bridge SecCertificateRef)[_cache objectForKey:certHash];
            [result addObject:(__bridge id)cert];
            secdebug("Anchors", "anchor cache hit: %@", certHash);
        } else {
            // Cache miss. Add the entry to the end and check cache size.
            secdebug("Anchors", "anchor cache miss: %@", certHash);
            SecCertificateRef cert = [self copyAnchorAssetForKey:certHash];
            if (!cert) {
                secerror("Malformed anchor record, no cert for hash: %{public}@", certHash);
                continue;
            }
            [_cache setObject:CFBridgingRelease(cert) forKey:certHash];
            if (kSecAnchorCacheSize <= [_cache_list count]) {
                // Remove least recently used cache entry.
                secdebug("Anchors", "cache remove stale: %@", [_cache_list objectAtIndex:0]);
                [_cache removeObjectForKey:[_cache_list objectAtIndex:0]];
                [_cache_list removeObjectAtIndex:0];
            }
            [_cache_list addObject:certHash];
            [result addObject:(__bridge id)cert];
            secdebug("Anchors", "cache add: %@", certHash);
        }
    }
    os_unfair_lock_unlock(&_cache_lock);
    return result;
}

- (NSArray*)anchorsForPolicyId:(NSString *)policyId
{
    NSMutableArray *anchors = [NSMutableArray array];
    bool appleAnchors = SecPolicyUsesAppleAnchors((__bridge CFStringRef)policyId);

    if (appleAnchors) {
        // Add the unconstrained Apple Anchors (since they aren't in the anchor table)
        [anchors addObjectsFromArray:(__bridge NSArray*)SecGetAppleTrustAnchors(false)];
    }

    for (NSString* anchorLookupKey in _anchor_table) {
        NSArray *anchorRecords = _anchor_table[anchorLookupKey];
        NSArray *permittedAnchorRecords = [SecAnchorCache anchorRecordsPermitttedForPolicy:anchorRecords
                                                                                  policyId:policyId];
        for (NSDictionary* anchorRecord in permittedAnchorRecords) {
            NSString *certHash = anchorRecord[@"sha2"];
            if (certHash) {
                if (!isNSString(certHash)) { continue; }
                /* lookup certs in cache but don't change the cache */
                os_unfair_lock_lock(&_cache_lock); // grab the cache lock before using the cache
                if ([_cache_list containsObject:certHash]) {
                    SecCertificateRef cert = (__bridge SecCertificateRef)[_cache objectForKey:certHash];
                    if (cert) {
                        [anchors addObject:(__bridge id)cert];
                    }
                } else {
                    SecCertificateRef cert = [self copyAnchorAssetForKey:certHash];
                    if (cert) {
                        [anchors addObject:(__bridge id)cert];
                    }
                    CFReleaseNull(cert);
                }
                os_unfair_lock_unlock(&_cache_lock);
            }
        }
    }
    if (anchors.count == 0) {
        return NULL;
    }
    return anchors;
}

+ (NSArray<NSDictionary*>*) anchorRecordsPermitttedForPolicy:(NSArray<NSDictionary*>*)anchorRecords
policyId:(NSString*)policyId {
    bool systemAnchorsAllowed = !SecPolicyUsesConstrainedAnchors((__bridge CFStringRef)policyId);
    bool appleAnchors = SecPolicyUsesAppleAnchors((__bridge CFStringRef)policyId);

    if (!isNSArray(anchorRecords)) {
        secerror("Malformed anchor records, not an array");
        return NULL;
    }

    NSMutableArray *matchingAnchorRecords = [NSMutableArray array];
    for (NSDictionary *anchorRecord in anchorRecords) {
        if (!isNSDictionary(anchorRecord)) {
            secerror("Malformed anchor record, not a dictionary: %{public}@", anchorRecord);
            continue;
        }
        NSArray *policyOids = anchorRecord[@"oids"];
        if (!isNSArray(policyOids)) {
            secerror("Malformed anchor record, oids not an array: %{public}@", policyOids);
            continue;
        }
        NSString *type = anchorRecord[@"type"];
        if (!isNSString(type)) {
            secerror("Malformed anchor record, type not a string: %{public}@", type);
            continue;
        }

        /* Match policy and policy anchor type to anchor record type */
        if ([type isEqual:(__bridge NSString*)kSecAnchorTypeSystem] && systemAnchorsAllowed) {
            if (policyOids.count < 1 || [policyOids containsObject:policyId]) {
                // System anchor is unconstrained or constrained to this policyId
                [matchingAnchorRecords addObject:anchorRecord];
            }
        } else if ([type isEqual:(__bridge NSString*)kSecAnchorTypeCustom] && !systemAnchorsAllowed && !appleAnchors) {
            if ([policyOids containsObject:policyId]) {
                // custom anchor is constrained to this policyId
                [matchingAnchorRecords addObject:anchorRecord];
            }
        } else if ([type isEqual:(__bridge NSString*)kSecAnchorTypePlatform] && appleAnchors) {
            if (policyOids.count < 1 || [policyOids containsObject:policyId]) {
                // apple anchor is unconstrained or constrained to this policyId
                [matchingAnchorRecords addObject:anchorRecord];
            }
        } else {
            secinfo("anchorCache", "unknown anchor type: %{public}@", type);
        }
    }

    if (matchingAnchorRecords.count > 0) {
        return matchingAnchorRecords;
    } else {
        return NULL;
    }
}

@end

/* C interfaces */
static SecAnchorCache *sAnchorCache = nil;
void SecAnchorCacheInitialize(void) {
    /* Create the anchor cache object once per launch */
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        if (!TrustdVariantHasCertificatesBundle()) {
            return;
        }
        @autoreleasepool {
            sAnchorCache = [[SecAnchorCache alloc] init];
            if ([sAnchorCache.anchor_table count] < 1) {
                CFErrorRef error = NULL;
                SecError(errSecInternal, &error, CFSTR("SecAnchorCache failed to copy anchor table"));
                [[TrustAnalytics logger] logHardError:(__bridge NSError *)error
                                               withEventName:TrustdHealthAnalyticsEventDatabaseEvent
                                              withAttributes:@{TrustdHealthAnalyticsAttributeAffectedDatabase : @(TATrustStore),
                                                               TrustdHealthAnalyticsAttributeDatabaseOperation : @(TAOperationRead)}];
                CFReleaseSafe(error);
            }
            [sAnchorCache preheatCache];
        }
    });
}

CFArrayRef SecAnchorCacheCopyParentCertificates(CFStringRef anchorLookupKey) {
    @autoreleasepool {
        NSArray* parents = [sAnchorCache anchorsForKey:(__bridge NSString*)anchorLookupKey];
        return CFBridgingRetain(parents);
    }
}

CFArrayRef SecAnchorCacheCopyAnchors(CFStringRef policyId) {
    @autoreleasepool {
        NSArray *anchors = [sAnchorCache anchorsForPolicyId:(__bridge NSString*)policyId];
        return CFBridgingRetain(anchors);
    }
}

CFArrayRef SecAnchorPolicyPermittedAnchorRecords(CFArrayRef cfAnchorRecords, CFStringRef policyId) {
    @autoreleasepool {
        return CFBridgingRetain([SecAnchorCache anchorRecordsPermitttedForPolicy:(__bridge NSArray*)cfAnchorRecords
                                                                        policyId:(__bridge NSString*)policyId]);
    }
}

