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
#include "trust/trustd/personalization.h"
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
#include <utilities/SecInternalReleasePriv.h>

CFStringRef kSecAnchorTypeUndefined = CFSTR("none");
CFStringRef kSecAnchorTypeSystem = CFSTR("system");
CFStringRef kSecAnchorTypePlatform = CFSTR("platform");
CFStringRef kSecAnchorTypeCustom = CFSTR("custom");

CFStringRef kSecAnchorTypePlatformTEST = CFSTR("test-platform");
CFStringRef kSecAnchorTypeSystemTEST = CFSTR("test-system");
CFStringRef kSecAnchorTypeCustomTEST = CFSTR("test-custom");

// MARK: -
// MARK: SecAnchorCache
/*
 ========================================================================
   SecAnchorCache
 ========================================================================
*/
#define kSecAnchorCacheSize 20

@interface SecAnchorCache()
// General Anchor Cache
@property (strong) NSDictionary <NSString *,NSArray *>* anchor_table;
@property (strong) NSMutableDictionary* cache;
@property (strong) NSMutableArray* cache_list;
@property (assign) os_unfair_lock cache_lock;

// Apple Anchors
@property (strong) NSMutableSet <NSString*>* apple_anchor_lookups;
@end

@implementation SecAnchorCache

+ (BOOL) allowTestAnchors {
    return false;
}

- (instancetype)init {
    self = [super init];
    if (self) {
        self.anchor_table = CFBridgingRelease(SecOTAPKICopyConstrainedAnchorLookupTable());
        self.cache = [NSMutableDictionary dictionary];
        self.cache_list = [NSMutableArray array];
        self.cache_lock = OS_UNFAIR_LOCK_INIT;

        self.apple_anchor_lookups = [NSMutableSet set];
        [self loadAppleAnchors];
        [self preheatCache];
    }
    return self;
}

- (NSDictionary <NSString *,NSArray *> *)loadBuiltInAppleAnchors {
    NSMutableDictionary <NSString *,NSMutableArray *> *anchor_table = [NSMutableDictionary dictionary];
    NSDictionary *anchors = CFBridgingRelease(SecCopyBuiltInAppleTrustAnchors([self.class allowTestAnchors]));
    for (id anchor in anchors) {
        SecCertificateRef cert = (__bridge SecCertificateRef)anchor;
        BOOL prod = [[anchors objectForKey:anchor] boolValue];

        NSString *anchorLookupKey = CFBridgingRelease(SecCertificateCopyAnchorLookupKey(cert));
        NSMutableArray *anchorRecords = anchor_table[anchorLookupKey];
        if (!anchorRecords) {
            anchorRecords = [NSMutableArray array];
            anchor_table[anchorLookupKey] = anchorRecords;
        }

        /* Create records for this cert */
        NSMutableDictionary *record = [NSMutableDictionary dictionary];
        record[@"oids"] = @[]; // built-in anchors don't have constraints
        CFDataRef hash = SecCertificateCopySHA256Digest(cert);
        record[@"sha2"] = CFBridgingRelease(CFDataCopyHexString(hash));
        CFReleaseNull(hash);
        CFDataRef spki_hash = SecCertificateCopySubjectPublicKeyInfoSHA256Digest(cert);
        record[@"spki-sha2"] = CFBridgingRelease(CFDataCopyHexString(spki_hash));
        CFReleaseNull(spki_hash);

        /* Add a platform and a system record */
        record[@"type"] = prod ? (__bridge NSString*)kSecAnchorTypePlatform :
                            (__bridge NSString*)kSecAnchorTypePlatformTEST;
        [anchorRecords addObject:[NSDictionary dictionaryWithDictionary:record]];

        record[@"type"] = prod ? (__bridge NSString*)kSecAnchorTypeSystem :
                            (__bridge NSString*)kSecAnchorTypeSystemTEST;
        [anchorRecords addObject:[NSDictionary dictionaryWithDictionary:record]];
    }
    return anchor_table;
}


- (void)loadAppleAnchors {
    /* Add the hardcoded ones in case something's wrong with the anchor table */
    NSDictionary <NSString *,NSArray *> *built_in_apple_anchors = [self loadBuiltInAppleAnchors];
    NSMutableDictionary <NSString *,NSArray *>*anchor_table = [self.anchor_table mutableCopy];
    if (!anchor_table) {
        // To support trustd variants that don't have the anchor table, they still need to load the built-in apple anchors
        anchor_table = [NSMutableDictionary dictionary];
    }
    [anchor_table addEntriesFromDictionary:built_in_apple_anchors]; // built-in anchors overrides OTA anchor table
    self.anchor_table = [NSDictionary dictionaryWithDictionary:anchor_table];

    for (NSString* anchorLookupKey in self.anchor_table) {
        NSArray *anchorRecords = [self anchorRecordsForLookupKey:anchorLookupKey];
        for (NSDictionary* anchorRecord in anchorRecords) {
            if (!isNSDictionary(anchorRecord)) {
                secerror("Malformed anchor record for %{public}@, not a dictionary: %{public}@", anchorLookupKey, anchorRecord);
                continue;
            }
            NSString *type = anchorRecord[@"type"];
            if (!isNSString(type)) {
                secerror("Malformed anchor record for %{public}@, type not a string: %{public}@", anchorLookupKey, type);
                continue;
            }
            if ([type isEqual:(__bridge NSString*)kSecAnchorTypePlatform] ||
                ([self.class allowTestAnchors] && [type isEqual:(__bridge NSString*)kSecAnchorTypePlatformTEST])) {
                [self.apple_anchor_lookups addObject:anchorLookupKey];
            }
        }
    }
}

- (void)preheatCache {
    SecOTAPKIRef otapkiRef = SecOTAPKICopyCurrentOTAPKIRef();
    /* Pre-heat cache with built-in apple anchors (temporary until rdar://139730485) */
    NSArray *appleAnchors = (__bridge NSArray*)SecGetAppleTrustAnchors([self.class allowTestAnchors]);
    for (id appleAnchor in appleAnchors) {
        SecCertificateRef cert = (__bridge SecCertificateRef) appleAnchor;
        CFDataRef hash  = SecCertificateCopySHA256Digest(cert);
        if (hash) {
            NSString *hashStr = CFBridgingRelease(CFDataCopyHexString(hash));
            [self.cache_list addObject:hashStr];
            self.cache[hashStr] = appleAnchor;
            CFReleaseNull(hash);
        }
    }

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

- (NSArray*)anchorRecordsForLookupKey:(NSString*)anchorLookupKey {
    NSArray *records = [_anchor_table objectForKey:anchorLookupKey];
    if (records && isNSArray(records)) {
        return records;
    }
    return nil;
}

- (NSArray*)anchorRecordsForLookupKey:(NSString*)anchorLookupKey
                                 hash:(NSData*)hash
                              hashKey:(NSString*)hashKey
{
    NSArray *records = [self anchorRecordsForLookupKey:anchorLookupKey];
    NSString *hashValue = CFBridgingRelease(CFDataCopyHexString((__bridge CFDataRef)hash));

    NSMutableArray *matchingRecords = [NSMutableArray array];
    for (NSDictionary *record in records) {
        if (!isNSDictionary(record)) {
            secerror("Malformed anchor record, not a dictionary: %{public}@", record);
            continue;
        }
        NSString *recordHash = record[hashKey];
        if (!isNSString(recordHash)) {
            secerror("Malformed anchor record, cert hash not a string: %{public}@", recordHash);
            continue;
        }
        if ([recordHash isEqual:hashValue]) {
            [matchingRecords addObject:record];
        }
    }

    if (matchingRecords.count > 0) {
        return matchingRecords;
    }
    return nil;
}

- (SecCertificateRef)copyCertForAppleCertHash:(NSString*)certHash CF_RETURNS_RETAINED {
    NSArray *built_in_anchors = (__bridge NSArray*)SecGetAppleTrustAnchors([self.class allowTestAnchors]);
    for (id anchor in built_in_anchors) {
        SecCertificateRef cert = (__bridge SecCertificateRef)anchor;
        CFDataRef hash = SecCertificateCopySHA256Digest(cert);
        NSString *built_in_hash = CFBridgingRelease(CFDataCopyHexString(hash));
        CFReleaseNull(hash);
        if ([certHash isEqualToString:built_in_hash]) {
            return CFRetainSafe(cert);
        }
    }
    return NULL;
}

// return array of SecCertificateRef, from cache if possible
- (NSArray*)anchorsForKey:(NSString*)anchorLookupKey {
    NSMutableSet* result = [NSMutableSet set];
    NSArray *records = [self anchorRecordsForLookupKey:anchorLookupKey];
    if (!records) { // Not in anchor table
        return @[];
    }

    os_unfair_lock_lock(&_cache_lock); // grab the cache lock before using the cache

    // iterate over records for normalized issuer hash lookup
    // (normally there is only 1 cert per record, but can be more)
    for (NSDictionary *record in records) {
        if (!isNSDictionary(record)) {
            secerror("Malformed anchor record, not a dictionary: %{public}@", record);
            continue;
        }
        NSString* certHash = [record objectForKey:@"sha2"];
        if (!isNSString(certHash)) {
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
            if (!cert && [self.apple_anchor_lookups containsObject:anchorLookupKey]) {
                // Look among the built-in Apple anchors
                cert = [self copyCertForAppleCertHash:certHash];
            }
            if (!cert) {
                secerror("Malformed anchor record, no cert for hash: %{public}@", certHash);
                continue;
            }
            [_cache setObject:(__bridge id)cert forKey:certHash];
            if (kSecAnchorCacheSize <= [_cache_list count]) {
                // Remove least recently used cache entry.
                secdebug("Anchors", "cache remove stale: %@", [_cache_list objectAtIndex:0]);
                [_cache removeObjectForKey:[_cache_list objectAtIndex:0]];
                [_cache_list removeObjectAtIndex:0];
            }
            [_cache_list addObject:certHash];
            [result addObject:(__bridge id)cert];
            CFReleaseNull(cert);
            secdebug("Anchors", "cache add: %@", certHash);
        }
    }
    os_unfair_lock_unlock(&_cache_lock);
    return result.allObjects;
}

- (NSArray*)anchorsForPolicyId:(NSString *)policyId
{
    NSMutableSet *anchors = [NSMutableSet set];
    bool appleAnchors = SecPolicyUsesAppleAnchors((__bridge CFStringRef)policyId);

    if (appleAnchors) {
        // Add the hardcoded (unconstrained) Apple Anchors (in case we don't have them in the anchor table)
        [anchors addObjectsFromArray:(__bridge NSArray*)SecGetAppleTrustAnchors([self.class allowTestAnchors])];
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
                    if (!cert && [self.apple_anchor_lookups containsObject:anchorLookupKey]) {
                        // Look among the built-in Apple anchors
                        cert = [self copyCertForAppleCertHash:certHash];
                    }
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
    return [anchors allObjects];
}

+ (NSArray<NSDictionary*>*) anchorRecordsPermitttedForPolicy:(NSArray<NSDictionary*>*)anchorRecords
                                                    policyId:(NSString*)policyId
{
    bool systemAnchorsAllowed = !SecPolicyUsesConstrainedAnchors((__bridge CFStringRef)policyId);
    bool appleAnchors = SecPolicyUsesAppleAnchors((__bridge CFStringRef)policyId);
    bool customAnchors = !systemAnchorsAllowed && !appleAnchors;
    bool allowTestAnchors = [SecAnchorCache allowTestAnchors];

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

        /* Skip over any unknown anchor types */
        if (![type isEqual:(__bridge NSString*)kSecAnchorTypeSystem] &&
            ![type isEqual:(__bridge NSString*)kSecAnchorTypeCustom] &&
            ![type isEqual:(__bridge NSString*)kSecAnchorTypePlatform] &&
            ![type isEqual:(__bridge NSString*)kSecAnchorTypeSystemTEST] &&
            ![type isEqual:(__bridge NSString*)kSecAnchorTypeCustomTEST] &&
            ![type isEqual:(__bridge NSString*)kSecAnchorTypePlatformTEST]) {
            secinfo("anchorCache", "unknown anchor type: %{public}@", type);
            continue;
        }

        /* Match policy and policy anchor type to anchor record type */
        if (systemAnchorsAllowed && ([type isEqual:(__bridge NSString*)kSecAnchorTypeSystem] ||
                                     (allowTestAnchors && [type isEqual:(__bridge NSString*)kSecAnchorTypeSystemTEST]))) {
            if (policyOids.count < 1 || [policyOids containsObject:policyId]) {
                // System anchor is unconstrained or constrained to this policyId
                [matchingAnchorRecords addObject:anchorRecord];
            }
        } else if (customAnchors && ([type isEqual:(__bridge NSString*)kSecAnchorTypeCustom] ||
                                     (allowTestAnchors && [type isEqual:(__bridge NSString*)kSecAnchorTypeCustomTEST]))) {
            if ([policyOids containsObject:policyId]) {
                // custom anchor is constrained to this policyId
                [matchingAnchorRecords addObject:anchorRecord];
            }
        } else if (appleAnchors && ([type isEqual:(__bridge NSString*)kSecAnchorTypePlatform] ||
                                    (allowTestAnchors && [type isEqual:(__bridge NSString*)kSecAnchorTypePlatformTEST]))) {
            if (policyOids.count < 1 || [policyOids containsObject:policyId]) {
                // apple anchor is unconstrained or constrained to this policyId
                [matchingAnchorRecords addObject:anchorRecord];
            }
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
static os_unfair_lock sAnchorCacheLock = OS_UNFAIR_LOCK_INIT;
void SecAnchorCacheInitialize(void) {
    @autoreleasepool {
        os_unfair_lock_lock(&sAnchorCacheLock);
        sAnchorCache = [[SecAnchorCache alloc] init];
        /* check that we loaded the anchor table for variants that use the certificates bundle */
        if (TrustdVariantHasCertificatesBundle() && [sAnchorCache.anchor_table count] < 1) {
            CFErrorRef error = NULL;
            SecError(errSecInternal, &error, CFSTR("SecAnchorCache failed to copy anchor table"));
            [[TrustAnalytics logger] logHardError:(__bridge NSError *)error
                                    withEventName:TrustdHealthAnalyticsEventDatabaseEvent
                                   withAttributes:@{TrustdHealthAnalyticsAttributeAffectedDatabase : @(TATrustStore),
                                                    TrustdHealthAnalyticsAttributeDatabaseOperation : @(TAOperationRead)}];
            CFReleaseSafe(error);
        }
        os_unfair_lock_unlock(&sAnchorCacheLock);
    }
}

static SecAnchorCache *getAnchorCache(void) {
    os_unfair_lock_lock(&sAnchorCacheLock);
    SecAnchorCache *currentAnchorCache = sAnchorCache;
    os_unfair_lock_unlock(&sAnchorCacheLock);
    return currentAnchorCache;
}

CFArrayRef SecAnchorCacheCopyAnchorRecordsForCertificate(SecCertificateRef cert) {
    @autoreleasepool {
        NSData *certificateHash = CFBridgingRelease(SecCertificateCopySHA256Digest(cert));
        NSString *anchorLookupKey = CFBridgingRelease(SecCertificateCopyAnchorLookupKey(cert));
        if (!certificateHash || !anchorLookupKey) {
            return NULL;
        }
        return CFBridgingRetain([getAnchorCache() anchorRecordsForLookupKey:anchorLookupKey
                                                                       hash:certificateHash
                                                                    hashKey:@"sha2"]);
    }

}

CFArrayRef SecAnchorCacheCopyAnchorRecordsForSPKI(SecCertificateRef cert) {
    @autoreleasepool {
        NSData *spkiHash = CFBridgingRelease(SecCertificateCopySubjectPublicKeyInfoSHA256Digest(cert));
        NSString *anchorLookupKey = CFBridgingRelease(SecCertificateCopyAnchorLookupKey(cert));
        if (!spkiHash || !anchorLookupKey) {
            return NULL;
        }
        return CFBridgingRetain([getAnchorCache() anchorRecordsForLookupKey:anchorLookupKey
                                                                   hash:spkiHash
                                                                hashKey:@"spki-sha2"]);
    }
}

CFArrayRef SecAnchorCacheCopyParentCertificates(CFStringRef anchorLookupKey) {
    @autoreleasepool {
        NSArray* parents = [getAnchorCache() anchorsForKey:(__bridge NSString*)anchorLookupKey];
        return CFBridgingRetain(parents);
    }
}

CFArrayRef SecAnchorCacheCopyAnchors(CFStringRef policyId) {
    @autoreleasepool {
        NSArray *anchors = [getAnchorCache() anchorsForPolicyId:(__bridge NSString*)policyId];
        return CFBridgingRetain(anchors);
    }
}

CFArrayRef SecAnchorPolicyPermittedAnchorRecords(CFArrayRef cfAnchorRecords, CFStringRef policyId) {
    @autoreleasepool {
        return CFBridgingRetain([SecAnchorCache anchorRecordsPermitttedForPolicy:(__bridge NSArray*)cfAnchorRecords
                                                                        policyId:(__bridge NSString*)policyId]);
    }
}

bool SecAnchorCacheIsBuiltInAppleAnchor(SecCertificateRef certificate) {
    SecAppleTrustAnchorFlags flags = 0;
    return SecIsBuiltInAppleAnchor(certificate, flags);
}

bool SecAnchorCacheIsAppleAnchor(SecCertificateRef certificate) {
    @autoreleasepool {
        NSString *anchorLookupKey = CFBridgingRelease(SecCertificateCopyAnchorLookupKey(certificate));
        SecAnchorCache *anchorCache = getAnchorCache();
        if ([anchorCache.apple_anchor_lookups containsObject:anchorLookupKey]) {
            /* Since the anchor lookup isn't cryptographically secure,
             * we need to also check that the cert matches the anchor for that lookup. */
            if (SecAnchorCacheIsBuiltInAppleAnchor(certificate)) {
                return true;
            }
            NSArray* anchors = [anchorCache anchorsForKey:anchorLookupKey];
            return [anchors containsObject:(__bridge id)certificate];
        }
        return false;
    }
}

CFDictionaryRef SecAnchorCacheCopyAppleAnchors(CFStringRef policyId) {
    @autoreleasepool {
        NSMutableDictionary *result = [NSMutableDictionary dictionary];
        SecAnchorCache *anchorCache = getAnchorCache();
        for (NSString *lookup in anchorCache.apple_anchor_lookups.allObjects) {
            NSArray *anchorRecords = [anchorCache anchorRecordsForLookupKey:lookup];
            /* If no policyId is specified, allow trusted for _any_ policy;
             * ow, filter for policy specified. */
            if (policyId) {
                anchorRecords = [SecAnchorCache anchorRecordsPermitttedForPolicy:anchorRecords
                                                                        policyId:(__bridge NSString*)policyId];
            }

            if (anchorRecords.count == 0) {
                // Anchor not permitted for this policy
                continue;
            }

            /* In theory, if allowTestAnchors is false, we should not have gotten any records for
             * test anchor types, but let's double check the records. If allowTestAnchors is true
             * we need to determine prod vs. test. */
            BOOL prod = YES;
            for (NSDictionary *record in anchorRecords) {
                NSString *type = record[@"type"];

                if ([type isEqual:(__bridge NSString*)kSecAnchorTypeSystemTEST] ||
                    [type isEqual:(__bridge NSString*)kSecAnchorTypeCustomTEST] ||
                    [type isEqual:(__bridge NSString*)kSecAnchorTypePlatformTEST]) {
                    prod = NO;
                    if (![SecAnchorCache allowTestAnchors]) {
                        secerror("Found test anchor type for %@; not allowed on prod system", lookup);
                    }
                }
            }

            /* Get the certs */
            NSArray *anchors = [anchorCache anchorsForKey:lookup];
            for (id anchor in anchors) {
                SecCertificateRef cert = (__bridge SecCertificateRef)anchor;
                NSData *certData = CFBridgingRelease(SecCertificateCopyData(cert));
                [result setObject:@(prod) forKey:certData];
            }
        }

        return CFDictionaryCreateCopy(NULL, (__bridge CFDictionaryRef)result);
    }
}
