//
//  PKITrustStoreAssetTests.m
//

#import <XCTest/XCTest.h>
#import <Foundation/Foundation.h>
#import <Security/SecCertificatePriv.h>
#import <Security/SecCertificateInternal.h>
#import <Security/SecPolicyPriv.h>
#import <Security/SecTrustPriv.h>
#import <utilities/SecCFRelease.h>
#import <sqlite3.h>
#import "trust/trustd/trustdFileLocations.h"
#import "trust/trustd/OTAAutoAssetClient.h"
#import "trust/trustd/OTATrustUtilities.h"
#import "trust/trustd/trustd_spi.h"

#import "TrustDaemonTestCase.h"
#import "PKITrustStoreAssetTests_data.h"

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

- (void)testConstrainedTestAnchors {
#if TARGET_OS_BRIDGE
    /* bridgeOS doesn't use trust store */
    XCTSkip();
#endif
    /* Set up test by writing the constrained anchors file */
    NSData *anchorData = [NSData dataWithBytes:_constrained_test_anchor length:sizeof(_constrained_test_anchor)];
    NSString *anchorB64 = [anchorData base64EncodedStringWithOptions:0];
    NSDictionary *testAnchors = @{
        @"1.2.840.113635.100.1.122" : @[
            anchorB64
        ]
    };
    NSURL *testAnchorsUrl = CFBridgingRelease(SecCopyURLForFileInPrivateTrustdDirectory(CFSTR("ConstrainedTestAnchors.plist")));
    XCTAssert([testAnchors writeToURL:testAnchorsUrl error:nil]);

    /* Intialize the Trust Store */
    SecOTAPKIRef store = SecOTAPKICopyCurrentOTAPKIRef();
    XCTAssertNotEqual(NULL, store);

    /* Verify that the test anchors are in the constrained anchors */
    NSDictionary *anchorLookupTable = CFBridgingRelease(SecOTAPKICopyConstrainedAnchorLookupTable(store));
    XCTAssertNotNil(anchorLookupTable);

    SecCertificateRef anchor = SecCertificateCreateWithBytes(NULL, _constrained_test_anchor, sizeof(_constrained_test_anchor));
    NSString *anchorLookupKey = CFBridgingRelease(SecCertificateCopyAnchorLookupKey(anchor));
    XCTAssertNotNil(anchorLookupKey);
    XCTAssertNotNil(anchorLookupTable[anchorLookupKey]);
    NSArray *anchorRecords = anchorLookupTable[anchorLookupKey];
    NSDictionary *anchorRecord = anchorRecords[0];
    NSArray *oids = anchorRecord[@"oids"];
    XCTAssertNotNil(oids);
    XCTAssert([oids containsObject:@"1.2.840.113635.100.1.122"]);

    NSString *sha2 = anchorRecord[@"sha2"];
    NSData *storeData = CFBridgingRelease(SecOTAPKICopyConstrainedAnchorData(store, (__bridge CFStringRef)sha2));
    XCTAssertNotNil(storeData);
    SecCertificateRef storeCert = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)storeData);
    XCTAssertNotEqual(NULL, storeCert);
    XCTAssert(CFEqual(anchor, storeCert));

    CFReleaseNull(anchor);
    CFReleaseNull(storeCert);

    /* Now that we know the store with the test anchor is ok,
     * let's finish initializing trustd and do an eval with that anchor */
    trustd_init_server();

    NSArray *orgs = @[ @"Apple Inc.", @"Test Org", @"SEAR" ];
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _constrained_test_leaf, sizeof(_constrained_test_leaf));
    XCTAssertNotEqual(NULL, leaf);
    SecPolicyRef policy = SecPolicyCreate3PMobileAsset((__bridge CFArrayRef)orgs);
    XCTAssertNotEqual(NULL, policy);
    SecTrustRef trust = NULL;
    SecTrustCreateWithCertificates(leaf, policy, &trust);
    XCTAssertNotEqual(NULL, trust);
    SecTrustSetVerifyDate(trust, (__bridge CFDateRef)[NSDate dateWithTimeIntervalSince1970:1737772686]);

    XCTAssert(SecTrustEvaluateWithError(trust, NULL));

    CFReleaseNull(leaf);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(store);
}

@end
