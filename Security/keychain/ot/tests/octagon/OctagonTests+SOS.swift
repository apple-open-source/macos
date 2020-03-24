#if OCTAGON

class OctagonSOSTests: OctagonTestsBase {

    func testSOSOctagonKeyConsistency() throws {
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
        XCTAssertNotNil(peerID, "Should have a peer ID")

        // CKKS will upload new TLKShares
        self.assertAllCKKSViewsUpload(tlkShares: 2)
        let newSOSPeer = createSOSPeer(peerID: peerID)
        self.mockSOSAdapter.selfPeer = newSOSPeer
        self.mockSOSAdapter.trustedPeers.add(newSOSPeer)

        // Now restart the context
        self.manager.removeContext(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.restartCKKSViews()
        self.cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.assertConsidersSelfTrustedCachedAccountStatus(context: self.cuttlefishContext)

        let restartedPeerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        XCTAssertNotNil(restartedPeerID, "Should have a peer ID after restarting")

        XCTAssertEqual(peerID, restartedPeerID, "Should have the same peer ID after restarting")
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        self.verifyDatabaseMocks()
    }

    func testSOSOctagonKeyConsistencyLocked() throws {
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
        XCTAssertNotNil(peerID, "Should have a peer ID")

        let newSOSPeer = createSOSPeer(peerID: peerID)
        self.mockSOSAdapter.selfPeer = newSOSPeer

        self.mockSOSAdapter.trustedPeers.add(newSOSPeer)

        self.aksLockState = true
        self.lockStateTracker.recheck()

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        // Now restart the context
        self.manager.removeContext(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.restartCKKSViews()
        self.cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForUnlock, within: 10 * NSEC_PER_SEC)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTrust, within: 10 * NSEC_PER_SEC)

        self.assertAllCKKSViewsUpload(tlkShares: 2)
        self.aksLockState = false
        self.lockStateTracker.recheck()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrustedCachedAccountStatus(context: self.cuttlefishContext)

        let restartedPeerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        XCTAssertNotNil(restartedPeerID, "Should have a peer ID after restarting")

        XCTAssertEqual(peerID, restartedPeerID, "Should have the same peer ID after restarting")

        self.verifyDatabaseMocks()
        self.waitForCKModifications()
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testSOSOctagonKeyConsistencySucceedsAfterUpdatingSOS() throws {
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
        XCTAssertNotNil(peerID, "Should have a peer ID")

        let newSOSPeer = createSOSPeer(peerID: peerID)
        self.mockSOSAdapter.selfPeer = newSOSPeer

        self.mockSOSAdapter.trustedPeers.add(newSOSPeer)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        self.assertConsidersSelfTrustedCachedAccountStatus(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        // Now restart the context
        self.manager.removeContext(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.restartCKKSViews()
        self.cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        self.cuttlefishContext.startOctagonStateMachine()

        self.aksLockState = true
        self.lockStateTracker.recheck()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForUnlock, within: 10 * NSEC_PER_SEC)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTrust, within: 10 * NSEC_PER_SEC)

        self.assertAllCKKSViewsUpload(tlkShares: 2)
        self.aksLockState = false
        self.lockStateTracker.recheck()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrustedCachedAccountStatus(context: self.cuttlefishContext)

        let restartedPeerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        XCTAssertNotNil(restartedPeerID, "Should have a peer ID after restarting")

        XCTAssertEqual(peerID, restartedPeerID, "Should have the same peer ID after restarting")

        self.verifyDatabaseMocks()
        self.waitForCKModifications()
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testSOSPerformOctagonKeyConsistencyOnCircleChange() throws {
        self.startCKAccountStatusMock()

        // Octagon establishes its identity before SOS joins
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCNotInCircle)

        self.assertResetAndBecomeTrustedInDefaultContext()

        let peerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        XCTAssertNotNil(peerID, "Should have a peer ID")

        // Now, SOS arrives
        let updateExpectation = self.expectation(description: "Octagon should inform SOS of its keys")
        self.mockSOSAdapter.updateOctagonKeySetListener = { _ in
            // Don't currently check the key set at all here
            updateExpectation.fulfill()
        }

        // CKKS will upload itself new shares (for the newly trusted SOS self peer)
        self.assertAllCKKSViewsUpload(tlkShares: 1)

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)
        // Note: this should probably be sendSelfPeerChangedUpdate, but we don't have great fidelity around which peer
        // actually changed. So, just use this channel for now
        self.mockSOSAdapter.sendTrustedPeerSetChangedUpdate()

        self.wait(for: [updateExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testDisablingSOSFeatureFlag() throws {
        self.startCKAccountStatusMock()
        OctagonSetSOSFeatureEnabled(false)
        let recoverykeyotcliqueContext = OTConfigurationContext()
        recoverykeyotcliqueContext.context = "recoveryContext"
        recoverykeyotcliqueContext.dsid = "1234"
        recoverykeyotcliqueContext.altDSID = self.mockAuthKit.altDSID!
        recoverykeyotcliqueContext.otControl = self.otControl

        var clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: recoverykeyotcliqueContext,
                                             resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }
        do {
            try clique.joinAfterRestore()
        } catch {
            XCTAssertNotNil(error, "error should not be nil")
        }

        do {
            try clique.isLastFriend()
        } catch {
            XCTAssertNotNil(error, "error should not be nil")
        }

        do {
            try clique.safariPasswordSyncingEnabled()
        } catch {
            XCTAssertNotNil(error, "error should not be nil")
        }

        do {
            try clique.waitForInitialSync()
        } catch {
            XCTAssertNotNil(error, "error should not be nil")
        }

        clique.viewSet(Set(), disabledViews: Set())

        do {
            try clique.setUserCredentialsAndDSID("", password: Data())
        } catch {
            XCTAssertNotNil(error, "error should not be nil")
        }

        do {
            try clique.tryUserCredentialsAndDSID("", password: Data())
        } catch {
            XCTAssertNotNil(error, "error should not be nil")
        }

        do {
            try clique.peersHaveViewsEnabled([""])
        } catch {
            XCTAssertNotNil(error, "error should not be nil")
        }

        do {
            try clique.requestToJoinCircle()
        } catch {
            XCTAssertNotNil(error, "error should not be nil")
        }

        clique.accountUserKeyAvailable()

        do {
            _ = try clique.copyViewUnawarePeerInfo()
        } catch {
            XCTAssertNotNil(error, "error should not be nil")
        }
        do {
            _ = try clique.copyPeerPeerInfo()
        } catch {
            XCTAssertNotNil(error, "error should not be nil")
        }
    }

    func testPreapproveSOSPeersWhenInCircle() throws {
        self.startCKAccountStatusMock()

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)
        let peer1Preapproval = TPHashBuilder.hash(with: .SHA256, of: self.mockSOSAdapter.selfPeer.publicSigningKey.encodeSubjectPublicKeyInfo())

        let peer2SOSMockPeer = self.createSOSPeer(peerID: "peer2ID")
        self.mockSOSAdapter.trustedPeers.add(peer2SOSMockPeer)
        let peer2Preapproval = TPHashBuilder.hash(with: .SHA256, of: peer2SOSMockPeer.publicSigningKey.encodeSubjectPublicKeyInfo())

        let peer3SOSMockPeer = self.createSOSPeer(peerID: "peer3ID")
        self.mockSOSAdapter.trustedPeers.add(peer3SOSMockPeer)
        let peer3Preapproval = TPHashBuilder.hash(with: .SHA256, of: peer3SOSMockPeer.publicSigningKey.encodeSubjectPublicKeyInfo())

        let establishTwiceExpectation = self.expectation(description: "establish should be called twice")
        establishTwiceExpectation.expectedFulfillmentCount = 2

        self.fakeCuttlefishServer.establishListener = { request in
            XCTAssertTrue(request.hasPeer, "establish request should have a peer")

            let newDynamicInfo = TPPeerDynamicInfo(data: request.peer.dynamicInfoAndSig.peerDynamicInfo, sig: request.peer.dynamicInfoAndSig.sig)
            XCTAssertNotNil(newDynamicInfo, "should be able to make a dynamicInfo from protobuf")

            XCTAssertTrue(newDynamicInfo?.preapprovals.contains(peer2Preapproval) ?? false, "Fake peer 2 should be preapproved")
            XCTAssertTrue(newDynamicInfo?.preapprovals.contains(peer3Preapproval) ?? false, "Fake peer 3 should be preapproved")

            establishTwiceExpectation.fulfill()
            return nil
        }

        self.assertAllCKKSViewsUpload(tlkShares: 3)

        // Just starting the state machine is sufficient; it should perform an SOS upgrade
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // And a reset does the right thing with preapprovals as well
        do {
            let arguments = OTConfigurationContext()
            arguments.altDSID = try self.cuttlefishContext.authKitAdapter.primaryiCloudAccountAltDSID()
            arguments.context = self.cuttlefishContext.contextID
            arguments.otControl = self.otControl

            let clique = try OTClique.newFriends(withContextData: arguments, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.wait(for: [establishTwiceExpectation], timeout: 1)

        // And do we do the right thing when joining via SOS preapproval?
        let peer2JoinExpectation = self.expectation(description: "join called")
        self.fakeCuttlefishServer.joinListener = { request in
            XCTAssertTrue(request.hasPeer, "establish request should have a peer")

            let newDynamicInfo = TPPeerDynamicInfo(data: request.peer.dynamicInfoAndSig.peerDynamicInfo, sig: request.peer.dynamicInfoAndSig.sig)
            XCTAssertNotNil(newDynamicInfo, "should be able to make a dynamicInfo from protobuf")

            XCTAssertFalse(newDynamicInfo?.preapprovals.contains(peer1Preapproval) ?? false, "Fake peer 1 should NOT be preapproved by peer2 (as it's already in Octagon)")
            XCTAssertTrue(newDynamicInfo?.preapprovals.contains(peer3Preapproval) ?? false, "Fake peer 3 should be preapproved by peer2")

            peer2JoinExpectation.fulfill()

            return nil
        }

        let peer2mockSOS = CKKSMockSOSPresentAdapter(selfPeer: peer2SOSMockPeer, trustedPeers: self.mockSOSAdapter.allPeers(), essential: false)
        let peer2 = self.makeInitiatorContext(contextID: "peer2", authKitAdapter: self.mockAuthKit2, sosAdapter: peer2mockSOS)

        peer2.startOctagonStateMachine()
        self.assertEnters(context: peer2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: peer2)

        self.wait(for: [peer2JoinExpectation], timeout: 1)
    }

    func testDoNotPreapproveSOSPeerWhenOutOfCircle() throws {
        self.startCKAccountStatusMock()

        // SOS returns 'trusted' peers without actually being in-circle
        // We don't want to preapprove those peers

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCNotInCircle)
        let peer1Preapproval = TPHashBuilder.hash(with: .SHA256, of: self.mockSOSAdapter.selfPeer.publicSigningKey.encodeSubjectPublicKeyInfo())

        let peer2SOSMockPeer = self.createSOSPeer(peerID: "peer2ID")
        self.mockSOSAdapter.trustedPeers.add(peer2SOSMockPeer)
        let peer2Preapproval = TPHashBuilder.hash(with: .SHA256, of: peer2SOSMockPeer.publicSigningKey.encodeSubjectPublicKeyInfo())

        let peer3SOSMockPeer = self.createSOSPeer(peerID: "peer3ID")
        self.mockSOSAdapter.trustedPeers.add(peer3SOSMockPeer)
        let peer3Preapproval = TPHashBuilder.hash(with: .SHA256, of: peer3SOSMockPeer.publicSigningKey.encodeSubjectPublicKeyInfo())

        self.fakeCuttlefishServer.establishListener = { request in
            XCTAssertTrue(request.hasPeer, "establish request should have a peer")

            let newDynamicInfo = TPPeerDynamicInfo(data: request.peer.dynamicInfoAndSig.peerDynamicInfo, sig: request.peer.dynamicInfoAndSig.sig)
            XCTAssertNotNil(newDynamicInfo, "should be able to make a dynamicInfo from protobuf")

            XCTAssertFalse(newDynamicInfo?.preapprovals.contains(peer2Preapproval) ?? false, "Fake peer 2 should not be preapproved")
            XCTAssertFalse(newDynamicInfo?.preapprovals.contains(peer3Preapproval) ?? false, "Fake peer 3 should not be preapproved")

            return nil
        }

        self.assertResetAndBecomeTrustedInDefaultContext()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        self.verifyDatabaseMocks()

        // And do we do the right thing when joining via bottle?
        let peer2JoinExpectation = self.expectation(description: "join called")
        self.fakeCuttlefishServer.joinListener = { request in
            XCTAssertTrue(request.hasPeer, "establish request should have a peer")

            let newDynamicInfo = TPPeerDynamicInfo(data: request.peer.dynamicInfoAndSig.peerDynamicInfo, sig: request.peer.dynamicInfoAndSig.sig)
            XCTAssertNotNil(newDynamicInfo, "should be able to make a dynamicInfo from protobuf")

            XCTAssertFalse(newDynamicInfo?.preapprovals.contains(peer1Preapproval) ?? false, "Fake peer 1 should NOT be preapproved by peer2 (as it's not in SOS)")
            XCTAssertFalse(newDynamicInfo?.preapprovals.contains(peer3Preapproval) ?? false, "Fake peer 3 should not be preapproved by peer2 (as it's not in SOS)")

            peer2JoinExpectation.fulfill()

            return nil
        }

        let peer2mockSOS = CKKSMockSOSPresentAdapter(selfPeer: peer2SOSMockPeer, trustedPeers: self.mockSOSAdapter.allPeers(), essential: false)
        peer2mockSOS.circleStatus = SOSCCStatus(kSOSCCNotInCircle)
        let peer2 = self.makeInitiatorContext(contextID: "peer2", authKitAdapter: self.mockAuthKit2, sosAdapter: peer2mockSOS)

        peer2.startOctagonStateMachine()

        _ = self.assertJoinViaEscrowRecovery(joiningContext: peer2, sponsor: self.cuttlefishContext)
        self.assertEnters(context: peer2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: peer2)

        self.wait(for: [peer2JoinExpectation], timeout: 1)
    }

    func testRespondToNewOctagonPeerWhenUpdatingPreapprovedKeys() throws {
        self.startCKAccountStatusMock()

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)
        let peer1Preapproval = TPHashBuilder.hash(with: .SHA256, of: self.mockSOSAdapter.selfPeer.publicSigningKey.encodeSubjectPublicKeyInfo())

        let peer2SOSMockPeer = self.createSOSPeer(peerID: "peer2ID")
        self.mockSOSAdapter.trustedPeers.add(peer2SOSMockPeer)
        let peer2Preapproval = TPHashBuilder.hash(with: .SHA256, of: peer2SOSMockPeer.publicSigningKey.encodeSubjectPublicKeyInfo())

        self.assertAllCKKSViewsUpload(tlkShares: 2)

        // Just starting the state machine is sufficient; it should perform an SOS upgrade
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        let peer1ID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // Another peer arrives, but we miss the Octagon push
        let peer2mockSOS = CKKSMockSOSPresentAdapter(selfPeer: peer2SOSMockPeer, trustedPeers: self.mockSOSAdapter.allPeers(), essential: false)
        let joiningContext = self.makeInitiatorContext(contextID: "joiner", authKitAdapter: self.mockAuthKit2, sosAdapter: peer2mockSOS)
        let peer2ID = self.assertJoinViaEscrowRecovery(joiningContext: joiningContext, sponsor: self.cuttlefishContext)

        // Now, SOS updates its key list: we should update our preapproved keys (and then also trust the newly-joined peer)
        let peer3SOSMockPeer = self.createSOSPeer(peerID: "peer3ID")
        self.mockSOSAdapter.trustedPeers.add(peer3SOSMockPeer)
        let peer3Preapproval = TPHashBuilder.hash(with: .SHA256, of: peer3SOSMockPeer.publicSigningKey.encodeSubjectPublicKeyInfo())

        let updateKeysExpectation = self.expectation(description: "UpdateTrust should fire (once)")
        self.fakeCuttlefishServer.updateListener = { [unowned self] request in
            XCTAssertEqual(request.peerID, peer1ID, "UpdateTrust should be for peer1")

            let newDynamicInfo = request.dynamicInfoAndSig.dynamicInfo()

            XCTAssertFalse(newDynamicInfo.preapprovals.contains(peer1Preapproval), "Fake peer 1 should NOT be preapproved by peer1 (as it's its own keys)")
            XCTAssertTrue(newDynamicInfo.preapprovals.contains(peer2Preapproval), "Fake peer 2 should be preapproved by original peer")
            XCTAssertTrue(newDynamicInfo.preapprovals.contains(peer3Preapproval), "Fake peer 3 should be preapproved by original peer")

            self.fakeCuttlefishServer.updateListener = nil
            updateKeysExpectation.fulfill()

            return nil
        }

        // And we'll send TLKShares to the new SOS peer and the new Octagon peer
        self.assertAllCKKSViewsUpload(tlkShares: 2)

        // to avoid CKKS race conditions (wherein it uploads each TLKShare in its own operation), send the SOS notification only to the Octagon context
        //self.mockSOSAdapter.sendTrustedPeerSetChangedUpdate()
        self.cuttlefishContext.trustedPeerSetChanged(self.mockSOSAdapter)
        self.wait(for: [updateKeysExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer1ID)),
                      "peer 1 should trust peer 1")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer2ID)),
                      "peer 1 should trust peer 2")
    }
}

#endif
