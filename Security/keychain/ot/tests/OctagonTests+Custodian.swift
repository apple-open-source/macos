#if OCTAGON

@objcMembers
class OctagonCustodianTests: OctagonTestsBase {
    override func setUp() {
        // Please don't make the SOS API calls, no matter what
        OctagonSetSOSFeatureEnabled(false)
        super.setUp()
    }

    func createClique(contextData: OTConfigurationContext) throws -> OTClique {
        let oldFF = OctagonIsSOSFeatureEnabled()
        defer { OctagonSetSOSFeatureEnabled(oldFF) }
        OctagonSetSOSFeatureEnabled(false)
        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: contextData, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            return clique
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }
    }

    func createClique() throws -> OTClique {
        return try self.createClique(contextData: self.otcliqueContext)
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

    func createAndSetCustodianRecoveryKey(context: OTCuttlefishContext) throws -> (OTCustodianRecoveryKey, CustodianRecoveryKey) {
        var retCRK: OTCustodianRecoveryKey?
        let createCustodianRecoveryKeyExpectation = self.expectation(description: "createCustodianRecoveryKey returns")
        self.manager.createCustodianRecoveryKey(self.otcontrolArgumentsFor(context: context), uuid: nil) { crk, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNotNil(crk, "crk should be non-nil")
            XCTAssertNotNil(crk?.uuid, "uuid should be non-nil")
            retCRK = crk
            createCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [createCustodianRecoveryKeyExpectation], timeout: 10)

        let otcrk = try XCTUnwrap(retCRK)

        self.assertEnters(context: context, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        let container = try self.tphClient.getContainer(with: try XCTUnwrap(context.activeAccount))
        let custodian = try XCTUnwrap(container.model.findCustodianRecoveryKey(with: otcrk.uuid))

        let crkWithKeys = try CustodianRecoveryKey(tpCustodian: custodian,
                                                   recoveryKeyString: otcrk.recoveryString,
                                                   recoverySalt: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))

        return (otcrk, crkWithKeys)
    }

    func testCreateCustodianTLKSharesDuringCreation() throws {
        try self.skipOnRecoveryKeyNotSupported()
        OctagonSetSOSFeatureEnabled(false)
        self.startCKAccountStatusMock()

        self.assertResetAndBecomeTrustedInDefaultContext()

        // This flag gates whether or not we'll error while setting the recovery key
        OctagonSetSOSFeatureEnabled(true)

        let (_, crk) = try self.createAndSetCustodianRecoveryKey(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.assertTLKSharesInCloudKit(receiverPeerID: crk.peerID, senderPeerID: crk.peerID)
    }

    func testJoinWithCustodianRecoveryKeyWithCKKSConflict() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        let remote = self.createEstablishContext(contextID: "remote")
        self.assertResetAndBecomeTrusted(context: remote)

        OctagonSetSOSFeatureEnabled(true)
        let (otcrk, crk) = try self.createAndSetCustodianRecoveryKey(context: remote)
        OctagonSetSOSFeatureEnabled(false)
        self.sendContainerChangeWaitForFetch(context: remote)

        self.silentFetchesAllowed = false
        self.expectCKFetchAndRun {
            self.putFakeKeyHierarchiesInCloudKit()
            self.putFakeDeviceStatusesInCloudKit()
            self.silentFetchesAllowed = true
        }

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: self.cuttlefishContext)

        let joinWithCustodianRecoveryKeyExpectation = self.expectation(description: "joinWithCustodianRecoveryKey callback occurs")
        self.cuttlefishContext.join(with: otcrk) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithCustodianRecoveryKeyExpectation], timeout: 20)

        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLK, within: 10 * NSEC_PER_SEC)

        let recoveryContextPeerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        let remoteContextPeerID = try remote.accountMetadataStore.getEgoPeerID()

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: recoveryContextPeerID, opinion: .trusts, target: remoteContextPeerID)),
                      "joined peer should trust the remote peer")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: recoveryContextPeerID, opinion: .trusts, target: crk.peerID)),
                      "joined peer should trust custodian peer ID")

        // WaitForTLK means no shares for anyone
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: recoveryContextPeerID, senderPeerID: recoveryContextPeerID))
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: crk.peerID, senderPeerID: recoveryContextPeerID))
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: remoteContextPeerID, senderPeerID: recoveryContextPeerID))
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: crk.peerID, senderPeerID: recoveryContextPeerID))

        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: recoveryContextPeerID, senderPeerID: crk.peerID), "Should be no shares from the CRK to any context")
    }

    func testCRKRecoveryRecoversCKKSCreatedShares() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        let remote = self.createEstablishContext(contextID: "remote")
        self.assertResetAndBecomeTrusted(context: remote)

        OctagonSetSOSFeatureEnabled(true)
        let (otcrk, crk) = try self.createAndSetCustodianRecoveryKey(context: remote)
        OctagonSetSOSFeatureEnabled(false)

        // And TLKShares for the RK are sent from the Octagon peer
        self.putFakeKeyHierarchiesInCloudKit()
        self.putFakeDeviceStatusesInCloudKit()

        try self.putSelfTLKSharesInCloudKit(context: remote)
        try self.putCustodianTLKSharesInCloudKit(crk: crk, sender: remote)
        XCTAssertTrue(try self.custodianTLKSharesInCloudKit(crk: crk, sender: remote))

        // Now, join! This should recover the TLKs.
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let preflightJoinWithCustodianRecoveryKeyExpectation = self.expectation(description: "preflightJoinWithCustodianRecoveryKey callback occurs")
        remote.preflightJoin(with: otcrk) { error in
            XCTAssertNil(error, "error should be nil")
            preflightJoinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [preflightJoinWithCustodianRecoveryKeyExpectation], timeout: 20)

        let joinWithCustodianRecoveryKeyExpectation = self.expectation(description: "joinWithCustodianRecoveryKey callback occurs")
        self.cuttlefishContext.join(with: otcrk) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithCustodianRecoveryKeyExpectation], timeout: 20)

        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.verifyDatabaseMocks()
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        let joinedPeerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        let remotePeerID = try remote.accountMetadataStore.getEgoPeerID()

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: joinedPeerID, opinion: .trusts, target: remotePeerID)),
                      "joined peer should trust the remote peer")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: joinedPeerID, opinion: .trusts, target: crk.peerID)),
                      "joined peer should trust custodian peer ID")

        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: joinedPeerID, senderPeerID: remotePeerID), "Remote peer isn't running CKKS in tests; should not send us shares")
        XCTAssertFalse(try self.custodianTLKSharesInCloudKit(crk: crk, sender: self.cuttlefishContext), "Joined peer should not send new TLKShares to CRK")
        self.assertSelfTLKSharesInCloudKit(peerID: joinedPeerID)
    }

    func testRecoverTLKSharesSentToCRKBeforeCKKSFetchCompletes() throws {
        try self.skipOnRecoveryKeyNotSupported()
        OctagonSetSOSFeatureEnabled(false)
        self.startCKAccountStatusMock()

        let remote = self.createEstablishContext(contextID: "remote")
        self.assertResetAndBecomeTrusted(context: remote)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: remote)
        self.assertSelfTLKSharesInCloudKit(context: remote)

        OctagonSetSOSFeatureEnabled(true)
        let (otcrk, crk) = try self.createAndSetCustodianRecoveryKey(context: remote)

        self.putCustodianTLKSharesInCloudKit(crk: crk)

        // Now, join from a new device
        // Simulate CKKS fetches taking forever. In practice, this is caused by many round-trip fetches to CK happening over minutes.
        self.holdCloudKitFetches()

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // device succeeds joining after restore
        self.mockSOSAdapter!.joinAfterRestoreResult = true
        // reset to offering on the mock adapter is by default set to false so this should cause cascading failures resulting in a cfu
        self.mockSOSAdapter!.joinAfterRestoreCircleStatusOverride = true
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCRequestPending)
        self.mockSOSAdapter!.resetToOfferingCircleStatusOverride = true

        let preflightJoinWithCustodianRecoveryKeyExpectation = self.expectation(description: "preflightJoinWithCustodianRecoveryKey callback occurs")
        remote.preflightJoin(with: otcrk) { error in
            XCTAssertNil(error, "error should be nil")
            preflightJoinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [preflightJoinWithCustodianRecoveryKeyExpectation], timeout: 20)

        let joinWithCustodianRecoveryKeyExpectation = self.expectation(description: "joinWithCustodianRecoveryKey callback occurs")
        self.cuttlefishContext.join(with: otcrk) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithCustodianRecoveryKeyExpectation], timeout: 20)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateFetch, within: 10 * NSEC_PER_SEC)

        // When Octagon is creating itself TLKShares as part of the escrow recovery, CKKS will get into the right state without any uploads

        self.releaseCloudKitFetchHold()
        self.verifyDatabaseMocks()

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        let remotePeerID = try remote.accountMetadataStore.getEgoPeerID()
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: crk.peerID, senderPeerID: remotePeerID), "Should be no shares from peer to CRK; as CRK has self-shares")
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: remotePeerID, senderPeerID: crk.peerID), "Should be no shares from crk to peer")
        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        XCTAssertEqual(self.mockSOSAdapter!.circleStatus, SOSCCStatus(kSOSCCRequestPending), "SOS should be Request Pending")
    }

    func testAddCustodianRecoveryKey() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        self.putFakeKeyHierarchiesInCloudKit()
        self.silentZoneDeletesAllowed = true

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(self.mockAuthKit.currentDeviceList().isEmpty, "should not have zero devices")

        _ = try self.createClique()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        OctagonSetSOSFeatureEnabled(true)

        let createCustodianRecoveryKeyExpectation = self.expectation(description: "createCustodianRecoveryKey returns")
        self.manager.createCustodianRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), uuid: nil) { crk, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNotNil(crk, "crk should be non-nil")
            XCTAssertNotNil(crk!.uuid, "uuid should be non-nil")
            createCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [createCustodianRecoveryKeyExpectation], timeout: 10)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func testAddCustodianRecoveryKeyAndCKKSTLKSharesHappen() throws {
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

        _ = try self.createClique()

        // Now, we should be in 'ready'
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertConsidersSelfTrustedCachedAccountStatus(context: self.cuttlefishContext)

        // and all subCKKSes should enter waitfortlk, as they don't have the TLKs uploaded by the other peer
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLK, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // And a custodian recovery key is set
        var retCRK: OTCustodianRecoveryKey?
        let createCustodianRecoveryKeyExpectation = self.expectation(description: "createCustodianRecoveryKey returns")
        self.manager.createCustodianRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), uuid: nil) { crk, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNotNil(crk, "crk should be non-nil")
            XCTAssertNotNil(crk!.uuid, "uuid should be non-nil")
            retCRK = crk
            createCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [createCustodianRecoveryKeyExpectation], timeout: 10)

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLK, within: 10 * NSEC_PER_SEC)

        // and now, all TLKs arrive! CKKS should upload two shares: one for itself, and one for the custodian recovery key
        self.assertAllCKKSViewsUpload(tlkShares: 2)
        self.saveTLKMaterialToKeychain()

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        let container = try self.tphClient.getContainer(with: try XCTUnwrap(self.cuttlefishContext.activeAccount))
        let custodian = try XCTUnwrap(container.model.findCustodianRecoveryKey(with: retCRK!.uuid), "Should be able to find the CRK we just created")
        let cuttlefishContextPeerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        self.assertTLKSharesInCloudKit(receiverPeerID: custodian.peerID, senderPeerID: cuttlefishContextPeerID)
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: cuttlefishContextPeerID, senderPeerID: custodian.peerID), "CRK should not create TLKShares to existing peers")
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: custodian.peerID, senderPeerID: custodian.peerID), "Should be no shares from the CRK to itself")
    }

    func testAddCustodianRecoveryKeyUUID() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        self.putFakeKeyHierarchiesInCloudKit()
        self.silentZoneDeletesAllowed = true

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(self.mockAuthKit.currentDeviceList().isEmpty, "should not have zero devices")

        _ = try self.createClique()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)

        OctagonSetSOSFeatureEnabled(true)

        let createCustodianRecoveryKeyExpectation = self.expectation(description: "createCustodianRecoveryKey returns")
        let uuid = UUID()
        self.manager.createCustodianRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), uuid: uuid) { crk, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNotNil(crk, "crk should be non-nil")
            XCTAssertEqual(uuid, crk!.uuid)
            createCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [createCustodianRecoveryKeyExpectation], timeout: 10)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func testAddCustodianRecoveryKeyUUIDTwice() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        self.putFakeKeyHierarchiesInCloudKit()
        self.silentZoneDeletesAllowed = true

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(self.mockAuthKit.currentDeviceList().isEmpty, "should not have zero devices")

        _ = try self.createClique()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)

        OctagonSetSOSFeatureEnabled(true)

        let createCustodianRecoveryKeyExpectation = self.expectation(description: "createCustodianRecoveryKey returns")
        let uuid = UUID()
        self.manager.createCustodianRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), uuid: uuid) { crk, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNotNil(crk, "crk should be non-nil")
            XCTAssertEqual(uuid, crk!.uuid)
            createCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [createCustodianRecoveryKeyExpectation], timeout: 10)

        let createCustodianRecoveryKeyExpectation2 = self.expectation(description: "createCustodianRecoveryKey2 returns")
        self.manager.createCustodianRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), uuid: uuid) { crk, error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual("com.apple.security.trustedpeers.container", (error! as NSError).domain, "error domain mismatch")
            XCTAssertEqual((error! as NSError).code, ContainerError.custodianRecoveryKeyUUIDExists.errorCode, "error code mismatch")
            XCTAssertNil(crk, "crk should be nil")
            createCustodianRecoveryKeyExpectation2.fulfill()
        }
        self.wait(for: [createCustodianRecoveryKeyExpectation2], timeout: 10)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func testAddRemoveAddCustodianRecoveryKeyUUID() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        self.putFakeKeyHierarchiesInCloudKit()
        self.silentZoneDeletesAllowed = true

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(self.mockAuthKit.currentDeviceList().isEmpty, "should not have zero devices")

        _ = try self.createClique()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)

        OctagonSetSOSFeatureEnabled(true)

        let createCustodianRecoveryKeyExpectation = self.expectation(description: "createCustodianRecoveryKey returns")
        let uuid = UUID()
        self.manager.createCustodianRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), uuid: uuid) { crk, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNotNil(crk, "crk should be non-nil")
            XCTAssertEqual(uuid, crk!.uuid)
            createCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [createCustodianRecoveryKeyExpectation], timeout: 10)

        let removeCustodianRecoveryKeyExpectation = self.expectation(description: "removeCustodianRecoveryKey returns")
        self.manager.removeCustodianRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), uuid: uuid) { error in
            XCTAssertNil(error, "error should be nil")
            removeCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [removeCustodianRecoveryKeyExpectation], timeout: 20)

        let createCustodianRecoveryKeyExpectation2 = self.expectation(description: "createCustodianRecoveryKey2 returns")
        self.manager.createCustodianRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), uuid: uuid) { crk, error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual("com.apple.security.trustedpeers.container", (error! as NSError).domain, "error domain mismatch")
            XCTAssertEqual((error! as NSError).code, ContainerError.custodianRecoveryKeyUUIDExists.errorCode, "error code mismatch")
            XCTAssertNil(crk, "crk should be nil")
            createCustodianRecoveryKeyExpectation2.fulfill()
        }
        self.wait(for: [createCustodianRecoveryKeyExpectation2], timeout: 10)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func testCustodianRecoveryKeyTrust() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(self.mockAuthKit.currentDeviceList().isEmpty, "should not have zero devices")

        _ = try self.createClique()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)

        OctagonSetSOSFeatureEnabled(true)

        let createCustodianRecoveryKeyExpectation = self.expectation(description: "createCustodianRecoveryKey returns")
        let uuid = UUID()
        self.manager.createCustodianRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), uuid: uuid) { crk, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNotNil(crk, "crk should be non-nil")
            XCTAssertEqual(uuid, crk!.uuid)
            createCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [createCustodianRecoveryKeyExpectation], timeout: 10)

        try self.assertTrusts(context: self.cuttlefishContext, includedPeerIDCount: 2, excludedPeerIDCount: 0)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.verifyDatabaseMocks()
    }

    func testJoinWithCustodianRecoveryKey() throws {
        try self.skipOnRecoveryKeyNotSupported()
        OctagonSetSOSFeatureEnabled(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl
        _ = try self.createClique(contextData: bottlerotcliqueContext)

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        let establishedPeerID = self.fetchEgoPeerID(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        OctagonSetSOSFeatureEnabled(true)

        let (otcrk, crk) = try self.createAndSetCustodianRecoveryKey(context: establishContext)

        self.putCustodianTLKSharesInCloudKit(crk: crk)
        self.sendContainerChangeWaitForFetch(context: establishContext)

        // Now, join from a new device
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: self.cuttlefishContext)

        let preflightJoinWithCustodianRecoveryKeyExpectation = self.expectation(description: "preflightJoinWithCustodianRecoveryKey callback occurs")
        establishContext.preflightJoin(with: otcrk) { error in
            XCTAssertNil(error, "error should be nil")
            preflightJoinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [preflightJoinWithCustodianRecoveryKeyExpectation], timeout: 20)

        let joinWithCustodianRecoveryKeyExpectation = self.expectation(description: "joinWithCustodianRecoveryKey callback occurs")
        self.cuttlefishContext.join(with: otcrk) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithCustodianRecoveryKeyExpectation], timeout: 20)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        let joinedPeerID = self.fetchEgoPeerID(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        let stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 3, "should be 3 peer ids")
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

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 3, "should be 3 peer ids")
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
        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: joinedPeerID, opinion: .trusts, target: crk.peerID)),
                      "establish peer should trust joined peer")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: establishedPeerID, opinion: .trusts, target: crk.peerID)),
                      "establish peer should trust joined peer")

        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: crk.peerID, senderPeerID: joinedPeerID), "Should be no shares to the CRK; it already has some")
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: joinedPeerID, senderPeerID: crk.peerID), "Should be no shares from a CRK to a peer")
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: establishedPeerID, senderPeerID: crk.peerID), "Should be no shares from a CRK to a peer")
    }

    func testJoinWithCustodianRecoveryKeyWithClique() throws {
        try self.skipOnRecoveryKeyNotSupported()
        OctagonSetSOSFeatureEnabled(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl
        _ = try self.createClique(contextData: bottlerotcliqueContext)

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        let establishedPeerID = self.fetchEgoPeerID(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        OctagonSetSOSFeatureEnabled(true)

        let (otcrk, crk) = try self.createAndSetCustodianRecoveryKey(context: establishContext)

        self.putCustodianTLKSharesInCloudKit(crk: crk)
        self.sendContainerChangeWaitForFetch(context: establishContext)

        // Now, join from a new device
        let newCliqueContext = OTConfigurationContext()
        newCliqueContext.context = OTDefaultContext
        newCliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit.primaryAltDSID())
        newCliqueContext.otControl = self.otControl

        let recoveryContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        recoveryContext.startOctagonStateMachine()
        self.assertEnters(context: recoveryContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: recoveryContext)

        let preflightJoinWithCustodianRecoveryKeyExpectation = self.expectation(description: "preflightJoinWithCustodianRecoveryKey callback occurs")
        OTClique.preflightRecoverOctagon(usingCustodianRecoveryKey: bottlerotcliqueContext, custodianRecoveryKey: otcrk) { error in
            XCTAssertNil(error, "error should be nil")
            preflightJoinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [preflightJoinWithCustodianRecoveryKeyExpectation], timeout: 20)

        let joinWithCustodianRecoveryKeyExpectation = self.expectation(description: "joinWithCustodianRecoveryKey callback occurs")
        OTClique.recoverOctagon(usingCustodianRecoveryKey: newCliqueContext, custodianRecoveryKey: otcrk) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithCustodianRecoveryKeyExpectation], timeout: 20)

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

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 3, "should be 3 peer ids")
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

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 3, "should be 3 peer ids")
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
        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: joinedPeerID, opinion: .trusts, target: crk.peerID)),
                      "establish peer should trust joined peer")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: establishedPeerID, opinion: .trusts, target: crk.peerID)),
                      "establish peer should trust joined peer")

        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: crk.peerID, senderPeerID: joinedPeerID), "Should be no shares to the CRK; it already has some")
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: joinedPeerID, senderPeerID: crk.peerID), "Should be no shares from a CRK to a peer")
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: establishedPeerID, senderPeerID: crk.peerID), "Should be no shares from a CRK to a peer")
    }

    func testJoinWithCustodianRecoveryKeyBadUUID() throws {
        try self.skipOnRecoveryKeyNotSupported()
        OctagonSetSOSFeatureEnabled(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl
        _ = try self.createClique(contextData: bottlerotcliqueContext)

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        OctagonSetSOSFeatureEnabled(true)

        let createCustodianRecoveryKeyExpectation = self.expectation(description: "createCustodianRecoveryExpectation returns")
        var crk: OTCustodianRecoveryKey?
        self.manager.createCustodianRecoveryKey(OTControlArguments(configuration: bottlerotcliqueContext), uuid: nil) { retcrk, error in
            XCTAssertNil(error, "error should be nil")
            crk = retcrk!
            XCTAssertNotNil(crk!.uuid, "uuid should not be nil")
            createCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [createCustodianRecoveryKeyExpectation], timeout: 10)
        let recoveryKey = crk!.recoveryString

        try self.putRecoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))
        self.sendContainerChangeWaitForFetch(context: establishContext)

        let newUUID = UUID()
        let crk2 = try OTCustodianRecoveryKey(uuid: newUUID, recoveryString: crk!.recoveryString)

        let preflightJoinWithCustodianRecoveryKeyExpectation = self.expectation(description: "preflightJoinWithCustodianRecoveryKey callback occurs")
        establishContext.preflightJoin(with: crk2) { error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual("com.apple.security.trustedpeers.container", (error! as NSError).domain, "error domain mismatch")
            XCTAssertEqual((error! as NSError).code, ContainerError.recoveryKeysNotEnrolled.errorCode, "error code mismatch")
            preflightJoinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [preflightJoinWithCustodianRecoveryKeyExpectation], timeout: 20)

        // Now, join from a new device
        let recoveryContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        recoveryContext.startOctagonStateMachine()
        self.assertEnters(context: recoveryContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: recoveryContext)

        let joinWithCustodianRecoveryKeyExpectation = self.expectation(description: "joinWithCustodianRecoveryKey callback occurs")

        recoveryContext.join(with: crk2) { error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual("com.apple.security.trustedpeers.container", (error! as NSError).domain, "error domain mismatch")
            XCTAssertEqual((error! as NSError).code, ContainerError.recoveryKeysNotEnrolled.errorCode, "error code mismatch")
            joinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithCustodianRecoveryKeyExpectation], timeout: 20)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTrust, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateWaitForTrust, within: 10 * NSEC_PER_SEC)
    }

    func testJoinWithCustodianRecoveryKeyBadKey() throws {
        try self.skipOnRecoveryKeyNotSupported()
        OctagonSetSOSFeatureEnabled(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl
        _ = try self.createClique(contextData: bottlerotcliqueContext)

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        OctagonSetSOSFeatureEnabled(true)

        let createCustodianRecoveryKeyExpectation = self.expectation(description: "createCustodianRecoveryExpectation returns")
        var crk: OTCustodianRecoveryKey?
        self.manager.createCustodianRecoveryKey(OTControlArguments(configuration: bottlerotcliqueContext), uuid: nil) { retcrk, error in
            XCTAssertNil(error, "error should be nil")
            crk = retcrk!
            XCTAssertNotNil(crk!.uuid, "uuid should not be nil")
            createCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [createCustodianRecoveryKeyExpectation], timeout: 10)
        let recoveryKey = crk!.recoveryString

        try self.putRecoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))
        self.sendContainerChangeWaitForFetch(context: establishContext)

        let anotherRecoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(anotherRecoveryKey, "SecRKCreateRecoveryKeyString failed")
        let crk2 = try OTCustodianRecoveryKey(uuid: crk!.uuid, recoveryString: anotherRecoveryKey!)

        let preflightJoinWithCustodianRecoveryKeyExpectation = self.expectation(description: "preflightJoinWithCustodianRecoveryKey callback occurs")
        establishContext.preflightJoin(with: crk2) { error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual("com.apple.security.trustedpeers.container", (error! as NSError).domain, "error domain mismatch")
            XCTAssertEqual((error! as NSError).code, ContainerError.failedToCreateRecoveryKey(suberror: ContainerError.unknownInternalError).errorCode, "error code mismatch")
            preflightJoinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [preflightJoinWithCustodianRecoveryKeyExpectation], timeout: 20)

        // Now, join from a new device
        let recoveryContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        recoveryContext.startOctagonStateMachine()
        self.assertEnters(context: recoveryContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: recoveryContext)

        let joinWithCustodianRecoveryKeyExpectation = self.expectation(description: "joinWithCustodianRecoveryKey callback occurs")
        recoveryContext.join(with: crk2) { error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual("com.apple.security.trustedpeers.container", (error! as NSError).domain, "error domain mismatch")
            XCTAssertEqual((error! as NSError).code, ContainerError.failedToCreateRecoveryKey(suberror: ContainerError.unknownInternalError).errorCode, "error code mismatch")
            joinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithCustodianRecoveryKeyExpectation], timeout: 20)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTrust, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateWaitForTrust, within: 10 * NSEC_PER_SEC)
    }

    func testJoinWithDistrustedCustodianRecoveryKey() throws {
        try self.skipOnRecoveryKeyNotSupported()
        OctagonSetSOSFeatureEnabled(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl
        let clique = try self.createClique(contextData: bottlerotcliqueContext)

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        OctagonSetSOSFeatureEnabled(true)

        let (otcrk, crk) = try self.createAndSetCustodianRecoveryKey(context: establishContext)

        let recoveryKey = otcrk.recoveryString

        try self.putRecoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))
        self.sendContainerChangeWaitForFetch(context: establishContext)

        XCTAssertNoThrow(try clique.removeFriends(inClique: [crk.peerID]), "Removing should not error")

        let preflightJoinWithCustodianRecoveryKeyExpectation = self.expectation(description: "preflightJoinWithCustodianRecoveryKey callback occurs")
        establishContext.preflightJoin(with: otcrk) { error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual("com.apple.security.trustedpeers.container", (error! as NSError).domain, "error domain mismatch")
            XCTAssertEqual((error! as NSError).code, ContainerError.untrustedRecoveryKeys.errorCode, "error code mismatch")
            preflightJoinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [preflightJoinWithCustodianRecoveryKeyExpectation], timeout: 20)

        // Now, join from a new device
        let recoveryContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        recoveryContext.startOctagonStateMachine()
        self.assertEnters(context: recoveryContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: recoveryContext)

        let joinWithCustodianRecoveryKeyExpectation = self.expectation(description: "joinWithCustodianRecoveryKey callback occurs")
        recoveryContext.join(with: otcrk) { error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual("com.apple.security.trustedpeers.container", (error! as NSError).domain, "error domain mismatch")
            XCTAssertEqual((error! as NSError).code, ContainerError.untrustedRecoveryKeys.errorCode, "error code mismatch")
            joinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithCustodianRecoveryKeyExpectation], timeout: 20)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTrust, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateWaitForTrust, within: 10 * NSEC_PER_SEC)
    }

    func testCRKWrapUnwrap() throws {
        let uuid = UUID()
        let crk1 = try OTCustodianRecoveryKey(uuid: uuid, recoveryString: "recoveryString")
        let crk2 = try OTCustodianRecoveryKey(wrappedKey: crk1.wrappedKey,
                                              wrappingKey: crk1.wrappingKey,
                                              uuid: uuid)
        XCTAssertEqual(crk1, crk2, "first CRK and reconstructed should match")
    }

    func testJoinWithCustodianRecoveryKeyFailsWithoutFetchRecoverableTLKShares() throws {
        try self.skipOnRecoveryKeyNotSupported()
        OctagonSetSOSFeatureEnabled(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        self.assertResetAndBecomeTrusted(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        OctagonSetSOSFeatureEnabled(true)

        let (otcrk, crk) = try self.createAndSetCustodianRecoveryKey(context: establishContext)

        self.putCustodianTLKSharesInCloudKit(crk: crk)
        self.sendContainerChangeWaitForFetch(context: establishContext)

        // Now, join from a new device
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // For this test, the fetchRecoverableTLKShares endpoint is suffering an outage. This should break custodian join.
        self.fakeCuttlefishServer.fetchRecoverableTLKSharesListener = { request in
            return NSError(domain: CKErrorDomain,
                           code: CKError.serverRejectedRequest.rawValue,
                           userInfo: [:])
        }

        self.sendContainerChangeWaitForUntrustedFetch(context: self.cuttlefishContext)

        let preflightJoinWithCustodianRecoveryKeyExpectation = self.expectation(description: "preflightJoinWithCustodianRecoveryKey callback occurs")
        establishContext.preflightJoin(with: otcrk) { error in
            XCTAssertNil(error, "error should be nil")
            preflightJoinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [preflightJoinWithCustodianRecoveryKeyExpectation], timeout: 20)

        let joinWithCustodianRecoveryKeyExpectation = self.expectation(description: "joinWithCustodianRecoveryKey callback occurs")
        self.cuttlefishContext.join(with: otcrk) { error in
            XCTAssertNotNil(error, "error should be non-nil")
            if let error {
                XCTAssertEqual((error as NSError).domain, CKErrorDomain, "error domain should be CloudKit")
                XCTAssertEqual((error as NSError).code, CKError.serverRejectedRequest.rawValue, "error code should be serverRejectedRequest")
            }
            joinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithCustodianRecoveryKeyExpectation], timeout: 20)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
    }

    func testRefetchOnUpgradeToCustodianCapableOS() throws {
        try self.skipOnRecoveryKeyNotSupported()
        OctagonSetSOSFeatureEnabled(false)
        self.startCKAccountStatusMock()

        let originalPeerID = self.assertResetAndBecomeTrustedInDefaultContext()

        let peerContext = self.makeInitiatorContext(contextID: "join-peer")
        let joinPeerID = self.assertJoinViaEscrowRecovery(joiningContext: peerContext, sponsor: self.cuttlefishContext)

        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // Now, our default context makes a recovery key

        // This flag gates whether or not we'll error while setting the recovery key
        OctagonSetSOSFeatureEnabled(true)

        let (_, crk) = try self.createAndSetCustodianRecoveryKey(context: self.cuttlefishContext)
        XCTAssertTrue(self.tlkSharesInCloudKit(receiverPeerID: crk.peerID, senderPeerID: crk.peerID))

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // Now, let's fake that the peerContext received these cuttlefish updates without receiving the custodian updates, because it was on old software at the time

        let container = try self.tphClient.getContainer(with: try XCTUnwrap(peerContext.activeAccount))
        container.testIgnoreCustodianUpdates = true

        self.sendContainerChangeWaitForFetch(context: peerContext)
        self.assertEnters(context: peerContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: originalPeerID, opinion: .trusts, target: crk.peerID)),
                      "peer 1 should trust uuid of Custodian")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: joinPeerID, opinion: .ignores, target: crk.peerID)),
                      "peer 2 should have no opinion of uuid of Custodian")

        // When the peer is restarted and next refetches, it should fetch from the beginning of time
        let fetchExpectation = self.expectation(description: "fetch occurs")
        self.fakeCuttlefishServer.fetchChangesListener = { request in
            XCTAssertEqual(request.changeToken, "", "Should be no change token in fetch")

            fetchExpectation.fulfill()
            self.fakeCuttlefishServer.fetchChangesListener = nil
            return nil
        }

        let restartedPeerContext = self.simulateRestart(context: peerContext)
        restartedPeerContext.notifyContainerChange(nil)

        self.wait(for: [fetchExpectation], timeout: 10)
        self.assertEnters(context: restartedPeerContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: originalPeerID, opinion: .trusts, target: crk.peerID)),
                      "peer 1 should trust uuid of Custodian")

        // The joining peer should trust the custodian key
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: joinPeerID, opinion: .trusts, target: crk.peerID)),
                      "peer 2 should trust uuid of Custodian")

        // Fetching again should have a change token
        do {
            let secondFetchExpectation = self.expectation(description: "fetch occurs")
            self.fakeCuttlefishServer.fetchChangesListener = { request in
                XCTAssertNotEqual(request.changeToken, "", "Should be some change token in fetch")

                secondFetchExpectation.fulfill()
                self.fakeCuttlefishServer.fetchChangesListener = nil
                return nil
            }

            restartedPeerContext.notifyContainerChange(nil)

            self.wait(for: [secondFetchExpectation], timeout: 10)
        }
    }

    func testRefetchOnlyOnceOnUpgradeToCustodianCapableOS() throws {
        try self.skipOnRecoveryKeyNotSupported()
        OctagonSetSOSFeatureEnabled(false)
        self.startCKAccountStatusMock()

        let originalPeerID = self.assertResetAndBecomeTrustedInDefaultContext()

        let peerContext = self.makeInitiatorContext(contextID: "join-peer")
        let joinPeerID = self.assertJoinViaEscrowRecovery(joiningContext: peerContext, sponsor: self.cuttlefishContext)

        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // Now, our default context makes a recovery key
        // This flag gates whether or not we'll error while setting the recovery key
        OctagonSetSOSFeatureEnabled(true)

        let (_, crk) = try self.createAndSetCustodianRecoveryKey(context: self.cuttlefishContext)
        XCTAssertTrue(self.tlkSharesInCloudKit(receiverPeerID: crk.peerID, senderPeerID: crk.peerID))

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // Now, let's fake that the peerContext received these cuttlefish updates without receiving the custodian updates, because it was on old software at the time

        let container = try self.tphClient.getContainer(with: try XCTUnwrap(peerContext.activeAccount))
        container.testIgnoreCustodianUpdates = true

        self.sendContainerChangeWaitForFetch(context: peerContext)
        self.assertEnters(context: peerContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: originalPeerID, opinion: .trusts, target: crk.peerID)),
                      "peer 1 should trust uuid of Custodian")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: joinPeerID, opinion: .ignores, target: crk.peerID)),
                      "peer 2 should have no opinion of uuid of Custodian")

        // When the peer is restarted and next refetches, it should fetch from the beginning of time
        let fetchExpectation = self.expectation(description: "fetch occurs")
        self.fakeCuttlefishServer.fetchChangesListener = { request in
            XCTAssertEqual(request.changeToken, "", "Should be no change token in fetch")

            fetchExpectation.fulfill()
            self.fakeCuttlefishServer.fetchChangesListener = nil
            return nil
        }

        // After the first refetch, the client will think it should refetch, but won't, because it has already done so on an OS that knows
        // about custodians.
        let restartedPeerContext = self.simulateRestart(context: peerContext)

        let container2 = try self.tphClient.getContainer(with: try XCTUnwrap(peerContext.activeAccount))
        container2.testIgnoreCustodianUpdates = true

        restartedPeerContext.notifyContainerChange(nil)

        self.wait(for: [fetchExpectation], timeout: 10)
        self.assertEnters(context: restartedPeerContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: originalPeerID, opinion: .trusts, target: crk.peerID)),
                      "peer 1 should trust uuid of Custodian")

        // The joining peer should still not have an opinion on the custodian key
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: joinPeerID, opinion: .ignores, target: crk.peerID)),
                      "peer 2 should have no opinion of uuid of Custodian")

        // Fetching again should have a change token
        do {
            let secondFetchExpectation = self.expectation(description: "fetch occurs")
            self.fakeCuttlefishServer.fetchChangesListener = { request in
                XCTAssertNotEqual(request.changeToken, "", "Should be some change token in fetch")

                secondFetchExpectation.fulfill()
                self.fakeCuttlefishServer.fetchChangesListener = nil
                return nil
            }

            restartedPeerContext.notifyContainerChange(nil)

            self.wait(for: [secondFetchExpectation], timeout: 10)
        }
    }

    func testJoinWithCustodianRecoveryKeyAndPromptCFUFromPrivateKeyNotAvailable() throws {
        try self.skipOnRecoveryKeyNotSupported()
        OctagonSetSOSFeatureEnabled(true)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl
        _ = try self.createClique(contextData: bottlerotcliqueContext)

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        OctagonSetSOSFeatureEnabled(true)

        let (otcrk, crk) = try self.createAndSetCustodianRecoveryKey(context: establishContext)

        self.putCustodianTLKSharesInCloudKit(crk: crk)
        self.sendContainerChangeWaitForFetch(context: establishContext)

        // Now, join from a new device
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: self.cuttlefishContext)

        let preflightJoinWithCustodianRecoveryKeyExpectation = self.expectation(description: "preflightJoinWithCustodianRecoveryKey callback occurs")
        establishContext.preflightJoin(with: otcrk) { error in
            XCTAssertNil(error, "error should be nil")
            preflightJoinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [preflightJoinWithCustodianRecoveryKeyExpectation], timeout: 20)

        let joinWithCustodianRecoveryKeyExpectation = self.expectation(description: "joinWithCustodianRecoveryKey callback occurs")
        self.cuttlefishContext.join(with: otcrk) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithCustodianRecoveryKeyExpectation], timeout: 20)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        XCTAssertEqual(self.mockSOSAdapter!.circleStatus, SOSCCStatus(kSOSCCNotInCircle), "SOS should NOT be in circle")
    }

    func testJoinWithCustodianRecoveryKeyAndPromptCFUNotInSOSCircle() throws {
        try self.skipOnRecoveryKeyNotSupported()
        OctagonSetSOSFeatureEnabled(true)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl
        _ = try self.createClique(contextData: bottlerotcliqueContext)

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        OctagonSetSOSFeatureEnabled(true)

        let (otcrk, crk) = try self.createAndSetCustodianRecoveryKey(context: establishContext)

        self.putCustodianTLKSharesInCloudKit(crk: crk)
        self.sendContainerChangeWaitForFetch(context: establishContext)

        // Now, join from a new device
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: self.cuttlefishContext)

        // device succeeds joining after restore
        self.mockSOSAdapter!.joinAfterRestoreResult = true
        // reset to offering on the mock adapter is by default set to false so this should cause cascading failures resulting in a cfu
        self.mockSOSAdapter!.joinAfterRestoreCircleStatusOverride = true
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCRequestPending)
        self.mockSOSAdapter!.resetToOfferingCircleStatusOverride = true

        let preflightJoinWithCustodianRecoveryKeyExpectation = self.expectation(description: "preflightJoinWithCustodianRecoveryKey callback occurs")
        establishContext.preflightJoin(with: otcrk) { error in
            XCTAssertNil(error, "error should be nil")
            preflightJoinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [preflightJoinWithCustodianRecoveryKeyExpectation], timeout: 20)

        let joinWithCustodianRecoveryKeyExpectation = self.expectation(description: "joinWithCustodianRecoveryKey callback occurs")
        self.cuttlefishContext.join(with: otcrk) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithCustodianRecoveryKeyExpectation], timeout: 20)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        XCTAssertEqual(self.mockSOSAdapter!.circleStatus, SOSCCStatus(kSOSCCRequestPending), "SOS should be Request Pending")
    }

    func testJoinWithCustodianRecoveryKeyAndJoinsSOS() throws {
        try self.skipOnRecoveryKeyNotSupported()
        OctagonSetSOSFeatureEnabled(true)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl
        _ = try self.createClique(contextData: bottlerotcliqueContext)

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        let (otcrk, crk) = try self.createAndSetCustodianRecoveryKey(context: establishContext)

        self.putCustodianTLKSharesInCloudKit(crk: crk)
        self.sendContainerChangeWaitForFetch(context: establishContext)

        // Now, join from a new device
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: self.cuttlefishContext)

        // device succeeds joining after restore
        self.mockSOSAdapter!.joinAfterRestoreResult = true

        let preflightJoinWithCustodianRecoveryKeyExpectation = self.expectation(description: "preflightJoinWithCustodianRecoveryKey callback occurs")
        establishContext.preflightJoin(with: otcrk) { error in
            XCTAssertNil(error, "error should be nil")
            preflightJoinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [preflightJoinWithCustodianRecoveryKeyExpectation], timeout: 20)

        let joinWithCustodianRecoveryKeyExpectation = self.expectation(description: "joinWithCustodianRecoveryKey callback occurs")
        self.cuttlefishContext.join(with: otcrk) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithCustodianRecoveryKeyExpectation], timeout: 20)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViewsUpload(tlkShares: 1)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.verifyDatabaseMocks()

        XCTAssertEqual(self.mockSOSAdapter!.circleStatus, SOSCCStatus(kSOSCCInCircle), "SOS should be in circle")
    }

    func testJoinWithCustodianRecoveryKeyAndJoinsSOSFromResetToOffering() throws {
        try self.skipOnRecoveryKeyNotSupported()
        OctagonSetSOSFeatureEnabled(true)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl
        _ = try self.createClique(contextData: bottlerotcliqueContext)

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        let (otcrk, crk) = try self.createAndSetCustodianRecoveryKey(context: establishContext)

        self.putCustodianTLKSharesInCloudKit(crk: crk)
        self.sendContainerChangeWaitForFetch(context: establishContext)

        // Now, join from a new device
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: self.cuttlefishContext)

        // device succeeds joining after restore
        self.mockSOSAdapter!.joinAfterRestoreResult = true
        self.mockSOSAdapter!.joinAfterRestoreCircleStatusOverride = true
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCRequestPending)
        self.mockSOSAdapter!.resetToOfferingResult = true

        let preflightJoinWithCustodianRecoveryKeyExpectation = self.expectation(description: "preflightJoinWithCustodianRecoveryKey callback occurs")
        establishContext.preflightJoin(with: otcrk) { error in
            XCTAssertNil(error, "error should be nil")
            preflightJoinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [preflightJoinWithCustodianRecoveryKeyExpectation], timeout: 20)

        let joinWithCustodianRecoveryKeyExpectation = self.expectation(description: "joinWithCustodianRecoveryKey callback occurs")
        self.cuttlefishContext.join(with: otcrk) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithCustodianRecoveryKeyExpectation], timeout: 20)

        self.assertAllCKKSViewsUpload(tlkShares: 1)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.verifyDatabaseMocks()

        XCTAssertEqual(self.mockSOSAdapter!.circleStatus, SOSCCStatus(kSOSCCInCircle), "SOS should be in circle")
    }

    func testAddRemoveCustodianRecoveryKey() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        self.putFakeKeyHierarchiesInCloudKit()
        self.silentZoneDeletesAllowed = true

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(self.mockAuthKit.currentDeviceList().isEmpty, "should not have zero devices")

        _ = try self.createClique()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        OctagonSetSOSFeatureEnabled(true)

        let (otcrk, _) = try self.createAndSetCustodianRecoveryKey(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        let removeCustodianRecoveryKeyExpectation = self.expectation(description: "removeCustodianRecoveryKey returns")
        self.manager.removeCustodianRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), uuid: otcrk.uuid) { error in
            XCTAssertNil(error, "error should be nil")
            removeCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [removeCustodianRecoveryKeyExpectation], timeout: 20)

        self.verifyDatabaseMocks()
    }

    func testAddRemoveCustodianRecoveryKeyClique() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        self.putFakeKeyHierarchiesInCloudKit()
        self.silentZoneDeletesAllowed = true

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(self.mockAuthKit.currentDeviceList().isEmpty, "should not have zero devices")

        _ = try self.createClique()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        OctagonSetSOSFeatureEnabled(true)

        let (otcrk, _) = try self.createAndSetCustodianRecoveryKey(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        let removeCustodianRecoveryKeyExpectation = self.expectation(description: "removeCustodianRecoveryKey returns")
        OTClique.removeCustodianRecoveryKey(self.otcliqueContext, custodianRecoveryKeyUUID: otcrk.uuid) { error in
            XCTAssertNil(error, "error should be nil")
            removeCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [removeCustodianRecoveryKeyExpectation], timeout: 20)

        self.verifyDatabaseMocks()
    }

    func testAddRemoveCustodianRecoveryKeyTwice() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        self.putFakeKeyHierarchiesInCloudKit()
        self.silentZoneDeletesAllowed = true

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(self.mockAuthKit.currentDeviceList().isEmpty, "should not have zero devices")

        _ = try self.createClique()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        OctagonSetSOSFeatureEnabled(true)

        let (otcrk, _) = try self.createAndSetCustodianRecoveryKey(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        let removeCustodianRecoveryKeyExpectation = self.expectation(description: "removeCustodianRecoveryKey returns")
        self.manager.removeCustodianRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), uuid: otcrk.uuid) { error in
            XCTAssertNil(error, "error should be nil")
            removeCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [removeCustodianRecoveryKeyExpectation], timeout: 20)

        let removeCustodianRecoveryKeyExpectation2 = self.expectation(description: "removeCustodianRecoveryKey2 returns")
        self.manager.removeCustodianRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), uuid: otcrk.uuid) { error in
            XCTAssertNil(error, "error should be nil")
            removeCustodianRecoveryKeyExpectation2.fulfill()
        }
        self.wait(for: [removeCustodianRecoveryKeyExpectation2], timeout: 20)

        self.verifyDatabaseMocks()
    }

    func testRemoveBadCustodianRecoveryKey() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        self.putFakeKeyHierarchiesInCloudKit()
        self.silentZoneDeletesAllowed = true

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(self.mockAuthKit.currentDeviceList().isEmpty, "should not have zero devices")

        _ = try self.createClique()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        OctagonSetSOSFeatureEnabled(true)

        let removeCustodianRecoveryKeyExpectation = self.expectation(description: "removeCustodianRecoveryKey returns")
        self.manager.removeCustodianRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), uuid: UUID()) { error in
            XCTAssertNotNil(error, "error should not be nil")
            removeCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [removeCustodianRecoveryKeyExpectation], timeout: 20)
    }

    func testJoinWithRemovedCustodianRecoveryKey() throws {
        try self.skipOnRecoveryKeyNotSupported()
        OctagonSetSOSFeatureEnabled(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.altDSID = try XCTUnwrap(self.mockAuthKit2.primaryAltDSID())
        bottlerotcliqueContext.otControl = self.otControl
        _ = try self.createClique(contextData: bottlerotcliqueContext)

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        OctagonSetSOSFeatureEnabled(true)

        let (otcrk, crk) = try self.createAndSetCustodianRecoveryKey(context: establishContext)

        self.putCustodianTLKSharesInCloudKit(crk: crk)
        self.sendContainerChangeWaitForFetch(context: establishContext)

        // Remove the CRK
        let removeCustodianRecoveryKeyExpectation = self.expectation(description: "removeCustodianRecoveryKey returns")
        self.manager.removeCustodianRecoveryKey(OTControlArguments(configuration: bottlerotcliqueContext), uuid: otcrk.uuid) { error in
            XCTAssertNil(error, "error should be nil")
            removeCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [removeCustodianRecoveryKeyExpectation], timeout: 20)

        // Now, join from a new device
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: self.cuttlefishContext)

        let preflightJoinWithCustodianRecoveryKeyExpectation = self.expectation(description: "preflightJoinWithCustodianRecoveryKey callback occurs")
        establishContext.preflightJoin(with: otcrk) { error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual("com.apple.security.trustedpeers.container", (error! as NSError).domain, "error domain mismatch")
            XCTAssertEqual((error! as NSError).code, ContainerError.untrustedRecoveryKeys.errorCode, "error code mismatch")
            preflightJoinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [preflightJoinWithCustodianRecoveryKeyExpectation], timeout: 20)

        let joinWithCustodianRecoveryKeyExpectation = self.expectation(description: "joinWithCustodianRecoveryKey callback occurs")
        self.cuttlefishContext.join(with: otcrk) { error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual("com.apple.security.trustedpeers.container", (error! as NSError).domain, "error domain mismatch")
            XCTAssertEqual((error! as NSError).code, ContainerError.untrustedRecoveryKeys.errorCode, "error code mismatch")
            joinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithCustodianRecoveryKeyExpectation], timeout: 20)
    }

    func testRemoveDistrustedCustodianRecoveryKey() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        self.putFakeKeyHierarchiesInCloudKit()
        self.silentZoneDeletesAllowed = true

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(self.mockAuthKit.currentDeviceList().isEmpty, "should not have zero devices")

        let clique = try self.createClique()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        OctagonSetSOSFeatureEnabled(true)

        let (otcrk, crk) = try self.createAndSetCustodianRecoveryKey(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertNoThrow(try clique.removeFriends(inClique: [crk.peerID]), "Removing should not error")

        let removeCustodianRecoveryKeyExpectation = self.expectation(description: "removeCustodianRecoveryKey returns")
        self.manager.removeCustodianRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), uuid: otcrk.uuid) { error in
            XCTAssertNil(error, "error should be nil")
            removeCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [removeCustodianRecoveryKeyExpectation], timeout: 20)

        self.verifyDatabaseMocks()
    }

    func testJoinAfterReboot() throws {
        try self.skipOnRecoveryKeyNotSupported()
        OctagonSetSOSFeatureEnabled(false)
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        _ = try self.createClique()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)

        OctagonSetSOSFeatureEnabled(true)

        let (otcrk, crk) = try self.createAndSetCustodianRecoveryKey(context: self.cuttlefishContext)

        self.putCustodianTLKSharesInCloudKit(crk: crk)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        // reboot and join on same device
        self.cuttlefishContext = self.simulateRestart(context: self.cuttlefishContext)

        let joinWithCustodianRecoveryKeyExpectation = self.expectation(description: "joinWithCustodianRecoveryKey callback occurs")
        self.cuttlefishContext.join(with: otcrk) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithCustodianRecoveryKeyExpectation], timeout: 20)
    }

    func testCreateRemoveRestartAttemptJoin() throws {
        try self.skipOnRecoveryKeyNotSupported()
        OctagonSetSOSFeatureEnabled(false)
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        _ = try self.createClique()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)

        OctagonSetSOSFeatureEnabled(true)

        let (otcrk, crk) = try self.createAndSetCustodianRecoveryKey(context: self.cuttlefishContext)

        self.putCustodianTLKSharesInCloudKit(crk: crk)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        // Remove the CRK
        let removeCustodianRecoveryKeyExpectation = self.expectation(description: "removeCustodianRecoveryKey returns")
        self.manager.removeCustodianRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), uuid: otcrk.uuid) { error in
            XCTAssertNil(error, "error should be nil")
            removeCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [removeCustodianRecoveryKeyExpectation], timeout: 20)

        // reboot and join on same device
        self.cuttlefishContext = self.simulateRestart(context: self.cuttlefishContext)

        let joinWithCustodianRecoveryKeyExpectation = self.expectation(description: "joinWithCustodianRecoveryKey callback occurs")
        self.cuttlefishContext.join(with: otcrk) { error in
            XCTAssertNotNil(error, "error should not be nil")
            joinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithCustodianRecoveryKeyExpectation], timeout: 20)
    }

    func testCustodianRecoveryKeyCheckNoKey() throws {
        try self.skipOnRecoveryKeyNotSupported()
        OctagonSetSOSFeatureEnabled(false)
        self.startCKAccountStatusMock()

        self.assertResetAndBecomeTrustedInDefaultContext()

        // This flag gates whether or not we'll error while setting the recovery key
        OctagonSetSOSFeatureEnabled(true)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        let checkCustodianRecoveryKeyExpectation = self.expectation(description: "checkCustodianRecoveryKey returns")
        self.manager.checkCustodianRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), uuid: UUID()) { exists, error in
            XCTAssertFalse(exists, "exists mismatch")
            XCTAssertNil(error, "error should be nil")
            checkCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [checkCustodianRecoveryKeyExpectation], timeout: 20)
        self.verifyDatabaseMocks()
    }

    func testCustodianRecoveryKeyExists() throws {
        try self.skipOnRecoveryKeyNotSupported()
        OctagonSetSOSFeatureEnabled(false)
        self.startCKAccountStatusMock()

        self.assertResetAndBecomeTrustedInDefaultContext()

        // This flag gates whether or not we'll error while setting the recovery key
        OctagonSetSOSFeatureEnabled(true)

        let (otcrk, crk) = try self.createAndSetCustodianRecoveryKey(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertTLKSharesInCloudKit(receiverPeerID: crk.peerID, senderPeerID: crk.peerID)

        let checkCustodianRecoveryKeyExpectation = self.expectation(description: "checkCustodianRecoveryKey returns")
        self.manager.checkCustodianRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), uuid: otcrk.uuid) { exists, error in
            XCTAssertTrue(exists, "exists mismatch")
            XCTAssertNil(error, "error should be nil")
            checkCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [checkCustodianRecoveryKeyExpectation], timeout: 20)

        let checkInheritanceKeyExpectation = self.expectation(description: "checkInheritanceKey returns")
        self.manager.checkInheritanceKey(OTControlArguments(configuration: self.otcliqueContext), uuid: otcrk.uuid) { exists, error in
            XCTAssertFalse(exists, "exists mismatch")
            XCTAssertNil(error, "error should be nil")
            checkInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [checkInheritanceKeyExpectation], timeout: 20)

        self.verifyDatabaseMocks()
    }

    func testCustodianRecoveryKeyNotExists() throws {
        try self.skipOnRecoveryKeyNotSupported()
        OctagonSetSOSFeatureEnabled(false)
        self.startCKAccountStatusMock()

        self.assertResetAndBecomeTrustedInDefaultContext()

        // This flag gates whether or not we'll error while setting the recovery key
        OctagonSetSOSFeatureEnabled(true)

        let (otcrk, crk) = try self.createAndSetCustodianRecoveryKey(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertTLKSharesInCloudKit(receiverPeerID: crk.peerID, senderPeerID: crk.peerID)

        // Remove the CRK
        let removeCustodianRecoveryKeyExpectation = self.expectation(description: "removeCustodianRecoveryKey returns")
        self.manager.removeCustodianRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), uuid: otcrk.uuid) { error in
            XCTAssertNil(error, "error should be nil")
            removeCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [removeCustodianRecoveryKeyExpectation], timeout: 20)

        let checkCustodianRecoveryKeyExpectation = self.expectation(description: "checkCustodianRecoveryKey returns")
        self.manager.checkCustodianRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), uuid: otcrk.uuid) { exists, error in
            // Removed CRKs should be exist=false, and error==untrustedRecoveryKeys
            XCTAssertFalse(exists, "exists should be false")
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual("com.apple.security.trustedpeers.container", (error! as NSError).domain, "error domain mismatch")
            XCTAssertEqual((error! as NSError).code, ContainerError.untrustedRecoveryKeys.errorCode, "error code mismatch")
            checkCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [checkCustodianRecoveryKeyExpectation], timeout: 20)

        self.verifyDatabaseMocks()
    }

    func testExcludedCustodianRecoveryKeyNotTrusted() throws {
        try self.skipOnRecoveryKeyNotSupported()
        OctagonSetSOSFeatureEnabled(false)
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let joiningContext = self.makeInitiatorContext(contextID: "joiner", authKitAdapter: self.mockAuthKit2)
        self.assertJoinViaEscrowRecoveryFromDefaultContextWithReciprocationAndTLKShares(joiningContext: joiningContext)

        let secondJoiningContext = self.makeInitiatorContext(contextID: "joiner2", authKitAdapter: self.mockAuthKit3)
        self.assertJoinViaEscrowRecoveryFromDefaultContextWithReciprocationAndTLKShares(joiningContext: secondJoiningContext)

        let (otcrk, _) = try self.createAndSetCustodianRecoveryKey(context: self.cuttlefishContext)

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: joiningContext)
        self.sendContainerChangeWaitForFetch(context: secondJoiningContext)

        let checkCustodianRecoveryKeyExpectation = self.expectation(description: "checkCustodianRecoveryKey returns")
        self.manager.checkCustodianRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), uuid: otcrk.uuid) { exists, error in
            XCTAssertTrue(exists, "exists mismatch")
            XCTAssertNil(error, "error should be nil")
            checkCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [checkCustodianRecoveryKeyExpectation], timeout: 20)

        let removeCustodianRecoveryKeyExpectation = self.expectation(description: "removeCustodianRecoveryKey returns")
        self.manager.removeCustodianRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), uuid: otcrk.uuid) { error in
            XCTAssertNil(error, "error should be nil")
            removeCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [removeCustodianRecoveryKeyExpectation], timeout: 20)

        self.sendContainerChangeWaitForFetch(context: joiningContext)
        self.sendContainerChangeWaitForFetch(context: secondJoiningContext)

        let check2Expectation = self.expectation(description: "check CRK on joiner")
        joiningContext.rpcCheckCustodianRecoveryKey(with: otcrk.uuid) { exists, error in
            XCTAssertFalse(exists, "exists mismatch")
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual("com.apple.security.trustedpeers.container", (error! as NSError).domain, "error domain mismatch")
            XCTAssertEqual((error! as NSError).code, ContainerError.untrustedRecoveryKeys.errorCode, "error code mismatch")
            check2Expectation.fulfill()
        }

        let check3Expectation = self.expectation(description: "check CRK on joiner2")
        secondJoiningContext.rpcCheckCustodianRecoveryKey(with: otcrk.uuid) { exists, error in
            XCTAssertFalse(exists, "exists mismatch")
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual("com.apple.security.trustedpeers.container", (error! as NSError).domain, "error domain mismatch")
            XCTAssertEqual((error! as NSError).code, ContainerError.untrustedRecoveryKeys.errorCode, "error code mismatch")
            check3Expectation.fulfill()
        }
        self.wait(for: [check2Expectation, check3Expectation], timeout: 10)
    }

    func testExcludedCustodianRecoveryKeyPreflight() throws {
        try self.skipOnRecoveryKeyNotSupported()
        OctagonSetSOSFeatureEnabled(false)
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let joiningContext = self.makeInitiatorContext(contextID: "joiner", authKitAdapter: self.mockAuthKit2)
        self.assertJoinViaEscrowRecoveryFromDefaultContextWithReciprocationAndTLKShares(joiningContext: joiningContext)

        let secondJoiningContext = self.makeInitiatorContext(contextID: "joiner2", authKitAdapter: self.mockAuthKit3)
        self.assertJoinViaEscrowRecoveryFromDefaultContextWithReciprocationAndTLKShares(joiningContext: secondJoiningContext)

        let (otcrk, _) = try self.createAndSetCustodianRecoveryKey(context: self.cuttlefishContext)

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: joiningContext)
        self.sendContainerChangeWaitForFetch(context: secondJoiningContext)

        let preflightExpectation = self.expectation(description: "preflight returns")
        self.cuttlefishContext.preflightJoin(with: otcrk) { error in
            XCTAssertNil(error, "error should be nil")
            preflightExpectation.fulfill()
        }
        self.wait(for: [preflightExpectation], timeout: 20)

        let removeCustodianRecoveryKeyExpectation = self.expectation(description: "removeCustodianRecoveryKey returns")
        self.manager.removeCustodianRecoveryKey(OTControlArguments(configuration: self.otcliqueContext), uuid: otcrk.uuid) { error in
            XCTAssertNil(error, "error should be nil")
            removeCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [removeCustodianRecoveryKeyExpectation], timeout: 20)

        self.sendContainerChangeWaitForFetch(context: joiningContext)
        self.sendContainerChangeWaitForFetch(context: secondJoiningContext)

        let preflight2Expectation = self.expectation(description: "preflight CKR on joiner")
        joiningContext.preflightJoin(with: otcrk) { error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual("com.apple.security.trustedpeers.container", (error! as NSError).domain, "error domain mismatch")
            XCTAssertEqual((error! as NSError).code, ContainerError.untrustedRecoveryKeys.errorCode, "error code mismatch")
            preflight2Expectation.fulfill()
        }

        let preflight3Expectation = self.expectation(description: "preflight CRK on joiner2")
        secondJoiningContext.preflightJoin(with: otcrk) { error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual("com.apple.security.trustedpeers.container", (error! as NSError).domain, "error domain mismatch")
            XCTAssertEqual((error! as NSError).code, ContainerError.untrustedRecoveryKeys.errorCode, "error code mismatch")
            preflight3Expectation.fulfill()
        }
        self.wait(for: [preflight2Expectation, preflight3Expectation], timeout: 10)
    }
}
#endif
