//
//  CAIssuerCacheTests.m
//  CAIssuerCacheTests
//

#import <Foundation/Foundation.h>

#include <AssertMacros.h>
#import <XCTest/XCTest.h>
#import <Security/SecCertificatePriv.h>
#include <utilities/SecCFWrappers.h>
#include <sqlite3.h>
#import "trust/trustd/SecCAIssuerCache.h"
#import "trust/trustd/SecCAIssuerRequest.h"
#import "trust/trustd/SecTrustServer.h"

#import "../TrustEvaluationTestHelpers.h"
#import "TrustDaemonTestCase.h"
#import "CAIssuerCacheTests_data.h"

@interface CAIssuerCacheTests : TrustDaemonTestCase
@end

@implementation CAIssuerCacheTests

static SecCertificateRef apple_leaf = NULL;
static SecCertificateRef apple_ca = NULL;
static NSURL *apple_uri = nil;

+ (void)setUp {
    [super setUp];
    apple_uri = [NSURL URLWithString:@"http://certs.apple.com/apsecc12g1.der"];
    apple_leaf = SecCertificateCreateWithBytes(NULL, _apple_caissuer, sizeof(_apple_caissuer));
    apple_ca = SecCertificateCreateWithBytes(NULL, _apple_public_ca, sizeof(_apple_public_ca));
}

+ (void)tearDown {
    CFReleaseNull(apple_leaf);
    CFReleaseNull(apple_ca);
    [super tearDown];
}

- (void)setUp
{
    /* Delete the cache DB so we can start fresh */
    SecCAIssuerCacheClear();
}

- (void)testCacheOps {
    // Insert entry into cache and verify can get it back (when expired allowed)
    SecCAIssuerCacheAddCertificates((__bridge CFArrayRef)@[(__bridge id)apple_ca], (__bridge CFURLRef)apple_uri, CFAbsoluteTimeGetCurrent());
    NSArray *result = CFBridgingRelease(SecCAIssuerCacheCopyMatching((__bridge CFURLRef)apple_uri, true));
    XCTAssertNotNil(result);
    XCTAssertEqual(result.count, 1);
    XCTAssert(CFEqualSafe(apple_ca, (__bridge SecCertificateRef)result[0]));

    // Verify can't get back entry when expired not allowed
    result = CFBridgingRelease(SecCAIssuerCacheCopyMatching((__bridge CFURLRef)apple_uri, false));
    XCTAssertNil(result);

    // Verify entry not present after clearing
    SecCAIssuerCacheClear();
    result = CFBridgingRelease(SecCAIssuerCacheCopyMatching((__bridge CFURLRef)apple_uri, true));
    XCTAssertNil(result);
}

static XCTestExpectation *static_expectation = nil;
static CFArrayRef static_certs = NULL;
static void Callback(void *context, CFArrayRef certs) {
    static_certs = CFRetainSafe(certs);
    [static_expectation fulfill];
}

- (void)testNetworkThenCache {
#if TARGET_OS_BRIDGE || TARGET_OS_WATCH
    /* watchOS and bridgeOS don't support networking in trustd */
    XCTSkip();
#endif
    /* Verify that the revocation server is potentially reachable */
    XCTSkipIf(!ping_host("certs.apple.com", "80"), @"Unable to contact required network resource");

    NSArray *result = CFBridgingRelease(SecCAIssuerCacheCopyMatching((__bridge CFURLRef)apple_uri, true));
    XCTAssertNil(result);

    /* set up the context */
    dispatch_queue_t queue = dispatch_queue_create("testQueue", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
    SecPathBuilderRef builder = SecPathBuilderCreate(queue, SecTrustServerCopySelfAuditToken(),
                                                     (__bridge CFArrayRef)@[(__bridge id)apple_leaf],
                                                     NULL, false, false, NULL, NULL, NULL, NULL, 0.0, NULL, NULL, NULL, NULL);

    // Call SecCAIssuerCopyParents and verify we get cert(s) in the callback
    static_expectation = [self expectationWithDescription:@"callback occurs"];
    XCTAssertFalse(SecCAIssuerCopyParents(apple_leaf, builder, Callback));
    [self waitForExpectations:@[static_expectation] timeout:10.0];
    static_expectation = nil;
    NSArray *receivedCerts = CFBridgingRelease(static_certs);
    XCTAssertNotNil(receivedCerts);
    static_certs = NULL;
    CFReleaseNull(builder);

    // Entry added to cache should not be expired
    result = CFBridgingRelease(SecCAIssuerCacheCopyMatching((__bridge CFURLRef)apple_uri, false));
    XCTAssertNotNil(result);
    XCTAssertEqualObjects(result, receivedCerts);
}

- (void)testExpiredCacheNetworkSuccess {
#if TARGET_OS_BRIDGE || TARGET_OS_WATCH
    /* watchOS and bridgeOS don't support networking in trustd */
    XCTSkip();
#endif
    /* Verify that the revocation server is potentially reachable */
    XCTSkipIf(!ping_host("certs.apple.com", "80"), @"Unable to contact required network resource");

    // Insert expired entry for (successful) URI into cache
    SecCAIssuerCacheAddCertificates((__bridge CFArrayRef)@[(__bridge id)apple_ca], (__bridge CFURLRef)apple_uri, CFAbsoluteTimeGetCurrent() - 10);
    NSArray *result = CFBridgingRelease(SecCAIssuerCacheCopyMatching((__bridge CFURLRef)apple_uri, false));
    XCTAssertNil(result);

    /* set up the context */
    dispatch_queue_t queue = dispatch_queue_create("testQueue", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
    SecPathBuilderRef builder = SecPathBuilderCreate(queue, SecTrustServerCopySelfAuditToken(),
                                                     (__bridge CFArrayRef)@[(__bridge id)apple_leaf],
                                                     NULL, false, false, NULL, NULL, NULL, NULL, 0.0, NULL, NULL, NULL, NULL);

    // Call SecCAIssuerCopyParents and verify we get cert(s) in the callback
    static_expectation = [self expectationWithDescription:@"callback occurs"];
    XCTAssertFalse(SecCAIssuerCopyParents(apple_leaf, builder, Callback));
    [self waitForExpectations:@[static_expectation] timeout:10.0];
    static_expectation = nil;
    NSArray *receivedCerts = CFBridgingRelease(static_certs);
    static_certs = NULL;

    XCTAssertNotNil(receivedCerts);

    CFReleaseNull(builder);

    // Entry updated in cache should not be expired
    result = CFBridgingRelease(SecCAIssuerCacheCopyMatching((__bridge CFURLRef)apple_uri, false));
    XCTAssertNotNil(result);
    XCTAssertEqualObjects(result, receivedCerts);
}

- (void)testExpiredCacheNetworkFail {
    const uint8_t hostname[] = "bad.example.com";
    NSURL *bad_uri = [NSURL URLWithString:@"http://bad.example.com/apsecc12g1.der"];
    NSMutableData *badApple = [NSMutableData dataWithBytes:_apple_caissuer length:sizeof(_apple_caissuer)];
    [badApple replaceBytesInRange:NSMakeRange(491, 15) withBytes:hostname]; // replace the CAIssuer URI hostname in the cert
    SecCertificateRef badAppleCert = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)badApple);

    // Insert expired entry for (failing) URI into cache
    SecCAIssuerCacheAddCertificates((__bridge CFArrayRef)@[(__bridge id)apple_ca], (__bridge CFURLRef)bad_uri, CFAbsoluteTimeGetCurrent() - 10);
    NSArray *result = CFBridgingRelease(SecCAIssuerCacheCopyMatching((__bridge CFURLRef)bad_uri, false));
    XCTAssertNil(result);

    /* set up the context */
    dispatch_queue_t queue = dispatch_queue_create("testQueue", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
    SecPathBuilderRef builder = SecPathBuilderCreate(queue, SecTrustServerCopySelfAuditToken(),
                                                     (__bridge CFArrayRef)@[(__bridge id)badAppleCert],
                                                     NULL, false, false, NULL, NULL, NULL, NULL, 0.0, NULL, NULL, NULL, NULL);

    // Call SecCAIssuerCopyParents and verify we still get cert in the callback
    static_expectation = [self expectationWithDescription:@"callback occurs"];
    XCTAssertFalse(SecCAIssuerCopyParents(badAppleCert, builder, Callback));
    [self waitForExpectations:@[static_expectation] timeout:10.0];
    static_expectation = nil;
    NSArray *receivedCerts = CFBridgingRelease(static_certs);
    static_certs = NULL;

    XCTAssertNotNil(receivedCerts);
    XCTAssertEqual(receivedCerts.count, 1);
    XCTAssert(CFEqualSafe(apple_ca, (__bridge SecCertificateRef)receivedCerts[0]));

    CFReleaseNull(builder);

    // Entry updated in cache should still be expired
    result = CFBridgingRelease(SecCAIssuerCacheCopyMatching((__bridge CFURLRef)apple_uri, false));
    XCTAssertNil(result);
    CFReleaseNull(badAppleCert);
}


@end
