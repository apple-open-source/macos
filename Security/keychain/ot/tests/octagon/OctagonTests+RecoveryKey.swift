#if OCTAGON

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

        // Set this to what it normally is. Each test can muck with it, if they like
        #if os(macOS) || os(iOS)
        OctagonSetPlatformSupportsSOS(true)
        self.manager.setSOSEnabledForPlatformFlag(true)
        #else
        self.manager.setSOSEnabledForPlatformFlag(false)
        OctagonSetPlatformSupportsSOS(false)
        #endif
    }

    func testSetRecoveryKey() throws {
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

        let recoveryKey = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")
        self.manager.setSOSEnabledForPlatformFlag(true)

        let createKeyExpectation = self.expectation(description: "createKeyExpectation returns")
        self.manager.createRecoveryKey(OTCKContainerName, contextID: self.otcliqueContext.context, recoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            createKeyExpectation.fulfill()
        }
        self.wait(for: [createKeyExpectation], timeout: 10)
    }

    func testSetRecoveryKeyPeerReaction() throws {
        self.startCKAccountStatusMock()
        self.manager.setSOSEnabledForPlatformFlag(false)

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
        self.manager.setSOSEnabledForPlatformFlag(true)

        let createKeyExpectation = self.expectation(description: "createKeyExpectation returns")
        self.manager.createRecoveryKey(OTCKContainerName, contextID: self.otcliqueContext.context, recoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            createKeyExpectation.fulfill()
        }
        self.wait(for: [createKeyExpectation], timeout: 10)

        let entropy = try self.loadSecret(label: clique.cliqueMemberIdentifier!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        XCTAssertTrue(try self.recoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.altDSID)))
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
        self.tphClient.dump(withContainer: OTCKContainerName, context: initiatorContextID) { dump, _ in
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
        self.tphClient.dump(withContainer: OTCKContainerName, context: self.otcliqueContext.context) { dump, _ in
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
        self.startCKAccountStatusMock()
        self.manager.setSOSEnabledForPlatformFlag(false)

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
        self.manager.setSOSEnabledForPlatformFlag(true)

        let createKeyExpectation = self.expectation(description: "createKeyExpectation returns")
        self.manager.createRecoveryKey(OTCKContainerName, contextID: self.otcliqueContext.context, recoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            createKeyExpectation.fulfill()
        }
        self.wait(for: [createKeyExpectation], timeout: 10)

        let entropy = try self.loadSecret(label: clique.cliqueMemberIdentifier!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        XCTAssertTrue(try self.recoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.altDSID)))

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
        self.tphClient.dump(withContainer: OTCKContainerName, context: initiatorContextID) { dump, _ in
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
        self.tphClient.dump(withContainer: OTCKContainerName, context: self.otcliqueContext.context) { dump, _ in
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
        self.tphClient.dump(withContainer: OTCKContainerName, context: thirdPeerContextID) { dump, _ in
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
            XCTAssertEqual(included!.count, 3, "should be 3df peer ids")

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
                                    sosAdapter: self.mockSOSAdapter,
                                    authKitAdapter: self.mockAuthKit2,
                                    lockStateTracker: self.lockStateTracker,
                                    accountStateTracker: self.accountStateTracker,
                                    deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-RK-iphone", serialNumber: "456", osVersion: "iOS (fake version)"))
    }

    func testJoinWithRecoveryKey() throws {
        OctagonRecoveryKeySetIsEnabled(true)
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
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = self.mockAuthKit2.altDSID!
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

        self.manager.setSOSEnabledForPlatformFlag(true)

        let createRecoveryExpectation = self.expectation(description: "createRecoveryExpectation returns")
        self.manager.createRecoveryKey(OTCKContainerName, contextID: establishContextID, recoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            createRecoveryExpectation.fulfill()
        }
        self.wait(for: [createRecoveryExpectation], timeout: 10)

        try self.putRecoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.altDSID))
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

        let joinedPeerID = self.fetchEgoPeerID(context: recoveryContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.sendContainerChangeWaitForFetch(context: recoveryContext)

        let stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(withContainer: OTCKContainerName, context: OTDefaultContext) { dump, _ in
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
        self.tphClient.dump(withContainer: OTCKContainerName, context: establishContextID) { dump, _ in
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
        OctagonRecoveryKeySetIsEnabled(true)
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
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = self.mockAuthKit2.altDSID!
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

        self.manager.setSOSEnabledForPlatformFlag(true)

        let createRecoveryExpectation = self.expectation(description: "createRecoveryExpectation returns")
        self.manager.createRecoveryKey(OTCKContainerName, contextID: establishContextID, recoveryKey: recoveryKey) { error in
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
        OctagonRecoveryKeySetIsEnabled(true)
        self.manager.setSOSEnabledForPlatformFlag(false)
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

        let recoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")
        self.manager.setSOSEnabledForPlatformFlag(true)

        let setRecoveryKeyExpectation = self.expectation(description: "setRecoveryKeyExpectation callback occurs")
        TestsObjectiveC.setNewRecoveryKeyWithData(self.otcliqueContext, recoveryKey: recoveryKey!) { rk, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNotNil(rk, "rk should not be nil")
            setRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [setRecoveryKeyExpectation], timeout: 10)
    }

    func testOTCliqueSet2ndRecoveryKey() throws {
        OctagonRecoveryKeySetIsEnabled(true)
        self.manager.setSOSEnabledForPlatformFlag(false)
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

        let recoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")
        self.manager.setSOSEnabledForPlatformFlag(true)

        let setRecoveryKeyExpectation = self.expectation(description: "setRecoveryKeyExpectation callback occurs")
        TestsObjectiveC.setNewRecoveryKeyWithData(self.otcliqueContext, recoveryKey: recoveryKey!) { rk, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNotNil(rk, "rk should not be nil")
            setRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [setRecoveryKeyExpectation], timeout: 10)

        let recoveryKey2 = SecRKCreateRecoveryKeyString(nil)

        let setRecoveryKeyExpectationAgain = self.expectation(description: "setRecoveryKeyExpectationAgain callback occurs")
        TestsObjectiveC.setNewRecoveryKeyWithData(self.otcliqueContext, recoveryKey: recoveryKey2!) { rk, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNotNil(rk, "rk should not be nil")
            setRecoveryKeyExpectationAgain.fulfill()
        }
        self.wait(for: [setRecoveryKeyExpectationAgain], timeout: 10)
    }
    func testRKReplacement() throws {
        OctagonRecoveryKeySetIsEnabled(true)
        self.manager.setSOSEnabledForPlatformFlag(false)
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
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        let initiatorContext = self.makeInitiatorContext(contextID: initiatorContextID)
        let initiatorConfigurationContext = OTConfigurationContext()
        initiatorConfigurationContext.context = initiatorContextID
        initiatorConfigurationContext.dsid = "1234"
        initiatorConfigurationContext.altDSID = self.mockAuthKit.altDSID!
        initiatorConfigurationContext.otControl = self.otControl

        initiatorContext.startOctagonStateMachine()
        self.sendContainerChange(context: initiatorContext)
        let restoreExpectation = self.expectation(description: "restore returns")

        self.manager!.restore(OTCKContainerName, contextID: initiatorContextID, bottleSalt: self.otcliqueContext.altDSID!, entropy: entropy!, bottleID: bottle.bottleID) { error in
            XCTAssertNil(error, "error should be nil")
            restoreExpectation.fulfill()
        }
        self.wait(for: [restoreExpectation], timeout: 10)

        self.assertEnters(context: initiatorContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        var initiatorDumpCallback = self.expectation(description: "initiatorDumpCallback callback occurs")
        self.tphClient.dump(withContainer: OTCKContainerName, context: initiatorContextID) { dump, _ in
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
        self.manager.setSOSEnabledForPlatformFlag(true)

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

        XCTAssertTrue(try self.recoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey2, salt: try XCTUnwrap(self.mockAuthKit.altDSID)))
        XCTAssertTrue(try self.recoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey2, salt: try XCTUnwrap(self.mockAuthKit.altDSID), sender: self.cuttlefishContext))

        var initiatorRecoverySigningKey: Data?
        var initiatorRecoveryEncryptionKey: Data?

        var firstDeviceRecoverySigningKey: Data?
        var firstDeviceRecoveryEncryptionKey: Data?

        //now let's ensure recovery keys are set for both the first device and second device
        initiatorDumpCallback = self.expectation(description: "initiatorDumpCallback callback occurs")
        self.tphClient.dump(withContainer: OTCKContainerName, context: initiatorContextID) { dump, _ in
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
        self.tphClient.dump(withContainer: OTCKContainerName, context: OTDefaultContext) { dump, _ in
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
        OctagonRecoveryKeySetIsEnabled(true)
        self.manager.setSOSEnabledForPlatformFlag(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let recoverykeyotcliqueContext = OTConfigurationContext()
        recoverykeyotcliqueContext.context = establishContextID
        recoverykeyotcliqueContext.dsid = "1234"
        recoverykeyotcliqueContext.altDSID = self.mockAuthKit.altDSID!
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
        self.manager.setSOSEnabledForPlatformFlag(true)

        let setRecoveryKeyExpectation = self.expectation(description: "setRecoveryKeyExpectation callback occurs")
        TestsObjectiveC.setNewRecoveryKeyWithData(recoverykeyotcliqueContext, recoveryKey: recoveryKey) { _, error in
            XCTAssertNil(error, "error should be nil")
            setRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [setRecoveryKeyExpectation], timeout: 10)

        try self.putRecoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.altDSID))

        self.sendContainerChangeWaitForFetch(context: establishContext)

        let newCliqueContext = OTConfigurationContext()
        newCliqueContext.context = OTDefaultContext
        newCliqueContext.dsid = self.otcliqueContext.dsid
        newCliqueContext.altDSID = self.mockAuthKit.altDSID!
        newCliqueContext.otControl = self.otControl

        let newGuyContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)
        self.verifyDatabaseMocks()

        self.manager.setSOSEnabledForPlatformFlag(true)
        let joinWithRecoveryKeyExpectation = self.expectation(description: "joinWithRecoveryKeyExpectation callback occurs")
        OTClique.recoverOctagon(usingData: newCliqueContext, recoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithRecoveryKeyExpectation], timeout: 20)

        self.sendContainerChangeWaitForFetch(context: newGuyContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
        XCTAssertTrue(try self.recoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.altDSID)))

        let stableInfoAcceptorCheckDumpCallback = self.expectation(description: "stableInfoAcceptorCheckDumpCallback callback occurs")
        self.tphClient.dump(withContainer: OTCKContainerName, context: OTDefaultContext) { dump, _ in
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
        try self.putSelfTLKSharesInCloudKit(context: newGuyContext)
        self.assertSelfTLKSharesInCloudKit(context: newGuyContext)

        self.sendContainerChangeWaitForFetch(context: establishContext)

        let stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(withContainer: OTCKContainerName, context: establishContextID) { dump, _ in
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

    func testJoinWithUnknownRecoveryKey() throws {
        OctagonRecoveryKeySetIsEnabled(true)

        self.startCKAccountStatusMock()

        let remote = self.makeInitiatorContext(contextID: "remote")
        self.assertResetAndBecomeTrusted(context: remote)

        let recoveryKey = try XCTUnwrap(SecRKCreateRecoveryKeyString(nil), "recoveryKey should not be nil")

        #if !os(macOS) && !os(iOS)
        let joinWithRecoveryKeyExpectation = self.expectation(description: "joinWithRecoveryKeyExpectation callback occurs")
        TestsObjectiveC.recoverOctagon(usingData: self.otcliqueContext, recoveryKey: recoveryKey) { error in
            XCTAssertNotNil(error, "error should exist")
            joinWithRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithRecoveryKeyExpectation], timeout: 20)

        // double-check that the status is not in
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        #else

        let joinWithRecoveryKeyExpectation = self.expectation(description: "joinWithRecoveryKeyExpectation callback occurs")
        TestsObjectiveC.recoverOctagon(usingData: self.otcliqueContext, recoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithRecoveryKeyExpectation], timeout: 20)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertConsidersSelfTrustedCachedAccountStatus(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        XCTAssertTrue(try self.recoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.altDSID)))
        self.verifyDatabaseMocks()

        let rejoinedDumpCallback = self.expectation(description: "dump callback occurs")
        self.tphClient.dump(withContainer: OTCKContainerName, context: OTDefaultContext) { dump, _ in
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
            XCTAssertEqual(included!.count, 1, "should be 1 peer ids")
            let vouchers = dump!["vouchers"]
            XCTAssertNotNil(vouchers, "vouchers should not be nil")
            rejoinedDumpCallback.fulfill()
        }
        self.wait(for: [rejoinedDumpCallback], timeout: 10)
        #endif
    }

    func testSetRecoveryKeyAsLimitedPeer() throws {
        self.manager.setSOSEnabledForPlatformFlag(false)

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

        let recoveryKey = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        let createKeyExpectation = self.expectation(description: "createKeyExpectation returns")
        self.manager.createRecoveryKey(OTCKContainerName, contextID: self.otcliqueContext.context, recoveryKey: recoveryKey) { error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error! as NSError).code, OctagonError.OTErrorLimitedPeer.rawValue, "error code should be limited peer")
            createKeyExpectation.fulfill()
        }
        self.wait(for: [createKeyExpectation], timeout: 10)
    }

    func testVouchWithRecoveryKeySetByUntrustedPeer() throws {
        OctagonRecoveryKeySetIsEnabled(true)
        self.manager.setSOSEnabledForPlatformFlag(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let recoverykeyotcliqueContext = OTConfigurationContext()
        recoverykeyotcliqueContext.context = establishContextID
        recoverykeyotcliqueContext.dsid = "1234"
        recoverykeyotcliqueContext.altDSID = self.mockAuthKit.altDSID!
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

        let joiningPeerContext = self.makeInitiatorContext(contextID: "joiner", authKitAdapter: self.mockAuthKit3)
        self.assertJoinViaEscrowRecovery(joiningContext: joiningPeerContext, sponsor: establishContext)
        self.sendContainerChangeWaitForFetch(context: establishContext)

        // Now, create the Recovery Key
        let recoveryKey = try XCTUnwrap(SecRKCreateRecoveryKeyString(nil), "should be able to create a recovery key")
        self.manager.setSOSEnabledForPlatformFlag(true)

        let setRecoveryKeyExpectation = self.expectation(description: "setRecoveryKeyExpectation callback occurs")
        TestsObjectiveC.setNewRecoveryKeyWithData(recoverykeyotcliqueContext, recoveryKey: recoveryKey) { _, error in
            XCTAssertNil(error, "error should be nil")
            setRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [setRecoveryKeyExpectation], timeout: 10)

        self.sendContainerChangeWaitForFetch(context: establishContext)

        //now this peer will leave octagon
        XCTAssertNoThrow(try clique.leave(), "Should be no error departing clique")

        // securityd should now consider itself untrusted
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: establishContext)

        let newCliqueContext = OTConfigurationContext()
        newCliqueContext.context = OTDefaultContext
        newCliqueContext.dsid = self.otcliqueContext.dsid
        newCliqueContext.altDSID = self.mockAuthKit.altDSID!
        newCliqueContext.otControl = self.otControl

        let newGuyContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)

        // We'll perform a reset here. Allow for CKKS to do the same.
        self.silentZoneDeletesAllowed = true

        self.manager.setSOSEnabledForPlatformFlag(true)
        let joinWithRecoveryKeyExpectation = self.expectation(description: "joinWithRecoveryKeyExpectation callback occurs")
        OTClique.recoverOctagon(usingData: newCliqueContext, recoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithRecoveryKeyExpectation], timeout: 20)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        XCTAssertTrue(try self.recoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.altDSID)))
    }

    func testVouchWithWrongRecoveryKey() throws {
        OctagonRecoveryKeySetIsEnabled(true)
        self.manager.setSOSEnabledForPlatformFlag(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let recoverykeyotcliqueContext = OTConfigurationContext()
        recoverykeyotcliqueContext.context = establishContextID
        recoverykeyotcliqueContext.dsid = "1234"
        recoverykeyotcliqueContext.altDSID = self.mockAuthKit.altDSID!
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

        var recoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")
        self.manager.setSOSEnabledForPlatformFlag(true)

        let setRecoveryKeyExpectation = self.expectation(description: "setRecoveryKeyExpectation callback occurs")
        TestsObjectiveC.setNewRecoveryKeyWithData(recoverykeyotcliqueContext, recoveryKey: recoveryKey!) { _, error in
            XCTAssertNil(error, "error should be nil")
            setRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [setRecoveryKeyExpectation], timeout: 10)

        self.sendContainerChangeWaitForFetch(context: establishContext)

        let newCliqueContext = OTConfigurationContext()
        newCliqueContext.context = OTDefaultContext
        newCliqueContext.dsid = self.otcliqueContext.dsid
        newCliqueContext.altDSID = self.mockAuthKit.altDSID!
        newCliqueContext.otControl = self.otControl

        let newGuyContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)

        self.manager.setSOSEnabledForPlatformFlag(true)
        let joinWithRecoveryKeyExpectation = self.expectation(description: "joinWithRecoveryKeyExpectation callback occurs")

        //creating new random recovery key
        recoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        // We'll reset Octagon here, so allow for CKKS to reset as well
        self.silentZoneDeletesAllowed = true

        OTClique.recoverOctagon(usingData: newCliqueContext, recoveryKey: recoveryKey!) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithRecoveryKeyExpectation], timeout: 20)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testRecoveryWithDistrustedPeers() throws {
        OctagonRecoveryKeySetIsEnabled(true)
        self.manager.setSOSEnabledForPlatformFlag(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let recoverykeyotcliqueContext = OTConfigurationContext()
        recoverykeyotcliqueContext.context = establishContextID
        recoverykeyotcliqueContext.dsid = "1234"
        recoverykeyotcliqueContext.altDSID = self.mockAuthKit.altDSID!
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

        let joiningPeerContext = self.makeInitiatorContext(contextID: "joiner", authKitAdapter: self.mockAuthKit3)
        self.assertJoinViaEscrowRecovery(joiningContext: joiningPeerContext, sponsor: establishContext)
        self.sendContainerChangeWaitForFetch(context: establishContext)

        let recoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")
        self.manager.setSOSEnabledForPlatformFlag(true)

        let setRecoveryKeyExpectation = self.expectation(description: "setRecoveryKeyExpectation callback occurs")
        TestsObjectiveC.setNewRecoveryKeyWithData(recoverykeyotcliqueContext, recoveryKey: recoveryKey!) { _, error in
            XCTAssertNil(error, "error should be nil")
            setRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [setRecoveryKeyExpectation], timeout: 10)

        self.sendContainerChangeWaitForFetch(context: establishContext)

        try self.putRecoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey!, salt: self.mockAuthKit.altDSID!)

        //now this peer will leave octagon
        XCTAssertNoThrow(try clique.leave(), "Should be no error departing clique")

        // securityd should now consider itself untrusted
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: establishContext)

        let newCliqueContext = OTConfigurationContext()
        newCliqueContext.context = OTDefaultContext
        newCliqueContext.dsid = self.otcliqueContext.dsid
        newCliqueContext.altDSID = self.mockAuthKit.altDSID!
        newCliqueContext.otControl = self.otControl

        let newGuyContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)

        self.manager.setSOSEnabledForPlatformFlag(true)
        let joinWithRecoveryKeyExpectation = self.expectation(description: "joinWithRecoveryKeyExpectation callback occurs")

        // We expect an Octagon reset here, because the RK is for a distrusted peer
        // This also performs a CKKS reset
        self.silentZoneDeletesAllowed = true

        OTClique.recoverOctagon(usingData: newCliqueContext, recoveryKey: recoveryKey!) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithRecoveryKeyExpectation], timeout: 20)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testMalformedRecoveryKey() throws {
        OctagonRecoveryKeySetIsEnabled(true)
        self.manager.setSOSEnabledForPlatformFlag(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let recoverykeyotcliqueContext = OTConfigurationContext()
        recoverykeyotcliqueContext.context = establishContextID
        recoverykeyotcliqueContext.dsid = "1234"
        recoverykeyotcliqueContext.altDSID = self.mockAuthKit.altDSID!
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
        self.manager.setSOSEnabledForPlatformFlag(true)

        let createKeyExpectation = self.expectation(description: "createKeyExpectation returns")
        self.manager.createRecoveryKey(OTCKContainerName, contextID: self.otcliqueContext.context, recoveryKey: recoveryKey) { error in
            XCTAssertNotNil(error, "error should NOT be nil")
            XCTAssertEqual((error! as NSError).code, 41, "error code should be 41/malformed recovery key")
            XCTAssertEqual((error! as NSError).domain, "com.apple.security.octagon", "error code domain should be com.apple.security.octagon")
            createKeyExpectation.fulfill()
        }
        self.wait(for: [createKeyExpectation], timeout: 10)

        let newCliqueContext = OTConfigurationContext()
        newCliqueContext.context = OTDefaultContext
        newCliqueContext.dsid = self.otcliqueContext.dsid
        newCliqueContext.altDSID = self.mockAuthKit.altDSID!
        newCliqueContext.otControl = self.otControl

        let newGuyContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        newGuyContext.startOctagonStateMachine()

        self.sendContainerChangeWaitForUntrustedFetch(context: newGuyContext)

        self.manager.setSOSEnabledForPlatformFlag(true)
        let joinWithRecoveryKeyExpectation = self.expectation(description: "joinWithRecoveryKeyExpectation callback occurs")

        OTClique.recoverOctagon(usingData: newCliqueContext, recoveryKey: recoveryKey) { error in
            XCTAssertNotNil(error, "error should NOT be nil")
            XCTAssertEqual((error! as NSError).code, 41, "error code should be 41/malformed recovery key")
            XCTAssertEqual((error! as NSError).domain, "com.apple.security.octagon", "error code domain should be com.apple.security.octagon")
            joinWithRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithRecoveryKeyExpectation], timeout: 20)
    }

    @discardableResult
    func createAndSetRecoveryKey(context: OTCuttlefishContext) throws -> String {
        let cliqueConfiguration = OTConfigurationContext()
        cliqueConfiguration.context = context.contextID
        cliqueConfiguration.altDSID = try! context.authKitAdapter.primaryiCloudAccountAltDSID()
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
        self.startCKAccountStatusMock()
        self.manager.setSOSEnabledForPlatformFlag(true)

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
        self.startCKAccountStatusMock()
        self.manager.setSOSEnabledForPlatformFlag(true)

        _ = self.assertResetAndBecomeTrustedInDefaultContext()
        // peer1 sets a recovery key
        try self.createAndSetRecoveryKey(context: self.cuttlefishContext)

        // Restart TPH
        self.tphClient.containerMap.removeAllContainers()

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
    }

    func testRecoveryKeyLoadingOnContainerLoadEvenIfMissing() throws {
        self.startCKAccountStatusMock()
        self.manager.setSOSEnabledForPlatformFlag(true)

        _ = self.assertResetAndBecomeTrustedInDefaultContext()
        // peer1 sets a recovery key
        try self.createAndSetRecoveryKey(context: self.cuttlefishContext)

        // Before restarting TPH, emulate a world in which the RK variables were not set on the container

        let containerName = ContainerName(container: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID)
        let container = try self.tphClient.containerMap.findOrCreate(name: containerName)
        container.removeRKFromContainer()

        // Restart TPH
        self.tphClient.containerMap.removeAllContainers()

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
    }

    func testCKKSSendsTLKSharesToRecoveryKey() throws {
        #if os(tvOS) || os(watchOS)
        self.startCKAccountStatusMock()
        throw XCTSkip("Apple TVs and watches will not set recovery key")
        #else

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
        XCTAssertTrue(try self.recoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.altDSID)))

        #endif // tvOS || watchOS
    }

    func testRKRecoveryRecoversCKKSCreatedShares() throws {
        self.startCKAccountStatusMock()

        let remote = self.createEstablishContext(contextID: "remote")
        self.assertResetAndBecomeTrusted(context: remote)

        #if os(tvOS) || os(watchOS)
        self.manager.setSOSEnabledForPlatformFlag(true)
        let recoveryKey = try self.createAndSetRecoveryKey(context: remote)
        self.manager.setSOSEnabledForPlatformFlag(false)
        #else
        let recoveryKey = try self.createAndSetRecoveryKey(context: remote)
        #endif

        // And TLKShares for the RK are sent from the Octagon peer
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putRecoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.altDSID), sender: remote)
        XCTAssertTrue(try self.recoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.altDSID), sender: remote))

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

    func testRecoverTLKSharesSentToRKAfterCKKSTimeout() throws {
        OctagonRecoveryKeySetIsEnabled(true)
        self.manager.setSOSEnabledForPlatformFlag(false)
        self.startCKAccountStatusMock()

        let remote = self.createEstablishContext(contextID: "remote")
        self.assertResetAndBecomeTrusted(context: remote)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: remote)
        self.assertSelfTLKSharesInCloudKit(context: remote)

        self.manager.setSOSEnabledForPlatformFlag(true)
        let recoveryKey = try self.createAndSetRecoveryKey(context: remote)

        try self.putRecoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.altDSID))

        // Now, join from a new device
        // Simulate CKKS fetches taking forever. In practice, this is caused by many round-trip fetches to CK happening over minutes.
        self.holdCloudKitFetches()

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let joinWithRecoveryKeyExpectation = self.expectation(description: "joinWithRecoveryKey callback occurs")
        self.cuttlefishContext.join(withRecoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithRecoveryKeyExpectation], timeout: 20)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateFetch, within: 10 * NSEC_PER_SEC)

        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.releaseCloudKitFetchHold()
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
    }
}
#endif
