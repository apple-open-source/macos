#if OCTAGON

@objcMembers
class OctagonRerollTests: OctagonTestsBase {
    override func setUp() {
        // Please don't make the SOS API calls, no matter what
        OctagonSetSOSFeatureEnabled(false)

        SetRollOctagonIdentityEnabled(true)
        super.setUp()
    }

    override func tearDown() {
        OctagonSetSOSFeatureEnabled(false)

        ClearRollOctagonIdentityEnabledOverride()
        super.tearDown()
    }

    func testReroll() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)

        let oldPeerID = self.fetchEgoPeerID()
        print("oldPeerID = \(oldPeerID)")

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)

        SecCKKSSetTestSkipTLKShareHealing(true)

        let rerollExpectation = self.expectation(description: "rerollExpectation returns")
        self.manager.reroll(OTControlArguments(configuration: self.otcliqueContext)) { error in
            XCTAssertNil(error, "error should be nil")
            rerollExpectation.fulfill()
        }
        self.wait(for: [rerollExpectation], timeout: 10)

        let newPeerID = self.fetchEgoPeerID()
        print("newPeerID = \(newPeerID)")
        XCTAssertNotEqual(oldPeerID, newPeerID, "peerID should be new")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: newPeerID, opinion: .trusts, target: newPeerID)),
                      "new Peer ID should trust itself")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: oldPeerID, opinion: .trusts, target: oldPeerID)),
                      "old Peer ID should trust itself")

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: newPeerID, opinion: .trusts, target: oldPeerID)),
                      "new Peer ID should exclude old Peer ID")
    }

    func testRerollFetchError() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)

        let oldPeerID = self.fetchEgoPeerID()
        print("oldPeerID = \(oldPeerID)")

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)

        SecCKKSSetTestSkipTLKShareHealing(true)

        let ckError = NSError(domain: AKAppleIDAuthenticationErrorDomain,
                              code: 17,
                              userInfo: [NSLocalizedDescriptionKey: "The Internet connection appears to be offline."])

        self.fakeCuttlefishServer.nextFetchErrors.append(ckError)

        let rerollExpectation = self.expectation(description: "rerollExpectation returns")
        self.manager.reroll(OTControlArguments(configuration: self.otcliqueContext)) { error in
            XCTAssertNil(error, "error should be nil")
            rerollExpectation.fulfill()
        }
        self.wait(for: [rerollExpectation], timeout: 10)

        let newPeerID = self.fetchEgoPeerID()
        print("newPeerID = \(newPeerID)")
        XCTAssertNotEqual(oldPeerID, newPeerID, "peerID should be new")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: newPeerID, opinion: .trusts, target: newPeerID)),
                      "new Peer ID should trust itself")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: oldPeerID, opinion: .trusts, target: oldPeerID)),
                      "old Peer ID should trust itself")

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: newPeerID, opinion: .trusts, target: oldPeerID)),
                      "new Peer ID should exclude old Peer ID")
    }

    func testRerollJoinError() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)

        let oldPeerID = self.fetchEgoPeerID()
        print("oldPeerID = \(oldPeerID)")

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)

        SecCKKSSetTestSkipTLKShareHealing(true)

        let joinExpectation = self.expectation(description: "joinExpectation")
        self.fakeCuttlefishServer.joinListener = { [unowned self] _ in
            self.fakeCuttlefishServer.joinListener = nil
            joinExpectation.fulfill()

            return FakeCuttlefishServer.makeCloudKitCuttlefishError(code: .joinFailed)
        }

        let rerollExpectation = self.expectation(description: "rerollExpectation returns")
        self.manager.reroll(OTControlArguments(configuration: self.otcliqueContext)) { error in
            XCTAssertNotNil(error, "error should not be nil")
            rerollExpectation.fulfill()
        }
        self.wait(for: [joinExpectation], timeout: 10)
        self.wait(for: [rerollExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
    }

    func testRerollWithRecoveryKey() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)

        let recoveryKey = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        let createKeyExpectation = self.expectation(description: "createKeyExpectation returns")
        self.manager.createRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), recoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            createKeyExpectation.fulfill()
        }
        self.wait(for: [createKeyExpectation], timeout: 10)

        let oldPeerID = self.fetchEgoPeerID()
        print("oldPeerID = \(oldPeerID)")

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)

        SecCKKSSetTestSkipTLKShareHealing(true)

        let rerollExpectation = self.expectation(description: "rerollExpectation returns")
        self.manager.reroll(OTControlArguments(configuration: self.otcliqueContext)) { error in
            XCTAssertNil(error, "error should be nil")
            rerollExpectation.fulfill()
        }
        self.wait(for: [rerollExpectation], timeout: 10)

        let newPeerID = self.fetchEgoPeerID()
        print("newPeerID = \(newPeerID)")
        XCTAssertNotEqual(oldPeerID, newPeerID, "peerID should be new")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)

        let stableInfoAcceptorCheckDumpCallback = self.expectation(description: "stableInfoAcceptorCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            stableInfoAcceptorCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoAcceptorCheckDumpCallback], timeout: 10)

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        let stableInfoAcceptorCheckDumpCallback2 = self.expectation(description: "stableInfoAcceptorCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 1, "should be 1 peer id")

            stableInfoAcceptorCheckDumpCallback2.fulfill()
        }
        self.wait(for: [stableInfoAcceptorCheckDumpCallback2], timeout: 10)

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: newPeerID, opinion: .trusts, target: newPeerID)),
                      "new Peer ID should trust itself")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: oldPeerID, opinion: .trusts, target: oldPeerID)),
                      "old Peer ID should trust itself")

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: newPeerID, opinion: .excludes, target: oldPeerID)),
                      "new Peer ID should exclude old Peer ID")
    }

    func testRerollBasedonMachineID() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)

        SecCKKSSetTestSkipTLKShareHealing(true)

        let healthCheckCallback1 = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(OTControlArguments(configuration: self.otcliqueContext),
                                 skipRateLimitingCheck: false,
                                 repair: false,
                                 danglingPeerCleanup: false,
                                 updateIdMS: false) { response, error in
            XCTAssertNotNil(response, "response should not be nil")
            XCTAssertNil(error, "error should be nil")
            healthCheckCallback1.fulfill()
        }
        self.wait(for: [healthCheckCallback1], timeout: 10)

        var peerID1: String?
        var machineID1: String?

        let dumpCallback1 = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "self should not be nil")
            peerID1 = (egoSelf!["peerID"] as? String)!
            let permanentInfo = egoSelf!["permanentInfo"] as? [String: AnyObject]
            XCTAssertNotNil(permanentInfo, "permanentInfo should not be nil")
            machineID1 = (permanentInfo!["machine_id"] as? String)!
            dumpCallback1.fulfill()
        }
        self.wait(for: [dumpCallback1], timeout: 10)
        XCTAssertEqual(machineID1, "MACHINE1")

        self.mockAuthKit.currentMachineID = "MACHINE1.1"

        let healthCheckCallback2 = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(OTControlArguments(configuration: self.otcliqueContext),
                                 skipRateLimitingCheck: true,
                                 repair: false,
                                 danglingPeerCleanup: false,
                                 updateIdMS: false) { response, error in
            XCTAssertNotNil(response, "response should not be nil")
            XCTAssertNil(error, "error should be nil")
            healthCheckCallback2.fulfill()
        }
        self.wait(for: [healthCheckCallback2], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        var peerID2: String?
        var machineID2: String?
        let dumpCallback2 = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "self should not be nil")
            peerID2 = (egoSelf!["peerID"] as? String)!
            let permanentInfo = egoSelf!["permanentInfo"] as? [String: AnyObject]
            XCTAssertNotNil(permanentInfo, "permanentInfo should not be nil")
            machineID2 = (permanentInfo!["machine_id"] as? String)!
            dumpCallback2.fulfill()
        }
        self.wait(for: [dumpCallback2], timeout: 10)

        XCTAssertEqual(machineID2, "MACHINE1.1")
        XCTAssertNotEqual(machineID1, machineID2, "machine ID should have changed")
        XCTAssertNotEqual(peerID1, peerID2, "peer ID should have changed")
    }
}
#endif
