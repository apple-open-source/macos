#if OCTAGON

import Foundation
import Security_Private.SecPasswordGenerate

class OctagonForwardCompatibilityTests: OctagonTestsBase {
    func testApprovePeerWithNewPolicy() throws {
        self.startCKAccountStatusMock()
        let peer1ID = self.assertResetAndBecomeTrustedInDefaultContext()

        // Now, we'll approve a new peer with a new policy! First, make that new policy.
        let currentPolicyOptional = prevailingPolicyDoc
        XCTAssertNotNil(currentPolicyOptional, "Should have one current policy")
        let currentPolicy = currentPolicyOptional!

        let newPolicy = currentPolicy.clone(withVersionNumber: currentPolicy.version.versionNumber + 1)
        self.fakeCuttlefishServer.policyOverlay.append(newPolicy)

        let peer2ContextID = "asdf"

        let peer2User = try CKKSTestsMockAccountsAuthKitAdapter.user(forAuthkit: self.mockAuthKit2,
                                                                 containerName: OTCKContainerName,
                                                                 contextID: peer2ContextID)

        // Assist the other client here: it'll likely already have the policy built-in
        let fetchExpectation = self.expectation(description: "fetch callback occurs")
        self.tphClient.fetchPolicyDocuments(with: peer2User,
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

        self.tphClient.prepare(with: peer2User,
                               epoch: 1,
                               machineID: self.mockAuthKit2.currentMachineID,
                               bottleSalt: try XCTUnwrap(self.mockAuthKit2.primaryAltDSID()),
                               bottleID: "why-is-this-nonnil",
                               modelID: self.mockDeviceInfo.modelID(),
                               deviceName: "new-policy-peer",
                               serialNumber: "1234",
                               osVersion: "something",
                               policyVersion: newPolicy.version,
                               policySecrets: nil,
                               syncUserControllableViews: .UNKNOWN,
                               secureElementIdentity: nil,
                               setting: nil,
                               signingPrivKeyPersistentRef: nil,
                               encPrivKeyPersistentRef: nil) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, _, error in
            XCTAssertNil(error, "Should be no error preparing the second peer")
            XCTAssertNotNil(peerID, "Should have a peerID")
            peer2ID = peerID!

            XCTAssertNotNil(stableInfo, "Should have a stable info")
            XCTAssertNotNil(stableInfoSig, "Should have a stable info signature")

            let newStableInfo = TPPeerStableInfo(data: stableInfo!, sig: stableInfoSig!)
            XCTAssertNotNil(newStableInfo, "should be able to make a stableInfo info from protobuf")

            XCTAssertEqual(newStableInfo?.frozenPolicyVersion, frozenPolicyVersion, "Frozen policy version in new identity should match frozen policy version")
            XCTAssertEqual(newStableInfo?.flexiblePolicyVersion, newPolicy.version, "Flexible policy version in new identity should match new policy version")

            self.tphClient.vouch(with: try! XCTUnwrap(self.cuttlefishContext.activeAccount),
                                 peerID: peerID!,
                                 permanentInfo: permanentInfo!,
                                 permanentInfoSig: permanentInfoSig!,
                                 stableInfo: stableInfo!,
                                 stableInfoSig: stableInfoSig!,
                                 ckksKeys: [],
                                 flowID: nil,
                                 deviceSessionID: nil,
                                 canSendMetrics: false) { voucher, voucherSig, error in
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

                self.tphClient.join(with: peer2User,
                                    voucherData: voucher!,
                                    voucherSig: voucherSig!,
                                    ckksKeys: [],
                                    tlkShares: [],
                                    preapprovedKeys: [],
                                    flowID: nil,
                                    deviceSessionID: nil,
                                    canSendMetrics: false) { peerID, _, _, error in
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

        self.assertAllCKKSViewsUpload(tlkShares: 1)

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.wait(for: [updateTrustExpectation], timeout: 10)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        XCTAssertEqual(try XCTUnwrap(self.defaultCKKS.syncingPolicy).version.versionNumber, newPolicy.version.versionNumber, "CKKS policy should be new policy version")

        // Cause a TPH+securityd restart, and ensure that the new policy can be gotten from TPH
        self.tphClient.containerMap.removeAllContainers()
        self.cuttlefishContext = self.simulateRestart(context: self.cuttlefishContext)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(try XCTUnwrap(self.defaultCKKS.syncingPolicy).version.versionNumber, newPolicy.version.versionNumber, "After restart, CKKS policy should be new policy version")

        let refetchPolicyExpectation = self.expectation(description: "refetchPolicy callback occurs")
        self.cuttlefishContext.rpcRefetchCKKSPolicy { error in
            XCTAssertNil(error, "Should be no error refetching the policy")
            refetchPolicyExpectation.fulfill()
        }
        self.wait(for: [refetchPolicyExpectation], timeout: 10)

        XCTAssertEqual(try XCTUnwrap(self.defaultCKKS.syncingPolicy).version.versionNumber, newPolicy.version.versionNumber, "After refetch, CKKS policy should be new policy version")

        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.cuttlefishContext.cuttlefishXPCWrapper.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, error in
            XCTAssertNil(error, "Should be no error getting dump")
            XCTAssertNotNil(dump, "dump should not be nil")
            let policies = dump!["registeredPolicyVersions"] as? [String]
            XCTAssertNotNil(policies, "policies should not be nil")

            XCTAssertTrue(policies!.contains("\(newPolicy.version.versionNumber), \(newPolicy.version.policyHash)"), "Registered policies should include newPolicy")

            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        let statusExpectation = self.expectation(description: "status callback occurs")
        self.cuttlefishContext.rpcStatus { result, error in
            XCTAssertNil(error, "Should have no error getting status")
            XCTAssertNotNil(result, "Should have some staatus")
            statusExpectation.fulfill()
        }
        self.wait(for: [statusExpectation], timeout: 10)
    }

    func testApprovePeerWithRedactedModelPolicy() throws {
        self.startCKAccountStatusMock()
        let peer1ID = self.assertResetAndBecomeTrustedInDefaultContext()

        // Now, we'll approve a new peer with a new policy! First, make that new policy.
        let currentPolicy = try XCTUnwrap(prevailingPolicyDoc, "Must have a current policy")

        let policyCrypto = PolicyRedactionCrypter()
        let redactionSecretKey = policyCrypto.randomKey()

        // The new iCycle is a completely new category of device
        let redaction: TPPBPolicyRedaction = try TPPolicyDocument.redaction(with: policyCrypto,
                                                                             redactionName: "r1",
                                                                             encryptionKey: redactionSecretKey,
                                                                             modelToCategory: [
                                                                                TPCategoryRule(prefix: "iCycle", category: "icycle"),
                                                                             ],
                                                                             categoriesByView: [ "LimitedPeersAllowed": ["full", "watch", "tv", "audio", "icycle"]],
                                                                             introducersByCategory: [
                                                                                "audio": ["full", "watch", "audio", "icycle"],
                                                                                "full": ["full", "watch", "icycle"],
                                                                                "tv": ["full", "watch", "tv", "icycle"],
                                                                                "watch": ["full", "watch", "icycle"],
                                                                                "windows": ["full", "watch", "icycle"],
                                                                                "icycle": ["full", "watch", "audio", "tv", "windows"],
                                                                             ],
                                                                             keyViewMapping: [])

        let newPolicyWithRedaction = currentPolicy.clone(withVersionNumber: currentPolicy.version.versionNumber + 1,
                                                         prependingCategoriesByView: nil,
                                                         prependingKeyViewMapping: nil,
                                                         prepending: [redaction])

        self.fakeCuttlefishServer.policyOverlay.append(newPolicyWithRedaction)

        let peer2ContextID = "asdf"
        let peer2User = try CKKSTestsMockAccountsAuthKitAdapter.user(forAuthkit: self.mockAuthKit2,
                                                                     containerName: OTCKContainerName,
                                                                     contextID: peer2ContextID)

        // Assist the other client here: it'll likely already have the policy built-in
        let fetchExpectation = self.expectation(description: "fetch callback occurs")
        self.tphClient.fetchPolicyDocuments(with: peer2User,
                                            versions: Set([newPolicyWithRedaction.version])) { _, error in
            XCTAssertNil(error, "Should have no error")
            fetchExpectation.fulfill()
        }
        self.wait(for: [fetchExpectation], timeout: 10)

        var peer2ID: String = "not initialized"

        let prepareExpectation = self.expectation(description: "prepare callback occurs")
        let failedVouchExpectation = self.expectation(description: "failing vouch callback occurs")
        let vouchExpectation = self.expectation(description: "vouch callback occurs")
        let joinExpectation = self.expectation(description: "join callback occurs")
        let serverJoinExpectation = self.expectation(description: "joinWithVoucher is called")

        // Do everything manually to inject the policySecrets to the prepare() call
        self.tphClient.prepare(with: peer2User,
                               epoch: 1,
                               machineID: self.mockAuthKit2.currentMachineID,
                               bottleSalt: try XCTUnwrap(self.mockAuthKit2.primaryAltDSID()),
                               bottleID: "why-is-this-nonnil",
                               modelID: "iCycle2,1",
                               deviceName: "new-policy-peer",
                               serialNumber: "1234",
                               osVersion: "something",
                               policyVersion: newPolicyWithRedaction.version,
                               policySecrets: ["r1": redactionSecretKey],
                               syncUserControllableViews: .UNKNOWN,
                               secureElementIdentity: nil,
                               setting: nil,
                               signingPrivKeyPersistentRef: nil,
                               encPrivKeyPersistentRef: nil) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, _, error in
            XCTAssertNil(error, "Should be no error preparing the second peer")
            XCTAssertNotNil(peerID, "Should have a peerID")
            peer2ID = peerID!

            XCTAssertNotNil(stableInfo, "Should have a stable info")
            XCTAssertNotNil(stableInfoSig, "Should have a stable info signature")

            let newStableInfo = TPPeerStableInfo(data: stableInfo!, sig: stableInfoSig!)
            XCTAssertNotNil(newStableInfo, "should be able to make a stableInfo info from protobuf")

            XCTAssertEqual(newStableInfo?.frozenPolicyVersion, frozenPolicyVersion, "Frozen policy version in new identity should match frozen policy version")
            XCTAssertEqual(newStableInfo?.flexiblePolicyVersion, newPolicyWithRedaction.version, "Flexible policy version in new identity should match new policy version")

            // If the new stableInfo does not have any policy secrets, vouching should fail (with 'unknown modelID')
            do {
                let stablePbWithoutSecrets = try! XCTUnwrap(TPPBPeerStableInfo(data: stableInfo))
                stablePbWithoutSecrets.clearPolicySecrets()
                let stableSerializedPbWithoutSecrets = try! XCTUnwrap(stablePbWithoutSecrets.data)

                let peer2Keys = try! loadEgoKeysSync(peerID: try! XCTUnwrap(peerID))
                let stableInfoWithoutSecrets = try! TPPeerStableInfo(data: stableSerializedPbWithoutSecrets, signing: peer2Keys.signingKey)

                self.tphClient.vouch(with: try! XCTUnwrap(self.cuttlefishContext.activeAccount),
                                     peerID: peerID!,
                                     permanentInfo: permanentInfo!,
                                     permanentInfoSig: permanentInfoSig!,
                                     stableInfo: stableInfoWithoutSecrets.data,
                                     stableInfoSig: stableInfoWithoutSecrets.sig,
                                     ckksKeys: [],
                                     flowID: nil,
                                     deviceSessionID: nil,
                                     canSendMetrics: false) { voucher, _, error in
                    XCTAssertNil(voucher, "Should have no voucher when failing to accept a new modelID without redactionSecrets")
                    XCTAssertNotNil(error, "should have some error when trying to use a new modelID without redaction secrets")
                    if let error = error as? NSError {
                        XCTAssertEqual(error.domain, TPErrorDomain, "error domain should be TPErrorDomain")
                        XCTAssertEqual(error.code, Int(TPError.modelNotFound.rawValue), "error code should be TPErrorModelNotFound")
                    } else {
                        XCTFail("Error is unexpectedly not an NSError")
                    }
                    failedVouchExpectation.fulfill()
                }
            }

            // This vouch should succeed because of the new secrets in the stableinfo
            self.tphClient.vouch(with: try! XCTUnwrap(self.cuttlefishContext.activeAccount),
                                 peerID: peerID!,
                                 permanentInfo: permanentInfo!,
                                 permanentInfoSig: permanentInfoSig!,
                                 stableInfo: stableInfo!,
                                 stableInfoSig: stableInfoSig!,
                                 ckksKeys: [],
                                 flowID: nil,
                                 deviceSessionID: nil,
                                 canSendMetrics: false) { voucher, voucherSig, error in
                XCTAssertNil(error, "Should be no error vouching")
                XCTAssertNotNil(voucher, "Should have a voucher")
                XCTAssertNotNil(voucherSig, "Should have a voucher signature")

                self.fakeCuttlefishServer.joinListener = { joinRequest in
                    XCTAssertEqual(peer2ID, joinRequest.peer.peerID, "joinWithVoucher request should be for peer 2")
                    XCTAssertTrue(joinRequest.peer.hasPermanentInfoAndSig, "Joining peer should have a permanent info")
                    let newPermanentInfo = joinRequest.permanentInfo()
                    XCTAssertEqual(newPermanentInfo.modelID, "iCycle2,1", "joining peer should be an iCycle")

                    XCTAssertTrue(joinRequest.peer.hasStableInfoAndSig, "Joining peer should have a stable info")
                    let newStableInfo = joinRequest.peer.stableInfoAndSig.stableInfo()

                    XCTAssertEqual(newStableInfo.frozenPolicyVersion, frozenPolicyVersion, "Frozen policy version in new identity should match frozen policy version")
                    XCTAssertEqual(newStableInfo.flexiblePolicyVersion, newPolicyWithRedaction.version, "Flexible policy version in new identity should match new policy version (as provided by new peer)")

                    serverJoinExpectation.fulfill()
                    return nil
                }

                self.tphClient.join(with: peer2User,
                                    voucherData: voucher!,
                                    voucherSig: voucherSig!,
                                    ckksKeys: [],
                                    tlkShares: [],
                                    preapprovedKeys: [],
                                    flowID: nil,
                                    deviceSessionID: nil,
                                    canSendMetrics: false) { peerID, _, _, error in
                    XCTAssertNil(error, "Should be no error joining")
                    XCTAssertNotNil(peerID, "Should have a peerID")
                    joinExpectation.fulfill()
                }
                vouchExpectation.fulfill()
            }
            prepareExpectation.fulfill()
        }
        self.wait(for: [prepareExpectation, vouchExpectation, joinExpectation, serverJoinExpectation, failedVouchExpectation], timeout: 10)

        // Then, after the remote peer joins, the original peer should realize it trusts the new peer and update its own stableinfo to use the new policy
        let updateTrustExpectation = self.expectation(description: "updateTrust")
        self.fakeCuttlefishServer.updateListener = { request in
            XCTAssertEqual(peer1ID, request.peerID, "updateTrust request should be for peer 1")
            let newDynamicInfo = request.dynamicInfoAndSig.dynamicInfo()
            XCTAssert(newDynamicInfo.includedPeerIDs.contains(peer2ID), "Peer1 should trust peer2")

            let newStableInfo = request.stableInfoAndSig.stableInfo()

            XCTAssertEqual(newStableInfo.frozenPolicyVersion, frozenPolicyVersion, "Policy version in peer should match frozen policy version")
            XCTAssertEqual(newStableInfo.flexiblePolicyVersion, newPolicyWithRedaction.version, "Prevailing policy version in peer should match new policy version")

            updateTrustExpectation.fulfill()
            return nil
        }

        self.assertAllCKKSViewsUpload(tlkShares: 1) { $0.zoneName == "LimitedPeersAllowed" }

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.wait(for: [updateTrustExpectation], timeout: 10)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func testRejectVouchingForPeerWithUnknownNewPolicy() throws {
        self.startCKAccountStatusMock()
        _ = self.assertResetAndBecomeTrustedInDefaultContext()

        // Now, a new peer joins with a policy we can't fetch
        let currentPolicyOptional = prevailingPolicyDoc
        XCTAssertNotNil(currentPolicyOptional, "Should have one current policy")
        let currentPolicy = currentPolicyOptional!

        let newPolicy = currentPolicy.clone(withVersionNumber: currentPolicy.version.versionNumber + 1)

        let peer2ContextID = "asdf"
        let peer2User = try CKKSTestsMockAccountsAuthKitAdapter.user(forAuthkit: self.mockAuthKit2,
                                                                 containerName: OTCKContainerName,
                                                                 contextID: peer2ContextID)

        // Assist the other client here: it'll likely already have this built-in
        self.fakeCuttlefishServer.policyOverlay.append(newPolicy)

        let fetchExpectation = self.expectation(description: "fetch callback occurs")
        self.tphClient.fetchPolicyDocuments(with: peer2User,
                                            versions: Set([newPolicy.version])) { _, error in
                                                XCTAssertNil(error, "Should have no error")
                                                fetchExpectation.fulfill()
        }
        self.wait(for: [fetchExpectation], timeout: 10)

        // Remove the policy, now that peer2 has it
        self.fakeCuttlefishServer.policyOverlay.removeAll()

        let prepareExpectation = self.expectation(description: "prepare callback occurs")
        let vouchExpectation = self.expectation(description: "vouch callback occurs")

        self.tphClient.prepare(with: peer2User,
                               epoch: 1,
                               machineID: self.mockAuthKit2.currentMachineID,
                               bottleSalt: try XCTUnwrap(self.mockAuthKit2.primaryAltDSID()),
                               bottleID: "why-is-this-nonnil",
                               modelID: self.mockDeviceInfo.modelID(),
                               deviceName: "new-policy-peer",
                               serialNumber: "1234",
                               osVersion: "something",
                               policyVersion: newPolicy.version,
                               policySecrets: nil,
                               syncUserControllableViews: .UNKNOWN,
                               secureElementIdentity: nil,
                               setting: nil,
                               signingPrivKeyPersistentRef: nil,
                               encPrivKeyPersistentRef: nil) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, _, error in
            XCTAssertNil(error, "Should be no error preparing the second peer")
            XCTAssertNotNil(peerID, "Should have a peerID")

            XCTAssertNotNil(stableInfo, "Should have a stable info")
            XCTAssertNotNil(stableInfoSig, "Should have a stable info signature")

            let newStableInfo = TPPeerStableInfo(data: stableInfo!, sig: stableInfoSig!)
            XCTAssertNotNil(newStableInfo, "should be able to make a stableInfo info from protobuf")

            XCTAssertEqual(newStableInfo?.frozenPolicyVersion, frozenPolicyVersion, "Frozen policy version in new identity should match frozen policy version")
            XCTAssertEqual(newStableInfo?.flexiblePolicyVersion, newPolicy.version, "Flexible policy version in new identity should match new policy version")

            self.tphClient.vouch(with: try! XCTUnwrap(self.cuttlefishContext.activeAccount),
                                 peerID: peerID!,
                                 permanentInfo: permanentInfo!,
                                 permanentInfoSig: permanentInfoSig!,
                                 stableInfo: stableInfo!,
                                 stableInfoSig: stableInfoSig!,
                                 ckksKeys: [],
                                 flowID: nil,
                                 deviceSessionID: nil,
                                 canSendMetrics: false) { voucher, voucherSig, error in
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
        let currentPolicyOptional = prevailingPolicyDoc
        XCTAssertNotNil(currentPolicyOptional, "Should have one current policy")
        let currentPolicy = currentPolicyOptional!

        let newPolicy = currentPolicy.clone(withVersionNumber: currentPolicy.version.versionNumber + 1)

        let peer2ContextID = "asdf"
        let peer2User = try CKKSTestsMockAccountsAuthKitAdapter.user(forAuthkit: self.mockAuthKit2,
                                                                 containerName: OTCKContainerName,
                                                                 contextID: peer2ContextID)

        // Assist the other client here: it'll likely already have this built-in
        self.fakeCuttlefishServer.policyOverlay.append(newPolicy)

        let fetchExpectation = self.expectation(description: "fetch callback occurs")
        self.tphClient.fetchPolicyDocuments(with: peer2User,
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
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
    }

    func createOctagonAndCKKSUsingFuturePolicy() throws -> (TPPolicyDocument, CKRecordZone.ID) {
        // We want to set up a world with a peer, in Octagon, with TLKs for zones that don't even exist in our current policy.
        // First, make a new policy.
        let currentPolicyOptional = prevailingPolicyDoc
        XCTAssertNotNil(currentPolicyOptional, "Should have one current policy")
        let currentPolicyDocument = currentPolicyOptional!

        let futureViewName = "FutureView"
        let futureViewMapping = TPPBPolicyKeyViewMapping()!

        let futureViewZoneID = CKRecordZone.ID(zoneName: futureViewName)
        self.ckksZones.add(futureViewZoneID)
        self.defaultCKKS.setSyncingViewsAllowList(Set((self.intendedCKKSZones.union([futureViewZoneID])).map { $0.zoneName }))

        self.zones![futureViewZoneID] = FakeCKZone(zone: futureViewZoneID)

        XCTAssertFalse(currentPolicyDocument.keyViewMapping.contains { $0.view == futureViewName }, "Current policy should not include future view")

        let newPolicyDocument = currentPolicyDocument.clone(withVersionNumber: currentPolicyDocument.version.versionNumber + 1,
                                                            prependingCategoriesByView: [futureViewName: Set(["watch", "full", "tv", "audio"])],
                                                            prependingKeyViewMapping: [futureViewMapping])

        self.fakeCuttlefishServer.policyOverlay.append(newPolicyDocument)

        return (newPolicyDocument, futureViewZoneID)
    }

    func createFuturePolicyMovingAllItemsToLimitedPeers() throws -> (TPPolicyDocument) {
        let currentPolicyOptional = prevailingPolicyDoc
        XCTAssertNotNil(currentPolicyOptional, "Should have one current policy")
        let currentPolicyDocument = currentPolicyOptional!

        let limitedPeersViewMapping = TPPBPolicyKeyViewMapping(view: "LimitedPeersAllowed",
                                                               matchingRule: TPDictionaryMatchingRule.trueMatch())

        let newPolicyDocument = currentPolicyDocument.clone(withVersionNumber: currentPolicyDocument.version.versionNumber + 1,
                                                            prependingCategoriesByView: nil,
                                                            prependingKeyViewMapping: [limitedPeersViewMapping])

        self.fakeCuttlefishServer.policyOverlay.append(newPolicyDocument)

        return newPolicyDocument
    }

    func testRestoreBottledPeerUsingFuturePolicy() throws {
        let (newPolicyDocument, futureViewZoneID) = try self.createOctagonAndCKKSUsingFuturePolicy()

        let futurePeerContext = self.makeInitiatorContext(contextID: "futurePeer")
        futurePeerContext.policyOverride = newPolicyDocument.version

        self.startCKAccountStatusMock()
        let futurePeerID = self.assertResetAndBecomeTrusted(context: futurePeerContext)

        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: futurePeerContext)
        XCTAssertTrue(self.tlkShareInCloudKit(receiverPeerID: futurePeerID, senderPeerID: futurePeerID, zoneID: futureViewZoneID))

        // Now, our peer (with no inbuilt knowledge of newPolicyDocument) joins via escrow recovery.

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
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.version, newPolicyDocument.version, "CKKS should be configured with new policy")
        self.assertTLKSharesInCloudKit(receiver: self.cuttlefishContext, sender: self.cuttlefishContext)
        self.verifyDatabaseMocks()

        self.wait(for: [serverJoinExpectation], timeout: 10)

        // And the joined peer should have recovered the TLK, and uploaded itself a share
        XCTAssertTrue(self.tlkShareInCloudKit(receiverPeerID: peerID, senderPeerID: peerID, zoneID: futureViewZoneID))
    }

    func testPairingJoinUsingFuturePolicy() throws {
        let (newPolicyDocument, futureViewZoneID) = try self.createOctagonAndCKKSUsingFuturePolicy()

        let futurePeerContext = self.makeInitiatorContext(contextID: "futurePeer")
        futurePeerContext.policyOverride = newPolicyDocument.version

        self.startCKAccountStatusMock()
        let futurePeerID = self.assertResetAndBecomeTrusted(context: futurePeerContext)

        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: futurePeerContext)
        XCTAssertTrue(self.tlkShareInCloudKit(receiverPeerID: futurePeerID, senderPeerID: futurePeerID, zoneID: futureViewZoneID))

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
        self.defaultCKKS.zoneChangeFetcher.notifyZoneChange(nil)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.version, newPolicyDocument.version, "CKKS should be configured with new policy")
        self.verifyDatabaseMocks()

        self.wait(for: [serverJoinExpectation], timeout: 10)

        // And the joined peer should have recovered the TLK, and uploaded itself a share
        XCTAssertTrue(self.tlkShareInCloudKit(receiverPeerID: peerID, senderPeerID: peerID, zoneID: futureViewZoneID))
    }

    func testRecoveryKeyJoinUsingFuturePolicy() throws {
    #if os(tvOS)
        throw XCTSkip("do not run test on tvOS platforms (appleTV, homepod)")
    #else
        let (newPolicyDocument, futureViewZoneID) = try self.createOctagonAndCKKSUsingFuturePolicy()

        let futurePeerContext = self.makeInitiatorContext(contextID: "futurePeer")
        futurePeerContext.policyOverride = newPolicyDocument.version

        self.startCKAccountStatusMock()
        let futurePeerID = self.assertResetAndBecomeTrusted(context: futurePeerContext)

        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: futurePeerContext)
        XCTAssertTrue(self.tlkShareInCloudKit(receiverPeerID: futurePeerID, senderPeerID: futurePeerID, zoneID: futureViewZoneID))

        // Create the recovery key
        let recoveryKey = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        OctagonSetSOSFeatureEnabled(true)

        let createRecoveryExpectation = self.expectation(description: "createRecoveryExpectation returns")
        self.manager.createRecoveryKey(self.otcontrolArgumentsFor(context: futurePeerContext), recoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            createRecoveryExpectation.fulfill()
        }
        self.wait(for: [createRecoveryExpectation], timeout: 10)

        // Setting the RK will make TLKShares to the RK, so help that out too
        try self.putRecoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))

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

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let joinWithRecoveryKeyExpectation = self.expectation(description: "joinWithRecoveryKey callback occurs")
        self.cuttlefishContext.join(withRecoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithRecoveryKeyExpectation], timeout: 20)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.version, newPolicyDocument.version, "CKKS should be configured with new policy")
        self.assertTLKSharesInCloudKit(receiver: self.cuttlefishContext, sender: self.cuttlefishContext)

        // And double-check that the future view is covered
        let accountMetadata = try self.cuttlefishContext.accountMetadataStore.loadOrCreateAccountMetadata()
        XCTAssertTrue(self.tlkShareInCloudKit(receiverPeerID: accountMetadata.peerID, senderPeerID: accountMetadata.peerID, zoneID: futureViewZoneID))
        self.verifyDatabaseMocks()

        self.wait(for: [serverJoinExpectation], timeout: 10)
    #endif
    }

    func testPreapprovedJoinUsingFuturePolicy() throws {
        let peer2mockSOS = CKKSMockSOSPresentAdapter(selfPeer: self.createSOSPeer(peerID: "peer2ID"), trustedPeers: self.mockSOSAdapter!.allPeers(), essential: false)
        print(peer2mockSOS.allPeers())
        self.mockSOSAdapter!.trustedPeers.add(peer2mockSOS.selfPeer)

        let futurePeerContext = self.manager.context(forContainerName: OTCKContainerName,
                                                     contextID: "futurePeer",
                                                     sosAdapter: peer2mockSOS,
                                                     accountsAdapter: self.mockAuthKit2,
                                                     authKitAdapter: self.mockAuthKit2,
                                                     tooManyPeersAdapter: self.mockTooManyPeers,
                                                     tapToRadarAdapter: self.mockTapToRadar,
                                                     lockStateTracker: self.lockStateTracker,
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

        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCInCircle)
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.wait(for: [serverJoinExpectation], timeout: 10)

        // But, since we're not mocking the remote peer sharing the TLKs, ckks should get stuck
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLK, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.version, newPolicyDocument.version, "CKKS should be configured with new policy")
        self.verifyDatabaseMocks()
    }

    func testRespondToFuturePoliciesInPeerUpdates() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        // Now, another peer comes along and joins via BP recovery, using a new policy
        let (newPolicyDocument, futureZoneID) = try self.createOctagonAndCKKSUsingFuturePolicy()

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

        // And the other peer creates the new View, and shares it with us
        self.putFakeKeyHierarchiesInCloudKit { zoneID in
            return zoneID.zoneName == futureZoneID.zoneName
        }

        // The remote peer uploads tlkshares for itself, because it received them during its escrow recovery
        try self.putSelfTLKSharesInCloudKit(context: futurePeerContext) // , filter)
        try self.putTLKShareInCloudKit(to: self.cuttlefishContext, from: futurePeerContext, zoneID: futureZoneID)

        // First, tell all existing CKKS views to fetch, and wait for it to do so. This will ensure that it receives the tlkshares uploaded above, and
        // won't immediately upload new ones when the peer list changes.
        self.silentFetchesAllowed = false
        self.expectCKFetch()
        self.defaultCKKS.zoneChangeFetcher.notifyZoneChange(nil)
        self.verifyDatabaseMocks()
        self.silentFetchesAllowed = true

        // Now, tell our first peer about the new changes. It should trust the new peer, and update its policy
        // It should also upload itself a TLKShare for the future view
        self.assertAllCKKSViewsUpload(tlkShares: 1) { $0.zoneName == futureZoneID.zoneName }

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

        // Once we've uploaded the TLKShare for the future view, then we're fairly sure the view object has been created locally.
        self.verifyDatabaseMocks()
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testRecoverFromPeerUsingOldPolicy() throws {
        self.startCKAccountStatusMock()

        let pastPeerContext = self.makeInitiatorContext(contextID: "pastPeer")

        let policyV6Document = builtInPolicyDocumentsFilteredByVersion { $0 == 6 }.first!()
        pastPeerContext.policyOverride = policyV6Document.version

        let serverEstablishExpectation = self.expectation(description: "futurePeer establishes successfully")
        self.fakeCuttlefishServer.establishListener = { establishRequest in
            XCTAssertTrue(establishRequest.peer.hasStableInfoAndSig, "Establishing peer should have a stable info")
            let newStableInfo = establishRequest.peer.stableInfoAndSig.stableInfo()

            XCTAssertEqual(newStableInfo.frozenPolicyVersion, frozenPolicyVersion, "Frozen policy version in peer should be frozen version")
            XCTAssertEqual(newStableInfo.flexiblePolicyVersion, policyV6Document.version, "Prevailing policy version in peer should be v6")
            serverEstablishExpectation.fulfill()
            return nil
        }

        self.assertResetAndBecomeTrusted(context: pastPeerContext)
        self.wait(for: [serverEstablishExpectation], timeout: 10)

        self.putFakeKeyHierarchiesInCloudKit { zoneID in policyV6Document.keyViewMapping.contains { $0.view == zoneID.zoneName } }
        try self.putSelfTLKSharesInCloudKit(context: pastPeerContext) { zoneID in policyV6Document.keyViewMapping.contains { $0.view == zoneID.zoneName } }

        // Ensure that CKKS will bring up the Backstop view
        self.defaultCKKS.setSyncingViewsAllowList(Set(["Backstop"] + self.intendedCKKSZones!.map { $0.zoneName }))

        // Now, Octagon comes along and recovers the bottle.

        // Right now, Octagon will join and then immediately updateTrust to upload the new set of TLKs
        // This probably can be reworked for performance.
        let serverJoinExpectation = self.expectation(description: "joins successfully")
        self.fakeCuttlefishServer.joinListener = { joinRequest in
            XCTAssertEqual(joinRequest.viewKeys.count, 0, "Should have zero sets of new viewkeys during join")
            serverJoinExpectation.fulfill()
            return nil
        }

        // TVs do not participate in the backstop view, and so won't upload anything
        #if !os(tvOS)
        let serverUpdateTrustExpectation = self.expectation(description: "updateTrust successfully")
        self.fakeCuttlefishServer.updateListener = { updateRequest in
            XCTAssertEqual(updateRequest.viewKeys.count, 1, "Should have one new set of viewkeys during update")
            serverUpdateTrustExpectation.fulfill()
            return nil
        }
        #endif

        self.assertJoinViaEscrowRecovery(joiningContext: self.cuttlefishContext, sponsor: pastPeerContext)
        self.wait(for: [serverJoinExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        #if !os(tvOS)
        self.wait(for: [serverUpdateTrustExpectation], timeout: 10)
        #endif

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
        self.assertTLKSharesInCloudKit(receiver: self.cuttlefishContext, sender: self.cuttlefishContext)
    }

    func testRecoverFromPeerUsingExtremelyOldPolicy() throws {
        self.startCKAccountStatusMock()

        let pastPeerContext = self.makeInitiatorContext(contextID: "pastPeer")
        let policyV1Document = builtInPolicyDocuments.first { $0.version.versionNumber == 1 }!
        pastPeerContext.policyOverride = policyV1Document.version

        let serverEstablishExpectation = self.expectation(description: "futurePeer establishes successfully")
        self.fakeCuttlefishServer.establishListener = { establishRequest in
            XCTAssertTrue(establishRequest.peer.hasStableInfoAndSig, "Establishing peer should have a stable info")
            let newStableInfo = establishRequest.peer.stableInfoAndSig.stableInfo()

            XCTAssertNil(newStableInfo.flexiblePolicyVersion, "Peer should be from before prevailing policy version were set")
            XCTAssertEqual(newStableInfo.frozenPolicyVersion, policyV1Document.version, "Frozen policy version in peer should be v1 - a very old peer")
            serverEstablishExpectation.fulfill()
            return nil
        }

        self.assertResetAndBecomeTrusted(context: pastPeerContext)
        self.wait(for: [serverEstablishExpectation], timeout: 10)

        // The ancient peer created TLKs for PCSEscrow, which we don't want around
        let pcsEscrowZoneID = CKRecordZone.ID(zoneName: "PCSEscrow")
        self.zones![pcsEscrowZoneID] = FakeCKZone(zone: pcsEscrowZoneID)

        self.putFakeKeyHierarchy(inCloudKit: pcsEscrowZoneID)
        self.putSelfTLKShares(inCloudKit: pcsEscrowZoneID)

        XCTAssertNotNil(self.fakeCuttlefishServer.getKeys(zoneID: pcsEscrowZoneID), "Should have keys for PCSEscrow")
        XCTAssertNil(self.fakeCuttlefishServer.getKeys(zoneID: self.manateeZoneID), "Should not have keys for Manatee")

        // Ensure that CKKS will bring up the Backstop view, and allow it to bring up PCSEscrow if that's what it wants (it shouldn't)
        self.defaultCKKS.setSyncingViewsAllowList(Set(["Backstop", "PCSEscrow"] + self.intendedCKKSZones!.map { $0.zoneName }))

        // Now, Octagon comes along and recovers the bottle.

        // Since Octagon doesn't wait for CKKS to finish fetching before joining, it won't create any TLKs at join time
        // It will create views as needed later via updates.
        let serverJoinExpectation = self.expectation(description: "joins successfully")
        let serverUpdateExpectation = self.expectation(description: "updates successfully")
        self.fakeCuttlefishServer.joinListener = { joinRequest in
            let zones = Set(joinRequest.viewKeys.map { $0.view })

            XCTAssertEqual(zones.count, 0, "Should have zero sets of new viewkeys during join")
            XCTAssertFalse(zones.contains("PCSEscrow"), "Should not have a TLK for the PCSEscrow view")

            let joiningPeer = joinRequest.peer.stableInfoAndSig.stableInfo()
            XCTAssertEqual(joiningPeer.flexiblePolicyVersion, prevailingPolicyVersion, "Our current policy should be the prevailing policy - the sponsor peer should be ignored")
            XCTAssertEqual(joiningPeer.frozenPolicyVersion, frozenPolicyVersion, "Frozen policy version in peer should be the real policy")

            serverJoinExpectation.fulfill()
            return nil
        }

        self.fakeCuttlefishServer.updateListener = { updateRequest in
            let zones = Set(updateRequest.viewKeys.map { $0.view })

            #if !os(tvOS)
            XCTAssertEqual(zones.count, 3, "Should have three sets of new viewkeys during join")
            XCTAssertTrue(zones.contains("Manatee"), "Should have a TLK for the manatee view")
            #else
            XCTAssertEqual(zones.count, 1, "Should have one set of new viewkeys during join")
            #endif
            XCTAssertTrue(zones.contains("LimitedPeersAllowed"), "Should have a TLK for the LimitedPeersAllowed view")

            serverUpdateExpectation.fulfill()
            return nil
        }

        self.assertJoinViaEscrowRecovery(joiningContext: self.cuttlefishContext, sponsor: pastPeerContext)
        self.wait(for: [serverJoinExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.wait(for: [serverUpdateExpectation], timeout: 10)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
        self.assertTLKSharesInCloudKit(receiver: self.cuttlefishContext, sender: self.cuttlefishContext)
    }

    func testRecoveryKeyFromPeerUsingOldPolicy() throws {
        try self.skipOnRecoveryKeyNotSupported()

        self.startCKAccountStatusMock()

        let pastPeerContext = self.makeInitiatorContext(contextID: "pastPeer")
        let policyV18Document = builtInPolicyDocuments.first { $0.version.versionNumber == 18 }!
        pastPeerContext.policyOverride = policyV18Document.version

        let serverEstablishExpectation = self.expectation(description: "futurePeer establishes successfully")
        self.fakeCuttlefishServer.establishListener = { establishRequest in
            XCTAssertTrue(establishRequest.peer.hasStableInfoAndSig, "Establishing peer should have a stable info")
            let newStableInfo = establishRequest.peer.stableInfoAndSig.stableInfo()
            XCTAssertEqual(newStableInfo.flexiblePolicyVersion, policyV18Document.version, "Policy version in peer should be v18")
            serverEstablishExpectation.fulfill()
            return nil
        }

        self.assertResetAndBecomeTrusted(context: pastPeerContext)
        self.wait(for: [serverEstablishExpectation], timeout: 10)

        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: pastPeerContext)
        self.assertSelfTLKSharesInCloudKit(context: pastPeerContext)

        // Now, create an RK
        let recoveryKey = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        let createRecoveryExpectation = self.expectation(description: "createRecoveryExpectation returns")
        self.manager.createRecoveryKey(self.otcontrolArgumentsFor(context: pastPeerContext), recoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            createRecoveryExpectation.fulfill()
        }
        self.wait(for: [createRecoveryExpectation], timeout: 10)

        try self.putRecoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))

        // Now, Octagon comes along and recovers via the RK.
        let serverJoinExpectation = self.expectation(description: "joins successfully")
        self.fakeCuttlefishServer.joinListener = { joinRequest in
            let zones = Set(joinRequest.viewKeys.map { $0.view })

            XCTAssertEqual(zones.count, 0, "Should have zero sets of new viewkeys during join")

            let joiningPeer = joinRequest.peer.stableInfoAndSig.stableInfo()
            XCTAssertEqual(joiningPeer.flexiblePolicyVersion, prevailingPolicyVersion, "Our current policy should be the prevailing policy - the sponsor peer should be ignored")
            XCTAssertEqual(joiningPeer.frozenPolicyVersion, frozenPolicyVersion, "Frozen policy version in peer should be the real policy")

            let tlkShareZones = joinRequest.tlkShares.map { $0.view }
#if !os(tvOS)
            XCTAssertEqual(joinRequest.tlkShares.count, 2, "Should have two TLKShares during join")
            XCTAssertTrue(tlkShareZones.contains("Manatee"), "Should have a TLKShare for the manatee view")
#else
            XCTAssertEqual(joinRequest.tlkShares.count, 1, "Should have two TLKShares during join")
#endif
            XCTAssertTrue(tlkShareZones.contains("LimitedPeersAllowed"), "Should have a TLKShare for the LimitedPeersAllowed view")

            serverJoinExpectation.fulfill()
            return nil
        }

        self.cuttlefishContext.startOctagonStateMachine()

        let joinWithRecoveryKeyExpectation = self.expectation(description: "joinWithRecoveryKey callback occurs")
        self.cuttlefishContext.join(withRecoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithRecoveryKeyExpectation], timeout: 20)
        self.wait(for: [serverJoinExpectation], timeout: 10)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
        self.assertTLKSharesInCloudKit(receiver: self.cuttlefishContext, sender: self.cuttlefishContext)
    }

    func testCustodianRecoveryKeyFromPeerUsingOldPolicy() throws {
        try self.skipOnRecoveryKeyNotSupported()

        self.startCKAccountStatusMock()

        let pastPeerContext = self.makeInitiatorContext(contextID: "pastPeer")
        let policyV18Document = builtInPolicyDocuments.first { $0.version.versionNumber == 18 }!
        pastPeerContext.policyOverride = policyV18Document.version

        let serverEstablishExpectation = self.expectation(description: "futurePeer establishes successfully")
        self.fakeCuttlefishServer.establishListener = { establishRequest in
            XCTAssertTrue(establishRequest.peer.hasStableInfoAndSig, "Establishing peer should have a stable info")
            let newStableInfo = establishRequest.peer.stableInfoAndSig.stableInfo()
            XCTAssertEqual(newStableInfo.flexiblePolicyVersion, policyV18Document.version, "Policy version in peer should be v18")
            serverEstablishExpectation.fulfill()
            return nil
        }

        self.assertResetAndBecomeTrusted(context: pastPeerContext)
        self.wait(for: [serverEstablishExpectation], timeout: 10)

        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: pastPeerContext)
        self.assertSelfTLKSharesInCloudKit(context: pastPeerContext)

        // Now, create a CRK
        var retCRK: OTCustodianRecoveryKey?
        let createCustodianRecoveryKeyExpectation = self.expectation(description: "createCustodianRecoveryKey returns")
        self.manager.createCustodianRecoveryKey(self.otcontrolArgumentsFor(context: pastPeerContext), uuid: nil) { crk, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNotNil(crk, "crk should be non-nil")
            XCTAssertNotNil(crk?.uuid, "uuid should be non-nil")
            retCRK = crk
            createCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [createCustodianRecoveryKeyExpectation], timeout: 10)

        let otcrk = try XCTUnwrap(retCRK)

        self.assertEnters(context: pastPeerContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        let container = try self.tphClient.getContainer(with: try XCTUnwrap(pastPeerContext.activeAccount))
        let custodian = try XCTUnwrap(container.model.findCustodianRecoveryKey(with: otcrk.uuid))

        let crkWithKeys = try CustodianRecoveryKey(tpCustodian: custodian,
                                                   recoveryKeyString: otcrk.recoveryString,
                                                   recoverySalt: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))
        self.verifyDatabaseMocks()

        // Since non-primary Octagons don't have CKKSes yet, help this out
        self.putAllTLKSharesInCloudKit(to: crkWithKeys.peerKeys, from: try self.loadPeerKeys(context: pastPeerContext))

        // Now, Octagon comes along and recovers via the CRK.

        let serverJoinExpectation = self.expectation(description: "joins successfully")
        self.fakeCuttlefishServer.joinListener = { joinRequest in
            let zones = Set(joinRequest.viewKeys.map { $0.view })

            XCTAssertEqual(zones.count, 0, "Should have zero sets of new viewkeys during join")

            let joiningPeer = joinRequest.peer.stableInfoAndSig.stableInfo()
            XCTAssertEqual(joiningPeer.flexiblePolicyVersion, prevailingPolicyVersion, "Our current policy should be the prevailing policy - the sponsor peer should be ignored")
            XCTAssertEqual(joiningPeer.frozenPolicyVersion, frozenPolicyVersion, "Frozen policy version in peer should be the real policy")

            let tlkShareZones = joinRequest.tlkShares.map { $0.view }
#if !os(tvOS)
            XCTAssertEqual(joinRequest.tlkShares.count, 2, "Should have two TLKShares during join")
            XCTAssertTrue(tlkShareZones.contains("Manatee"), "Should have a TLKShare for the manatee view")
#else
            XCTAssertEqual(joinRequest.tlkShares.count, 1, "Should have two TLKShares during join")
#endif
            XCTAssertTrue(tlkShareZones.contains("LimitedPeersAllowed"), "Should have a TLKShare for the LimitedPeersAllowed view")

            serverJoinExpectation.fulfill()
            return nil
        }

        self.cuttlefishContext.startOctagonStateMachine()

        let joinWithCustodianRecoveryKeyExpectation = self.expectation(description: "joinWithCustodianRecoveryKey callback occurs")
        self.cuttlefishContext.join(with: otcrk) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithCustodianRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithCustodianRecoveryKeyExpectation], timeout: 20)
        self.wait(for: [serverJoinExpectation], timeout: 10)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
        self.assertTLKSharesInCloudKit(receiver: self.cuttlefishContext, sender: self.cuttlefishContext)
    }

    func testCKKSRequestPolicyCheck() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let newPolicyDocument = try self.createFuturePolicyMovingAllItemsToLimitedPeers()

        let futurePeerContext = self.makeInitiatorContext(contextID: "futurePeer")
        futurePeerContext.policyOverride = newPolicyDocument.version

        let serverJoinExpectation = self.expectation(description: "futurePeer joins successfully")
        self.fakeCuttlefishServer.joinListener = { joinRequest in
            XCTAssertTrue(joinRequest.peer.hasStableInfoAndSig, "Joining peer should have a stable info")
            let newStableInfo = joinRequest.peer.stableInfoAndSig.stableInfo()

            XCTAssertEqual(newStableInfo.frozenPolicyVersion, frozenPolicyVersion, "Policy version in peer should match frozen policy version")
            XCTAssertEqual(newStableInfo.flexiblePolicyVersion, newPolicyDocument.version, "Prevailing policy version in peer should match new policy version")

#if !os(tvOS)
            XCTAssertEqual(joinRequest.tlkShares.count, 2, "futurePeer should create TLKShares as part of joining")
#else
            XCTAssertEqual(joinRequest.tlkShares.count, 1, "futurePeer should create TLKShares as part of joining")
#endif
            serverJoinExpectation.fulfill()
            return nil
        }

        self.assertJoinViaEscrowRecovery(joiningContext: futurePeerContext, sponsor: self.cuttlefishContext)
        self.wait(for: [serverJoinExpectation], timeout: 10)

        // And the peer adds a new item to the LimitedPeersAllowed view, but one that didn't used to go there
        var item = self.fakeRecordDictionary("account0", zoneID: self.limitedPeersAllowedZoneID)
        item["vwht"] = "asdf"

        self.addItem(toCloudKitZone: item, recordName: "7B598D31-F9C5-481E-98AC-5A507ACB2D85", zoneID: self.limitedPeersAllowedZoneID)

        let limitedPeersViewState = self.defaultCKKS.viewState(name: "LimitedPeersAllowed")
        XCTAssertNotNil(limitedPeersViewState, "Should have a LimitedPeersAllowed view")

        // This CKKS notification should cause Octagon to update trust, and then fill CKKS in (which should then accept the item)

        let updateExpectation = self.expectation(description: "peer updates successfully")
        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()

            XCTAssertEqual(newStableInfo.frozenPolicyVersion, frozenPolicyVersion, "Policy version in peer should match frozen policy version")
            XCTAssertEqual(newStableInfo.flexiblePolicyVersion, newPolicyDocument.version, "Prevailing policy version in peer should match new policy version")

            updateExpectation.fulfill()
            return nil
        }

        self.defaultCKKS.zoneChangeFetcher.notifyZoneChange(nil)

        // And wait for the updateTrust to occur, then for Octagon to return to ready, then for any incoming queue processing in ckks
        self.wait(for: [updateExpectation], timeout: 10)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.defaultCKKS.waitForFetchAndIncomingQueueProcessing()
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // The item should be found
        self.findGenericPassword("account0", expecting: errSecSuccess)
    }
}

#endif
