#if OCTAGON

import Security_Private.OTClique_Private

class OctagonErrorHandlingTests: OctagonTestsBase {
    func testEstablishFailedError() throws {
        self.startCKAccountStatusMock()

        let establishExpectation = self.expectation(description: "establishExpectation")

        self.fakeCuttlefishServer.establishListener = { [unowned self] request in
            self.fakeCuttlefishServer.establishListener = nil

            self.fakeCuttlefishServer.establish(request) { response in
                switch response {
                case .success(let response):
                    XCTAssertNotNil(response, "should get a response from establish")
                case .failure(let error):
                    XCTAssertNil(error, "should be no error from establish")
                }
                // drop the response on the floor
            }

            establishExpectation.fulfill()

            return FakeCuttlefishServer.makeCloudKitCuttlefishError(code: .establishFailed)
        }

        _ = self.assertResetAndBecomeTrustedInDefaultContext()
        self.wait(for: [establishExpectation], timeout: 10)
    }

    func testRecoverFromImmediateTimeoutDuringEstablish() throws {
        self.startCKAccountStatusMock()

        let establishExpectation = self.expectation(description: "establishExpectation")

        self.fakeCuttlefishServer.establishListener = { [unowned self] _ in
            self.fakeCuttlefishServer.establishListener = nil
            establishExpectation.fulfill()

            return NSError(domain: CKErrorDomain,
                           code: CKError.networkFailure.rawValue,
                           userInfo: [:])
        }

        _ = self.assertResetAndBecomeTrustedInDefaultContext()
        self.wait(for: [establishExpectation], timeout: 10)
    }

    func testRecoverFromRetryableErrorDuringEstablish() throws {
        self.startCKAccountStatusMock()

        let establishExpectation = self.expectation(description: "establishExpectation")

        var t0 = Date.distantPast

        self.fakeCuttlefishServer.establishListener = { [unowned self] _ in
            self.fakeCuttlefishServer.establishListener = nil
            establishExpectation.fulfill()

            t0 = Date()
            return FakeCuttlefishServer.makeCloudKitCuttlefishError(code: .retryableServerFailure)
        }

        _ = self.assertResetAndBecomeTrustedInDefaultContext()
        self.wait(for: [establishExpectation], timeout: 10)
        let t1 = Date()
        let d = t0.distance(to: t1)
        XCTAssertGreaterThanOrEqual(d, 4)
        // Let slower devices have a few extra seconds: we expect this after 5s, but sometimes they need a bit.
        XCTAssertLessThanOrEqual(d, 8)
    }

    func testRecoverFromTransactionalErrorDuringJoinWithVoucher() throws {
        self.startCKAccountStatusMock()

        self.assertResetAndBecomeTrustedInDefaultContext()

        var t0 = Date.distantPast

        let joinExpectation = self.expectation(description: "joinExpectation")
        self.fakeCuttlefishServer.joinListener = { [unowned self] _ in
            self.fakeCuttlefishServer.joinListener = nil
            joinExpectation.fulfill()

            t0 = Date()
            return FakeCuttlefishServer.makeCloudKitCuttlefishError(code: .transactionalFailure)
        }

        let joiningContext = self.makeInitiatorContext(contextID: "joiner", authKitAdapter: self.mockAuthKit2)
        _ = self.assertJoinViaEscrowRecovery(joiningContext: joiningContext, sponsor: self.cuttlefishContext)

        self.wait(for: [joinExpectation], timeout: 10)
        let t1 = Date()
        let d = t0.distance(to: t1)
        XCTAssertGreaterThanOrEqual(d, 4)
        // Let slower devices have a few extra seconds: we expect this after 5s, but sometimes they need a bit.
        XCTAssertLessThanOrEqual(d, 8)
    }

    func testReceiveUpdateWhileUntrustedAndLocked() {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.aksLockState = true
        self.lockStateTracker.recheck()

        // Note that we will not send this notification, because we don't fetch until the device unlocks
        let cliqueChangedNotificationExpectation = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: OTCliqueChanged))
        cliqueChangedNotificationExpectation.isInverted = true

        self.sendContainerChange(context: self.cuttlefishContext)

        XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(10 * NSEC_PER_SEC), "state machine should pause")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.wait(for: [cliqueChangedNotificationExpectation], timeout: 1)
    }

    func testReceiveUpdateWhileReadyAndLocked() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

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

        self.aksLockState = true
        self.lockStateTracker.recheck()

        let invertedCliqueChangedNotificationExpectation = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: OTCliqueChanged))
        invertedCliqueChangedNotificationExpectation.isInverted = true

        self.sendContainerChange(context: self.cuttlefishContext)

        XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(10 * NSEC_PER_SEC), "state machine should pause")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.aksLockState = false
        self.lockStateTracker.recheck()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        // and again!
        self.aksLockState = true
        self.lockStateTracker.recheck()
        self.sendContainerChange(context: self.cuttlefishContext)

        try self.waitForPushToArriveAtStateMachine(context: self.cuttlefishContext)
        XCTAssertTrue(self.cuttlefishContext.stateMachine.possiblePendingFlags().contains(OctagonFlagCuttlefishNotification), "Should have recd_push pending flag")

        let waitForUnlockStateCondition: CKKSCondition = self.cuttlefishContext.stateMachine.stateConditions[OctagonStateWaitForUnlock]!
        XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(10 * NSEC_PER_SEC), "state machine should pause")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.wait(for: [invertedCliqueChangedNotificationExpectation], timeout: 1)

        // Check that we haven't been spinning
        let sameWaitForUnlockStateCondition: CKKSCondition = self.cuttlefishContext.stateMachine.stateConditions[OctagonStateWaitForUnlock]!
        XCTAssertEqual(waitForUnlockStateCondition, sameWaitForUnlockStateCondition, "Conditions should be the same (as the state machine should be halted)")

        let pendingFlagCondition = try XCTUnwrap(self.cuttlefishContext.stateMachine.flags.condition(forFlag: OctagonFlagCuttlefishNotification))

        self.aksLockState = false
        self.lockStateTracker.recheck()

        XCTAssertEqual(0, pendingFlagCondition.wait(10 * NSEC_PER_SEC), "State machine should have handled the OctagonFlagCuttlefishNotification notification")

        XCTAssertEqual(self.cuttlefishContext.stateMachine.possiblePendingFlags(), [], "Should have 0 pending flags")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testReceiveUpdateWhileReadyAndAuthkitRetry() {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

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

        self.mockAuthKit.machineIDFetchErrors.add(NSError(domain: CKErrorDomain, code: CKError.networkUnavailable.rawValue, userInfo: [CKErrorRetryAfterKey: 2]))

        self.sendContainerChange(context: self.cuttlefishContext)

        XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(10 * NSEC_PER_SEC), "state machine should pause")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.sendContainerChange(context: self.cuttlefishContext)
        XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(10 * NSEC_PER_SEC), "state machine should pause")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testReceiveUpdateWhileReadyAndLockedAndAuthkitRetry() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

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

        self.aksLockState = true
        self.lockStateTracker.recheck()

        self.mockAuthKit.machineIDFetchErrors.add(NSError(domain: CKErrorDomain, code: CKError.networkUnavailable.rawValue, userInfo: [CKErrorRetryAfterKey: 2]))

        self.sendContainerChange(context: self.cuttlefishContext)

        XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(10 * NSEC_PER_SEC), "state machine should pause")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.sendContainerChange(context: self.cuttlefishContext)
        XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(10 * NSEC_PER_SEC), "state machine should pause")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        try self.waitForPushToArriveAtStateMachine(context: self.cuttlefishContext)
        XCTAssertTrue(self.cuttlefishContext.stateMachine.possiblePendingFlags().contains(OctagonFlagCuttlefishNotification), "Should have recd_push pending flag")
        let pendingFlagCondition = try XCTUnwrap(self.cuttlefishContext.stateMachine.flags.condition(forFlag: OctagonFlagCuttlefishNotification))

        self.aksLockState = false
        self.lockStateTracker.recheck()

        XCTAssertEqual(0, pendingFlagCondition.wait(10 * NSEC_PER_SEC), "State machine should have handled the notification")

        XCTAssertEqual(self.cuttlefishContext.stateMachine.possiblePendingFlags(), [], "Should have 0 pending flags")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testReceiveTransactionErrorDuringUpdate() {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

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

        let pre = self.fakeCuttlefishServer.fetchChangesCalledCount

        let ckError = FakeCuttlefishServer.makeCloudKitCuttlefishError(code: .transactionalFailure)
        self.fakeCuttlefishServer.nextFetchErrors.append(ckError)

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(10 * NSEC_PER_SEC), "state machine should pause")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertEqual(self.cuttlefishContext.stateMachine.possiblePendingFlags(), [], "Should have zero pending flags after retry")

        sleep(5)
        let post = self.fakeCuttlefishServer.fetchChangesCalledCount
        XCTAssertEqual(post, pre + 2, "should have fetched two times, the first response would have been a transaction error")
    }

    func testPreapprovedPushWhileLocked() throws {
        // Peer 1 becomes SOS+Octagon
        self.putFakeKeyHierarchiesInCloudKit()
        self.putSelfTLKSharesInCloudKit()
        self.saveTLKMaterialToKeychain()

        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCInCircle)
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        let peerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        XCTAssertNotNil(peerID, "Should have a peer ID after making new friends")

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        // Peer 2 attempts to join via preapproval
        let peer2SOSMockPeer = self.createSOSPeer(peerID: "peer2ID")
        let peer2contextID = "peer2"
        let peer2mockSOS = CKKSMockSOSPresentAdapter(selfPeer: peer2SOSMockPeer, trustedPeers: self.mockSOSAdapter!.allPeers(), essential: false)
        let peer2 = self.manager.context(forContainerName: OTCKContainerName,
                                         contextID: peer2contextID,
                                         sosAdapter: peer2mockSOS,
                                         accountsAdapter: self.mockAuthKit2,
                                         authKitAdapter: self.mockAuthKit2,
                                         tooManyPeersAdapter: self.mockTooManyPeers,
                                         tapToRadarAdapter: self.mockTapToRadar,
                                         lockStateTracker: self.lockStateTracker,
                                         deviceInformationAdapter: self.makeInitiatorDeviceInfoAdapter())

        peer2.startOctagonStateMachine()

        self.assertEnters(context: peer2, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // Now, Peer1 should preapprove Peer2
        let peer2Preapproval = TPHashBuilder.hash(with: .SHA256, of: peer2SOSMockPeer.publicSigningKey.encodeSubjectPublicKeyInfo())

        self.mockSOSAdapter!.trustedPeers.add(peer2SOSMockPeer)
        self.mockSOSAdapter!.sendTrustedPeerSetChangedUpdate()

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

        self.verifyDatabaseMocks()
        self.wait(for: [updateTrustExpectation], timeout: 100)
        self.fakeCuttlefishServer.updateListener = nil

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        // Now, peer 2 should lock and receive an Octagon push
        self.aksLockState = true
        self.lockStateTracker.recheck()

        // Now, peer2 should receive an Octagon push, try to realize it is preapproved, and get stuck waiting for an unlock
        let flagCondition = peer2.stateMachine.flags.condition(forFlag: OctagonFlagCuttlefishNotification)

        self.sendContainerChange(context: peer2)
        self.assertEnters(context: peer2, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // The pending flag should become a real flag after the lock state changes
        // But it should not fire for a bit
        XCTAssertNotEqual(0, flagCondition.wait(1 * NSEC_PER_SEC), "Cuttlefish Notification flag should not be removed while locked")

        self.aksLockState = false
        self.lockStateTracker.recheck()

        XCTAssertEqual(0, flagCondition.wait(10 * NSEC_PER_SEC), "Cuttlefish Notification flag should be removed")
        XCTAssertEqual(peer2.stateMachine.possiblePendingFlags(), [], "Should have 0 pending flags")

        self.assertEnters(context: peer2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testReceiveMachineListUpdateWhileReadyAndLocked() throws {
        // Peer 1 becomes SOS+Octagon
        self.putFakeKeyHierarchiesInCloudKit()
        self.putSelfTLKSharesInCloudKit()
        self.saveTLKMaterialToKeychain()

        self.startCKAccountStatusMock()
        self.cuttlefishContext.startOctagonStateMachine()

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

        // Peer 2 arrives (with a voucher), but is not on the trusted device list
        let firstPeerID = clique.cliqueMemberIdentifier
        XCTAssertNotNil(firstPeerID, "Clique should have a member identifier")
        let bottle = self.fakeCuttlefishServer.state.bottles[0]
        let entropy = try self.loadSecret(label: firstPeerID!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        let bNewOTCliqueContext = self.createOTConfigurationContextForTests(contextID: "restoreB",
                                                                            otControl: self.otcliqueContext.otControl,
                                                                            altDSID: self.otcliqueContext.altDSID,
                                                                            sbd: OTMockSecureBackup(bottleID: bottle.bottleID, entropy: entropy!))

        let deviceBmockAuthKit = CKKSTestsMockAccountsAuthKitAdapter(altDSID: self.otcliqueContext.altDSID!,
                                                      machineID: "b-machine-id",
                                                      otherDevices: [self.mockAuthKit.currentMachineID])

        let bRestoreContext = self.manager.context(forContainerName: OTCKContainerName,
                                                   contextID: bNewOTCliqueContext.context,
                                                   sosAdapter: OTSOSMissingAdapter(),
                                                   accountsAdapter: deviceBmockAuthKit,
                                                   authKitAdapter: deviceBmockAuthKit,
                                                   tooManyPeersAdapter: self.mockTooManyPeers,
                                                   tapToRadarAdapter: self.mockTapToRadar,
                                                   lockStateTracker: self.lockStateTracker,
                                                   deviceInformationAdapter: self.makeInitiatorDeviceInfoAdapter())

        bRestoreContext.startOctagonStateMachine()

        self.sendContainerChange(context: bRestoreContext)
        let bNewClique: OTClique
        do {
            bNewClique = try OTClique.performEscrowRecovery(withContextData: bNewOTCliqueContext, escrowArguments: [:])
            XCTAssertNotNil(bNewClique, "bNewClique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored recovering: \(error)")
            throw error
        }
        self.assertEnters(context: bRestoreContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        // Device A notices, but doesn't update (because B isn't on the device list)
        self.fakeCuttlefishServer.updateListener = { _ in
            XCTFail("Should not have updated trust")
            return nil
        }

        // Device A locks and gets the device list notification
        self.aksLockState = true
        self.lockStateTracker.recheck()

        self.mockAuthKit.otherDevices.add(deviceBmockAuthKit.currentMachineID)

        self.cuttlefishContext.notificationOfMachineIDListChange()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForUnlock, within: 10 * NSEC_PER_SEC)

        let updateTrustExpectation = self.expectation(description: "updateTrust")
        self.fakeCuttlefishServer.updateListener = { request in
            XCTAssertEqual(firstPeerID, request.peerID, "updateTrust request should be for ego peer ID")
            XCTAssertTrue(request.hasDynamicInfoAndSig, "updateTrust request should have a dynamic info")
            let newDynamicInfo = TPPeerDynamicInfo(data: request.dynamicInfoAndSig.peerDynamicInfo, sig: request.dynamicInfoAndSig.sig)
            XCTAssertNotNil(newDynamicInfo, "should be able to make a dynamic info from protobuf")

            XCTAssertEqual(newDynamicInfo?.includedPeerIDs.count, 2, "Should trust both peers")
            updateTrustExpectation.fulfill()
            return nil
        }

        self.sendContainerChange(context: self.cuttlefishContext)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForUnlock, within: 10 * NSEC_PER_SEC)

        // And on unlock, it should handle the update
        self.assertAllCKKSViewsUpload(tlkShares: 1)

        self.aksLockState = false
        self.lockStateTracker.recheck()

        self.wait(for: [updateTrustExpectation], timeout: 30)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func testCKKSResetRecoverFromCKKSConflict() throws {
        self.putFakeKeyHierarchiesInCloudKit()
        self.putFakeDeviceStatusesInCloudKit()
        // But do NOT add them to the keychain

        // CKKS should get stuck in waitfortlk
        self.startCKAccountStatusMock()
        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        do {
            let clique = OTClique(contextData: self.otcliqueContext)
            try clique.establish()
        } catch {
            XCTFail("Shouldn't have errored establishing Octagon: \(error)")
        }

        // Now, we should be in 'ready', and CKKS should be stuck
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLK, within: 10 * NSEC_PER_SEC)

        // Now, CKKS decides to reset the world, but a conflict occurs on hierarchy upload
        self.silentZoneDeletesAllowed = true
        var tlkUUIDs: [CKRecordZone.ID: String] = [:]

        self.silentFetchesAllowed = false
        self.expectCKFetchAndRun {
            self.putFakeKeyHierarchiesInCloudKit()
            self.putFakeDeviceStatusesInCloudKit()
            self.silentFetchesAllowed = true

            for zoneID in self.ckksZones {
                tlkUUIDs[zoneID as! CKRecordZone.ID] = (self.keys![zoneID] as? ZoneKeys)?.tlk?.uuid
            }
        }

        let resetExepctation = self.expectation(description: "reset callback is called")
        self.cuttlefishContext.ckks!.rpcResetCloudKit(nil) { error in
            XCTAssertNil(error, "should be no error resetting cloudkit")
            resetExepctation.fulfill()
        }

        // Deletions should occur, then the fetches, then get stuck (as we don't have the TLK)
        self.wait(for: [resetExepctation], timeout: 10)
        self.verifyDatabaseMocks()

        // all subCKKSes should get stuck in waitfortlk
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLK, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        XCTAssertEqual(tlkUUIDs.count, self.ckksZones.count, "Should have the right number of conflicted TLKs")
        for (zoneID, tlkUUID) in tlkUUIDs {
            XCTAssertEqual(tlkUUID, (self.keys![zoneID] as? ZoneKeys)?.tlk?.uuid, "TLK should match conflicted version")
        }
    }

    func testHandlePeerMissingOnSetUserControllableViews() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        // Another device updates the world, but we don't get the push
        let reset = self.makeInitiatorContext(contextID: "reset")
        self.assertResetAndBecomeTrusted(context: reset)

        // Now, the original peer sets their view status
        #if os(tvOS)
        throw XCTSkip("TVs don't set user-controllable views")
        #else

        let clique = self.cliqueFor(context: self.cuttlefishContext)

        do {
            try clique.setUserControllableViewsSyncStatus(true)
            XCTFail("Should be an error setting user-visible sync status")
        } catch {
        }

        // Octagon should notice that it's been kicked out.
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        #endif // os(tvOS)
    }

    func testHandlePeerMissingOnTrustUpdate() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        // Another device joins
        let joiner = self.makeInitiatorContext(contextID: "joiner")
        self.assertJoinViaEscrowRecovery(joiningContext: joiner, sponsor: self.cuttlefishContext)

        // Now, directly after the default context fetches, another device resets Octagon.
        // To simulate this, we reset things in the updateTrust listener.
        let reset = self.makeInitiatorContext(contextID: "reset")

        let updateTrustExpectation = self.expectation(description: "updateTrust")
        self.fakeCuttlefishServer.updateListener = { _ in
            self.fakeCuttlefishServer.updateListener = nil
            reset.startOctagonStateMachine()

            do {
                try reset.setCDPEnabled()
                self.assertEnters(context: reset, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

                let arguments = self.createOTConfigurationContextForTests(contextID: reset.contextID,
                                                                          otControl: self.otControl,
                                                                          altDSID: try XCTUnwrap(reset.activeAccount?.altDSID))
                let clique = try OTClique.newFriends(withContextData: arguments, resetReason: .testGenerated)
                XCTAssertNotNil(clique, "Clique should not be nil")
            } catch {
                XCTFail("Shouldn't have errored making new friends: \(error)")
            }
            self.assertEnters(context: reset, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

            updateTrustExpectation.fulfill()
            return FakeCuttlefishServer.makeCloudKitCuttlefishError(code: .updateTrustPeerNotFound)
        }

        // Notify Octagon of the join
        self.sendContainerChange(context: self.cuttlefishContext)

        self.wait(for: [updateTrustExpectation], timeout: 10)

        // Octagon should notice that it's been kicked out, and should no longer have its old peerID
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let accountMetadata = try self.cuttlefishContext.accountMetadataStore.loadOrCreateAccountMetadata()
        XCTAssertNil(accountMetadata.peerID, "Should have no peer ID anymore")
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        // and TPH doesn't think we have an identity, either
        let trustStatusExpectation = self.expectation(description: "trustStatus callback occurs")
        self.tphClient.trustStatus(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { egoStatus, error in
            XCTAssertNil(error, "error should be nil")

            XCTAssertNil(egoStatus.egoPeerID, "should not have a local peer ID")
            XCTAssertTrue(egoStatus.isExcluded, "should be excluded")
            XCTAssert(egoStatus.egoStatus.contains(.excluded), "self should be excluded (because there is no self)")
            trustStatusExpectation.fulfill()
        }
        self.wait(for: [trustStatusExpectation], timeout: 10)

        // But you can rejoin!
        self.assertAllCKKSViewsUpload(tlkShares: 2)
        self.assertJoinViaEscrowRecovery(joiningContext: self.cuttlefishContext, sponsor: reset)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testHandlePeerMissingOnHealthCheckNoJoinAttempt() throws {
        self.mockSOSAdapter!.setSOSEnabled(false)

        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let joiner = self.makeInitiatorContext(contextID: "joiner")
        joiner.startOctagonStateMachine()

        self.assertEnters(context: joiner, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(OTControlArguments(containerName: OTCKContainerName, contextID: "joiner", altDSID: OTMockPersonaAdapter.defaultMockPersonaString()), skipRateLimitingCheck: false, repair: false) { response, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNil(response, "response should be nil")
            healthCheckCallback.fulfill()
        }

        joiner.stateMachine.testPause(afterEntering: OctagonStatePeerMissingFromServer)

        self.wait(for: [healthCheckCallback], timeout: 10)

        let accountMetadata = try joiner.accountMetadataStore.loadOrCreateAccountMetadata()
        XCTAssertEqual(accountMetadata.attemptedJoin, OTAccountMetadataClassC_AttemptedAJoinState.NOTATTEMPTED, "Should not have attempted join")
        self.assertConsidersSelfUntrusted(context: joiner)

        // and TPH doesn't think we have an identity, either
        let trustStatusExpectation = self.expectation(description: "trustStatus callback occurs")
        self.tphClient.trustStatus(with: try XCTUnwrap(joiner.activeAccount)) { egoStatus, error in
            XCTAssertNil(error, "error should be nil")

            XCTAssertNil(egoStatus.egoPeerID, "should not have a local peer ID")
            XCTAssertTrue(egoStatus.isExcluded, "should be excluded")
            XCTAssert(egoStatus.egoStatus.contains(.excluded), "self should be excluded (because there is no self)")
            trustStatusExpectation.fulfill()
        }
        self.wait(for: [trustStatusExpectation], timeout: 10)
    }

    func testHandlePeerMissingOnTrustUpdateNoJoinAttempt() throws {
        self.mockSOSAdapter!.setSOSEnabled(false)
        self.startCKAccountStatusMock()
        try self.cuttlefishContext.setCDPEnabled()
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let metadata = try self.cuttlefishContext.accountMetadataStore.loadOrCreateAccountMetadata()
        XCTAssertNotNil(metadata, "metadata should not be nil")
        XCTAssertEqual(metadata.attemptedJoin, .NOTATTEMPTED, "should not have attempted to join")

        let reset = self.makeInitiatorContext(contextID: "reset")
        reset.startOctagonStateMachine()

        do {
            try reset.setCDPEnabled()
            self.assertEnters(context: reset, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

            let arguments = self.createOTConfigurationContextForTests(contextID: reset.contextID,
                                                                      otControl: self.otControl,
                                                                      altDSID: try XCTUnwrap(reset.activeAccount?.altDSID))

            let clique = try OTClique.newFriends(withContextData: arguments, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
        }
        self.assertEnters(context: reset, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        let updateTrustExpectation = self.expectation(description: "updateTrust")
        self.fakeCuttlefishServer.fetchChangesListener = { _ in
            self.fakeCuttlefishServer.fetchChangesListener = nil
            updateTrustExpectation.fulfill()
            return nil
        }

        self.sendContainerChange(context: self.cuttlefishContext)

        self.cuttlefishContext.stateMachine.testPause(afterEntering: OctagonStatePeerMissingFromServer)

        self.wait(for: [updateTrustExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let accountMetadata = try self.cuttlefishContext.accountMetadataStore.loadOrCreateAccountMetadata()
        XCTAssertNil(accountMetadata.peerID, "Should have no peer ID anymore")
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)
    }
}

#endif
