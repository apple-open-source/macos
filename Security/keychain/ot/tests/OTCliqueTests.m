/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

#import <Foundation/Foundation.h>
#import "OTClique.h"
#import <XCTest/XCTest.h>
#import <utilities/SecCFWrappers.h>

@interface CliqueUnitTests : XCTestCase
@property (nonatomic, strong) OTClique* testClique;
@property (nonatomic, strong) OTConfigurationContext* testData;
@property (nonatomic, copy) NSString* testContextName;
@property (nonatomic, copy) NSString* testDSID;
@property (nonatomic, copy) NSString* testAltDSID;
@property (nonatomic, strong) SFSignInAnalytics *analytics;
@property (nonatomic, strong)  XCTestExpectation *spiBlockExpectation;

@end

@implementation CliqueUnitTests

- (void)setUp
{
    NSError *error = NULL;

    [super setUp];
    self.continueAfterFailure = NO;
    _testDSID = @"123456789";
    _testContextName = @"contextName";
    _testAltDSID = @"testAltDSID";
    _testData = [[OTConfigurationContext alloc]init];
    _testData.context = _testContextName;
    _testData.dsid = _testDSID;
    _testData.altDSID = _testAltDSID;
    _testData.analytics = _analytics;

    _analytics = [[SFSignInAnalytics alloc]initWithSignInUUID:[NSUUID UUID].UUIDString category:@"com.apple.cdp" eventName:@"signed in"];
    XCTAssertNotNil(_analytics, "sign in analytics object should not be nil");

    _testClique = [[OTClique alloc]initWithContextData:_testData error:&error];
    XCTAssertNotNil(_testClique, "clique should not be nil: %@", error);
}

- (void)tearDown
{
    _analytics = nil;
    _testClique = nil;

    [super tearDown];
}


-(void) testCliqueStatus
{
    NSError *error = NULL;
    CliqueStatus clique = [_testClique fetchCliqueStatus:&error];
    XCTAssertTrue(clique == CliqueStatusIn || clique == CliqueStatusError, "circle status should be in circle");
    XCTAssertTrue(error == nil || [error.domain isEqualToString:(__bridge NSString*)kSOSErrorDomain], "error should be nil");
}

-(void) testCachedCliqueStatus
{
    NSError *error = NULL;
#pragma clang push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    CliqueStatus clique = [_testClique cachedCliqueStatus:YES error:&error];
#pragma clang pop
    XCTAssertTrue(clique == CliqueStatusIn || clique == CliqueStatusError, "circle status should be in circle");
    XCTAssertTrue(error == nil || [error.domain isEqualToString:(__bridge NSString*)kSOSErrorDomain], "error should be nil");
}

- (void) testMakeNewFriends
{
    NSError *error = NULL;
    OTConfigurationContext* newData = [[OTConfigurationContext alloc]init];
    newData.context = _testContextName;
    newData.dsid = _testDSID;
    newData.altDSID = _testAltDSID;
    newData.analytics = _analytics;
    
    OTClique* clique = [OTClique newFriendsWithContextData:newData error:&error];
    if(error){
        XCTAssertTrue([error.domain isEqualToString:(__bridge NSString*)kSOSErrorDomain] || [error.domain isEqualToString:NSMachErrorDomain], "error should be kern return error");
        XCTAssertNil(clique, "clique should be nil");
    }
    else{
        XCTAssertNotNil(clique, "new clique should be nil");
        XCTAssertNil(error, "error should be nil");
    }
}

- (void) testRemoveFriendFromClique
{
    NSError *error = NULL;
    CFErrorRef validPeerError = NULL;
    CFArrayRef peerList = SOSCCCopyValidPeerPeerInfo(&validPeerError);
    if(validPeerError){
        BOOL result = [_testClique removeFriendsInClique:@[@""] error:&error];
        XCTAssertTrue([error.domain isEqualToString:(__bridge NSString*)kSOSErrorDomain] || [error.domain isEqualToString:NSMachErrorDomain], "error should be kern return error");
        XCTAssertFalse(result, "should have returned NO, attempting to remove friends when not valid in the circle");
    }else{
        BOOL result = [_testClique removeFriendsInClique:@[@""] error:&error];
        XCTAssertFalse(result, "should have returned NO, we passed an empty list");
    }
    CFReleaseNull(peerList);
}

- (void) testLeaveClique
{
    NSError *error = NULL;
    BOOL result = [_testClique leaveClique:&error];
    if(error){
        XCTAssertTrue([error.domain isEqualToString:(__bridge NSString*)kSOSErrorDomain] || [error.domain isEqualToString:NSMachErrorDomain], "error should be kern return error");
        XCTAssertFalse(result, "result should be NO");
    }else{
        XCTAssertNil(error, "error should be nil");
    }
}

- (void) testListFriendIDs
{
    NSError *error = NULL;
    NSDictionary<NSString *,NSString *> *friends = [_testClique peerDeviceNamesByPeerID:&error];
    if(error){
        XCTAssertEqual([friends count], 0, "friends should be nil");
        XCTAssertTrue([error.domain isEqualToString:(__bridge NSString*)kSOSErrorDomain], "error should be from sos");
    }else{
        XCTAssertNotNil(friends, "friends should not be nil");
    }
}

- (void) testWaitForInitialSync
{
    NSError *error = NULL;
    BOOL result = [_testClique waitForInitialSync:&error];
    if(error){
        XCTAssertFalse(result, "result should be NO");
        XCTAssertTrue([error.domain isEqualToString:(__bridge NSString*)kSOSErrorDomain], "error should be from sos");
    }else{
        XCTAssertTrue(result, "wait for initial sync should succeed");
    }
}

- (void) testCopyViewUnawarePeerInfo
{
    NSError *error = NULL;
    NSArray* result = [_testClique copyViewUnawarePeerInfo:&error];
    if(error){
        XCTAssertNil(result, "result should be nil");
        XCTAssertTrue([error.domain isEqualToString:(__bridge NSString*)kSOSErrorDomain], "error should be from sos");
    }else{
        XCTAssertNotNil(result, "copy view unaware peer info should return an array of peer infos");
    }
}

- (void) testSetUserCredentialsAndDSID
{
    NSError *error = NULL;

    BOOL result = [_testClique setUserCredentialsAndDSID:[NSString string] password:[NSData data] error:&error];
    if(error) {
        XCTAssertFalse(result, "result should be NO");
        XCTAssertTrue([error.domain isEqualToString:(__bridge NSString*)kSOSErrorDomain] || [error.domain isEqualToString:@"SyncedDefaults"] , "error should be from sos or KVS");
    }else{
        XCTAssertTrue(result, "result should be YES");
    }
}

- (void) testTryUserCredentialsAndDSID
{
    NSError *error = NULL;

    BOOL result = [_testClique tryUserCredentialsAndDSID:[NSString string] password:[NSData data] error:&error];
    if(error){
        XCTAssertFalse(result, "result should be NO");
        XCTAssertTrue([error.domain isEqualToString:(__bridge NSString*)kSOSErrorDomain], "error should be from sos");
    }else{
        XCTAssertTrue(result, "result should be YES");
    }
}

- (void) testCopyPeerPeerInfo
{
    NSError *error = NULL;

    NSArray* result = [_testClique copyPeerPeerInfo:&error];
    if(error){
        XCTAssertNil(result, "result should be nil");
        XCTAssertTrue([error.domain isEqualToString:(__bridge NSString*)kSOSErrorDomain], "error should be from sos");
    }else{
        XCTAssertNotNil(result, "result should not be nil");
    }
}

- (void) testPeersHaveViewsEnabled
{
    NSError *error = NULL;

    BOOL result = [_testClique peersHaveViewsEnabled:[NSArray array] error:&error];
    if(error){
        XCTAssertTrue([error.domain isEqualToString:(__bridge NSString*)kSOSErrorDomain], "error should be from sos");
    }
    XCTAssertFalse(result, "result should be NO");
}
- (void) testRequestToJoinCircle
{
    NSError *error = NULL;

    BOOL result = [_testClique requestToJoinCircle:&error];
    if(error){
        XCTAssertFalse(result, "result should be NO");
        XCTAssertTrue([error.domain isEqualToString:(__bridge NSString*)kSOSErrorDomain], "error should be from sos");
    }else{
        XCTAssertTrue(result, "result should be YES");
    }
}

@end
