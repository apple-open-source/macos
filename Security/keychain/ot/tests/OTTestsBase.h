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

#ifndef OTTestsBase_h
#define OTTestsBase_h

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>

#import "keychain/ot/OTContext.h"
#import "keychain/ot/OTEscrowKeys.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/ot/OTControl.h"
#import "keychain/ot/OTManager.h"
#import "SFPublicKey+SPKI.h"
#import <Security/SecKey.h>

#import <SecurityFoundation/SFKey.h>
#import <SecurityFoundation/SFKey_Private.h>

#import "keychain/ckks/tests/CloudKitKeychainSyncingTestsBase.h"
#import "keychain/ckks/tests/CloudKitMockXCTest.h"
#import "keychain/ckks/tests/MockCloudKit.h"
#import "keychain/ckks/tests/CKKSTests.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSViewManager.h"

NS_ASSUME_NONNULL_BEGIN

@interface OTTestsBase : CloudKitKeychainSyncingTestsBase <OTContextIdentityProvider>
@property id otControl;
@property OTManager* manager;
@property (nonatomic, strong) OTCloudStore*         cloudStore;
@property (nonatomic, strong) OTLocalStore*         localStore;
@property (nonatomic, strong) FakeCKZone*           otFakeZone;
@property (nonatomic, strong) CKRecordZoneID*       otZoneID;
@property (nonatomic, strong) OTContext*            context;
@property (nonatomic, strong) _SFECKeyPair*         peerSigningKey;
@property (nonatomic, strong) _SFECKeyPair*         peerEncryptionKey;
@property (nonatomic, strong) NSData*               secret;
@property (nonatomic, strong) NSString* recordName;
@property (nonatomic, strong) NSString* egoPeerID;
@property (nonatomic, strong) NSString* sosPeerID;
@property (nonatomic, strong) OTEscrowKeys* escrowKeys;

@property (nonatomic, strong) FakeCKZone* rampZone;
@property (nonatomic, strong) CKRecord *enrollRampRecord;
@property (nonatomic, strong) CKRecord *restoreRampRecord;
@property (nonatomic, strong) CKRecord *cfuRampRecord;

@property (nonatomic, strong) OTRamp *enroll;
@property (nonatomic, strong) OTRamp *restore;
@property (nonatomic, strong) OTRamp *cfu;
@property (nonatomic, strong) CKKSNearFutureScheduler* scheduler;
@property (nonatomic, strong) XCTestExpectation *expectation;
@property (nonatomic, strong) XCTestExpectation *spiBlockExpectation;

@property (nonatomic, strong) CKRecordZoneID* rampZoneID;

-(OTRamp*) fakeRamp:(NSString*)recordName featureName:(NSString*)featureName;

-(void)expectAddedCKModifyRecords:(NSDictionary<NSString*, NSNumber*>*)records holdFetch:(BOOL)shouldHoldTheFetch;
-(void)expectDeletedCKModifyRecords:(NSDictionary<NSString*, NSNumber*>*)records holdFetch:(BOOL)shouldHoldTheFetch;
-(void) setUpRampRecordsInCloudKitWithFeatureOn;
-(void) setUpRampRecordsInCloudKitWithFeatureOff;

@end
NS_ASSUME_NONNULL_END

#endif /* OTTestsBase_h */
#endif /* OCTAGON */
