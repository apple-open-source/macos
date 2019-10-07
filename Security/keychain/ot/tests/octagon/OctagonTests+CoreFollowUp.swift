
#if OCTAGON

class OctagonCoreFollowUpTests: OctagonTestsBase {
    func testAttemptedJoinStateAttempted() throws {
        self.startCKAccountStatusMock()

        // Prepare an identity, then pretend like securityd thought it was in the right account
        let containerName = OTCKContainerName
        let contextName = OTDefaultContext

        var selfPeerID: String?
        let prepareExpectation = self.expectation(description: "prepare callback occurs")
        tphClient.prepare(withContainer: containerName,
                          context: contextName,
                          epoch: 0,
                          machineID: "asdf",
                          bottleSalt: "123456789",
                          bottleID: UUID().uuidString,
                          modelID: "asdf",
                          deviceName: "asdf",
                          serialNumber: "1234",
                          osVersion: "asdf",
                          policyVersion: nil,
                          policySecrets: nil,
                          signingPrivKeyPersistentRef: nil,
                          encPrivKeyPersistentRef: nil) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, error in
                            XCTAssertNil(error, "Should be no error preparing identity")
                            XCTAssertNotNil(peerID, "Should be a peer ID")
                            XCTAssertNotNil(permanentInfo, "Should have a permenent info")
                            XCTAssertNotNil(permanentInfoSig, "Should have a permanent info signature")
                            XCTAssertNotNil(stableInfo, "Should have a stable info")
                            XCTAssertNotNil(stableInfoSig, "Should have a stable info signature")
                            selfPeerID = peerID

                            prepareExpectation.fulfill()
        }
        self.wait(for: [prepareExpectation], timeout: 10)

        let account = OTAccountMetadataClassC()!
        account.peerID = selfPeerID
        account.icloudAccountState = .ACCOUNT_AVAILABLE
        account.trustState = .TRUSTED
        account.attemptedJoin = .ATTEMPTED

        XCTAssertNoThrow(try account.saveToKeychain(forContainer: containerName, contextID: contextName), "Should be no error saving fake account metadata")

        OctagonInitialize()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        // CKKS should be waiting for assistance
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        #if !os(tvOS)
        XCTAssertTrue(self.cuttlefishContext.postedRepairCFU, "should have posted an repair CFU");
        #else
        // Apple TV should not post a CFU, as there's no peers to join
        XCTAssertFalse(self.cuttlefishContext.postedRepairCFU, "appleTV should not have posted a repair CFU");
        #endif
    }

    func testAttemptedJoinNotAttemptedStateSOSEnabled() throws {
        self.startCKAccountStatusMock()

        // Prepare an identity, then pretend like securityd thought it was in the right account
        let containerName = OTCKContainerName
        let contextName = OTDefaultContext

        var selfPeerID: String?
        let prepareExpectation = self.expectation(description: "prepare callback occurs")
        tphClient.prepare(withContainer: containerName,
                          context: contextName,
                          epoch: 0,
                          machineID: "asdf",
                          bottleSalt: "123456789",
                          bottleID: UUID().uuidString,
                          modelID: "asdf",
                          deviceName: "asdf",
                          serialNumber: "1234",
                          osVersion: "asdf",
                          policyVersion: nil,
                          policySecrets: nil,
                          signingPrivKeyPersistentRef: nil,
                          encPrivKeyPersistentRef: nil) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, error in
                            XCTAssertNil(error, "Should be no error preparing identity")
                            XCTAssertNotNil(peerID, "Should be a peer ID")
                            XCTAssertNotNil(permanentInfo, "Should have a permenent info")
                            XCTAssertNotNil(permanentInfoSig, "Should have a permanent info signature")
                            XCTAssertNotNil(stableInfo, "Should have a stable info")
                            XCTAssertNotNil(stableInfoSig, "Should have a stable info signature")
                            selfPeerID = peerID

                            prepareExpectation.fulfill()
        }
        self.wait(for: [prepareExpectation], timeout: 10)

        let account = OTAccountMetadataClassC()!
        account.peerID = selfPeerID
        account.icloudAccountState = .ACCOUNT_AVAILABLE
        account.trustState = .TRUSTED
        account.attemptedJoin = .NOTATTEMPTED

        XCTAssertNoThrow(try account.saveToKeychain(forContainer: containerName, contextID: contextName), "Should be no error saving fake account metadata")

        OctagonInitialize()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        // CKKS should be waiting for assistance
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(self.cuttlefishContext.postedRepairCFU, "should NOT have posted an repair CFU");
    }

    func testAttemptedJoinNotAttemptedStateSOSDisabled() throws {
        self.startCKAccountStatusMock()
        // Octagon only examines the JoinState if SOS is enabled
        self.mockSOSAdapter.sosEnabled = false

        // No need to mock not joining; Octagon won't have attempted a join if we just start it
        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        // CKKS should be waiting for assistance
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        #if !os(tvOS)
        XCTAssertTrue(self.cuttlefishContext.postedRepairCFU, "should have posted an repair CFU, as SOS is disabled");
        #else
        // Apple TV should not post a CFU, as there's no peers to join
        XCTAssertFalse(self.cuttlefishContext.postedRepairCFU, "appleTV should not have posted a repair CFU");
        #endif
    }

    func testAttemptedJoinStateUnknown() throws {
        self.startCKAccountStatusMock()

        // Prepare an identity, then pretend like securityd thought it was in the right account
        let containerName = OTCKContainerName
        let contextName = OTDefaultContext

        var selfPeerID: String?
        let prepareExpectation = self.expectation(description: "prepare callback occurs")
        tphClient.prepare(withContainer: containerName,
                          context: contextName,
                          epoch: 0,
                          machineID: "asdf",
                          bottleSalt: "123456789",
                          bottleID: UUID().uuidString,
                          modelID: "asdf",
                          deviceName: "asdf",
                          serialNumber: "1234",
                          osVersion: "asdf",
                          policyVersion: nil,
                          policySecrets: nil,
                          signingPrivKeyPersistentRef: nil,
                          encPrivKeyPersistentRef: nil) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, error in
                            XCTAssertNil(error, "Should be no error preparing identity")
                            XCTAssertNotNil(peerID, "Should be a peer ID")
                            XCTAssertNotNil(permanentInfo, "Should have a permenent info")
                            XCTAssertNotNil(permanentInfoSig, "Should have a permanent info signature")
                            XCTAssertNotNil(stableInfo, "Should have a stable info")
                            XCTAssertNotNil(stableInfoSig, "Should have a stable info signature")
                            selfPeerID = peerID

                            prepareExpectation.fulfill()
        }
        self.wait(for: [prepareExpectation], timeout: 10)

        let account = OTAccountMetadataClassC()!
        account.peerID = selfPeerID
        account.icloudAccountState = .ACCOUNT_AVAILABLE
        account.trustState = .TRUSTED
        account.attemptedJoin = .UNKNOWN

        XCTAssertNoThrow(try account.saveToKeychain(forContainer: containerName, contextID: contextName), "Should be no error saving fake account metadata")

        OctagonInitialize()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        // CKKS should be waiting for assistance
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        #if !os(tvOS)
        XCTAssertTrue(self.cuttlefishContext.postedRepairCFU, "should have posted an repair CFU");
        #else
        // Apple TV should not post a CFU, as there's no peers to join
        XCTAssertFalse(self.cuttlefishContext.postedRepairCFU, "appleTV should not have posted a repair CFU");
        #endif
    }

    #if os(tvOS)
    func testPostCFUWhenApprovalCapablePeerJoins() throws {
        self.startCKAccountStatusMock()
        // Octagon only examines the JoinState if SOS is enabled
        self.mockSOSAdapter.sosEnabled = false

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        // Apple TV should not post a CFU, as there's no peers to join
        XCTAssertFalse(self.cuttlefishContext.postedRepairCFU, "appleTV should not have posted a repair CFU");

        // Now, an iphone appears!
        let iphone = self.manager.context(forContainerName: OTCKContainerName,
                                          contextID: "asdf",
                                          sosAdapter: self.mockSOSAdapter,
                                          authKitAdapter: self.mockAuthKit2,
                                          lockStateTracker: self.lockStateTracker,
                                          accountStateTracker: self.accountStateTracker,
                                          deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-iphone", serialNumber: "456", osVersion: "iOS (fake version)"))
        iphone.startOctagonStateMachine()

        let resetAndEstablishExpectation = self.expectation(description: "resetAndEstablishExpectation returns")
        iphone.rpcResetAndEstablish() { resetError in
            XCTAssertNil(resetError, "should be no error resetting and establishing")
            resetAndEstablishExpectation.fulfill()
        }
        self.wait(for: [resetAndEstablishExpectation], timeout: 10)

        self.sendContainerChangeWaitForUntrustedFetch(context: self.cuttlefishContext)

        // The TV should now post a CFU, as there's an iphone that can repair it
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        XCTAssertTrue(self.cuttlefishContext.postedRepairCFU, "appleTV should have posted a repair CFU");
    }

    func testDontPostCFUWhenApprovalIncapablePeerJoins() throws {
        self.startCKAccountStatusMock()
        // Octagon only examines the JoinState if SOS is enabled
        self.mockSOSAdapter.sosEnabled = false

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        // Apple TV should not post a CFU, as there's no peers to join
        XCTAssertFalse(self.cuttlefishContext.postedRepairCFU, "appleTV should not have posted a repair CFU");

        // Now, a mac appears! macs cannot fix apple TVs.
        let mac = self.manager.context(forContainerName: OTCKContainerName,
                                          contextID: "asdf",
                                          sosAdapter: self.mockSOSAdapter,
                                          authKitAdapter: self.mockAuthKit2,
                                          lockStateTracker: self.lockStateTracker,
                                          accountStateTracker: self.accountStateTracker,
                                          deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iMac7,1", deviceName: "test-mac", serialNumber: "456", osVersion: "macOS (fake version)"))
        mac.startOctagonStateMachine()

        let resetAndEstablishExpectation = self.expectation(description: "resetAndEstablishExpectation returns")
        mac.rpcResetAndEstablish() { resetError in
            XCTAssertNil(resetError, "should be no error resetting and establishing")
            resetAndEstablishExpectation.fulfill()
        }
        self.wait(for: [resetAndEstablishExpectation], timeout: 10)

        self.sendContainerChangeWaitForUntrustedFetch(context: self.cuttlefishContext)

        // The TV should not post a CFU, as there's still no iPhone to repair it
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        XCTAssertFalse(self.cuttlefishContext.postedRepairCFU, "appleTV should not have posted a repair CFU; no devices present can repair it");
    }
    #endif
}

#endif // OCTAGON
