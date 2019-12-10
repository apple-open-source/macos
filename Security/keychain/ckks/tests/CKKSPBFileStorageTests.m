//
//  CKKSPBFileStorageTests.m
//

#import <XCTest/XCTest.h>

#import "keychain/ckks/CKKSPBFileStorage.h"
#import "keychain/ckks/proto/generated_source/CKKSSerializedKey.h"

@interface CKKSPBFileStorageTests : XCTestCase
@property NSURL * tempDir;
@end

@implementation CKKSPBFileStorageTests

- (void)setUp {
    self.tempDir = [[NSFileManager defaultManager] temporaryDirectory];
}
- (void)tearDown {
    [[NSFileManager defaultManager] removeItemAtURL:self.tempDir error:nil];
    self.tempDir = nil;
}

- (void)testCKKSPBStorage {

    NSURL *file = [self.tempDir URLByAppendingPathComponent:@"file"];

    CKKSPBFileStorage<CKKSSerializedKey *> *pbstorage;

    pbstorage = [[CKKSPBFileStorage alloc] initWithStoragePath:file
                                                  storageClass:[CKKSSerializedKey class]];
    XCTAssertNotNil(pbstorage, "CKKSPBFileStorage should create an object");

    CKKSSerializedKey *storage = pbstorage.storage;
    storage.uuid = @"uuid";
    storage.zoneName = @"uuid";
    storage.keyclass = @"ak";
    storage.key = [NSData data];

    [pbstorage setStorage:storage];

    pbstorage = [[CKKSPBFileStorage alloc] initWithStoragePath:file
                                                  storageClass:[CKKSSerializedKey class]];
    XCTAssertNotNil(pbstorage, "CKKSPBFileStorage should create an object");

    XCTAssertEqualObjects(pbstorage.storage.keyclass, @"ak", "should be the same");
}

@end
