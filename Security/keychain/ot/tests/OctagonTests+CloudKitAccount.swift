#if OCTAGON

class OctagonCloudKitAccountTests: OctagonTestsBase {
    func testSignInSucceedsAfterCloudKitNotification() throws {
        // Device is signed out
        self.mockAuthKit.removePrimaryAccount()

        // but CK is going to win the race, and tell us everything is fine first
        self.accountStatus = .available
        self.startCKAccountStatusMock()
        self.accountStateTracker.notifyCKAccountStatusChangeAndWaitForSignal()

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // Account sign in occurs
        let newAltDSID = UUID().uuidString
        let account = CloudKitAccount(altDSID: newAltDSID, persona: nil, hsa2: true, demo: false, accountStatus: .available)
        self.mockAuthKit.add(account)
        XCTAssertNoThrow(try self.cuttlefishContext.accountAvailable(newAltDSID), "Sign-in shouldn't error")

        // We should reach 'waitforcdp', as we cached the CK value
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfWaitingForCDP(context: self.cuttlefishContext)

        // Fetching the syncing status from 'waitforcdp' should be fast
        let clique = self.cliqueFor(context: self.cuttlefishContext)
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: false)
    }

    func testSignInPausesForCloudKit() throws {
        // Device is signed out
        self.mockAuthKit.removePrimaryAccount()

        // And out of cloudkit
        self.accountStatus = .noAccount
        self.startCKAccountStatusMock()

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // And CKKS shouldn't be spun up
        XCTAssertEqual(self.defaultCKKS.viewList.count, 0, "Should have no CKKS views before CK login")

        // Account sign in occurs
        let newAltDSID = UUID().uuidString
        let account = CloudKitAccount(altDSID: newAltDSID, persona: nil, hsa2: true, demo: false, accountStatus: .available)
        self.mockAuthKit.add(account)
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
        self.mockAuthKit.removePrimaryAccount()
        XCTAssertNoThrow(self.cuttlefishContext.accountNoLongerAvailable(), "sign-out shouldn't error")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)

        // and CKKS is still in 'loggedout'
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateLoggedOut, within: 10 * NSEC_PER_SEC)
    }

    func testSignOutFromWaitingForCloudKit() {
        // Device is signed out
        self.mockAuthKit.removePrimaryAccount()

        // And out of cloudkit
        self.accountStatus = .noAccount
        self.startCKAccountStatusMock()

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // Account sign in occurs
        let newAltDSID = UUID().uuidString
        let account = CloudKitAccount(altDSID: newAltDSID, persona: nil, hsa2: true, demo: false, accountStatus: .available)
        self.mockAuthKit.add(account)
        XCTAssertNoThrow(try self.cuttlefishContext.accountAvailable(newAltDSID), "Sign-in shouldn't error")

        // Octagon should go into 'wait for cloudkit account'
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitingForCloudKitAccount, within: 10 * NSEC_PER_SEC)

        // On sign-out, octagon should go back to 'no account'
        self.mockAuthKit.removePrimaryAccount()
        XCTAssertNoThrow(self.cuttlefishContext.accountNoLongerAvailable(), "sign-out shouldn't error")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)
    }

    func testCloudKitAccountDisappears() {
        // Device is signed out
        self.mockAuthKit.removePrimaryAccount()

        // And out of cloudkit
        self.accountStatus = .noAccount
        self.startCKAccountStatusMock()

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // Account sign in occurs
        let newAltDSID = UUID().uuidString
        let account = CloudKitAccount(altDSID: newAltDSID, persona: nil, hsa2: true, demo: false, accountStatus: .available)
        self.mockAuthKit.add(account)

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

        self.mockAuthKit.removePrimaryAccount()
        XCTAssertNoThrow(self.cuttlefishContext.accountNoLongerAvailable(), "sign-out shouldn't error")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateLoggedOut, within: 10 * NSEC_PER_SEC)
    }

    func testSignOutOfSAAccount() throws {
        // Device is signed out
        self.mockAuthKit.removePrimaryAccount()

        self.startCKAccountStatusMock()

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // CloudKit sign in occurs, but HSA2 status isn't here yet
        let newAltDSID = UUID().uuidString
        let account = CloudKitAccount(altDSID: newAltDSID, persona: nil, hsa2: false, demo: false, accountStatus: .available)
        self.mockAuthKit.add(account)

        XCTAssertNoThrow(try self.cuttlefishContext.accountAvailable(newAltDSID), "Sign-in shouldn't error")

        // Octagon should go into 'waitforhsa2'
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDPCapableSecurityLevel, within: 10 * NSEC_PER_SEC)

        XCTAssertNoThrow(try self.cuttlefishContext.idmsTrustLevelChanged(), "Notification of IDMS trust level shouldn't error")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDPCapableSecurityLevel, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)

        XCTAssertEqual(self.defaultCKKS.viewList.count, 0, "Should have no CKKS views in an SA account")

        // On CK account sign-out, Octagon should stay in 'wait for hsa2': if there's no HSA2, we don't actually care about the CK account status
        self.accountStatus = .noAccount
        self.accountStateTracker.notifyCKAccountStatusChangeAndWaitForSignal()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDPCapableSecurityLevel, within: 10 * NSEC_PER_SEC)

        // On sign-out, octagon should go back to 'no account'
        self.mockAuthKit.removePrimaryAccount()
        XCTAssertNoThrow(self.cuttlefishContext.accountNoLongerAvailable(), "sign-out shouldn't error")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)

        XCTAssertEqual(self.defaultCKKS.viewList.count, 0, "Should have no CKKS views in an SA account")
    }

    func testSAtoHSA2PromotionWithoutCloudKit() throws {
        // Device is signed out
        self.mockAuthKit.removePrimaryAccount()

        self.accountStatus = .noAccount
        self.accountStateTracker.notifyCKAccountStatusChangeAndWaitForSignal()

        self.startCKAccountStatusMock()

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // Account signs in as SA.
        let newAltDSID = UUID().uuidString
        let account = CloudKitAccount(altDSID: newAltDSID, persona: nil, hsa2: false, demo: false, accountStatus: .available)
        self.mockAuthKit.add(account)

        XCTAssertNoThrow(try self.cuttlefishContext.idmsTrustLevelChanged(), "Notification of IDMS trust level shouldn't error")
        XCTAssertNoThrow(try self.cuttlefishContext.accountAvailable(newAltDSID), "Sign-in shouldn't error")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDPCapableSecurityLevel, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)

        // Account promotes to HSA2
        let account2 = CloudKitAccount(altDSID: newAltDSID, persona: nil, hsa2: true, demo: false, accountStatus: .available)
        self.mockAuthKit.add(account2)

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
        self.mockAuthKit.removePrimaryAccount()
        XCTAssertNoThrow(self.cuttlefishContext.accountNoLongerAvailable(), "sign-out shouldn't error")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)

        // But CKKS is listening for the CK account removal, not the accountNoLongerAvailable call
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        self.accountStatus = .noAccount
        self.accountStateTracker.notifyCKAccountStatusChangeAndWaitForSignal()
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateLoggedOut, within: 10 * NSEC_PER_SEC)
    }

    func testSAtoHSA2PromotionandCDPWithoutCloudKit() throws {
        // Device is signed out
        self.mockAuthKit.removePrimaryAccount()

        self.accountStatus = .noAccount
        self.accountStateTracker.notifyCKAccountStatusChangeAndWaitForSignal()

        self.startCKAccountStatusMock()

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // Account signs in as SA.
        let newAltDSID = UUID().uuidString
        let account = CloudKitAccount(altDSID: newAltDSID, persona: nil, hsa2: false, demo: false, accountStatus: .available)
        self.mockAuthKit.add(account)

        XCTAssertNoThrow(try self.cuttlefishContext.idmsTrustLevelChanged(), "Notification of IDMS trust level shouldn't error")
        XCTAssertNoThrow(try self.cuttlefishContext.accountAvailable(newAltDSID), "Sign-in shouldn't error")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDPCapableSecurityLevel, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)

        // Account promotes to HSA2
        let account2 = CloudKitAccount(altDSID: newAltDSID, persona: nil, hsa2: true, demo: false, accountStatus: .available)
        self.mockAuthKit.add(account2)
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
        self.mockAuthKit.removePrimaryAccount()
        XCTAssertNoThrow(self.cuttlefishContext.accountNoLongerAvailable(), "sign-out shouldn't error")
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
        let primaryAccount = try XCTUnwrap(self.mockAuthKit.primaryAccount())
        let account = CloudKitAccount(altDSID: primaryAccount.altDSID, persona: nil, hsa2: false, demo: false, accountStatus: .available)
        self.mockAuthKit.add(account)

        XCTAssertNoThrow(try self.cuttlefishContext.idmsTrustLevelChanged(), "Notification of IDMS trust level shouldn't error")
        XCTAssertNoThrow(try self.cuttlefishContext.accountAvailable(try XCTUnwrap(self.mockAuthKit.primaryAltDSID())), "Sign-in shouldn't error")

        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDPCapableSecurityLevel, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)

        // Calling OTClique API should error (eventually)
        self.cuttlefishContext.stateMachine.setWatcherTimeout(4 * NSEC_PER_SEC)
        XCTAssertThrowsError(try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated), "establishing new friends in an SA account should error")

        // And octagon should still believe everything is terrible
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDPCapableSecurityLevel, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(self.cuttlefishContext.currentMemoizedTrustState(), .UNKNOWN, "Trust state should be unknown")

        let statusexpectation = self.expectation(description: "trust status returns")
        let configuration = OTOperationConfiguration()
        configuration.timeoutWaitForCKAccount = 500 * NSEC_PER_MSEC
        self.cuttlefishContext.rpcTrustStatus(configuration) { egoStatus, _, _, _, _, error in
            XCTAssertEqual(.noCloudKitAccount, egoStatus, "cliqueStatus should be 'no cloudkit account'")
            XCTAssertNil(error, "should have no error fetching status")
            statusexpectation.fulfill()
        }
        self.wait(for: [statusexpectation], timeout: 10)

        self.assertNoAccount(context: self.cuttlefishContext)

        XCTAssertEqual(self.defaultCKKS.viewList.count, 0, "Should have no CKKS views")
    }

    func testStatusRPCsWithUnknownCloudKitAccount() throws {
        try self.skipOnRecoveryKeyNotSupported()

        // If CloudKit isn't returning our calls, we should still return something reasonable...
        let statusexpectation = self.expectation(description: "trust status returns")
        let configuration = OTOperationConfiguration()
        configuration.timeoutWaitForCKAccount = 500 * NSEC_PER_MSEC
        self.cuttlefishContext.rpcTrustStatus(configuration) { egoStatus, _, _, _, _, _ in
            XCTAssertTrue([.absent].contains(egoStatus), "Self peer should be in the 'absent' state")
            statusexpectation.fulfill()
        }
        self.wait(for: [statusexpectation], timeout: 10)

        let fetchUserViewsExpectation = self.expectation(description: "fetchUCV returns")
        let clique = self.cliqueFor(context: self.cuttlefishContext)
        XCTAssertThrowsError(try clique.fetchUserControllableViewsSyncingEnabled()) { error in
            let nerrror = error as NSError
            XCTAssertEqual(nerrror.domain, OctagonErrorDomain, "Error should be an Octagon Error")
            XCTAssertEqual(nerrror.code, OctagonError.iCloudAccountStateUnknown.rawValue, "Error should be 'account state unknown'")
            fetchUserViewsExpectation.fulfill()
        }
        self.wait(for: [fetchUserViewsExpectation], timeout: 10)

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
        self.defaultCKKS.accountTracker = self.manager.accountStateTracker

        self.manager.removeContext(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.defaultCKKS = try XCTUnwrap(self.cuttlefishContext.ckks)

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
        self.defaultCKKS.accountTracker = self.manager.accountStateTracker

        self.manager.removeContext(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.defaultCKKS = try XCTUnwrap(self.cuttlefishContext.ckks)

        // Because the state machine hasn't started up yet, and CK hasn't told us to look, we don't spin up the active account just yet.
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext, checkActiveAccount: false)
        self.assertConsidersSelfTrustedCachedAccountStatus(context: self.cuttlefishContext)

        // Let Octagon know about the account status, so test teardown is fast
        self.startCKAccountStatusMock()
    }

    func testReceiveOctagonAPICallBeforeCKAccountNotification() throws {
        // Device is signed out of everything
        self.mockAuthKit.removePrimaryAccount()
        self.accountStatus = .noAccount
        self.startCKAccountStatusMock()

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(self.defaultCKKS.viewList.count, 0, "Should have no CKKS views before CK login")

        // Account sign in occurs
        let newAltDSID = UUID().uuidString
        let account = CloudKitAccount(altDSID: newAltDSID, persona: nil, hsa2: true, demo: false, accountStatus: .available)
        self.mockAuthKit.add(account)
        XCTAssertNoThrow(try self.cuttlefishContext.accountAvailable(newAltDSID), "Sign-in shouldn't error")

        // Octagon should go into 'wait for cloudkit account'
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitingForCloudKitAccount, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateLoggedOut, within: 10 * NSEC_PER_SEC)

        // RPCs should fail, since CK is not present
        let fetchViableFailureExpectation = self.expectation(description: "fetchViableBottles callback occurs")
        self.cuttlefishContext.rpcFetchAllViableBottles(from: .default) { _, _, error in
            XCTAssertNotNil(error, "should be an error fetching viable bottles before CK is ready")

            if let nserror = error as NSError? {
                XCTAssertEqual(nserror.domain, OctagonErrorDomain, "Error should be from correct domain")
                XCTAssertEqual(nserror.code, OctagonError.notSignedIn.rawValue, "Error should indicate no account present")
            } else {
                XCTFail("Unable to convert error to nserror")
            }
            fetchViableFailureExpectation.fulfill()
        }
        self.wait(for: [fetchViableFailureExpectation], timeout: 10)

        // CK signs in, but we aren't yet notified about it
        self.accountStatus = .available

        // RPCs should cause Octagon to recheck the CK account status and wake up
        // In particular, fetching all viable bottles should succeed, instead of erroring with 'no CK account'
        let fetchViableExpectation = self.expectation(description: "fetchViableBottles callback occurs")
        self.cuttlefishContext.rpcFetchAllViableBottles(from: .default) { _, _, error in
            XCTAssertNil(error, "should be no error fetching viable bottles")
            fetchViableExpectation.fulfill()
        }
        self.wait(for: [fetchViableExpectation], timeout: 10)

        // With no SOS and no CDP, Octagon should wait for enablement
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)
    }

    func testRecoverFromCloudKitAccountFetchXPCFailureOnRPC() throws {
        // When we ask CK for the account status, we get an XPC error (probably due to background/aqua session confusion on macOS)
        self.accountStatus = .couldNotDetermine
        self.ckAccountStatusFetchError = NSError(domain: NSCocoaErrorDomain, code: 4099, userInfo: nil)

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitingForCloudKitAccount, within: 10 * NSEC_PER_SEC)

        // But then, the aqua session now exists, and CK is ready to tell us the state!
        // We do not get any notifications about this, because we started too early and the CK notification machinery is in a bad state
        self.accountStatus = .available
        self.ckAccountStatusFetchError = nil

        // RPCs should cause Octagon to recheck the CK account status and wake up
        // In particular, fetching all viable bottles should succeed, instead of erroring with 'no CK account'
        let fetchViableExpectation = self.expectation(description: "fetchViableBottles callback occurs")
        self.cuttlefishContext.rpcFetchAllViableBottles(from: .default) { _, _, error in
            XCTAssertNil(error, "should be no error fetching viable bottles")
            fetchViableExpectation.fulfill()
        }
        self.wait(for: [fetchViableExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)
    }

    func testRecoverFromCloudKitAccountFetchXPCFailureAfterTimer() throws {
        // When we ask CK for the account status, we get an XPC error (probably due to background/aqua session confusion on macOS)
        self.accountStatus = .couldNotDetermine
        self.ckAccountStatusFetchError = NSError(domain: NSCocoaErrorDomain, code: 4099, userInfo: nil)

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitingForCloudKitAccount, within: 10 * NSEC_PER_SEC)

        // But then, the aqua session now exists, and CK is ready to tell us the state!
        // We do not get any notifications about this, because we started too early and the CK notification machinery is in a bad state
        // We should refetch this within 5s.
        self.accountStatus = .available
        self.ckAccountStatusFetchError = nil

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)
    }
}

#endif // OCTAGON
