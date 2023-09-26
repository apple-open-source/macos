#if OCTAGON

import Security_Private.SecPasswordGenerate

extension Container {
    func removeRKFromContainer() {
        self.moc.performAndWait {
            self.containerMO.recoveryKeySigningSPKI = nil
            self.containerMO.recoveryKeyEncryptionSPKI = nil

            try! self.moc.save()
        }
    }
}

@objcMembers
class OctagonRecoveryKeyTests: OctagonTestsBase {
    override func setUp() {
        // Please don't make the SOS API calls, no matter what
        OctagonSetSOSFeatureEnabled(false)

        super.setUp()
    }

    override func tearDown() {
        OctagonSetSOSFeatureEnabled(false)

        super.tearDown()
    }

    func testSetRecoveryKey() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(self.mockAuthKit.currentDeviceList().isEmpty, "should not have zero devices")

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        let entropy = try self.loadSecret(label: clique.cliqueMemberIdentifier!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)

        let recoveryKey = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")
        OctagonSetSOSFeatureEnabled(true)

        let createKeyExpectation = self.expectation(description: "createKeyExpectation returns")
        self.manager.createRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), recoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            createKeyExpectation.fulfill()
        }
        self.wait(for: [createKeyExpectation], timeout: 10)
    }

    func testSetOTCliqueRecoveryKey() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(self.mockAuthKit.currentDeviceList().isEmpty, "should not have zero devices")

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        let entropy = try self.loadSecret(label: clique.cliqueMemberIdentifier!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)

        let recoveryKey = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")
        OctagonSetSOSFeatureEnabled(true)

        XCTAssertTrue(OctagonTrustCliqueBridge.setRecoveryKeyWith(self.otcliqueContext, recoveryKey: recoveryKey, error: nil), "should return true")
    }

    func testSetOTCliqueMalformedRecoveryKey() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(self.mockAuthKit.currentDeviceList().isEmpty, "should not have zero devices")

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        let entropy = try self.loadSecret(label: clique.cliqueMemberIdentifier!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)

        let recoveryKey = "I'm a malformed recovery key"
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        XCTAssertFalse(OctagonTrustCliqueBridge.setRecoveryKeyWith(self.otcliqueContext, recoveryKey: recoveryKey, error: nil), "should return false")
    }

    func testSetRecoveryKeyPeerReaction() throws {
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
        self.verifyDatabaseMocks()

        let recoveryKey = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        let createKeyExpectation = self.expectation(description: "createKeyExpectation returns")
        self.manager.createRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), recoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            createKeyExpectation.fulfill()
        }
        self.wait(for: [createKeyExpectation], timeout: 10)

        let entropy = try self.loadSecret(label: clique.cliqueMemberIdentifier!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        XCTAssertTrue(try self.recoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.primaryAltDSID())))
        self.verifyDatabaseMocks()

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        let initiatorContextID = "new guy"

        let initiatorContext = self.makeInitiatorContext(contextID: initiatorContextID, authKitAdapter: self.mockAuthKit2)

        initiatorContext.startOctagonStateMachine()
        self.sendContainerChange(context: initiatorContext)

        let joinWithBottleExpectation = self.expectation(description: "joinWithBottle callback occurs")
        initiatorContext.join(withBottle: bottle.bottleID, entropy: entropy!, bottleSalt: self.otcliqueContext.altDSID!) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithBottleExpectation.fulfill()
        }
        self.wait(for: [joinWithBottleExpectation], timeout: 10)
        self.verifyDatabaseMocks()

        self.sendContainerChangeWaitForFetch(context: initiatorContext)

        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        let stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(initiatorContext.activeAccount)) { dump, _ in
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

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

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
        self.verifyDatabaseMocks()
    }

    func testSetRecoveryKey3PeerReaction() throws {
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
        self.verifyDatabaseMocks()

        let recoveryKey = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        let createKeyExpectation = self.expectation(description: "createKeyExpectation returns")
        self.manager.createRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), recoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            createKeyExpectation.fulfill()
        }
        self.wait(for: [createKeyExpectation], timeout: 10)

        let entropy = try self.loadSecret(label: clique.cliqueMemberIdentifier!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        XCTAssertTrue(try self.recoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.primaryAltDSID())))

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        let initiatorContextID = "new guy"
        let initiatorContext = self.makeInitiatorContext(contextID: initiatorContextID)

        initiatorContext.startOctagonStateMachine()

        self.sendContainerChange(context: initiatorContext)

        self.assertEnters(context: initiatorContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let joinWithBottleExpectation = self.expectation(description: "joinWithBottle callback occurs")
        initiatorContext.join(withBottle: bottle.bottleID, entropy: entropy!, bottleSalt: self.otcliqueContext.altDSID!) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithBottleExpectation.fulfill()
        }
        self.wait(for: [joinWithBottleExpectation], timeout: 10)

        self.verifyDatabaseMocks()

        self.sendContainerChangeWaitForFetch(context: initiatorContext)

        // The first peer will upload TLKs for the new peer
        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        let stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(initiatorContext.activeAccount)) { dump, _ in
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

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

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

        let thirdPeerContextID = "3rd guy"
        let thirdPeerContext = self.makeInitiatorContext(contextID: thirdPeerContextID, authKitAdapter: self.mockAuthKit3)

        thirdPeerContext.startOctagonStateMachine()

        self.sendContainerChange(context: thirdPeerContext)
        let thirdPeerJoinWithBottleExpectation = self.expectation(description: "thirdPeerJoinWithBottleExpectation callback occurs")
        thirdPeerContext.join(withBottle: bottle.bottleID, entropy: entropy!, bottleSalt: self.otcliqueContext.altDSID!) { error in
            XCTAssertNil(error, "error should be nil")
            thirdPeerJoinWithBottleExpectation.fulfill()
        }
        self.wait(for: [thirdPeerJoinWithBottleExpectation], timeout: 10)

        self.verifyDatabaseMocks()

        self.sendContainerChangeWaitForFetch(context: thirdPeerContext)
        let thirdPeerStableInfoCheckDumpCallback = self.expectation(description: "thirdPeerStableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(thirdPeerContext.activeAccount)) { dump, _ in
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
            XCTAssertEqual(included!.count, 3, "should be 3 peer ids")

            thirdPeerStableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [thirdPeerStableInfoCheckDumpCallback], timeout: 10)

        // And ensure that the original peer uploads shares for the third as well
        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func createEstablishContext(contextID: String) -> OTCuttlefishContext {
        return self.manager.context(forContainerName: OTCKContainerName,
                                    contextID: contextID,
                                    sosAdapter: self.mockSOSAdapter!,
                                    accountsAdapter: self.mockAuthKit2,
                                    authKitAdapter: self.mockAuthKit2,
                                    tooManyPeersAdapter: self.mockTooManyPeers,
                                    tapToRadarAdapter: self.mockTapToRadar,
                                    lockStateTracker: self.lockStateTracker,
                                    deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-RK-iphone", serialNumber: "456", osVersion: "iOS (fake version)"))
    }

    func testJoinWithRecoveryKey() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        let establishedPeerID = self.fetchEgoPeerID(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        let recoveryKey = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        OctagonSetSOSFeatureEnabled(true)

        let createRecoveryExpectation = self.expectation(description: "createRecoveryExpectation returns")
        self.manager.createRecoveryKey(self.otcontrolArgumentsFor(context: establishContext), recoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            createRecoveryExpectation.fulfill()
        }
        self.wait(for: [createRecoveryExpectation], timeout: 10)

        try self.putRecoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))
        self.sendContainerChangeWaitForFetch(context: establishContext)

        // Now, join from a new device
        let recoveryContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        recoveryContext.startOctagonStateMachine()
        self.assertEnters(context: recoveryContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: recoveryContext)

        SecCKKSSetTestSkipTLKShareHealing(true)

        let joinWithRecoveryKeyExpectation = self.expectation(description: "joinWithRecoveryKey callback occurs")
        recoveryContext.join(withRecoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithRecoveryKeyExpectation], timeout: 20)

        let joinedPeerID = self.fetchEgoPeerID(context: recoveryContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.sendContainerChangeWaitForFetch(context: recoveryContext)

        let stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
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
            let vouchers = dump!["vouchers"]
            XCTAssertNotNil(vouchers, "vouchers should not be nil")
            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        self.sendContainerChangeWaitForFetch(context: establishContext)

        let stableInfoAcceptorCheckDumpCallback = self.expectation(description: "stableInfoAcceptorCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(establishContext.activeAccount)) { dump, _ in
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
            let vouchers = dump!["vouchers"]
            XCTAssertNotNil(vouchers, "vouchers should not be nil")
            stableInfoAcceptorCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoAcceptorCheckDumpCallback], timeout: 10)

        // And check the current state of the world
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: joinedPeerID, opinion: .trusts, target: joinedPeerID)),
                       "joined peer should trust itself")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: joinedPeerID, opinion: .trusts, target: establishedPeerID)),
                      "joined peer should trust establish peer")

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: establishedPeerID, opinion: .trusts, target: establishedPeerID)),
                       "establish peer should trust itself")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: establishedPeerID, opinion: .trusts, target: joinedPeerID)),
                      "establish peer should trust joined peer")

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertSelfTLKSharesInCloudKit(context: recoveryContext)
    }

    func testJoinWithRecoveryKeyWithCKKSConflict() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        let recoveryKey = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        OctagonSetSOSFeatureEnabled(true)

        let createRecoveryExpectation = self.expectation(description: "createRecoveryExpectation returns")
        self.manager.createRecoveryKey(self.otcontrolArgumentsFor(context: establishContext), recoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            createRecoveryExpectation.fulfill()
        }
        self.wait(for: [createRecoveryExpectation], timeout: 10)

        self.sendContainerChangeWaitForFetch(context: establishContext)

        self.silentFetchesAllowed = false
        self.expectCKFetchAndRun {
            self.putFakeKeyHierarchiesInCloudKit()
            self.putFakeDeviceStatusesInCloudKit()
            self.silentFetchesAllowed = true
        }
        let recoveryContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        recoveryContext.startOctagonStateMachine()
        self.assertEnters(context: recoveryContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: recoveryContext)

        SecCKKSSetTestSkipTLKShareHealing(true)

        let joinWithRecoveryKeyExpectation = self.expectation(description: "joinWithRecoveryKey callback occurs")
        recoveryContext.join(withRecoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithRecoveryKeyExpectation], timeout: 20)

        self.assertConsidersSelfTrusted(context: recoveryContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLK, within: 10 * NSEC_PER_SEC)
    }

    func testOTCliqueSettingRecoveryKey() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

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

        let recoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        let setRecoveryKeyExpectation = self.expectation(description: "setRecoveryKeyExpectation callback occurs")
        TestsObjectiveC.setNewRecoveryKeyWithData(self.otcliqueContext, recoveryKey: recoveryKey!) { rk, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNotNil(rk, "rk should not be nil")
            setRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [setRecoveryKeyExpectation], timeout: 10)
    }

    func testOTCliqueSet2ndRecoveryKey() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

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

        let recoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        let setRecoveryKeyExpectation = self.expectation(description: "setRecoveryKeyExpectation callback occurs")
        TestsObjectiveC.setNewRecoveryKeyWithData(self.otcliqueContext, recoveryKey: recoveryKey!) { rk, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNotNil(rk, "rk should not be nil")
            setRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [setRecoveryKeyExpectation], timeout: 10)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)

        let recoveryKey2 = SecRKCreateRecoveryKeyString(nil)

        let setRecoveryKeyExpectationAgain = self.expectation(description: "setRecoveryKeyExpectationAgain callback occurs")
        TestsObjectiveC.setNewRecoveryKeyWithData(self.otcliqueContext, recoveryKey: recoveryKey2!) { rk, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNotNil(rk, "rk should not be nil")
            setRecoveryKeyExpectationAgain.fulfill()
        }
        self.wait(for: [setRecoveryKeyExpectationAgain], timeout: 10)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testRKReplacement() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        let initiatorContextID = "initiator-context-id"
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

        self.verifyDatabaseMocks()

        let entropy = try self.loadSecret(label: clique.cliqueMemberIdentifier!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        let initiatorContext = self.makeInitiatorContext(contextID: initiatorContextID)
        let initiatorConfigurationContext = OTConfigurationContext()
        initiatorConfigurationContext.context = initiatorContextID
        initiatorConfigurationContext.altDSID = try XCTUnwrap(self.mockAuthKit.primaryAltDSID())
        initiatorConfigurationContext.otControl = self.otControl

        initiatorContext.startOctagonStateMachine()
        self.sendContainerChange(context: initiatorContext)
        let restoreExpectation = self.expectation(description: "restore returns")

        self.manager.restore(fromBottle: OTControlArguments(configuration: initiatorConfigurationContext), entropy: entropy!, bottleID: bottle.bottleID) { error in
            XCTAssertNil(error, "error should be nil")
            restoreExpectation.fulfill()
        }
        self.wait(for: [restoreExpectation], timeout: 10)

        self.assertEnters(context: initiatorContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        var initiatorDumpCallback = self.expectation(description: "initiatorDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(initiatorContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            initiatorDumpCallback.fulfill()
        }
        self.wait(for: [initiatorDumpCallback], timeout: 10)
        let recoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        let setRecoveryKeyExpectation = self.expectation(description: "setRecoveryKeyExpectation callback occurs")
        TestsObjectiveC.setNewRecoveryKeyWithData(self.otcliqueContext, recoveryKey: recoveryKey!) { rk, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNotNil(rk, "rk should not be nil")
            setRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [setRecoveryKeyExpectation], timeout: 10)

        let recoveryKey2 = try XCTUnwrap(SecRKCreateRecoveryKeyString(nil))
        let setRecoveryKeyExpectationAgain = self.expectation(description: "setRecoveryKeyExpectationAgain callback occurs")
        TestsObjectiveC.setNewRecoveryKeyWithData(initiatorConfigurationContext, recoveryKey: recoveryKey2) { rk, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNotNil(rk, "rk should not be nil")
            setRecoveryKeyExpectationAgain.fulfill()
        }
        self.wait(for: [setRecoveryKeyExpectationAgain], timeout: 10)

        self.sendContainerChangeWaitForFetch(context: initiatorContext)

        // When the original peer responds to the new peer, it should upload tlkshares for the new peer and the new RK
        // (since the remote peer didn't upload shares for the new RK)
        self.assertAllCKKSViewsUpload(tlkShares: 2)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // In real life, the peer setting the recovery key should have created these TLKShares. But, in these tests, it doesn't have a functioning CKKS to help it.
        XCTAssertFalse(try self.recoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey2, salt: try XCTUnwrap(self.mockAuthKit.primaryAltDSID())))
        XCTAssertTrue(try self.recoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey2, salt: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()), sender: self.cuttlefishContext))

        var initiatorRecoverySigningKey: Data?
        var initiatorRecoveryEncryptionKey: Data?

        var firstDeviceRecoverySigningKey: Data?
        var firstDeviceRecoveryEncryptionKey: Data?

        // now let's ensure recovery keys are set for both the first device and second device
        initiatorDumpCallback = self.expectation(description: "initiatorDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(initiatorContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")

            initiatorRecoverySigningKey = stableInfo!["recovery_signing_public_key"] as? Data
            initiatorRecoveryEncryptionKey = stableInfo!["recovery_encryption_public_key"] as? Data

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            let vouchers = dump!["vouchers"]
            XCTAssertNotNil(vouchers, "vouchers should not be nil")
            initiatorDumpCallback.fulfill()
        }
        self.wait(for: [initiatorDumpCallback], timeout: 10)

        let firstDeviceDumpCallback = self.expectation(description: "firstDeviceDumpCallback callback occurs")
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

            firstDeviceRecoverySigningKey = stableInfo!["recovery_signing_public_key"] as? Data
            firstDeviceRecoveryEncryptionKey = stableInfo!["recovery_encryption_public_key"] as? Data

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            let vouchers = dump!["vouchers"]
            XCTAssertNotNil(vouchers, "vouchers should not be nil")
            firstDeviceDumpCallback.fulfill()
        }
        self.wait(for: [firstDeviceDumpCallback], timeout: 10)

        XCTAssertEqual(firstDeviceRecoverySigningKey, initiatorRecoverySigningKey, "recovery signing keys should be equal")
        XCTAssertEqual(firstDeviceRecoveryEncryptionKey, initiatorRecoveryEncryptionKey, "recovery encryption keys should be equal")
    }

    func testOTCliqueJoiningUsingRecoveryKey() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let recoverykeyotcliqueContext = OTConfigurationContext()
        recoverykeyotcliqueContext.context = establishContextID
        recoverykeyotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit.primaryAltDSID())
        recoverykeyotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: recoverykeyotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        let recoveryKey = try XCTUnwrap(SecRKCreateRecoveryKeyString(nil))
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        let setRecoveryKeyExpectation = self.expectation(description: "setRecoveryKeyExpectation callback occurs")
        TestsObjectiveC.setNewRecoveryKeyWithData(recoverykeyotcliqueContext, recoveryKey: recoveryKey) { _, error in
            XCTAssertNil(error, "error should be nil")
            setRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [setRecoveryKeyExpectation], timeout: 10)

        try self.putRecoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))

        self.sendContainerChangeWaitForFetch(context: establishContext)

        let newCliqueContext = OTConfigurationContext()
        newCliqueContext.context = OTDefaultContext
        newCliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit.primaryAltDSID())
        newCliqueContext.otControl = self.otControl

        let newGuyContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)
        self.verifyDatabaseMocks()

        SecCKKSSetTestSkipTLKShareHealing(true)

        let joinWithRecoveryKeyExpectation = XCTestExpectation(description: "join callback")
        self.fakeCuttlefishServer.joinListener = { request in
            self.fakeCuttlefishServer.joinListener = nil
            let newStableInfo = request.peer.stableInfoAndSig.stableInfo()
            XCTAssertNotNil(newStableInfo.recoverySigningPublicKey, "recoverySigningPublicKey should not be nil")
            XCTAssertNotNil(newStableInfo.recoveryEncryptionPublicKey, "recoveryEncryptionPublicKey should not be nil")
            joinWithRecoveryKeyExpectation.fulfill()
            return nil
        }

        SecCKKSSetTestSkipTLKShareHealing(true)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.recover(withRecoveryKey: self.otcliqueContext, recoveryKey: recoveryKey))
        self.wait(for: [joinWithRecoveryKeyExpectation], timeout: 10)

        self.sendContainerChangeWaitForFetch(context: newGuyContext)
        self.verifyDatabaseMocks()
        XCTAssertTrue(try self.recoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.primaryAltDSID())))

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
            let vouchers = dump!["vouchers"]
            XCTAssertNotNil(vouchers, "vouchers should not be nil")
            stableInfoAcceptorCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoAcceptorCheckDumpCallback], timeout: 10)
        self.assertEnters(context: newGuyContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: newGuyContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertSelfTLKSharesInCloudKit(context: newGuyContext)

        self.sendContainerChangeWaitForFetch(context: establishContext)

        let stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(establishContext.activeAccount)) { dump, _ in
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
            let vouchers = dump!["vouchers"]
            XCTAssertNotNil(vouchers, "vouchers should not be nil")
            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }

    func testEstablishWhileUsingUnknownRecoveryKey() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        let recoveryKey = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        OctagonSetSOSFeatureEnabled(true)

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: self.cuttlefishContext)

        // joining via recovery key just as if SBD kicked off a join during _recoverWithRequest()
        let joinWithRecoveryKeyExpectation = self.expectation(description: "joinWithRecoveryKey callback occurs")

        // There's a CKKS race in this call, where CKKS will sometimes win the race and decide to upload a TLKShare to the new RK,
        // and sometimes it loses and treats the TLKShare that Octagon uploads as sufficient.

        // Cause CKKS to not even try the race.
        SecCKKSSetTestSkipTLKShareHealing(true)

        self.manager.join(withRecoveryKey: OTControlArguments(configuration: self.otcliqueContext), recoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithRecoveryKeyExpectation], timeout: 10)

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        var peerIDBeforeRestore: String?

        var dumpExpectation = self.expectation(description: "dump callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, error in
            XCTAssertNil(error, "Should be no error dumping data")
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let egoPeerID = egoSelf!["peerID"] as? String
            XCTAssertNotNil(egoPeerID, "egoPeerID should not be nil")
            peerIDBeforeRestore = egoPeerID
            dumpExpectation.fulfill()
        }
        self.wait(for: [dumpExpectation], timeout: 10)

        XCTAssertNotNil(peerIDBeforeRestore, "peerIDBeforeRestore should not be nil")

        let newOTCliqueContext = OTConfigurationContext()
        newOTCliqueContext.context = OTDefaultContext
        newOTCliqueContext.altDSID = self.otcliqueContext.altDSID
        newOTCliqueContext.otControl = self.otcliqueContext.otControl
        newOTCliqueContext.sbd = OTMockSecureBackup(bottleID: "", entropy: Data())

        let newClique: OTClique
        do {
            newClique = try OTClique.performEscrowRecovery(withContextData: newOTCliqueContext, escrowArguments: ["SecureBackupRecoveryKey": recoveryKey])
            XCTAssertNotNil(newClique, "newClique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored recovering: \(error)")
            throw error
        }

        // ensure the ego peer id hasn't changed
        dumpExpectation = self.expectation(description: "dump callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, error in
            XCTAssertNil(error, "Should be no error dumping data")
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let egoPeerID = egoSelf!["peerID"] as? String
            XCTAssertNotNil(egoPeerID, "egoPeerID should not be nil")
            XCTAssertTrue(egoPeerID == peerIDBeforeRestore, "peerIDs should be the same")

            dumpExpectation.fulfill()
        }
        self.wait(for: [dumpExpectation], timeout: 10)
    }

    func testJoinWithUnknownRecoveryKey() throws {
        self.startCKAccountStatusMock()

        let remote = self.makeInitiatorContext(contextID: "remote")
        self.assertResetAndBecomeTrusted(context: remote)

        let unknownRecoveryKey = try XCTUnwrap(SecRKCreateRecoveryKeyString(nil), "recoveryKey should not be nil")

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

#if os(tvOS)
        let joinWithRecoveryKeyExpectation = self.expectation(description: "joinWithRecoveryKeyExpectation callback occurs")

        XCTAssertThrowsError(try OctagonTrustCliqueBridge.recover(withRecoveryKey: self.otcliqueContext, recoveryKey: unknownRecoveryKey)) { error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error as NSError).domain, OctagonErrorDomain, "error domain should be OctagonErrorDomain")
            XCTAssertEqual((error as NSError).code, OctagonError.noRecoveryKeyRegistered.rawValue, "error code should be OctagonErrorNoRecoveryKeyRegistered")
            joinWithRecoveryKeyExpectation.fulfill()
        }

        self.wait(for: [joinWithRecoveryKeyExpectation], timeout: 10)

        // double-check that the status is not in
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

#else

        // There's a CKKS race in this call, where CKKS will sometimes win the race and decide to upload a TLKShare to the new RK,
        // and sometimes it loses and treats the TLKShare that Octagon uploads as sufficient.

        // Cause CKKS to not even try the race.
        SecCKKSSetTestSkipTLKShareHealing(true)

        let joinWithRecoveryKeyExpectation = XCTestExpectation(description: "join callback NOT expected")
        joinWithRecoveryKeyExpectation.isInverted = true
        self.fakeCuttlefishServer.joinListener = { _ in
            self.fakeCuttlefishServer.joinListener = nil
            joinWithRecoveryKeyExpectation.fulfill()
            return nil
        }

        XCTAssertThrowsError(try OctagonTrustCliqueBridge.recover(withRecoveryKey: self.otcliqueContext, recoveryKey: unknownRecoveryKey)) { error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error as NSError).domain, OctagonErrorDomain, "error domain should be OctagonErrorDomain")
            XCTAssertEqual((error as NSError).code, OctagonError.noRecoveryKeyRegistered.rawValue, "error code should be OctagonErrorNoRecoveryKeyRegistered")
        }
        self.wait(for: [joinWithRecoveryKeyExpectation], timeout: 2)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)
#endif
    }

    func testSetRecoveryKeyAsLimitedPeer() throws {
#if !os(tvOS)
        throw XCTSkip("test only runs on TVOS")
#else
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(self.mockAuthKit.currentDeviceList().isEmpty, "should not have zero devices")

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        let entropy = try self.loadSecret(label: clique.cliqueMemberIdentifier!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)

        let recoveryKey = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        let createKeyExpectation = self.expectation(description: "createKeyExpectation returns")
        self.manager.createRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), recoveryKey: recoveryKey) { error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error! as NSError).code, OctagonError.operationUnavailableOnLimitedPeer.rawValue, "error code should be limited peer")
            createKeyExpectation.fulfill()
        }
        self.wait(for: [createKeyExpectation], timeout: 10)
#endif
    }

    func testVouchWithWrongRecoveryKey() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let recoverykeyotcliqueContext = OTConfigurationContext()
        recoverykeyotcliqueContext.context = establishContextID
        recoverykeyotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit.primaryAltDSID())
        recoverykeyotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: recoverykeyotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)

        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        let recoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        let setRecoveryKeyExpectation = self.expectation(description: "setRecoveryKeyExpectation callback occurs")
        TestsObjectiveC.setNewRecoveryKeyWithData(recoverykeyotcliqueContext, recoveryKey: recoveryKey!) { _, error in
            XCTAssertNil(error, "error should be nil")
            setRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [setRecoveryKeyExpectation], timeout: 10)

        self.sendContainerChangeWaitForFetch(context: establishContext)

        let newCliqueContext = OTConfigurationContext()
        newCliqueContext.context = OTDefaultContext
        newCliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit.primaryAltDSID())
        newCliqueContext.otControl = self.otControl

        let newGuyContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)

        // creating new random recovery key
        let wrongRecoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(wrongRecoveryKey, "wrongRecoveryKey should not be nil")

        // We'll reset Octagon here, so allow for CKKS to reset as well
        self.silentZoneDeletesAllowed = true

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // There's a CKKS race in this call, where CKKS will sometimes win the race and decide to upload a TLKShare to the new RK,
        // and sometimes it loses and treats the TLKShare that Octagon uploads as sufficient.

        // Cause CKKS to not even try the race.
        SecCKKSSetTestSkipTLKShareHealing(true)

        let joinWithRecoveryKeyExpectation = self.expectation(description: "join callback NOT expected")
        joinWithRecoveryKeyExpectation.isInverted = true
        self.fakeCuttlefishServer.joinListener = { _ in
            self.fakeCuttlefishServer.joinListener = nil
            joinWithRecoveryKeyExpectation.fulfill()
            return nil
        }

        XCTAssertThrowsError(try OctagonTrustCliqueBridge.recover(withRecoveryKey: self.otcliqueContext, recoveryKey: wrongRecoveryKey!)) {error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error as NSError).domain, TrustedPeersHelperErrorDomain, "error domain should be com.apple.security.trustedpeers.container")
            XCTAssertEqual((error as NSError).code, TrustedPeersHelperErrorCode.codeUntrustedRecoveryKeys.rawValue, "error code should be untrusted recovery keys")
        }
        self.wait(for: [joinWithRecoveryKeyExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTrust, within: 10 * NSEC_PER_SEC)
    }

    // This test emulates the scenario where there are two peers in the circle, one of them falls into a lake.
    // the other sets a recovery key and signs out.
    // Then the user takes a brand new device - signs in - and attempts to use Recovery Key
    // This new device should be able to use the Recovery Key to join the existing Octagon circle

    func testRecoveryWithDistrustedPeers() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let recoverykeyotcliqueContext = OTConfigurationContext()
        recoverykeyotcliqueContext.context = establishContextID
        recoverykeyotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit.primaryAltDSID())
        recoverykeyotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: recoverykeyotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)

        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        let jumpsInALakePeerContext = self.makeInitiatorContext(contextID: "jumpsInALakePeerContext", authKitAdapter: self.mockAuthKit3)
        self.assertJoinViaEscrowRecovery(joiningContext: jumpsInALakePeerContext, sponsor: establishContext)
        self.sendContainerChangeWaitForFetch(context: establishContext)

        let recoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        let setRecoveryKeyExpectation = self.expectation(description: "setRecoveryKeyExpectation callback occurs")
        TestsObjectiveC.setNewRecoveryKeyWithData(recoverykeyotcliqueContext, recoveryKey: recoveryKey!) { _, error in
            XCTAssertNil(error, "error should be nil")
            setRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [setRecoveryKeyExpectation], timeout: 10)

        // jumpsInALakePeerContext does not receive the recovery key because it is in a lake.
        self.sendContainerChangeWaitForFetch(context: establishContext)

        try self.putRecoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey!, salt: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))

        // now this peer will attempt to leave Octagon
        XCTAssertThrowsError(try clique.leave(), "Should be no error departing clique") { error in
            XCTAssertNotNil(error, "error should not be nil")
        }

        // securityd should *still* consider itself trusted
        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        let brandNewDeviceCliqueContext = OTConfigurationContext()
        brandNewDeviceCliqueContext.context = OTDefaultContext
        brandNewDeviceCliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit.primaryAltDSID())
        brandNewDeviceCliqueContext.otControl = self.otControl

        let brandNewDeviceContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        brandNewDeviceContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: brandNewDeviceContext)

        // We expect an Octagon reset here, because the RK is for a distrusted peer
        // This also performs a CKKS reset
        self.silentZoneDeletesAllowed = true

        let joinWithRecoveryKeyExpectation = self.expectation(description: "join callback expected to occur")
        self.fakeCuttlefishServer.joinListener = { request in
            self.fakeCuttlefishServer.joinListener = nil
            let newStableInfo = request.peer.stableInfoAndSig.stableInfo()

            XCTAssertNotNil(newStableInfo.recoverySigningPublicKey, "Recovery signing key should be set")
            XCTAssertNotNil(newStableInfo.recoveryEncryptionPublicKey, "Recovery encryption key should be set")
            joinWithRecoveryKeyExpectation.fulfill()
            return nil
        }

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.recover(withRecoveryKey: brandNewDeviceCliqueContext, recoveryKey: recoveryKey!))

        self.wait(for: [joinWithRecoveryKeyExpectation], timeout: 10)

        self.assertEnters(context: brandNewDeviceContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testMalformedRecoveryKey() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let recoverykeyotcliqueContext = OTConfigurationContext()
        recoverykeyotcliqueContext.context = establishContextID
        recoverykeyotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit.primaryAltDSID())
        recoverykeyotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: recoverykeyotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try! self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        let recoveryKey = "malformedRecoveryKey"
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")
        OctagonSetSOSFeatureEnabled(true)

        let createKeyExpectation = self.expectation(description: "createKeyExpectation returns")
        self.manager.createRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), recoveryKey: recoveryKey) { error in
            XCTAssertNotNil(error, "error should NOT be nil")
            XCTAssertEqual((error! as NSError).code, 41, "error code should be 41/malformed recovery key")
            XCTAssertEqual((error! as NSError).domain, "com.apple.security.octagon", "error code domain should be com.apple.security.octagon")
            createKeyExpectation.fulfill()
        }
        self.wait(for: [createKeyExpectation], timeout: 2)

        let newCliqueContext = OTConfigurationContext()
        newCliqueContext.context = OTDefaultContext
        newCliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit.primaryAltDSID())
        newCliqueContext.otControl = self.otControl

        let newGuyContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)

        SecCKKSSetTestSkipTLKShareHealing(true)

        let joinWithRecoveryKeyExpectation = self.expectation(description: "join callback expected NOT to occur")
        joinWithRecoveryKeyExpectation.isInverted = true
        self.fakeCuttlefishServer.joinListener = { _ in
            self.fakeCuttlefishServer.joinListener = nil
            joinWithRecoveryKeyExpectation.fulfill()
            return nil
        }

        XCTAssertThrowsError(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueContext, recoveryKey: recoveryKey)) { error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error as NSError).code, 41, "error code should be 41/malformed recovery key")
            XCTAssertEqual((error as NSError).domain, "com.apple.security.octagon", "error code domain should be com.apple.security.octagon")
        }
        self.wait(for: [joinWithRecoveryKeyExpectation], timeout: 2)
    }

    @discardableResult
    func createAndSetRecoveryKey(context: OTCuttlefishContext) throws -> String {
        let cliqueConfiguration = OTConfigurationContext()
        cliqueConfiguration.context = context.contextID
        cliqueConfiguration.altDSID = try XCTUnwrap(context.activeAccount?.altDSID)
        cliqueConfiguration.otControl = self.otControl

        let recoveryKey = try XCTUnwrap(SecRKCreateRecoveryKeyString(nil), "should be able to create a recovery key")

        let setRecoveryKeyExpectation = self.expectation(description: "setRecoveryKeyExpectation callback occurs")
        TestsObjectiveC.setNewRecoveryKeyWithData(cliqueConfiguration, recoveryKey: recoveryKey) { _, error in
            XCTAssertNil(error, "error should be nil")
            setRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [setRecoveryKeyExpectation], timeout: 10)

        return recoveryKey
    }

    func testConcurWithTrustedPeer() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        self.assertResetAndBecomeTrustedInDefaultContext()

        let peer2Context = self.makeInitiatorContext(contextID: "peer2")
        let peer2ID = self.assertJoinViaEscrowRecovery(joiningContext: peer2Context, sponsor: self.cuttlefishContext)

        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // peer1 sets a recovery key
        var rkSigningPubKey: Data?
        var rkEncryptionPubKey: Data?

        let setRKExpectation = self.expectation(description: "setRecoveryKey")
        self.fakeCuttlefishServer.setRecoveryKeyListener = { request in
            self.fakeCuttlefishServer.setRecoveryKeyListener = nil
            XCTAssertNotNil(request.recoverySigningPubKey, "signing public key should be present")
            XCTAssertNotNil(request.recoveryEncryptionPubKey, "encryption public key should be present")

            rkSigningPubKey = request.recoverySigningPubKey
            rkEncryptionPubKey = request.recoveryEncryptionPubKey

            setRKExpectation.fulfill()
            return nil
        }

        try self.createAndSetRecoveryKey(context: self.cuttlefishContext)
        self.wait(for: [setRKExpectation], timeout: 10)

        // And peer2 concurs with it upon receiving a push
        let updateTrustExpectation = self.expectation(description: "updateTrust")
        self.fakeCuttlefishServer.updateListener = { [unowned self] request in
            XCTAssertEqual(request.peerID, peer2ID, "Update should be for peer2")

            let newStableInfo = request.stableInfoAndSig.stableInfo()
            XCTAssertEqual(newStableInfo.recoverySigningPublicKey, rkSigningPubKey, "Recovery signing key should match other peer")
            XCTAssertEqual(newStableInfo.recoveryEncryptionPublicKey, rkEncryptionPubKey, "Recovery encryption key should match other peer")
            self.fakeCuttlefishServer.updateListener = nil
            updateTrustExpectation.fulfill()

            return nil
        }

        self.sendContainerChangeWaitForFetch(context: peer2Context)
        self.wait(for: [updateTrustExpectation], timeout: 10)

        // Restart TPH, and ensure that more updates succeed
        self.tphClient.containerMap.removeAllContainers()

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: peer2Context)
    }

    func testRecoveryKeyLoadingOnContainerLoad() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        _ = self.assertResetAndBecomeTrustedInDefaultContext()
        // peer1 sets a recovery key
        try self.createAndSetRecoveryKey(context: self.cuttlefishContext)

        // Restart TPH
        self.tphClient.containerMap.removeAllContainers()

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
    }

    func testRecoveryKeyLoadingOnContainerLoadEvenIfMissing() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        _ = self.assertResetAndBecomeTrustedInDefaultContext()
        // peer1 sets a recovery key
        try self.createAndSetRecoveryKey(context: self.cuttlefishContext)

        // Before restarting TPH, emulate a world in which the RK variables were not set on the container

        let container = try self.tphClient.containerMap.findOrCreate(user: try XCTUnwrap(self.cuttlefishContext.activeAccount))
        container.removeRKFromContainer()

        // Restart TPH
        self.tphClient.containerMap.removeAllContainers()

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
    }

    func testCKKSSendsTLKSharesToRecoveryKey() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        // To get into a state where we don't upload the TLKShares to each RK on RK creation, put Octagon into a waitfortlk state
        // Right after CKKS fetches for the first time, insert a new key hierarchy into CloudKit
        self.silentFetchesAllowed = false
        self.expectCKFetchAndRun {
            self.putFakeKeyHierarchiesInCloudKit()
            self.putFakeDeviceStatusesInCloudKit()
            self.silentFetchesAllowed = true
        }

        do {
            let clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
        }

        // Now, we should be in 'ready'
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertConsidersSelfTrustedCachedAccountStatus(context: self.cuttlefishContext)

        // and all subCKKSes should enter waitfortlk, as they don't have the TLKs uploaded by the other peer
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLK, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // And a recovery key is set
        let recoveryKey = try self.createAndSetRecoveryKey(context: self.cuttlefishContext)

        // and now, all TLKs arrive! CKKS should upload two shares: one for itself, and one for the recovery key
        self.assertAllCKKSViewsUpload(tlkShares: 2)
        self.saveTLKMaterialToKeychain()

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
        XCTAssertTrue(try self.recoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()), sender: self.cuttlefishContext))
    }

    func testRKRecoveryRecoversCKKSCreatedShares() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        let remote = self.createEstablishContext(contextID: "remote")
        self.assertResetAndBecomeTrusted(context: remote)

#if os(tvOS)
        OctagonSetSOSFeatureEnabled(true)
        let recoveryKey = try self.createAndSetRecoveryKey(context: remote)
        OctagonSetSOSFeatureEnabled(false)
#else
        let recoveryKey = try self.createAndSetRecoveryKey(context: remote)
#endif

        // And TLKShares for the RK are sent from the Octagon peer
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putRecoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()), sender: remote)
        XCTAssertTrue(try self.recoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()), sender: remote))

        // Now, join! This should recover the TLKs.
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.assertAllCKKSViewsUpload(tlkShares: 1)

        let joinWithRecoveryKeyExpectation = self.expectation(description: "joinWithRecoveryKey callback occurs")
        self.cuttlefishContext.join(withRecoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithRecoveryKeyExpectation], timeout: 20)

        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func testRecoverTLKSharesSentToRKBeforeCKKSFetchCompletes() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        let remote = self.createEstablishContext(contextID: "remote")
        self.assertResetAndBecomeTrusted(context: remote)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: remote)
        self.assertSelfTLKSharesInCloudKit(context: remote)

        let recoveryKey = try self.createAndSetRecoveryKey(context: remote)

        try self.putRecoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))

        // Now, join from a new device
        // Simulate CKKS fetches taking forever. In practice, this is caused by many round-trip fetches to CK happening over minutes.
        self.holdCloudKitFetches()

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        SecCKKSSetTestSkipTLKShareHealing(true)

        let joinWithRecoveryKeyExpectation = self.expectation(description: "joinWithRecoveryKey callback occurs")
        self.cuttlefishContext.join(withRecoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithRecoveryKeyExpectation], timeout: 20)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateFetch, within: 10 * NSEC_PER_SEC)

        // When Octagon is creating itself TLKShares as part of the escrow recovery, CKKS will get into the right state without any uploads

        self.releaseCloudKitFetchHold()
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
    }

    func testJoinWithRecoveryKeyWithManyLimitedPeers() throws {
        try self.skipOnRecoveryKeyNotSupported()
        let homepodMIDs = (0...5).map { i in
            return "homepod\(i)"
        }

        self.mockAuthKit.otherDevices.addObjects(from: homepodMIDs)
        self.mockAuthKit2.otherDevices.addObjects(from: homepodMIDs)
        self.mockAuthKit3.otherDevices.addObjects(from: homepodMIDs)

        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        let establishPeerID = self.assertResetAndBecomeTrusted(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        let recoveryKey = try XCTUnwrap(SecRKCreateRecoveryKeyString(nil))

        let setRecoveryKeyExpectation = self.expectation(description: "setRecoveryKeyExpectation callback occurs")

        TestsObjectiveC.setNewRecoveryKeyWithData(try self.otconfigurationContextFor(context: establishContext), recoveryKey: recoveryKey) { _, error in
            XCTAssertNil(error, "error should be nil")
            setRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [setRecoveryKeyExpectation], timeout: 10)

        try self.putRecoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))

        self.sendContainerChangeWaitForFetch(context: establishContext)

        // Now, sponsor in the HomePods
        let homepodPeers = try homepodMIDs.map { machineID -> (String, OTCuttlefishContext) in
            let deviceInfo = OTMockDeviceInfoAdapter(modelID: "AudioAccessory,1,1",
                                                     deviceName: machineID,
                                                     serialNumber: NSUUID().uuidString,
                                                     osVersion: "NonsenseOS")

            let mockAuthKit = CKKSTestsMockAccountsAuthKitAdapter(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()),
                                                   machineID: machineID,
                                                   otherDevices: self.mockAuthKit.currentDeviceList())

            self.fakeCuttlefishServer.joinListener = { joinRequest in
                XCTAssertTrue(joinRequest.peer.hasStableInfoAndSig, "Joining peer should have a stable info")
                let newStableInfo = joinRequest.peer.stableInfoAndSig.stableInfo()

                XCTAssertNotNil(newStableInfo.recoverySigningPublicKey, "Recovery signing key should be set")
                XCTAssertNotNil(newStableInfo.recoveryEncryptionPublicKey, "Recovery encryption key should be set")

                return nil
            }

            let homepod = self.manager.context(forContainerName: OTCKContainerName,
                                               contextID: machineID,
                                               sosAdapter: self.mockSOSAdapter!,
                                               accountsAdapter: mockAuthKit,
                                               authKitAdapter: mockAuthKit,
                                               tooManyPeersAdapter: self.mockTooManyPeers,
                                               tapToRadarAdapter: self.mockTapToRadar,
                                               lockStateTracker: self.lockStateTracker,
                                               deviceInformationAdapter: deviceInfo)
            let peerID = self.assertJoinViaProximitySetup(joiningContext: homepod, sponsor: establishContext)

            self.fakeCuttlefishServer.joinListener = nil

            // Not right, but will save us complexity later in the test
            try self.putSelfTLKSharesInCloudKit(context: homepod)
            return (peerID, homepod)
        }

        self.sendContainerChangeWaitForFetch(context: establishContext)

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        homepodPeers.forEach { peerID, _ in
            XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: establishPeerID, opinion: .trusts, target: peerID)),
                          "establish peer should trust homepod \(peerID)")
        }

        self.cuttlefishContext.startOctagonStateMachine()

        OctagonSetSOSFeatureEnabled(true)

        let joinExpectation = self.expectation(description: "joinExpectation")
        self.fakeCuttlefishServer.joinListener = { request in
            self.fakeCuttlefishServer.joinListener = nil
            let newStableInfo = request.peer.stableInfoAndSig.stableInfo()
            XCTAssertNotNil(newStableInfo.recoverySigningPublicKey, "recoverySigningPublicKey should not be nil")
            XCTAssertNotNil(newStableInfo.recoveryEncryptionPublicKey, "recoveryEncryptionPublicKey should not be nil")
            joinExpectation.fulfill()
            return nil
        }

        SecCKKSSetTestSkipTLKShareHealing(true)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.recover(withRecoveryKey: self.otcliqueContext, recoveryKey: recoveryKey))
        self.wait(for: [joinExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        self.verifyDatabaseMocks()
        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testCreateAndSetRecoveryKey() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        var error: NSError?
        var recoveryString: String?
        XCTAssertNoThrow(recoveryString = cliqueBridge.createAndSetRecoveryKey(with: self.otcliqueContext, error: &error), "createAndSetRecoveryKey should not error")
        XCTAssertNil(error, "error should be nil")
        XCTAssertNotNil(recoveryString, "recoveryString should not be nil")
    }

    func testIsRecoveryKeySetAPIWhereRKIsNotSet() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertThrowsError(try OctagonTrustCliqueBridge.isRecoveryKeySet(self.otcliqueContext), "isRecoveryKeySet() should throw an error")
    }

    func testIsRecoveryKeySetAPIWhereFetchChangesErrors() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let fetchChangesExpectation = self.expectation(description: "fetch changes occurs")
        fetchChangesExpectation.expectedFulfillmentCount = 6
        self.fakeCuttlefishServer.fetchChangesListener = { _ in
            fetchChangesExpectation.fulfill()
            return FakeCuttlefishServer.makeCloudKitCuttlefishError(code: .transactionalFailure)
        }

        OctagonSetSOSFeatureEnabled(false)
        do {
            try OctagonTrustCliqueBridge.isRecoveryKeySet(self.otcliqueContext)
        } catch {
            XCTAssertNotNil(error)
            XCTAssertEqual((error as NSError).domain, CKErrorDomain, "error domain should be CKErrorDomain")
            XCTAssertEqual((error as NSError).code, CKError.serverRejectedRequest.rawValue, "error code should be server rejected request")
        }
        self.wait(for: [fetchChangesExpectation], timeout: 10)
    }

    func testIsRecoveryKeySetAPIWhereRKIsSet() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(self.mockAuthKit.currentDeviceList().isEmpty, "should not have zero devices")

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        let entropy = try self.loadSecret(label: clique.cliqueMemberIdentifier!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)

        let recoveryKey = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")
        OctagonSetSOSFeatureEnabled(true)

        let createKeyExpectation = self.expectation(description: "createKeyExpectation returns")
        self.manager.createRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), recoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            createKeyExpectation.fulfill()
        }
        self.wait(for: [createKeyExpectation], timeout: 10)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(self.otcliqueContext), "isRecoveryKeySet should not throw an error")
    }

    func testCreateAndSetRecoveryKeyThenRecoverWithRecoveryKey() throws {
        try self.skipOnRecoveryKeyNotSupported()
        OctagonSetSOSFeatureEnabled(false)

        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        var error: NSError?
        var recoveryString: String?
        XCTAssertNoThrow(recoveryString = cliqueBridge.createAndSetRecoveryKey(with: self.otcliqueContext, error: &error), "createAndSetRecoveryKey should not error")
        XCTAssertNil(error, "error should be nil")
        XCTAssertNotNil(recoveryString, "recoveryString should not be nil")

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueConfiguration = OTConfigurationContext()
        newCliqueConfiguration.context = newContextID
        newCliqueConfiguration.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueConfiguration.otControl = self.otControl
        newCliqueConfiguration.sbd = OTMockSecureBackup(bottleID: nil, entropy: nil)

        let mockSBD: OTMockSecureBackup = self.otcliqueContext.sbd as! OTMockSecureBackup
        mockSBD.setRecoveryKey(recoveryKey: recoveryString)
        self.otcliqueContext.sbd = mockSBD
        newCliqueConfiguration.sbd = mockSBD

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)
        self.verifyDatabaseMocks()

        SecCKKSSetTestSkipTLKShareHealing(true)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueConfiguration, recoveryKey: recoveryString!), "recoverWithRecoveryKey should not throw an error")
        self.assertEnters(context: newGuyContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForFetch(context: newGuyContext)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(newCliqueConfiguration), "isRecoveryKeySet should not throw an error")

        var stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(newGuyContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")

            let recoverySigningData = stableInfo!["recovery_signing_public_key"]
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")
            let recoverySigningData = stableInfo!["recovery_signing_public_key"]
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }

    func testRecoverWithRecoveryKeyWithRecoveryKeyNotSetInSOSAndOctagon() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let recoveryKey = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        XCTAssertThrowsError(try OctagonTrustCliqueBridge.recover(withRecoveryKey: self.otcliqueContext, recoveryKey: recoveryKey)) { error in
            let nsError = error as NSError
            XCTAssertEqual(nsError.domain, OctagonErrorDomain, "error domain should be OctagonErrorDomain")
            XCTAssertEqual(nsError.code, OctagonError.noRecoveryKeyRegistered.rawValue, "error code should be OctagonErrorNoRecoveryKeyRegistered")
        }
    }

    func testRecoverWithRecoveryKeyExpectOctagonReset() throws {
#if os(tvOS) || os(watchOS)
        throw XCTSkip("Apple TVs and watches will not set recovery key")
#else
        self.startCKAccountStatusMock()
        self.cuttlefishContext.startOctagonStateMachine()

        let recoveryKey = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        self.otcliqueContext.sbd = OTMockSecureBackup(bottleID: nil, entropy: nil)
        self.otcliqueContext.overrideForJoinAfterRestore = true

        let establishExpectation = self.expectation(description: "establishExpectation")

        self.fakeCuttlefishServer.establishListener = { _ in
            self.fakeCuttlefishServer.establishListener = nil
            establishExpectation.fulfill()
            return nil
        }

        OctagonSetSOSFeatureEnabled(true)
        let mockSBD = otcliqueContext.sbd as! OTMockSecureBackup
        mockSBD.setRecoveryKey(recoveryKey: recoveryKey)
        self.otcliqueContext.sbd = mockSBD

        SecCKKSSetTestSkipTLKShareHealing(true)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.recover(withRecoveryKey: self.otcliqueContext, recoveryKey: recoveryKey), "recoverWithRecoveryKey should not throw an error")

        self.wait(for: [establishExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
#endif
    }

    func testRemoveRecoveryKey() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        var error: NSError?
        var recoveryString: String?
        XCTAssertNoThrow(recoveryString = cliqueBridge.createAndSetRecoveryKey(with: self.otcliqueContext, error: &error), "createAndSetRecoveryKey should not error")
        XCTAssertNil(error, "error should be nil")
        XCTAssertNotNil(recoveryString, "recoveryString should not be nil")

        let mockSBD = self.otcliqueContext.sbd as! OTMockSecureBackup
        mockSBD.setRecoveryKey(recoveryKey: recoveryString!)
        self.otcliqueContext.sbd = mockSBD

        // Recovery Key should be set
        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(self.otcliqueContext), "recovery key should be set")

        let removeExpectation = self.expectation(description: "removeExpectation")
        self.fakeCuttlefishServer.removeRecoveryKeyListener = { _ in
            self.fakeCuttlefishServer.removeRecoveryKeyListener = nil
            removeExpectation.fulfill()
            return nil
        }

        XCTAssertNoThrow(try cliqueBridge.removeRecoveryKey(with: self.otcliqueContext), "removeRecoveryKey should not error")
        mockSBD.setRecoveryKey(recoveryKey: nil)

        self.wait(for: [removeExpectation], timeout: 10)

        XCTAssertNil(error, "error should be nil")
        XCTAssertThrowsError(try OctagonTrustCliqueBridge.isRecoveryKeySet(self.otcliqueContext), "recovery key should not be set")

        let stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should be nil")
            XCTAssertNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should be nil")

            XCTAssertNil(dump!["modelRecoverySigningPublicKey"], "modelRecoverySigningPublicKey should be nil")
            XCTAssertNil(dump!["modelRecoveryEncryptionPublicKey"], "modelRecoveryEncryptionPublicKey should be nil")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 1, "should be 1 peer id")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }

    func testRemoveRecoveryKeyAllPeersUntrustRKPeer() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        var error: NSError?
        var recoveryString: String?
        OctagonSetSOSFeatureEnabled(false)

        let setExpectation = self.expectation(description: "setExpectation")
        self.fakeCuttlefishServer.setRecoveryKeyListener = { _ in
            self.fakeCuttlefishServer.setRecoveryKeyListener = nil
            setExpectation.fulfill()
            return nil
        }
        XCTAssertNoThrow(recoveryString = cliqueBridge.createAndSetRecoveryKey(with: self.otcliqueContext, error: &error), "createAndSetRecoveryKey should not error")
        self.wait(for: [setExpectation], timeout: 10)

        XCTAssertNil(error, "error should be nil")
        XCTAssertNotNil(recoveryString, "recoveryString should not be nil")

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueContext = OTConfigurationContext()
        newCliqueContext.context = newContextID
        newCliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueContext.otControl = self.otControl
        newCliqueContext.sbd = OTMockSecureBackup(bottleID: nil, entropy: nil)

        let mockSBD = self.otcliqueContext.sbd as! OTMockSecureBackup
        mockSBD.setRecoveryKey(recoveryKey: recoveryString!)
        self.otcliqueContext.sbd = mockSBD
        newCliqueContext.sbd = mockSBD

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)

        let joinExpectation = self.expectation(description: "joinExpectation")
        self.fakeCuttlefishServer.joinListener = { request in
            self.fakeCuttlefishServer.joinListener = nil
            let newStableInfo = request.peer.stableInfoAndSig.stableInfo()
            XCTAssertNotNil(newStableInfo.recoverySigningPublicKey, "recoverySigningPublicKey should not be nil")
            XCTAssertNotNil(newStableInfo.recoveryEncryptionPublicKey, "recoveryEncryptionPublicKey should not be nil")
            joinExpectation.fulfill()
            return nil
        }

        SecCKKSSetTestSkipTLKShareHealing(true)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueContext, recoveryKey: recoveryString!), "recoverWithRecoveryKey should not throw an error")
        self.wait(for: [joinExpectation], timeout: 10)

        self.assertEnters(context: newGuyContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        // Recovery Key should be set
        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(self.otcliqueContext), "recovery key should be set")
        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(newCliqueContext), "recovery key should be set")

        let removeExpectation = self.expectation(description: "removeExpectation")
        self.fakeCuttlefishServer.removeRecoveryKeyListener = { _ in
            self.fakeCuttlefishServer.removeRecoveryKeyListener = nil
            removeExpectation.fulfill()
            return nil
        }

        XCTAssertNoThrow(try cliqueBridge.removeRecoveryKey(with: self.otcliqueContext), "removeRecoveryKey should not error")
        mockSBD.setRecoveryKey(recoveryKey: nil)

        self.wait(for: [removeExpectation], timeout: 10)

        self.sendContainerChangeWaitForFetch(context: newGuyContext)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        XCTAssertThrowsError(try OctagonTrustCliqueBridge.isRecoveryKeySet(self.otcliqueContext), "recovery key should not be set")
        XCTAssertThrowsError(try OctagonTrustCliqueBridge.isRecoveryKeySet(newCliqueContext), "recovery key should not be set")

        var recoveryKeyRemovalCallback = self.expectation(description: "recoveryKeyRemovalCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let excluded = dynamicInfo!["excluded"] as? [String]
            let recoveryKeyPeerID = excluded![0]
            XCTAssertTrue(recoveryKeyPeerID.contains("RK-"), "should contain excluded recovery key peerID")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should be nil")
            XCTAssertNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should be nil")

            XCTAssertNil(dump!["modelRecoverySigningPublicKey"], "modelRecoverySigningPublicKey should be nil")
            XCTAssertNil(dump!["modelRecoveryEncryptionPublicKey"], "modelRecoveryEncryptionPublicKey should be nil")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            recoveryKeyRemovalCallback.fulfill()
        }
        self.wait(for: [recoveryKeyRemovalCallback], timeout: 10)

        recoveryKeyRemovalCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(newGuyContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let excluded = dynamicInfo!["excluded"] as? [String]
            let recoveryKeyPeerID = excluded![0]
            XCTAssertTrue(recoveryKeyPeerID.contains("RK-"), "should contain excluded recovery key peerID")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should be nil")
            XCTAssertNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should be nil")

            XCTAssertNil(dump!["modelRecoverySigningPublicKey"], "modelRecoverySigningPublicKey should be nil")
            XCTAssertNil(dump!["modelRecoveryEncryptionPublicKey"], "modelRecoveryEncryptionPublicKey should be nil")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            recoveryKeyRemovalCallback.fulfill()
        }
        self.wait(for: [recoveryKeyRemovalCallback], timeout: 10)
    }

    func testIsRecoveryKeySetWithoutAccount() throws {
        try self.skipOnRecoveryKeyNotSupported()
        let isRecoveryKeySetExpectation = self.expectation(description: "isRecoveryKeySet callback occurs")
        XCTAssertThrowsError(try OctagonTrustCliqueBridge.isRecoveryKeySet(self.otcliqueContext), "isRecoveryKeySet should fail") { error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error as NSError).domain, OctagonErrorDomain, "error domain should be OctagonErrorDomain")
            XCTAssertEqual((error as NSError).code, OctagonError.iCloudAccountStateUnknown.rawValue, "error code should be OctagonErrorNotSignedIn")
            isRecoveryKeySetExpectation.fulfill()
        }
        self.wait(for: [isRecoveryKeySetExpectation], timeout: 20)
        self.startCKAccountStatusMock()
    }

    func testRemoveRemoveKeyWithoutAccount() throws {
        try self.skipOnRecoveryKeyNotSupported()
        let removeRecoveryKeyExpectation = self.expectation(description: "removeRecoveryKey callback occurs")
        self.cuttlefishContext.rpcRemoveRecoveryKey() { _, error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error! as NSError).domain, OctagonErrorDomain, "error domain should be OctagonErrorDomain")
            XCTAssertEqual((error! as NSError).code, OctagonError.iCloudAccountStateUnknown.rawValue, "error code should be OctagonErrorNotSignedIn")
            removeRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [removeRecoveryKeyExpectation], timeout: 20)
        self.startCKAccountStatusMock()
    }

// MARK: tests for case device has KVS, Octagon and SOS
    func testNoRKInOctagonAndNoRKInSecurebackup() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        OctagonSetSOSFeatureEnabled(true)

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueContext = OTConfigurationContext()
        newCliqueContext.context = newContextID
        newCliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueContext.otControl = self.otControl
        newCliqueContext.sbd = OTMockSecureBackup(bottleID: nil, entropy: nil)

        let mockSBD = self.otcliqueContext.sbd as! OTMockSecureBackup
        self.otcliqueContext.sbd = mockSBD
        newCliqueContext.sbd = mockSBD

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.assertEnters(context: newGuyContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let unsetRecoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(unsetRecoveryKey, "unsetRecoveryKey should not be nil")

        XCTAssertThrowsError(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueContext, recoveryKey: unsetRecoveryKey!)) { error in
            let nsError = error as NSError
            XCTAssertEqual(nsError.domain, OctagonErrorDomain, "error domain should be OctagonErrorDomain")
            XCTAssertEqual(nsError.code, OctagonError.noRecoveryKeyRegistered.rawValue, "error code should be OctagonErrorNoRecoveryKeyRegistered")
        }

        self.assertEnters(context: newGuyContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
    }

    func testRKInSecureBackupRKInOctagonAndMatchCorrectlyEntered() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        var error: NSError?
        var recoveryString: String?
        XCTAssertNoThrow(recoveryString = cliqueBridge.createAndSetRecoveryKey(with: self.otcliqueContext, error: &error), "createAndSetRecoveryKey should not error")
        XCTAssertNil(error, "error should be nil")
        XCTAssertNotNil(recoveryString, "recoveryString should not be nil")

        OctagonSetSOSFeatureEnabled(true)

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueConfiguration = OTConfigurationContext()
        newCliqueConfiguration.context = newContextID
        newCliqueConfiguration.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueConfiguration.otControl = self.otControl

        let mockSBD = self.otcliqueContext.sbd as! OTMockSecureBackup
        mockSBD.setRecoveryKey(recoveryKey: recoveryString!)
        self.otcliqueContext.sbd = mockSBD
        newCliqueConfiguration.sbd = mockSBD

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)
        self.verifyDatabaseMocks()

        SecCKKSSetTestSkipTLKShareHealing(true)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueConfiguration, recoveryKey: recoveryString!), "recoverWithRecoveryKey should not throw an error")
        self.assertEnters(context: newGuyContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForFetch(context: newGuyContext)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(newCliqueConfiguration), "isRecoveryKeySet should not throw an error")

        var stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(newGuyContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")

            let recoverySigningData = stableInfo!["recovery_signing_public_key"]
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")
            let recoverySigningData = stableInfo!["recovery_signing_public_key"]
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }

    func testRKInSecureBackupRKInOctagonAndMatchAndInCorrectlyEntered() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        var error: NSError?
        var recoveryString: String?
        XCTAssertNoThrow(recoveryString = cliqueBridge.createAndSetRecoveryKey(with: self.otcliqueContext, error: &error), "createAndSetRecoveryKey should not error")
        XCTAssertNil(error, "error should be nil")
        XCTAssertNotNil(recoveryString, "recoveryString should not be nil")

        OctagonSetSOSFeatureEnabled(true)

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueConfiguration = OTConfigurationContext()
        newCliqueConfiguration.context = newContextID
        newCliqueConfiguration.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueConfiguration.otControl = self.otControl
        newCliqueConfiguration.sbd = OTMockSecureBackup(bottleID: nil, entropy: nil)

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)
        self.verifyDatabaseMocks()

        let mockSBD = self.otcliqueContext.sbd as! OTMockSecureBackup

        mockSBD.setRecoveryKey(recoveryKey: recoveryString)
        self.otcliqueContext.sbd = mockSBD
        newCliqueConfiguration.sbd = mockSBD

        let resetExpectation = self.expectation(description: "reset expected NOT to occur")
        resetExpectation.isInverted = true
        self.fakeCuttlefishServer.resetListener = {  _ in
            self.fakeCuttlefishServer.resetListener = nil
            resetExpectation.fulfill()
            return nil
        }

        let wrongRecoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(wrongRecoveryKey, "wrongRecoveryKey should not be nil")

        XCTAssertThrowsError(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueConfiguration, recoveryKey: wrongRecoveryKey!)) { error in
            let nsError = error as NSError
            XCTAssertEqual(nsError.domain, TrustedPeersHelperErrorDomain, "error domain should be TrustedPeersHelperErrorDomain")
            XCTAssertEqual(nsError.code, TrustedPeersHelperErrorCode.codeUntrustedRecoveryKeys.rawValue, "error code should be TrustedPeersHelperErrorCodeUntrustedRecoveryKeys")
        }

        self.wait(for: [resetExpectation], timeout: 2)

        self.assertEnters(context: newGuyContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(newCliqueConfiguration), "isRecoveryKeySet should not throw an error")
        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(self.otcliqueContext), "isRecoveryKeySet should not throw an error")

        var stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(newGuyContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")

            XCTAssertNotNil(dump!["modelRecoverySigningPublicKey"] as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertNotNil(dump!["modelRecoveryEncryptionPublicKey"] as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")
            let recoverySigningData = stableInfo!["recovery_signing_public_key"]
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 1, "should be 1 peer id")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }

    func testRKInSecureBackupNORKInOctagon() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()
        OctagonSetSOSFeatureEnabled(true)

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueConfiguration = OTConfigurationContext()
        newCliqueConfiguration.context = newContextID
        newCliqueConfiguration.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueConfiguration.otControl = self.otControl
        newCliqueConfiguration.overrideForJoinAfterRestore = true

        let mockSBD = OTMockSecureBackup(bottleID: nil, entropy: nil)
        let correctInSOSRecoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(correctInSOSRecoveryKey, "correctInSOSRecoveryKey should not be nil")
        mockSBD.setRecoveryKey(recoveryKey: correctInSOSRecoveryKey!)
        newCliqueConfiguration.sbd = mockSBD
        self.otcliqueContext.sbd = mockSBD

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)
        self.verifyDatabaseMocks()

        let resetExpectation = self.expectation(description: "resetExpectation")
        self.fakeCuttlefishServer.resetListener = {  _ in
            self.fakeCuttlefishServer.resetListener = nil
            resetExpectation.fulfill()
            return nil
        }

        let setRKExpectation = self.expectation(description: "setRecoveryKey")
        self.fakeCuttlefishServer.setRecoveryKeyListener = { request in
            self.fakeCuttlefishServer.setRecoveryKeyListener = nil

            XCTAssertNotNil(request.recoverySigningPubKey, "signing public key should be present")
            XCTAssertNotNil(request.recoveryEncryptionPubKey, "encryption public key should be present")
            setRKExpectation.fulfill()
            return nil
        }

        SecCKKSSetTestSkipTLKShareHealing(true)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueConfiguration, recoveryKey: correctInSOSRecoveryKey!), "recoverWithRecoveryKey should not throw an error")

        self.wait(for: [resetExpectation, setRKExpectation], timeout: 10)

        self.assertEnters(context: newGuyContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        // RK is now set in SOS/Octagon
        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(newCliqueConfiguration), "isRecoveryKeySet should not throw an error")
        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(self.otcliqueContext), "isRecoveryKeySet should not throw an error")

        self.sendContainerChangeWaitForFetch(context: newGuyContext)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        var stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(newGuyContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")

            let recoverySigningData = stableInfo!["recovery_signing_public_key"]
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 1, "should be 1 peer id")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNil(dynamicInfo, "dynamicInfo should be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNil(stableInfo, "stableInfo should be nil")

            XCTAssertNil(stableInfo?["recovery_signing_public_key"], "recovery signing key should be nil")
            XCTAssertNil(stableInfo?["recovery_encryption_public_key"], "recovery encryption key should be nil")

            XCTAssertNotNil(dump?["modelRecoverySigningPublicKey"], "recovery signing key should not be nil")
            XCTAssertNotNil(dump?["modelRecoveryEncryptionPublicKey"], "recovery encryption key should not be nil")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }

    func testRKInSecureBackupNORKInOctagonIncorrectRKEntered() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()
        OctagonSetSOSFeatureEnabled(true)

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueConfiguration = OTConfigurationContext()
        newCliqueConfiguration.context = newContextID
        newCliqueConfiguration.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueConfiguration.otControl = self.otControl
        newCliqueConfiguration.overrideForJoinAfterRestore = true

        let mockSBD = OTMockSecureBackup(bottleID: nil, entropy: nil)
        let correctInSOSRecoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(correctInSOSRecoveryKey, "correctInSOSRecoveryKey should not be nil")
        mockSBD.setRecoveryKey(recoveryKey: correctInSOSRecoveryKey!)
        newCliqueConfiguration.sbd = mockSBD
        self.otcliqueContext.sbd = mockSBD

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)
        self.verifyDatabaseMocks()

        let resetExpectation = self.expectation(description: "reset expected NOT to occur")
        resetExpectation.isInverted = true
        self.fakeCuttlefishServer.resetListener = {  _ in
            self.fakeCuttlefishServer.resetListener = nil
            resetExpectation.fulfill()
            return nil
        }

        let setRKExpectation = self.expectation(description: "setRecoveryKey expected NOT to occur")
        setRKExpectation.isInverted = true
        self.fakeCuttlefishServer.setRecoveryKeyListener = { _ in
            self.fakeCuttlefishServer.setRecoveryKeyListener = nil
            setRKExpectation.fulfill()
            return nil
        }

        let wrongRecoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(wrongRecoveryKey, "recoveryKey should not be nil")

        XCTAssertThrowsError(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueConfiguration, recoveryKey: wrongRecoveryKey!)) { error in
            let nsError = error as NSError
            XCTAssertEqual(nsError.domain, OctagonErrorDomain, "error domain should be OctagonErrorDomain")
            XCTAssertEqual(nsError.code, OctagonError.secureBackupRestoreUsingRecoveryKeyFailed.rawValue, "error code should be OctagonErrorSecureBackupRestoreUsingRecoveryKeyFailed")
        }

        self.wait(for: [resetExpectation, setRKExpectation], timeout: 2)

        self.assertEnters(context: newGuyContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // RK is still set in SOS
        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(newCliqueConfiguration), "isRecoveryKeySet should not throw an error")
        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(self.otcliqueContext), "isRecoveryKeySet should not throw an error")

        var stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(newGuyContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            XCTAssertNil(egoSelf?["dynamicInfo"], "dynamicInfo should be nil")
            XCTAssertNil(egoSelf?["stableInfo"], "dynamicInfo should be nil")

            XCTAssertNil(dump!["modelRecoverySigningPublicKey"], "modelRecoverySigningPublicKey should be nil")
            XCTAssertNil(dump!["modelRecoveryEncryptionPublicKey"], "modelRecoveryEncryptionPublicKey should be nil")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNil(stableInfo?["recovery_signing_public_key"], "recovery_signing_public_key should be nil")
            XCTAssertNil(stableInfo?["recovery_encryption_public_key"], "recovery_encryption_public_key should be nil")

            XCTAssertNil(dump!["modelRecoverySigningPublicKey"], "modelRecoverySigningPublicKey should be nil")
            XCTAssertNil(dump!["modelRecoveryEncryptionPublicKey"], "modelRecoveryEncryptionPublicKey should be nil")

            let included = dynamicInfo?["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 1, "should be 1 peer id")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }

    func testNORKInSecureBackupRKInOctagonAndCorrect() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        var error: NSError?
        var recoveryString: String?
        XCTAssertNoThrow(recoveryString = cliqueBridge.createAndSetRecoveryKey(with: self.otcliqueContext, error: &error), "createAndSetRecoveryKey should not error")
        XCTAssertNil(error, "error should be nil")
        XCTAssertNotNil(recoveryString, "recoveryString should not be nil")

        OctagonSetSOSFeatureEnabled(true)

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueConfiguration = OTConfigurationContext()
        newCliqueConfiguration.context = newContextID
        newCliqueConfiguration.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueConfiguration.otControl = self.otControl
        let mockSBD: OTMockSecureBackup = self.otcliqueContext.sbd as! OTMockSecureBackup
        self.otcliqueContext.sbd = mockSBD
        newCliqueConfiguration.sbd = mockSBD

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)
        self.verifyDatabaseMocks()

        SecCKKSSetTestSkipTLKShareHealing(true)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueConfiguration, recoveryKey: recoveryString!), "recoverWithRecoveryKey should not throw an error")
        self.assertEnters(context: newGuyContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForFetch(context: newGuyContext)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(newCliqueConfiguration), "isRecoveryKeySet should not throw an error")

        var stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(newGuyContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")

            let recoverySigningData = stableInfo!["recovery_signing_public_key"]
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")
            let recoverySigningData = stableInfo!["recovery_signing_public_key"]
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }

    func testNORKInSecureBackupRKInOctagonAndIncorrect() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        var error: NSError?
        var recoveryString: String?
        XCTAssertNoThrow(recoveryString = cliqueBridge.createAndSetRecoveryKey(with: self.otcliqueContext, error: &error), "createAndSetRecoveryKey should not error")
        XCTAssertNil(error, "error should be nil")
        XCTAssertNotNil(recoveryString, "recoveryString should not be nil")

        OctagonSetSOSFeatureEnabled(true)

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueConfiguration = OTConfigurationContext()
        newCliqueConfiguration.context = newContextID
        newCliqueConfiguration.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueConfiguration.otControl = self.otControl
        let mockSBD: OTMockSecureBackup = self.otcliqueContext.sbd as! OTMockSecureBackup
        mockSBD.setRecoveryKey(recoveryKey: recoveryString!)
        self.otcliqueContext.sbd = mockSBD
        newCliqueConfiguration.sbd = mockSBD

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)
        self.verifyDatabaseMocks()

        let resetExpectation = self.expectation(description: "reset expected NOT to occur")
        resetExpectation.isInverted = true
        self.fakeCuttlefishServer.resetListener = {  _ in
            self.fakeCuttlefishServer.resetListener = nil
            resetExpectation.fulfill()
            return nil
        }

        let setRKExpectation = self.expectation(description: "setRecoveryKey expected NOT to occur")
        setRKExpectation.isInverted = true
        self.fakeCuttlefishServer.setRecoveryKeyListener = { _ in
            self.fakeCuttlefishServer.setRecoveryKeyListener = nil
            setRKExpectation.fulfill()
            return nil
        }

        let wrongRecoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(wrongRecoveryKey, "wrongRecoveryKey should not be nil")

        XCTAssertThrowsError(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueConfiguration, recoveryKey: wrongRecoveryKey!)) { error in
            let nsError = error as NSError
            XCTAssertEqual(nsError.domain, TrustedPeersHelperErrorDomain, "error domain should be TrustedPeersHelperErrorDomain")
            XCTAssertEqual(nsError.code, TrustedPeersHelperErrorCode.codeUntrustedRecoveryKeys.rawValue, "error code should be TrustedPeersHelperErrorCodeUntrustedRecoveryKeys")
        }

        self.wait(for: [resetExpectation, setRKExpectation], timeout: 2)

        self.assertEnters(context: newGuyContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(newCliqueConfiguration), "isRecoveryKeySet should not throw an error")

        var stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(newGuyContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            XCTAssertNil(egoSelf?["dynamicInfo"], "dynamicInfo should be nil")
            XCTAssertNil(egoSelf?["stableInfo"], "stableInfo should be nil")

            XCTAssertNotNil(dump!["modelRecoverySigningPublicKey"] as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertNotNil(dump!["modelRecoveryEncryptionPublicKey"] as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")
            let recoverySigningData = stableInfo!["recovery_signing_public_key"]
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 1, "should be 1 peer id")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }

    func testRKInSecureBackupRKInOctagonAndMisMatchCorrectInSOS() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        var error: NSError?
        var recoveryString: String?
        XCTAssertNoThrow(recoveryString = cliqueBridge.createAndSetRecoveryKey(with: self.otcliqueContext, error: &error), "createAndSetRecoveryKey should not error")
        XCTAssertNil(error, "error should be nil")
        XCTAssertNotNil(recoveryString, "recoveryString should not be nil")

        OctagonSetSOSFeatureEnabled(true)

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueConfiguration = OTConfigurationContext()
        newCliqueConfiguration.context = newContextID
        newCliqueConfiguration.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueConfiguration.otControl = self.otControl
        newCliqueConfiguration.octagonCapableRecordsExist = true

        let mockSBD: OTMockSecureBackup = self.otcliqueContext.sbd as! OTMockSecureBackup
        let correctInSOSRecoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(correctInSOSRecoveryKey, "correctInSOSRecoveryKey should not be nil")
        mockSBD.setRecoveryKey(recoveryKey: correctInSOSRecoveryKey!)
        newCliqueConfiguration.sbd = mockSBD
        self.otcliqueContext.sbd = mockSBD

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)
        self.verifyDatabaseMocks()

        XCTAssertThrowsError(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueConfiguration, recoveryKey: correctInSOSRecoveryKey!)) { error in
            let nsError = error as NSError
            XCTAssertEqual(nsError.domain, TrustedPeersHelperErrorDomain, "error domain should be TrustedPeersHelperErrorDomain")
            XCTAssertEqual(nsError.code, TrustedPeersHelperErrorCode.codeUntrustedRecoveryKeys.rawValue, "error code should be TrustedPeersHelperErrorCodeUntrustedRecoveryKeys")
        }
        self.assertEnters(context: newGuyContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(newCliqueConfiguration), "isRecoveryKeySet should not throw an error")

        var stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(newGuyContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            XCTAssertNil(egoSelf?["dynamicInfo"], "dynamicInfo should be nil")
            XCTAssertNil(egoSelf?["stableInfo"], "stableInfo should be nil")

            XCTAssertNotNil(dump!["modelRecoverySigningPublicKey"] as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertNotNil(dump!["modelRecoveryEncryptionPublicKey"] as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")
            let recoverySigningData = stableInfo!["recovery_signing_public_key"]
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 1, "should be 1 peer id")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }

    func testRKInSecureBackupRKInOctagonAndMisMatchCorrectInOctagon() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        var error: NSError?
        var recoveryString: String?
        XCTAssertNoThrow(recoveryString = cliqueBridge.createAndSetRecoveryKey(with: self.otcliqueContext, error: &error), "createAndSetRecoveryKey should not error")
        XCTAssertNil(error, "error should be nil")
        XCTAssertNotNil(recoveryString, "recoveryString should not be nil")

        OctagonSetSOSFeatureEnabled(true)

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueConfiguration = OTConfigurationContext()
        newCliqueConfiguration.context = newContextID
        newCliqueConfiguration.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueConfiguration.otControl = self.otControl
        let mockSBD: OTMockSecureBackup = self.otcliqueContext.sbd as! OTMockSecureBackup
        let mismatchedRecoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(mismatchedRecoveryKey, "mismatchedRecoveryKey should not be nil")
        mockSBD.setRecoveryKey(recoveryKey: mismatchedRecoveryKey!)
        newCliqueConfiguration.sbd = mockSBD
        self.otcliqueContext.sbd = mockSBD

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)
        self.verifyDatabaseMocks()

        let resetExpectation = self.expectation(description: "reset expected NOT to occur")
        resetExpectation.isInverted = true
        self.fakeCuttlefishServer.resetListener = {  _ in
            self.fakeCuttlefishServer.resetListener = nil
            resetExpectation.fulfill()
            return nil
        }

        SecCKKSSetTestSkipTLKShareHealing(true)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueConfiguration, recoveryKey: recoveryString!), "recoverWithRecoveryKey should not throw an error")
        self.wait(for: [resetExpectation], timeout: 2)

        self.assertEnters(context: newGuyContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(newCliqueConfiguration), "isRecoveryKeySet should not throw an error")
        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(self.otcliqueContext), "isRecoveryKeySet should not throw an error")

        self.sendContainerChangeWaitForFetch(context: newGuyContext)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        var stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(newGuyContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")
            let recoverySigningData = stableInfo!["recovery_signing_public_key"]
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")
            let recoverySigningData = stableInfo!["recovery_signing_public_key"]
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }

    func testRKInSecureBackupRKInOctagonAndIncorrectForBoth() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        var error: NSError?
        var recoveryString: String?
        XCTAssertNoThrow(recoveryString = cliqueBridge.createAndSetRecoveryKey(with: self.otcliqueContext, error: &error), "createAndSetRecoveryKey should not error")
        XCTAssertNil(error, "error should be nil")
        XCTAssertNotNil(recoveryString, "recoveryString should not be nil")

        OctagonSetSOSFeatureEnabled(true)

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueConfiguration = OTConfigurationContext()
        newCliqueConfiguration.context = newContextID
        newCliqueConfiguration.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueConfiguration.otControl = self.otControl
        let mockSBD: OTMockSecureBackup = self.otcliqueContext.sbd as! OTMockSecureBackup
        let mismatchedRecoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(mismatchedRecoveryKey, "mismatchedRecoveryKey should not be nil")
        mockSBD.setRecoveryKey(recoveryKey: mismatchedRecoveryKey!)

        newCliqueConfiguration.sbd = mockSBD
        self.otcliqueContext.sbd = mockSBD

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)
        self.verifyDatabaseMocks()

        let resetExpectation = self.expectation(description: "reset expected NOT to occur")
        resetExpectation.isInverted = true
        self.fakeCuttlefishServer.resetListener = {  _ in
            self.fakeCuttlefishServer.resetListener = nil
            resetExpectation.fulfill()
            return nil
        }

        let wrongRecoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(wrongRecoveryKey, "wrongRecoveryKey should not be nil")

        XCTAssertThrowsError(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueConfiguration, recoveryKey: wrongRecoveryKey!)) { error in
            let nsError = error as NSError
            XCTAssertEqual(nsError.domain, TrustedPeersHelperErrorDomain, "error domain should be TrustedPeersHelperErrorDomain")
            XCTAssertEqual(nsError.code, TrustedPeersHelperErrorCode.codeUntrustedRecoveryKeys.rawValue, "error code should be TrustedPeersHelperErrorCodeUntrustedRecoveryKeys")
        }
        self.wait(for: [resetExpectation], timeout: 2)

        self.assertEnters(context: newGuyContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(newCliqueConfiguration), "isRecoveryKeySet should not throw an error")

        var stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(newGuyContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            XCTAssertNil(egoSelf?["dynamicInfo"], "dynamicInfo should be nil")
            XCTAssertNil(egoSelf?["stableInfo"], "stableInfo should be nil")

            XCTAssertNotNil(dump!["modelRecoverySigningPublicKey"] as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertNotNil(dump!["modelRecoveryEncryptionPublicKey"] as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")
            let recoverySigningData = stableInfo!["recovery_signing_public_key"]
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 1, "should be 1 peer id")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }

    // MARK: tests for case device has KVS, Octagon and NO SOS
    func testNoRKInOctagonAndNoRKInSecurebackupNOSOS() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        OctagonSetSOSFeatureEnabled(false)

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueContext = OTConfigurationContext()
        newCliqueContext.context = newContextID
        newCliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueContext.otControl = self.otControl
        newCliqueContext.sbd = OTMockSecureBackup(bottleID: nil, entropy: nil)

        let mockSBD = self.otcliqueContext.sbd as! OTMockSecureBackup
        self.otcliqueContext.sbd = mockSBD
        newCliqueContext.sbd = mockSBD

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.assertEnters(context: newGuyContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let unsetRecoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(unsetRecoveryKey, "unsetRecoveryKey should not be nil")

        XCTAssertThrowsError(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueContext, recoveryKey: unsetRecoveryKey!)) { error in
            let nsError = error as NSError
            XCTAssertEqual(nsError.domain, OctagonErrorDomain, "error domain should be OctagonErrorDomain")
            XCTAssertEqual(nsError.code, OctagonError.noRecoveryKeyRegistered.rawValue, "error code should be OctagonErrorNoRecoveryKeyRegistered")
        }

        self.assertEnters(context: newGuyContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
    }

    func testRKInSecureBackupRKInOctagonAndMatchCorrectlyEnteredNOSOS() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()
        OctagonSetSOSFeatureEnabled(false)

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        var error: NSError?
        var recoveryString: String?
        XCTAssertNoThrow(recoveryString = cliqueBridge.createAndSetRecoveryKey(with: self.otcliqueContext, error: &error), "createAndSetRecoveryKey should not error")
        XCTAssertNil(error, "error should be nil")
        XCTAssertNotNil(recoveryString, "recoveryString should not be nil")

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueConfiguration = OTConfigurationContext()
        newCliqueConfiguration.context = newContextID
        newCliqueConfiguration.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueConfiguration.otControl = self.otControl

        let mockSBD = self.otcliqueContext.sbd as! OTMockSecureBackup
        mockSBD.setRecoveryKey(recoveryKey: recoveryString!)
        self.otcliqueContext.sbd = mockSBD
        newCliqueConfiguration.sbd = mockSBD

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)
        self.verifyDatabaseMocks()

        SecCKKSSetTestSkipTLKShareHealing(true)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueConfiguration, recoveryKey: recoveryString!), "recoverWithRecoveryKey should not throw an error")
        self.assertEnters(context: newGuyContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForFetch(context: newGuyContext)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(newCliqueConfiguration), "isRecoveryKeySet should not throw an error")

        var stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(newGuyContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")

            let recoverySigningData = stableInfo!["recovery_signing_public_key"]
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")
            let recoverySigningData = stableInfo!["recovery_signing_public_key"]
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }

    func testRKInSecureBackupRKInOctagonAndMatchIncorrectlyEnteredRKNOSOS() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()
        OctagonSetSOSFeatureEnabled(false)

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        var error: NSError?
        var recoveryString: String?
        XCTAssertNoThrow(recoveryString = cliqueBridge.createAndSetRecoveryKey(with: self.otcliqueContext, error: &error), "createAndSetRecoveryKey should not error")
        XCTAssertNil(error, "error should be nil")
        XCTAssertNotNil(recoveryString, "recoveryString should not be nil")

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueConfiguration = OTConfigurationContext()
        newCliqueConfiguration.context = newContextID
        newCliqueConfiguration.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueConfiguration.otControl = self.otControl

        let mockSBD = self.otcliqueContext.sbd as! OTMockSecureBackup
        mockSBD.setRecoveryKey(recoveryKey: recoveryString!)
        self.otcliqueContext.sbd = mockSBD
        newCliqueConfiguration.sbd = mockSBD

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)
        self.verifyDatabaseMocks()

        let resetExpectation = self.expectation(description: "reset expected NOT to occur")
        resetExpectation.isInverted = true
        self.fakeCuttlefishServer.resetListener = {  _ in
            self.fakeCuttlefishServer.resetListener = nil
            resetExpectation.fulfill()
            return nil
        }

        let wrongRecoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(wrongRecoveryKey, "wrongRecoveryKey should not be nil")

        XCTAssertThrowsError(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueConfiguration, recoveryKey: wrongRecoveryKey!)) { error in
            let nsError = error as NSError
            XCTAssertEqual(nsError.domain, TrustedPeersHelperErrorDomain, "error domain should be TrustedPeersHelperErrorDomain")
            XCTAssertEqual(nsError.code, TrustedPeersHelperErrorCode.codeUntrustedRecoveryKeys.rawValue, "error code should be TrustedPeersHelperErrorCodeUntrustedRecoveryKeys")
        }
        self.wait(for: [resetExpectation], timeout: 2)

        self.assertEnters(context: newGuyContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(newCliqueConfiguration), "isRecoveryKeySet should not throw an error")

        var stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(newGuyContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            XCTAssertNil(egoSelf?["dynamicInfo"], "dynamicInfo should be nil")
            XCTAssertNil(egoSelf?["stableInfo"], "stableInfo should be nil")

            XCTAssertNotNil(dump!["modelRecoverySigningPublicKey"] as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertNotNil(dump!["modelRecoveryEncryptionPublicKey"] as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")
            let recoverySigningData = stableInfo!["recovery_signing_public_key"]
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 1, "should be 1 peer id")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }

    func testRKInSecureBackupNORKInOctagonOctagonRecordsExistNOSOS() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()
        OctagonSetSOSFeatureEnabled(false)

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueConfiguration = OTConfigurationContext()
        newCliqueConfiguration.context = newContextID
        newCliqueConfiguration.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueConfiguration.otControl = self.otControl
        newCliqueConfiguration.overrideForJoinAfterRestore = true
        newCliqueConfiguration.octagonCapableRecordsExist = true

        let mockSBD = OTMockSecureBackup(bottleID: nil, entropy: nil)
        let recoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")
        mockSBD.setRecoveryKey(recoveryKey: recoveryKey!)
        newCliqueConfiguration.sbd = mockSBD
        self.otcliqueContext.sbd = mockSBD

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)
        self.verifyDatabaseMocks()

        let resetExpectation = self.expectation(description: "reset expected NOT to occur")
        resetExpectation.isInverted = true
        self.fakeCuttlefishServer.resetListener = {  _ in
            self.fakeCuttlefishServer.resetListener = nil
            resetExpectation.fulfill()
            return nil
        }

        let setRKExpectation = self.expectation(description: "setRecoveryKey expected NOT to occur")
        setRKExpectation.isInverted = true
        self.fakeCuttlefishServer.setRecoveryKeyListener = { _ in
            self.fakeCuttlefishServer.setRecoveryKeyListener = nil
            setRKExpectation.fulfill()
            return nil
        }

        XCTAssertThrowsError(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueConfiguration, recoveryKey: recoveryKey!)) { error in
            let nsError = error as NSError
            XCTAssertEqual(nsError.domain, OctagonErrorDomain, "error domain should be OctagonErrorDomain")
            XCTAssertEqual(nsError.code, OctagonError.recoverWithRecoveryKeyNotSupported.rawValue, "error code should be OctagonErrorRecoverWithRecoveryKeyNotSupported")
        }

        self.wait(for: [resetExpectation, setRKExpectation], timeout: 2)

        self.assertEnters(context: newGuyContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // RK still exists in SOS
        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(newCliqueConfiguration), "isRecoveryKeySet should not throw an error")
        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(self.otcliqueContext), "isRecoveryKeySet should not throw an error")

        var stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(newGuyContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            XCTAssertNil(egoSelf?["dynamicInfo"], "dynamicInfo should be nil")
            XCTAssertNil(egoSelf?["stableInfo"], "dynamicInfo should be nil")

            XCTAssertNil(dump!["modelRecoverySigningPublicKey"], "modelRecoverySigningPublicKey should be nil")
            XCTAssertNil(dump!["modelRecoveryEncryptionPublicKey"], "modelRecoveryEncryptionPublicKey should be nil")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should be nil")
            XCTAssertNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should be nil")

            XCTAssertNil(dump!["modelRecoverySigningPublicKey"], "modelRecoverySigningPublicKey should be nil")
            XCTAssertNil(dump!["modelRecoveryEncryptionPublicKey"], "modelRecoveryEncryptionPublicKey should be nil")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 1, "should be 1 peer id")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }

    func testRKInSecureBackupNORKInOctagonResetInlineNOSOS() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()
        OctagonSetSOSFeatureEnabled(false)

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueConfiguration = OTConfigurationContext()
        newCliqueConfiguration.context = newContextID
        newCliqueConfiguration.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueConfiguration.otControl = self.otControl
        newCliqueConfiguration.overrideForJoinAfterRestore = true

        let mockSBD = OTMockSecureBackup(bottleID: nil, entropy: nil)
        let correctInSOSRecoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(correctInSOSRecoveryKey, "correctInSOSRecoveryKey should not be nil")
        mockSBD.setRecoveryKey(recoveryKey: correctInSOSRecoveryKey!)
        newCliqueConfiguration.sbd = mockSBD
        self.otcliqueContext.sbd = mockSBD

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)
        self.verifyDatabaseMocks()

        let resetExpectation = self.expectation(description: "resetExpectation")
        self.fakeCuttlefishServer.resetListener = {  _ in
            self.fakeCuttlefishServer.resetListener = nil
            resetExpectation.fulfill()
            return nil
        }

        let setRKExpectation = self.expectation(description: "setRecoveryKey")
        self.fakeCuttlefishServer.setRecoveryKeyListener = { request in
            self.fakeCuttlefishServer.setRecoveryKeyListener = nil
            XCTAssertNotNil(request.recoverySigningPubKey, "signing public key should be present")
            XCTAssertNotNil(request.recoveryEncryptionPubKey, "encryption public key should be present")
            setRKExpectation.fulfill()
            return nil
        }

        SecCKKSSetTestSkipTLKShareHealing(true)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueConfiguration, recoveryKey: correctInSOSRecoveryKey!), "recoverWithRecoveryKey should not throw an error")

        self.wait(for: [resetExpectation, setRKExpectation], timeout: 10)

        self.assertEnters(context: newGuyContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        // RK is now set in SOS/Octagon
        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(newCliqueConfiguration), "isRecoveryKeySet should not throw an error")
        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(self.otcliqueContext), "isRecoveryKeySet should not throw an error")

        self.sendContainerChangeWaitForFetch(context: newGuyContext)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        var stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(newGuyContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")

            let recoverySigningData = stableInfo!["recovery_signing_public_key"]
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 1, "should be 1 peer id")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNil(dynamicInfo, "dynamicInfo should be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNil(stableInfo, "stableInfo should be nil")

            XCTAssertNil(stableInfo?["recovery_signing_public_key"], "recovery signing key should be nil")
            XCTAssertNil(stableInfo?["recovery_encryption_public_key"], "recovery encryption key should be nil")

            XCTAssertNotNil(dump?["modelRecoverySigningPublicKey"], "recovery signing key should not be nil")
            XCTAssertNotNil(dump?["modelRecoveryEncryptionPublicKey"], "recovery encryption key should not be nil")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }

    func testRKInSecureBackupNORKInOctagonIncorrectRKEnteredNOSOS() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()
        OctagonSetSOSFeatureEnabled(false)

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueConfiguration = OTConfigurationContext()
        newCliqueConfiguration.context = newContextID
        newCliqueConfiguration.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueConfiguration.otControl = self.otControl
        newCliqueConfiguration.overrideForJoinAfterRestore = true

        let mockSBD = OTMockSecureBackup(bottleID: nil, entropy: nil)
        let correctInSOSRecoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(correctInSOSRecoveryKey, "correctInSOSRecoveryKey should not be nil")
        mockSBD.setRecoveryKey(recoveryKey: correctInSOSRecoveryKey!)
        newCliqueConfiguration.sbd = mockSBD
        self.otcliqueContext.sbd = mockSBD

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)
        self.verifyDatabaseMocks()

        let resetExpectation = self.expectation(description: "reset expected NOT to occur")
        resetExpectation.isInverted = true
        self.fakeCuttlefishServer.resetListener = {  _ in
            self.fakeCuttlefishServer.resetListener = nil
            resetExpectation.fulfill()
            return nil
        }

        let setRKExpectation = self.expectation(description: "setRecoveryKey expected NOT to occur")
        setRKExpectation.isInverted = true
        self.fakeCuttlefishServer.setRecoveryKeyListener = { _ in
            self.fakeCuttlefishServer.setRecoveryKeyListener = nil
            setRKExpectation.fulfill()
            return nil
        }

        let wrongRecoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(wrongRecoveryKey, "wrongRecoveryKey should not be nil")

        XCTAssertThrowsError(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueConfiguration, recoveryKey: wrongRecoveryKey!)) { error in
            let nsError = error as NSError
            XCTAssertEqual(nsError.domain, OctagonErrorDomain, "error domain should be OctagonErrorDomain")
            XCTAssertEqual(nsError.code, OctagonError.secureBackupRestoreUsingRecoveryKeyFailed.rawValue, "error code should be OctagonErrorSecureBackupRestoreUsingRecoveryKeyFailed")
        }

        self.wait(for: [resetExpectation, setRKExpectation], timeout: 2)

        self.assertEnters(context: newGuyContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // RK is still set in SOS
        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(newCliqueConfiguration), "isRecoveryKeySet should not throw an error")
        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(self.otcliqueContext), "isRecoveryKeySet should not throw an error")

        var stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(newGuyContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            XCTAssertNil(egoSelf?["dynamicInfo"], "dynamicInfo should be nil")
            XCTAssertNil(egoSelf?["stableInfo"], "dynamicInfo should be nil")

            XCTAssertNil(dump!["modelRecoverySigningPublicKey"], "modelRecoverySigningPublicKey should be nil")
            XCTAssertNil(dump!["modelRecoveryEncryptionPublicKey"], "modelRecoveryEncryptionPublicKey should be nil")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNil(stableInfo?["recovery_signing_public_key"], "recovery_signing_public_key should be nil")
            XCTAssertNil(stableInfo?["recovery_encryption_public_key"], "recovery_encryption_public_key should be nil")

            XCTAssertNil(dump!["modelRecoverySigningPublicKey"], "modelRecoverySigningPublicKey should be nil")
            XCTAssertNil(dump!["modelRecoveryEncryptionPublicKey"], "modelRecoveryEncryptionPublicKey should be nil")

            let included = dynamicInfo?["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 1, "should be 1 peer id")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }

    func testNORKInSecureBackupRKInOctagonAndCorrectNOSOS() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()
        OctagonSetSOSFeatureEnabled(false)

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        var error: NSError?
        var recoveryString: String?
        XCTAssertNoThrow(recoveryString = cliqueBridge.createAndSetRecoveryKey(with: self.otcliqueContext, error: &error), "createAndSetRecoveryKey should not error")
        XCTAssertNil(error, "error should be nil")
        XCTAssertNotNil(recoveryString, "recoveryString should not be nil")

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueConfiguration = OTConfigurationContext()
        newCliqueConfiguration.context = newContextID
        newCliqueConfiguration.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueConfiguration.otControl = self.otControl
        let mockSBD: OTMockSecureBackup = self.otcliqueContext.sbd as! OTMockSecureBackup
        self.otcliqueContext.sbd = mockSBD
        newCliqueConfiguration.sbd = mockSBD

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)
        self.verifyDatabaseMocks()

        SecCKKSSetTestSkipTLKShareHealing(true)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueConfiguration, recoveryKey: recoveryString!), "recoverWithRecoveryKey should not throw an error")
        self.assertEnters(context: newGuyContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForFetch(context: newGuyContext)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(newCliqueConfiguration), "isRecoveryKeySet should not throw an error")

        var stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(newGuyContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")

            let recoverySigningData = stableInfo!["recovery_signing_public_key"]
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")
            let recoverySigningData = stableInfo!["recovery_signing_public_key"]
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }

    func testNORKInSecureBackupRKInOctagonAndIncorrectNOSOS() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()
        OctagonSetSOSFeatureEnabled(false)

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        var error: NSError?
        var recoveryString: String?
        XCTAssertNoThrow(recoveryString = cliqueBridge.createAndSetRecoveryKey(with: self.otcliqueContext, error: &error), "createAndSetRecoveryKey should not error")
        XCTAssertNil(error, "error should be nil")
        XCTAssertNotNil(recoveryString, "recoveryString should not be nil")

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueConfiguration = OTConfigurationContext()
        newCliqueConfiguration.context = newContextID
        newCliqueConfiguration.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueConfiguration.otControl = self.otControl
        let mockSBD: OTMockSecureBackup = self.otcliqueContext.sbd as! OTMockSecureBackup
        mockSBD.setRecoveryKey(recoveryKey: recoveryString!)
        self.otcliqueContext.sbd = mockSBD
        newCliqueConfiguration.sbd = mockSBD

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)
        self.verifyDatabaseMocks()

        let resetExpectation = self.expectation(description: "reset expected NOT to occur")
        resetExpectation.isInverted = true
        self.fakeCuttlefishServer.resetListener = {  _ in
            self.fakeCuttlefishServer.resetListener = nil
            resetExpectation.fulfill()
            return nil
        }

        let setRKExpectation = self.expectation(description: "setRecoveryKey expected NOT to occur")
        setRKExpectation.isInverted = true
        self.fakeCuttlefishServer.setRecoveryKeyListener = { _ in
            self.fakeCuttlefishServer.setRecoveryKeyListener = nil
            setRKExpectation.fulfill()
            return nil
        }

        let wrongRecoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(wrongRecoveryKey, "wrongRecoveryKey should not be nil")

        XCTAssertThrowsError(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueConfiguration, recoveryKey: wrongRecoveryKey!)) { error in
            let nsError = error as NSError
            XCTAssertEqual(nsError.domain, TrustedPeersHelperErrorDomain, "error domain should be TrustedPeersHelperErrorDomain")
            XCTAssertEqual(nsError.code, TrustedPeersHelperErrorCode.codeUntrustedRecoveryKeys.rawValue, "error code should be TrustedPeersHelperErrorCodeUntrustedRecoveryKeys")
        }

        self.wait(for: [resetExpectation, setRKExpectation], timeout: 2)

        self.assertEnters(context: newGuyContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(newCliqueConfiguration), "isRecoveryKeySet should not throw an error")

        var stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(newGuyContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            XCTAssertNil(egoSelf?["dynamicInfo"], "dynamicInfo should be nil")
            XCTAssertNil(egoSelf?["stableInfo"], "stableInfo should be nil")

            XCTAssertNotNil(dump!["modelRecoverySigningPublicKey"] as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertNotNil(dump!["modelRecoveryEncryptionPublicKey"] as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")
            let recoverySigningData = stableInfo!["recovery_signing_public_key"]
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 1, "should be 1 peer id")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }

    func testRKInSecureBackupRKInOctagonAndMisMatchCorrectInSOSNOSOS() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()
        OctagonSetSOSFeatureEnabled(false)

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        var error: NSError?
        var recoveryString: String?
        XCTAssertNoThrow(recoveryString = cliqueBridge.createAndSetRecoveryKey(with: self.otcliqueContext, error: &error), "createAndSetRecoveryKey should not error")
        XCTAssertNil(error, "error should be nil")
        XCTAssertNotNil(recoveryString, "recoveryString should not be nil")

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueConfiguration = OTConfigurationContext()
        newCliqueConfiguration.context = newContextID
        newCliqueConfiguration.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueConfiguration.otControl = self.otControl
        newCliqueConfiguration.octagonCapableRecordsExist = true

        let mockSBD: OTMockSecureBackup = self.otcliqueContext.sbd as! OTMockSecureBackup
        let correctInSOSRecoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(correctInSOSRecoveryKey, "correctInSOSRecoveryKey should not be nil")
        mockSBD.setRecoveryKey(recoveryKey: correctInSOSRecoveryKey!)
        newCliqueConfiguration.sbd = mockSBD
        self.otcliqueContext.sbd = mockSBD

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)
        self.verifyDatabaseMocks()

        let resetExpectation = self.expectation(description: "reset expected NOT to occur")
        resetExpectation.isInverted = true
        self.fakeCuttlefishServer.resetListener = {  _ in
            self.fakeCuttlefishServer.resetListener = nil
            resetExpectation.fulfill()
            return nil
        }

        let setRKExpectation = self.expectation(description: "setRecoveryKey expected NOT to occur")
        setRKExpectation.isInverted = true
        self.fakeCuttlefishServer.setRecoveryKeyListener = { _ in
            self.fakeCuttlefishServer.setRecoveryKeyListener = nil
            setRKExpectation.fulfill()
            return nil
        }

        XCTAssertThrowsError(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueConfiguration, recoveryKey: correctInSOSRecoveryKey!)) { error in
            let nsError = error as NSError
            XCTAssertEqual(nsError.domain, TrustedPeersHelperErrorDomain, "error domain should be TrustedPeersHelperErrorDomain")
            XCTAssertEqual(nsError.code, TrustedPeersHelperErrorCode.codeUntrustedRecoveryKeys.rawValue, "error code should be TrustedPeersHelperErrorCodeUntrustedRecoveryKeys")
        }

        self.wait(for: [resetExpectation, setRKExpectation], timeout: 2)

        self.assertEnters(context: newGuyContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(newCliqueConfiguration), "isRecoveryKeySet should not throw an error")

        var stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(newGuyContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            XCTAssertNil(egoSelf?["dynamicInfo"], "dynamicInfo should be nil")
            XCTAssertNil(egoSelf?["stableInfo"], "stableInfo should be nil")

            XCTAssertNotNil(dump!["modelRecoverySigningPublicKey"] as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertNotNil(dump!["modelRecoveryEncryptionPublicKey"] as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")
            let recoverySigningData = stableInfo!["recovery_signing_public_key"]
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 1, "should be 1 peer id")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }

    func testRKInSecureBackupRKInOctagonAndMisMatchCorrectInOctagonNOSOS() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()
        OctagonSetSOSFeatureEnabled(false)

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        var error: NSError?
        var recoveryString: String?
        XCTAssertNoThrow(recoveryString = cliqueBridge.createAndSetRecoveryKey(with: self.otcliqueContext, error: &error), "createAndSetRecoveryKey should not error")
        XCTAssertNil(error, "error should be nil")
        XCTAssertNotNil(recoveryString, "recoveryString should not be nil")

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueConfiguration = OTConfigurationContext()
        newCliqueConfiguration.context = newContextID
        newCliqueConfiguration.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueConfiguration.otControl = self.otControl
        let mockSBD: OTMockSecureBackup = self.otcliqueContext.sbd as! OTMockSecureBackup
        let mismatchedRecoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(mismatchedRecoveryKey, "mismatchedRecoveryKey should not be nil")
        mockSBD.setRecoveryKey(recoveryKey: mismatchedRecoveryKey!)
        newCliqueConfiguration.sbd = mockSBD
        self.otcliqueContext.sbd = mockSBD

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)
        self.verifyDatabaseMocks()

        let resetExpectation = self.expectation(description: "reset expected NOT to occur")
        resetExpectation.isInverted = true
        self.fakeCuttlefishServer.resetListener = {  _ in
            self.fakeCuttlefishServer.resetListener = nil
            resetExpectation.fulfill()
            return nil
        }

        let setRKExpectation = self.expectation(description: "setRecoveryKey expected NOT to occur")
        setRKExpectation.isInverted = true
        self.fakeCuttlefishServer.setRecoveryKeyListener = { _ in
            self.fakeCuttlefishServer.setRecoveryKeyListener = nil
            setRKExpectation.fulfill()
            return nil
        }

        SecCKKSSetTestSkipTLKShareHealing(true)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueConfiguration, recoveryKey: recoveryString!), "recoverWithRecoveryKey should not throw an error")
        self.wait(for: [resetExpectation, setRKExpectation], timeout: 2)

        self.assertEnters(context: newGuyContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(newCliqueConfiguration), "isRecoveryKeySet should not throw an error")
        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(self.otcliqueContext), "isRecoveryKeySet should not throw an error")

        self.sendContainerChangeWaitForFetch(context: newGuyContext)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        var stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(newGuyContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")
            let recoverySigningData = stableInfo!["recovery_signing_public_key"]
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")
            let recoverySigningData = stableInfo!["recovery_signing_public_key"]
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }

    func testRKInSecureBackupRKInOctagonAndIncorrectForBothNOSOS() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()
        OctagonSetSOSFeatureEnabled(false)

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        var error: NSError?
        var recoveryString: String?
        XCTAssertNoThrow(recoveryString = cliqueBridge.createAndSetRecoveryKey(with: self.otcliqueContext, error: &error), "createAndSetRecoveryKey should not error")
        XCTAssertNil(error, "error should be nil")
        XCTAssertNotNil(recoveryString, "recoveryString should not be nil")

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueConfiguration = OTConfigurationContext()
        newCliqueConfiguration.context = newContextID
        newCliqueConfiguration.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueConfiguration.otControl = self.otControl
        let mockSBD: OTMockSecureBackup = self.otcliqueContext.sbd as! OTMockSecureBackup
        let mismatchedRecoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(mismatchedRecoveryKey, "mismatchedRecoveryKey should not be nil")
        mockSBD.setRecoveryKey(recoveryKey: mismatchedRecoveryKey!)

        newCliqueConfiguration.sbd = mockSBD
        self.otcliqueContext.sbd = mockSBD

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)
        self.verifyDatabaseMocks()

        let setRKExpectation = self.expectation(description: "setRecoveryKey expected NOT to occur")
        setRKExpectation.isInverted = true
        self.fakeCuttlefishServer.setRecoveryKeyListener = { _ in
            self.fakeCuttlefishServer.setRecoveryKeyListener = nil
            setRKExpectation.fulfill()
            return nil
        }

        let resetExpectation = self.expectation(description: "reset expected NOT to occur")
        resetExpectation.isInverted = true
        self.fakeCuttlefishServer.resetListener = {  _ in
            self.fakeCuttlefishServer.resetListener = nil
            resetExpectation.fulfill()
            return nil
        }

        let wrongRecoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(wrongRecoveryKey, "wrongRecoveryKey should not be nil")

        XCTAssertThrowsError(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueConfiguration, recoveryKey: wrongRecoveryKey!)) { error in
            let nsError = error as NSError
            XCTAssertEqual(nsError.domain, TrustedPeersHelperErrorDomain, "error domain should be TrustedPeersHelperErrorDomain")
            XCTAssertEqual(nsError.code, TrustedPeersHelperErrorCode.codeUntrustedRecoveryKeys.rawValue, "error code should be TrustedPeersHelperErrorCodeUntrustedRecoveryKeys")
        }
        self.wait(for: [resetExpectation, setRKExpectation], timeout: 2)

        self.assertEnters(context: newGuyContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(newCliqueConfiguration), "isRecoveryKeySet should not throw an error")

        var stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(newGuyContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            XCTAssertNil(egoSelf?["dynamicInfo"], "dynamicInfo should be nil")
            XCTAssertNil(egoSelf?["stableInfo"], "stableInfo should be nil")

            XCTAssertNotNil(dump!["modelRecoverySigningPublicKey"] as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertNotNil(dump!["modelRecoveryEncryptionPublicKey"] as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")

            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")
            let recoverySigningData = stableInfo!["recovery_signing_public_key"]

            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 1, "should be 1 peer id")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }

    // MARK: tests for case device has Octagon and NO KVS and NO SOS
    func testNoRKInOctagonAndNoRKInSecurebackupNOSOSNOKVS() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        OctagonSetSOSFeatureEnabled(false)

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueContext = OTConfigurationContext()
        newCliqueContext.context = newContextID
        newCliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueContext.otControl = self.otControl
        newCliqueContext.sbd = OTMockSecureBackup(bottleID: nil, entropy: nil)

        let mockSBD = self.otcliqueContext.sbd as! OTMockSecureBackup
        let error = NSError(domain: "KVSErrorDomain", code: -1)
        mockSBD.setExpectKVSError(error)
        self.otcliqueContext.sbd = mockSBD
        newCliqueContext.sbd = mockSBD

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.assertEnters(context: newGuyContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let unsetRecoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(unsetRecoveryKey, "unsetRecoveryKey should not be nil")

        XCTAssertThrowsError(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueContext, recoveryKey: unsetRecoveryKey!)) { error in
            let nsError = error as NSError
            XCTAssertEqual(nsError.domain, OctagonErrorDomain, "error domain should be OctagonErrorDomain")
            XCTAssertEqual(nsError.code, OctagonError.noRecoveryKeyRegistered.rawValue, "error code should be OctagonErrorNoRecoveryKeyRegistered")
        }

        self.assertEnters(context: newGuyContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
    }

    func testNORKInSecureBackupRKInOctagonAndCorrectNOSOSNOKVS() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()
        OctagonSetSOSFeatureEnabled(false)

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        var error: NSError?
        var recoveryString: String?
        XCTAssertNoThrow(recoveryString = cliqueBridge.createAndSetRecoveryKey(with: self.otcliqueContext, error: &error), "createAndSetRecoveryKey should not error")
        XCTAssertNil(error, "error should be nil")
        XCTAssertNotNil(recoveryString, "recoveryString should not be nil")

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueConfiguration = OTConfigurationContext()
        newCliqueConfiguration.context = newContextID
        newCliqueConfiguration.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueConfiguration.otControl = self.otControl

        let mockSBD: OTMockSecureBackup = self.otcliqueContext.sbd as! OTMockSecureBackup
        let kvsError = NSError(domain: "KVSErrorDomain", code: -1)
        mockSBD.setExpectKVSError(kvsError)

        self.otcliqueContext.sbd = mockSBD
        newCliqueConfiguration.sbd = mockSBD

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)
        self.verifyDatabaseMocks()

        SecCKKSSetTestSkipTLKShareHealing(true)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueConfiguration, recoveryKey: recoveryString!), "recoverWithRecoveryKey should not throw an error")
        self.assertEnters(context: newGuyContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForFetch(context: newGuyContext)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(newCliqueConfiguration), "isRecoveryKeySet should not throw an error")

        var stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(newGuyContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")

            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")
            let recoverySigningData = stableInfo!["recovery_signing_public_key"]

            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")

            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")
            let recoverySigningData = stableInfo!["recovery_signing_public_key"]

            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }

    func testNORKInSecureBackupRKInOctagonAndInCorrectNOSOSNOKVS() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()
        OctagonSetSOSFeatureEnabled(false)

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        var error: NSError?
        var recoveryString: String?
        XCTAssertNoThrow(recoveryString = cliqueBridge.createAndSetRecoveryKey(with: self.otcliqueContext, error: &error), "createAndSetRecoveryKey should not error")
        XCTAssertNil(error, "error should be nil")
        XCTAssertNotNil(recoveryString, "recoveryString should not be nil")

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueConfiguration = OTConfigurationContext()
        newCliqueConfiguration.context = newContextID
        newCliqueConfiguration.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueConfiguration.otControl = self.otControl
        let mockSBD: OTMockSecureBackup = self.otcliqueContext.sbd as! OTMockSecureBackup
        let kvsError = NSError(domain: "KVSErrorDomain", code: -1)
        mockSBD.setExpectKVSError(kvsError)

        self.otcliqueContext.sbd = mockSBD
        newCliqueConfiguration.sbd = mockSBD

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)
        self.verifyDatabaseMocks()

        let setRKExpectation = self.expectation(description: "setRecoveryKey expected NOT to occur")
        setRKExpectation.isInverted = true
        self.fakeCuttlefishServer.setRecoveryKeyListener = { _ in
            self.fakeCuttlefishServer.setRecoveryKeyListener = nil
            setRKExpectation.fulfill()
            return nil
        }

        let resetExpectation = self.expectation(description: "reset expected NOT to occur")
        resetExpectation.isInverted = true
        self.fakeCuttlefishServer.resetListener = {  _ in
            self.fakeCuttlefishServer.resetListener = nil
            resetExpectation.fulfill()
            return nil
        }

        let wrongRecoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(wrongRecoveryKey, "wrongRecoveryKey should not be nil")

        XCTAssertThrowsError(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueConfiguration, recoveryKey: wrongRecoveryKey!)) { error in
            let nsError = error as NSError
            XCTAssertEqual(nsError.domain, TrustedPeersHelperErrorDomain, "error domain should be TrustedPeersHelperErrorDomain")
            XCTAssertEqual(nsError.code, TrustedPeersHelperErrorCode.codeUntrustedRecoveryKeys.rawValue, "error code should be TrustedPeersHelperErrorCodeUntrustedRecoveryKeys")
        }
        self.wait(for: [setRKExpectation, resetExpectation], timeout: 2)

        self.assertEnters(context: newGuyContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(newCliqueConfiguration), "isRecoveryKeySet should not throw an error")

        var stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(newGuyContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            XCTAssertNil(egoSelf?["dynamicInfo"], "dynamicInfo should be nil")
            XCTAssertNil(egoSelf?["stableInfo"], "stableInfo should be nil")

            XCTAssertNotNil(dump!["modelRecoverySigningPublicKey"] as! Data, "modelRecoverySigningPublicKey should not be nil")
            XCTAssertNotNil(dump!["modelRecoveryEncryptionPublicKey"] as! Data, "modelRecoveryEncryptionPublicKey should not be nil")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")

            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")
            let recoverySigningData = stableInfo!["recovery_signing_public_key"]

            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 1, "should be 1 peer id")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }

    func testPreflightRecoveryKeySuccess() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.manager.setSOSEnabledForPlatformFlag(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        let recoveryKey = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        self.manager.setSOSEnabledForPlatformFlag(true)

        let createRecoveryExpectation = self.expectation(description: "createRecoveryExpectation returns")
        self.manager.createRecoveryKey(self.otcontrolArgumentsFor(context: establishContext), recoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            createRecoveryExpectation.fulfill()
        }
        self.wait(for: [createRecoveryExpectation], timeout: 10)

        try self.putRecoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))
        self.sendContainerChangeWaitForFetch(context: establishContext)

        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        XCTAssertNotNil(cliqueBridge, "cliqueBridge should not be nil")

        let fetchExpectation = self.expectation(description: "fetch expectation")
        self.fakeCuttlefishServer.fetchChangesListener = { _ in

            fetchExpectation.fulfill()
            return nil
        }

        var localError: NSError?
        let result = cliqueBridge.preflightRecoveryKey(self.otcliqueContext, recoveryKey: recoveryKey, error: &localError)
        self.wait(for: [fetchExpectation], timeout: 10)

        XCTAssertTrue(result, "recovery key should be correct")
        XCTAssertNil(localError, "error should be nil")
    }

    func testPreflightRecoveryKeyFailureIncorrectRecoveryKey() throws {
        try self.skipOnRecoveryKeyNotSupported()

        self.manager.setSOSEnabledForPlatformFlag(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        let recoveryKey = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        self.manager.setSOSEnabledForPlatformFlag(true)

        let createRecoveryExpectation = self.expectation(description: "createRecoveryExpectation returns")
        self.manager.createRecoveryKey(self.otcontrolArgumentsFor(context: establishContext), recoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            createRecoveryExpectation.fulfill()
        }
        self.wait(for: [createRecoveryExpectation], timeout: 10)

        try self.putRecoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))
        self.sendContainerChangeWaitForFetch(context: establishContext)

        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        XCTAssertNotNil(cliqueBridge, "cliqueBridge should not be nil")

        let fetchExpectation = self.expectation(description: "fetch expectation")
        self.fakeCuttlefishServer.fetchChangesListener = { _ in

            fetchExpectation.fulfill()
            return nil
        }

        let wrongRecoveryKey = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        XCTAssertNotNil(wrongRecoveryKey, "wrongRecoveryKey should not be nil")

        var localError: NSError?
        let result = cliqueBridge.preflightRecoveryKey(self.otcliqueContext, recoveryKey: wrongRecoveryKey, error: &localError)
        self.wait(for: [fetchExpectation], timeout: 10)

        XCTAssertFalse(result, "recovery key should be incorrect")
        XCTAssertNotNil(localError, "error should not be nil")
        XCTAssertEqual(localError!.domain, TrustedPeersHelperErrorDomain, "error domain should be TrustedPeersHelperErrorDomain")
        XCTAssertEqual(localError!.code, TrustedPeersHelperErrorCode.recoveryKeyIsNotCorrect.rawValue, "error code should be recoveryKeyIsNotCorrect")
    }

    func testPreflightRecoveryKeyFailureNotEnrolled() throws {
        try self.skipOnRecoveryKeyNotSupported()

        self.manager.setSOSEnabledForPlatformFlag(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        self.manager.setSOSEnabledForPlatformFlag(true)

        self.sendContainerChangeWaitForFetch(context: establishContext)

        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        XCTAssertNotNil(cliqueBridge, "cliqueBridge should not be nil")

        let fetchExpectation = self.expectation(description: "fetch expectation")
        self.fakeCuttlefishServer.fetchChangesListener = { _ in

            fetchExpectation.fulfill()
            return nil
        }

        let notEnrolledRK = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        XCTAssertNotNil(notEnrolledRK, "notEnrolledRK should not be nil")

        var localError: NSError?
        let result = cliqueBridge.preflightRecoveryKey(self.otcliqueContext, recoveryKey: notEnrolledRK, error: &localError)
        self.wait(for: [fetchExpectation], timeout: 10)

        XCTAssertFalse(result, "recovery key should be incorrect")
        XCTAssertNotNil(localError, "error should not be nil")
        XCTAssertEqual(localError!.domain, TrustedPeersHelperErrorDomain, "error domain should be TrustedPeersHelperErrorDomain")
        XCTAssertEqual(localError!.code, TrustedPeersHelperErrorCode.codeNotEnrolled.rawValue, "error code should be recovery key not enrolled")
    }

    func testPreflightRecoveryKeyFailureErrorFetching() throws {
        try self.skipOnRecoveryKeyNotSupported()

        self.manager.setSOSEnabledForPlatformFlag(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        let recoveryKey = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        self.manager.setSOSEnabledForPlatformFlag(true)

        let createRecoveryExpectation = self.expectation(description: "createRecoveryExpectation returns")
        self.manager.createRecoveryKey(self.otcontrolArgumentsFor(context: establishContext), recoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            createRecoveryExpectation.fulfill()
        }
        self.wait(for: [createRecoveryExpectation], timeout: 10)

        try self.putRecoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))
        self.sendContainerChangeWaitForFetch(context: establishContext)

        // Now, join from a new device
        let recoveryContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        recoveryContext.startOctagonStateMachine()
        self.assertEnters(context: recoveryContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: recoveryContext)

        let joinWithRecoveryKeyExpectation = self.expectation(description: "joinWithRecoveryKey callback occurs")
        recoveryContext.join(withRecoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithRecoveryKeyExpectation], timeout: 20)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.sendContainerChangeWaitForFetch(context: recoveryContext)

        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        XCTAssertNotNil(cliqueBridge, "cliqueBridge should not be nil")

        let rolledRecoveryKey = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        XCTAssertNotNil(rolledRecoveryKey, "wrongRecoveryKey should not be nil")

        let rolledRecoveryExpectation = self.expectation(description: "rolledRecoveryExpectation returns")
        self.manager.createRecoveryKey(self.otcontrolArgumentsFor(context: establishContext), recoveryKey: rolledRecoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            rolledRecoveryExpectation.fulfill()
        }
        self.wait(for: [rolledRecoveryExpectation], timeout: 10)

        let ckError = FakeCuttlefishServer.makeCloudKitCuttlefishError(code: .transactionalFailure)
        self.fakeCuttlefishServer.nextFetchErrors.append(ckError)
        self.fakeCuttlefishServer.nextFetchErrors.append(ckError)
        self.fakeCuttlefishServer.nextFetchErrors.append(ckError)
        self.fakeCuttlefishServer.nextFetchErrors.append(ckError)
        self.fakeCuttlefishServer.nextFetchErrors.append(ckError)
        self.fakeCuttlefishServer.nextFetchErrors.append(ckError)

        let fetchExpectation = self.expectation(description: "fetch expectation")
        fetchExpectation.expectedFulfillmentCount = 6
        self.fakeCuttlefishServer.fetchChangesListener = { _ in
            fetchExpectation.fulfill()
            return nil
        }

        var localError: NSError?
        let result = cliqueBridge.preflightRecoveryKey(self.otcliqueContext, recoveryKey: rolledRecoveryKey, error: &localError)
        self.wait(for: [fetchExpectation], timeout: 10)

        XCTAssertFalse(result, "recovery key should be incorrect")
        XCTAssertNotNil(localError, "error should not be nil")
        XCTAssertEqual(localError!.domain, CKErrorDomain, "error domain should be CKErrorDomain")
        XCTAssertEqual(localError!.code, 15, "error code should be 15")
    }

    func testPreflightRecoveryKeyFailureMalformed() throws {
        try self.skipOnRecoveryKeyNotSupported()

        self.manager.setSOSEnabledForPlatformFlag(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        let recoveryKey = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        self.manager.setSOSEnabledForPlatformFlag(true)

        let createRecoveryExpectation = self.expectation(description: "createRecoveryExpectation returns")
        self.manager.createRecoveryKey(self.otcontrolArgumentsFor(context: establishContext), recoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            createRecoveryExpectation.fulfill()
        }
        self.wait(for: [createRecoveryExpectation], timeout: 10)

        try self.putRecoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))
        self.sendContainerChangeWaitForFetch(context: establishContext)

        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        XCTAssertNotNil(cliqueBridge, "cliqueBridge should not be nil")

        let fetchExpectation = self.expectation(description: "fetch expectation")
        fetchExpectation.isInverted = true
        self.fakeCuttlefishServer.fetchChangesListener = { _ in
            fetchExpectation.fulfill()
            return nil
        }

        var localError: NSError?
        let result = cliqueBridge.preflightRecoveryKey(self.otcliqueContext, recoveryKey: "malformed recovery key", error: &localError)
        self.wait(for: [fetchExpectation], timeout: 2)
        XCTAssertFalse(result, "preflight recovery key should fail")
        XCTAssertNotNil(localError, "error should not be nil")
        XCTAssertEqual(localError?.domain, OctagonErrorDomain, "error domain should be OctagonErrorDomain")
        XCTAssertEqual(localError?.code, OctagonError.recoveryKeyMalformed.rawValue, "error domain should be OctagonErrorRecoveryKeyMalformed")
    }

    func testRegisterRecoveryKeyAPI() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        let recoveryString = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(recoveryString, "recoveryString should not be nil")

        XCTAssertNoThrow(try cliqueBridge.registerRecoveryKey(with: self.otcliqueContext, recoveryKey: recoveryString!), "registerRecoveryKey should not error")

        OctagonSetSOSFeatureEnabled(true)

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueConfiguration = OTConfigurationContext()
        newCliqueConfiguration.context = newContextID
        newCliqueConfiguration.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueConfiguration.otControl = self.otControl

        let mockSBD = self.otcliqueContext.sbd as! OTMockSecureBackup
        mockSBD.setRecoveryKey(recoveryKey: recoveryString!)
        self.otcliqueContext.sbd = mockSBD
        newCliqueConfiguration.sbd = mockSBD

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)
        self.verifyDatabaseMocks()

        SecCKKSSetTestSkipTLKShareHealing(true)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueConfiguration, recoveryKey: recoveryString!), "recoverWithRecoveryKey should not throw an error")
        self.assertEnters(context: newGuyContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForFetch(context: newGuyContext)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(newCliqueConfiguration), "isRecoveryKeySet should not throw an error")

        var stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(newGuyContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")

            let recoverySigningData = stableInfo!["recovery_signing_public_key"]
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNotNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should not be nil")
            let recoverySigningData = stableInfo!["recovery_signing_public_key"]
            XCTAssertNotNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should not be nil")
            let recoveryEncryptionData = stableInfo!["recovery_encryption_public_key"]

            XCTAssertEqual(dump!["modelRecoverySigningPublicKey"] as! Data, recoverySigningData as! Data, "modelRecoverySigningPublicKey should not be empty")
            XCTAssertEqual(dump!["modelRecoveryEncryptionPublicKey"] as! Data, recoveryEncryptionData as! Data, "modelRecoveryEncryptionPublicKey should not be empty")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }

    func testRegisterRecoveryKeyAPIFail() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        let recoveryString = "bad recovery key"
        XCTAssertNotNil(recoveryString, "recoveryString should not be nil")

        XCTAssertThrowsError(try cliqueBridge.registerRecoveryKey(with: self.otcliqueContext, recoveryKey: recoveryString), "registerRecoveryKey should error")

        OctagonSetSOSFeatureEnabled(true)

        let newContextID = "joiningWithRecoveryKeyContext"
        let newCliqueConfiguration = OTConfigurationContext()
        newCliqueConfiguration.context = newContextID
        newCliqueConfiguration.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        newCliqueConfiguration.otControl = self.otControl

        let newGuyContext = self.makeInitiatorContext(contextID: newContextID, authKitAdapter: self.mockAuthKit2)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)
        self.verifyDatabaseMocks()

        SecCKKSSetTestSkipTLKShareHealing(true)

        XCTAssertThrowsError(try OctagonTrustCliqueBridge.recover(withRecoveryKey: newCliqueConfiguration, recoveryKey: recoveryString), "recoverWithRecoveryKey should throw an error")
        self.assertEnters(context: newGuyContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        XCTAssertThrowsError(try OctagonTrustCliqueBridge.isRecoveryKeySet(newCliqueConfiguration), "isRecoveryKeySet should throw an error")

        var stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(newGuyContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNil(stableInfo, "stableInfo should not be nil")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should be nil")
            XCTAssertNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should be nil")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 1, "should be 1 peer id")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }

    // MARK: tests for case device SOS not enabled, in SOS and not in SOS

    func testRegisterRecoveryKeyAPIDeviceNotInSOSAndSOSNotEnabled() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        SecCKKSSetTestSkipTLKShareHealing(true)

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        let recoveryString = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(recoveryString, "recoveryString should not be nil")

        self.mockSOSAdapter?.circleStatus = SOSCCStatus(kSOSCCNotInCircle)
        XCTAssertNoThrow(try cliqueBridge.registerRecoveryKey(with: self.otcliqueContext, recoveryKey: recoveryString!), "registerRecoveryKey should not error")
        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(self.otcliqueContext), "isRecoveryKeySet should not throw an error")
    }

    func testRegisterRecoveryKeyAPIDeviceInSOSAndSOSNotEnabled() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        SecCKKSSetTestSkipTLKShareHealing(true)

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        let recoveryString = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(recoveryString, "recoveryString should not be nil")

        self.mockSOSAdapter?.circleStatus = SOSCCStatus(kSOSCCInCircle)
        XCTAssertNoThrow(try cliqueBridge.registerRecoveryKey(with: self.otcliqueContext, recoveryKey: recoveryString!), "registerRecoveryKey should not error")
        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(self.otcliqueContext), "isRecoveryKeySet should not throw an error")
    }

    // MARK: tests for case device SOS enabled, in SOS and not in SOS

    func testRegisterRecoveryKeyAPIDeviceNotInSOSAndSOSEnabled() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        SecCKKSSetTestSkipTLKShareHealing(true)

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        let recoveryString = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(recoveryString, "recoveryString should not be nil")

        OctagonSetSOSFeatureEnabled(true)
        enableSOSCompatibilityForTests()

        self.mockSOSAdapter?.circleStatus = SOSCCStatus(kSOSCCNotInCircle)
        XCTAssertNoThrow(try cliqueBridge.registerRecoveryKey(with: self.otcliqueContext, recoveryKey: recoveryString!), "registerRecoveryKey should not error")
        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(self.otcliqueContext), "isRecoveryKeySet should not throw an error")
    }

    func testRegisterRecoveryKeyAPIDeviceInSOSAndSOSEnabled() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        SecCKKSSetTestSkipTLKShareHealing(true)

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        let recoveryString = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(recoveryString, "recoveryString should not be nil")

        OctagonSetSOSFeatureEnabled(true)
        enableSOSCompatibilityForTests()

        self.mockSOSAdapter?.circleStatus = SOSCCStatus(kSOSCCInCircle)
        XCTAssertNoThrow(try cliqueBridge.registerRecoveryKey(with: self.otcliqueContext, recoveryKey: recoveryString!), "registerRecoveryKey should not error")

        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(self.otcliqueContext), "isRecoveryKeySet should not throw an error")
    }

    func testRemoveRecoveryKeyAndAttemptReAdd() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        var error: NSError?
        var recoveryString: String?
        XCTAssertNoThrow(recoveryString = cliqueBridge.createAndSetRecoveryKey(with: self.otcliqueContext, error: &error), "createAndSetRecoveryKey should not error")
        XCTAssertNil(error, "error should be nil")
        XCTAssertNotNil(recoveryString, "recoveryString should not be nil")

        let mockSBD = self.otcliqueContext.sbd as! OTMockSecureBackup
        mockSBD.setRecoveryKey(recoveryKey: recoveryString!)
        self.otcliqueContext.sbd = mockSBD

        // Recovery Key should be set
        XCTAssertNoThrow(try OctagonTrustCliqueBridge.isRecoveryKeySet(self.otcliqueContext), "recovery key should be set")

        let removeExpectation = self.expectation(description: "removeExpectation")
        self.fakeCuttlefishServer.removeRecoveryKeyListener = { _ in
            self.fakeCuttlefishServer.removeRecoveryKeyListener = nil
            removeExpectation.fulfill()
            return nil
        }

        XCTAssertNoThrow(try cliqueBridge.removeRecoveryKey(with: self.otcliqueContext), "removeRecoveryKey should not error")
        mockSBD.setRecoveryKey(recoveryKey: nil)

        self.wait(for: [removeExpectation], timeout: 10)

        XCTAssertNil(error, "error should be nil")
        XCTAssertThrowsError(try OctagonTrustCliqueBridge.isRecoveryKeySet(self.otcliqueContext), "recovery key should not be set")

        let stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should be nil")
            XCTAssertNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should be nil")

            XCTAssertNil(dump!["modelRecoverySigningPublicKey"], "modelRecoverySigningPublicKey should be nil")
            XCTAssertNil(dump!["modelRecoveryEncryptionPublicKey"], "modelRecoveryEncryptionPublicKey should be nil")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 1, "should be 1 peer id")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        let reRegisterRecoveryKeyExpectation = self.expectation(description: "registerRecoveryKey callback occurs")
        XCTAssertThrowsError(try cliqueBridge.registerRecoveryKey(with: self.otcliqueContext, recoveryKey: recoveryString!), "registerRecoveryKey should throw error") { error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error as NSError).domain, TrustedPeersHelperErrorDomain, "error domain should be com.apple.security.trustedpeers.container")
            XCTAssertEqual((error as NSError).code, TrustedPeersHelperErrorCode.codeUntrustedRecoveryKeys.rawValue, "error code should be untrusted recovery keys")
            reRegisterRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [reRegisterRecoveryKeyExpectation], timeout: 10)

    }

}
#endif
