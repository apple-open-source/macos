//
//  SecAnchorCacheTests.m
//  Security
//
//

#import <Foundation/Foundation.h>
#include <AssertMacros.h>
#import <XCTest/XCTest.h>

#import <Security/SecPolicyPriv.h>
#include <utilities/SecAppleAnchorPriv.h>
#import "trust/trustd/SecAnchorCache.h"

#import "TrustDaemonTestCase.h"

@interface AnchorCacheTests : TrustDaemonTestCase
@end

@implementation AnchorCacheTests

+ (void)setUp {
    [super setUp];
    SecAnchorCacheInitialize();
}

- (void)testCopyAnchors {
#if TARGET_OS_BRIDGE
    /* bridgeOS doesn't use trust store */
    XCTSkip();
#endif
    /* Apple Anchors */
    NSArray *anchors = CFBridgingRelease(SecAnchorCacheCopyAnchors(kSecPolicyAppleMobileAsset));
    XCTAssertNotNil(anchors);
    NSArray *appleAnchors = (__bridge NSArray *)SecGetAppleTrustAnchors(false);
    XCTAssertEqualObjects(anchors, appleAnchors);

    /* Constrained Anchors */
    anchors = CFBridgingRelease(SecAnchorCacheCopyAnchors(kSecPolicyAppleVerifiedMark));
    XCTAssertNotNil(anchors);
    XCTAssertGreaterThan(anchors.count, 0);

    anchors = CFBridgingRelease(SecAnchorCacheCopyAnchors(kSecPolicyAppleMDLTerminalAuth));
    XCTAssertNotNil(anchors);
    XCTAssertGreaterThan(anchors.count, 0);

    /* System anchors */
    anchors = CFBridgingRelease(SecAnchorCacheCopyAnchors(kSecPolicyAppleX509Basic));
    XCTAssertNotNil(anchors);
    XCTAssertGreaterThan(anchors.count, 1);

    NSArray *ocspAnchors = CFBridgingRelease(SecAnchorCacheCopyAnchors(kSecPolicyAppleOCSPSigner));
    XCTAssertNotNil(ocspAnchors);
    XCTAssertEqualObjects(anchors, ocspAnchors);

    /* Prime anchor cache and then copy anchors again */
    CFStringRef sectigoServerAuthRootLookupKey = CFSTR("6FAEB525494DCEC35FC629C946482912999E2EC8"); // Sectigo Public Server Authentication Root R46
    NSArray *parents = CFBridgingRelease(SecAnchorCacheCopyParentCertificates(sectigoServerAuthRootLookupKey));
    XCTAssertNotNil(parents);
    NSArray *cachedAnchors = CFBridgingRelease(SecAnchorCacheCopyAnchors(kSecPolicyAppleX509Basic));
    XCTAssertNotNil(cachedAnchors);
    XCTAssertEqualObjects(anchors, cachedAnchors);
}

@end
