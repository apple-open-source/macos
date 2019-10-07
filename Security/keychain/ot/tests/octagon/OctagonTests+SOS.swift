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

}

#endif
