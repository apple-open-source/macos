#if OCTAGON

class OctagonSOSTests: OctagonTestsBase {

    func testSOSOctagonKeyConsistency() throws {
        self.putFakeKeyHierarchy(inCloudKit: self.manateeZoneID)
        self.putSelfTLKShares(inCloudKit: self.manateeZoneID)
        self.saveTLKMaterial(toKeychain: self.manateeZoneID)

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
        self.putFakeKeyHierarchy(inCloudKit: self.manateeZoneID)
        self.putSelfTLKShares(inCloudKit: self.manateeZoneID)
        self.saveTLKMaterial(toKeychain: self.manateeZoneID)

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
        self.putFakeKeyHierarchy(inCloudKit: self.manateeZoneID!)
        self.putSelfTLKShares(inCloudKit: self.manateeZoneID!)
        self.saveTLKMaterial(toKeychain: self.manateeZoneID!)

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
}

#endif
