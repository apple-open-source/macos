#if OCTAGON

class OctagonErrorHandlingTests: OctagonTestsBase {

    func testRecoverFromImmediateTimeoutDuringEstablish() throws {
        self.startCKAccountStatusMock()

        let establishExpectation = self.expectation(description: "establishExpectation")

        self.fakeCuttlefishServer.establishListener = {  [unowned self] request in
            self.fakeCuttlefishServer.establishListener = nil
            establishExpectation.fulfill()

            return CKPrettyError(domain: CKErrorDomain,
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

        self.fakeCuttlefishServer.establishListener = {  [unowned self] request in
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
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.aksLockState = true
        self.lockStateTracker.recheck()

        self.sendContainerChange(context: self.cuttlefishContext)

        XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(10 * NSEC_PER_SEC), "state machine should pause")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
    }

    func testReceiveUpdateWhileReadyAndLocked() {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
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

        sleep(1)
        XCTAssertTrue(self.cuttlefishContext.stateMachine.possiblePendingFlags().contains("recd_push"), "Should have recd_push pending flag")

        let waitForUnlockStateCondition = self.cuttlefishContext.stateMachine.stateConditions[OctagonStateWaitForUnlock] as! CKKSCondition
        XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(10 * NSEC_PER_SEC), "state machine should pause")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        sleep(1)
        // Check that we haven't been spinning
        let sameWaitForUnlockStateCondition = self.cuttlefishContext.stateMachine.stateConditions[OctagonStateWaitForUnlock] as! CKKSCondition
        XCTAssert(waitForUnlockStateCondition == sameWaitForUnlockStateCondition, "Conditions should be the same (as the state machine should be halted)")

        self.aksLockState = false
        self.lockStateTracker.recheck()

        sleep(1)

        XCTAssertEqual(self.cuttlefishContext.stateMachine.possiblePendingFlags(), [], "Should have 0 pending flags")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testReceiveUpdateWhileReadyAndAuthkitRetry() {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
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

        self.mockAuthKit.machineIDFetchErrors.append(CKPrettyError(domain: CKErrorDomain,
                code: CKError.networkUnavailable.rawValue,
                userInfo: [CKErrorRetryAfterKey: 2]))

        self.sendContainerChange(context: self.cuttlefishContext)

        XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(10 * NSEC_PER_SEC), "state machine should pause")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.sendContainerChange(context: self.cuttlefishContext)
        XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(10 * NSEC_PER_SEC), "state machine should pause")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testReceiveUpdateWhileReadyAndLockedAndAuthkitRetry() {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
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

        self.mockAuthKit.machineIDFetchErrors.append(CKPrettyError(domain: CKErrorDomain,
                code: CKError.networkUnavailable.rawValue,
                userInfo: [CKErrorRetryAfterKey: 2]))

        self.sendContainerChange(context: self.cuttlefishContext)

        XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(10 * NSEC_PER_SEC), "state machine should pause")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.sendContainerChange(context: self.cuttlefishContext)
        XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(10 * NSEC_PER_SEC), "state machine should pause")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        sleep(1)

        XCTAssertTrue(self.cuttlefishContext.stateMachine.possiblePendingFlags().contains("recd_push"), "Should have recd_push pending flag")

        self.aksLockState = false
        self.lockStateTracker.recheck()

        sleep(1)

        XCTAssertEqual(self.cuttlefishContext.stateMachine.possiblePendingFlags(), [], "Should have 0 pending flags")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testReceiveTransactionErrorDuringUpdate() {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
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

        sleep(5)

        XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(10 * NSEC_PER_SEC), "state machine should pause")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertEqual(self.cuttlefishContext.stateMachine.possiblePendingFlags(), [], "Should have zero pending flags after retry")

        let post = self.fakeCuttlefishServer.fetchChangesCalledCount
        XCTAssertEqual(post, pre + 2, "should have fetched two times, the first response would have been a transaction error")
    }

    func testPreapprovedPushWhileLocked() throws {
        // Peer 1 becomes SOS+Octagon
        self.putFakeKeyHierarchy(inCloudKit: self.manateeZoneID)
        self.putSelfTLKShares(inCloudKit: self.manateeZoneID)
        self.saveTLKMaterial(toKeychain: self.manateeZoneID)

        XCTAssertTrue(OctagonPerformSOSUpgrade(), "SOS upgrade should be on")

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        let peerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        XCTAssertNotNil(peerID, "Should have a peer ID after making new friends")

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        // Peer 2 attempts to join via preapprovalh
        let peer2SOSMockPeer = self.createSOSPeer(peerID: "peer2ID")
        let peer2contextID = "peer2"
        let peer2mockSOS = CKKSMockSOSPresentAdapter(selfPeer: peer2SOSMockPeer, trustedPeers: self.mockSOSAdapter.allPeers(), essential: false)
        let peer2 = self.manager.context(forContainerName: OTCKContainerName,
                                         contextID: peer2contextID,
                                         sosAdapter: peer2mockSOS,
                                         authKitAdapter: self.mockAuthKit2,
                                         lockStateTracker: self.lockStateTracker,
                                         accountStateTracker: self.accountStateTracker,
                                         deviceInformationAdapter: self.makeInitiatorDeviceInfoAdapter())

        peer2.startOctagonStateMachine()

        self.assertEnters(context: peer2, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // Now, Peer1 should preapprove Peer2
        let peer2Preapproval = TPHashBuilder.hash(with: .SHA256, of: peer2SOSMockPeer.publicSigningKey.encodeSubjectPublicKeyInfo())

        self.mockSOSAdapter.trustedPeers.add(peer2SOSMockPeer)
        self.mockSOSAdapter.sendTrustedPeerSetChangedUpdate()

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

        // Now, peer 2 should lock and receive an Octagon push
        self.aksLockState = true
        self.lockStateTracker.recheck()

        // Now, peer2 should receive an Octagon push, try to realize it is preapproved, and get stuck
        self.sendContainerChange(context: peer2)
        self.assertEnters(context: peer2, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        sleep(1)

        XCTAssertTrue(peer2.stateMachine.possiblePendingFlags().contains("recd_push"), "Should have recd_push pending flag")

        self.aksLockState = false
        self.lockStateTracker.recheck()

        sleep(1)

        XCTAssertEqual(self.cuttlefishContext.stateMachine.possiblePendingFlags(), [], "Should have 0 pending flags")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.assertEnters(context: peer2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testReceiveMachineListUpdateWhileReadyAndLocked() throws {
        // Peer 1 becomes SOS+Octagon
        self.putFakeKeyHierarchy(inCloudKit: self.manateeZoneID)
        self.putSelfTLKShares(inCloudKit: self.manateeZoneID)
        self.saveTLKMaterial(toKeychain: self.manateeZoneID)

        XCTAssertTrue(OctagonPerformSOSUpgrade(), "SOS upgrade should be on")

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

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        // Peer 2 arrives (with a voucher), but is not on the trusted device list
        let firstPeerID = clique.cliqueMemberIdentifier
        XCTAssertNotNil(firstPeerID, "Clique should have a member identifier")
        let bottle = self.fakeCuttlefishServer.state.bottles[0]
        let entropy = try self.loadSecret(label: firstPeerID!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        let bNewOTCliqueContext = OTConfigurationContext()
        bNewOTCliqueContext.context = "restoreB"
        bNewOTCliqueContext.dsid = self.otcliqueContext.dsid
        bNewOTCliqueContext.altDSID = self.otcliqueContext.altDSID
        bNewOTCliqueContext.otControl = self.otcliqueContext.otControl
        bNewOTCliqueContext.sbd = OTMockSecureBackup(bottleID: bottle.bottleID, entropy: entropy!)

        let deviceBmockAuthKit = OTMockAuthKitAdapter(altDSID: self.otcliqueContext.altDSID,
                                                      machineID: "b-machine-id",
                                                      otherDevices: [self.mockAuthKit.currentMachineID])

        let bRestoreContext = self.manager.context(forContainerName: OTCKContainerName,
                                                   contextID: bNewOTCliqueContext.context!,
                                                   sosAdapter: OTSOSMissingAdapter(),
                                                   authKitAdapter: deviceBmockAuthKit,
                                                   lockStateTracker: self.lockStateTracker,
                                                   accountStateTracker: self.accountStateTracker,
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

        self.mockAuthKit.otherDevices.insert(deviceBmockAuthKit.currentMachineID)

        self.cuttlefishContext.incompleteNotificationOfMachineIDListChange()
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
        self.aksLockState = false
        self.lockStateTracker.recheck()

        self.wait(for: [updateTrustExpectation], timeout: 30)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testCKKSResetRecoverFromCKKSConflict() throws {
        self.putFakeKeyHierarchy(inCloudKit: self.manateeZoneID)
        self.putFakeDeviceStatus(inCloudKit: self.manateeZoneID)
        // But do NOT add them to the keychain

        // CKKS should get stuck in waitfortlk
        self.startCKAccountStatusMock()
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        do {
            let clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
        }

        // Now, we should be in 'ready', and CKKS should be stuck
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLK, within: 10 * NSEC_PER_SEC)

        // Now, CKKS decides to reset the world, but a conflict occurs on hierarchy upload
        self.silentZoneDeletesAllowed = true
        var tlkUUIDs : [CKRecordZone.ID:String] = [:]

        self.silentFetchesAllowed = false
        self.expectCKFetchAndRun(beforeFinished: {
            self.putFakeKeyHierarchy(inCloudKit: self.manateeZoneID)
            self.putFakeDeviceStatus(inCloudKit: self.manateeZoneID)
            self.silentFetchesAllowed = true

            // Use the commented version below when multi-zone support is readded to the tets
            tlkUUIDs[self.manateeZoneID!] = (self.keys![self.manateeZoneID!] as? ZoneKeys)?.tlk?.uuid
            /*
            for zoneID in self.ckksZones {
                tlkUUIDs[zoneID as! CKRecordZone.ID] = (self.keys![zoneID] as? ZoneKeys)?.tlk?.uuid
            }
             */
        })

        let resetExepctation = self.expectation(description: "reset callback is called")
        self.cuttlefishContext.viewManager!.rpcResetCloudKit(nil, reason: "unit-test") {
            error in
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
        for (zoneID,tlkUUID) in tlkUUIDs {
            XCTAssertEqual(tlkUUID, (self.keys![zoneID] as? ZoneKeys)?.tlk?.uuid, "TLK should match conflicted version")
        }
    }
}

#endif
