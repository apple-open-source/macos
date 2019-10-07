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

        // On CK account sign-out, Octagon should go back to 'wait for cloudkit account'
        self.accountStatus = .noAccount
        self.accountStateTracker.notifyCKAccountStatusChangeAndWaitForSignal()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitingForCloudKitAccount, within: 10 * NSEC_PER_SEC)

        // On sign-out, octagon should go back to 'no account'
        self.mockAuthKit.altDSID = nil
        XCTAssertNoThrow(try self.cuttlefishContext.accountNoLongerAvailable(), "sign-out shouldn't error")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateLoggedOut, within: 10 * NSEC_PER_SEC)
    }
}

#endif // OCTAGON
