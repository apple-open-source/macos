#if OCTAGON

import Security

class OctagonPerformanceTests: OctagonTestsBase {
    func testJoinWithManyPeersAndManyDistrustedPeersWithRecoveryKey() throws {
        #if true
        // Skip this test by default and in BATS. Once this entire test runs within 15-20s, we can enable this.
        throw XCTSkip("Ignoring test due to performance")
        #else

        // Turn off graph validity checking on every update(); trust this test to do the right thing
        self.fakeCuttlefishServer.checkValidityofGraphOnUpdateTrust = false

        // This test will:
        //  1. Establish an Octagon clique from an iPhone
        //  2. Join $homePodPeersToAdd HomePod peers via direct voucher creation
        //  3. Update the original establisher's idea of the world (it should trust all 200 homepod peers)
        //  4. Join one more iPhone peer using pairing with the establisher
        //  5. Remove all but $homePodPeersToKeepTrusted HomePods from the TDL, and have the establishing peer kick them out via update()
        //  6. Join one more iPhone peer using pairing with the establisher
        //
        //  It's intended to probe any differences in join time when there are many untrusted peers via many trusted peers.

        let homePodPeersToAdd = 200
        let homePodPeersToKeepTrusted = 5
        let everyPeerHasRecoveryKey = true

        // HomePods don't ever preapprove peers (because they don't run SOS), but this test is only interested in the
        // performance cost of preapprovals. We can have them "preapprove" without breaking any Octagon invariant.
        let everyHomepodPeerPreapprovesFirstHomePod = false

        let homepodMIDs = (0...homePodPeersToAdd).map { i in
            return "homepod\(i)"
        }

        let homePodMIDsToRemove = Array(homepodMIDs.dropLast(homePodPeersToKeepTrusted))

        self.mockAuthKit.otherDevices.addObjects(from: homepodMIDs)
        self.mockAuthKit2.otherDevices.addObjects(from: homepodMIDs)
        self.mockAuthKit3.otherDevices.addObjects(from: homepodMIDs)

        self.startCKAccountStatusMock()

        let establishContext = self.manager.context(forContainerName: OTCKContainerName,
                                                    contextID: "establish-context-id",
                                                    sosAdapter: self.mockSOSAdapter!,
                                                    accountsAdapter: self.mockAuthKit,
                                                    authKitAdapter: self.mockAuthKit,
                                                    tooManyPeersAdapter: self.mockTooManyPeers,
                                                    tapToRadarAdapter: self.mockTapToRadar,
                                                    lockStateTracker: self.lockStateTracker,
                                                    deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-iphone", serialNumber: "456", osVersion: "iOS (fake version)"))

        let join1 = self.manager.context(forContainerName: OTCKContainerName,
                                                    contextID: "join1-context-id",
                                                    sosAdapter: self.mockSOSAdapter!,
                                                    accountsAdapter: self.mockAuthKit2,
                                                    authKitAdapter: self.mockAuthKit2,
                                                    tooManyPeersAdapter: self.mockTooManyPeers,
                                                    tapToRadarAdapter: self.mockTapToRadar,
                                                    lockStateTracker: self.lockStateTracker,
                                                    deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-iphone", serialNumber: "456", osVersion: "iOS (fake version)"))

        let join2 = self.manager.context(forContainerName: OTCKContainerName,
                                                    contextID: "join2-context-id",
                                                    sosAdapter: self.mockSOSAdapter!,
                                                    accountsAdapter: self.mockAuthKit3,
                                                    authKitAdapter: self.mockAuthKit3,
                                                    tooManyPeersAdapter: self.mockTooManyPeers,
                                                    tapToRadarAdapter: self.mockTapToRadar,
                                                    lockStateTracker: self.lockStateTracker,
                                                    deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-iphone", serialNumber: "456", osVersion: "iOS (fake version)"))

        let establishPeerID = self.assertResetAndBecomeTrusted(context: establishContext)

        let recoveryKeyString = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        let recoveryKeys = try RecoveryKey(recoveryKeyString: recoveryKeyString, recoverySalt: try XCTUnwrap(self.mockAuthKit2.primaryAltDSID()))

        if everyPeerHasRecoveryKey {
            XCTAssertTrue(OctagonTrustCliqueBridge.setRecoveryKeyWith(self.createOTConfigurationContextForTests(contextID: establishContext.contextID), recoveryKey: recoveryKeyString, error: nil), "should return true")
        }

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        // Make HomePod keys
        let homepodPeers: [(String, TPKeyPair, TPPeerPermanentInfo, TPPeerStableInfo)] = try homepodMIDs.map { machineID in
            let keySpecifier = _SFECKeySpecifier(curve: .nistp384)
            let encryptionKey = try XCTUnwrap(_SFECKeyPair(randomKeyPairWith: keySpecifier))
            let signingKey = try XCTUnwrap(_SFECKeyPair(randomKeyPairWith: keySpecifier))

            let permanentInfo = try TPPeerPermanentInfo(machineID: machineID, modelID: "AudioAccessory1,1",
                                                        epoch: 1,
                                                        signing: signingKey,
                                                        encryptionKeyPair: encryptionKey,
                                                        creationTime: 4711,
                                                        peerIDHashAlgo: .SHA256)

            let stableInfo = try TPPeerStableInfo(clock: 1,
                                                  frozenPolicyVersion: frozenPolicyVersion,
                                                  flexiblePolicyVersion: prevailingPolicyVersion,
                                                  policySecrets: [:],
                                                  syncUserControllableViews: .FOLLOWING,
                                                  secureElementIdentity: nil,
                                                  walrusSetting: nil,
                                                  webAccess: nil, deviceName: machineID,
                                                  serialNumber: machineID,
                                                  osVersion: "fakeOS",
                                                  signing: signingKey,
                                                  recoverySigningPubKey: everyPeerHasRecoveryKey ? try XCTUnwrap(recoveryKeys.peerKeys.publicSigningKey).encodeSubjectPublicKeyInfo() : nil,
                                                  recoveryEncryptionPubKey: everyPeerHasRecoveryKey ? try XCTUnwrap(recoveryKeys.peerKeys.publicEncryptionKey).encodeSubjectPublicKeyInfo() : nil,
                                                  isInheritedAccount: false)

            return (machineID, signingKey, permanentInfo, stableInfo)
        }

        // Everyone might preapprove the first homepod!
        let firstHomePodPreapproval = TPHashBuilder.hash(with: .SHA256, of: homepodPeers[0].1.publicKey().spki())

        let peerIDs: [String] = [establishPeerID] + homepodPeers.map { _, _, permanentInfo, _ in permanentInfo.peerID }

        homepodPeers.forEach { _, signingKey, permanentInfo, stableInfo in
            let joinExpectation = self.expectation(description: "join callback occurs")

            self.tphClient.vouch(with: try! XCTUnwrap(establishContext.activeAccount),
                                     peerID: permanentInfo.peerID,
                                     permanentInfo: permanentInfo.data,
                                     permanentInfoSig: permanentInfo.sig,
                                     stableInfo: stableInfo.data,
                                     stableInfoSig: stableInfo.sig,
                                     ckksKeys: [],
                                     flowID: nil,
                                     deviceSessionID: nil,
                                     canSendMetrics: false) { voucher, voucherSig, error in
                XCTAssertNil(error, "Should be no error vouching")
                XCTAssertNotNil(voucher, "Should have a voucher")
                XCTAssertNotNil(voucherSig, "Should have a voucher signature")

                let voucher = try! XCTUnwrap(voucher)
                let voucherSig = try! XCTUnwrap(voucherSig)

                let dynamicInfo = try! TPPeerDynamicInfo(clock: 1,
                                                         includedPeerIDs: Set(peerIDs),
                                                         excludedPeerIDs: Set([]),
                                                         dispositions: [:],
                                                         preapprovals: everyHomepodPeerPreapprovesFirstHomePod ? Set([firstHomePodPreapproval]) : Set(),
                                                         signing: signingKey)

                let request = JoinWithVoucherRequest.with {
                    $0.changeToken = self.fakeCuttlefishServer.currentChangeToken
                    $0.peer = Peer.with {
                        $0.peerID = permanentInfo.peerID
                        $0.permanentInfoAndSig = SignedPeerPermanentInfo.with {
                            $0.peerPermanentInfo = permanentInfo.data
                            $0.sig = permanentInfo.sig
                        }
                        $0.stableInfoAndSig = SignedPeerStableInfo.with {
                            $0.peerStableInfo = stableInfo.data
                            $0.sig = stableInfo.sig
                        }
                        $0.dynamicInfoAndSig = SignedPeerDynamicInfo.with {
                            $0.peerDynamicInfo = dynamicInfo.data
                            $0.sig = dynamicInfo.sig
                        }
                        $0.vouchers = [SignedVoucher.with {
                            $0.voucher = voucher
                            $0.sig = voucherSig
                        }, ]
                    }
                }

                self.fakeCuttlefishServer.joinWithVoucher(request) { response in
                    switch response {
                    case .success:
                        break
                    case .failure(let error):
                        XCTFail("JoinWithVoucher failed: \(String(describing: error))")
                    }
                    joinExpectation.fulfill()
                }
            }

            self.wait(for: [joinExpectation], timeout: 1000)
        }

        do {
            TPStartTrackingCheckSigCount()
            TPStartTrackingSignatureGenerationCount()
            TPStartTrackingHMACCount()

            let startDate = Date()
            self.sendContainerChangeWaitForFetch(context: establishContext)

            self.assertEnters(context: establishContext, state: OctagonStateReady, within: 100 * NSEC_PER_SEC)
            self.assertConsidersSelfTrusted(context: establishContext)

            let finishDate = Date()
            self.logger.info("PERF-MEASUREMENT: Updating trust to trust newly-joined homepods took \(finishDate.timeIntervalSince(startDate)) seconds, with \(TPCheckSigCount()) signature checks, \(TPCheckSignatureGenerationCount()) signature generations, \(TPCheckHMACCount()) HMAC checks")

            TPStopTrackingCheckSigCount()
            TPStopTrackingSignatureGenerationCount()
            TPStopTrackingHMACCount()
        }

        homepodPeers.forEach { _, _, permanentInfo, _ in
            XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: establishPeerID, opinion: .trusts, target: permanentInfo.peerID)),
                          "establish peer should trust homepod \(permanentInfo.peerID)")
        }

        SecCKKSSetTestSkipTLKShareHealing(true)

        do {
            TPStartTrackingCheckSigCount()
            TPStartTrackingSignatureGenerationCount()
            TPStartTrackingHMACCount()

            let startDate = Date()
            self.assertJoinViaProximitySetup(joiningContext: join1, sponsor: establishContext)
            let finishDate = Date()

            self.logger.info("PERF-MEASUREMENT: Joining join1 via proximity took \(finishDate.timeIntervalSince(startDate)) seconds, with \(TPCheckSigCount()) signature checks, \(TPCheckSignatureGenerationCount()) signature generations, \(TPCheckHMACCount()) HMAC checks")

            TPStopTrackingCheckSigCount()
            TPStopTrackingSignatureGenerationCount()
            TPStopTrackingHMACCount()

            self.assertEnters(context: join1, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
            self.assertConsidersSelfTrusted(context: join1)
        }

        self.verifyDatabaseMocks()

        // Now kick out the majority of the HomePod peers
        homePodMIDsToRemove.forEach {
            self.mockAuthKit.otherDevices.remove($0)
        }
        self.mockAuthKit.excludeDevices.addObjects(from: homePodMIDsToRemove)

        homePodMIDsToRemove.forEach {
            self.mockAuthKit2.otherDevices.remove($0)
        }
        self.mockAuthKit2.excludeDevices.addObjects(from: homePodMIDsToRemove)

        homePodMIDsToRemove.forEach {
            self.mockAuthKit3.otherDevices.remove($0)
        }
        self.mockAuthKit3.excludeDevices.addObjects(from: homePodMIDsToRemove)

        do {
            let updateTrustExpectation = self.expectation(description: "updateTrust")
            self.fakeCuttlefishServer.updateListener = { _ in
                self.fakeCuttlefishServer.updateListener = nil
                updateTrustExpectation.fulfill()
                return nil
            }

            TPStartTrackingCheckSigCount()
            TPStartTrackingSignatureGenerationCount()
            TPStartTrackingHMACCount()

            let startDate = Date()
            self.mockAuthKit.sendIncompleteNotification()

            self.wait(for: [updateTrustExpectation], timeout: 100)
            self.assertEnters(context: establishContext, state: OctagonStateReady, within: 100 * NSEC_PER_SEC)

            let finishDate = Date()

            self.logger.info("PERF-MEASUREMENT: Updating trust (to remove homepods due to MID list) took \(finishDate.timeIntervalSince(startDate)) seconds, with \(TPCheckSigCount()) signature checks, \(TPCheckSignatureGenerationCount()) signature generations, \(TPCheckHMACCount()) HMAC checks")

            TPStopTrackingCheckSigCount()
            TPStopTrackingSignatureGenerationCount()
            TPStopTrackingHMACCount()
        }

        do {
            TPStartTrackingCheckSigCount()
            TPStartTrackingSignatureGenerationCount()
            TPStartTrackingHMACCount()

            let startDate = Date()
            self.assertJoinViaProximitySetup(joiningContext: join2, sponsor: establishContext)
            let finishDate = Date()

            self.logger.info("PERF-MEASUREMENT: Joining join2 via proximity took \(finishDate.timeIntervalSince(startDate)) seconds, with \(TPCheckSigCount()) signature checks, \(TPCheckSignatureGenerationCount()) signature generations, \(TPCheckHMACCount()) HMAC checks")
            TPStopTrackingCheckSigCount()
            TPStopTrackingSignatureGenerationCount()
            TPStopTrackingHMACCount()

            self.assertEnters(context: join2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
            self.assertConsidersSelfTrusted(context: join2)
        }

        #endif
    }
}

#endif // OCTAGON
