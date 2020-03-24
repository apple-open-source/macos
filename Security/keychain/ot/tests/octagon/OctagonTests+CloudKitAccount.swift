#if OCTAGON

class OctagonCloudKitAccountTests: OctagonTestsBase {
    func testSignInSucceedsAfterCloudKitNotification() throws {
        // Device is signed out
        self.mockAuthKit.altDSID = nil

        // but CK is going to win the race, and tell us everything is fine first
        self.accountStatus = .available
        self.startCKAccountStatusMock()
        self.accountStateTracker.notifyCKAccountStatusChangeAndWaitForSignal()

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // Account sign in occurs
        let newAltDSID = UUID().uuidString
        self.mockAuthKit.altDSID = newAltDSID
        XCTAssertNoThrow(try self.cuttlefishContext.accountAvailable(newAltDSID), "Sign-in shouldn't error")

        // We should reach 'waitforcdp', as we cached the CK value
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfWaitingForCDP(context: self.cuttlefishContext)
    }

    func testSignInPausesForCloudKit() throws {
        // Device is signed out
        self.mockAuthKit.altDSID = nil

        // And out of cloudkit
        self.accountStatus = .noAccount
        self.startCKAccountStatusMock()

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

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

        // And when CK shows up, we should go into 'waitforcdp'
        self.accountStatus = .available
        self.accountStateTracker.notifyCKAccountStatusChangeAndWaitForSignal()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfWaitingForCDP(context: self.cuttlefishContext)

        // And then CDP is set to be on
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())

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

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // Account sign in occurs
        let newAltDSID = UUID().uuidString
        self.mockAuthKit.altDSID = newAltDSID
        XCTAssertNoThrow(try self.cuttlefishContext.accountAvailable(newAltDSID), "Sign-in shouldn't error")

        // Octagon should go into 'wait for cloudkit account'
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitingForCloudKitAccount, within: 10 * NSEC_PER_SEC)

        // And when CK shows up, we should go into 'waitforcdp'
        self.accountStatus = .available
        self.accountStateTracker.notifyCKAccountStatusChangeAndWaitForSignal()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfWaitingForCDP(context: self.cuttlefishContext)

        // and then, when CDP tells us, we should go into 'untrusted'
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())

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

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

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

        // On CK account sign-in, Octagon should race to 'waitforcdp'
        self.accountStatus = .available
        self.accountStateTracker.notifyCKAccountStatusChangeAndWaitForSignal()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfWaitingForCDP(context: self.cuttlefishContext)

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
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

    func testSAtoHSA2PromotionandCDPWithoutCloudKit() throws {
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

        // And setting CDP bit now should be successful, but not move the state machine
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitingForCloudKitAccount, within: 10 * NSEC_PER_SEC)

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

    func testStatusRPCsWithUnknownCloudKitAccount() throws {
        // If CloudKit isn't returning our calls, we should still return something reasonable...
        let statusexpectation = self.expectation(description: "trust status returns")
        let configuration = OTOperationConfiguration()
        configuration.timeoutWaitForCKAccount = 500 * NSEC_PER_MSEC
        self.cuttlefishContext.rpcTrustStatus(configuration) { egoStatus, _, _, _, _ in
            XCTAssertTrue([.absent].contains(egoStatus), "Self peer should be in the 'absent' state")
            statusexpectation.fulfill()
        }
        self.wait(for: [statusexpectation], timeout: 10)

        // Now sign in to 'untrusted'
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        // And restart, without any idea of the cloudkit state
        self.ckaccountHoldOperation = BlockOperation()
        self.manager.accountStateTracker = CKKSAccountStateTracker(self.manager.cloudKitContainer,
                                                                   nsnotificationCenterClass: FakeNSNotificationCenter.self as CKKSNSNotificationCenter.Type)
        self.injectedManager!.accountTracker = self.manager.accountStateTracker

        self.manager.removeContext(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.restartCKKSViews()
        self.cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        // Should know it's untrusted
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)
        self.startCKAccountStatusMock()

        // Now become ready
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
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // Restart one more time:

        self.ckaccountHoldOperation = BlockOperation()
        self.manager.accountStateTracker = CKKSAccountStateTracker(self.manager.cloudKitContainer,
                                                                   nsnotificationCenterClass: FakeNSNotificationCenter.self as CKKSNSNotificationCenter.Type)
        self.injectedManager!.accountTracker = self.manager.accountStateTracker

        self.manager.removeContext(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.restartCKKSViews()
        self.cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertConsidersSelfTrustedCachedAccountStatus(context: self.cuttlefishContext)
    }
}

#endif // OCTAGON
