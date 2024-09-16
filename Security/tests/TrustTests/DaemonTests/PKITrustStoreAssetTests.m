//
//  PKITrustStoreAssetTests.m
//

#import <XCTest/XCTest.h>
#import <Foundation/Foundation.h>
#import <Security/SecTrustPriv.h>
#import "trust/trustd/trustdFileLocations.h"
#import "trust/trustd/OTAAutoAssetClient.h"
#import "trust/trustd/OTATrustUtilities.h"

#import "TrustDaemonTestCase.h"

@interface PKITrustStoreAssetInitializationTests : TrustDaemonInitializationTestCase
@end

@implementation PKITrustStoreAssetInitializationTests

- (void)testInvalidSavedAssetPath {
#if TARGET_OS_BRIDGE
    /* bridgeOS doesn't use Mobile Asset */
    XCTSkip();
#endif
    NSError *assetError = NULL;
    OTAAutoAssetClient *autoAssetClient = [[OTAAutoAssetClient alloc] initWithError:&assetError];
    XCTAssert(autoAssetClient != NULL && assetError == NULL);
    // save an asset path that's invalid
    NSString *fakePath = @"/private/tmp/com_apple_MobileAsset_PKITrustStore";
    BOOL didSave = [OTAAutoAssetClient saveTrustStoreAssetPath:fakePath];
    // rdar:// 121743620: if we can't save the asset path in the test environment, bail out now
    if (!didSave) { XCTSkip(); }
    XCTAssert(didSave == YES);
    // read it back to make sure it was written
    NSString *savedPath = [OTAAutoAssetClient savedTrustStoreAssetPath];
    XCTAssert(savedPath != NULL);
    XCTAssertEqualObjects(savedPath, fakePath);
    // make sure our validation method returns NULL for the fake path, even without existing
    NSString *resolvedPath = [OTAAutoAssetClient validTrustStoreAssetPath:fakePath mustExist:NO];
    XCTAssert(resolvedPath == NULL);
}

- (void)testValidSavedAssetPath {
#if TARGET_OS_BRIDGE
    /* bridgeOS doesn't use Mobile Asset */
    XCTSkip();
#endif
    NSError *assetError = NULL;
    OTAAutoAssetClient *autoAssetClient = [[OTAAutoAssetClient alloc] initWithError:&assetError];
    XCTAssert(autoAssetClient != NULL && assetError == NULL);
    // we can't just call [autoAssetClient startUsingLocalAsset] since that will kill our test
    // as soon as the completion routine decides to exit.
    // instead: save a test asset path whose directory components exist.
    // note: there can be a race for creation of our PKITrustStore and PKITrustSupplementals
    // directories in the AssetsV2 directory, so use a pre-existing directory instead.
#if (TARGET_OS_MAC && !TARGET_OS_IPHONE)
    NSString *assetPath = @"/System/Library/AssetsV2/persisted";
#else
    NSString *assetPath = @"/private/var/MobileAsset/AssetsV2/persisted";
#endif
    BOOL didSave = [[NSFileManager defaultManager] fileExistsAtPath:assetPath];
    if (didSave) { didSave = [OTAAutoAssetClient saveTrustStoreAssetPath:assetPath]; }
    // rdar://121743620: if we can't save the asset path in the test environment, bail out now
    if (!didSave) { XCTSkip(); }
    XCTAssert(didSave == YES);
    // read it back to make sure it was written
    NSString *savedPath = [OTAAutoAssetClient savedTrustStoreAssetPath];
    XCTAssert(savedPath != NULL);
    XCTAssertEqualObjects(savedPath, assetPath);
    // make sure our validation method returns the validated path
    NSString *resolvedPath = [OTAAutoAssetClient validTrustStoreAssetPath:assetPath mustExist:NO];
    XCTAssert(resolvedPath != NULL);
    XCTAssertEqualObjects(resolvedPath, assetPath);
}

- (void)testInitializeSecOTAPKIRef {
#if TARGET_OS_BRIDGE
    /* bridgeOS doesn't use Mobile Asset */
    XCTSkip();
#endif
    SecOTAPKIRef otapki = SecOTAPKICopyCurrentOTAPKIRef();
    XCTAssert(otapki != NULL);
    CFDictionaryRef table = SecOTAPKICopyAnchorLookupTable(otapki);
    XCTAssert(table != NULL);
    if (table) { CFRelease(table); }
    if (otapki) { CFRelease(otapki); }
}

- (void)testTrustStoreAssetVersion {
/* minimum possible version string is "0.0.0.0.1,0" */
#define MINIMUM_ASSET_VERSION_STR_LENGTH 11
#if TARGET_OS_BRIDGE
    /* bridgeOS doesn't use Mobile Asset or trust store */
    XCTSkip();
#endif
    CFStringRef assetVersion = SecTrustCopyTrustStoreAssetVersion(NULL);
    if (!assetVersion) { XCTSkip(); } /* may be NULL if we have no readable asset */
    NSUInteger assetVersionLength = [(__bridge NSString*)assetVersion length];
    XCTAssert(assetVersionLength >= MINIMUM_ASSET_VERSION_STR_LENGTH);
    if (assetVersion) { CFRelease(assetVersion); }
}

- (void)testTrustStoreContentDigest {
/* content digest uses SHA256, so hex string representation is sizeof(digest)*2 */
#define SHA256_DIGEST_STR_LENGTH 32*2
#if TARGET_OS_BRIDGE
    /* bridgeOS doesn't use Mobile Asset or trust store */
    XCTSkip();
#endif
    CFStringRef contentDigest = SecTrustCopyTrustStoreContentDigest(NULL);
    XCTAssert(contentDigest != NULL); /* trust store should always have a content digest */
    NSUInteger contentDigestLength = [(__bridge NSString*)contentDigest length];
    XCTAssert(contentDigestLength == SHA256_DIGEST_STR_LENGTH);
    if (contentDigest) { CFRelease(contentDigest); }
}

@end
