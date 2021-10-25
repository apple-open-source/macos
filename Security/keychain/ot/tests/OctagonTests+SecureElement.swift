#if OCTAGON

import Foundation

class OctagonSecureElementTests: OctagonTestsBase {
    override func setUp() {
        super.setUp()

        self.otControlEntitlementBearer.entitlements[kSecEntitlementPrivateOctagonSecureElement] = true
    }

    func testSecureElementAPIRequiresEntitlement() throws {
        self.otControlEntitlementBearer.entitlements.removeAll()

        self.startCKAccountStatusMock()

        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        let sePeerID = "peerID".data(using: .utf8)!
        let sei = OTSecureElementPeerIdentity()!
        sei.peerIdentifier = sePeerID
        sei.peerData = "data".data(using: .utf8)!

        XCTAssertThrowsError(try cliqueBridge.setLocalSecureElementIdentity(sei)) { error in
            let nserror = error as NSError
            XCTAssertEqual(nserror.code, Int(errSecMissingEntitlement), "Error should be errSecMissingEntitlement")
        }

        XCTAssertThrowsError(try cliqueBridge.removeLocalSecureElementIdentityPeerID(sePeerID)) { error in
            let nserror = error as NSError
            XCTAssertEqual(nserror.code, Int(errSecMissingEntitlement), "Error should be errSecMissingEntitlement")
            print(error)
        }

        XCTAssertThrowsError(try cliqueBridge.fetchTrustedSecureElementIdentities()) { error in
            let nserror = error as NSError
            XCTAssertEqual(nserror.code, Int(errSecMissingEntitlement), "Error should be errSecMissingEntitlement")
            print(error)
        }
    }

    func testSetSecureElementIdentityAfterEstablish() throws {
        self.startCKAccountStatusMock()
        let originalPeerID = self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        do {
            let seIdentities = try cliqueBridge.fetchTrustedSecureElementIdentities()
            XCTAssertNil(seIdentities.localPeerIdentity, "Should have no local peer SE identity before setting it")
        }

        let sePeerID = "peerID".data(using: .utf8)!
        let sei = OTSecureElementPeerIdentity()!
        sei.peerIdentifier = sePeerID
        sei.peerData = "data".data(using: .utf8)!

        // Add a SecureElement identity
        let setExpectation = self.expectation(description: "update() occurs adding the ID")
        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()
            XCTAssertEqual(newStableInfo.secureElementIdentity?.peerIdentifier, sei.peerIdentifier, "Peer identifier should be correctly set")
            XCTAssertEqual(newStableInfo.secureElementIdentity?.peerData, sei.peerData, "Peer data should be correctly set")

            // While we're holding up the update, call the fetch API and see what's there
            do {
                let seIdentities = try cliqueBridge.fetchTrustedSecureElementIdentities()
                XCTAssertNil(seIdentities.localPeerIdentity, "Should have not a local peer SE identity (while update is in flight)")
                XCTAssertNotNil(seIdentities.pendingLocalPeerIdentity, "Should have a pending local peer SE identity (while update is in flight)")
                XCTAssertEqual(seIdentities.pendingLocalPeerIdentity?.peerIdentifier, sei.peerIdentifier, "Pending SE Peer Identity should match set value")
                XCTAssertEqual(seIdentities.pendingLocalPeerIdentity?.peerData, sei.peerData, "Pending SE Peer Identity should match set value")
            } catch {
                XCTFail("unexpected exception \(error)")
            }

            setExpectation.fulfill()
            return nil
        }

        XCTAssertNoThrow(try cliqueBridge.setLocalSecureElementIdentity(sei), "Should be able to successfully set the secure element identity")
        self.wait(for: [setExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        do {
            let seIdentities = try cliqueBridge.fetchTrustedSecureElementIdentities()
            XCTAssertNotNil(seIdentities.localPeerIdentity, "Should have a local peer SE identity")
            XCTAssertEqual(seIdentities.localPeerIdentity?.peerIdentifier, sei.peerIdentifier, "SE Peer Identity should match set value")
            XCTAssertEqual(seIdentities.localPeerIdentity?.peerData, sei.peerData, "SE Peer Identity should match set value")
            XCTAssertNil(seIdentities.pendingLocalPeerIdentity, "Should have no pending local peer SE identity")
        }

        // And be sure that trusting a new peer doesn't change the identity

        let joinedContext = self.makeInitiatorContext(contextID: "joining", authKitAdapter: self.mockAuthKit2)
        let newPeerID = self.assertJoinViaEscrowRecovery(joiningContext: joinedContext, sponsor: self.cuttlefishContext)

        let trustExpectation = self.expectation(description: "update() occurs trusting the nwe peer")
        self.fakeCuttlefishServer.updateListener = { _ in
            trustExpectation.fulfill()
            return nil
        }

        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.wait(for: [trustExpectation], timeout: 10)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.verifyDatabaseMocks()
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: originalPeerID, opinion: .trusts, target: newPeerID)))
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: newPeerID, opinion: .trusts, target: originalPeerID)))

        do {
            let seIdentities = try cliqueBridge.fetchTrustedSecureElementIdentities()
            XCTAssertNotNil(seIdentities.localPeerIdentity, "Should have a local peer SE identity")
            XCTAssertEqual(seIdentities.localPeerIdentity?.peerIdentifier, sei.peerIdentifier, "SE Peer Identity should match set value")
            XCTAssertEqual(seIdentities.localPeerIdentity?.peerData, sei.peerData, "SE Peer Identity should match set value")
            XCTAssertNil(seIdentities.pendingLocalPeerIdentity, "Should have no pending local peer SE identity")
        }

        // and remove the SecureElement identity
        let removeExpectation = self.expectation(description: "update() occurs removing the ID")
        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()
            XCTAssertNil(newStableInfo.secureElementIdentity, "Secure Element identifier should be gone")

            removeExpectation.fulfill()
            return nil
        }

        XCTAssertNoThrow(try cliqueBridge.removeLocalSecureElementIdentityPeerID(sei.peerIdentifier), "Should be able to successfully remove the secure element identity")
        self.wait(for: [removeExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        do {
            let seIdentities = try cliqueBridge.fetchTrustedSecureElementIdentities()
            XCTAssertNil(seIdentities.localPeerIdentity, "Should have no local peer SE identity after removing it")
        }
    }

    func testSetSecureElementIdentityBeforeEstablish() throws {
        self.startCKAccountStatusMock()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        let sePeerID = "peerID".data(using: .utf8)!
        let sei = OTSecureElementPeerIdentity()!
        sei.peerIdentifier = sePeerID
        sei.peerData = "data".data(using: .utf8)!

        XCTAssertNoThrow(try cliqueBridge.setLocalSecureElementIdentity(sei), "Should be able to successfully set the secure element identity")

        do {
            let seIdentities = try cliqueBridge.fetchTrustedSecureElementIdentities()
            XCTAssertNil(seIdentities.localPeerIdentity, "Should have not a local peer SE identity (before establish)")
            XCTAssertNotNil(seIdentities.pendingLocalPeerIdentity, "Should have a pending local peer SE identity (before establish)")
            XCTAssertEqual(seIdentities.pendingLocalPeerIdentity?.peerIdentifier, sei.peerIdentifier, "Pending SE Peer Identity should match set value (before establish)")
            XCTAssertEqual(seIdentities.pendingLocalPeerIdentity?.peerData, sei.peerData, "Pending SE Peer Identity should match set value (before establish)")
        }

        // Add a SecureElement identity
        let establishExpectation = self.expectation(description: "establish occurs")
        self.fakeCuttlefishServer.establishListener = { request in
            let newStableInfo = request.peer.stableInfoAndSig.stableInfo()
            XCTAssertEqual(newStableInfo.secureElementIdentity?.peerIdentifier, sei.peerIdentifier, "Peer identifier should be correctly set")
            XCTAssertEqual(newStableInfo.secureElementIdentity?.peerData, sei.peerData, "Peer data should be correctly set")

            establishExpectation.fulfill()
            return nil
        }

        self.assertResetAndBecomeTrustedInDefaultContext()

        self.wait(for: [establishExpectation], timeout: 10)
    }

    func testSetSecureElementIdentityBeforeEscrowJoin() throws {
        self.startCKAccountStatusMock()

        let originatorContext = self.makeInitiatorContext(contextID: "originator", authKitAdapter: self.mockAuthKit2)
        let originatorPeerID = self.assertResetAndBecomeTrusted(context: originatorContext)

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        let sePeerID = "peerID".data(using: .utf8)!
        let sei = OTSecureElementPeerIdentity()!
        sei.peerIdentifier = sePeerID
        sei.peerData = "data".data(using: .utf8)!

        XCTAssertNoThrow(try cliqueBridge.setLocalSecureElementIdentity(sei), "Should be able to successfully set the secure element identity")

        // Add a SecureElement identity
        let joinExpectation = self.expectation(description: "join occurs")
        self.fakeCuttlefishServer.joinListener = { request in
            let newStableInfo = request.peer.stableInfoAndSig.stableInfo()
            XCTAssertEqual(newStableInfo.secureElementIdentity?.peerIdentifier, sei.peerIdentifier, "Peer identifier should be correctly set")
            XCTAssertEqual(newStableInfo.secureElementIdentity?.peerData, sei.peerData, "Peer data should be correctly set")

            joinExpectation.fulfill()
            return nil
        }

        let newPeerID = self.assertJoinViaEscrowRecovery(joiningContext: self.cuttlefishContext, sponsor: originatorContext)
        self.wait(for: [joinExpectation], timeout: 10)

        // The originator should see and accept the change
        self.sendContainerChangeWaitForFetch(context: originatorContext)
        self.assertEnters(context: originatorContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: originatorPeerID, opinion: .trusts, target: newPeerID)),
                      "originator should trust the new peer")

        let originatorBridge = OctagonTrustCliqueBridge(clique: self.cliqueFor(context: originatorContext))
        do {
            let seIdentities = try originatorBridge.fetchTrustedSecureElementIdentities()
            XCTAssertNil(seIdentities.localPeerIdentity, "Should not have a local peer SE identity for originator")
            XCTAssertEqual(seIdentities.trustedPeerSecureElementIdentitiesCount(), 1, "Should have one remote SE identity")

            if let trustedSEIdentity = Array(seIdentities.trustedPeerSecureElementIdentities ?? []).first as! OTSecureElementPeerIdentity? {
                XCTAssertEqual(trustedSEIdentity.peerIdentifier, sei.peerIdentifier, "Trusted SE Peer Identity should match set value")
                XCTAssertEqual(trustedSEIdentity.peerData, sei.peerData, "Trusted SE Peer Identity should match set value")
            } else {
                XCTFail("Couldn't find a trusted peer identity")
            }
        }
    }
}

#endif
