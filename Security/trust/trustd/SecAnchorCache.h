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

/*!
 @header SecAnchorCache
 The functions provided in SecAnchorCache.h provide an interface to
 a caching module for SecAnchorRef instances. Lookups which do not hit
 the cache will attempt to read the certificate data from the disk and
 then add a new entry to the cache.
 */

#ifndef _SECURITY_SECANCHORCACHE_H_
#define _SECURITY_SECANCHORCACHE_H_

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>

#include <os/transaction_private.h>
#include <os/variant_private.h>
#include <os/lock.h>

#if __OBJC__
#import <Foundation/Foundation.h>
#endif

__BEGIN_DECLS

CF_ASSUME_NONNULL_BEGIN
CF_IMPLICIT_BRIDGING_ENABLED

extern CFStringRef kSecAnchorTypeUnspecified;
extern CFStringRef kSecAnchorTypeSystem;
extern CFStringRef kSecAnchorTypePlatform;
extern CFStringRef kSecAnchorTypeCustom;

#if __OBJC__

@interface SecAnchorCache : NSObject

- (SecCertificateRef _Nullable)copyAnchorAssetForKey:(NSString * _Nullable)anchorHash;
- (NSArray * _Nonnull)anchorsForKey:(NSString*_Nullable)anchorLookupKey;
+ (NSArray<NSDictionary*>* _Nullable) anchorRecordsPermitttedForPolicy:(NSArray<NSDictionary*>*)anchorRecords
                                                    policyId:(NSString*)policyId;
@end
#endif // __OBJC__

void SecAnchorCacheInitialize(void);

CF_RETURNS_RETAINED
CFArrayRef SecAnchorCacheCopyParentCertificates(CFStringRef anchorLookupKey);

CF_RETURNS_RETAINED
CFArrayRef SecAnchorCacheCopyAnchors(CFStringRef policyId);

CF_RETURNS_RETAINED
_Nullable CFArrayRef SecAnchorPolicyPermittedAnchorRecords(CFArrayRef cfAnchorRecords, CFStringRef policyId);

CF_IMPLICIT_BRIDGING_DISABLED
CF_ASSUME_NONNULL_END

__END_DECLS

#endif /* _SECURITY_SECANCHORCACHE_H_ */
