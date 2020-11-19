/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

#ifndef _TRUSTTESTS_EVALUATION_TESTCASE_H_
#define _TRUSTTESTS_EVALUATION_TESTCASE_H_

#import <XCTest/XCTest.h>
#include <Security/Security.h>
#include "../TrustEvaluationTestHelpers.h"

NS_ASSUME_NONNULL_BEGIN

@interface TrustEvaluationTestCase : XCTestCase
- (id _Nullable)addTrustSettingsForCert:(SecCertificateRef)cert trustSettings:(id)trustSettings; // returns a persistent ref for call to removeTrustSettings, takes a dictionary or array of trust settings
- (id _Nullable)addTrustSettingsForCert:(SecCertificateRef)cert; // returns a persistent ref for call to removeTrustSettings
- (void)removeTrustSettingsForCert:(SecCertificateRef)cert persistentRef:(id _Nullable)persistentRef;
- (void)setTestRootAsSystem:(const uint8_t*)sha256hash; // this is expected to be a 32-byte array
- (void)removeTestRootAsSystem;

// ported from regressionBase
- (void)runCertificateTestForDirectory:(SecPolicyRef)policy subDirectory:(NSString *)resourceSubDirectory verifyDate:(NSDate*)date;

- (id _Nullable) CF_RETURNS_RETAINED SecCertificateCreateFromResource:(NSString * )name subdirectory:(NSString *)dir;
- (id _Nullable) CF_RETURNS_RETAINED SecCertificateCreateFromPEMResource:(NSString *)name subdirectory:(NSString *)dir;
@end

/* Use this interface to get a SecCertificateRef that has the same CFTypeID
 * as used by the Security framework */
CF_RETURNS_RETAINED _Nullable
SecCertificateRef SecFrameworkCertificateCreate(const uint8_t * der_bytes, CFIndex der_length);
CF_RETURNS_RETAINED _Nullable
SecCertificateRef SecFrameworkCertificateCreateFromTestCert(SecCertificateRef cert);
CF_RETURNS_RETAINED
SecPolicyRef SecFrameworkPolicyCreateSSL(Boolean server, CFStringRef __nullable hostname);
CF_RETURNS_RETAINED
SecPolicyRef SecFrameworkPolicyCreateBasicX509(void);
CF_RETURNS_RETAINED
SecPolicyRef SecFrameworkPolicyCreateSMIME(CFIndex smimeUsage, CFStringRef __nullable email);

NS_ASSUME_NONNULL_END

#endif /* _TRUSTTESTS_EVALUATION_TESTCASE_H_ */
