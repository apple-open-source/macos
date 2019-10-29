#if OCTAGON

class OctagonCloudKitAccountTests: OctagonTestsBase {
    func testSignInSucceedsAfterCloudKitNotification() throws {
        // Device is signed out
        self.mockAuthKit.altDSID = nil

        // but CK is going to win the race, and tell us everything is fine first
        self.accountStatus = .available
        self.startCKAccountStatusMock()
        self.accountStateTracker.notifyCKAccountStatusChangeAndWaitForSignal()

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // Account sign in occurs
        let newAltDSID = UUID().uuidString
        self.mockAuthKit.altDSID = newAltDSID
        XCTAssertNoThrow(try self.cuttlefishContext.accountAvailable(newAltDSID), "Sign-in shouldn't error")

        // We should reach 'untrusted', as we cached the CK value
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)
    }

    func testSignInPausesForCloudKit() throws {
        // Device is signed out
        self.mockAuthKit.altDSID = nil

        // And out of cloudkit
        self.accountStatus = .noAccount
        self.startCKAccountStatusMock()

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // And CKKS should be in 'loggedout'
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateLoggedOut, within: 10 * NSEC_PER_SEC)

        // Account sign in occurs
        let newAltDSID = UUID().uuidString
        self.mockAuthKit.altDSID = newAltDSID
        XCTAssertNoThrow(try self.cuttlefishContext.accountAvailable(newAltDSID), "Sign-in shouldn't error")

        // Octagon should go into 'wait for cloudkit account'
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitingForCloudKitAccount, within: 10 * NSEC_PER_SEC)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateLoggedOut, within: 10 * NSEC_PER_SEC)

        // And when CK shows up, we should go into 'untrusted'
        self.accountStatus = .available
        self.accountStateTracker.notifyCKAccountStatusChangeAndWaitForSignal()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        // sign-out CK first:
        self.accountStatus = .noAccount
        self.accountStateTracker.notifyCKAccountStatusChangeAndWaitForSignal()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitingForCloudKitAccount, within: 10 * NSEC_PER_SEC)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateLoggedOut, within: 10 * NSEC_PER_SEC)

        // On sign-out, octagon should go back to 'no account'
        self.mockAuthKit.altDSID = nil
        XCTAssertNoThrow(try self.cuttlefishContext.accountNoLongerAvailable(), "sign-out shouldn't error")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)

        // and CKKS is still in 'loggedout'
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateLoggedOut, within: 10 * NSEC_PER_SEC)
    }

    func testSignOutFromWaitingForCloudKit() {
        // Device is signed out
        self.mockAuthKit.altDSID = nil

        // And out of cloudkit
        self.accountStatus = .noAccount
        self.startCKAccountStatusMock()

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // Account sign in occurs
        let newAltDSID = UUID().uuidString
        self.mockAuthKit.altDSID = newAltDSID
        XCTAssertNoThrow(try self.cuttlefishContext.accountAvailable(newAltDSID), "Sign-in shouldn't error")

        // Octagon should go into 'wait for cloudkit account'
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitingForCloudKitAccount, within: 10 * NSEC_PER_SEC)

        // On sign-out, octagon should go back to 'no account'
        self.mockAuthKit.altDSID = nil
        XCTAssertNoThrow(try self.cuttlefishContext.accountNoLongerAvailable(), "sign-out shouldn't error")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)
    }

    func testCloudKitAccountDisappears() {
        // Device is signed out
        self.mockAuthKit.altDSID = nil

        // And out of cloudkit
        self.accountStatus = .noAccount
        self.startCKAccountStatusMock()

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // Account sign in occurs
        let newAltDSID = UUID().uuidString
        self.mockAuthKit.altDSID = newAltDSID
        XCTAssertNoThrow(try self.cuttlefishContext.accountAvailable(newAltDSID), "Sign-in shouldn't error")

        // Octagon should go into 'wait for cloudkit account'
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitingForCloudKitAccount, within: 10 * NSEC_PER_SEC)

        // And when CK shows up, we should go into 'untrusted'
        self.accountStatus = .available
        self.accountStateTracker.notifyCKAccountStatusChangeAndWaitForSignal()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        // On CK account sign-out, Octagon should go back to 'wait for cloudkit account'
        self.accountStatus = .noAccount
        self.accountStateTracker.notifyCKAccountStatusChangeAndWaitForSignal()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitingForCloudKitAccount, within: 10 * NSEC_PER_SEC)

        self.mockAuthKit.altDSID = nil
        XCTAssertNoThrow(try self.cuttlefishContext.accountNoLongerAvailable(), "sign-out shouldn't error")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateLoggedOut, within: 10 * NSEC_PER_SEC)
    }

    func testSignOutOfSAAccount() throws {
        self.startCKAccountStatusMock()

        // Device is signed out
        self.mockAuthKit.altDSID = nil
        self.mockAuthKit.hsa2 = false

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // CloudKit sign in occurs, but HSA2 status isn't here yet
        let newAltDSID = UUID().uuidString
        self.mockAuthKit.altDSID = newAltDSID
        XCTAssertNoThrow(try self.cuttlefishContext.accountAvailable(newAltDSID), "Sign-in shouldn't error")

        // Octagon should go into 'waitforhsa2'
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForHSA2, within: 10 * NSEC_PER_SEC)

        XCTAssertNoThrow(try self.cuttlefishContext.idmsTrustLevelChanged(), "Notification of IDMS trust level shouldn't error")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForHSA2, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateLoggedOut, within: 10 * NSEC_PER_SEC)

        // On CK account sign-out, Octagon should stay in 'wait for hsa2': if there's no HSA2, we don't actually care about the CK account status
        self.accountStatus = .noAccount
        self.accountStateTracker.notifyCKAccountStatusChangeAndWaitForSignal()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForHSA2, within: 10 * NSEC_PER_SEC)

        // On sign-out, octagon should go back to 'no account'
        self.mockAuthKit.altDSID = nil
        XCTAssertNoThrow(try self.cuttlefishContext.accountNoLongerAvailable(), "sign-out shouldn't error")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateLoggedOut, within: 10 * NSEC_PER_SEC)
    }

    func testSAtoHSA2PromotionWithoutCloudKit() throws {
        self.startCKAccountStatusMock()

        // Device is signed out
        self.mockAuthKit.altDSID = nil
        self.mockAuthKit.hsa2 = false

        self.accountStatus = .noAccount
        self.accountStateTracker.notifyCKAccountStatusChangeAndWaitForSignal()

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // Account signs in as SA.
        let newAltDSID = UUID().uuidString
        self.mockAuthKit.altDSID = newAltDSID

        XCTAssertNoThrow(try self.cuttlefishContext.idmsTrustLevelChanged(), "Notification of IDMS trust level shouldn't error")
        XCTAssertNoThrow(try self.cuttlefishContext.accountAvailable(newAltDSID), "Sign-in shouldn't error")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForHSA2, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)

        // Account promotes to HSA2
        self.mockAuthKit.hsa2 = true
        XCTAssertNoThrow(try self.cuttlefishContext.idmsTrustLevelChanged(), "Notification of IDMS trust level shouldn't error")

        // Octagon should go into 'waitforcloudkit'
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitingForCloudKitAccount, within: 10 * NSEC_PER_SEC)
        self.assertAccountAvailable(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateLoggedOut, within: 10 * NSEC_PER_SEC)

        // On CK account sign-in, Octagon should race to 'untrusted'
        self.accountStatus = .available
        self.accountStateTracker.notifyCKAccountStatusChangeAndWaitForSignal()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        // On sign-out, octagon should go back to 'no account'
        self.mockAuthKit.altDSID = nil
        XCTAssertNoThrow(try self.cuttlefishContext.accountNoLongerAvailable(), "sign-out shouldn't error")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)

        // But CKKS is listening for the CK account removal, not the accountNoLongerAvailable call
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        self.accountStatus = .noAccount
        self.accountStateTracker.notifyCKAccountStatusChangeAndWaitForSignal()
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateLoggedOut, within: 10 * NSEC_PER_SEC)
    }

    func testAPIFailureWhenSA() throws {
        self.startCKAccountStatusMock()

        // Account is present, but SA
        self.mockAuthKit.hsa2 = false

        XCTAssertNoThrow(try self.cuttlefishContext.idmsTrustLevelChanged(), "Notification of IDMS trust level shouldn't error")
        XCTAssertNoThrow(try self.cuttlefishContext.accountAvailable(self.mockAuthKit.altDSID!), "Sign-in shouldn't error")

        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForHSA2, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)

        // Calling OTClique API should error (eventually)
        self.cuttlefishContext.stateMachine.setWatcherTimeout(4 * NSEC_PER_SEC)
        XCTAssertThrowsError(try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated), "establishing new friends in an SA account should error")

        // And octagon should still believe everything is terrible
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForHSA2, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(self.cuttlefishContext.currentMemoizedTrustState(), .UNKNOWN, "Trust state should be unknown")

        let statusexpectation = self.expectation(description: "trust status returns")
        let configuration = OTOperationConfiguration()
        configuration.timeoutWaitForCKAccount = 500 * NSEC_PER_MSEC
        self.cuttlefishContext.rpcTrustStatus(configuration) { egoStatus, _, _, _, _ in
            XCTAssertEqual(.noCloudKitAccount, egoStatus, "cliqueStatus should be 'no cloudkit account'")
            statusexpectation.fulfill()
        }
        self.wait(for: [statusexpectation], timeout: 10)

        self.assertNoAccount(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateLoggedOut, within: 10 * NSEC_PER_SEC)
    }
}

#endif // OCTAGON
