#if OCTAGON
import Foundation

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

extension OctagonTestsBase {
    func simulateRestart(context: OTCuttlefishContext) -> OTCuttlefishContext {
        self.manager.removeContext(forContainerName: context.containerName, contextID: context.contextID)

        if context.contextID == OTDefaultContext {
            self.restartCKKSViews()
        }

        let newContext = self.manager.context(forContainerName: context.containerName, contextID: context.contextID)

        if context.contextID == OTDefaultContext {
            XCTAssertNil(self.injectedManager?.policy, "CKKS should not have a policy after 'restart'")
        }

        newContext.startOctagonStateMachine()

        return newContext
    }

    func cliqueFor(context: OTCuttlefishContext) -> OTClique {
        let otcliqueContext = OTConfigurationContext()
        otcliqueContext.context = context.contextID
        otcliqueContext.altDSID = try! context.authKitAdapter.primaryiCloudAccountAltDSID()
        otcliqueContext.otControl = self.otControl

        return OTClique(contextData: otcliqueContext)
    }

    func assertFetchUserControllableViewsSyncStatus(clique: OTClique, status: Bool) {
        do {
            let result = try clique.fetchUserControllableViewsSyncingEnabled()
            XCTAssertEqual(result, status, "API should report that sync status matches expectation")
        } catch {
            XCTFail("Should be no error fetching status: \(error)")
        }
    }

    func assertModifyUserViews(clique: OTClique, intendedSyncStatus: Bool) {
        let updateTrustExpectation = self.expectation(description: "updateTrust")
        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()
            if intendedSyncStatus {
                XCTAssertEqual(newStableInfo.syncUserControllableViews, .ENABLED, "User views should now be enabled")
            } else {
                XCTAssertEqual(newStableInfo.syncUserControllableViews, .DISABLED, "User views should now be disabled")
            }
            updateTrustExpectation.fulfill()

            return nil
        }

        XCTAssertNoThrow(try clique.setUserControllableViewsSyncStatus(intendedSyncStatus), "Should be no error setting user-visible sync status")
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: intendedSyncStatus)

        self.wait(for: [updateTrustExpectation], timeout: 10)
        self.fakeCuttlefishServer.updateListener = nil
    }

    func assertModifyUserViewsWithNoPeerUpdate(clique: OTClique, intendedSyncStatus: Bool, finalSyncStatus: Bool? = nil) {
        self.fakeCuttlefishServer.updateListener = { request in
            XCTFail("Expected no updates during user view status modification")
            return nil
        }

        XCTAssertNoThrow(try clique.setUserControllableViewsSyncStatus(intendedSyncStatus), "Should be no error setting user-visible sync status")
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: finalSyncStatus ?? intendedSyncStatus)

        self.fakeCuttlefishServer.updateListener = nil
    }
}

#endif
