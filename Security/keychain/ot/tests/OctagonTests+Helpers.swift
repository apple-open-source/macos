#if OCTAGON
import Foundation
import Security

extension SignedPeerStableInfo {
    func stableInfo() -> TPPeerStableInfo {
        let newStableInfo = TPPeerStableInfo(data: self.peerStableInfo, sig: self.sig)
        XCTAssertNotNil(newStableInfo, "should be able to make a stableInfo from protobuf")
        return newStableInfo!
    }
}

extension SignedPeerDynamicInfo {
    func dynamicInfo() -> TPPeerDynamicInfo {
        let newDynamicInfo = TPPeerDynamicInfo(data: self.peerDynamicInfo, sig: self.sig)
        XCTAssertNotNil(newDynamicInfo, "should be able to make a dynamicInfo from protobuf")
        return newDynamicInfo!
    }
}

extension EstablishRequest {
    func permanentInfo() -> TPPeerPermanentInfo {
        XCTAssertTrue(self.hasPeer, "establish request should have a peer")
        XCTAssertTrue(self.peer.hasPermanentInfoAndSig, "establish request should have a permanentInfo")
        let newPermanentInfo = TPPeerPermanentInfo(peerID: self.peer.peerID,
                                                   data: self.peer.permanentInfoAndSig.peerPermanentInfo,
                                                   sig: self.peer.permanentInfoAndSig.sig,
                                                   keyFactory: TPECPublicKeyFactory())
        XCTAssertNotNil(newPermanentInfo, "should be able to make a permanantInfo from protobuf")
        return newPermanentInfo!
    }
}

extension JoinWithVoucherRequest {
    func permanentInfo() -> TPPeerPermanentInfo {
        XCTAssertTrue(self.hasPeer, "joinWithVoucher request should have a peer")
        XCTAssertTrue(self.peer.hasPermanentInfoAndSig, "joinWithVoucher request should have a permanentInfo")
        let newPermanentInfo = TPPeerPermanentInfo(peerID: self.peer.peerID,
                                                   data: self.peer.permanentInfoAndSig.peerPermanentInfo,
                                                   sig: self.peer.permanentInfoAndSig.sig,
                                                   keyFactory: TPECPublicKeyFactory())
        XCTAssertNotNil(newPermanentInfo, "should be able to make a permanantInfo from protobuf")
        return newPermanentInfo!
    }
}

extension OctagonTestsBase {
    func simulateRestart(context: OTCuttlefishContext) -> OTCuttlefishContext {
        self.tphClient.containerMap.removeContainer(name: ContainerName(container: context.containerName, context: context.contextID))

        let newContext = try! XCTUnwrap(self.manager.restartOctagonContext(context))
        if context.contextID == OTDefaultContext {
            self.defaultCKKS = try! XCTUnwrap(newContext.ckks)
            XCTAssertNil(self.defaultCKKS.syncingPolicy, "CKKS should not have a policy after 'restart'")
        }

        newContext.startOctagonStateMachine()

        return newContext
    }

    func createOTConfigurationContextForTests(contextID: String,
                                              otControl: OTControl? = nil,
                                              altDSID: String? = nil,
                                              sbd: Any? = nil,
                                              authenticationAppleID: String? = nil,
                                              passwordEquivalentToken: String? = nil,
                                              octagonCapableRecordsExist: Bool = false,
                                              overrideForJoinAfterRestore: Bool = false,
                                              ckksControl: CKKSControl? = nil) -> OTConfigurationContext {

        let otcliqueContext = OTConfigurationContext()
        otcliqueContext.context = contextID
        otcliqueContext.otControl = otControl ?? self.otControl
        otcliqueContext.containerName = OTCKContainerName
        otcliqueContext.altDSID = altDSID
        otcliqueContext.testsEnabled = true
        otcliqueContext.sbd = sbd ?? OTMockSecureBackup(bottleID: nil, entropy: nil)
        otcliqueContext.authenticationAppleID = authenticationAppleID
        otcliqueContext.passwordEquivalentToken = passwordEquivalentToken
        otcliqueContext.octagonCapableRecordsExist = octagonCapableRecordsExist
        otcliqueContext.overrideForJoinAfterRestore = overrideForJoinAfterRestore
        otcliqueContext.ckksControl = ckksControl
        return otcliqueContext
    }

    func otcontrolArgumentsFor(context: OTCuttlefishContext) -> OTControlArguments {
        return OTControlArguments(containerName: context.containerName,
                                  contextID: context.contextID,
                                  altDSID: context.activeAccount?.altDSID)
    }

    func cliqueFor(context: OTCuttlefishContext) -> OTClique {
        return OTClique(contextData: self.createOTConfigurationContextForTests(contextID: context.contextID))
    }

    func assertFetchUserControllableViewsSyncStatus(clique: OTClique, status: Bool, file: StaticString = #file, line: UInt = #line) {
        do {
            let result = try clique.fetchUserControllableViewsSyncingEnabled()
            XCTAssertEqual(result, status, "API should report that sync status matches expectation", file: file, line: line)
        } catch {
            XCTFail("Should be no error fetching status: \(error)", file: file, line: line)
        }
    }

    func assertFetchUserControllableViewsSyncStatusAsync(clique: OTClique, status: Bool, file: StaticString = #file, line: UInt = #line) {
        let fetchExpectation = self.expectation(description: "fetch callback occurs")
        let asyncExpectation = self.expectation(description: "function is truly async")
        clique.fetchUserControllableViewsSyncingEnabledAsync { result, error in
            self.wait(for: [asyncExpectation], timeout: 10)

            XCTAssertEqual(result, status, "API should report that sync status matches expectation", file: file, line: line)
            XCTAssertNil(error, "Should be no error fetching status")
            fetchExpectation.fulfill()
        }
        asyncExpectation.fulfill()
        self.wait(for: [fetchExpectation], timeout: 10)
    }

    func assertModifyUserViews(clique: OTClique, intendedSyncStatus: Bool, file: StaticString = #file, line: UInt = #line) {
        let updateTrustExpectation = self.expectation(description: "updateTrust")
        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()
            if intendedSyncStatus {
                XCTAssertEqual(newStableInfo.syncUserControllableViews, .ENABLED, "User views should now be enabled", file: file, line: line)
            } else {
                XCTAssertEqual(newStableInfo.syncUserControllableViews, .DISABLED, "User views should now be disabled", file: file, line: line)
            }
            updateTrustExpectation.fulfill()

            return nil
        }

        let ucvStatusChangeNotificationExpectation = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: OTUserControllableViewStatusChanged))

        XCTAssertNoThrow(try clique.setUserControllableViewsSyncStatus(intendedSyncStatus), "Should be no error setting user-visible sync status", file: file, line: line)
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: intendedSyncStatus)

        self.wait(for: [updateTrustExpectation, ucvStatusChangeNotificationExpectation], timeout: 10)
        self.fakeCuttlefishServer.updateListener = nil
    }

    func assertModifyUserViewsWithNoPeerUpdate(clique: OTClique, intendedSyncStatus: Bool, finalSyncStatus: Bool? = nil, file: StaticString = #file, line: UInt = #line) {
        self.fakeCuttlefishServer.updateListener = { _ in
            XCTFail("Expected no updates during user view status modification", file: file, line: line)
            return nil
        }

        // Note that the attempt will send the notification anyway, even though there's no change
        // This should happen rarely enough that it's not a performance issue
        let ucvStatusChangeNotificationExpectation = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: OTUserControllableViewStatusChanged))

        XCTAssertNoThrow(try clique.setUserControllableViewsSyncStatus(intendedSyncStatus), "Should be no error setting user-visible sync status", file: file, line: line)
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: finalSyncStatus ?? intendedSyncStatus)

        self.wait(for: [ucvStatusChangeNotificationExpectation], timeout: 10)

        self.fakeCuttlefishServer.updateListener = nil
    }
}

#endif
