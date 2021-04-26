#if OCTAGON

@objcMembers
class OctagonEscrowRecoveryTests: OctagonTestsBase {
    override func setUp() {
        super.setUp()
    }

    func testJoinWithBottle() throws {
        let initiatorContextID = "initiator-context-id"
        let bottlerContext = self.makeInitiatorContext(contextID: initiatorContextID)

        bottlerContext.startOctagonStateMachine()
        let ckacctinfo = CKAccountInfo()
        ckacctinfo.accountStatus = .available
        ckacctinfo.hasValidCredentials = true
        ckacctinfo.accountPartition = .production

        bottlerContext.cloudkitAccountStateChange(nil, to: ckacctinfo)
        XCTAssertNoThrow(try bottlerContext.setCDPEnabled())
        self.assertEnters(context: bottlerContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = initiatorContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = self.mockAuthKit.altDSID!
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: bottlerContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: bottlerContext)

        let entropy = try self.loadSecret(label: clique.cliqueMemberIdentifier!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: bottlerContext)

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // Before you call joinWithBottle, you need to call fetchViableBottles.
        let fetchViableExpectation = self.expectation(description: "fetchViableBottles callback occurs")
        self.cuttlefishContext.rpcFetchAllViableBottles { viable, _, error in
            XCTAssertNil(error, "should be no error fetching viable bottles")
            XCTAssert(viable?.contains(bottle.bottleID) ?? false, "The bottle we're about to restore should be viable")
            fetchViableExpectation.fulfill()
        }
        self.wait(for: [fetchViableExpectation], timeout: 10)

        let joinWithBottleExpectation = self.expectation(description: "joinWithBottle callback occurs")
        self.cuttlefishContext.join(withBottle: bottle.bottleID, entropy: entropy!, bottleSalt: self.otcliqueContext.altDSID!) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithBottleExpectation.fulfill()
        }

        self.wait(for: [joinWithBottleExpectation], timeout: 100)

        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(withContainer: OTCKContainerName, context: OTDefaultContext) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        self.verifyDatabaseMocks()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertTLKSharesInCloudKit(receiver: self.cuttlefishContext, sender: self.cuttlefishContext)
    }

    func testBottleRestoreEntersOctagonReady() throws {
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
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        let initiatorContext = self.makeInitiatorContext(contextID: initiatorContextID)

        initiatorContext.startOctagonStateMachine()
        let restoreExpectation = self.expectation(description: "restore returns")

        self.manager!.restore(OTCKContainerName, contextID: initiatorContextID, bottleSalt: self.otcliqueContext.altDSID!, entropy: entropy!, bottleID: bottle.bottleID) { error in
            XCTAssertNil(error, "error should be nil")
            restoreExpectation.fulfill()
        }
        self.wait(for: [restoreExpectation], timeout: 10)

        self.assertEnters(context: initiatorContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        let initiatorDumpCallback = self.expectation(description: "initiatorDumpCallback callback occurs")
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
    }

    func testJoinWithBottleWithCKKSConflict() throws {
        let initiatorContextID = "initiator-context-id"
        let bottlerContext = self.makeInitiatorContext(contextID: initiatorContextID)

        bottlerContext.startOctagonStateMachine()
        let ckacctinfo = CKAccountInfo()
        ckacctinfo.accountStatus = .available
        ckacctinfo.hasValidCredentials = true
        ckacctinfo.accountPartition = .production

        bottlerContext.cloudkitAccountStateChange(nil, to: ckacctinfo)
        XCTAssertNoThrow(try bottlerContext.setCDPEnabled())
        self.assertEnters(context: bottlerContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = initiatorContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = self.mockAuthKit.altDSID!
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: bottlerContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: bottlerContext)

        let entropy = try self.loadSecret(label: clique.cliqueMemberIdentifier!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        // During the join, there's a CKKS key race
        self.silentFetchesAllowed = false
        self.expectCKFetchAndRun {
            self.putFakeKeyHierarchiesInCloudKit()
            self.putFakeDeviceStatusesInCloudKit()
            self.silentFetchesAllowed = true
        }

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // Before you call joinWithBottle, you need to call fetchViableBottles.
        let fetchViableExpectation = self.expectation(description: "fetchViableBottles callback occurs")
        self.cuttlefishContext.rpcFetchAllViableBottles { viable, _, error in
            XCTAssertNil(error, "should be no error fetching viable bottles")
            XCTAssert(viable?.contains(bottle.bottleID) ?? false, "The bottle we're about to restore should be viable")
            fetchViableExpectation.fulfill()
        }
        self.wait(for: [fetchViableExpectation], timeout: 10)

        let joinWithBottleExpectation = self.expectation(description: "joinWithBottle callback occurs")
        self.cuttlefishContext.join(withBottle: bottle.bottleID, entropy: entropy!, bottleSalt: self.otcliqueContext.altDSID!) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithBottleExpectation.fulfill()
        }

        self.wait(for: [joinWithBottleExpectation], timeout: 100)

        self.verifyDatabaseMocks()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLK, within: 10 * NSEC_PER_SEC)
    }

    func testBottleRestoreWithSameMachineID() throws {
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

        self.verifyDatabaseMocks()

        let entropy = try self.loadSecret(label: clique.cliqueMemberIdentifier!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        // some other peer should restore from this bottle
        let differentDevice = self.makeInitiatorContext(contextID: "differenDevice")
        let differentRestoreExpectation = self.expectation(description: "different restore returns")
        differentDevice.startOctagonStateMachine()
        differentDevice.join(withBottle: bottle.bottleID,
                             entropy: entropy!,
                             bottleSalt: self.otcliqueContext.altDSID!) { error in
                                XCTAssertNil(error, "error should be nil")
                                differentRestoreExpectation.fulfill()
        }
        self.wait(for: [differentRestoreExpectation], timeout: 10)

        self.assertTrusts(context: differentDevice, includedPeerIDCount: 2, excludedPeerIDCount: 0)

        // The first peer will upload TLKs for the new peer
        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.assertTrusts(context: self.cuttlefishContext, includedPeerIDCount: 2, excludedPeerIDCount: 0)

        // Explicitly use the same authkit parameters as the original peer when restoring this time
        let restoreContext = self.makeInitiatorContext(contextID: "restoreContext", authKitAdapter: self.mockAuthKit)

        restoreContext.startOctagonStateMachine()

        let restoreExpectation = self.expectation(description: "restore returns")
        restoreContext.join(withBottle: bottle.bottleID,
                            entropy: entropy!,
                            bottleSalt: self.otcliqueContext.altDSID!) { error in
                                XCTAssertNil(error, "error should be nil")
                                restoreExpectation.fulfill()
        }
        self.wait(for: [restoreExpectation], timeout: 10)

        self.assertEnters(context: restoreContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: restoreContext)

        // The restore context should exclude its sponsor
        self.assertTrusts(context: restoreContext, includedPeerIDCount: 2, excludedPeerIDCount: 1)

        // Then, the remote peer should trust the new peer and exclude the original
        self.sendContainerChangeWaitForFetchForStates(context: differentDevice, states: [OctagonStateReadyUpdated, OctagonStateReady])
        self.assertTrusts(context: differentDevice, includedPeerIDCount: 2, excludedPeerIDCount: 1)

        // Then, if by some strange miracle the original peer is still around, it should bail (as it's now untrusted)
        self.sendContainerChangeWaitForFetchForStates(context: self.cuttlefishContext, states: [OctagonStateUntrusted])
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)
        self.assertTrusts(context: self.cuttlefishContext, includedPeerIDCount: 0, excludedPeerIDCount: 1)
    }

    func testRestoreSPIFromPiggybackingState() throws {
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
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        let initiatorContext = self.makeInitiatorContext(contextID: initiatorContextID)

        initiatorContext.startOctagonStateMachine()

        XCTAssertNoThrow(try initiatorContext.setCDPEnabled())
        self.assertEnters(context: initiatorContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let restoreExpectation = self.expectation(description: "restore returns")

        self.manager!.restore(OTCKContainerName, contextID: initiatorContextID, bottleSalt: self.otcliqueContext.altDSID!, entropy: entropy!, bottleID: bottle.bottleID) { error in
            XCTAssertNil(error, "error should be nil")
            restoreExpectation.fulfill()
        }
        self.wait(for: [restoreExpectation], timeout: 10)

        let initiatorDumpCallback = self.expectation(description: "initiatorDumpCallback callback occurs")
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
    }

    func testRestoreBadBottleIDFails() throws {
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
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        _ = self.fakeCuttlefishServer.state.bottles[0]

        let initiatorContext = self.makeInitiatorContext(contextID: initiatorContextID)

        initiatorContext.startOctagonStateMachine()

        let restoreExpectation = self.expectation(description: "restore returns")

        self.manager!.restore(OTCKContainerName, contextID: initiatorContextID, bottleSalt: self.otcliqueContext.altDSID!, entropy: entropy!, bottleID: "bad escrow record ID") { error in
            XCTAssertNotNil(error, "error should not be nil")
            restoreExpectation.fulfill()
        }
        self.wait(for: [restoreExpectation], timeout: 10)
        self.assertEnters(context: initiatorContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let acceptorDumpCallback = self.expectation(description: "acceptorDumpCallback callback occurs")
        self.tphClient.dump(withContainer: OTCKContainerName, context: OTDefaultContext) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 1, "should be 1 peer ids")

            acceptorDumpCallback.fulfill()
        }
        self.wait(for: [acceptorDumpCallback], timeout: 10)
    }

    func testRestoreOptimalBottleIDs() throws {
        self.startCKAccountStatusMock()

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

        var bottleIDs = try OTClique.findOptimalBottleIDs(withContextData: self.otcliqueContext)
        XCTAssertNotNil(bottleIDs.preferredBottleIDs, "preferredBottleIDs should not be nil")
        XCTAssertEqual(bottleIDs.preferredBottleIDs.count, 1, "preferredBottleIDs should have 1 bottle")
        XCTAssertEqual(bottleIDs.partialRecoveryBottleIDs.count, 0, "partialRecoveryBottleIDs should be empty")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        let initiatorContext = self.manager.context(forContainerName: OTCKContainerName,
                                                    contextID: "restoreContext",
                                                    sosAdapter: OTSOSMissingAdapter(),
                                                    authKitAdapter: self.mockAuthKit2,
                                                    lockStateTracker: self.lockStateTracker,
                                                    accountStateTracker: self.accountStateTracker,
                                                    deviceInformationAdapter: self.makeInitiatorDeviceInfoAdapter())

        initiatorContext.startOctagonStateMachine()
        let newOTCliqueContext = OTConfigurationContext()
        newOTCliqueContext.context = "restoreContext"
        newOTCliqueContext.dsid = self.otcliqueContext.dsid
        newOTCliqueContext.altDSID = self.otcliqueContext.altDSID
        newOTCliqueContext.otControl = self.otcliqueContext.otControl
        newOTCliqueContext.sbd = OTMockSecureBackup(bottleID: bottle.bottleID, entropy: entropy!)

        let newClique: OTClique
        do {
            newClique = try OTClique.performEscrowRecovery(withContextData: newOTCliqueContext, escrowArguments: [:])
            XCTAssertNotNil(newClique, "newClique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored recovering: \(error)")
            throw error
        }

        // We will upload a new TLK for the new peer
        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.sendContainerChangeWaitForFetch(context: initiatorContext)
        bottleIDs = try OTClique.findOptimalBottleIDs(withContextData: self.otcliqueContext)
        XCTAssertNotNil(bottleIDs.preferredBottleIDs, "preferredBottleIDs should not be nil")
        XCTAssertEqual(bottleIDs.preferredBottleIDs.count, 2, "preferredBottleIDs should have 2 bottle")
        XCTAssertEqual(bottleIDs.partialRecoveryBottleIDs.count, 0, "partialRecoveryBottleIDs should be empty")
    }

    func testRestoreFromEscrowContents() throws {
        self.startCKAccountStatusMock()

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        var bottleIDs = try OTClique.findOptimalBottleIDs(withContextData: self.otcliqueContext)
        XCTAssertNotNil(bottleIDs.preferredBottleIDs, "preferredBottleIDs should not be nil")
        XCTAssertEqual(bottleIDs.preferredBottleIDs.count, 1, "preferredBottleIDs should have 1 bottle")
        XCTAssertEqual(bottleIDs.partialRecoveryBottleIDs.count, 0, "partialRecoveryBottleIDs should be empty")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.verifyDatabaseMocks()

        var entropy = Data()
        var bottledID: String = ""

        let fetchEscrowContentsExpectation = self.expectation(description: "fetchEscrowContentsExpectation returns")
        clique.fetchEscrowContents { e, b, s, _ in
            XCTAssertNotNil(e, "entropy should not be nil")
            XCTAssertNotNil(b, "bottleID should not be nil")
            XCTAssertNotNil(s, "signingPublicKey should not be nil")
            entropy = e!
            bottledID = b!
            fetchEscrowContentsExpectation.fulfill()
        }

        self.wait(for: [fetchEscrowContentsExpectation], timeout: 10)

        let initiatorContext = self.manager.context(forContainerName: OTCKContainerName,
                                                    contextID: "restoreContext",
                                                    sosAdapter: OTSOSMissingAdapter(),
                                                    authKitAdapter: self.mockAuthKit2,
                                                    lockStateTracker: self.lockStateTracker,
                                                    accountStateTracker: self.accountStateTracker,
                                                    deviceInformationAdapter: self.makeInitiatorDeviceInfoAdapter())

        initiatorContext.startOctagonStateMachine()
        let newOTCliqueContext = OTConfigurationContext()
        newOTCliqueContext.context = "restoreContext"
        newOTCliqueContext.dsid = self.otcliqueContext.dsid
        newOTCliqueContext.altDSID = self.otcliqueContext.altDSID
        newOTCliqueContext.otControl = self.otcliqueContext.otControl
        newOTCliqueContext.sbd = OTMockSecureBackup(bottleID: bottledID, entropy: entropy)

        let newClique: OTClique
        do {
            newClique = try OTClique.performEscrowRecovery(withContextData: newOTCliqueContext, escrowArguments: [:])
            XCTAssertNotNil(newClique, "newClique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored recovering: \(error)")
            throw error
        }

        // We will upload a new TLK for the new peer
        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.sendContainerChangeWaitForFetch(context: initiatorContext)

        bottleIDs = try OTClique.findOptimalBottleIDs(withContextData: self.otcliqueContext)
        XCTAssertNotNil(bottleIDs.preferredBottleIDs, "preferredBottleIDs should not be nil")
        XCTAssertEqual(bottleIDs.preferredBottleIDs.count, 2, "preferredBottleIDs should have 2 bottle")
        XCTAssertEqual(bottleIDs.partialRecoveryBottleIDs.count, 0, "partialRecoveryBottleIDs should be empty")

        let dumpExpectation = self.expectation(description: "dump callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) { dump, error in
            XCTAssertNil(error, "Should be no error dumping data")
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let peerID = egoSelf!["peerID"] as? String
            XCTAssertNotNil(peerID, "peerID should not be nil")

            dumpExpectation.fulfill()
        }
        self.wait(for: [dumpExpectation], timeout: 10)

        self.otControlCLI.status(OTCKContainerName,
                                 context: newOTCliqueContext.context,
                                 json: false)
    }

    func testFetchEmptyOptimalBottleList () throws {
        self.startCKAccountStatusMock()

        let bottleIDs = try OTClique.findOptimalBottleIDs(withContextData: self.otcliqueContext)
        XCTAssertEqual(bottleIDs.preferredBottleIDs.count, 0, "should be 0 preferred bottle ids")
        XCTAssertEqual(bottleIDs.partialRecoveryBottleIDs.count, 0, "should be 0 partialRecoveryBottleIDs")
    }

    func testFetchOptimalBottlesAfterFailedRestore() throws {
        self.startCKAccountStatusMock()

        self.otcliqueContext.sbd = OTMockSecureBackup(bottleID: "bottle ID", entropy: Data(count: 72))

        XCTAssertThrowsError(try OTClique.performEscrowRecovery(withContextData: self.otcliqueContext, escrowArguments: [:]))

        let bottleIDs = try OTClique.findOptimalBottleIDs(withContextData: self.otcliqueContext)
        XCTAssertNotNil(bottleIDs.preferredBottleIDs, "preferredBottleIDs should not be nil")
        XCTAssertEqual(bottleIDs.preferredBottleIDs.count, 0, "preferredBottleIDs should have 0 bottle")
        XCTAssertEqual(bottleIDs.partialRecoveryBottleIDs.count, 0, "partialRecoveryBottleIDs should be empty")
    }

    func testMakeNewFriendsAndFetchEscrowContents () throws {
        self.startCKAccountStatusMock()

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
            self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

            let fetchEscrowContentsException = self.expectation(description: "update returns")

            clique.fetchEscrowContents { e, b, s, _ in
                XCTAssertNotNil(e, "entropy should not be nil")
                XCTAssertNotNil(b, "bottleID should not be nil")
                XCTAssertNotNil(s, "signingPublicKey should not be nil")
                fetchEscrowContentsException.fulfill()
            }
            self.wait(for: [fetchEscrowContentsException], timeout: 10)
        } catch {
            XCTFail("failed to reset clique: \(error)")
        }

        self.verifyDatabaseMocks()
    }

    func testFetchEscrowContentsBeforeIdentityExists() {
        self.startCKAccountStatusMock()

        let initiatorContext = self.manager.context(forContainerName: OTCKContainerName, contextID: "initiator")
        let fetchEscrowContentsException = self.expectation(description: "update returns")

        initiatorContext.fetchEscrowContents { entropy, bottleID, signingPubKey, error in
            XCTAssertNil(entropy, "entropy should be nil")
            XCTAssertNil(bottleID, "bottleID should be nil")
            XCTAssertNil(signingPubKey, "signingPublicKey should be nil")
            XCTAssertNotNil(error, "error should not be nil")

            fetchEscrowContentsException.fulfill()
        }
        self.wait(for: [fetchEscrowContentsException], timeout: 10)
    }

    func testFetchEscrowContentsChecksEntitlement() throws {
        self.startCKAccountStatusMock()

        let contextName = OTDefaultContext
        let containerName = OTCKContainerName

        // First, fail due to not having any data
        let fetchEscrowContentsExpectation = self.expectation(description: "fetchEscrowContentsExpectation returns")
        self.otControl.fetchEscrowContents(containerName, contextID: contextName) { entropy, bottle, signingPublicKey, error in
            XCTAssertNotNil(error, "error should not be nil")
            //XCTAssertNotEqual(error.code, errSecMissingEntitlement, "Error should not be 'missing entitlement'")
            XCTAssertNil(entropy, "entropy should be nil")
            XCTAssertNil(bottle, "bottleID should be nil")
            XCTAssertNil(signingPublicKey, "signingPublicKey should be nil")
            fetchEscrowContentsExpectation.fulfill()
        }
        self.wait(for: [fetchEscrowContentsExpectation], timeout: 10)

        // Now, fail due to the client not having an entitlement
        self.otControlEntitlementBearer.entitlements.removeAll()
        let failFetchEscrowContentsExpectation = self.expectation(description: "fetchEscrowContentsExpectation returns")
        self.otControl.fetchEscrowContents(containerName, contextID: contextName) { entropy, bottle, signingPublicKey, error in
            XCTAssertNotNil(error, "error should not be nil")
            switch error {
            case .some(let error as NSError):
                XCTAssertEqual(error.domain, NSOSStatusErrorDomain, "Error should be an OS status")
                XCTAssertEqual(error.code, Int(errSecMissingEntitlement), "Error should be 'missing entitlement'")
            default:
                XCTFail("Unable to turn error into NSError: \(String(describing: error))")
            }

            XCTAssertNil(entropy, "entropy should not be nil")
            XCTAssertNil(bottle, "bottleID should not be nil")
            XCTAssertNil(signingPublicKey, "signingPublicKey should not be nil")
            failFetchEscrowContentsExpectation.fulfill()
        }
        self.wait(for: [failFetchEscrowContentsExpectation], timeout: 10)
    }

    func testJoinWithBottleFailCaseBottleDoesntExist() throws {
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

        let entropy = try self.loadSecret(label: clique.cliqueMemberIdentifier!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.verifyDatabaseMocks()

        let bottle = self.fakeCuttlefishServer.state.bottles[0]
        self.fakeCuttlefishServer.state.bottles.removeAll()

        let initiatorContext = self.makeInitiatorContext(contextID: initiatorContextID)

        initiatorContext.startOctagonStateMachine()

        let joinWithBottleExpectation = self.expectation(description: "joinWithBottle callback occurs")
        initiatorContext.join(withBottle: bottle.bottleID, entropy: entropy!, bottleSalt: self.otcliqueContext.altDSID!) { error in
            XCTAssertNotNil(error, "error should not be nil")
            joinWithBottleExpectation.fulfill()
        }
        self.wait(for: [joinWithBottleExpectation], timeout: 10)
        self.assertEnters(context: initiatorContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
    }

    func testJoinWithBottleFailCaseBadEscrowRecord() throws {
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

        let entropy = try self.loadSecret(label: clique.cliqueMemberIdentifier!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        _ = self.fakeCuttlefishServer.state.bottles[0]

        let initiatorContext = self.makeInitiatorContext(contextID: initiatorContextID)

        initiatorContext.startOctagonStateMachine()

        let joinWithBottleExpectation = self.expectation(description: "joinWithBottle callback occurs")
        initiatorContext.join(withBottle: "sos peer id", entropy: entropy!, bottleSalt: self.otcliqueContext.altDSID!) { error in
            XCTAssertNotNil(error, "error should be nil")
            joinWithBottleExpectation.fulfill()
        }
        self.wait(for: [joinWithBottleExpectation], timeout: 10)
        self.assertEnters(context: initiatorContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
    }

    func testJoinWithBottleFailCaseBadEntropy() throws {
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

        let entropy = try self.loadSecret(label: clique.cliqueMemberIdentifier!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.verifyDatabaseMocks()

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        let initiatorContext = self.makeInitiatorContext(contextID: initiatorContextID)

        initiatorContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        let joinWithBottleExpectation = self.expectation(description: "joinWithBottle callback occurs")
        initiatorContext.join(withBottle: bottle.bottleID, entropy: Data(count: 72), bottleSalt: self.otcliqueContext.altDSID!) { error in
            XCTAssertNotNil(error, "error should not be nil, when entropy is missing")
            joinWithBottleExpectation.fulfill()
        }
        self.wait(for: [joinWithBottleExpectation], timeout: 10)
        self.assertEnters(context: initiatorContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
    }

    func testJoinWithBottleFailCaseBadBottleSalt() throws {
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

        let entropy = try self.loadSecret(label: clique.cliqueMemberIdentifier!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.verifyDatabaseMocks()

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        let initiatorContext = self.makeInitiatorContext(contextID: initiatorContextID)

        initiatorContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        let joinWithBottleExpectation = self.expectation(description: "joinWithBottle callback occurs")
        initiatorContext.join(withBottle: bottle.bottleID, entropy: Data(count: 72), bottleSalt: "123456789") { error in
            XCTAssertNotNil(error, "error should not be nil with bad entropy and salt")
            joinWithBottleExpectation.fulfill()
        }
        self.wait(for: [joinWithBottleExpectation], timeout: 10)
        self.assertEnters(context: initiatorContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
    }

    func testRecoverFromDeviceNotOnMachineIDList() throws {
        self.startCKAccountStatusMock()

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        let firstPeerID = clique.cliqueMemberIdentifier
        XCTAssertNotNil(firstPeerID, "Clique should have a member identifier")
        let entropy = try self.loadSecret(label: firstPeerID!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        let bottleIDs = try OTClique.findOptimalBottleIDs(withContextData: self.otcliqueContext)
        XCTAssertNotNil(bottleIDs.preferredBottleIDs, "preferredBottleIDs should not be nil")
        XCTAssertEqual(bottleIDs.preferredBottleIDs.count, 1, "preferredBottleIDs should have 1 bottle")
        XCTAssertEqual(bottleIDs.partialRecoveryBottleIDs.count, 0, "partialRecoveryBottleIDs should be empty")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        // To get into the state we need, we need to introduce peer B and C. C should then distrust A, whose bottle it used
        // B shouldn't have an opinion of C.

        let bNewOTCliqueContext = OTConfigurationContext()
        bNewOTCliqueContext.context = "restoreB"
        bNewOTCliqueContext.dsid = self.otcliqueContext.dsid
        bNewOTCliqueContext.altDSID = self.otcliqueContext.altDSID
        bNewOTCliqueContext.otControl = self.otcliqueContext.otControl
        bNewOTCliqueContext.sbd = OTMockSecureBackup(bottleID: bottle.bottleID, entropy: entropy!)

        let deviceBmockAuthKit = OTMockAuthKitAdapter(altDSID: self.otcliqueContext.altDSID!,
                                                      machineID: "b-machine-id",
                                                      otherDevices: [self.mockAuthKit.currentMachineID])

        let bRestoreContext = self.manager.context(forContainerName: OTCKContainerName,
                                                   contextID: bNewOTCliqueContext.context,
                                                   sosAdapter: OTSOSMissingAdapter(),
                                                   authKitAdapter: deviceBmockAuthKit,
                                                   lockStateTracker: self.lockStateTracker,
                                                   accountStateTracker: self.accountStateTracker,
                                                   deviceInformationAdapter: self.makeInitiatorDeviceInfoAdapter())
        bRestoreContext.startOctagonStateMachine()
        let bNewClique: OTClique
        do {
            bNewClique = try OTClique.performEscrowRecovery(withContextData: bNewOTCliqueContext, escrowArguments: [:])
            XCTAssertNotNil(bNewClique, "bNewClique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored recovering: \(error)")
            throw error
        }
        self.assertEnters(context: bRestoreContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        // And introduce C, which will kick out A
        // During the next sign in, the machine ID list has changed to just the new one
        let restoremockAuthKit = OTMockAuthKitAdapter(altDSID: self.otcliqueContext.altDSID!,
                                                      machineID: "c-machine-id",
                                                      otherDevices: [self.mockAuthKit.currentMachineID, deviceBmockAuthKit.currentMachineID])
        let restoreContext = self.manager.context(forContainerName: OTCKContainerName,
                                                  contextID: "restoreContext",
                                                  sosAdapter: OTSOSMissingAdapter(),
                                                  authKitAdapter: restoremockAuthKit,
                                                  lockStateTracker: self.lockStateTracker,
                                                  accountStateTracker: self.accountStateTracker,
                                                  deviceInformationAdapter: self.makeInitiatorDeviceInfoAdapter())

        restoreContext.startOctagonStateMachine()
        let newOTCliqueContext = OTConfigurationContext()
        newOTCliqueContext.context = "restoreContext"
        newOTCliqueContext.dsid = self.otcliqueContext.dsid
        newOTCliqueContext.altDSID = self.otcliqueContext.altDSID
        newOTCliqueContext.otControl = self.otcliqueContext.otControl
        newOTCliqueContext.sbd = OTMockSecureBackup(bottleID: bottle.bottleID, entropy: entropy!)

        let newClique: OTClique
        do {
            newClique = try OTClique.performEscrowRecovery(withContextData: newOTCliqueContext, escrowArguments: [:])
            XCTAssertNotNil(newClique, "newClique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored recovering: \(error)")
            throw error
        }

        self.assertEnters(context: restoreContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
        self.assertConsidersSelfTrusted(context: restoreContext)

        let restoreDumpCallback = self.expectation(description: "acceptorDumpCallback callback occurs")
        self.tphClient.dump(withContainer: OTCKContainerName, context: newOTCliqueContext.context) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 3, "should be 3 peer id included")

            restoreDumpCallback.fulfill()
        }
        self.wait(for: [restoreDumpCallback], timeout: 10)

        // Now, exclude peer A's machine ID
        restoremockAuthKit.otherDevices = [deviceBmockAuthKit.currentMachineID]

        // Peer C should upload a new trust status
        let updateTrustExpectation = self.expectation(description: "updateTrust")
        self.fakeCuttlefishServer.updateListener = { request in
            XCTAssertTrue(request.hasDynamicInfoAndSig, "updateTrust request should have a dynamic info")
            let newDynamicInfo = TPPeerDynamicInfo(data: request.dynamicInfoAndSig.peerDynamicInfo,
                                                   sig: request.dynamicInfoAndSig.sig)
            XCTAssertNotNil(newDynamicInfo, "should be able to make a dynamic info from protobuf")

            XCTAssertEqual(newDynamicInfo!.excludedPeerIDs.count, 1, "Should have a single excluded peer")
            updateTrustExpectation.fulfill()
            return nil
        }

        restoreContext.incompleteNotificationOfMachineIDListChange()
        self.wait(for: [updateTrustExpectation], timeout: 10)

        self.assertEnters(context: restoreContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
        self.assertConsidersSelfTrusted(context: restoreContext)
    }

    func testCachedBottleFetch() throws {
        OctagonSetOptimizationEnabled(false)
        let initiatorContextID = "initiator-context-id"
        let bottlerContext = self.makeInitiatorContext(contextID: initiatorContextID)

        bottlerContext.startOctagonStateMachine()
        let ckacctinfo = CKAccountInfo()
        ckacctinfo.accountStatus = .available
        ckacctinfo.hasValidCredentials = true
        ckacctinfo.accountPartition = .production

        bottlerContext.cloudkitAccountStateChange(nil, to: ckacctinfo)
        XCTAssertNoThrow(try bottlerContext.setCDPEnabled())
        self.assertEnters(context: bottlerContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = initiatorContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = self.mockAuthKit.altDSID!
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: bottlerContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: bottlerContext)

        let entropy = try self.loadSecret(label: clique.cliqueMemberIdentifier!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: bottlerContext)

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        // Try to enforce that that CKKS doesn't know about the key hierarchy until Octagon asks it
        self.holdCloudKitFetches()

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let joinWithBottleExpectation = self.expectation(description: "joinWithBottle callback occurs")
        self.cuttlefishContext.join(withBottle: bottle.bottleID, entropy: entropy!, bottleSalt: self.otcliqueContext.altDSID!) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithBottleExpectation.fulfill()
        }

        sleep(1)
        self.releaseCloudKitFetchHold()

        self.wait(for: [joinWithBottleExpectation], timeout: 100)

        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(withContainer: OTCKContainerName, context: OTDefaultContext) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        self.verifyDatabaseMocks()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        //now call fetchviablebottles, we should get the uncached version
        let fetchUnCachedViableBottlesExpectation = self.expectation(description: "fetch UnCached ViableBottles")

        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            fetchUnCachedViableBottlesExpectation.fulfill()
            return nil
        }
        let FetchAllViableBottles = self.expectation(description: "FetchAllViableBottles callback occurs")

        self.cuttlefishContext.rpcFetchAllViableBottles { viable, _, error in
            XCTAssertNil(error, "should be no error fetching viable bottles")
            XCTAssert(viable?.contains(bottle.bottleID) ?? false, "The bottle we're about to restore should be viable")
            XCTAssertEqual(viable?.count, 2, "There should be 2 bottles")
            XCTAssertNotEqual(viable?[0], "", "Bottle should not be empty")
            XCTAssertNotEqual(viable?[1], "", "Bottle should not be empty")

            FetchAllViableBottles.fulfill()
        }
        self.wait(for: [FetchAllViableBottles], timeout: 10)
        self.wait(for: [fetchUnCachedViableBottlesExpectation], timeout: 10)

        let fetchViableExpectation = self.expectation(description: "fetchViableBottles callback occurs")
        self.cuttlefishContext.rpcFetchAllViableBottles { viable, _, error in
            XCTAssertNil(error, "should be no error fetching viable bottles")
            XCTAssert(viable?.contains(bottle.bottleID) ?? false, "The bottle we're about to restore should be viable")
            fetchViableExpectation.fulfill()
        }
        self.wait(for: [fetchViableExpectation], timeout: 10)

        //now call fetchviablebottles, we should get the cached version
        let fetchViableBottlesExpectation = self.expectation(description: "fetch Cached ViableBottles")
        fetchViableBottlesExpectation.isInverted = true

        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            fetchViableBottlesExpectation.fulfill()
            return nil
        }
        let fetchExpectation = self.expectation(description: "fetchExpectation callback occurs")

        self.cuttlefishContext.rpcFetchAllViableBottles { viable, _, error in
            XCTAssertNil(error, "should be no error fetching viable bottles")
            XCTAssert(viable?.contains(bottle.bottleID) ?? false, "The bottle we're about to restore should be viable")
            fetchExpectation.fulfill()
        }
        self.wait(for: [fetchExpectation], timeout: 10)
        self.wait(for: [fetchViableBottlesExpectation], timeout: 10)
    }

    func testViableBottleCachingAfterJoin() throws {
        OctagonSetOptimizationEnabled(false)
        let initiatorContextID = "initiator-context-id"
        let bottlerContext = self.makeInitiatorContext(contextID: initiatorContextID)

        bottlerContext.startOctagonStateMachine()
        let ckacctinfo = CKAccountInfo()
        ckacctinfo.accountStatus = .available
        ckacctinfo.hasValidCredentials = true
        ckacctinfo.accountPartition = .production

        bottlerContext.cloudkitAccountStateChange(nil, to: ckacctinfo)
        XCTAssertNoThrow(try bottlerContext.setCDPEnabled())
        self.assertEnters(context: bottlerContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = initiatorContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = self.mockAuthKit.altDSID!
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: bottlerContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: bottlerContext)

        let entropy = try self.loadSecret(label: clique.cliqueMemberIdentifier!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: bottlerContext)

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let joinWithBottleExpectation = self.expectation(description: "joinWithBottle callback occurs")
        self.cuttlefishContext.join(withBottle: bottle.bottleID, entropy: entropy!, bottleSalt: self.otcliqueContext.altDSID!) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithBottleExpectation.fulfill()
        }

        self.wait(for: [joinWithBottleExpectation], timeout: 100)

        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(withContainer: OTCKContainerName, context: OTDefaultContext) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        self.verifyDatabaseMocks()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertTLKSharesInCloudKit(receiver: self.cuttlefishContext, sender: self.cuttlefishContext)

        //now call fetchviablebottles, we should get the uncached version
        let fetchUnCachedViableBottlesExpectation = self.expectation(description: "fetch UnCached ViableBottles")

        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            fetchUnCachedViableBottlesExpectation.fulfill()
            return nil
        }
        let FetchAllViableBottles = self.expectation(description: "FetchAllViableBottles callback occurs")

        self.cuttlefishContext.rpcFetchAllViableBottles { viable, _, error in
            XCTAssertNil(error, "should be no error fetching viable bottles")
            XCTAssert(viable?.contains(bottle.bottleID) ?? false, "The bottle we're about to restore should be viable")
            FetchAllViableBottles.fulfill()
        }
        self.wait(for: [FetchAllViableBottles], timeout: 10)
        self.wait(for: [fetchUnCachedViableBottlesExpectation], timeout: 10)

        let fetchViableExpectation = self.expectation(description: "fetchViableBottles callback occurs")
        self.cuttlefishContext.rpcFetchAllViableBottles { viable, _, error in
            XCTAssertNil(error, "should be no error fetching viable bottles")
            XCTAssert(viable?.contains(bottle.bottleID) ?? false, "The bottle we're about to restore should be viable")
            fetchViableExpectation.fulfill()
        }
        self.wait(for: [fetchViableExpectation], timeout: 10)

        //now call fetchviablebottles, we should get the cached version
        let fetchViableBottlesExpectation = self.expectation(description: "fetch Cached ViableBottles")
        fetchViableBottlesExpectation.isInverted = true

        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            fetchViableBottlesExpectation.fulfill()
            return nil
        }
        let fetchExpectation = self.expectation(description: "fetchExpectation callback occurs")

        self.cuttlefishContext.rpcFetchAllViableBottles { viable, _, error in
            XCTAssertNil(error, "should be no error fetching viable bottles")
            XCTAssert(viable?.contains(bottle.bottleID) ?? false, "The bottle we're about to restore should be viable")
            fetchExpectation.fulfill()
        }
        self.wait(for: [fetchExpectation], timeout: 10)
        self.wait(for: [fetchViableBottlesExpectation], timeout: 10)
    }

    func testViableBottleReturns1Bottle() throws {
        let initiatorContextID = "initiator-context-id"
        let bottlerContext = self.makeInitiatorContext(contextID: initiatorContextID)

        bottlerContext.startOctagonStateMachine()
        let ckacctinfo = CKAccountInfo()
        ckacctinfo.accountStatus = .available
        ckacctinfo.hasValidCredentials = true
        ckacctinfo.accountPartition = .production

        bottlerContext.cloudkitAccountStateChange(nil, to: ckacctinfo)
        XCTAssertNoThrow(try bottlerContext.setCDPEnabled())
        self.assertEnters(context: bottlerContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = initiatorContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = self.mockAuthKit.altDSID!
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: bottlerContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: bottlerContext)

        let entropy = try self.loadSecret(label: clique.cliqueMemberIdentifier!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: bottlerContext)

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let joinWithBottleExpectation = self.expectation(description: "joinWithBottle callback occurs")
        self.cuttlefishContext.join(withBottle: bottle.bottleID, entropy: entropy!, bottleSalt: self.otcliqueContext.altDSID!) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithBottleExpectation.fulfill()
        }

        self.wait(for: [joinWithBottleExpectation], timeout: 100)

        var egoPeerID: String?

        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(withContainer: OTCKContainerName, context: OTDefaultContext) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            egoPeerID = egoSelf!["peerID"] as? String
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        self.verifyDatabaseMocks()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertTLKSharesInCloudKit(receiver: self.cuttlefishContext, sender: self.cuttlefishContext)

        let bottles: [Bottle] = self.fakeCuttlefishServer.state.bottles
        var bottleToExclude: String?
        bottles.forEach { bottle in
            if bottle.peerID == egoPeerID {
                bottleToExclude = bottle.bottleID
            }
        }

        XCTAssertNotNil(bottleToExclude, "bottleToExclude should not be nil")

        //now call fetchviablebottles, we should get the uncached version
        var fetchUnCachedViableBottlesExpectation = self.expectation(description: "fetch UnCached ViableBottles")

        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            fetchUnCachedViableBottlesExpectation.fulfill()
            return nil
        }
        self.fakeCuttlefishServer.fetchViableBottlesDontReturnBottleWithID = bottleToExclude
        var FetchAllViableBottles = self.expectation(description: "FetchAllViableBottles callback occurs")

        self.cuttlefishContext.rpcFetchAllViableBottles { viable, _, error in
            XCTAssertNil(error, "should be no error fetching viable bottles")
            XCTAssert(viable?.contains(bottle.bottleID) ?? false, "The bottle we're about to restore should be viable")
            FetchAllViableBottles.fulfill()
        }
        self.wait(for: [FetchAllViableBottles], timeout: 10)
        self.wait(for: [fetchUnCachedViableBottlesExpectation], timeout: 10)

        //now call fetchviablebottles, we should get the cached version
        fetchUnCachedViableBottlesExpectation = self.expectation(description: "fetch cached ViableBottles")
        fetchUnCachedViableBottlesExpectation.isInverted = true

        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            fetchUnCachedViableBottlesExpectation.fulfill()
            return nil
        }

        self.fakeCuttlefishServer.fetchViableBottlesDontReturnBottleWithID = bottleToExclude

        FetchAllViableBottles = self.expectation(description: "FetchAllViableBottles callback occurs")

        self.cuttlefishContext.rpcFetchAllViableBottles { viable, _, error in
            XCTAssertNil(error, "should be no error fetching viable bottles")
            XCTAssert(viable?.contains(bottle.bottleID) ?? false, "The bottle we're about to restore should be viable")
            FetchAllViableBottles.fulfill()
        }
        self.wait(for: [FetchAllViableBottles], timeout: 10)
        self.wait(for: [fetchUnCachedViableBottlesExpectation], timeout: 1)
    }

    func testRecoverTLKSharesSendByPeers() throws {
        // First, set up the world: two peers, one of which has sent TLKs to itself and the other
        let noSelfSharesContext = self.makeInitiatorContext(contextID: "noShares", authKitAdapter: self.mockAuthKit2)
        let allSharesContext = self.makeInitiatorContext(contextID: "allShares", authKitAdapter: self.mockAuthKit3)

        self.startCKAccountStatusMock()
        let noSelfSharesPeerID = self.assertResetAndBecomeTrusted(context: noSelfSharesContext)
        let allSharesPeerID = self.assertJoinViaEscrowRecovery(joiningContext: allSharesContext, sponsor: noSelfSharesContext)

        self.sendContainerChangeWaitForFetch(context: noSelfSharesContext)
        self.assertEnters(context: noSelfSharesContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: noSelfSharesPeerID, opinion: .trusts, target: allSharesPeerID)),
                      "noShares should trust allShares")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: noSelfSharesPeerID, opinion: .trusts, target: noSelfSharesPeerID)),
                      "No shares should trust itself")

        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: allSharesContext)
        try self.putAllTLKSharesInCloudKit(to: noSelfSharesContext, from: allSharesContext)

        self.ckksZones.forEach { zone in
            XCTAssertFalse(self.tlkShareInCloudKit(receiverPeerID: noSelfSharesPeerID, senderPeerID: noSelfSharesPeerID, zoneID: zone as! CKRecordZone.ID), "Should not have self shares for noSelfShares")
            XCTAssertTrue(self.tlkShareInCloudKit(receiverPeerID: noSelfSharesPeerID, senderPeerID: allSharesPeerID, zoneID: zone as! CKRecordZone.ID), "Should have a share for noSelfShares from allShares")
        }

        // Now, recover from noSelfShares
        self.assertJoinViaEscrowRecovery(joiningContext: self.cuttlefishContext, sponsor: noSelfSharesContext)
        // And CKKS should enter ready!
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
        self.assertTLKSharesInCloudKit(receiver: self.cuttlefishContext, sender: self.cuttlefishContext)
    }

    func testRecoverTLKSharesSentByPeersAfterCKKSFetchTimeout() throws {
        // First, set up the world: two peers, one of which has sent TLKs to itself and the other
        let noSelfSharesContext = self.makeInitiatorContext(contextID: "noShares", authKitAdapter: self.mockAuthKit2)
        let allSharesContext = self.makeInitiatorContext(contextID: "allShares", authKitAdapter: self.mockAuthKit3)

        self.startCKAccountStatusMock()
        let noSelfSharesPeerID = self.assertResetAndBecomeTrusted(context: noSelfSharesContext)
        let allSharesPeerID = self.assertJoinViaEscrowRecovery(joiningContext: allSharesContext, sponsor: noSelfSharesContext)

        self.sendContainerChangeWaitForFetch(context: noSelfSharesContext)
        self.assertEnters(context: noSelfSharesContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: noSelfSharesPeerID, opinion: .trusts, target: allSharesPeerID)),
                      "noShares should trust allShares")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: noSelfSharesPeerID, opinion: .trusts, target: noSelfSharesPeerID)),
                      "No shares should trust itself")

        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: allSharesContext)
        try self.putAllTLKSharesInCloudKit(to: noSelfSharesContext, from: allSharesContext)

        self.ckksZones.forEach { zone in
            XCTAssertFalse(self.tlkShareInCloudKit(receiverPeerID: noSelfSharesPeerID, senderPeerID: noSelfSharesPeerID, zoneID: zone as! CKRecordZone.ID), "Should not have self shares for noSelfShares")
            XCTAssertTrue(self.tlkShareInCloudKit(receiverPeerID: noSelfSharesPeerID, senderPeerID: allSharesPeerID, zoneID: zone as! CKRecordZone.ID), "Should have a share for noSelfShares from allShares")
        }

        // Simulate CKKS fetches taking forever. In practice, this is caused by many round-trip fetches to CK happening over minutes.
        self.holdCloudKitFetches()

        self.assertJoinViaEscrowRecovery(joiningContext: self.cuttlefishContext, sponsor: noSelfSharesContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateFetch, within: 10 * NSEC_PER_SEC)

        // now, let CKKS fetch and become ready
        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.releaseCloudKitFetchHold()
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
        self.assertTLKSharesInCloudKit(receiver: self.cuttlefishContext, sender: self.cuttlefishContext)
    }

    func testJoinWithBottleCreatedBeforeSignin() throws {
        // A remote device creates the keys before CKKS starts up at all
        self.putFakeKeyHierarchiesInCloudKit()

        self.startCKAccountStatusMock()

        let remote = self.makeInitiatorContext(contextID: "remote")
        let remotePeerID = self.assertResetAndBecomeTrusted(context: remote)

        // Note that depending on when CKKS starts up, it may or may not have received this in its initial fetch. But, Octagon should force a fetch during signin anyway!
        try self.putSelfTLKSharesInCloudKit(context: remote)

        let entropy = try self.loadSecret(label: remotePeerID)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        self.assertJoinViaEscrowRecovery(joiningContext: self.cuttlefishContext, sponsor: remote)

        self.verifyDatabaseMocks()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertTLKSharesInCloudKit(receiver: self.cuttlefishContext, sender: self.cuttlefishContext)
    }

    func testJoinWithBottleCreatedAfterInitialFetch() throws {
        // Our local machine signs into CloudKit, and becomes untrusted

        self.startCKAccountStatusMock()

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        // Now, another device comes along and performs an establish
        let remote = self.makeInitiatorContext(contextID: "remote")
        let remotePeerID = self.assertResetAndBecomeTrusted(context: remote)

        let entropy = try self.loadSecret(label: remotePeerID)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: remote)

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        // And now our local machine is told to restore from the newly-joined peer
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.otcliqueContext.sbd = OTMockSecureBackup(bottleID: bottle.bottleID, entropy: entropy)

        let newClique: OTClique
        do {
            newClique = try OTClique.performEscrowRecovery(withContextData: self.otcliqueContext, escrowArguments: [:])
            XCTAssertNotNil(newClique, "newClique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored recovering: \(error)")
            throw error
        }

        self.verifyDatabaseMocks()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertTLKSharesInCloudKit(receiver: self.cuttlefishContext, sender: self.cuttlefishContext)
    }

    func testRestoreUsingRecoveryKey() throws {
        OctagonRecoveryKeySetIsEnabled(true)
        self.manager.setSOSEnabledForPlatformFlag(false)
        self.startCKAccountStatusMock()

        let recoveryKey = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        self.manager.setSOSEnabledForPlatformFlag(true)

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: self.cuttlefishContext)

        //joining via recovery key just as if SBD kicked off a join during _recoverWithRequest()
        let joinWithRecoveryKeyExpectation = self.expectation(description: "joinWithRecoveryKey callback occurs")

        self.manager.join(withRecoveryKey: OTCKContainerName, contextID: OTDefaultContext, recoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithRecoveryKeyExpectation], timeout: 10)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
      
        var peerIDBeforeRestore: String?
        
        var dumpExpectation = self.expectation(description: "dump callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) { dump, error in
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
        newOTCliqueContext.dsid = self.otcliqueContext.dsid
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
        
        //ensure the ego peer id hasn't changed
        dumpExpectation = self.expectation(description: "dump callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) { dump, error in
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
}

#endif
