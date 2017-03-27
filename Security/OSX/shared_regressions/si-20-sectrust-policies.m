/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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


/* INSTRUCTIONS FOR ADDING NEW SUBTESTS:
 *   1. Add the certificates, as DER-encoded files with the 'cer' extension, to OSX/shared_regressions/si-20-sectrust-policies-data/
 *   2. Add a new dictionary to the test plist (OSX/shared_regressions/si-20-sectrust-policies-data/PinningPolicyTrustTest.plist).
 *      This dictionary must include: (see constants below)
 *          MajorTestName
 *          MinorTestName
 *          Policies
 *          Leaf
 *          Intermediates
 *          ExpectedResult
 *      It is strongly recommended that all test dictionaries include the Anchors and VerifyDate keys.
 *      Addtional optional keys are defined below.
 */

/* INSTRUCTIONS FOR DEBUGGING SUBTESTS:
 *   Add a debugging.plist to OSX/shared_regressions/si-20-sectrust-policies-data/ containing only those subtest dictionaries
 *   you want to debug. Git will ignore this file, so you don't accidentally commit it.
 */

#include "shared_regressions.h"

#include <AssertMacros.h>
#import <Foundation/Foundation.h>

#include <utilities/SecInternalReleasePriv.h>
#include <utilities/SecCFRelease.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrust.h>

/* Key Constants for Test Dictionaries */
const NSString *kSecTrustTestMajorTestName  = @"MajorTestName";     /* Required; value: string */
const NSString *kSecTrustTestMinorTestName  = @"MinorTestName";     /* Required; value: string */
const NSString *kSecTrustTestPolicies       = @"Policies";          /* Required; value: dictionary or array of dictionaries */
const NSString *kSecTrustTestLeaf           = @"Leaf";              /* Required; value: string */
const NSString *kSecTrustTestIntermediates  = @"Intermediates";     /* Required; value: string or array of strings */
const NSString *kSecTrustTestAnchors        = @"Anchors";           /* Recommended; value: string or array of strings */
const NSString *kSecTrustTestVerifyDate     = @"VerifyDate";        /* Recommended; value: date */
const NSString *kSecTrustTestExpectedResult = @"ExpectedResult";    /* Required; value: number */
const NSString *kSecTrustTestChainLength    = @"ChainLength";       /* Optional; value: number */
const NSString *kSecTrustTestEnableTestCerts= @"EnableTestCertificates"; /* Optiona; value: string */

/* Key Constants for Policies Dictionaries */
const NSString *kSecTrustTestPolicyOID      = @"PolicyIdentifier";  /* Required; value: string */
const NSString *kSecTrustTestPolicyProperties = @"Properties";      /* Optional; value: dictionary, see Policy Value Constants, SecPolicy.h */

const NSString *kSecTrustTestPinningPolicyResources = @"si-20-sectrust-policies-data";

@interface TestObject : NSObject
@property (readonly) NSMutableArray *certificates;
@property (readonly) NSMutableArray *policies;
@property (readonly) NSMutableArray *anchors;
@property (readonly) NSString *fullTestName;

- (id)initWithMajorTestName:(NSString *)majorTestName minorTestName:(NSString *)minorTestName;
- (bool)addLeafToCertificates:(NSString *)leafName;
- (bool)addCertsToArray:(id)pathsObj outputArray:(NSMutableArray *)outArray;
- (bool)addIntermediatesToCertificates:(id)intermediatesObj;
- (bool)addPolicies:(id)policiesObj;
- (bool)addAnchors:(id)anchorsObj;
@end

@implementation TestObject

- (id)init {
    self = [super init];
    return self;
}

- (id)initWithMajorTestName:(NSString *)majorTestName minorTestName:(NSString *)minorTestName {
    if ((self = [super init])) {
        _fullTestName = [[majorTestName stringByAppendingString:@"-"] stringByAppendingString:minorTestName];
    }
    return self;
}

- (bool)addLeafToCertificates:(NSString *)leafName {
    SecCertificateRef cert;
    NSString *path = nil;
    require_action_quiet(leafName, errOut,
                         fail("%@: failed to get leaf for test", _fullTestName));

    path = [[NSBundle mainBundle]
                      pathForResource:leafName
                      ofType:@"cer"
                      inDirectory:(NSString *)kSecTrustTestPinningPolicyResources];
    require_action_quiet(path, errOut, fail("%@: failed to get path for leaf", _fullTestName));
    cert = SecCertificateCreateWithData(NULL, (CFDataRef)[NSData dataWithContentsOfFile:path]);
    require_action_quiet(cert, errOut,
                         fail("%@: failed to create leaf certificate from path %@",
                              _fullTestName, path));
    _certificates = [[NSMutableArray alloc] initWithObjects:(__bridge id)cert, nil];
    CFReleaseNull(cert);
    require_action_quiet(_certificates, errOut,
                         fail("%@: failed to initialize certificates array",
                              _fullTestName));
    return true;

errOut:
    return false;
}

- (bool)addCertsToArray:(id)pathsObj outputArray:(NSMutableArray *)outArray {
    __block SecCertificateRef cert = NULL;
    __block NSString* path = nil;
    require_action_quiet(pathsObj, errOut,
                         fail("%@: failed to get certificate paths for test", _fullTestName));

    if ([pathsObj isKindOfClass:[NSString class]]) {
        /* Only one cert path */
        path = [[NSBundle mainBundle]
                pathForResource:pathsObj
                ofType:@"cer"
                inDirectory:(NSString *)kSecTrustTestPinningPolicyResources];
        require_action_quiet(path, errOut, fail("%@: failed to get path for cert",
                                                _fullTestName));
        cert = SecCertificateCreateWithData(NULL, (CFDataRef)[NSData dataWithContentsOfFile:path]);
        require_action_quiet(cert, errOut,
                             fail("%@: failed to create certificate from path %@",
                                  _fullTestName, path));
        [outArray addObject:(__bridge id)cert];
        CFReleaseNull(cert);
    }

    else if ([pathsObj isKindOfClass:[NSArray class]]) {
        /* Test has more than one intermediate */
        [(NSArray *)pathsObj enumerateObjectsUsingBlock:^(NSString *resource, NSUInteger idx, BOOL *stop) {
            path = [[NSBundle mainBundle]
                    pathForResource:resource
                    ofType:@"cer"
                    inDirectory:(NSString *)kSecTrustTestPinningPolicyResources];
            require_action_quiet(path, blockOut,
                                 fail("%@: failed to get path for cert %ld",
                                      self->_fullTestName, (unsigned long)idx));
            cert = SecCertificateCreateWithData(NULL, (CFDataRef)[NSData dataWithContentsOfFile:path]);
            require_action_quiet(cert, blockOut,
                                 fail("%@: failed to create certificate %ld from path %@",
                                      self->_fullTestName, (unsigned long) idx, path));
            [outArray addObject:(__bridge id)cert];

            CFReleaseNull(cert);
            return;

        blockOut:
            CFReleaseNull(cert);
            *stop = YES;
        }];
    }

    else {
        fail("%@: unexpected type for intermediates or anchors value", _fullTestName);
        goto errOut;
    }

    return true;

errOut:
    CFReleaseNull(cert);
    return false;

}

- (bool)addIntermediatesToCertificates:(id)intermediatesObj {
    require_action_quiet(intermediatesObj, errOut,
                         fail("%@: failed to get intermediates for test", _fullTestName));

    require_action_quiet([self addCertsToArray:intermediatesObj outputArray:_certificates], errOut,
                         fail("%@: failed to add intermediates to certificates array", _fullTestName));

    if ([intermediatesObj isKindOfClass:[NSString class]]) {
        require_action_quiet([_certificates count] == 2, errOut,
                             fail("%@: failed to add all intermediates", _fullTestName));
    } else if ([intermediatesObj isKindOfClass:[NSArray class]]) {
        require_action_quiet([_certificates count] == [(NSArray *)intermediatesObj count] + 1, errOut,
                             fail("%@: failed to add all intermediates", _fullTestName));
    }

    return true;

errOut:
    return false;
}

- (bool)addPolicies:(id)policiesObj {
    __block SecPolicyRef policy = NULL;
    require_action_quiet(policiesObj, errOut,
                         fail("%@: failed to get policies for test", _fullTestName));

    _policies = [[NSMutableArray alloc] init];
    require_action_quiet(_policies, errOut,
                         fail("%@: failed to initialize policies array", _fullTestName));
    if ([policiesObj isKindOfClass:[NSDictionary class]]) {
        /* Test has only one policy */
        NSString *policyIdentifier = [(NSDictionary *)policiesObj objectForKey:kSecTrustTestPolicyOID];
        NSDictionary *policyProperties = [(NSDictionary *)policiesObj objectForKey:kSecTrustTestPolicyProperties];
        require_action_quiet(policyIdentifier, errOut, fail("%@: failed to get policy OID", _fullTestName));

        policy = SecPolicyCreateWithProperties((__bridge CFStringRef)policyIdentifier,
                                               (__bridge CFDictionaryRef)policyProperties);
        require_action_quiet(policy, errOut,
                             fail("%@: failed to create properties for policy OID %@",
                                  _fullTestName, policyIdentifier));
        [_policies addObject:(__bridge id)policy];
        CFReleaseNull(policy);
    }

    else if ([policiesObj isKindOfClass:[NSArray class]]) {
        /* Test more than one intermediate */
        [(NSArray *)policiesObj enumerateObjectsUsingBlock:^(NSDictionary *policyDict, NSUInteger idx, BOOL *stop) {
            NSString *policyIdentifier = [(NSDictionary *)policyDict objectForKey:kSecTrustTestPolicyOID];
            NSDictionary *policyProperties = [(NSDictionary *)policyDict objectForKey:kSecTrustTestPolicyProperties];
            require_action_quiet(policyIdentifier, blockOut, fail("%@: failed to get policy OID", self->_fullTestName));

            policy = SecPolicyCreateWithProperties((__bridge CFStringRef)policyIdentifier,
                                                   (__bridge CFDictionaryRef)policyProperties);
            require_action_quiet(policy, blockOut,
                                 fail("%@: failed to create properties for policy OID %@",
                                      self->_fullTestName, policyIdentifier));
            [self->_policies addObject:(__bridge id)policy];

            CFReleaseNull(policy);
            return;

        blockOut:
            CFReleaseNull(policy);
            *stop = YES;
        }];

        require_action_quiet([(NSArray *)policiesObj count] == [_policies count], errOut,
                             fail("%@: failed to add all policies", _fullTestName));
    }

    else {
        fail("%@: unexpected type for %@ value", _fullTestName, kSecTrustTestPolicies);
        goto errOut;
    }

    return true;

errOut:
    CFReleaseNull(policy);
    return false;
}

- (bool)addAnchors:(id)anchorsObj {
    require_action_quiet(anchorsObj, errOut,
                         fail("%@: failed to get anchors for test", _fullTestName));

    _anchors = [[NSMutableArray alloc] init];
    require_action_quiet(_anchors, errOut,
                         fail("%@: failed to initialize anchors array", _fullTestName));
    require_action_quiet([self addCertsToArray:anchorsObj outputArray:_anchors], errOut,
                         fail("%@: failed to add anchors to anchors array", _fullTestName));

    if ([anchorsObj isKindOfClass:[NSString class]]) {
        require_action_quiet([_anchors count] == 1, errOut,
                             fail("%@: failed to add all anchors", _fullTestName));
    } else if ([anchorsObj isKindOfClass:[NSArray class]]) {
        require_action_quiet([_anchors count] == [(NSArray *)anchorsObj count], errOut,
                             fail("%@: failed to add all anchors", _fullTestName));
    }

    return true;

errOut:
    return false;
}

@end

void (^runTestForObject)(id, NSUInteger, BOOL *) =
^(NSDictionary *testDict, NSUInteger idx, BOOL *stop) {
    NSString *majorTestName = nil, *minorTestName = nil;
    TestObject *test = nil;
    SecTrustRef trust = NULL;
    SecTrustResultType trustResult = kSecTrustResultInvalid;
    NSDate *verifyDate = nil;
    NSNumber *expectedResult = nil, *chainLen = nil;

    bool enableTestCertificates = (bool)[testDict objectForKey:kSecTrustTestEnableTestCerts];

    /* Test name, for documentation purposes */
    majorTestName = [testDict objectForKey:kSecTrustTestMajorTestName];
    minorTestName = [testDict objectForKey:kSecTrustTestMinorTestName];
    require_action_quiet(majorTestName && minorTestName, testOut,
                   fail("Failed to create test names for test %lu",(unsigned long)idx));
    test = [[TestObject alloc] initWithMajorTestName:majorTestName minorTestName:minorTestName];
    require_action_quiet((test), testOut, fail("%@-%@: failed to create test object", majorTestName, minorTestName));

    /* Populate the certificates array */
    require_quiet([test addLeafToCertificates:[testDict objectForKey:kSecTrustTestLeaf]], testOut);
    require_quiet([test addIntermediatesToCertificates:[testDict objectForKey:kSecTrustTestIntermediates]], testOut);

    /* Optionally: enable test certificates for the policy */
    if (enableTestCertificates) {
        /* Note: Some of the policies use defaults writes with the "com.apple.Security" domain; 
         * others use "com.apple.security". Set both since we don't know which one this is. */
        CFPreferencesSetAppValue((__bridge CFStringRef)[testDict objectForKey:kSecTrustTestEnableTestCerts],
                                 kCFBooleanTrue, CFSTR("com.apple.Security"));
        CFPreferencesSetAppValue((__bridge CFStringRef)[testDict objectForKey:kSecTrustTestEnableTestCerts],
                                 kCFBooleanTrue, CFSTR("com.apple.security"));
    }

    /* Create the policies */
    require_quiet([test addPolicies:[testDict objectForKey:kSecTrustTestPolicies]], testOut);

    /* Create the trust object */
    require_noerr_action_quiet(SecTrustCreateWithCertificates((__bridge CFArrayRef)test.certificates,
                                                              (__bridge CFArrayRef)test.policies,
                                                              &trust),
                               testOut,
                               fail("%@: failed to create trust ref", test.fullTestName));

    /* Optionally set anchors in trust object */
    if ([testDict objectForKey:kSecTrustTestAnchors]) {
        require_quiet([test addAnchors:[testDict objectForKey:kSecTrustTestAnchors]], testOut);
        require_noerr_action_quiet(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)test.anchors),
                                   testOut,
                                   fail("%@: failed to add anchors to trust ref", test.fullTestName));
    }

    /* Set optional date in trust object */
    verifyDate = [testDict objectForKey:kSecTrustTestVerifyDate];
    if (verifyDate) {
        require_noerr_action_quiet(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)verifyDate), testOut,
                                   fail("%@: failed to set verify date, %@, in trust ref", test.fullTestName,
                                        verifyDate));
    }

    /* Evaluate */
    require_noerr_action_quiet(SecTrustEvaluate(trust, &trustResult), testOut,
                               fail("%@: failed to evaluate trust", test.fullTestName));

    /* Check results */
    require_action_quiet(expectedResult = [testDict objectForKey:kSecTrustTestExpectedResult],
                         testOut, fail("%@: failed to get expected result for test", test.fullTestName));

    /* If we enabled test certificates on a non-internal device, expect a failure instead of succees. */
    if (enableTestCertificates && !SecIsInternalRelease() && ([expectedResult unsignedIntValue] == 4)) {
        ok(trustResult == 5,
           "%@: actual trust result %u did not match expected trust result %u",
           test.fullTestName, trustResult, 5);
    } else {
        ok(trustResult == [expectedResult unsignedIntValue],
           "%@: actual trust result %u did not match expected trust result %u",
           test.fullTestName, trustResult, [expectedResult unsignedIntValue]);
    }
    require_quiet(trustResult == [expectedResult unsignedIntValue], testOut);

    require_quiet(chainLen = [testDict objectForKey:kSecTrustTestChainLength], testOut);
    require_action_quiet(SecTrustGetCertificateCount(trust) == [chainLen longValue], testOut,
                         fail("%@: actual chain length %ld did not match expected chain length %ld",
                              test.fullTestName, SecTrustGetCertificateCount(trust), [chainLen longValue]));

testOut:
    // Unset preferences to prevent contamination
    if (enableTestCertificates) {
        CFPreferencesSetAppValue((__bridge CFStringRef)[testDict objectForKey:kSecTrustTestEnableTestCerts],
                                 kCFBooleanFalse, CFSTR("com.apple.security"));
        CFPreferencesSetAppValue((__bridge CFStringRef)[testDict objectForKey:kSecTrustTestEnableTestCerts],
                                 kCFBooleanFalse, CFSTR("com.apple.Security"));
    }
    CFReleaseNull(trust);
};

static void tests(void)
{
    NSURL *testPlist = nil;
    NSArray *testsArray = nil;

    testPlist = [[NSBundle mainBundle] URLForResource:@"debugging" withExtension:@"plist"
                                         subdirectory:(NSString *)kSecTrustTestPinningPolicyResources ];
    if (!testPlist) {
        testPlist = [[NSBundle mainBundle] URLForResource:nil withExtension:@"plist"
                                             subdirectory:(NSString *)kSecTrustTestPinningPolicyResources ];
    }
    require_action_quiet(testPlist, exit,
                   fail("Failed to get tests plist from %@", kSecTrustTestPinningPolicyResources));

    testsArray = [NSArray arrayWithContentsOfURL: testPlist];
    require_action_quiet(testsArray, exit,
                   fail("Failed to create array from plist"));

    plan_tests((int)[testsArray count]);

    [testsArray enumerateObjectsUsingBlock:runTestForObject];

exit:
    return;
}

int si_20_sectrust_policies(int argc, char *const *argv)
{

    @autoreleasepool {
        tests();
    }

    return 0;
}
