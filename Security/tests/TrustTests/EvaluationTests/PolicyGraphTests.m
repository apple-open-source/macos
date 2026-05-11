//
//  PolicyGraphTests.m
//  Security
//

#include <utilities/SecCFWrappers.h>

#import "TrustEvaluationTestCase.h"
#include "../TestMacroConversions.h"
#include "../TrustEvaluationTestHelpers.h"

const NSString *kSecTrustTestPolicyGraphResources = @"PolicyGraphTests-data";

@interface PolicyGraphTests : TrustEvaluationTestCase
@end

@implementation PolicyGraphTests

// Chain with 14 Policy Mappings
- (void)test173742116_DHS_Federal_Common_Root {
    id leaf = [self SecCertificateCreateFromResource:@"m1prwbdhsid02.dhsid.dhs"
                                       subdirectory:(NSString *)kSecTrustTestPolicyGraphResources];
    id dhsCA4 = [self SecCertificateCreateFromResource:@"dhs_ca4"
                                          subdirectory:(NSString *)kSecTrustTestPolicyGraphResources];
    id treasuryRoot = [self SecCertificateCreateFromResource:@"US_Treasury_Root_CA"
                                                subdirectory:(NSString *)kSecTrustTestPolicyGraphResources];
    id federalCommonRoot = [self SecCertificateCreateFromResource:@"Federal_Common_Policy_CA_G2"
                                                    subdirectory:(NSString *)kSecTrustTestPolicyGraphResources];
    XCTAssertNotNil(leaf, "Failed to load leaf cert");
    XCTAssertNotNil(dhsCA4, "Failed to load DHS CA4 cert");
    XCTAssertNotNil(treasuryRoot, "Failed to load US Treasury Root CA cert");
    XCTAssertNotNil(federalCommonRoot, "Failed to load Federal Common Policy CA G2 cert");

    NSArray *certs = @[ leaf, dhsCA4, treasuryRoot ];
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("m1prwbdhsid02.dhsid.dhs"));

    TestTrustEvaluation *test = [[TestTrustEvaluation alloc] initWithCertificates:certs
                                                                        policies:@[(__bridge id)policy]];
    [test setAnchors:@[ federalCommonRoot ]];
    [test setVerifyDate:[NSDate dateWithTimeIntervalSinceReferenceDate:788000000.0]]; // December 21, 2025

    NSError *error = nil;
    bool result = [test evaluate:&error];

    // Log debugging information
    NSLog(@"Trust evaluation result: %d", result);
    NSLog(@"Trust result type: %u", [test trustResult]);
    if (error) {
        NSLog(@"Trust evaluation error: %@", error);
    }
    if ([test resultDictionary]) {
        NSLog(@"Result dictionary: %@", [test resultDictionary]);
    }

    XCTAssertTrue(result, "Trust evaluation should succeed for DHS Federal Common chain, error: %@", error);

    CFBridgingRelease((__bridge SecCertificateRef)leaf);
    CFBridgingRelease((__bridge SecCertificateRef)dhsCA4);
    CFBridgingRelease((__bridge SecCertificateRef)treasuryRoot);
    CFBridgingRelease((__bridge SecCertificateRef)federalCommonRoot);
    CFReleaseNull(policy);
}

/* RFC 9618 Section 5.4 b.2 per-IDP conformance test.
 * Intermediate has policies {PolicyA, anyPolicy} and mappings {PolicyA→PolicyX, PolicyB→PolicyY}.
 * PolicyA matches a node at depth 1 (b.1 applies). PolicyB has NO node at depth 1,
 * so b.2 must create a PolicyB node via the anyPolicy node.
 * Leaf has {PolicyX, PolicyY}. PolicyY only matches if b.2 ran for the unmatched PolicyB IDP.
 * Without the per-IDP fix, b.2 is skipped entirely because PolicyA matched (idp_match=true),
 * causing PolicyY to fail to match and the evaluation to fail. */
- (void)testPolicyGraph_b2_partial_IDP_match {
    id leaf = [self SecCertificateCreateFromResource:@"b2_leaf"
                                       subdirectory:(NSString *)kSecTrustTestPolicyGraphResources];
    id intermediate = [self SecCertificateCreateFromResource:@"b2_intermediate"
                                               subdirectory:(NSString *)kSecTrustTestPolicyGraphResources];
    id root = [self SecCertificateCreateFromResource:@"b2_root"
                                        subdirectory:(NSString *)kSecTrustTestPolicyGraphResources];
    XCTAssertNotNil(leaf, "Failed to load b2 leaf cert");
    XCTAssertNotNil(intermediate, "Failed to load b2 intermediate cert");
    XCTAssertNotNil(root, "Failed to load b2 root cert");

    NSArray *certs = @[ leaf, intermediate ];
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("b2test.example.com"));

    TestTrustEvaluation *test = [[TestTrustEvaluation alloc] initWithCertificates:certs
                                                                        policies:@[(__bridge id)policy]];
    [test setAnchors:@[ root ]];
    [test setVerifyDate:[NSDate dateWithTimeIntervalSinceReferenceDate:804000000.0]]; // June 2026

    NSError *error = nil;
    bool result = [test evaluate:&error];

    NSLog(@"b2 partial IDP: result=%d trustResult=%u error=%@", result, [test trustResult], error);
    if ([test resultDictionary]) {
        NSLog(@"b2 result dictionary: %@", [test resultDictionary]);
    }

    XCTAssertTrue(result, "b.2 per-IDP: evaluation should succeed when some IDPs match and others need anyPolicy fallback, error: %@", error);

    CFBridgingRelease((__bridge SecCertificateRef)leaf);
    CFBridgingRelease((__bridge SecCertificateRef)intermediate);
    CFBridgingRelease((__bridge SecCertificateRef)root);
    CFReleaseNull(policy);
}

/* Size inflation conformance test.
 * Intermediate has policies {PolicyA, PolicyB, anyPolicy} and mappings {A→C, B→C}.
 * Both PolicyA and PolicyB at depth 1 get expected_policy_set = {PolicyC}.
 * Leaf has {PolicyC}. PolicyC matches BOTH parents' expected sets, so
 * policy_graph_add_child_if_match calls policy_graph_add_child for the same child twice.
 * Without the containsObject guard, graph->size is inflated (incremented twice for one node).
 * This chain should pass regardless, but with many such overlaps the inflated size
 * could prematurely trigger the POLICY_TREE_MAX_NODES limit. */
- (void)testPolicyGraph_multi_parent_size_tracking {
    id leaf = [self SecCertificateCreateFromResource:@"mp_leaf"
                                       subdirectory:(NSString *)kSecTrustTestPolicyGraphResources];
    id intermediate = [self SecCertificateCreateFromResource:@"mp_intermediate"
                                               subdirectory:(NSString *)kSecTrustTestPolicyGraphResources];
    id root = [self SecCertificateCreateFromResource:@"mp_root"
                                        subdirectory:(NSString *)kSecTrustTestPolicyGraphResources];
    XCTAssertNotNil(leaf, "Failed to load mp leaf cert");
    XCTAssertNotNil(intermediate, "Failed to load mp intermediate cert");
    XCTAssertNotNil(root, "Failed to load mp root cert");

    NSArray *certs = @[ leaf, intermediate ];
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("multiparent.example.com"));

    TestTrustEvaluation *test = [[TestTrustEvaluation alloc] initWithCertificates:certs
                                                                        policies:@[(__bridge id)policy]];
    [test setAnchors:@[ root ]];
    [test setVerifyDate:[NSDate dateWithTimeIntervalSinceReferenceDate:804000000.0]]; // June 2026

    NSError *error = nil;
    bool result = [test evaluate:&error];

    NSLog(@"multi parent: result=%d trustResult=%u error=%@", result, [test trustResult], error);
    if ([test resultDictionary]) {
        NSLog(@"multi parent result dictionary: %@", [test resultDictionary]);
    }

    XCTAssertTrue(result, "Multi-parent: evaluation should succeed when child policy matches multiple parents' expected sets, error: %@", error);

    CFBridgingRelease((__bridge SecCertificateRef)leaf);
    CFBridgingRelease((__bridge SecCertificateRef)intermediate);
    CFBridgingRelease((__bridge SecCertificateRef)root);
    CFReleaseNull(policy);
}

@end
