#if OCTAGON

class OctagonDeviceListTests: OctagonTestsBase {
    func testSignInFailureBecauseUntrustedDevice() throws {
        // Check that we honor IdMS trusted device list that we got and reject device that
        // are not it

        self.startCKAccountStatusMock()

        // Must positively assert some device in list, so that the machine ID list isn't empty
        self.mockAuthKit.otherDevices.add("some-machine-id")
        self.mockAuthKit.excludeDevices.add(try! self.mockAuthKit.machineID(self.mockAuthKit.primaryAltDSID(), flowID: "flowID", deviceSessionID: "deviceSessionID", canSendMetrics: false))

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let expectFail = self.expectation(description: "expect to fail")

        do {
            let clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNil(clique, "Clique should be nil")
        } catch {
            expectFail.fulfill()
        }

        self.wait(for: [expectFail], timeout: 10)

        // Now, we should be in 'untrusted'
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
    }

    func testSignInWithIDMSBypass() throws {
        // Check that we can bypass IdMS trusted device list (needed for demo accounts)

        self.startCKAccountStatusMock()

        self.mockAuthKit.excludeDevices.union(self.mockAuthKit.currentDeviceList())

        let account = CloudKitAccount(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()), persona: nil, hsa2: true, demo: true, accountStatus: .available)
        self.mockAuthKit.add(account)
        XCTAssertTrue(self.mockAuthKit.currentDeviceList().isEmpty, "should have zero devices")

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

        // and all subCKKSes should enter ready...
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)

        // And we haven't helpfully added anything to the MID list
        self.assertMIDList(context: self.cuttlefishContext, allowed: Set(), disallowed: Set(), unknown: Set())
    }

    func testRemovePeerWhenRemovedFromDeviceList() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        XCTAssert(self.mockAuthKit.currentDeviceList().contains(self.mockAuthKit2.currentMachineID), "AuthKit should already have device 2 on the list")

        let peer1ID = self.assertResetAndBecomeTrustedInDefaultContext()

        let joiningContext = self.makeInitiatorContext(contextID: "joiner", authKitAdapter: self.mockAuthKit2)
        let peer2ID = self.assertJoinViaEscrowRecovery(joiningContext: joiningContext, sponsor: self.cuttlefishContext)

        // Now, tell peer1 about the change. It will trust the peer and upload some TLK shares
        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer2ID)),
                      "peer 1 should trust peer 2 after update")

        // Then peer2 drops off the device list. Peer 1 should distrust peer2.
        let updateTrustExpectation = self.expectation(description: "updateTrust")
        self.fakeCuttlefishServer.updateListener = { request in
            XCTAssertTrue(request.hasDynamicInfoAndSig, "updateTrust request should have a dynamic info")
            let newDynamicInfo = TPPeerDynamicInfo(data: request.dynamicInfoAndSig.peerDynamicInfo,
                                                   sig: request.dynamicInfoAndSig.sig)
            XCTAssertNotNil(newDynamicInfo, "should be able to make a dynamic info from protobuf")

            XCTAssertTrue(!(newDynamicInfo?.includedPeerIDs.contains(peer2ID) ?? true), "peer1 should no longer trust peer2")
            updateTrustExpectation.fulfill()
            return nil
        }

        self.mockAuthKit.removeAndSendNotification(try! self.mockAuthKit2.machineID(self.mockAuthKit2.primaryAltDSID(), flowID: "flowID", deviceSessionID: "deviceSessionID", canSendMetrics: false))

        self.wait(for: [updateTrustExpectation], timeout: 10)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .excludes, target: peer2ID)),
                      "peer 1 should distrust peer 2 after update")

        self.fakeCuttlefishServer.updateListener = nil
        // Let peer2 receive the distrust
        self.sendContainerChangeWaitForFetch(context: joiningContext)
        self.assertConsidersSelfUntrusted(context: joiningContext)

        if self.mockDeviceInfo.isHomePod() {
            XCTAssertEqual(self.mockTapToRadar.timesHomePodTTRSent, 1, "Should have posted a HomePod TTR")
        } else {
            XCTAssertEqual(self.mockTapToRadar.timesHomePodTTRSent, 0, "Should not have posted a HomePod TTR")
        }
    }

    func testNumberOfPeersInModel() throws {
        self.startCKAccountStatusMock()

        XCTAssert(self.mockAuthKit.currentDeviceList().contains(self.mockAuthKit2.currentMachineID), "AuthKit should already have device 2 on the list")

        _ = self.assertResetAndBecomeTrustedInDefaultContext()

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        do {
            let egoPeerStatus = try self.cuttlefishContext.egoPeerStatus()

            let numberOfPeersWithMID = egoPeerStatus.peerCountsByMachineID[self.mockAuthKit.currentMachineID] ?? NSNumber(0)
            XCTAssertEqual(numberOfPeersWithMID.intValue, 1, "Should have a one peer for the current MID after an initial join")

            let numberOfPeersWithInvalidMID = egoPeerStatus.peerCountsByMachineID["not-a-real-machine-id"]
            XCTAssertNil(numberOfPeersWithInvalidMID, "Should have a zero peers for an invalid MID")
        } catch {
            XCTFail("Should not have failed fetching the number of peers with a mid: \(error)")
        }
    }

    func testRemovePeerWhenRemovedFromDeviceListViaIncompleteNotification() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        XCTAssert(self.mockAuthKit.currentDeviceList().contains(self.mockAuthKit2.currentMachineID), "AuthKit should already have device 2 on the list")

        let peer1ID = self.assertResetAndBecomeTrustedInDefaultContext()

        let joiningContext = self.makeInitiatorContext(contextID: "joiner", authKitAdapter: self.mockAuthKit2)
        let peer2ID = self.assertJoinViaEscrowRecovery(joiningContext: joiningContext, sponsor: self.cuttlefishContext)

        // Now, tell peer1 about the change. It will trust the peer and upload some TLK shares
        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer2ID)),
                      "peer 1 should trust peer 2 after update")

        // Then peer2 drops off the device list. Peer 1 should distrust peer2.
        let updateTrustExpectation = self.expectation(description: "updateTrust")
        self.fakeCuttlefishServer.updateListener = { request in
            XCTAssertTrue(request.hasDynamicInfoAndSig, "updateTrust request should have a dynamic info")
            let newDynamicInfo = TPPeerDynamicInfo(data: request.dynamicInfoAndSig.peerDynamicInfo,
                                                   sig: request.dynamicInfoAndSig.sig)
            XCTAssertNotNil(newDynamicInfo, "should be able to make a dynamic info from protobuf")

            XCTAssertTrue(!(newDynamicInfo?.includedPeerIDs.contains(peer2ID) ?? true), "peer1 should no longer trust peer2")
            updateTrustExpectation.fulfill()
            return nil
        }

        self.mockAuthKit.excludeDevices.add(try! self.mockAuthKit2.machineID(self.mockAuthKit2.primaryAltDSID(), flowID: "flowID", deviceSessionID: "deviceSessionID", canSendMetrics: false))
        XCTAssertFalse(self.mockAuthKit.currentDeviceList().contains(self.mockAuthKit2.currentMachineID), "AuthKit should not still have device 2 on the list")
        self.mockAuthKit.sendIncompleteNotification()

        self.wait(for: [updateTrustExpectation], timeout: 10)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .excludes, target: peer2ID)),
                      "peer 1 should distrust peer 2 after update")
    }

    func testTrustPeerWhenMissingFromDeviceList() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        self.mockAuthKit.otherDevices.removeAllObjects()
        XCTAssertEqual(self.mockAuthKit.currentDeviceList(), Set([self.mockAuthKit.currentMachineID]), "AuthKit should have exactly one device on the list")
        XCTAssertFalse(self.mockAuthKit.currentDeviceList().contains(self.mockAuthKit2.currentMachineID), "AuthKit should not already have device 2 on the list")

        let peer1ID = self.assertResetAndBecomeTrustedInDefaultContext()

        let joiningContext = self.makeInitiatorContext(contextID: "joiner", authKitAdapter: self.mockAuthKit2)
        let peer2ID = self.assertJoinViaEscrowRecovery(joiningContext: joiningContext, sponsor: self.cuttlefishContext)

        // Now, tell peer1 about the change. It will trust the peer, despite it missing from the list, and upload some TLK shares
        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer2ID)),
                      "peer 1 should trust peer 2 after update")
        self.assertMIDList(context: self.cuttlefishContext,
                           allowed: self.mockAuthKit.currentDeviceList(),
                           disallowed: Set(),
                           unknown: Set([self.mockAuthKit2.currentMachineID]))

        // On a follow-up update, peer1 should _not_ hit IDMS, even though there's an unknown peer ID in its DB
        let currentCount = self.mockAuthKit.fetchInvocations
        self.sendContainerChangeWaitForFetchForStates(context: self.cuttlefishContext, states: [OctagonStateReadyUpdated, OctagonStateReady])
        self.assertMIDList(context: self.cuttlefishContext,
                           allowed: self.mockAuthKit.currentDeviceList(),
                           disallowed: Set(),
                           unknown: Set([self.mockAuthKit2.currentMachineID]))
        XCTAssertEqual(currentCount, self.mockAuthKit.fetchInvocations, "Receving a push while having an unknown peer MID should not cause an AuthKit fetch")

        ////////

        // Then peer2 arrives on the device list. Peer 1 should update its dynamic info to no longer have a disposition for peer2.
        let updateTrustExpectation = self.expectation(description: "updateTrust")
        self.fakeCuttlefishServer.updateListener = { request in
            XCTAssertTrue(request.hasDynamicInfoAndSig, "updateTrust request should have a dynamic info")
            let newDynamicInfo = TPPeerDynamicInfo(data: request.dynamicInfoAndSig.peerDynamicInfo,
                                                   sig: request.dynamicInfoAndSig.sig)
            XCTAssertNotNil(newDynamicInfo, "should be able to make a dynamic info from protobuf")

            // TODO: swift refuses to see the dispositions object on newDynamicInfo; ah well
            updateTrustExpectation.fulfill()
            return nil
        }

        self.mockAuthKit.addAndSendNotification(try! self.mockAuthKit2.machineID(self.mockAuthKit2.primaryAltDSID(), flowID: "flowID", deviceSessionID: "deviceSessionID", canSendMetrics: false))

        self.wait(for: [updateTrustExpectation], timeout: 10)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer2ID)),
                      "peer 1 should trust peer 2 after update")
    }

    func testRemoveSelfWhenRemovedFromOnlySelfList() throws {
        self.startCKAccountStatusMock()

        self.mockAuthKit.otherDevices.removeAllObjects()
        XCTAssertEqual(self.mockAuthKit.currentDeviceList(), Set([self.mockAuthKit.currentMachineID]), "AuthKit should have exactly one device on the list")

        let peer1ID = self.assertResetAndBecomeTrustedInDefaultContext()
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer1ID)),
                      "peer 1 should trust peer 1")

        // Then peer1 drops off the device list
        // It should _not_ remove trust in itself, as it's the only peer around
        self.fakeCuttlefishServer.updateListener = { _ in
            XCTFail("Should not have updatedTrust")
            return nil
        }

        self.mockAuthKit.removeAndSendNotification(try! self.mockAuthKit.machineID(self.mockAuthKit.primaryAltDSID(), flowID: "flowID", deviceSessionID: "deviceSessionID", canSendMetrics: false))

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testRemoveSelfWhenRemovedFromLargeDeviceList() throws {
        try self.skipOnRecoveryKeyNotSupported()

        self.startCKAccountStatusMock()

        XCTAssert(self.mockAuthKit.currentDeviceList().count > 1, "AuthKit should have more than one device on the list")

        let peer1ID = self.assertResetAndBecomeTrustedInDefaultContext()
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer1ID)),
                      "peer 1 should trust peer 1")

        let joiningContext = self.makeInitiatorContext(contextID: "joiner", authKitAdapter: self.mockAuthKit2)
        self.assertJoinViaEscrowRecoveryFromDefaultContextWithReciprocationAndTLKShares(joiningContext: joiningContext)

        // Then peer1 drops off the device list
        // It should remove trust in itself, as joiner is still around
        let updateTrustExpectation = self.expectation(description: "updateTrust")
        self.fakeCuttlefishServer.updateListener = { request in
            XCTAssertTrue(request.hasDynamicInfoAndSig, "updateTrust request should have a dynamic info")
            let newDynamicInfo = TPPeerDynamicInfo(data: request.dynamicInfoAndSig.peerDynamicInfo,
                                                   sig: request.dynamicInfoAndSig.sig)
            XCTAssertNotNil(newDynamicInfo, "should be able to make a dynamic info from protobuf")

            XCTAssertEqual(newDynamicInfo!.includedPeerIDs.count, 0, "peer1 should no longer trust anyone")
            XCTAssertEqual(newDynamicInfo!.excludedPeerIDs, Set([peer1ID]), "peer1 should exclude itself")
            updateTrustExpectation.fulfill()
            return nil
        }

        self.mockAuthKit.removeAndSendNotification(try! self.mockAuthKit.machineID(self.mockAuthKit.primaryAltDSID(), flowID: "flowID", deviceSessionID: "deviceSessionID", canSendMetrics: false))

        self.wait(for: [updateTrustExpectation], timeout: 10)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTrust, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)
    }

    func testRemoveSelfWhenRemovedFromLargeDeviceListByIncompleteNotification() throws {
        try self.skipOnRecoveryKeyNotSupported()

        self.startCKAccountStatusMock()

        XCTAssert(self.mockAuthKit.currentDeviceList().count > 1, "AuthKit should have more than one device on the list")

        let peer1ID = self.assertResetAndBecomeTrustedInDefaultContext()
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer1ID)),
                      "peer 1 should trust peer 1")

        let joiningContext = self.makeInitiatorContext(contextID: "joiner", authKitAdapter: self.mockAuthKit2)
        self.assertJoinViaEscrowRecoveryFromDefaultContextWithReciprocationAndTLKShares(joiningContext: joiningContext)

        // Then peer1 drops off the device list
        // It should remove trust in itself
        let updateTrustExpectation = self.expectation(description: "updateTrust")
        self.fakeCuttlefishServer.updateListener = { request in
            XCTAssertTrue(request.hasDynamicInfoAndSig, "updateTrust request should have a dynamic info")
            let newDynamicInfo = TPPeerDynamicInfo(data: request.dynamicInfoAndSig.peerDynamicInfo,
                                                   sig: request.dynamicInfoAndSig.sig)
            XCTAssertNotNil(newDynamicInfo, "should be able to make a dynamic info from protobuf")

            XCTAssertEqual(newDynamicInfo!.includedPeerIDs.count, 0, "peer1 should no longer trust anyone")
            XCTAssertEqual(newDynamicInfo!.excludedPeerIDs, Set([peer1ID]), "peer1 should exclude itself")
            updateTrustExpectation.fulfill()
            return nil
        }

        self.mockAuthKit.excludeDevices.add(try! self.mockAuthKit.machineID(self.mockAuthKit.primaryAltDSID(), flowID: "flowID", deviceSessionID: "deviceSessionID", canSendMetrics: false))
        XCTAssertFalse(self.mockAuthKit.currentDeviceList().contains(self.mockAuthKit.currentMachineID), "AuthKit should not still have device 2 on the list")
        self.mockAuthKit.sendIncompleteNotification()

        self.wait(for: [updateTrustExpectation], timeout: 10)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTrust, within: 10 * NSEC_PER_SEC)

        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)
    }

    func testIgnoreRemoveFromWrongAltDSID() throws {
        self.startCKAccountStatusMock()

        XCTAssert(self.mockAuthKit.currentDeviceList().contains(self.mockAuthKit2.currentMachineID), "AuthKit should already have device 2 on the list")

        let peer1ID = self.assertResetAndBecomeTrustedInDefaultContext()

        let joiningContext = self.makeInitiatorContext(contextID: "joiner", authKitAdapter: self.mockAuthKit2)
        let peer2ID = self.assertJoinViaEscrowRecovery(joiningContext: joiningContext, sponsor: self.cuttlefishContext)

        // Now, tell peer1 about the change. It will trust the peer and upload some TLK shares
        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer2ID)),
                      "peer 1 should trust peer 2 after update")

        // We receive a 'remove' push for peer2's ID, but for the wrong DSID. The peer should do nothing useful.
        self.fakeCuttlefishServer.updateListener = { _ in
            XCTFail("shouldn't have updated trust")
            return nil
        }

        self.mockAuthKit.sendRemoveNotification(try! self.mockAuthKit2.machineID(self.mockAuthKit2.primaryAltDSID(), flowID: "flowID", deviceSessionID: "deviceSessionID", canSendMetrics: false), altDSID: "completely-wrong")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer2ID)),
                      "peer 1 should trust peer 2 after update")

        self.assertMIDList(context: self.cuttlefishContext,
                           allowed: self.mockAuthKit.currentDeviceList(),
                           disallowed: Set(),
                           unknown: Set())
    }

    func testIgnoreAddFromWrongAltDSID() throws {
        self.startCKAccountStatusMock()

        XCTAssert(self.mockAuthKit.currentDeviceList().contains(self.mockAuthKit2.currentMachineID), "AuthKit should already have device 2 on the list")

        let peer1ID = self.assertResetAndBecomeTrustedInDefaultContext()

        let joiningContext = self.makeInitiatorContext(contextID: "joiner", authKitAdapter: self.mockAuthKit2)
        let peer2ID = self.assertJoinViaEscrowRecovery(joiningContext: joiningContext, sponsor: self.cuttlefishContext)

        // Now, tell peer1 about the change. It will trust the peer and upload some TLK shares
        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer2ID)),
                      "peer 1 should trust peer 2 after update")

        // We receive a 'add' push for a new ID, but for the wrong DSID. The peer should do nothing useful.
        self.fakeCuttlefishServer.updateListener = { _ in
            XCTFail("shouldn't have updated trust")
            return nil
        }

        let newMachineID = "newID"
        self.mockAuthKit.sendAddNotification(newMachineID, altDSID: "completely-wrong")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer2ID)),
                      "peer 1 should trust peer 2 after update")

        // newMachineID should be on no lists
        self.assertMIDList(context: self.cuttlefishContext,
                           allowed: self.mockAuthKit.currentDeviceList(),
                           disallowed: Set(),
                           unknown: Set())
    }

    func testPeerJoiningWithUnknownMachineIDTriggersMachineIDFetchAfterGracePeriod() throws {
        self.startCKAccountStatusMock()

        // Peer 2 is not on Peer 1's machine ID list yet
        self.mockAuthKit.otherDevices.remove(self.mockAuthKit2.currentMachineID)

        _ = self.assertResetAndBecomeTrustedInDefaultContext()
        let joiningContext = self.makeInitiatorContext(contextID: "joiner", authKitAdapter: self.mockAuthKit2)
        _ = self.assertJoinViaEscrowRecovery(joiningContext: joiningContext, sponsor: self.cuttlefishContext)

        // Now, add peer2 to the machineID list, but don't send peer1 a notification about the IDMS change
        self.mockAuthKit.otherDevices.add(self.mockAuthKit2.currentMachineID)
        self.assertMIDList(context: self.cuttlefishContext, allowed: self.mockAuthKit.currentDeviceList().subtracting(Set([self.mockAuthKit2.currentMachineID])))

        let condition = CKKSCondition()
        self.mockAuthKit.fetchCondition = condition

        // But peer1 does get the cuttlefish push
        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // At this time, peer1 should trust peer2, but it should _not_ have fetched the AuthKit list,
        // because peer2 is still within the 48 hour grace period. peer1 is hoping for a push to arrive.

        XCTAssertNotEqual(condition.wait(2 * NSEC_PER_SEC), 0, "Octagon should not fetch the authkit machine ID list")
        let peer2MIDSet = Set([self.mockAuthKit2.currentMachineID])
        self.assertMIDList(context: self.cuttlefishContext,
                           allowed: self.mockAuthKit.currentDeviceList().subtracting(peer2MIDSet),
                           unknown: peer2MIDSet)

        // Now, let's pretend that 2 days pass, and do this again... Octagon should now fetch the MID list and become happy.
        let container = try self.tphClient.getContainer(with: try XCTUnwrap(self.cuttlefishContext.activeAccount))
        container.moc.performAndWait {
            var foundPeer2 = false
            for machinemo in container.containerMO.machines as? Set<MachineMO> ?? Set()
                where machinemo.machineID == self.mockAuthKit2.currentMachineID {
                    foundPeer2 = true
                    //
                    machinemo.modified = Date(timeIntervalSinceNow: -60 * 60 * 24 * TimeInterval(2))
                    XCTAssertEqual(machinemo.status, Int64(TPMachineIDStatus.unknown.rawValue), "peer2's MID entry should be 'unknown'")
            }

            XCTAssertTrue(foundPeer2, "Should have found an entry for peer2")
            try! container.moc.save()
        }

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(condition.wait(10 * NSEC_PER_SEC), 0, "Octagon should fetch the authkit machine ID list")

        self.assertMIDList(context: self.cuttlefishContext, allowed: self.mockAuthKit.currentDeviceList())
    }

    func testTrustPeerWheMissingFromDeviceListAndLocked() throws {
        try self.skipOnRecoveryKeyNotSupported()

        self.startCKAccountStatusMock()

        self.mockAuthKit.otherDevices.removeAllObjects()
        XCTAssertEqual(self.mockAuthKit.currentDeviceList(), Set([self.mockAuthKit.currentMachineID]), "AuthKit should have exactly one device on the list")
        XCTAssertFalse(self.mockAuthKit.currentDeviceList().contains(self.mockAuthKit2.currentMachineID), "AuthKit should not already have device 2 on the list")

        let peer1ID = self.assertResetAndBecomeTrustedInDefaultContext()

        let joiningContext = self.makeInitiatorContext(contextID: "joiner", authKitAdapter: self.mockAuthKit2)
        let peer2ID = self.assertJoinViaEscrowRecovery(joiningContext: joiningContext, sponsor: self.cuttlefishContext)

        // Now, tell peer1 about the change. It will trust the peer, despite it missing from the list, and upload some TLK shares
        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer2ID)),
                      "peer 1 should trust peer 2 after update")

        // Then peer2 arrives on the device list. Peer 1 should update its dynamic info to no longer have a disposition for peer2.
        let updateTrustExpectation = self.expectation(description: "updateTrust")
        self.fakeCuttlefishServer.updateListener = { request in
            XCTAssertTrue(request.hasDynamicInfoAndSig, "updateTrust request should have a dynamic info")
            let newDynamicInfo = TPPeerDynamicInfo(data: request.dynamicInfoAndSig.peerDynamicInfo,
                                                   sig: request.dynamicInfoAndSig.sig)
            XCTAssertNotNil(newDynamicInfo, "should be able to make a dynamic info from protobuf")

            // TODO: swift refuses to see the dispositions object on newDynamicInfo; ah well
            updateTrustExpectation.fulfill()
            return nil
        }

        // Now, peer should lock and receive an Octagon push
        self.aksLockState = true
        self.lockStateTracker.recheck()

        // Now, peer2 should receive an Octagon push, try to realize it is preapproved, and get stuck
        self.sendContainerChange(context: self.cuttlefishContext)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.mockAuthKit.addAndSendNotification(try! self.mockAuthKit2.machineID(self.mockAuthKit2.primaryAltDSID(), flowID: "flowID", deviceSessionID: "deviceSessionID", canSendMetrics: false))

        try self.waitForPushToArriveAtStateMachine(context: self.cuttlefishContext)
        XCTAssertTrue(self.cuttlefishContext.stateMachine.possiblePendingFlags().contains(OctagonFlagCuttlefishNotification), "Should have recd_push pending flag")

        let pendingFlagCondition = try XCTUnwrap(self.cuttlefishContext.stateMachine.flags.condition(forFlag: OctagonFlagCuttlefishNotification))

        // Now, peer should unlock and receive an Octagon push
        self.aksLockState = false
        self.lockStateTracker.recheck()

        XCTAssertEqual(0, pendingFlagCondition.wait(10 * NSEC_PER_SEC), "State machine should have handled the notification")

        XCTAssertEqual(self.cuttlefishContext.stateMachine.possiblePendingFlags(), [], "Should have 0 pending flags")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.wait(for: [updateTrustExpectation], timeout: 10)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer2ID)),
                      "peer 1 should trust peer 2 after update")
    }

    func testDemoAccountBypassIDMSTrustedDeviceList() throws {
        // Check that we can bypass IdMS trusted device list (needed for demo accounts)

        self.startCKAccountStatusMock()

        self.mockAuthKit.excludeDevices.union(self.mockAuthKit.currentDeviceList())
        let account = CloudKitAccount(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()), persona: nil, hsa2: true, demo: true, accountStatus: .available)
        self.mockAuthKit.add(account)
        XCTAssertTrue(self.mockAuthKit.currentDeviceList().isEmpty, "should have zero devices")

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

        // and all subCKKSes should enter ready...
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)

        // And we haven't helpfully added anything to the MID list
        self.assertMIDList(context: self.cuttlefishContext, allowed: Set(), disallowed: Set(), unknown: Set())
    }

    func testDemoAccountTrustPeerWhenMissingFromDeviceList() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        self.mockAuthKit.otherDevices.removeAllObjects()
        let account = CloudKitAccount(altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()), persona: nil, hsa2: true, demo: true, accountStatus: .available)
        self.mockAuthKit.add(account)

        let account2 = CloudKitAccount(altDSID: try XCTUnwrap(self.mockAuthKit2.primaryAltDSID()), persona: nil, hsa2: true, demo: true, accountStatus: .available)
        self.mockAuthKit2.add(account2)

        XCTAssertEqual(self.mockAuthKit.currentDeviceList(), Set([self.mockAuthKit.currentMachineID]), "AuthKit should have exactly one device on the list")
        XCTAssertFalse(self.mockAuthKit.currentDeviceList().contains(self.mockAuthKit2.currentMachineID), "AuthKit should not already have device 2 on the list")

        let peer1ID = self.assertResetAndBecomeTrustedInDefaultContext()

        let joiningContext = self.makeInitiatorContext(contextID: "joiner", authKitAdapter: self.mockAuthKit2)
        let peer2ID = self.assertJoinViaEscrowRecovery(joiningContext: joiningContext, sponsor: self.cuttlefishContext)

        // Now, tell peer1 about the change. It will trust the peer, despite it missing from the list, and upload some TLK shares
        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer2ID)),
                      "peer 1 should trust peer 2 after update")
        self.assertMIDList(context: self.cuttlefishContext,
                           allowed: self.mockAuthKit.currentDeviceList(),
                           disallowed: Set(),
                           unknown: Set())

        // On a follow-up update, peer1 should _not_ hit IDMS, even though there's an unknown peer ID in its DB
        let currentCount = self.mockAuthKit.fetchInvocations
        self.sendContainerChangeWaitForFetchForStates(context: self.cuttlefishContext, states: [OctagonStateReadyUpdated, OctagonStateReady])
        self.assertMIDList(context: self.cuttlefishContext,
                           allowed: self.mockAuthKit.currentDeviceList(),
                           disallowed: Set(),
                           unknown: Set())
        XCTAssertEqual(currentCount, self.mockAuthKit.fetchInvocations, "Receving a push while having an unknown peer MID should not cause an AuthKit fetch")

        ////////

        // Then peer2 arrives on the device list. Peer 1 should update its dynamic info to no longer have a disposition for peer2.
        let updateTrustExpectation = self.expectation(description: "updateTrust")
        self.fakeCuttlefishServer.updateListener = { request in
            XCTAssertTrue(request.hasDynamicInfoAndSig, "updateTrust request should have a dynamic info")
            let newDynamicInfo = TPPeerDynamicInfo(data: request.dynamicInfoAndSig.peerDynamicInfo,
                                                   sig: request.dynamicInfoAndSig.sig)
            XCTAssertNotNil(newDynamicInfo, "should be able to make a dynamic info from protobuf")

            // TODO: swift refuses to see the dispositions object on newDynamicInfo; ah well
            updateTrustExpectation.fulfill()
            return nil
        }

        self.mockAuthKit.addAndSendNotification(try! self.mockAuthKit2.machineID(self.mockAuthKit2.primaryAltDSID(), flowID: "flowID", deviceSessionID: "deviceSessionID", canSendMetrics: false))

        self.wait(for: [updateTrustExpectation], timeout: 10)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer2ID)),
                      "peer 1 should trust peer 2 after update")
    }

    func testRemovePeerWhenRemovedFromDeviceListAndAuthkitThrowsError() throws {
        try self.skipOnRecoveryKeyNotSupported()

        self.startCKAccountStatusMock()

        XCTAssert(self.mockAuthKit.currentDeviceList().contains(self.mockAuthKit2.currentMachineID), "AuthKit should already have device 2 on the list")

        let peer2ContextID = "joiner"
        #if os(watchOS)
        self.mockDeviceInfo = OTMockDeviceInfoAdapter(modelID: "iPhone15,2",
                                                      deviceName: "I'm an iPhone",
                                                      serialNumber: "456",
                                                      osVersion: "iPhone OS Version")
        #endif
        let peer1ID = self.assertResetAndBecomeTrustedInDefaultContext()

        let joiningContext = self.makeInitiatorContext(contextID: peer2ContextID, authKitAdapter: self.mockAuthKit2)
        let peer2ID = self.assertJoinViaEscrowRecovery(joiningContext: joiningContext, sponsor: self.cuttlefishContext)

        // Now, tell peer1 about the change. It will trust the peer and upload some TLK shares
        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer2ID)),
                      "peer 1 should trust peer 2 after update")

        // Then peer2 drops off the device list, and is no longer able to fetch the deveice list.
        // It shouldn't even try to update Cuttlefish: the write will fail
        self.fakeCuttlefishServer.updateListener = { _ in
            XCTFail("no update was expected")
            return nil
        }

        do {
            let enforcing = try joiningContext.currentlyEnforcingIDMSTDL()
            XCTAssertEqual(enforcing, 1, "Should report that the local MID list will be enforced")
        }

        let deviceListFetches = self.mockAuthKit2.fetchInvocations
        self.mockAuthKit2.injectAuthErrorsAtFetchTime = true
        self.mockAuthKit2.removeAndSendNotification(self.mockAuthKit2.currentMachineID)

        self.assertEnters(context: joiningContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: joiningContext)

        XCTAssertFalse(joiningContext.followupHandler.hasPosted(.stateRepair), "device should not have posted a CFU; other components should post it to regain auth")

        // It should still be on its locally cached list, but it should realize it shouldn't enforce any further list changes
        do {
            let errorPtr: NSErrorPointer = nil
            let onList = joiningContext.machineID(onMemoizedList: self.mockAuthKit2.currentMachineID, error: errorPtr)
            XCTAssertNil(errorPtr, "Should have had no error checking memoized list")
            XCTAssertTrue(onList, "Should no longer report that the local MID is on the memoized MID list")
        }

        do {
            let enforcing = try joiningContext.currentlyEnforcingIDMSTDL()
            XCTAssertEqual(enforcing, 0, "Should not report that the local MID list will be enforced")
        }

        XCTAssertEqual(deviceListFetches + 1, self.mockAuthKit2.fetchInvocations, "Should have fetched device list exactly once")

        // Now, when peer1 hears of the change, it should distrust peer2.
        let updateTrustExpectation = self.expectation(description: "updateTrust")
        self.fakeCuttlefishServer.updateListener = { request in
            XCTAssertTrue(request.hasDynamicInfoAndSig, "updateTrust request should have a dynamic info")
            let newDynamicInfo = TPPeerDynamicInfo(data: request.dynamicInfoAndSig.peerDynamicInfo,
                                                   sig: request.dynamicInfoAndSig.sig)
            XCTAssertNotNil(newDynamicInfo, "should be able to make a dynamic info from protobuf")

            XCTAssertTrue(!(newDynamicInfo?.includedPeerIDs.contains(peer2ID) ?? true), "peer1 should no longer trust peer2")
            updateTrustExpectation.fulfill()
            return nil
        }

        self.mockAuthKit.removeAndSendNotification(try! self.mockAuthKit2.machineID(self.mockAuthKit2.primaryAltDSID(), flowID: "flowID", deviceSessionID: "deviceSessionID", canSendMetrics: false))

        self.wait(for: [updateTrustExpectation], timeout: 10)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .excludes, target: peer2ID)),
                      "peer 1 should distrust peer 2 after update")

        // But peer2 can always rejoin, if it's readded!
        self.mockAuthKit2.excludeDevices.remove(self.mockAuthKit2.currentMachineID)
        let peer2ID_retry = self.assertJoinViaEscrowRecovery(joiningContext: joiningContext, sponsor: self.cuttlefishContext)

        XCTAssertNotEqual(peer2ID, peer2ID_retry, "Should use a different peer ID after rejoining")
        self.assertEnters(context: joiningContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: joiningContext)

        do {
            let errorPtr: NSErrorPointer = nil
            let onList = joiningContext.machineID(onMemoizedList: self.mockAuthKit2.currentMachineID, error: errorPtr)
            XCTAssertNil(errorPtr, "Should have had no error checking memoized list")
            XCTAssertTrue(onList, "Should be on the memoized list again")
        }
    }

    func testResetWhileNotOnListQuiesces() throws {
        self.startCKAccountStatusMock()

        self.assertResetAndBecomeTrustedInDefaultContext()

        self.fakeCuttlefishServer.updateListener = { _ in
            XCTFail("no update was expected")
            return nil
        }

        self.mockAuthKit.injectAuthErrorsAtFetchTime = true
        self.mockAuthKit.removeAndSendNotification(self.mockAuthKit.currentMachineID)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        let deviceListFetches = self.mockAuthKit.fetchInvocations

        do {
            let arguments = self.createOTConfigurationContextForTests(contextID: self.cuttlefishContext.contextID,
                                                                      otControl: self.otControl,
                                                                      altDSID: try XCTUnwrap(self.cuttlefishContext.activeAccount?.altDSID))
            try OTClique.newFriends(withContextData: arguments, resetReason: .testGenerated)
            XCTFail("Should have errored trying to perform an Octagon Establish while not on the trusted device list")
        } catch {
            // pass
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(3 * NSEC_PER_SEC), "State machine should be paused, and not looping")
        XCTAssertEqual(deviceListFetches + 1, self.mockAuthKit.fetchInvocations, "Should have fetched device list exactly once")
    }

    func testATVCannotDistrustDevicesUponTDLDistrust() throws {
        try self.skipOnRecoveryKeyNotSupported()

        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

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
        self.verifyDatabaseMocks()
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        do {
            let peersByID = try clique.peerDeviceNamesByPeerID()
            XCTAssertNotNil(peersByID, "Should have received information on peers")
            XCTAssertTrue(peersByID.isEmpty, "peer1 should report no trusted peers")
        } catch {
            XCTFail("Error thrown: \(error)")
        }

        let appleTVDeviceAdapter = OTMockDeviceInfoAdapter(modelID: "AppleTV",
                                                           deviceName: "test-appletv",
                                                           serialNumber: NSUUID().uuidString,
                                                           osVersion: "21K627")

        let appleTV = self.manager.context(forContainerName: OTCKContainerName,
                                         contextID: "appletv-context",
                                         sosAdapter: self.mockSOSAdapter!,
                                         accountsAdapter: self.mockAuthKit2,
                                         authKitAdapter: self.mockAuthKit2,
                                         tooManyPeersAdapter: self.mockTooManyPeers,
                                         tapToRadarAdapter: self.mockTapToRadar,
                                         lockStateTracker: self.lockStateTracker,
                                         deviceInformationAdapter: appleTVDeviceAdapter)

        appleTV.startOctagonStateMachine()

        self.assertJoinViaProximitySetup(joiningContext: appleTV, sponsor: self.cuttlefishContext)
        self.assertEnters(context: appleTV, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.mockAuthKit2.removeAndSendNotification(self.mockAuthKit.currentMachineID)

        self.sendContainerChangeWaitForFetch(context: appleTV)

        var stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(appleTV.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should be nil")
            XCTAssertNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should be nil")

            XCTAssertNil(dump!["modelRecoverySigningPublicKey"], "modelRecoverySigningPublicKey should be nil")
            XCTAssertNil(dump!["modelRecoveryEncryptionPublicKey"], "modelRecoveryEncryptionPublicKey should be nil")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        // Cuttlefish context removes itself from the IdMS TDL and becomes untrusted
        self.mockAuthKit.removeAndSendNotification(self.mockAuthKit.currentMachineID)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // appleTV gets updates from cuttlefish context
        self.sendContainerChangeWaitForFetch(context: appleTV)

        // appleTV should now be the only device in the account
        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(appleTV.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should be nil")
            XCTAssertNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should be nil")

            XCTAssertNil(dump!["modelRecoverySigningPublicKey"], "modelRecoverySigningPublicKey should be nil")
            XCTAssertNil(dump!["modelRecoveryEncryptionPublicKey"], "modelRecoveryEncryptionPublicKey should be nil")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 1, "should be 1 peer id")

            let excluded = dynamicInfo!["excluded"] as? [String]
            XCTAssertNotNil(excluded, "excluded should not be nil")

            XCTAssertNotEqual(egoSelf!["peerID"] as! String, excluded![0] as String, "peer should be excluded")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        // cuttlefish context should have a dynamic info
        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should be nil")
            XCTAssertNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should be nil")

            XCTAssertNil(dump!["modelRecoverySigningPublicKey"], "modelRecoverySigningPublicKey should be nil")
            XCTAssertNil(dump!["modelRecoveryEncryptionPublicKey"], "modelRecoveryEncryptionPublicKey should be nil")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNil(included, "included should be nil")

            let excluded = dynamicInfo!["excluded"] as? [String]
            XCTAssertNotNil(excluded, "excluded should not be nil")

            XCTAssertEqual(egoSelf!["peerID"] as! String, excluded![0] as String, "peer should be excluded")
            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }

    func testHomePodCannotDistrustDevicesUponTDLDistrust() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

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
        self.verifyDatabaseMocks()
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        do {
            let peersByID = try clique.peerDeviceNamesByPeerID()
            XCTAssertNotNil(peersByID, "Should have received information on peers")
            XCTAssertTrue(peersByID.isEmpty, "peer1 should report no trusted peers")
        } catch {
            XCTFail("Error thrown: \(error)")
        }

        let homepodDeviceAdapter = OTMockDeviceInfoAdapter(modelID: "AudioAccessory",
                                                           deviceName: "test-homepod",
                                                           serialNumber: NSUUID().uuidString,
                                                           osVersion: "21K627")

        let homepod = self.manager.context(forContainerName: OTCKContainerName,
                                         contextID: "appletv-context",
                                         sosAdapter: self.mockSOSAdapter!,
                                         accountsAdapter: self.mockAuthKit2,
                                         authKitAdapter: self.mockAuthKit2,
                                         tooManyPeersAdapter: self.mockTooManyPeers,
                                         tapToRadarAdapter: self.mockTapToRadar,
                                         lockStateTracker: self.lockStateTracker,
                                         deviceInformationAdapter: homepodDeviceAdapter)

        homepod.startOctagonStateMachine()

        self.assertJoinViaProximitySetup(joiningContext: homepod, sponsor: self.cuttlefishContext)
        self.assertEnters(context: homepod, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.mockAuthKit2.removeAndSendNotification(self.mockAuthKit.currentMachineID)

        self.sendContainerChangeWaitForFetch(context: homepod)

        var stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(homepod.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should be nil")
            XCTAssertNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should be nil")

            XCTAssertNil(dump!["modelRecoverySigningPublicKey"], "modelRecoverySigningPublicKey should be nil")
            XCTAssertNil(dump!["modelRecoveryEncryptionPublicKey"], "modelRecoveryEncryptionPublicKey should be nil")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        // Cuttlefish context removes itself from the IdMS TDL and becomes untrusted
        self.mockAuthKit.removeAndSendNotification(self.mockAuthKit.currentMachineID)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // appleTV gets updates from cuttlefish context
        self.sendContainerChangeWaitForFetch(context: homepod)

        // appleTV should now be the only device in the account
        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(homepod.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should be nil")
            XCTAssertNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should be nil")

            XCTAssertNil(dump!["modelRecoverySigningPublicKey"], "modelRecoverySigningPublicKey should be nil")
            XCTAssertNil(dump!["modelRecoveryEncryptionPublicKey"], "modelRecoveryEncryptionPublicKey should be nil")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 1, "should be 1 peer id")

            let excluded = dynamicInfo!["excluded"] as? [String]
            XCTAssertNotNil(excluded, "excluded should not be nil")

            XCTAssertNotEqual(egoSelf!["peerID"] as! String, excluded![0] as String, "peer should be excluded")

            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)

        // cuttlefish context should have a dynamic info
        stableInfoCheckDumpCallback = self.expectation(description: "stableInfoCheckDumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")

            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            XCTAssertNil(stableInfo!["recovery_signing_public_key"], "recoverySigningPublicKey should be nil")
            XCTAssertNil(stableInfo!["recovery_encryption_public_key"], "recoveryEncryptionPublicKey should be nil")

            XCTAssertNil(dump!["modelRecoverySigningPublicKey"], "modelRecoverySigningPublicKey should be nil")
            XCTAssertNil(dump!["modelRecoveryEncryptionPublicKey"], "modelRecoveryEncryptionPublicKey should be nil")

            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNil(included, "included should be nil")

            let excluded = dynamicInfo!["excluded"] as? [String]
            XCTAssertNotNil(excluded, "excluded should not be nil")

            XCTAssertEqual(egoSelf!["peerID"] as! String, excluded![0] as String, "peer should be excluded")
            stableInfoCheckDumpCallback.fulfill()
        }
        self.wait(for: [stableInfoCheckDumpCallback], timeout: 10)
    }
}

#endif
