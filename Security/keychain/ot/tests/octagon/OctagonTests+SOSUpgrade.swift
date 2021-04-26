#if OCTAGON

class OctagonSOSUpgradeTests: OctagonTestsBase {
    func testSOSUpgrade() throws {
        self.putFakeKeyHierarchiesInCloudKit()
        self.putSelfTLKSharesInCloudKit()
        self.saveTLKMaterialToKeychain()

        XCTAssertTrue(OctagonPerformSOSUpgrade(), "SOS upgrade should be on")

        // Also, during the establish, Octagon shouldn't bother uploading the TLKShares that already exist
        // So, it should have exactly the number of TLKShares as TLKs, and they should be shared to the new identity
        let establishExpectation = self.expectation(description: "establish")
        self.fakeCuttlefishServer.establishListener = { request in
            XCTAssertEqual(request.tlkShares.count, request.viewKeys.count, "Should upload one TLK per keyset")
            for tlkShare in request.tlkShares {
                XCTAssertEqual(tlkShare.sender, request.peer.peerID, "TLKShare should be sent from uploading identity")
                XCTAssertEqual(tlkShare.receiver, request.peer.peerID, "TLKShare should be sent to uploading identity")
            }
            establishExpectation.fulfill()
            return nil
        }

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.wait(for: [establishExpectation], timeout: 10)

        self.verifyDatabaseMocks()

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        // Also, CKKS should be configured with the prevailing policy version
        XCTAssertNotNil(self.injectedManager?.policy, "Should have given CKKS a TPPolicy during SOS upgrade")
        XCTAssertEqual(self.injectedManager?.policy?.version, prevailingPolicyVersion, "Policy given to CKKS should be prevailing policy")

        // And we should have followed the SOS Safari view state
        XCTAssertTrue(self.mockSOSAdapter.safariViewEnabled, "SOS adapter should say that the safari view is enabled")

        // And we should have told SOS that CKKS4All is on
        XCTAssertTrue(self.mockSOSAdapter.ckks4AllStatus, "SOS adapter should have been told that CKKS4All is enabled")

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: true)
    }

    // Verify that an SOS upgrade only does one establish (and no update trust).
    func testSOSUpgradeUpdateNoUpdateTrust() throws {
        self.putFakeKeyHierarchiesInCloudKit()
        self.putSelfTLKSharesInCloudKit()
        self.saveTLKMaterialToKeychain()

        XCTAssertTrue(OctagonPerformSOSUpgrade(), "SOS upgrade should be on")

        var establishCount = 0

        self.fakeCuttlefishServer.establishListener = { request in
            establishCount += 1
            return nil
        }

        // Expect no updateTrust calls.
        self.fakeCuttlefishServer.updateListener = { _ in
            XCTFail("This case should not cause any updateTrust calls")
            return nil
        }

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 40 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        XCTAssertEqual(1, establishCount, "expected exactly one establish calls")

        self.verifyDatabaseMocks()

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testSOSUpgradeAuthkitError() throws {
        self.putFakeKeyHierarchiesInCloudKit()
        self.putSelfTLKSharesInCloudKit()
        self.saveTLKMaterialToKeychain()

        XCTAssertTrue(OctagonPerformSOSUpgrade(), "SOS upgrade should be on")

        // Also, during the establish, Octagon shouldn't bother uploading the TLKShares that already exist
        // So, it should have exactly the number of TLKShares as TLKs, and they should be shared to the new identity
        let establishExpectation = self.expectation(description: "establish")
        self.fakeCuttlefishServer.establishListener = { request in
            XCTAssertEqual(request.tlkShares.count, request.viewKeys.count, "Should upload one TLK per keyset")
            for tlkShare in request.tlkShares {
                XCTAssertEqual(tlkShare.sender, request.peer.peerID, "TLKShare should be sent from uploading identity")
                XCTAssertEqual(tlkShare.receiver, request.peer.peerID, "TLKShare should be sent to uploading identity")
            }
            establishExpectation.fulfill()
            return nil
        }

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)
        self.startCKAccountStatusMock()

        self.mockAuthKit.machineIDFetchErrors.append(CKPrettyError(domain: CKErrorDomain,
                                                                   code: CKError.networkUnavailable.rawValue,
                                                                   userInfo: [CKErrorRetryAfterKey: 2]))

        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 15 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.wait(for: [establishExpectation], timeout: 10)

        self.verifyDatabaseMocks()

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testSOSUpgradeWhileLocked() throws {
        // Test that we tries to perform SOS upgrade once we unlock device again
        //

        self.putFakeKeyHierarchiesInCloudKit()
        self.putSelfTLKSharesInCloudKit()
        self.saveTLKMaterialToKeychain()

        XCTAssertTrue(OctagonPerformSOSUpgrade(), "SOS upgrade should be on")

        // Device is locked
        self.aksLockState = true
        self.lockStateTracker.recheck()

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForUnlock, within: 10 * NSEC_PER_SEC)

        //Now unblock device
        self.aksLockState = false
        self.lockStateTracker.recheck()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        self.verifyDatabaseMocks()

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testSOSUpgradeDuringNetworkOutage() throws {
        // Test that we tries to perform SOS upgrade after a bit after a failure
        self.putFakeKeyHierarchiesInCloudKit()
        self.putSelfTLKSharesInCloudKit()
        self.saveTLKMaterialToKeychain()

        XCTAssertTrue(OctagonPerformSOSUpgrade(), "SOS upgrade should be on")

        let establishExpectation = self.expectation(description: "establishExpectation")
        self.fakeCuttlefishServer.establishListener = {  [unowned self] request in
            // Stop erroring next time!
            self.fakeCuttlefishServer.establishListener = nil
            establishExpectation.fulfill()

            return CKPrettyError(domain: CKErrorDomain, code: CKError.networkUnavailable.rawValue, userInfo: [CKErrorRetryAfterKey: 2])
        }

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        self.wait(for: [establishExpectation], timeout: 10)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // Some time later, it should become ready
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        self.verifyDatabaseMocks()

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testSOSUpgradeStopsIfSplitGraph() throws {
        // Test that we tries to perform SOS upgrade after a bit after a failure
        self.putFakeKeyHierarchiesInCloudKit()
        self.putSelfTLKSharesInCloudKit()
        self.saveTLKMaterialToKeychain()

        XCTAssertTrue(OctagonPerformSOSUpgrade(), "SOS upgrade should be on")

        let establishExpectation = self.expectation(description: "establishExpectation")
        self.fakeCuttlefishServer.establishListener = {  [unowned self] request in
            // Stop erroring next time!
            self.fakeCuttlefishServer.establishListener = nil
            establishExpectation.fulfill()

            return FakeCuttlefishServer.makeCloudKitCuttlefishError(code: .resultGraphNotFullyReachable, retryAfter: 2)
        }

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        self.wait(for: [establishExpectation], timeout: 10)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(self.mockSOSAdapter.ckks4AllStatus, "SOS adapter should not have been told that CKKS4All is enabled")

        // It should be paused
        XCTAssertEqual(self.cuttlefishContext.stateMachine.possiblePendingFlags(), [], "Should have zero pending flags after 'not reachable'")
    }

    func testSOSUpgradeStopsIfNoPreapprovals() throws {
        self.startCKAccountStatusMock()

        // Another peer shows up, preapproving only itself
        let peer2SOSMockPeer = self.createSOSPeer(peerID: "peer2ID")
        let peer2contextID = "peer2"
        let peer2mockSOS = CKKSMockSOSPresentAdapter(selfPeer: peer2SOSMockPeer, trustedPeers: [], essential: false)
        let peer2 = self.manager.context(forContainerName: OTCKContainerName,
                                         contextID: peer2contextID,
                                         sosAdapter: peer2mockSOS,
                                         authKitAdapter: self.mockAuthKit2,
                                         lockStateTracker: self.lockStateTracker,
                                         accountStateTracker: self.accountStateTracker,
                                         deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-SOS-iphone", serialNumber: "456", osVersion: "iOS (fake version)"))
        peer2.startOctagonStateMachine()

        self.assertEnters(context: peer2, state: OctagonStateReady, within: 100 * NSEC_PER_SEC)

        // Now we arrive, and attempt to SOS join
        let sosUpgradeStateCondition : CKKSCondition = self.cuttlefishContext.stateMachine.stateConditions[OctagonStateAttemptSOSUpgrade]!
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertEqual(0, sosUpgradeStateCondition.wait(10 * NSEC_PER_SEC), "Should attempt SOS upgrade")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // Importantly, we should never have asked IDMS for the machine list
        XCTAssertEqual(self.mockAuthKit.fetchInvocations, 0, "Shouldn't have asked AuthKit for the machineID during a non-preapproved join attempt")

        // And we should be paused
        XCTAssertEqual(self.cuttlefishContext.stateMachine.possiblePendingFlags(), [], "Should have zero pending flags after 'no peers preapprove'")
    }

    func testSOSUpgradeWithNoTLKs() throws {
        self.putFakeKeyHierarchiesInCloudKit()
        self.putFakeDeviceStatusesInCloudKit()

        self.startCKAccountStatusMock()

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)

        self.cuttlefishContext.startOctagonStateMachine()

        XCTAssertTrue(OctagonPerformSOSUpgrade(), "SOS upgrade should be on")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 1000 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLK, within: 10 * NSEC_PER_SEC)
    }

    func testsSOSUpgradeWithCKKSConflict() throws {
        // Right after CKKS fetches for the first time, insert a new key hierarchy into CloudKit
        self.silentFetchesAllowed = false
        self.expectCKFetchAndRun {
            self.putFakeKeyHierarchiesInCloudKit()
            self.putFakeDeviceStatusesInCloudKit()
            self.silentFetchesAllowed = true
        }

        self.startCKAccountStatusMock()

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)

        self.cuttlefishContext.startOctagonStateMachine()

        XCTAssertTrue(OctagonPerformSOSUpgrade(), "SOS upgrade should be on")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 1000 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLK, within: 10 * NSEC_PER_SEC)
    }

    func testDontSOSUpgradeIfWouldRemovePreapprover() throws {
        // If a remote peer resets Octagon, they might preapprove the local device's SOS identity.
        // But, if the local device responds to the reset and rejoins, it might not have received the
        // SOS circle containing the remote peer.
        //
        // In that case, it should accept its kicked-out fate and not rejoin (and kick out the reset device).

        self.putFakeKeyHierarchiesInCloudKit()
        self.putSelfTLKSharesInCloudKit()
        self.saveTLKMaterialToKeychain()

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.verifyDatabaseMocks()

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        // Now, peer2 comes along, and resets the world

        let peer2SOSMockPeer = self.createSOSPeer(peerID: "peer2ID")
        // but note: this peer is not yet added to the mockSOSAdapter
        let peer2mockSOS = CKKSMockSOSPresentAdapter(selfPeer: peer2SOSMockPeer,
                                                     trustedPeers: self.mockSOSAdapter.allPeers(),
                                                     essential: false)
        let peer2 = self.makeInitiatorContext(contextID: "peer2",
                                              authKitAdapter: self.mockAuthKit2,
                                              sosAdapter: peer2mockSOS)

        self.assertResetAndBecomeTrusted(context: peer2)

        // Peer1 should accept the reset, and not rejoin.
        self.fakeCuttlefishServer.joinListener = { _ in
            XCTFail("Should not have attemped to re-join")
            return nil
        }

        self.sendContainerChangeWaitForFetchForStates(context: self.cuttlefishContext,
                                                      states: [OctagonStateReadyUpdated,
                                                               OctagonStateBecomeUntrusted,
                                                               OctagonStateAttemptSOSUpgrade,
                                                               OctagonStateUntrusted, ])

        XCTAssertEqual(self.cuttlefishContext.stateMachine.paused.wait(5 * NSEC_PER_SEC), 0, "State machine should have paused")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 1 * NSEC_PER_SEC)

        // But when SOS does catch up, we join just fine.
        self.mockSOSAdapter.trustedPeers.add(peer2SOSMockPeer)
        self.assertAllCKKSViewsUpload(tlkShares: 2)

        self.fakeCuttlefishServer.joinListener = { joinRequest in
            let newDynamicInfo = joinRequest.peer.dynamicInfoAndSig.dynamicInfo()
            XCTAssertEqual(newDynamicInfo.includedPeerIDs.count, 2, "Peer should trust two identities")
            return nil
        }

        self.mockSOSAdapter.sendTrustedPeerSetChangedUpdate()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertTrue(self.mockSOSAdapter.ckks4AllStatus, "SOS adapter should have been told that CKKS4All is enabled")

        self.verifyDatabaseMocks()
    }

    func testDontSOSUpgradeIfErrorFetchingPeers() throws {
        self.putFakeKeyHierarchiesInCloudKit()
        self.putSelfTLKSharesInCloudKit()
        self.saveTLKMaterialToKeychain()

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)
        self.mockSOSAdapter.trustedPeersError = NSError(domain: NSOSStatusErrorDomain,
                                                        code: 1)
        self.startCKAccountStatusMock()
        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateAttemptSOSUpgrade, within: 10 * NSEC_PER_SEC)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func testSOSJoin() throws {
        if !OctagonPerformSOSUpgrade() {
            return
        }
        self.startCKAccountStatusMock()

        let peer1EscrowRequestNotification = expectation(forNotification: OTMockEscrowRequestNotification,
                                                         object: nil,
                                                         handler: nil)

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)

        let peer2SOSMockPeer = self.createSOSPeer(peerID: "peer2ID")

        self.mockSOSAdapter.trustedPeers.add(peer2SOSMockPeer)

        // Due to how everything is shaking out, SOS TLKShares will be uploaded in a second transaction after Octagon uploads its TLKShares
        // This isn't great: <rdar://problem/49080104> Octagon: upload SOS TLKShares alongside initial key hierarchy
        self.assertAllCKKSViewsUpload(tlkShares: 2)

        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        let peer1ID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        XCTAssertTrue(self.mockSOSAdapter.safariViewEnabled, "SOS adapter should say that the safari view is enabled")
        let clique = self.cliqueFor(context: self.cuttlefishContext)
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: true)

        // Peer1 should have sent a request for silent escrow update
        self.wait(for: [peer1EscrowRequestNotification], timeout: 5)

        // Peer1 just joined. It should trust only itself.
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer1ID)),
                      "peer 1 should trust peer 1")
        XCTAssertEqual(self.fakeCuttlefishServer.state.bottles.count, 1, "should be 1 bottles")

        ///////////// peer2
        let peer2EscrowRequestNotification = expectation(forNotification: OTMockEscrowRequestNotification,
                                                         object: nil,
                                                         handler: nil)
        let peer2contextID = "peer2"
        let peer2mockSOS = CKKSMockSOSPresentAdapter(selfPeer: peer2SOSMockPeer, trustedPeers: self.mockSOSAdapter.allPeers(), essential: false)
        let peer2 = self.manager.context(forContainerName: OTCKContainerName,
                                         contextID: peer2contextID,
                                         sosAdapter: peer2mockSOS,
                                         authKitAdapter: self.mockAuthKit2,
                                         lockStateTracker: self.lockStateTracker,
                                         accountStateTracker: self.accountStateTracker,
                                         deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-SOS-iphone", serialNumber: "456", osVersion: "iOS (fake version)"))

        peer2.startOctagonStateMachine()
        self.assertEnters(context: peer2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: peer2)

        XCTAssertTrue(peer2mockSOS.safariViewEnabled, "SOS adapter should say that the safari view is enabled")
        self.assertFetchUserControllableViewsSyncStatus(clique: self.cliqueFor(context: peer2), status: true)

        // Peer2 should have sent a request for silent escrow update
        self.wait(for: [peer2EscrowRequestNotification], timeout: 5)

        let peer2ID = try peer2.accountMetadataStore.getEgoPeerID()

        // Right now, peer2 has just upgraded after peer1. It should trust peer1, and both should implictly trust each other
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer2ID, opinion: .trusts, target: peer1ID)),
                      "peer 2 should trust peer 1")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trustsByPreapproval, target: peer2ID)),
                      "peer 1 should trust peer 2 by preapproval")
        XCTAssertFalse(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer2ID)),
                       "peer 1 should not trust peer 2 (as it hasn't responded to peer2's upgradeJoin yet)")

        // Now, tell peer1 about the change
        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        // Peer1 should trust peer2 now, since it upgraded it from implicitly explicitly trusted
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer2ID)),
                      "peer 1 should trust peer 2 after update")
        XCTAssertEqual(self.fakeCuttlefishServer.state.bottles.count, 2, "should be 2 bottles")

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func testSOSJoinWithDisabledSafariView() throws {
        self.startCKAccountStatusMock()

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)
        self.mockSOSAdapter.safariViewEnabled = false

        let peer2SOSMockPeer = self.createSOSPeer(peerID: "peer2ID")
        self.mockSOSAdapter.trustedPeers.add(peer2SOSMockPeer)

        self.assertAllCKKSViewsUpload(tlkShares: 2)
        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        let clique = self.cliqueFor(context: self.cuttlefishContext)

        XCTAssertFalse(self.mockSOSAdapter.safariViewEnabled, "SOS adapter should say that the safari view is disabled")

        #if os(tvOS)
        // TVs won't ever turn this off
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: true)
        #else
        // Watches don't have SOS, but in this test, we fake that they do. They should follow "SOS"'s state, just like phones and macs
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: false)
        #endif

        let peer2mockSOS = CKKSMockSOSPresentAdapter(selfPeer: peer2SOSMockPeer,
                                                     trustedPeers: self.mockSOSAdapter.allPeers(),
                                                     essential: false)
        peer2mockSOS.safariViewEnabled = false

        let peer2 = self.makeInitiatorContext(contextID: "peer2", authKitAdapter: self.mockAuthKit2, sosAdapter: peer2mockSOS)

        peer2.startOctagonStateMachine()
        self.assertEnters(context: peer2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: peer2)

        #if os(tvOS)
        self.assertFetchUserControllableViewsSyncStatus(clique: self.cliqueFor(context: peer2), status: true)
        #else
        self.assertFetchUserControllableViewsSyncStatus(clique: self.cliqueFor(context: peer2), status: false)
        #endif
    }

    func testSOSJoinWithEnabledSafariViewButDisabledByPeer() throws {
        self.startCKAccountStatusMock()

        // This peer joins with disabled user views
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)
        self.mockSOSAdapter.safariViewEnabled = false

        let peer2SOSMockPeer = self.createSOSPeer(peerID: "peer2ID")
        self.mockSOSAdapter.trustedPeers.add(peer2SOSMockPeer)

        self.assertAllCKKSViewsUpload(tlkShares: 2)
        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        XCTAssertFalse(self.mockSOSAdapter.safariViewEnabled, "SOS adapter should say that the safari view is disabled")
        XCTAssertTrue(self.mockSOSAdapter.ckks4AllStatus, "SOS adapter should have been told that CKKS4All is enabled")
        let clique = self.cliqueFor(context: self.cuttlefishContext)

        #if os(tvOS)
        //  TVs won't ever turn this off
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: true)
        #else
        // Watches don't have SOS, but in this test, we fake that they do. They should follow "SOS"'s state, just like phones and macs
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: false)
        #endif

        let peer2mockSOS = CKKSMockSOSPresentAdapter(selfPeer: peer2SOSMockPeer,
                                                     trustedPeers: self.mockSOSAdapter.allPeers(),
                                                     essential: false)
        // peer2 joins via SOS preapproval, but with the safari view enabled. It should enable user view syncing, even though the other peer has it off
        peer2mockSOS.safariViewEnabled = true

        let peer2 = self.makeInitiatorContext(contextID: "peer2", authKitAdapter: self.mockAuthKit2, sosAdapter: peer2mockSOS)

        peer2.startOctagonStateMachine()
        self.assertEnters(context: peer2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: peer2)

        self.assertFetchUserControllableViewsSyncStatus(clique: self.cliqueFor(context: peer2), status: true)
    }

    func testSOSJoinUponNotificationOfPreapproval() throws {
        // Peer 1 becomes SOS+Octagon
        self.putFakeKeyHierarchiesInCloudKit()
        self.putSelfTLKSharesInCloudKit()
        self.saveTLKMaterialToKeychain()

        XCTAssertTrue(OctagonPerformSOSUpgrade(), "SOS upgrade should be on")

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 100 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        let peerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        XCTAssertNotNil(peerID, "Should have a peer ID after making new friends")

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        XCTAssertTrue(self.mockSOSAdapter.ckks4AllStatus, "SOS adapter should have been told that CKKS4All is enabled")

        // Peer 2 attempts to join via preapproval
        let peer2SOSMockPeer = self.createSOSPeer(peerID: "peer2ID")
        let peer2contextID = "peer2"
        let peer2mockSOS = CKKSMockSOSPresentAdapter(selfPeer: peer2SOSMockPeer, trustedPeers: self.mockSOSAdapter.allPeers(), essential: false)
        let peer2 = self.manager.context(forContainerName: OTCKContainerName,
                                         contextID: peer2contextID,
                                         sosAdapter: peer2mockSOS,
                                         authKitAdapter: self.mockAuthKit2,
                                         lockStateTracker: self.lockStateTracker,
                                         accountStateTracker: self.accountStateTracker,
                                         deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-SOS-iphone", serialNumber: "456", osVersion: "iOS (fake version)"))

        peer2.startOctagonStateMachine()

        self.assertEnters(context: peer2, state: OctagonStateUntrusted, within: 100 * NSEC_PER_SEC)

        // Now, Peer1 should preapprove Peer2
        let peer2Preapproval = TPHashBuilder.hash(with: .SHA256, of: peer2SOSMockPeer.publicSigningKey.encodeSubjectPublicKeyInfo())

        // Peer1 should upload TLKs for Peer2
        self.assertAllCKKSViewsUpload(tlkShares: 1)

        let updateTrustExpectation = self.expectation(description: "updateTrust")
        self.fakeCuttlefishServer.updateListener = { request in
            XCTAssertEqual(peerID, request.peerID, "updateTrust request should be for ego peer ID")
            XCTAssertTrue(request.hasDynamicInfoAndSig, "updateTrust request should have a dynamic info")
            let newDynamicInfo = TPPeerDynamicInfo(data: request.dynamicInfoAndSig.peerDynamicInfo, sig: request.dynamicInfoAndSig.sig)
            XCTAssertNotNil(newDynamicInfo, "should be able to make a dynamic info from protobuf")

            XCTAssertEqual(newDynamicInfo!.preapprovals.count, 1, "Should have a single preapproval")
            XCTAssertTrue(newDynamicInfo!.preapprovals.contains(peer2Preapproval), "Octagon peer should preapprove new SOS peer")

            // But, since this is an SOS peer and TLK uploads for those peers are currently handled through CK CRUD operations, this update
            // shouldn't have any TLKShares
            XCTAssertEqual(0, request.tlkShares.count, "Trust update should not have any new TLKShares")

            updateTrustExpectation.fulfill()
            return nil
        }

        self.mockSOSAdapter.trustedPeers.add(peer2SOSMockPeer)
        self.mockSOSAdapter.sendTrustedPeerSetChangedUpdate()

        self.verifyDatabaseMocks()
        self.wait(for: [updateTrustExpectation], timeout: 100)
        self.fakeCuttlefishServer.updateListener = nil

        // Now, peer2 should receive an Octagon push, realize it is now preapproved, and join
        self.sendContainerChange(context: peer2)
        self.assertEnters(context: peer2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testSOSJoinUponNotificationOfPreapprovalRetry() throws {
        // Peer 1 becomes SOS+Octagon
        self.putFakeKeyHierarchiesInCloudKit()
        self.putSelfTLKSharesInCloudKit()
        self.saveTLKMaterialToKeychain()

        XCTAssertTrue(OctagonPerformSOSUpgrade(), "SOS upgrade should be on")

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 100 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        let peerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        XCTAssertNotNil(peerID, "Should have a peer ID after making new friends")

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        // Peer 2 attempts to join via preapproval
        let peer2SOSMockPeer = self.createSOSPeer(peerID: "peer2ID")
        let peer2contextID = "peer2"
        let peer2mockSOS = CKKSMockSOSPresentAdapter(selfPeer: peer2SOSMockPeer, trustedPeers: self.mockSOSAdapter.allPeers(), essential: false)
        let peer2 = self.manager.context(forContainerName: OTCKContainerName,
                                         contextID: peer2contextID,
                                         sosAdapter: peer2mockSOS,
                                         authKitAdapter: self.mockAuthKit2,
                                         lockStateTracker: self.lockStateTracker,
                                         accountStateTracker: self.accountStateTracker,
                                         deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-SOS-iphone", serialNumber: "456", osVersion: "iOS (fake version)"))

        peer2.startOctagonStateMachine()

        self.assertEnters(context: peer2, state: OctagonStateUntrusted, within: 100 * NSEC_PER_SEC)

        // Now, Peer1 should preapprove Peer2
        let peer2Preapproval = TPHashBuilder.hash(with: .SHA256, of: peer2SOSMockPeer.publicSigningKey.encodeSubjectPublicKeyInfo())

        // Peer1 should upload TLKs for Peer2
        self.assertAllCKKSViewsUpload(tlkShares: 1)

        // Error out updateTrust1 -- expect a retry
        let updateTrustExpectation1 = self.expectation(description: "updateTrust1")
        let updateTrustExpectation2 = self.expectation(description: "updateTrust2")

        self.fakeCuttlefishServer.updateListener = { [unowned self] request in
            self.fakeCuttlefishServer.updateListener = { request in
                self.fakeCuttlefishServer.updateListener = nil
                XCTAssertEqual(peerID, request.peerID, "updateTrust request should be for ego peer ID")
                XCTAssertTrue(request.hasDynamicInfoAndSig, "updateTrust request should have a dynamic info")
                let newDynamicInfo = TPPeerDynamicInfo(data: request.dynamicInfoAndSig.peerDynamicInfo, sig: request.dynamicInfoAndSig.sig)
                XCTAssertNotNil(newDynamicInfo, "should be able to make a dynamic info from protobuf")

                XCTAssertEqual(newDynamicInfo!.preapprovals.count, 1, "Should have a single preapproval")
                XCTAssertTrue(newDynamicInfo!.preapprovals.contains(peer2Preapproval), "Octagon peer should preapprove new SOS peer")

                // But, since this is an SOS peer and TLK uploads for those peers are currently handled through CK CRUD operations, this update
                // shouldn't have any TLKShares
                XCTAssertEqual(0, request.tlkShares.count, "Trust update should not have any new TLKShares")

                updateTrustExpectation2.fulfill()
                return nil
            }
            updateTrustExpectation1.fulfill()

            return CKPrettyError(domain: CKErrorDomain, code: CKError.networkUnavailable.rawValue, userInfo: [CKErrorRetryAfterKey: 2])
        }

        self.mockSOSAdapter.trustedPeers.add(peer2SOSMockPeer)
        self.mockSOSAdapter.sendTrustedPeerSetChangedUpdate()

        self.wait(for: [updateTrustExpectation1], timeout: 10)
        self.wait(for: [updateTrustExpectation2], timeout: 10)
        self.verifyDatabaseMocks()

        // Now, peer2 should receive an Octagon push, realize it is now preapproved, and join
        self.sendContainerChange(context: peer2)

        self.assertEnters(context: peer2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testSOSJoinUponNotificationOfPreapprovalRetryFail() throws {
        // Peer 1 becomes SOS+Octagon
        self.putFakeKeyHierarchiesInCloudKit()
        self.putSelfTLKSharesInCloudKit()
        self.saveTLKMaterialToKeychain()

        XCTAssertTrue(OctagonPerformSOSUpgrade(), "SOS upgrade should be on")

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 100 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        let peerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        XCTAssertNotNil(peerID, "Should have a peer ID after making new friends")

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        // Peer 2 attempts to join via preapproval
        let peer2SOSMockPeer = self.createSOSPeer(peerID: "peer2ID")
        let peer2contextID = "peer2"
        let peer2mockSOS = CKKSMockSOSPresentAdapter(selfPeer: peer2SOSMockPeer, trustedPeers: self.mockSOSAdapter.allPeers(), essential: false)
        let peer2 = self.manager.context(forContainerName: OTCKContainerName,
                                         contextID: peer2contextID,
                                         sosAdapter: peer2mockSOS,
                                         authKitAdapter: self.mockAuthKit2,
                                         lockStateTracker: self.lockStateTracker,
                                         accountStateTracker: self.accountStateTracker,
                                         deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-SOS-iphone", serialNumber: "456", osVersion: "iOS (fake version)"))

        peer2.startOctagonStateMachine()

        self.assertEnters(context: peer2, state: OctagonStateUntrusted, within: 100 * NSEC_PER_SEC)

        // Now, Peer1 should preapprove Peer2
        _ = TPHashBuilder.hash(with: .SHA256, of: peer2SOSMockPeer.publicSigningKey.encodeSubjectPublicKeyInfo())

        // Peer1 should upload TLKs for Peer2
        self.assertAllCKKSViewsUpload(tlkShares: 1)

        // Error out updateTrust1 -- retry
        let updateTrustExpectation1 = self.expectation(description: "updateTrust1")
        self.fakeCuttlefishServer.updateListener = { [unowned self] request in
            self.fakeCuttlefishServer.updateListener = nil
            updateTrustExpectation1.fulfill()

            return NSError(domain: TrustedPeersHelperErrorDomain,
                           code: Int(TrustedPeersHelperErrorCode.noPreparedIdentity.rawValue))
        }

        self.mockSOSAdapter.trustedPeers.add(peer2SOSMockPeer)
        self.mockSOSAdapter.sendTrustedPeerSetChangedUpdate()

        self.verifyDatabaseMocks()
        self.wait(for: [updateTrustExpectation1], timeout: 100)
    }

    func testSOSAcceptJoinEvenIfMachineIDListOutOfDate() throws {
        self.startCKAccountStatusMock()

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)

        // Peer 2 is not on Peer 1's machine ID list yet
        self.mockAuthKit.otherDevices.remove(try! self.mockAuthKit2.machineID())

        let peer2SOSMockPeer = self.createSOSPeer(peerID: "peer2ID")
        self.mockSOSAdapter.trustedPeers.add(peer2SOSMockPeer)

        self.assertAllCKKSViewsUpload(tlkShares: 2)

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        let peer1ID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // Peer1 just joined. It should trust only itself.
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer1ID)),
                      "peer 1 should trust peer 1")
        XCTAssertEqual(self.fakeCuttlefishServer.state.bottles.count, 1, "should be 1 bottles")

        ///////////// peer2
        let peer2contextID = "peer2"
        let peer2mockSOS = CKKSMockSOSPresentAdapter(selfPeer: peer2SOSMockPeer, trustedPeers: self.mockSOSAdapter.allPeers(), essential: false)
        let peer2 = self.manager.context(forContainerName: OTCKContainerName,
                                         contextID: peer2contextID,
                                         sosAdapter: peer2mockSOS,
                                         authKitAdapter: self.mockAuthKit2,
                                         lockStateTracker: self.lockStateTracker,
                                         accountStateTracker: self.accountStateTracker,
                                         deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-SOS-iphone", serialNumber: "456", osVersion: "iOS (fake version)"))

        peer2.startOctagonStateMachine()
        self.assertEnters(context: peer2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: peer2)

        let peer2ID = try peer2.accountMetadataStore.getEgoPeerID()

        // Right now, peer2 has just upgraded after peer1. It should trust peer1, and both should implictly trust each other
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer2ID, opinion: .trusts, target: peer1ID)),
                      "peer 2 should trust peer 1")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer2ID, opinion: .trusts, target: peer1ID)),
                      "peer 2 should trust peer 1")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trustsByPreapproval, target: peer2ID)),
                      "peer 1 should trust peer 2 by preapproval")
        XCTAssertFalse(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer2ID)),
                       "peer 1 should not trust peer 2 (as it hasn't responded to peer2's upgradeJoin yet)")

        // Now, tell peer1 about the change. It should send some TLKShares.
        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        // Peer1 should trust peer2 now, even though peer2 is not on the machine ID list yet
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer2ID)),
                      "peer 1 should trust peer 2 after update")
        XCTAssertEqual(self.fakeCuttlefishServer.state.bottles.count, 2, "should be 2 bottles")

        // And then the machine ID list becomes consistent

        let updateTrustExpectation = self.expectation(description: "updateTrust")
        self.fakeCuttlefishServer.updateListener = { request in
            XCTAssertTrue(request.hasDynamicInfoAndSig, "updateTrust request should have a dynamic info")
            let newDynamicInfo = TPPeerDynamicInfo(data: request.dynamicInfoAndSig.peerDynamicInfo,
                                                   sig: request.dynamicInfoAndSig.sig)
            XCTAssertNotNil(newDynamicInfo, "should be able to make a dynamic info from protobuf")

            XCTAssertTrue(newDynamicInfo?.includedPeerIDs.contains(peer2ID) ?? false, "peer1 should still trust peer2")
            updateTrustExpectation.fulfill()
            return nil
        }

        self.mockAuthKit.otherDevices.insert(try! self.mockAuthKit2.machineID())
        self.cuttlefishContext.incompleteNotificationOfMachineIDListChange()
        self.wait(for: [updateTrustExpectation], timeout: 10)

        self.verifyDatabaseMocks()
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testSOSJoinUploadsNonexistentTLKs() throws {
        // Peer 1 becomes SOS+Octagon, but doesn't upload any TLKs
        // note that due to how the tests work right now, a different context won't run any CKKS items

        let originalPeerSOSMockPeer = self.createSOSPeer(peerID: "originalPeerID")
        let originalPeerContextID = "peer2"

        self.startCKAccountStatusMock()

        // peer2 should preapprove the future Octagon peer for peer1
        let orignalPeerMockSOS = CKKSMockSOSPresentAdapter(selfPeer: originalPeerSOSMockPeer, trustedPeers: self.mockSOSAdapter.allPeers(), essential: false)
        let originalPeer = self.manager.context(forContainerName: OTCKContainerName,
                                                contextID: originalPeerContextID,
                                                sosAdapter: orignalPeerMockSOS,
                                                authKitAdapter: self.mockAuthKit2,
                                                lockStateTracker: self.lockStateTracker,
                                                accountStateTracker: self.accountStateTracker,
                                                deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-SOS-iphone", serialNumber: "456", osVersion: "iOS (fake version)"))

        originalPeer.startOctagonStateMachine()
        self.assertEnters(context: originalPeer, state: OctagonStateReady, within: 40 * NSEC_PER_SEC)

        // Now, the circle status changes

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)
        self.mockSOSAdapter.trustedPeers.add(originalPeerSOSMockPeer)
        self.cuttlefishContext.startOctagonStateMachine()

        // Peer1 should upload TLKShares for SOS Peer2 via CK CRUD. We should probably fix that someday?
        self.assertAllCKKSViewsUpload(tlkShares: 2)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 40 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // Also, CKKS should be configured with the prevailing policy version
        XCTAssertNotNil(self.injectedManager?.policy, "Should have given CKKS a TPPolicy during SOS upgrade")
        XCTAssertEqual(self.injectedManager?.policy?.version, prevailingPolicyVersion, "Policy given to CKKS should be prevailing policy")
    }

    func testSOSDoNotAttemptUpgradeWhenPlatformDoesntSupport() throws {
        self.startCKAccountStatusMock()

        self.mockSOSAdapter.sosEnabled = false
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCNotInCircle)

        let everEnteredSOSUpgrade: CKKSCondition = self.cuttlefishContext.stateMachine.stateConditions[OctagonStateAttemptSOSUpgrade]!

        self.cuttlefishContext.startOctagonStateMachine()

        // Cheat and even turn on CDP for the account
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        XCTAssertNotEqual(0, everEnteredSOSUpgrade.wait(10 * NSEC_PER_MSEC), "Octagon should have never entered 'attempt sos upgrade'")
    }

    func testSOSUpgradeStopsWhenOutOfCircle() throws {
        self.startCKAccountStatusMock()

        self.mockSOSAdapter.sosEnabled = true
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCNotInCircle)

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)
    }

    func testSosUpgradeAndReady() throws {
        self.putFakeKeyHierarchiesInCloudKit()
        self.putSelfTLKSharesInCloudKit()
        self.saveTLKMaterialToKeychain()

        self.startCKAccountStatusMock()

        self.mockSOSAdapter.sosEnabled = true
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCNotInCircle)

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)

        let upgradeExpectation = self.expectation(description: "waitForOctagonUpgrade")
        self.manager.wait(forOctagonUpgrade: OTCKContainerName, context: self.otcliqueContext.context) { error in
            XCTAssertNil(error, "operation should not fail")
            upgradeExpectation.fulfill()
        }
        self.wait(for: [upgradeExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 40 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        self.verifyDatabaseMocks()

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        XCTAssertTrue(self.mockSOSAdapter.ckks4AllStatus, "SOS adapter should have been told that CKKS4All is enabled")
    }

    func testNotInSOSCircleAndWaitForUpgrade() throws {
        self.putFakeKeyHierarchiesInCloudKit()
        self.putSelfTLKSharesInCloudKit()
        self.saveTLKMaterialToKeychain()

        self.startCKAccountStatusMock()

        self.mockSOSAdapter.sosEnabled = true
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCNotInCircle)

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        let upgradeExpectation = self.expectation(description: "waitForOctagonUpgrade")
        self.manager.wait(forOctagonUpgrade: OTCKContainerName, context: self.otcliqueContext.context) { error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error! as NSError).domain, "com.apple.security.sos.error", "domain should be com.apple.security.sos.error")
            XCTAssertEqual((error! as NSError).code, 1037, "code should be 1037")
            upgradeExpectation.fulfill()
        }
        self.wait(for: [upgradeExpectation], timeout: 2)
    }

    func testSosUpgradeFromDisabledCDPStatus() throws {
        self.putFakeKeyHierarchiesInCloudKit()
        self.putSelfTLKSharesInCloudKit()
        self.saveTLKMaterialToKeychain()

        self.startCKAccountStatusMock()

        self.mockSOSAdapter.sosEnabled = true
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(self.fetchCDPStatus(context: self.cuttlefishContext), .disabled, "CDP status should be 'disabled'")

        // SOS arrives!
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)

        // Attempting the upgrade succeeds, now that SOS is present
        let upgradeExpectation = self.expectation(description: "waitForOctagonUpgrade")
        self.manager.wait(forOctagonUpgrade: OTCKContainerName, context: self.otcliqueContext.context) { error in
            XCTAssertNil(error, "operation should not fail")
            upgradeExpectation.fulfill()
        }
        self.wait(for: [upgradeExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 40 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        XCTAssertEqual(self.fetchCDPStatus(context: self.cuttlefishContext), .enabled, "CDP status should be 'enabled'")

        self.verifyDatabaseMocks()

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testSosUpgradeAPIWhenCDPStateOff() {
        // this test checks that calling waitForOctagonUpgrade (when SOS is still absent) doesn't unconditionally set the CDP bit.
        self.startCKAccountStatusMock()

        self.mockSOSAdapter.sosEnabled = true
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(self.fetchCDPStatus(context: self.cuttlefishContext), .disabled, "CDP status should be 'disabled'")

        let upgradeExpectation = self.expectation(description: "waitForOctagonUpgrade")
        self.manager.wait(forOctagonUpgrade: OTCKContainerName, context: self.otcliqueContext.context) { error in
            XCTAssertNotNil(error, "operation should have failed - SOS is absent and Octagon cannot upgrade from it")
            XCTAssertEqual((error! as NSError).domain, "com.apple.security.sos.error", "domain should be com.apple.security.sos.error")
            XCTAssertEqual((error! as NSError).code, kSOSErrorNoCircle, "code should be kSOSErrorNoCircle")
            upgradeExpectation.fulfill()
        }
        self.wait(for: [upgradeExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(self.fetchCDPStatus(context: self.cuttlefishContext), .disabled, "CDP status should be 'disabled'")

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)
    }

    func testDoNotAttemptUpgradeOnRestart() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        do {
            let clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.verifyDatabaseMocks()
        self.waitForCKModifications()
        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        let peerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        XCTAssertNotNil(peerID, "Should have a peer ID after making new friends")

        // Now restart the context, with SOS being in-circle
        self.assertAllCKKSViewsUpload(tlkShares: 1)

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)
        self.cuttlefishContext = self.simulateRestart(context: self.cuttlefishContext)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        let restartedPeerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        XCTAssertNotNil(restartedPeerID, "Should have a peer ID after restarting")

        XCTAssertEqual(peerID, restartedPeerID, "Should have the same peer ID after restarting")
    }

    func testSOSJoinAndBottle() throws {
        if !OctagonPerformSOSUpgrade() {
            return
        }
        self.startCKAccountStatusMock()

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)

        let peer2SOSMockPeer = self.createSOSPeer(peerID: "peer2ID")
        self.mockSOSAdapter.trustedPeers.add(peer2SOSMockPeer)

        // Due to how everything is shaking out, SOS TLKShares will be uploaded in a second transaction after Octagon uploads its TLKShares
        // This isn't great: <rdar://problem/49080104> Octagon: upload SOS TLKShares alongside initial key hierarchy
        self.assertAllCKKSViewsUpload(tlkShares: 2)
        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        let peer1ID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.verifyDatabaseMocks()
        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        self.assertSelfTLKSharesInCloudKit(peerID: self.mockSOSAdapter.selfPeer.peerID)
        self.assertTLKSharesInCloudKit(receiverPeerID: peer2SOSMockPeer.peerID, senderPeerID: self.mockSOSAdapter.selfPeer.peerID)

        // Peer1 just joined. It should trust only itself.
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer1ID)),
                      "peer 1 should trust peer 1")
        XCTAssertEqual(self.fakeCuttlefishServer.state.bottles.count, 1, "should be 1 bottles")

        let peer2contextID = "peer2"
        let peer2mockSOS = CKKSMockSOSPresentAdapter(selfPeer: peer2SOSMockPeer, trustedPeers: self.mockSOSAdapter.allPeers(), essential: false)
        let peer2 = self.manager.context(forContainerName: OTCKContainerName,
                                         contextID: peer2contextID,
                                         sosAdapter: peer2mockSOS,
                                         authKitAdapter: self.mockAuthKit2,
                                         lockStateTracker: self.lockStateTracker,
                                         accountStateTracker: self.accountStateTracker,
                                         deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-SOS-iphone", serialNumber: "456", osVersion: "iOS (fake version)"))

        peer2.startOctagonStateMachine()
        self.assertEnters(context: peer2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: peer2)

        let peer2ID = try peer2.accountMetadataStore.getEgoPeerID()

        // Right now, peer2 has just upgraded after peer1. It should trust peer1, and peer1 should implictly trust it
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer2ID, opinion: .trusts, target: peer1ID)),
                      "peer 2 should trust peer 1")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer2ID, opinion: .trusts, target: peer1ID)),
                      "peer 2 should trust peer 1")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trustsByPreapproval, target: peer2ID)),
                      "peer 1 should trust peer 2 by preapproval")
        XCTAssertFalse(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer2ID)),
                       "peer 1 should not trust peer 2 (as it hasn't responded to peer2's upgradeJoin yet)")

        // Now, tell peer1 about the change
        // The first peer will upload TLKs for the new peer
        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // Peer1 should trust peer2 now, since it upgraded it from implicitly explicitly trusted
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer2ID)),
                      "peer 1 should trust peer 2 after update")
        XCTAssertEqual(self.fakeCuttlefishServer.state.bottles.count, 2, "should be 2 bottles")

        var entropy = Data()
        var bottleID: String = ""

        //now try restoring bottles
        let mockNoSOS = CKKSMockSOSPresentAdapter(selfPeer: self.createSOSPeer(peerID: "peer3ID"), trustedPeers: Set(), essential: false)
        mockNoSOS.circleStatus = SOSCCStatus(kSOSCCNotInCircle)
        let newGuyUsingBottle = self.manager.context(forContainerName: OTCKContainerName,
                                                     contextID: "NewGuyUsingBottle",
                                                     sosAdapter: mockNoSOS,
                                                     authKitAdapter: self.mockAuthKit3,
                                                     lockStateTracker: self.lockStateTracker,
                                                     accountStateTracker: self.accountStateTracker,
                                                     deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-SOS-iphone-3", serialNumber: "456", osVersion: "iOS (fake version)"))
        let peer2AltDSID = try peer2.accountMetadataStore.loadOrCreateAccountMetadata().altDSID
        newGuyUsingBottle.startOctagonStateMachine()

        let fetchExpectation = self.expectation(description: "fetch callback occurs")
        peer2.fetchEscrowContents { e, bID, s, error  in
            XCTAssertNotNil(e, "entropy should not be nil")
            XCTAssertNotNil(bID, "bID should not be nil")
            XCTAssertNotNil(s, "s should not be nil")
            XCTAssertNil(error, "error should be nil")
            entropy = e!
            bottleID = bID!
            fetchExpectation.fulfill()
        }
        self.wait(for: [fetchExpectation], timeout: 10)

        XCTAssertNotNil(peer2AltDSID, "should have a dsid for peer2")

        let joinWithBottleExpectation = self.expectation(description: "joinWithBottle callback occurs")
        newGuyUsingBottle.join(withBottle: bottleID, entropy: entropy, bottleSalt: peer2AltDSID!) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithBottleExpectation.fulfill()
        }
        self.wait(for: [joinWithBottleExpectation], timeout: 10)
        self.assertEnters(context: newGuyUsingBottle, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        let newGuyPeerID = try newGuyUsingBottle.accountMetadataStore.getEgoPeerID()
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: newGuyPeerID, opinion: .trusts, target: newGuyPeerID)),
                      "newPeer should trust itself after bottle restore")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: newGuyPeerID, opinion: .trusts, target: peer1ID)),
                      "newPeer should trust peer 1 after bottle restore")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: newGuyPeerID, opinion: .trusts, target: peer2ID)),
                      "newPeer should trust peer 2 after bottle restore")

        let gonnaFailContext = self.manager.context(forContainerName: OTCKContainerName, contextID: "gonnaFailContext")
        gonnaFailContext.startOctagonStateMachine()
        let joinWithBottleFailExpectation = self.expectation(description: "joinWithBottleFail callback occurs")
        gonnaFailContext.join(withBottle: bottleID, entropy: entropy, bottleSalt: "") { error in
            XCTAssertNotNil(error, "error should be nil")
            joinWithBottleFailExpectation.fulfill()
        }
        self.wait(for: [joinWithBottleFailExpectation], timeout: 10)
        self.assertEnters(context: gonnaFailContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.assertConsidersSelfTrusted(context: self.cuttlefishContext, isLocked: false)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testSOSPeerUpdatePreapprovesNewPeer() throws {
        self.putFakeKeyHierarchiesInCloudKit()
        self.putSelfTLKSharesInCloudKit()
        self.saveTLKMaterialToKeychain()

        self.startCKAccountStatusMock()

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)

        XCTAssertTrue(OctagonPerformSOSUpgrade(), "SOS upgrade should be on")
        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        self.verifyDatabaseMocks()
        self.waitForCKModifications()

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)

        let peerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        XCTAssertNotNil(peerID, "Should have a peer ID after making new friends")

        // A new SOS peer arrives!
        // We expect a TLK upload for the new peer, as well as an update of preapproved keys
        let peer2SOSMockPeer = self.createSOSPeer(peerID: "peer2-sos-only-ID")
        let peer2Preapproval = TPHashBuilder.hash(with: .SHA256, of: peer2SOSMockPeer.publicSigningKey.encodeSubjectPublicKeyInfo())

        self.assertAllCKKSViewsUpload(tlkShares: 1)

        let updateTrustExpectation = self.expectation(description: "updateTrust")
        self.fakeCuttlefishServer.updateListener = { request in
            XCTAssertEqual(peerID, request.peerID, "updateTrust request should be for ego peer ID")
            XCTAssertTrue(request.hasDynamicInfoAndSig, "updateTrust request should have a dynamic info")
            let newDynamicInfo = TPPeerDynamicInfo(data: request.dynamicInfoAndSig.peerDynamicInfo, sig: request.dynamicInfoAndSig.sig)
            XCTAssertNotNil(newDynamicInfo, "should be able to make a dynamic info from protobuf")

            XCTAssertEqual(newDynamicInfo!.preapprovals.count, 1, "Should have a single preapproval")
            XCTAssertTrue(newDynamicInfo!.preapprovals.contains(peer2Preapproval), "Octagon peer should preapprove new SOS peer")

            // But, since this is an SOS peer and TLK uploads for those peers are currently handled through CK CRUD operations, this update
            // shouldn't have any TLKShares
            XCTAssertEqual(0, request.tlkShares.count, "Trust update should not have any new TLKShares")

            updateTrustExpectation.fulfill()
            return nil
        }

        self.mockSOSAdapter.trustedPeers.add(peer2SOSMockPeer)
        self.mockSOSAdapter.sendTrustedPeerSetChangedUpdate()

        self.verifyDatabaseMocks()
        self.wait(for: [updateTrustExpectation], timeout: 10)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testSOSPeerUpdateOnRestartAfterMissingNotification() throws {
        self.putFakeKeyHierarchiesInCloudKit()
        self.putSelfTLKSharesInCloudKit()
        self.saveTLKMaterialToKeychain()

        self.startCKAccountStatusMock()

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)

        XCTAssertTrue(OctagonPerformSOSUpgrade(), "SOS upgrade should be on")
        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        self.verifyDatabaseMocks()
        self.waitForCKModifications()

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)

        let peerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        XCTAssertNotNil(peerID, "Should have a peer ID after making new friends")

        // A new SOS peer arrives!
        // We expect a TLK upload for the new peer, as well as an update of preapproved keys
        let peer2SOSMockPeer = self.createSOSPeer(peerID: "peer2-sos-only-ID")
        let peer2Preapproval = TPHashBuilder.hash(with: .SHA256, of: peer2SOSMockPeer.publicSigningKey.encodeSubjectPublicKeyInfo())

        self.assertAllCKKSViewsUpload(tlkShares: 1)

        let updateTrustExpectation = self.expectation(description: "updateTrust")
        self.fakeCuttlefishServer.updateListener = { request in
            XCTAssertEqual(peerID, request.peerID, "updateTrust request should be for ego peer ID")
            XCTAssertTrue(request.hasDynamicInfoAndSig, "updateTrust request should have a dynamic info")
            let newDynamicInfo = TPPeerDynamicInfo(data: request.dynamicInfoAndSig.peerDynamicInfo, sig: request.dynamicInfoAndSig.sig)
            XCTAssertNotNil(newDynamicInfo, "should be able to make a dynamic info from protobuf")

            XCTAssertEqual(newDynamicInfo!.preapprovals.count, 1, "Should have a single preapproval")
            XCTAssertTrue(newDynamicInfo!.preapprovals.contains(peer2Preapproval), "Octagon peer should preapprove new SOS peer")

            // But, since this is an SOS peer and TLK uploads for those peers are currently handled through CK CRUD operations, this update
            // shouldn't have any TLKShares
            XCTAssertEqual(0, request.tlkShares.count, "Trust update should not have any new TLKShares")

            updateTrustExpectation.fulfill()
            return nil
        }

        self.mockSOSAdapter.trustedPeers.add(peer2SOSMockPeer)

        // But, SOS doesn't send this update. Let's test that the upload occurs on the next securityd restart
        self.cuttlefishContext = self.simulateRestart(context: self.cuttlefishContext)

        self.verifyDatabaseMocks()
        self.wait(for: [updateTrustExpectation], timeout: 10)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        // BUT, a second restart shouldn't hit the server

        self.fakeCuttlefishServer.updateListener = { request in
            XCTFail("shouldn't have updateTrusted")
            return nil
        }
        self.cuttlefishContext = self.simulateRestart(context: self.cuttlefishContext)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testResetAndEstablishDoesNotReuploadSOSTLKShares() throws {
        self.putFakeKeyHierarchiesInCloudKit()
        self.putSelfTLKSharesInCloudKit()
        self.saveTLKMaterialToKeychain()

        XCTAssertTrue(OctagonPerformSOSUpgrade(), "SOS upgrade should be on")

        // Also, during the establish, Octagon shouldn't bother uploading the TLKShares that already exist
        // So, it should have exactly the number of TLKShares as TLKs, and they should be shared to the new identity
        let establishExpectation = self.expectation(description: "establish")
        self.fakeCuttlefishServer.establishListener = { request in
            XCTAssertEqual(request.tlkShares.count, request.viewKeys.count, "Should upload one TLK per keyset")
            for tlkShare in request.tlkShares {
                XCTAssertEqual(tlkShare.sender, request.peer.peerID, "TLKShare should be sent from uploading identity")
                XCTAssertEqual(tlkShare.receiver, request.peer.peerID, "TLKShare should be sent to uploading identity")
            }
            establishExpectation.fulfill()
            return nil
        }

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.wait(for: [establishExpectation], timeout: 10)

        self.verifyDatabaseMocks()

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        // Now, set up for calling resetAndEstablish

        let nextEstablishExpectation = self.expectation(description: "establish() #2")
        self.fakeCuttlefishServer.establishListener = { request in
            XCTAssertEqual(request.tlkShares.count, request.viewKeys.count, "Should upload one TLK per keyset")
            for tlkShare in request.tlkShares {
                XCTAssertEqual(tlkShare.sender, request.peer.peerID, "TLKShare should be sent from uploading identity")
                XCTAssertEqual(tlkShare.receiver, request.peer.peerID, "TLKShare should be sent to uploading identity")
            }
            nextEstablishExpectation.fulfill()
            return nil
        }

        let resetAndEstablishExpectation = self.expectation(description: "resetAndEstablish")
        self.cuttlefishContext.rpcResetAndEstablish(.testGenerated) { error in
            XCTAssertNil(error, "Should be no error performing a reset and establish")
            resetAndEstablishExpectation.fulfill()
        }

        self.wait(for: [resetAndEstablishExpectation, nextEstablishExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testResetAndEstablishReusesSOSKeys() throws {
        self.putFakeKeyHierarchiesInCloudKit()
        self.putSelfTLKSharesInCloudKit()
        self.saveTLKMaterialToKeychain()

        XCTAssertTrue(OctagonPerformSOSUpgrade(), "SOS upgrade should be on")

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        self.verifyDatabaseMocks()

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        // Fetch permanent info information
        let dumpExpectation = self.expectation(description: "dump callback occurs")
        var encryptionPubKey = Data()
        var signingPubKey = Data()
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) { dump, error in
            XCTAssertNil(error, "Should be no error dumping data")
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")

            let permanentInfo = egoSelf!["permanentInfo"] as? [String: AnyObject]
            XCTAssertNotNil(permanentInfo, "should have a permanent info")

            let epk = permanentInfo!["encryption_pub_key"] as? Data
            XCTAssertNotNil(epk, "Should have an encryption public key")
            encryptionPubKey = epk!

            let spk = permanentInfo!["signing_pub_key"] as? Data
            XCTAssertNotNil(spk, "Should have an signing public key")
            signingPubKey = spk!

            dumpExpectation.fulfill()
        }
        self.wait(for: [dumpExpectation], timeout: 10)

        let resetAndEstablishExpectation = self.expectation(description: "resetAndEstablish")
        self.cuttlefishContext.rpcResetAndEstablish(.testGenerated) { error in
            XCTAssertNil(error, "Should be no error performing a reset and establish")
            resetAndEstablishExpectation.fulfill()
        }

        self.wait(for: [resetAndEstablishExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        // And check that the pub keys are equivalent
        let dumpResetExpectation = self.expectation(description: "dump callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) { dump, error in
            XCTAssertNil(error, "Should be no error dumping data")
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")

            let permanentInfo = egoSelf!["permanentInfo"] as? [String: AnyObject]
            XCTAssertNotNil(permanentInfo, "should have a permanent info")

            let epk = permanentInfo!["encryption_pub_key"] as? Data
            XCTAssertNotNil(epk, "Should have an encryption public key")
            XCTAssertEqual(encryptionPubKey, epk!, "Encryption public key should be the same across a reset")

            let spk = permanentInfo!["signing_pub_key"] as? Data
            XCTAssertNotNil(spk, "Should have an signing public key")
            XCTAssertEqual(signingPubKey, spk!, "Signing public key should be the same across a reset")

            dumpResetExpectation.fulfill()
        }
        self.wait(for: [dumpResetExpectation], timeout: 10)
    }

    func testSOSUpgradeWithFailingAuthKit() throws {
        self.putFakeKeyHierarchiesInCloudKit()
        self.putSelfTLKSharesInCloudKit()
        self.saveTLKMaterialToKeychain()

        XCTAssertTrue(OctagonPerformSOSUpgrade(), "SOS upgrade should be on")

        self.mockAuthKit.machineIDFetchErrors.append(NSError(domain: AKAppleIDAuthenticationErrorDomain,
                                                             code: AKAppleIDAuthenticationError.authenticationErrorCannotFindServer.rawValue,
                                                             userInfo: nil))

        // Octagon should decide it is quite sad.
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateAttemptSOSUpgrade, within: 10 * NSEC_PER_SEC)

        let updateChangesExpectation = self.expectation(description: "fetchChanges")
        self.fakeCuttlefishServer.fetchChangesListener = { request in
            self.fakeCuttlefishServer.fetchChangesReturnEmptyResponse = true
            updateChangesExpectation.fulfill()
            self.fakeCuttlefishServer.fetchChangesListener = nil
            self.mockAuthKit.machineIDFetchErrors.append(NSError(domain: AKAppleIDAuthenticationErrorDomain,
                                                                 code: AKAppleIDAuthenticationError.authenticationErrorCannotFindServer.rawValue,
                                                                 userInfo: nil))

            return nil
        }
        self.wait(for: [updateChangesExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)
    }

    func testCliqueOctagonUpgrade () throws {
        self.putFakeKeyHierarchiesInCloudKit()
        self.putSelfTLKSharesInCloudKit()
        self.saveTLKMaterialToKeychain()
        self.startCKAccountStatusMock()
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)

        XCTAssertTrue(OctagonPerformSOSUpgrade(), "SOS upgrade should be on")

        OctagonSetPlatformSupportsSOS(true)

        let clique = OTClique(contextData: self.otcliqueContext)
        XCTAssertNotNil(clique, "Clique should not be nil")
        XCTAssertNoThrow(try clique.waitForOctagonUpgrade(), "Upgrading should pass")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
    }

    func testCliqueOctagonUpgradeFail () throws {
        self.putFakeKeyHierarchiesInCloudKit()
        self.putSelfTLKSharesInCloudKit()
        self.saveTLKMaterialToKeychain()
        self.startCKAccountStatusMock()

        XCTAssertTrue(OctagonPerformSOSUpgrade(), "SOS upgrade should be on")

        OctagonSetPlatformSupportsSOS(true)

        let clique = OTClique(contextData: self.otcliqueContext)
        XCTAssertNotNil(clique, "Clique should not be nil")
        XCTAssertThrowsError(try clique.waitForOctagonUpgrade(), "Upgrading should fail")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)
    }

    func testSOSDoNotJoinByPreapprovalMultipleTimes() throws {
        self.startCKAccountStatusMock()

        // First, peer 1 establishes, preapproving both peer2 and peer3. Then, peer2 and peer3 join and harmonize.
        // Peer1 is never told about the follow-on joins.
        // Then, the test can begin.

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)

        let peer2SOSMockPeer = self.createSOSPeer(peerID: "peer2ID")
        let peer3SOSMockPeer = self.createSOSPeer(peerID: "peer3ID")

        self.mockSOSAdapter.trustedPeers.add(peer2SOSMockPeer)
        self.mockSOSAdapter.trustedPeers.add(peer3SOSMockPeer)

        // Due to how everything is shaking out, SOS TLKShares will be uploaded in a second transaction after Octagon uploads its TLKShares
        // This isn't great: <rdar://problem/49080104> Octagon: upload SOS TLKShares alongside initial key hierarchy
        self.assertAllCKKSViewsUpload(tlkShares: 3)

        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        let peer1ID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // peer2
        let peer2mockSOS = CKKSMockSOSPresentAdapter(selfPeer: peer2SOSMockPeer, trustedPeers: self.mockSOSAdapter.allPeers(), essential: false)
        let peer2 = self.makeInitiatorContext(contextID: "peer2", authKitAdapter: self.mockAuthKit2, sosAdapter: peer2mockSOS)

        peer2.startOctagonStateMachine()
        self.assertEnters(context: peer2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: peer2)
        let peer2ID = try peer2.accountMetadataStore.getEgoPeerID()

        // peer3
        let peer3mockSOS = CKKSMockSOSPresentAdapter(selfPeer: peer3SOSMockPeer, trustedPeers: self.mockSOSAdapter.allPeers(), essential: false)
        let peer3 = self.makeInitiatorContext(contextID: "peer3", authKitAdapter: self.mockAuthKit3, sosAdapter: peer3mockSOS)

        peer3.startOctagonStateMachine()
        self.assertEnters(context: peer3, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: peer3)
        let peer3ID = try peer3.accountMetadataStore.getEgoPeerID()

        // Now, tell peer2 about peer3's join
        self.sendContainerChangeWaitForFetch(context: peer2)

        // Peer 1 should preapprove both peers.
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trustsByPreapproval, target: peer2ID)),
                      "peer 1 should trust peer 2 by preapproval")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trustsByPreapproval, target: peer2ID)),
                      "peer 1 should trust peer 3 by preapproval")

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer2ID, opinion: .trusts, target: peer1ID)),
                      "peer 2 should trust peer 1")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer2ID, opinion: .trusts, target: peer3ID)),
                      "peer 2 should trust peer 3")

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer3ID, opinion: .trusts, target: peer1ID)),
                      "peer 3 should trust peer 1")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer3ID, opinion: .trusts, target: peer2ID)),
                      "peer 3 should trust peer 2")

        // Now, the test can begin. Peer2 decides it rules the world.
        let removalExpectation = self.expectation(description: "removal occurs")
        peer2.rpcRemoveFriends(inClique: [peer1ID, peer3ID]) { removeError in
            XCTAssertNil(removeError, "Should be no error removing peer1 and peer3")
            removalExpectation.fulfill()
        }
        self.wait(for: [removalExpectation], timeout: 5)
        self.assertEnters(context: peer2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: peer2)

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer2ID, opinion: .excludes, target: peer1ID)),
                      "peer 2 should distrust peer 1")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer2ID, opinion: .excludes, target: peer3ID)),
                      "peer 2 should distrust peer 3")

        // And we notify peer3 about this, and it should become sad
        self.sendContainerChangeWaitForFetchForStates(context: peer3, states: [OctagonStateReadyUpdated, OctagonStateUntrusted])
        self.assertEnters(context: peer3, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: peer3)

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer3ID, opinion: .excludes, target: peer3ID)),
                      "peer 3 should distrust peer 3")

        // And if peer3 decides to reupgrade, but it shouldn't: there's no potentially-trusted peer that preapproves it
        let upgradeExpectation = self.expectation(description: "sosUpgrade call returns")
        peer3.waitForOctagonUpgrade { error in
            XCTAssertNotNil(error, "should be an error performing an SOS upgrade (the second time)")
            upgradeExpectation.fulfill()
        }
        self.wait(for: [upgradeExpectation], timeout: 5)

        // And peer3 remains untrusted
        self.assertEnters(context: peer3, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: peer3)

        // And "wait for upgrade" does something reasonable too
        let upgradeWaitExpectation = self.expectation(description: "sosWaitForUpgrade call returns")
        peer3.waitForOctagonUpgrade { error in
            XCTAssertNotNil(error, "should be an error waiting for an SOS upgrade (the second time)")
            upgradeWaitExpectation.fulfill()
        }
        self.wait(for: [upgradeWaitExpectation], timeout: 5)
    }

    func testSOSJoinByPreapprovalAfterUnknownState() throws {
        self.startCKAccountStatusMock()

        // First, peer 1 establishes, preapproving both peer2 and peer3. Then, peer2 and peer3 join and harmonize.
        // Peer1 is never told about the follow-on joins.
        // Then, the test can begin.

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)

        let peer2SOSMockPeer = self.createSOSPeer(peerID: "peer2ID")
        let peer3SOSMockPeer = self.createSOSPeer(peerID: "peer3ID")

        self.mockSOSAdapter.trustedPeers.add(peer2SOSMockPeer)
        self.mockSOSAdapter.trustedPeers.add(peer3SOSMockPeer)

        // Due to how everything is shaking out, SOS TLKShares will be uploaded in a second transaction after Octagon uploads its TLKShares
        // This isn't great: <rdar://problem/49080104> Octagon: upload SOS TLKShares alongside initial key hierarchy
        self.assertAllCKKSViewsUpload(tlkShares: 3)

        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        let peer1ID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // peer2
        let peer2mockSOS = CKKSMockSOSPresentAdapter(selfPeer: peer2SOSMockPeer, trustedPeers: self.mockSOSAdapter.allPeers(), essential: false)
        let peer2 = self.makeInitiatorContext(contextID: "peer2", authKitAdapter: self.mockAuthKit2, sosAdapter: peer2mockSOS)

        peer2.startOctagonStateMachine()
        self.assertEnters(context: peer2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: peer2)
        let peer2ID = try peer2.accountMetadataStore.getEgoPeerID()

        // peer3
        let peer3mockSOS = CKKSMockSOSPresentAdapter(selfPeer: peer3SOSMockPeer, trustedPeers: self.mockSOSAdapter.allPeers(), essential: false)
        let peer3 = self.makeInitiatorContext(contextID: "peer3", authKitAdapter: self.mockAuthKit3, sosAdapter: peer3mockSOS)

        peer3.startOctagonStateMachine()
        self.assertEnters(context: peer3, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: peer3)
        let peer3ID = try peer3.accountMetadataStore.getEgoPeerID()

        // Now, tell peer2 about peer3's join
        self.sendContainerChangeWaitForFetch(context: peer2)

        // Peer 1 should preapprove both peers.
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trustsByPreapproval, target: peer2ID)),
                      "peer 1 should trust peer 2 by preapproval")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trustsByPreapproval, target: peer2ID)),
                      "peer 1 should trust peer 3 by preapproval")

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer2ID, opinion: .trusts, target: peer1ID)),
                      "peer 2 should trust peer 1")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer2ID, opinion: .trusts, target: peer3ID)),
                      "peer 2 should trust peer 3")

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer3ID, opinion: .trusts, target: peer1ID)),
                      "peer 3 should trust peer 1")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer3ID, opinion: .trusts, target: peer2ID)),
                      "peer 3 should trust peer 2")

        let container = try! self.tphClient.getContainer(withContainer: self.cuttlefishContext.containerName, context: "peer3")
        container.moc.performAndWait {
            container.model.deletePeer(withID: peer3ID)
        }

        // And we notify peer3 about this, and it should become sad
        self.sendContainerChangeWaitForFetchForStates(context: peer3, states: [OctagonStateReadyUpdated, OctagonStateReady])
        self.assertConsidersSelfTrusted(context: peer3)
    }
}

#endif // OCTAGON
