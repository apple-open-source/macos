/*
 * Copyright (c) 2019 Apple Inc. All Rights Reserved.
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

#ifndef _TRUSTTEST_TRUST_HELPERS_H_
#define _TRUSTTEST_TRUST_HELPERS_H_

#import <Foundation/Foundation.h>
#import <Security/Security.h>

NS_ASSUME_NONNULL_BEGIN

NSURL * _Nullable setUpTmpDir(void);
int ping_host(char *host_name);

@interface TestTrustEvaluation : NSObject
@property (assign, nonnull) SecTrustRef trust;
@property NSString *fullTestName;
@property BOOL bridgeOSDisabled;

// Outputs
@property (assign) SecTrustResultType trustResult;
@property (nullable) NSDictionary *resultDictionary;

// Expected results
@property NSNumber *expectedResult;
@property NSNumber *expectedChainLength;

// These properties have the side effect of modifying the SecTrustRef
@property (nullable,assign,nonatomic) NSArray *anchors;
@property (nullable,assign,nonatomic) NSArray *ocspResponses;
@property (nullable,nonatomic) NSArray *presentedSCTs;
@property (nullable,nonatomic) NSArray *trustedCTLogs;
@property (nullable,nonatomic) NSDate *verifyDate;

- (instancetype _Nullable )initWithCertificates:(NSArray * _Nonnull)certs policies:(NSArray * _Nullable)policies;
- (instancetype _Nullable) initWithTrustDictionary:(NSDictionary *)testDict;

- (void)addAnchor:(SecCertificateRef)certificate;
- (void)setNeedsEvaluation;

- (bool)evaluate:(out NSError * _Nullable __autoreleasing * _Nullable)outError;
- (bool)evaluateForExpectedResults:(out NSError * _Nullable __autoreleasing *)outError;
@end

NS_ASSUME_NONNULL_END

#endif /*_TRUSTTEST_TRUST_HELPERS_H_ */
