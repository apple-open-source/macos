/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#if OCTAGON

#import <XCTest/XCTest.h>
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSTLKShareRecord.h"
#import "keychain/ckks/CKKSPeer.h"
#import "keychain/ckks/tests/CloudKitMockXCTest.h"

#import <SecurityFoundation/SFSigningOperation.h>
#import <SecurityFoundation/SFKey.h>
#import <SecurityFoundation/SFKey_Private.h>
#import <SecurityFoundation/SFDigestOperation.h>

@interface CloudKitKeychainTLKSharingEncryptionTests : CloudKitMockXCTest
@property CKKSKey* tlk;
@property CKKSSOSSelfPeer* localPeer;
@property CKKSSOSSelfPeer* remotePeer;
@property CKKSSOSSelfPeer* remotePeer2;
@end

@implementation CloudKitKeychainTLKSharingEncryptionTests

- (void)setUp {
    // We don't really want to spin up the whole machinery for the encryption tests
    SecCKKSDisable();

    NSError* error = nil;
    self.tlk = [CKKSKey randomKeyWrappedBySelf:[[CKRecordZoneID alloc] initWithZoneName:@"testzone" ownerName:CKCurrentUserDefaultName] error:&error];
    XCTAssertNil(error, "Shouldn't be an error creating a new TLK");

    self.localPeer = [[CKKSSOSSelfPeer alloc] initWithSOSPeerID:@"local"
                                                  encryptionKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                     signingKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                       viewList:self.managedViewList];
    XCTAssertNotNil(self.localPeer, "Should be able to make a new local peer");

    self.remotePeer = [[CKKSSOSSelfPeer alloc] initWithSOSPeerID:@"remote"
                                                   encryptionKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                      signingKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                        viewList:self.managedViewList];
    XCTAssertNotNil(self.remotePeer, "Should be able to make a new remote peer");

    self.remotePeer2 = [[CKKSSOSSelfPeer alloc] initWithSOSPeerID:@"remote"
                                                    encryptionKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                       signingKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                         viewList:self.managedViewList];
    XCTAssertNotNil(self.remotePeer2, "Should be able to make a new remote peer");

    [super setUp];
}

- (void)tearDown {
    [super tearDown];
}

- (void)testKeyWrapAndUnwrap {
    NSError* error = nil;
    CKKSTLKShareRecord* share = [CKKSTLKShareRecord share:self.tlk
                                           as:self.localPeer
                                           to:self.remotePeer
                                        epoch:-1
                                     poisoned:0
                                        error:&error];
    XCTAssertNil(error, "Should have been no error sharing a CKKSKey");

    XCTAssertEqual(self.tlk.uuid, share.tlkUUID, "TLK shares should know which key they hold");

    CKKSKey* unwrappedKey = [share unwrapUsing:self.remotePeer error:&error];
    XCTAssertNil(error, "Should have been no error unwrapping a CKKSKey");
    XCTAssertEqualObjects(self.tlk, unwrappedKey, "CKKSKeys should be identical after wrapping/unwrapping through a TLK Share record");
}

- (void)testTLKShareSignAndVerify {
    NSError* error = nil;
    CKKSTLKShareRecord* share = [CKKSTLKShareRecord share:self.tlk
                                           as:self.localPeer
                                           to:self.remotePeer
                                        epoch:-1
                                     poisoned:0
                                        error:&error];
    XCTAssertNil(error, "Should have been no error sharing a CKKSKey");

    NSData* signature = [share signRecord:self.localPeer.signingKey error:&error];
    XCTAssertNil(error, "Should have been no error signing a CKKSTLKShare");
    XCTAssertNotNil(signature, "Should have received a signature blob");

    XCTAssertTrue([share verifySignature:signature verifyingPeer:self.localPeer error:&error], "Signature should verify using the local peer's signing key");
}

- (void)testTLKShareSignAndFailVerify {
    NSError* error = nil;
    CKKSTLKShareRecord* share = [CKKSTLKShareRecord share:self.tlk
                                           as:self.localPeer
                                           to:self.remotePeer
                                        epoch:-1
                                     poisoned:0
                                        error:&error];
    XCTAssertNil(error, "Should have been no error sharing a CKKSKey");

    NSData* signature = [share signRecord:self.localPeer.signingKey error:&error];
    XCTAssertNil(error, "Should have been no error signing a CKKSTLKShare");
    XCTAssertNotNil(signature, "Should have received a signature blob");

    CKKSTLKShareRecord* shareEpoch = [share copy];
    XCTAssertTrue([shareEpoch verifySignature:signature verifyingPeer:self.localPeer error:&error], "Signature should verify using the local peer's signing key");
    error = nil;
    shareEpoch.share.epoch = 1;
    XCTAssertFalse([shareEpoch verifySignature:signature verifyingPeer:self.localPeer error:&error], "After epoch change, signature should no longer verify");
    XCTAssertNotNil(error, "Signature verification after epoch change should have produced an error");
    error = nil;

    CKKSTLKShareRecord* sharePoisoned = [share copy];
    XCTAssertTrue([sharePoisoned verifySignature:signature verifyingPeer:self.localPeer error:&error], "Signature should verify using the local peer's signing key");
    error = nil;
    sharePoisoned.share.poisoned = 1;
    XCTAssertFalse([sharePoisoned verifySignature:signature verifyingPeer:self.localPeer error:&error], "After poison change, signature should no longer verify");
    XCTAssertNotNil(error, "Signature verification after poison change should have produced an error");
    error = nil;

    CKKSTLKShareRecord* shareData = [share copy];
    XCTAssertTrue([shareData verifySignature:signature verifyingPeer:self.localPeer error:&error], "Signature should verify using the local peer's signing key");
    error = nil;
    shareData.share.wrappedTLK = [NSMutableData dataWithLength:shareData.wrappedTLK.length];
    XCTAssertFalse([shareData verifySignature:signature verifyingPeer:self.localPeer error:&error], "After data change, signature should no longer verify");
    XCTAssertNotNil(error, "Signature verification due to data change should have produced an error");
    error = nil;
}

- (void)testKeyShareAndRecover {
    NSError* error = nil;
    CKKSTLKShareRecord* share = [CKKSTLKShareRecord share:self.tlk
                                           as:self.localPeer
                                           to:self.remotePeer
                                        epoch:-1
                                     poisoned:0
                                        error:&error];
    XCTAssertNil(error, "Should have been no error sharing a CKKSKey");

    NSSet* peers = [NSSet setWithObject:self.localPeer];
    CKKSKey* key = [share recoverTLK:self.remotePeer trustedPeers:peers error:&error];
    XCTAssertNil(error, "Should have been no error unwrapping a CKKSKey");
    XCTAssertEqualObjects(self.tlk, key, "CKKSKeys should be identical after wrapping/unwrapping through a TLK Share record");
}

- (void)testKeyShareAndFailRecovery {
    NSError* error = nil;
    CKKSKey* key = nil;
    CKKSTLKShareRecord* share = [CKKSTLKShareRecord share:self.tlk
                                           as:self.localPeer
                                           to:self.remotePeer
                                        epoch:-1
                                     poisoned:0
                                        error:&error];
    XCTAssertNil(error, "Should have been no error sharing a CKKSKey");

    NSSet* peers = [NSSet setWithObject:self.localPeer];

    key = [share recoverTLK:self.remotePeer trustedPeers:[NSSet set] error:&error];
    XCTAssertNil(key, "No key should have been extracted when no trusted peers exist");
    XCTAssertNotNil(error, "Should have produced an error when failing to extract a key with no trusted peers");
    error = nil;

    key = [share recoverTLK:self.remotePeer2 trustedPeers:peers error:&error];
    XCTAssertNil(key, "No key should have been extracted when using the wrong key");
    XCTAssertNotNil(error, "Should have produced an error when failing to extract with the wrong key");
    error = nil;

    CKKSTLKShareRecord* shareSignature = [share copy];
    shareSignature.share.signature = [NSMutableData dataWithLength:shareSignature.signature.length];
    key = [shareSignature recoverTLK:self.remotePeer trustedPeers:peers error:&error];
    XCTAssertNil(key, "No key should have been extracted when signature fails to verify");
    XCTAssertNotNil(error, "Should have produced an error when failing to extract a key with an invalid signature");
    error = nil;

    CKKSTLKShareRecord* shareUUID = [share copy];
    shareUUID.share.tlkUUID = [[NSUUID UUID] UUIDString];
    key = [shareUUID recoverTLK:self.remotePeer trustedPeers:peers error:&error];
    XCTAssertNil(key, "No key should have been extracted when uuid has changed");
    XCTAssertNotNil(error, "Should have produced an error when failing to extract a key after uuid has changed");
    error = nil;
}

- (void)testKeyShareSaveAndLoad {
    NSError* error = nil;
    CKKSTLKShareRecord* share = [CKKSTLKShareRecord share:self.tlk
                                           as:self.localPeer
                                           to:self.remotePeer
                                        epoch:-1
                                     poisoned:0
                                        error:&error];
    XCTAssertNil(error, "Should have been no error sharing a CKKSKey");

    [share saveToDatabase:&error];
    XCTAssertNil(error, "Shouldn't be an error saving a TLKShare record to the database");

    CKKSTLKShareRecord* loadedShare = [CKKSTLKShareRecord fromDatabase:self.tlk.uuid
                                            receiverPeerID:self.remotePeer.peerID
                                              senderPeerID:self.localPeer.peerID
                                                    zoneID:self.tlk.zoneID
                                                     error:&error];
    XCTAssertNil(error, "Shouldn't get an error loading the share from the db");
    XCTAssertNotNil(loadedShare, "Should've gotten a TLK share object back from the database");

    XCTAssertEqualObjects(share, loadedShare, "Re-loaded TLK share object should be equivalent to the original");

    CKRecord* record = [share CKRecordWithZoneID:self.tlk.zoneID];
    XCTAssertNotNil(record, "Should be able to turn a share into a CKRecord");
    XCTAssertTrue([share matchesCKRecord: record], "Should be able to compare a CKRecord with a TLKShare");

    CKKSTLKShareRecord* fromCKRecord = [[CKKSTLKShareRecord alloc] initWithCKRecord:record];
    XCTAssertNotNil(fromCKRecord, "Should be able to turn a CKRecord into a TLK share");

    XCTAssertEqualObjects(share, fromCKRecord, "TLK shares sent through CloudKit should be identical");
}

- (void)testKeyShareSignExtraFieldsInCKRecord {
    NSError* error = nil;
    CKKSTLKShareRecord* share = [CKKSTLKShareRecord share:self.tlk
                                           as:self.localPeer
                                           to:self.remotePeer
                                        epoch:-1
                                     poisoned:0
                                        error:&error];
    XCTAssertNil(error, "Should have been no error sharing a CKKSKey");

    CKRecord* record =  [share CKRecordWithZoneID:self.tlk.zoneID];
    XCTAssertNotNil(record, "Should be able to turn a share into a CKRecord");
    XCTAssertTrue([share matchesCKRecord: record], "Should be able to compare a CKRecord with a TLKShare");

    // Add another few fields to the record
    record[@"extra_field"] = @"asdf";
    record[@"another_field"] = [NSNumber numberWithInt:5];
    record[@"data"] = [@"asdfdata" dataUsingEncoding:NSUTF8StringEncoding];
    CKKSTLKShareRecord* share2 = [share copy];
    share2.storedCKRecord = record;

    XCTAssertNotNil([share dataForSigning], "TLKShares should be able to produce some data to sign");
    XCTAssertNotNil([share2 dataForSigning], "TLKShares should be able to produce some data to sign (that includes extra fields)");
    XCTAssertNotEqualObjects([share dataForSigning], [share2 dataForSigning], "TLKShares should prepare to sign extra unknown data");

    share2.share.signature = [share2 signRecord:self.localPeer.signingKey error:&error];
    XCTAssertNil(error, "Shouldn't be an error signing a record with extra fields");
    XCTAssertNotEqualObjects(share.signature, share2.signature, "Signatures should be different for different data");

    XCTAssert([share2 verifySignature:share2.signature verifyingPeer:self.localPeer error:&error], "Signature with extra data should verify");

    // Now, change some of the extra data and see how that works
    CKRecord* changedDataRecord = [record copy];
    changedDataRecord[@"extra_field"] = @"no signature here";
    share2.storedCKRecord = changedDataRecord;
    XCTAssertFalse([share2 verifySignature:share2.signature verifyingPeer:self.localPeer error:&error], "Signature with extra data shouldn't verify if the data changes");

    CKRecord* addedDataRecord = [record copy];
    addedDataRecord[@"anotherfieldaltogether"] = @"asdfasdf";
    share2.storedCKRecord = addedDataRecord;
    XCTAssertFalse([share2 verifySignature:share2.signature verifyingPeer:self.localPeer error:&error], "Signature with extra data shouldn't verify if extra data is added");

    // And verify that saving to disk and reloading is successful
    share2.storedCKRecord = record;
    XCTAssert([share2 verifySignature:share2.signature verifyingPeer:self.localPeer error:&error], "Signature with extra data should verify");
    [share2 saveToDatabase:&error];
    XCTAssertNil(error, "No error saving share2 to database");

    CKKSTLKShareRecord* loadedShare2 = [CKKSTLKShareRecord tryFromDatabaseFromCKRecordID:record.recordID error:&error];
    XCTAssertNil(error, "No error loading loadedShare2 from database");
    XCTAssertNotNil(loadedShare2, "Should have received a CKKSTLKShare from the database");

    XCTAssert([loadedShare2 verifySignature:loadedShare2.signature verifyingPeer:self.localPeer error:&error], "Signature with extra data should verify after save/load");
}

@end

#endif // OCTAGON
