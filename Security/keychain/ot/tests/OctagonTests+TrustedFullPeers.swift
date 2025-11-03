/*
* Copyright (c) 2025 Apple Inc. All Rights Reserved.
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

import Foundation

#if OCTAGON

class OctagonTrustedFullPeersTests: OctagonTestsBase {
    func testTrustedFullPeerCountAPI() throws {
        try skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        self.assertResetAndBecomeTrustedInDefaultContext()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        var count: NSNumber?
        XCTAssertNoThrow(count = try OctagonTrustCliqueBridge.trustedFullPeers(self.otcliqueContext), "trustedFullPeers should not error")
        XCTAssertNotNil(count, "count should not be nil")
        XCTAssertEqual(count?.intValue, 1, "count should be 1")

        // until there's another peer around
        let joiningContext = self.makeInitiatorContext(contextID: "joiner", authKitAdapter: self.mockAuthKit2)
        self.assertJoinViaEscrowRecoveryFromDefaultContextWithReciprocationAndTLKShares(joiningContext: joiningContext)

        XCTAssertNoThrow(count = try OctagonTrustCliqueBridge.trustedFullPeers(self.otcliqueContext), "trustedFullPeers should not error")
        XCTAssertNotNil(count, "count should not be nil")
        XCTAssertEqual(count?.intValue, 2, "count should be 2")

        let secondJoiningContext = self.makeInitiatorContext(contextID: "second_joiner", authKitAdapter: self.mockAuthKit3)
        self.assertJoinViaEscrowRecoveryFromDefaultContextWithReciprocationAndTLKShares(joiningContext: secondJoiningContext)

        XCTAssertNoThrow(count = try OctagonTrustCliqueBridge.trustedFullPeers(self.otcliqueContext), "trustedFullPeers should not error")
        XCTAssertNotNil(count, "count should not be nil")
        XCTAssertEqual(count?.intValue, 3, "count should be 3")
    }

    func testTrustedFullPeerCountAPIWhileNotTrusted() throws {
        try skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        self.assertResetAndBecomeTrustedInDefaultContext()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        var count: NSNumber?
        XCTAssertNoThrow(count = try OctagonTrustCliqueBridge.trustedFullPeers(self.otcliqueContext), "trustedFullPeers should not error")
        XCTAssertNotNil(count, "count should not be nil")
        XCTAssertEqual(count?.intValue, 1, "count should be 1")

        // until there's another peer around
        let joiningContext = self.makeInitiatorContext(contextID: "joiner", authKitAdapter: self.mockAuthKit2)
        self.assertJoinViaEscrowRecoveryFromDefaultContextWithReciprocationAndTLKShares(joiningContext: joiningContext)

        XCTAssertNoThrow(count = try OctagonTrustCliqueBridge.trustedFullPeers(self.otcliqueContext), "trustedFullPeers should not error")
        XCTAssertNotNil(count, "count should not be nil")
        XCTAssertEqual(count?.intValue, 2, "count should be 2")

        let secondJoiningContext = self.makeInitiatorContext(contextID: "second_joiner", authKitAdapter: self.mockAuthKit3)
        self.assertJoinViaEscrowRecoveryFromDefaultContextWithReciprocationAndTLKShares(joiningContext: secondJoiningContext)

        XCTAssertNoThrow(count = try OctagonTrustCliqueBridge.trustedFullPeers(self.otcliqueContext), "trustedFullPeers should not error")
        XCTAssertNotNil(count, "count should not be nil")
        XCTAssertEqual(count?.intValue, 3, "count should be 3")

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        XCTAssertNoThrow(try clique.leave(), "Should not be an error departing clique")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertNoThrow(count = try OctagonTrustCliqueBridge.trustedFullPeers(self.otcliqueContext), "trustedFullPeers should not error")
        XCTAssertNotNil(count, "count should not be nil")
        XCTAssertEqual(count?.intValue, 2, "count should be 2")
    }

    func _testTrustedFullPeersWithDifferentModel(model: String, fullPeer: Bool) throws {
        SecCKKSSetTestSkipTLKShareHealing(true)
        try skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        self.assertResetAndBecomeTrustedInDefaultContext()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        var count: NSNumber?
        XCTAssertNoThrow(count = try OctagonTrustCliqueBridge.trustedFullPeers(self.otcliqueContext), "trustedFullPeers should not error")
        XCTAssertNotNil(count, "count should not be nil")
        XCTAssertEqual(count?.intValue, 1, "count should be 1")

        self.mockDeviceInfo = OTMockDeviceInfoAdapter(modelID: model,
                                                deviceName: "device-" + model,
                                                serialNumber: NSUUID().uuidString,
                                                osVersion: "17.17")
        let joiningContext = self.manager.context(forContainerName: OTCKContainerName,
                                             contextID: model,
                                             sosAdapter: self.mockSOSAdapter!,
                                             accountsAdapter: self.mockAuthKit2,
                                             authKitAdapter: self.mockAuthKit2,
                                             tooManyPeersAdapter: self.mockTooManyPeers,
                                             tapToRadarAdapter: self.mockTapToRadar,
                                             lockStateTracker: self.lockStateTracker,
                                             deviceInformationAdapter: self.mockDeviceInfo)
        self.assertJoinViaProximitySetup(joiningContext: joiningContext, sponsor: self.cuttlefishContext)
        try self.putSelfTLKSharesInCloudKit(context: joiningContext)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        XCTAssertNoThrow(count = try OctagonTrustCliqueBridge.totalTrustedPeers(self.otcliqueContext), "trustedFullPeers should not error")
        XCTAssertNotNil(count, "count should not be nil")
        XCTAssertEqual(count?.intValue, 2, "count should match")

        if fullPeer {
            XCTAssertNoThrow(count = try OctagonTrustCliqueBridge.trustedFullPeers(self.otcliqueContext), "trustedFullPeers should not error")
            XCTAssertNotNil(count, "count should not be nil")
            XCTAssertEqual(count?.intValue, 2, "count should be 2")
        } else {
            XCTAssertNoThrow(count = try OctagonTrustCliqueBridge.trustedFullPeers(self.otcliqueContext), "trustedFullPeers should not error")
            XCTAssertNotNil(count, "count should not be nil")
            XCTAssertEqual(count?.intValue, 1, "count should be 1")
        }
    }

    func testTrustedFullPeersWithDifferentPeersMac() throws {
        try self._testTrustedFullPeersWithDifferentModel(model: "Mac17", fullPeer: true)
    }

    func testTrustedFullPeersWithDifferentPeersIPhone() throws {
        try self._testTrustedFullPeersWithDifferentModel(model: "iPhone7", fullPeer: true)
    }

    func testTrustedFullPeersWithDifferentPeersIPad() throws {
        try self._testTrustedFullPeersWithDifferentModel(model: "iPad4", fullPeer: true)
    }

    func testTrustedFullPeersWithDifferentPeersIPod() throws {
        try self._testTrustedFullPeersWithDifferentModel(model: "iPod3", fullPeer: true)
    }

    func testTrustedFullPeersWithDifferentPeersWatch() throws {
        try self._testTrustedFullPeersWithDifferentModel(model: "Watch10", fullPeer: true)
    }

    func testTrustedFullPeersWithDifferentPeersRealityDevice() throws {
        try self._testTrustedFullPeersWithDifferentModel(model: "RealityDevice0", fullPeer: true)
    }

    func testTrustedFullPeersWithDifferentPeersTV() throws {
        try self._testTrustedFullPeersWithDifferentModel(model: "AppleTV4", fullPeer: false)
    }

    func testTrustedFullPeersWithDifferentPeersHomePod() throws {
        try self._testTrustedFullPeersWithDifferentModel(model: "AudioAccessory1", fullPeer: false)
    }

    func testTrustedFullPeersWithNoAccount() throws {
        let trustedFullPeersExpectation = self.expectation(description: "trustedFullPeers callback occurs")
        XCTAssertThrowsError(try OctagonTrustCliqueBridge.trustedFullPeers(self.otcliqueContext)) { error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error as NSError).domain, OctagonErrorDomain, "error domain should be OctagonErrorDomain")
            XCTAssertEqual((error as NSError).code, OctagonError.iCloudAccountStateUnknown.rawValue, "error code should be OctagonErrorNotSignedIn")
            trustedFullPeersExpectation.fulfill()
        }
        self.wait(for: [trustedFullPeersExpectation], timeout: 20)
        self.startCKAccountStatusMock()
    }

    func testTrustedFullPeersWithNoCred() throws {
        self.startCKAccountStatusMock()
        let fakeAccount = FakeCKAccountInfo()
        fakeAccount.accountStatus = .available
        fakeAccount.hasValidCredentials = false
        fakeAccount.accountPartition = .production
        let ckacctinfo = unsafeBitCast(fakeAccount, to: CKAccountInfo.self)
        self.cuttlefishContext.cloudkitAccountStateChange(nil, to: ckacctinfo)
        let trustedFullPeersExpectation = self.expectation(description: "trustedFullPeers callback occurs")
        XCTAssertThrowsError(try OctagonTrustCliqueBridge.trustedFullPeers(self.otcliqueContext)) { error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error as NSError).domain, OctagonErrorDomain, "wrong error domain")
            XCTAssertEqual((error as NSError).code, OctagonError.accountDoesNotHaveValidCreds.rawValue, "wrong error code")
            trustedFullPeersExpectation.fulfill()
        }
        self.wait(for: [trustedFullPeersExpectation], timeout: 20)
    }
}

#endif // OCTAGON
