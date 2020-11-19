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
                          modelID: "iPhone9,1",
                          deviceName: "asdf",
                          serialNumber: "1234",
                          osVersion: "asdf",
                          policyVersion: nil,
                          policySecrets: nil,
                          syncUserControllableViews: .UNKNOWN,
                          signingPrivKeyPersistentRef: nil,
                          encPrivKeyPersistentRef: nil) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, _, error in
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

        self.cuttlefishContext.startOctagonStateMachine()

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        // CKKS should be waiting for assistance
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        #if !os(tvOS)
        XCTAssertTrue(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "should have posted an repair CFU")
        #else
        // Apple TV should not post a CFU, as there's no peers to join
        XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "appleTV should not have posted a repair CFU")
        #endif
    }

    func testAttemptedJoinNotAttemptedStateSOSEnabled() throws {
        self.startCKAccountStatusMock()

        self.mockSOSAdapter.sosEnabled = true
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCNotInCircle)

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
                          modelID: "iPhone9,1",
                          deviceName: "asdf",
                          serialNumber: "1234",
                          osVersion: "asdf",
                          policyVersion: nil,
                          policySecrets: nil,
                          syncUserControllableViews: .UNKNOWN,
                          signingPrivKeyPersistentRef: nil,
                          encPrivKeyPersistentRef: nil) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, _, error in
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

        self.cuttlefishContext.startOctagonStateMachine()

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        // CKKS should be waiting for assistance
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        // Since SOS isn't around to help, Octagon should post a CFU
        #if os(tvOS)
        XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "Should not have posted a CFU on aTV (due to having no peers to join)")
        #else
        XCTAssertTrue(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "should have posted an repair CFU, as SOS can't help")
        #endif
    }

    func testAttemptedJoinNotAttemptedStateSOSError() throws {
        self.startCKAccountStatusMock()

        // Note that some errors mean "out of circle", so use NotReady here to avoid that
        self.mockSOSAdapter.sosEnabled = true
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCError)
        self.mockSOSAdapter.circleStatusError = NSError(domain: kSOSErrorDomain as String, code: kSOSErrorNotReady, userInfo: nil)

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
                          modelID: "iPhone9,1",
                          deviceName: "asdf",
                          serialNumber: "1234",
                          osVersion: "asdf",
                          policyVersion: nil,
                          policySecrets: nil,
                          syncUserControllableViews: .UNKNOWN,
                          signingPrivKeyPersistentRef: nil,
                          encPrivKeyPersistentRef: nil) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, _, error in
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

        self.cuttlefishContext.startOctagonStateMachine()

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        // CKKS should be waiting for assistance
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        // Since SOS is in 'error', octagon shouldn't post until SOS can say y/n
        XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "should NOT have posted an repair CFU")
    }

    func testAttemptedJoinNotAttemptedStateSOSDisabled() throws {
        self.startCKAccountStatusMock()
        // Octagon only examines the JoinState if SOS is enabled
        self.mockSOSAdapter.sosEnabled = false

        // No need to mock not joining; Octagon won't have attempted a join if we just start it
        self.cuttlefishContext.startOctagonStateMachine()

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        // CKKS should be waiting for assistance
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        #if !os(tvOS)
        XCTAssertTrue(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "should have posted an repair CFU, as SOS is disabled")
        #else
        // Apple TV should not post a CFU, as there's no peers to join
        XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "appleTV should not have posted a repair CFU")
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
                          modelID: "iPhone9,1",
                          deviceName: "asdf",
                          serialNumber: "1234",
                          osVersion: "asdf",
                          policyVersion: nil,
                          policySecrets: nil,
                          syncUserControllableViews: .UNKNOWN,
                          signingPrivKeyPersistentRef: nil,
                          encPrivKeyPersistentRef: nil) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, _, error in
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

        self.cuttlefishContext.startOctagonStateMachine()

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        // CKKS should be waiting for assistance
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        #if !os(tvOS)
        XCTAssertTrue(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "should have posted an repair CFU")
        #else
        // Apple TV should not post a CFU, as there's no peers to join
        XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "appleTV should not have posted a repair CFU")
        #endif
    }

    #if os(tvOS)
    func testPostCFUWhenApprovalCapablePeerJoins() throws {
        self.startCKAccountStatusMock()
        // Octagon only examines the JoinState if SOS is enabled
        self.mockSOSAdapter.sosEnabled = false

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        // Apple TV should not post a CFU, as there's no peers to join
        XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "appleTV should not have posted a repair CFU")

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
        iphone.rpcResetAndEstablish(.testGenerated) { resetError in
            XCTAssertNil(resetError, "should be no error resetting and establishing")
            resetAndEstablishExpectation.fulfill()
        }
        self.wait(for: [resetAndEstablishExpectation], timeout: 10)

        self.sendContainerChangeWaitForUntrustedFetch(context: self.cuttlefishContext)

        // The TV should now post a CFU, as there's an iphone that can repair it
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        XCTAssertTrue(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "appleTV should have posted a repair CFU")
    }

    func testDontPostCFUWhenApprovalIncapablePeerJoins() throws {
        self.startCKAccountStatusMock()
        // Octagon only examines the JoinState if SOS is enabled
        self.mockSOSAdapter.sosEnabled = false

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        // Apple TV should not post a CFU, as there's no peers to join
        XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "appleTV should not have posted a repair CFU")

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
        mac.rpcResetAndEstablish(.testGenerated) { resetError in
            XCTAssertNil(resetError, "should be no error resetting and establishing")
            resetAndEstablishExpectation.fulfill()
        }
        self.wait(for: [resetAndEstablishExpectation], timeout: 10)

        self.sendContainerChangeWaitForUntrustedFetch(context: self.cuttlefishContext)

        // The TV should not post a CFU, as there's still no iPhone to repair it
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "appleTV should not have posted a repair CFU; no devices present can repair it")
    }

    func testDontPostCFUWhenCapablePeersAreUntrusted() throws {
        self.startCKAccountStatusMock()
        // Octagon only examines the JoinState if SOS is enabled
        self.mockSOSAdapter.sosEnabled = false

        // An iPhone establishes some octagon state, then untrusts itself
        // This is techinically an invalid situation, since Cuttlefish should have rejected the untrust, but it should trigger the condition we're interested in

        let iphone = self.manager.context(forContainerName: OTCKContainerName,
                                          contextID: "firstPhone",
                                          sosAdapter: self.mockSOSAdapter,
                                          authKitAdapter: self.mockAuthKit2,
                                          lockStateTracker: self.lockStateTracker,
                                          accountStateTracker: self.accountStateTracker,
                                          deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-iphone", serialNumber: "456", osVersion: "iOS (fake version)"))
        iphone.startOctagonStateMachine()

        let resetAndEstablishExpectation = self.expectation(description: "resetAndEstablishExpectation returns")
        iphone.rpcResetAndEstablish(.testGenerated) { resetError in
            XCTAssertNil(resetError, "should be no error resetting and establishing")
            resetAndEstablishExpectation.fulfill()
        }
        self.wait(for: [resetAndEstablishExpectation], timeout: 10)

        let iphonePeerID = try iphone.accountMetadataStore.loadOrCreateAccountMetadata().peerID!

        let leaveExpectation = self.expectation(description: "rpcLeaveClique returns")
        iphone.rpcLeaveClique { leaveError in
            XCTAssertNil(leaveError, "Should be no error leaving")
            leaveExpectation.fulfill()
        }
        self.wait(for: [leaveExpectation], timeout: 10)

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: iphonePeerID, opinion: .excludes, target: iphonePeerID)),
                      "iphone should distrust itself")

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // Ensure that the aTV has fetched properly
        self.sendContainerChangeWaitForUntrustedFetch(context: self.cuttlefishContext)

        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        // Apple TV should not post a CFU, as the only iPhone around is untrusted
        XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "appleTV should not have posted a repair CFU")

        // Another iPhone resets the world
        let iphone2 = self.manager.context(forContainerName: OTCKContainerName,
                                           contextID: "firstPhone",
                                           sosAdapter: self.mockSOSAdapter,
                                           authKitAdapter: self.mockAuthKit3,
                                           lockStateTracker: self.lockStateTracker,
                                           accountStateTracker: self.accountStateTracker,
                                           deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-iphone", serialNumber: "456", osVersion: "iOS (fake version)"))
        iphone2.startOctagonStateMachine()

        let resetAndEstablishExpectation2 = self.expectation(description: "resetAndEstablishExpectation returns")
        iphone2.rpcResetAndEstablish(.testGenerated) { resetError in
            XCTAssertNil(resetError, "should be no error resetting and establishing")
            resetAndEstablishExpectation2.fulfill()
        }
        self.wait(for: [resetAndEstablishExpectation2], timeout: 10)

        // The aTV is notified, and now posts a CFU
        self.sendContainerChangeWaitForUntrustedFetch(context: self.cuttlefishContext)
        XCTAssertTrue(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "appleTV should have posted a repair CFU")
    }
    #endif

    func testPostCFUAfterSOSUpgradeFails() throws {
        self.startCKAccountStatusMock()

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCNotInCircle)

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        #if os(tvOS)
        XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "Should not have posted a CFU on aTV (due to having no peers to join)")
        #else
        XCTAssertTrue(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "should have posted an repair CFU, as SOS can't help")
        #endif
    }
}

#endif // OCTAGON
