//
//  SMIMEPolicyTests.m
//  Security
//
//  Created by Bailey Basile on 11/5/19.
//

#import <Foundation/Foundation.h>

#include <Security/SecPolicyPriv.h>
#include <utilities/SecCFWrappers.h>

#import "../TrustEvaluationTestHelpers.h"
#import "TrustEvaluationTestCase.h"

@interface SMIMEPolicyTests : TrustEvaluationTestCase
@end

NSString *testDir = @"SMIMEPolicyTests-data";
const CFStringRef emailAddr = CFSTR("test@example.com");

@implementation SMIMEPolicyTests

- (NSArray *)anchors
{
    id root = [self SecCertificateCreateFromResource:@"root" subdirectory:testDir];
    NSArray *anchors = @[root];
    CFRelease((SecCertificateRef)root);
    return anchors;
}

- (NSDate *)verifyDate
{
    return [NSDate dateWithTimeIntervalSinceReferenceDate:600000000.0]; // January 6, 2020 at 2:40:00 AM PST
}

- (bool)runTrustEvalForLeaf:(id)leaf policy:(SecPolicyRef)policy
{
    TestTrustEvaluation *eval = [[TestTrustEvaluation alloc] initWithCertificates:@[leaf] policies:@[(__bridge id)policy]];
    XCTAssertNotNil(eval);
    [eval setAnchors:[self anchors]];
    [eval setVerifyDate:[self verifyDate]];
    return [eval evaluate:nil];
}

// MARK: Expiration tests

- (void)testCheckExpiration
{
    SecPolicyRef policy = SecPolicyCreateSMIME(kSecAnyEncryptSMIME | kSecSignSMIMEUsage, emailAddr);
    id leaf = [self SecCertificateCreateFromResource:@"email_protection" subdirectory:testDir];
    XCTAssertNotNil(leaf);

    // Within validity
    TestTrustEvaluation *eval = [[TestTrustEvaluation alloc] initWithCertificates:@[leaf] policies:@[(__bridge id)policy]];
    XCTAssertNotNil(eval);
    [eval setAnchors:[self anchors]];
    [eval setVerifyDate:[self verifyDate]];
    XCTAssert([eval evaluate:nil]);

    // Expired
    [eval setVerifyDate:[NSDate dateWithTimeIntervalSinceReferenceDate:630000000.0]]; // December 18, 2020 at 8:00:00 AM PST
    XCTAssertFalse([eval evaluate:nil]);

    CFReleaseNull(policy);
    CFRelease((SecCertificateRef)leaf);
}

- (void)testIgnoreExpiration
{
    SecPolicyRef policy = SecPolicyCreateSMIME(kSecAnyEncryptSMIME | kSecSignSMIMEUsage | kSecIgnoreExpirationSMIMEUsage, emailAddr);
    id leaf = [self SecCertificateCreateFromResource:@"email_protection" subdirectory:testDir];
    XCTAssertNotNil(leaf);

    // Within validity
    TestTrustEvaluation *eval = [[TestTrustEvaluation alloc] initWithCertificates:@[leaf] policies:@[(__bridge id)policy]];
    XCTAssertNotNil(eval);
    [eval setAnchors:[self anchors]];
    [eval setVerifyDate:[self verifyDate]];
    XCTAssert([eval evaluate:nil]);

    // Expired
    [eval setVerifyDate:[NSDate dateWithTimeIntervalSinceReferenceDate:630000000.0]]; // December 18, 2020 at 8:00:00 AM PST
    XCTAssert([eval evaluate:nil]);

    CFReleaseNull(policy);
    CFRelease((SecCertificateRef)leaf);
}

// MARK: Name tests

- (void)testNoEmailName
{
    id leaf = [self SecCertificateCreateFromResource:@"no_name" subdirectory:testDir];
    XCTAssertNotNil(leaf);

    // Check Email name
    SecPolicyRef policy = SecPolicyCreateSMIME(kSecAnyEncryptSMIME | kSecSignSMIMEUsage, emailAddr);
    XCTAssertFalse([self runTrustEvalForLeaf:leaf policy:policy]);

    // Skip email name check
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(kSecAnyEncryptSMIME | kSecSignSMIMEUsage, NULL);
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    CFReleaseNull(policy);
    CFRelease((SecCertificateRef)leaf);
}

- (void)testEmailNameSAN
{
    id leaf = [self SecCertificateCreateFromResource:@"san_name" subdirectory:testDir];
    XCTAssertNotNil(leaf);

    // Address match
    SecPolicyRef policy = SecPolicyCreateSMIME(kSecAnyEncryptSMIME | kSecSignSMIMEUsage, emailAddr);
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    // Address mismatch
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(kSecAnyEncryptSMIME | kSecSignSMIMEUsage, CFSTR("wrong@example.com"));
    XCTAssertFalse([self runTrustEvalForLeaf:leaf policy:policy]);

    // Case-insensitive match
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(kSecAnyEncryptSMIME | kSecSignSMIMEUsage, CFSTR("TEST@example.com"));
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    CFReleaseNull(policy);
    CFRelease((SecCertificateRef)leaf);
}

- (void)testEmailCommonName
{
    id leaf = [self SecCertificateCreateFromResource:@"common_name" subdirectory:testDir];
    XCTAssertNotNil(leaf);

    // Address match but common name matches not supported
    SecPolicyRef policy = SecPolicyCreateSMIME(kSecAnyEncryptSMIME | kSecSignSMIMEUsage, emailAddr);
    XCTAssertFalse([self runTrustEvalForLeaf:leaf policy:policy]);

    CFReleaseNull(policy);
    CFRelease((SecCertificateRef)leaf);
}

- (void)testEmailAddressField
{
    id leaf = [self SecCertificateCreateFromResource:@"email_field" subdirectory:testDir];
    XCTAssertNotNil(leaf);

    // Address match but subject name matches not supported
    SecPolicyRef policy = SecPolicyCreateSMIME(kSecAnyEncryptSMIME | kSecSignSMIMEUsage, emailAddr);
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    // Address mismatch
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(kSecAnyEncryptSMIME | kSecSignSMIMEUsage, CFSTR("wrong@example.com"));
    XCTAssertFalse([self runTrustEvalForLeaf:leaf policy:policy]);

    // Case-insensitive match
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(kSecAnyEncryptSMIME | kSecSignSMIMEUsage, CFSTR("TEST@example.com"));
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    CFReleaseNull(policy);
    CFRelease((SecCertificateRef)leaf);
}

// MARK: KU tests
- (void)testNoKU
{
    id leaf = [self SecCertificateCreateFromResource:@"san_name" subdirectory:testDir];
    XCTAssertNotNil(leaf);

    // Look for sign KU (which allows unspecified KU)
    SecPolicyRef policy = SecPolicyCreateSMIME(kSecSignSMIMEUsage, emailAddr);
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    // Look for sign KU or any encrypt KU
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(kSecSignSMIMEUsage | kSecAnyEncryptSMIME, emailAddr);
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    // Look for encrypt KU
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(kSecAnyEncryptSMIME, emailAddr);
    XCTAssertFalse([self runTrustEvalForLeaf:leaf policy:policy]);

    // Don't look at KU
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(0, emailAddr);
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    CFReleaseNull(policy);
    CFRelease((SecCertificateRef)leaf);
}

- (void)testSignKU
{
    id leaf = [self SecCertificateCreateFromResource:@"digital_signature" subdirectory:testDir];
    XCTAssertNotNil(leaf);

    // Look for sign KU
    SecPolicyRef policy = SecPolicyCreateSMIME(kSecSignSMIMEUsage, emailAddr);
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    // Look for sign KU or any encrypt KU
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(kSecSignSMIMEUsage | kSecAnyEncryptSMIME, emailAddr);
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    // Look for encrypt KU
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(kSecAnyEncryptSMIME, emailAddr);
    XCTAssertFalse([self runTrustEvalForLeaf:leaf policy:policy]);

    CFReleaseNull(policy);
    CFRelease((SecCertificateRef)leaf);
}

- (void)testNonRepudiationKU
{
    id leaf = [self SecCertificateCreateFromResource:@"non_repudiation" subdirectory:testDir];
    XCTAssertNotNil(leaf);

    // Look for sign KU
    SecPolicyRef policy = SecPolicyCreateSMIME(kSecSignSMIMEUsage, emailAddr);
    XCTAssertFalse([self runTrustEvalForLeaf:leaf policy:policy]);

    // Look for sign KU or any encrypt KU
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(kSecSignSMIMEUsage | kSecAnyEncryptSMIME, emailAddr);
    XCTAssertFalse([self runTrustEvalForLeaf:leaf policy:policy]);

    // No KU
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(0, emailAddr);
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    CFReleaseNull(policy);
    CFRelease((SecCertificateRef)leaf);
}

- (void)testKeyEncipherKU
{
    id leaf = [self SecCertificateCreateFromResource:@"key_encipher" subdirectory:testDir];
    XCTAssertNotNil(leaf);

    // Look for Key Encipher KU
    SecPolicyRef policy = SecPolicyCreateSMIME(kSecKeyEncryptSMIMEUsage, emailAddr);
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    // Look for any encrypt KU
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(kSecAnyEncryptSMIME, emailAddr);
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    // Look for Data Encipher KU
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(kSecDataEncryptSMIMEUsage, emailAddr);
    XCTAssertFalse([self runTrustEvalForLeaf:leaf policy:policy]);

    CFReleaseNull(policy);
    CFRelease((SecCertificateRef)leaf);
}

- (void)testDataEncipherKU
{
    id leaf = [self SecCertificateCreateFromResource:@"data_encipher" subdirectory:testDir];
    XCTAssertNotNil(leaf);

    // Look for Data Encipher KU
    SecPolicyRef policy = SecPolicyCreateSMIME(kSecDataEncryptSMIMEUsage, emailAddr);
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    // Look for any encrypt KU
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(kSecAnyEncryptSMIME, emailAddr);
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    // Look for Key Encipher KU
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(kSecKeyEncryptSMIMEUsage, emailAddr);
    XCTAssertFalse([self runTrustEvalForLeaf:leaf policy:policy]);

    CFReleaseNull(policy);
    CFRelease((SecCertificateRef)leaf);
}

- (void)testKeyAgreementKU
{
    id leaf = [self SecCertificateCreateFromResource:@"key_agreement" subdirectory:testDir];
    XCTAssertNotNil(leaf);

    // Look for Key Agreement Encrypt KU /* <rdar://57130017> */
    SecPolicyRef policy = SecPolicyCreateSMIME(kSecKeyExchangeEncryptSMIMEUsage, emailAddr);
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    // Look for Key Agreement Decrypt KU /* <rdar://57130017> */
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(kSecKeyExchangeDecryptSMIMEUsage, emailAddr);
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    // Look for Key Agreement Both KU
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(kSecKeyExchangeBothSMIMEUsage, emailAddr);
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    // Look for Digital Signature KU
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(kSecSignSMIMEUsage, emailAddr);
    XCTAssertFalse([self runTrustEvalForLeaf:leaf policy:policy]);

    CFReleaseNull(policy);
    CFRelease((SecCertificateRef)leaf);
}

- (void)testKeyAgreementEncipherOnlyKU
{
    id leaf = [self SecCertificateCreateFromResource:@"key_agreement_encipher_only" subdirectory:testDir];
    XCTAssertNotNil(leaf);

    // Look for Key Agreement Encrypt KU
    SecPolicyRef policy = SecPolicyCreateSMIME(kSecKeyExchangeEncryptSMIMEUsage, emailAddr);
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    // Look for Key Agreement Decrypt KU /* <rdar://57130017> */
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(kSecKeyExchangeDecryptSMIMEUsage, emailAddr);
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    // Look for Key Agreement Both KU
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(kSecKeyExchangeBothSMIMEUsage, emailAddr);
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    // Look for Digital Signature KU
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(kSecSignSMIMEUsage, emailAddr);
    XCTAssertFalse([self runTrustEvalForLeaf:leaf policy:policy]);

    CFReleaseNull(policy);
    CFRelease((SecCertificateRef)leaf);
}

- (void)testKeyAgreementDecipherOnlyKU
{
    id leaf = [self SecCertificateCreateFromResource:@"key_agreement_decipher_only" subdirectory:testDir];
    XCTAssertNotNil(leaf);

    // Look for Key Agreement Encrypt KU /* <rdar://57130017> */
    SecPolicyRef policy = SecPolicyCreateSMIME(kSecKeyExchangeEncryptSMIMEUsage, emailAddr);
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    // Look for Key Agreement Decrypt KU
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(kSecKeyExchangeDecryptSMIMEUsage, emailAddr);
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    // Look for Key Agreement Both KU
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(kSecKeyExchangeBothSMIMEUsage, emailAddr);
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    // Look for Digital Signature KU
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(kSecSignSMIMEUsage, emailAddr);
    XCTAssertFalse([self runTrustEvalForLeaf:leaf policy:policy]);

    CFReleaseNull(policy);
    CFRelease((SecCertificateRef)leaf);
}

- (void)testKeyAgreementBothKU
{
    id leaf = [self SecCertificateCreateFromResource:@"key_agreement_encipher_decipher" subdirectory:testDir];
    XCTAssertNotNil(leaf);

    // Look for Key Agreement Encrypt KU
    SecPolicyRef policy = SecPolicyCreateSMIME(kSecKeyExchangeEncryptSMIMEUsage, emailAddr);
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    // Look for Key Agreement Decrypt KU
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(kSecKeyExchangeDecryptSMIMEUsage, emailAddr);
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    // Look for Key Agreement Both KU
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(kSecKeyExchangeBothSMIMEUsage, emailAddr);
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    // Look for Digital Signature KU
    CFReleaseNull(policy);
    policy = SecPolicyCreateSMIME(kSecSignSMIMEUsage, emailAddr);
    XCTAssertFalse([self runTrustEvalForLeaf:leaf policy:policy]);

    CFReleaseNull(policy);
    CFRelease((SecCertificateRef)leaf);
}

// MARK: EKU tests
- (void)testNoEKU
{
    id leaf = [self SecCertificateCreateFromResource:@"digital_signature" subdirectory:testDir];
    XCTAssertNotNil(leaf);

    SecPolicyRef policy = SecPolicyCreateSMIME(kSecSignSMIMEUsage, emailAddr);
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    CFReleaseNull(policy);
    CFRelease((SecCertificateRef)leaf);
}

- (void)testAnyEKU
{
    id leaf = [self SecCertificateCreateFromResource:@"any_eku" subdirectory:testDir];
    XCTAssertNotNil(leaf);

    SecPolicyRef policy = SecPolicyCreateSMIME(kSecSignSMIMEUsage, emailAddr);
    XCTAssertFalse([self runTrustEvalForLeaf:leaf policy:policy]);

    CFReleaseNull(policy);
    CFRelease((SecCertificateRef)leaf);
}

- (void)testEmailEKU
{
    id leaf = [self SecCertificateCreateFromResource:@"email_protection" subdirectory:testDir];
    XCTAssertNotNil(leaf);

    SecPolicyRef policy = SecPolicyCreateSMIME(kSecSignSMIMEUsage, emailAddr);
    XCTAssert([self runTrustEvalForLeaf:leaf policy:policy]);

    CFReleaseNull(policy);
    CFRelease((SecCertificateRef)leaf);
}

@end
