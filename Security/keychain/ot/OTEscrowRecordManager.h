/*
 * Copyright (c) 2026 Apple Inc. All Rights Reserved.
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

#import <OctagonTrust/OTEscrowCheckCallResult.h>

NS_ASSUME_NONNULL_BEGIN

@interface OTEscrowRecordManager : NSObject

+ (OTEscrowCheckRateLimitState)effectiveRateLimitState:(OTCuttlefishAccountStateHolder*)deps
                                    passcodeGeneration:(NSNumber*)passcodeGeneration
                                      intendedBottleID:(NSString*)intendedBottleID
                                   computedDaysLeftOnRateLimit:(NSInteger* _Nullable)daysLeftOnRateLimit
                                                 error:(NSError**)error;

+ (BOOL)generateCachedEscrowRecordForReason:(AppleKeyStorePasscodeCacheReason)reason
                         passcodeGeneration:(NSNumber*)passcodeGeneration
                               dependencies:(OTOperationDependencies*)deps
                       cuttlefishXPCWrapper:(CuttlefishXPCWrapper*)cuttlefishXPCWrapper
                                      error:(NSError**)error;

+ (NSNumber* __nullable)passcodeGenerationWithError:(NSError**)error;

@end

NS_ASSUME_NONNULL_END
