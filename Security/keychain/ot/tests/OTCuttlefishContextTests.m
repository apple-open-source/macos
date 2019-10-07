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

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>
#import <KeychainCircle/KeychainCircle.h>
#import "OTTestsBase.h"
#import "OTSOSAdapter.h"

@interface OTCuttlefishContextTests : OTTestsBase
@end

@implementation OTCuttlefishContextTests

- (void)setUp
{
    [super setUp];
}

- (void)tearDown
{
    [super tearDown];
}

- (void)testStateMachineInitialization {
    XCTAssertEqual(0, [self.cuttlefishContext.stateConditions[OctagonStateMachineNotStarted] wait:10*NSEC_PER_SEC], "State machine should enter 'not started'");

    [self.cuttlefishContext startOctagonStateMachine];

    XCTAssertEqual(0, [self.cuttlefishContext.stateConditions[OctagonStateInitializing] wait:10*NSEC_PER_SEC], "State machine should enter 'initializing'");

    XCTAssertEqual(0, [self.cuttlefishContext.stateConditions[OctagonStateUntrusted] wait:10*NSEC_PER_SEC], "State machine should enter 'signedout'");
}

- (void)testPrepare {
    [self.cuttlefishContext startOctagonStateMachine];

    XCTAssertEqual(0, [self.cuttlefishContext.stateConditions[OctagonStateUntrusted] wait:10*NSEC_PER_SEC], "State machine should enter 'signedout'");

    XCTestExpectation* rpcCallbackOccurs = [self expectationWithDescription:@"rpcPrepare callback occurs"];
    [self.cuttlefishContext prepareForApplicant:0 reply:^(NSString * _Nullable peerID, NSData * _Nullable permanentInfo, NSData * _Nullable permanentInfoSig, NSData * _Nullable stableInfo, NSData * _Nullable stableInfoSig, NSError * _Nullable error) {
        XCTAssertNil(error, "Should be no error calling 'prepare'");

        XCTAssertNotNil(peerID, "Prepare should have returned a peerID");
        XCTAssertNotNil(permanentInfo, "Prepare should have returned a permanentInfo");
        XCTAssertNotNil(permanentInfoSig, "Prepare should have returned a permanentInfoSig");
        XCTAssertNotNil(stableInfo, "Prepare should have returned a stableInfo");
        XCTAssertNotNil(stableInfoSig, "Prepare should have returned a stableInfoSig");

        [rpcCallbackOccurs fulfill];
    }];

    [self waitForExpectations:@[rpcCallbackOccurs] timeout:10];

}

- (void)testPrepareTimeoutIfStateMachineUnstarted {

    XCTestExpectation* rpcCallbackOccurs = [self expectationWithDescription:@"rpcPrepare callback occurs"];
    [self.cuttlefishContext prepareForApplicant:0 reply:^(NSString * _Nullable peerID, NSData * _Nullable permanentInfo, NSData * _Nullable permanentInfoSig, NSData * _Nullable stableInfo, NSData * _Nullable stableInfoSig, NSError * _Nullable error) {
        XCTAssertNotNil(error, "Should be an error calling 'prepare'");
        XCTAssertEqualObjects(error.domain, CKKSResultErrorDomain, "Error domain should be CKKSResultErrorDomain");
        XCTAssertEqual(error.code, CKKSResultTimedOut, "Error result should be CKKSResultTimedOut");


        XCTAssertNil(peerID, "Prepare should not have returned a peerID");
        XCTAssertNil(permanentInfo, "Prepare should not have returned a permanentInfo");
        XCTAssertNil(permanentInfoSig, "Prepare should not have returned a permanentInfoSig");
        XCTAssertNil(stableInfo, "Prepare should not have returned a stableInfo");
        XCTAssertNil(stableInfoSig, "Prepare should not have returned a stableInfoSig");

        [rpcCallbackOccurs fulfill];
    }];

    [self waitForExpectations:@[rpcCallbackOccurs] timeout:10];
}

@end
#endif
