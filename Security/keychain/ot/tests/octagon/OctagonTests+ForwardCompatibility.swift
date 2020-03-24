#if OCTAGON

import Foundation

class OctagonForwardCompatibilityTests: OctagonTestsBase {
    func testApprovePeerWithNewPolicy() throws {
        self.startCKAccountStatusMock()
        let peer1ID = self.assertResetAndBecomeTrustedInDefaultContext()

        // Now, we'll approve a new peer with a new policy! First, make that new policy.
        let currentPolicyOptional = builtInPolicyDocuments().filter { $0.version.versionNumber == prevailingPolicyVersion.versionNumber }.first
        XCTAssertNotNil(currentPolicyOptional, "Should have one current policy")
        let currentPolicy = currentPolicyOptional!

        let newPolicy = currentPolicy.clone(withVersionNumber: currentPolicy.version.versionNumber + 1)!
        self.fakeCuttlefishServer.policyOverlay.append(newPolicy)

        let peer2ContextID = "asdf"

        // Assist the other client here: it'll likely already have the policy built-in
        let fetchExpectation = self.expectation(description: "fetch callback occurs")
        self.tphClient.fetchPolicyDocuments(withContainer: OTCKContainerName,
                                            context: peer2ContextID,
                                            versions: Set([newPolicy.version])) { _, error in
                                                XCTAssertNil(error, "Should have no error")
                                                fetchExpectation.fulfill()
        }
        self.wait(for: [fetchExpectation], timeout: 10)

        var peer2ID: String = "not initialized"

        let prepareExpectation = self.expectation(description: "prepare callback occurs")
        let vouchExpectation = self.expectation(description: "vouch callback occurs")
        let joinExpectation = self.expectation(description: "join callback occurs")
        let serverJoinExpectation = self.expectation(description: "joinWithVoucher is called")

        self.tphClient.prepare(withContainer: OTCKContainerName,
                               context: peer2ContextID,
                               epoch: 1,
                               machineID: self.mockAuthKit2.currentMachineID,
                               bottleSalt: self.mockAuthKit2.altDSID!,
                               bottleID: "why-is-this-nonnil",
                               modelID: self.mockDeviceInfo.modelID(),
                               deviceName: "new-policy-peer",
                               serialNumber: "1234",
                               osVersion: "something",
                               policyVersion: newPolicy.version,
                               policySecrets: nil,
                               signingPrivKeyPersistentRef: nil,
                               encPrivKeyPersistentRef: nil) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, _, _, error in
                                XCTAssertNil(error, "Should be no error preparing the second peer")
                                XCTAssertNotNil(peerID, "Should have a peerID")
                                peer2ID = peerID!

                                XCTAssertNotNil(stableInfo, "Should have a stable info")
                                XCTAssertNotNil(stableInfoSig, "Should have a stable info signature")

                                let newStableInfo = TPPeerStableInfo(data: stableInfo!, sig: stableInfoSig!)
                                XCTAssertNotNil(newStableInfo, "should be able to make a stableInfo info from protobuf")

                                XCTAssertEqual(newStableInfo?.frozenPolicyVersion, frozenPolicyVersion, "Frozen policy version in new identity should match frozen policy version")
                                XCTAssertEqual(newStableInfo?.flexiblePolicyVersion, newPolicy.version, "Flexible policy version in new identity should match new policy version")

                                self.tphClient.vouch(withContainer: self.cuttlefishContext.containerName,
                                                     context: self.cuttlefishContext.contextID,
                                                     peerID: peerID!,
                                                     permanentInfo: permanentInfo!,
                                                     permanentInfoSig: permanentInfoSig!,
                                                     stableInfo: stableInfo!,
                                                     stableInfoSig: stableInfoSig!,
                                                     ckksKeys: []) { voucher, voucherSig, error in
                                                        XCTAssertNil(error, "Should be no error vouching")
                                                        XCTAssertNotNil(voucher, "Should have a voucher")
                                                        XCTAssertNotNil(voucherSig, "Should have a voucher signature")

                                                        self.fakeCuttlefishServer.joinListener = { joinRequest in
                                                            XCTAssertEqual(peer2ID, joinRequest.peer.peerID, "joinWithVoucher request should be for peer 2")
                                                            XCTAssertTrue(joinRequest.peer.hasStableInfoAndSig, "Joining peer should have a stable info")
                                                            let newStableInfo = joinRequest.peer.stableInfoAndSig.stableInfo()

                                                            XCTAssertEqual(newStableInfo.frozenPolicyVersion, frozenPolicyVersion, "Frozen policy version in new identity should match frozen policy version")
                                                            XCTAssertEqual(newStableInfo.flexiblePolicyVersion, newPolicy.version, "Flexible policy version in new identity should match new policy version (as provided by new peer)")

                                                            serverJoinExpectation.fulfill()
                                                            return nil
                                                        }

                                                        self.tphClient.join(withContainer: OTCKContainerName,
                                                                            context: peer2ContextID,
                                                                            voucherData: voucher!,
                                                                            voucherSig: voucherSig!,
                                                                            ckksKeys: [],
                                                                            tlkShares: [],
                                                                            preapprovedKeys: []) { peerID, _, _, _, error in
                                                                                XCTAssertNil(error, "Should be no error joining")
                                                                                XCTAssertNotNil(peerID, "Should have a peerID")
                                                                                joinExpectation.fulfill()
                                                        }
                                                        vouchExpectation.fulfill()
                                }
                                prepareExpectation.fulfill()
        }
        self.wait(for: [prepareExpectation, vouchExpectation, joinExpectation, serverJoinExpectation], timeout: 10)

        // Then, after the remote peer joins, the original peer should realize it trusts the new peer and update its own stableinfo to use the new policy
        let updateTrustExpectation = self.expectation(description: "updateTrust")
        self.fakeCuttlefishServer.updateListener = { request in
            XCTAssertEqual(peer1ID, request.peerID, "updateTrust request should be for peer 1")
            let newDynamicInfo = request.dynamicInfoAndSig.dynamicInfo()
            XCTAssert(newDynamicInfo.includedPeerIDs.contains(peer2ID), "Peer1 should trust peer2")

            let newStableInfo = request.stableInfoAndSig.stableInfo()

            XCTAssertEqual(newStableInfo.frozenPolicyVersion, frozenPolicyVersion, "Policy version in peer should match frozen policy version")
            XCTAssertEqual(newStableInfo.flexiblePolicyVersion, newPolicy.version, "Prevailing policy version in peer should match new policy version")

            updateTrustExpectation.fulfill()
            return nil
        }

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.wait(for: [updateTrustExpectation], timeout: 10)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testRejectVouchingForPeerWithUnknownNewPolicy() throws {
        self.startCKAccountStatusMock()
        _ = self.assertResetAndBecomeTrustedInDefaultContext()

        // Now, a new peer joins with a policy we can't fetch
        let currentPolicyOptional = builtInPolicyDocuments().filter { $0.version.versionNumber == prevailingPolicyVersion.versionNumber }.first
        XCTAssertNotNil(currentPolicyOptional, "Should have one current policy")
        let currentPolicy = currentPolicyOptional!

        let newPolicy = currentPolicy.clone(withVersionNumber: currentPolicy.version.versionNumber + 1)!

        let peer2ContextID = "asdf"

        // Assist the other client here: it'll likely already have this built-in
        self.fakeCuttlefishServer.policyOverlay.append(newPolicy)

        let fetchExpectation = self.expectation(description: "fetch callback occurs")
        self.tphClient.fetchPolicyDocuments(withContainer: OTCKContainerName,
                                            context: peer2ContextID,
                                            versions: Set([newPolicy.version])) { _, error in
                                                XCTAssertNil(error, "Should have no error")
                                                fetchExpectation.fulfill()
        }
        self.wait(for: [fetchExpectation], timeout: 10)

        // Remove the policy, now that peer2 has it
        self.fakeCuttlefishServer.policyOverlay.removeAll()

        let prepareExpectation = self.expectation(description: "prepare callback occurs")
        let vouchExpectation = self.expectation(description: "vouch callback occurs")

        self.tphClient.prepare(withContainer: OTCKContainerName,
                               context: peer2ContextID,
                               epoch: 1,
                               machineID: self.mockAuthKit2.currentMachineID,
                               bottleSalt: self.mockAuthKit2.altDSID!,
                               bottleID: "why-is-this-nonnil",
                               modelID: self.mockDeviceInfo.modelID(),
                               deviceName: "new-policy-peer",
                               serialNumber: "1234",
                               osVersion: "something",
                               policyVersion: newPolicy.version,
                               policySecrets: nil,
                               signingPrivKeyPersistentRef: nil,
                               encPrivKeyPersistentRef: nil) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, _, _, error in
                                XCTAssertNil(error, "Should be no error preparing the second peer")
                                XCTAssertNotNil(peerID, "Should have a peerID")

                                XCTAssertNotNil(stableInfo, "Should have a stable info")
                                XCTAssertNotNil(stableInfoSig, "Should have a stable info signature")

                                let newStableInfo = TPPeerStableInfo(data: stableInfo!, sig: stableInfoSig!)
                                XCTAssertNotNil(newStableInfo, "should be able to make a stableInfo info from protobuf")

                                XCTAssertEqual(newStableInfo?.frozenPolicyVersion, frozenPolicyVersion, "Frozen policy version in new identity should match frozen policy version")
                                XCTAssertEqual(newStableInfo?.flexiblePolicyVersion, newPolicy.version, "Flexible policy version in new identity should match new policy version")

                                self.tphClient.vouch(withContainer: self.cuttlefishContext.containerName,
                                                     context: self.cuttlefishContext.contextID,
                                                     peerID: peerID!,
                                                     permanentInfo: permanentInfo!,
                                                     permanentInfoSig: permanentInfoSig!,
                                                     stableInfo: stableInfo!,
                                                     stableInfoSig: stableInfoSig!,
                                                     ckksKeys: []) { voucher, voucherSig, error in
                                                        XCTAssertNotNil(error, "should be an error vouching for a peer with an unknown policy")
                                                        XCTAssertNil(voucher, "Should have no voucher")
                                                        XCTAssertNil(voucherSig, "Should have no voucher signature")

                                                        vouchExpectation.fulfill()
                                }
                                prepareExpectation.fulfill()
        }
        self.wait(for: [prepareExpectation, vouchExpectation], timeout: 10)
    }

    func testIgnoreAlreadyJoinedPeerWithUnknownNewPolicy() throws {
        self.startCKAccountStatusMock()
        let peer1ID = self.assertResetAndBecomeTrustedInDefaultContext()

        // Now, a new peer joins with a policy we can't fetch
        let currentPolicyOptional = builtInPolicyDocuments().filter { $0.version.versionNumber == prevailingPolicyVersion.versionNumber }.first
        XCTAssertNotNil(currentPolicyOptional, "Should have one current policy")
        let currentPolicy = currentPolicyOptional!

        let newPolicy = currentPolicy.clone(withVersionNumber: currentPolicy.version.versionNumber + 1)!

        let peer2ContextID = "asdf"

        // Assist the other client here: it'll likely already have this built-in
        self.fakeCuttlefishServer.policyOverlay.append(newPolicy)

        let fetchExpectation = self.expectation(description: "fetch callback occurs")
        self.tphClient.fetchPolicyDocuments(withContainer: OTCKContainerName,
                                            context: peer2ContextID,
                                            versions: Set([newPolicy.version])) { _, error in
                                                XCTAssertNil(error, "Should have no error")
                                                fetchExpectation.fulfill()
        }
        self.wait(for: [fetchExpectation], timeout: 10)

        // Remove the policy, now that peer2 has it
        self.fakeCuttlefishServer.policyOverlay.removeAll()

        let joiningContext = self.makeInitiatorContext(contextID: peer2ContextID, authKitAdapter: self.mockAuthKit2)
        joiningContext.policyOverride = newPolicy.version

        let serverJoinExpectation = self.expectation(description: "peer2 joins successfully")
        self.fakeCuttlefishServer.joinListener = { joinRequest in
            XCTAssertTrue(joinRequest.peer.hasStableInfoAndSig, "Joining peer should have a stable info")
            let newStableInfo = joinRequest.peer.stableInfoAndSig.stableInfo()

            XCTAssertEqual(newStableInfo.frozenPolicyVersion, frozenPolicyVersion, "Frozen policy version in new identity should match frozen policy version")
            XCTAssertEqual(newStableInfo.flexiblePolicyVersion, newPolicy.version, "Flexible policy version in new identity should match new policy version (as provided by new peer)")

            serverJoinExpectation.fulfill()
            return nil
        }

        _ = self.assertJoinViaEscrowRecovery(joiningContext: joiningContext, sponsor: self.cuttlefishContext)
        self.wait(for: [serverJoinExpectation], timeout: 10)

        // Then, after the remote peer joins, the original peer should ignore it entirely: peer1 has no idea what this new policy is about
        // That means it won't update its trust in response to the join
        self.fakeCuttlefishServer.updateListener = { request in
            XCTFail("Expected no updateTrust after peer1 joins")
            XCTAssertEqual(peer1ID, request.peerID, "updateTrust request should be for peer 1")
            /*
             * But, if it did update its trust, here's what we would expect:
            let newDynamicInfo = request.dynamicInfoAndSig.dynamicInfo()
            XCTAssertFalse(newDynamicInfo.includedPeerIDs.contains(peer2ID), "Peer1 should not trust peer2")
            XCTAssertFalse(newDynamicInfo.excludedPeerIDs.contains(peer2ID), "Peer1 should not distrust peer2")

            let newStableInfo = request.stableInfoAndSig.stableInfo()

            XCTAssertEqual(newStableInfo.frozenPolicyVersion, frozenPolicyVersion, "Policy version in peer should match frozen policy version")
            XCTAssertEqual(newStableInfo.flexiblePolicyVersion, prevailingPolicyVersion, "Prevailing policy version in peer should match current prevailing policy version")

            updateTrustExpectation.fulfill()
             */
            return nil
        }

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func createOctagonAndCKKSUsingFuturePolicy() throws -> (TPPolicyDocument, CKRecordZone.ID) {
        // We want to set up a world with a peer, in Octagon, with TLKs for zones that don't even exist in our current policy.
        // First, make a new policy.
        let currentPolicyOptional = builtInPolicyDocuments().filter { $0.version.versionNumber == prevailingPolicyVersion.versionNumber }.first
        XCTAssertNotNil(currentPolicyOptional, "Should have one current policy")
        let currentPolicyDocument = currentPolicyOptional!

        let futureViewName = "FutureView"
        let futureViewMapping = TPPBPolicyKeyViewMapping()!

        let futureViewZoneID = CKRecordZone.ID(zoneName: futureViewName)
        self.ckksZones.add(futureViewZoneID)
        self.injectedManager!.setSyncingViewsAllowList(Set((self.intendedCKKSZones.union([futureViewZoneID])).map { $0.zoneName }))

        self.zones![futureViewZoneID] = FakeCKZone(zone: futureViewZoneID)

        XCTAssertFalse(currentPolicyDocument.categoriesByView.keys.contains(futureViewName), "Current policy should not include future view")

        let newPolicyDocument = try TPPolicyDocument(internalVersion: currentPolicyDocument.version.versionNumber + 1,
            modelToCategory: currentPolicyDocument.modelToCategory,
            categoriesByView: currentPolicyDocument.categoriesByView.merging([futureViewName: Set(["watch", "full", "tv"])]) { _, new in new },
            introducersByCategory: currentPolicyDocument.introducersByCategory,
            redactions: [:],
            keyViewMapping: [futureViewMapping] + currentPolicyDocument.keyViewMapping,
            hashAlgo: .SHA256)

        self.fakeCuttlefishServer.policyOverlay.append(newPolicyDocument)

        return (newPolicyDocument, futureViewZoneID)
    }

    func testRestoreBottledPeerUsingFuturePolicy() throws {
        let (newPolicyDocument, futureViewZoneID) = try self.createOctagonAndCKKSUsingFuturePolicy()

        let futurePeerContext = self.makeInitiatorContext(contextID: "futurePeer")
        futurePeerContext.policyOverride = newPolicyDocument.version

        self.startCKAccountStatusMock()
        let futurePeerID = self.assertResetAndBecomeTrusted(context: futurePeerContext)

        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: futurePeerContext)
        XCTAssertTrue(try self.tlkShareInCloudKit(receiverPeerID: futurePeerID, senderPeerID: futurePeerID, zoneID: futureViewZoneID))

        // Now, our peer (with no inbuilt knowledge of newPolicyDocument) joins via escrow recovery.
        // It should be able to recover the FutureView TLK
        self.assertAllCKKSViewsUpload(tlkShares: 1)

        let serverJoinExpectation = self.expectation(description: "peer1 joins successfully")
        self.fakeCuttlefishServer.joinListener = { joinRequest in
            XCTAssertTrue(joinRequest.peer.hasStableInfoAndSig, "Joining peer should have a stable info")
            let newStableInfo = joinRequest.peer.stableInfoAndSig.stableInfo()

            XCTAssertEqual(newStableInfo.frozenPolicyVersion, frozenPolicyVersion, "Policy version in peer should match frozen policy version")
            XCTAssertEqual(newStableInfo.flexiblePolicyVersion, newPolicyDocument.version, "Prevailing policy version in peer should match new policy version")

            serverJoinExpectation.fulfill()
            return nil
        }

        let peerID = self.assertJoinViaEscrowRecovery(joiningContext: self.cuttlefishContext, sponsor: futurePeerContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(self.injectedManager!.policy?.version, newPolicyDocument.version, "CKKS should be configured with new policy")
        self.verifyDatabaseMocks()

        self.wait(for: [serverJoinExpectation], timeout: 10)

        // And the joined peer should have recovered the TLK, and uploaded itself a share
        XCTAssertTrue(try self.tlkShareInCloudKit(receiverPeerID: peerID, senderPeerID: peerID, zoneID: futureViewZoneID))
    }

    func testPairingJoinUsingFuturePolicy() throws {
        let (newPolicyDocument, futureViewZoneID) = try self.createOctagonAndCKKSUsingFuturePolicy()

        let futurePeerContext = self.makeInitiatorContext(contextID: "futurePeer")
        futurePeerContext.policyOverride = newPolicyDocument.version

        self.startCKAccountStatusMock()
        let futurePeerID = self.assertResetAndBecomeTrusted(context: futurePeerContext)

        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: futurePeerContext)
        XCTAssertTrue(try self.tlkShareInCloudKit(receiverPeerID: futurePeerID, senderPeerID: futurePeerID, zoneID: futureViewZoneID))

        // Now, our peer (with no inbuilt knowledge of newPolicyDocument) joins via pairing
        // It should be able to recover the FutureView TLK
        self.assertAllCKKSViewsUpload(tlkShares: 1)

        let serverJoinExpectation = self.expectation(description: "peer1 joins successfully")
        self.fakeCuttlefishServer.joinListener = { joinRequest in
            XCTAssertTrue(joinRequest.peer.hasStableInfoAndSig, "Joining peer should have a stable info")
            let newStableInfo = joinRequest.peer.stableInfoAndSig.stableInfo()

            XCTAssertEqual(newStableInfo.frozenPolicyVersion, frozenPolicyVersion, "Policy version in peer should match frozen policy version")
            XCTAssertEqual(newStableInfo.flexiblePolicyVersion, newPolicyDocument.version, "Prevailing policy version in peer should match new policy version")

            serverJoinExpectation.fulfill()
            return nil
        }

        let peerID = self.assertJoinViaProximitySetup(joiningContext: self.cuttlefishContext, sponsor: futurePeerContext)

        // And then fake like the other peer uploaded TLKShares after the join succeeded (it would normally happen during, but that's okay)
        try self.putAllTLKSharesInCloudKit(to: self.cuttlefishContext, from: futurePeerContext)
        self.sendAllCKKSViewsZoneChanged()

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(self.injectedManager!.policy?.version, newPolicyDocument.version, "CKKS should be configured with new policy")
        self.verifyDatabaseMocks()

        self.wait(for: [serverJoinExpectation], timeout: 10)

        // And the joined peer should have recovered the TLK, and uploaded itself a share
        XCTAssertTrue(try self.tlkShareInCloudKit(receiverPeerID: peerID, senderPeerID: peerID, zoneID: futureViewZoneID))
    }

    func testRecoveryKeyJoinUsingFuturePolicy() throws {
        let (newPolicyDocument, futureViewZoneID) = try self.createOctagonAndCKKSUsingFuturePolicy()

        let futurePeerContext = self.makeInitiatorContext(contextID: "futurePeer")
        futurePeerContext.policyOverride = newPolicyDocument.version

        self.startCKAccountStatusMock()
        let futurePeerID = self.assertResetAndBecomeTrusted(context: futurePeerContext)

        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: futurePeerContext)
        XCTAssertTrue(try self.tlkShareInCloudKit(receiverPeerID: futurePeerID, senderPeerID: futurePeerID, zoneID: futureViewZoneID))

        // Create the recovery key
        let recoveryKey = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        self.manager.setSOSEnabledForPlatformFlag(true)

        let createRecoveryExpectation = self.expectation(description: "createRecoveryExpectation returns")
        self.manager.createRecoveryKey(OTCKContainerName, contextID: futurePeerContext.contextID, recoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            createRecoveryExpectation.fulfill()
        }
        self.wait(for: [createRecoveryExpectation], timeout: 10)

        // Setting the RK will make TLKShares to the RK, so help that out too
        try self.putRecoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: self.mockAuthKit.altDSID!)

        // Now, our peer (with no inbuilt knowledge of newPolicyDocument) joins via RecoveryKey
        let serverJoinExpectation = self.expectation(description: "peer1 joins successfully")
        self.fakeCuttlefishServer.joinListener = { joinRequest in
            XCTAssertTrue(joinRequest.peer.hasStableInfoAndSig, "Joining peer should have a stable info")
            let newStableInfo = joinRequest.peer.stableInfoAndSig.stableInfo()

            XCTAssertEqual(newStableInfo.frozenPolicyVersion, frozenPolicyVersion, "Policy version in peer should match frozen policy version")
            XCTAssertEqual(newStableInfo.flexiblePolicyVersion, newPolicyDocument.version, "Prevailing policy version in peer should match new policy version")

            serverJoinExpectation.fulfill()
            return nil
        }

        // It should recover and upload the FutureView TLK
        self.assertAllCKKSViewsUpload(tlkShares: 1)

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let joinWithRecoveryKeyExpectation = self.expectation(description: "joinWithRecoveryKey callback occurs")
        self.cuttlefishContext.join(withRecoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithRecoveryKeyExpectation], timeout: 10)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(self.injectedManager!.policy?.version, newPolicyDocument.version, "CKKS should be configured with new policy")
        self.verifyDatabaseMocks()

        self.wait(for: [serverJoinExpectation], timeout: 10)
    }

    func testPreapprovedJoinUsingFuturePolicy() throws {
        let peer2mockSOS = CKKSMockSOSPresentAdapter(selfPeer: self.createSOSPeer(peerID: "peer2ID"), trustedPeers: self.mockSOSAdapter.allPeers(), essential: false)
        print(peer2mockSOS.allPeers())
        self.mockSOSAdapter.trustedPeers.add(peer2mockSOS.selfPeer)

        let futurePeerContext = self.manager.context(forContainerName: OTCKContainerName,
                                                     contextID: "futurePeer",
                                                     sosAdapter: peer2mockSOS,
                                                     authKitAdapter: self.mockAuthKit2,
                                                     lockStateTracker: self.lockStateTracker,
                                                     accountStateTracker: self.accountStateTracker,
                                                     deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-SOS-iphone", serialNumber: "456", osVersion: "iOS (fake version)"))

        let (newPolicyDocument, _) = try self.createOctagonAndCKKSUsingFuturePolicy()
        futurePeerContext.policyOverride = newPolicyDocument.version

        let serverEstablishExpectation = self.expectation(description: "futurePeer establishes successfully")
        self.fakeCuttlefishServer.establishListener = { establishRequest in
            XCTAssertTrue(establishRequest.peer.hasStableInfoAndSig, "Establishing peer should have a stable info")
            let newStableInfo = establishRequest.peer.stableInfoAndSig.stableInfo()

            XCTAssertEqual(newStableInfo.frozenPolicyVersion, frozenPolicyVersion, "Policy version in peer should match frozen policy version")
            XCTAssertEqual(newStableInfo.flexiblePolicyVersion, newPolicyDocument.version, "Prevailing policy version in peer should match new policy version")
            serverEstablishExpectation.fulfill()
            return nil
        }

        // Setup is complete. Join using a new peer
        self.startCKAccountStatusMock()
        futurePeerContext.startOctagonStateMachine()
        self.assertEnters(context: futurePeerContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.wait(for: [serverEstablishExpectation], timeout: 10)

        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: futurePeerContext)

        // Now, the default peer joins via SOS preapproval
        let serverJoinExpectation = self.expectation(description: "peer1 joins successfully")
        self.fakeCuttlefishServer.joinListener = { joinRequest in
            XCTAssertTrue(joinRequest.peer.hasStableInfoAndSig, "Joining peer should have a stable info")
            let newStableInfo = joinRequest.peer.stableInfoAndSig.stableInfo()

            XCTAssertEqual(newStableInfo.frozenPolicyVersion, frozenPolicyVersion, "Policy version in peer should match frozen policy version")
            XCTAssertEqual(newStableInfo.flexiblePolicyVersion, newPolicyDocument.version, "Prevailing policy version in peer should match new policy version")
            serverJoinExpectation.fulfill()
            return nil
        }

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.wait(for: [serverJoinExpectation], timeout: 10)

        // But, since we're not mocking the remote peer sharing the TLKs, ckks should get stuck
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLK, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(self.injectedManager!.policy?.version, newPolicyDocument.version, "CKKS should be configured with new policy")
        self.verifyDatabaseMocks()
    }

    func testRespondToFuturePoliciesInPeerUpdates() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        // Now, another peer comes along and joins via BP recovery, using a new policy
        let (newPolicyDocument, _) = try self.createOctagonAndCKKSUsingFuturePolicy()

        let futurePeerContext = self.makeInitiatorContext(contextID: "futurePeer")
        futurePeerContext.policyOverride = newPolicyDocument.version

        let serverJoinExpectation = self.expectation(description: "futurePeer joins successfully")
         self.fakeCuttlefishServer.joinListener = { joinRequest in
             XCTAssertTrue(joinRequest.peer.hasStableInfoAndSig, "Joining peer should have a stable info")
             let newStableInfo = joinRequest.peer.stableInfoAndSig.stableInfo()

             XCTAssertEqual(newStableInfo.frozenPolicyVersion, frozenPolicyVersion, "Policy version in peer should match frozen policy version")
             XCTAssertEqual(newStableInfo.flexiblePolicyVersion, newPolicyDocument.version, "Prevailing policy version in peer should match new policy version")
             serverJoinExpectation.fulfill()
             return nil
         }

        let peer2ID = self.assertJoinViaEscrowRecovery(joiningContext: futurePeerContext, sponsor: self.cuttlefishContext)
        self.wait(for: [serverJoinExpectation], timeout: 10)

        // Now, tell our first peer about the new changes. It should trust the new peer, and update its policy
        let updateTrustExpectation = self.expectation(description: "updateTrustExpectation successfully")
        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()

            XCTAssertEqual(newStableInfo.frozenPolicyVersion, frozenPolicyVersion, "Policy version in peer should match frozen policy version")
            XCTAssertEqual(newStableInfo.flexiblePolicyVersion, newPolicyDocument.version, "Prevailing policy version in peer should match new policy version")

            let newDynamicInfo = request.dynamicInfoAndSig.dynamicInfo()
            XCTAssert(newDynamicInfo.includedPeerIDs.contains(peer2ID), "Peer1 should trust peer2")

            updateTrustExpectation.fulfill()
            return nil
        }

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.wait(for: [updateTrustExpectation], timeout: 10)
    }
}

#endif
