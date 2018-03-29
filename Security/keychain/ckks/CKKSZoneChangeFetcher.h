/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#if OCTAGON
#import <CloudKit/CloudKit.h>
#import <Foundation/Foundation.h>
#import "keychain/ckks/CKKSResultOperation.h"

NS_ASSUME_NONNULL_BEGIN

/* Fetch Reasons */
@protocol SecCKKSFetchBecause <NSObject>
@end
typedef NSString<SecCKKSFetchBecause> CKKSFetchBecause;
extern CKKSFetchBecause* const CKKSFetchBecauseAPNS;
extern CKKSFetchBecause* const CKKSFetchBecauseAPIFetchRequest;
extern CKKSFetchBecause* const CKKSFetchBecauseCurrentItemFetchRequest;
extern CKKSFetchBecause* const CKKSFetchBecauseInitialStart;
extern CKKSFetchBecause* const CKKSFetchBecauseSecuritydRestart;
extern CKKSFetchBecause* const CKKSFetchBecausePreviousFetchFailed;
extern CKKSFetchBecause* const CKKSFetchBecauseKeyHierarchy;
extern CKKSFetchBecause* const CKKSFetchBecauseTesting;
extern CKKSFetchBecause* const CKKSFetchBecauseResync;

@protocol CKKSChangeFetcherErrorOracle <NSObject>
- (bool)isFatalCKFetchError:(NSError*)error;
@end

/*
 * This class implements a CloudKit-fetch-with-retry.
 * In the case of network or other failures, it'll issue retries.
 * Only in the case of a clean fetch will its operation dependency resolve.
 */

@class CKKSKeychainView;

@interface CKKSZoneChangeFetcher : NSObject

@property (nullable, weak) CKKSKeychainView* ckks;
@property (readonly) NSError* lastCKFetchError;
@property CKRecordZoneID* zoneID;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCKKSKeychainView:(CKKSKeychainView*)ckks;

- (CKKSResultOperation*)requestSuccessfulFetch:(CKKSFetchBecause*)why;
- (CKKSResultOperation*)requestSuccessfulResyncFetch:(CKKSFetchBecause*)why;

// We don't particularly care what this does, as long as it finishes
- (void)holdFetchesUntil:(CKKSResultOperation* _Nullable)holdOperation;

- (void)cancel;
@end

NS_ASSUME_NONNULL_END
#endif
