#if OCTAGON

class OctagonHealthCheckTests: OctagonTestsBase {

    func testHealthCheckAllTrusted() throws {
        let containerName = OTCKContainerName
        let contextName = OTDefaultContext

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName)
            XCTAssertEqual(2, accountState.trustState.rawValue, "saved account should be trusted")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(containerName, context: contextName, skipRateLimitingCheck: false) { error in
            XCTAssertNil(error, "error should be nil")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(withContainer: containerName, context: contextName) {
            dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? Dictionary<String, AnyObject>
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        self.verifyDatabaseMocks()
        self.assertEnters(context: cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testHealthCheckNoPeers() throws {
        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        cuttlefishContext.checkOctagonHealth(false) { error in
            XCTAssertNotNil(error, "error should be present; device is not healthy")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        #if os(tvOS)
        XCTAssertEqual(self.cuttlefishContext.postedRepairCFU, false, "Should not have posted a CFU on aTV")
        #else
        XCTAssertEqual(self.cuttlefishContext.postedRepairCFU, true, "Should have posted a CFU (due to being untrusted)")
        #endif

        self.verifyDatabaseMocks()
        self.assertEnters(context: cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // Reset flag for remainder of test
        self.cuttlefishContext.setPostedBool(false)

        // Set the "have I attempted to join" bit; TVs should still not CFU, but other devices should
        try! self.cuttlefishContext.accountMetadataStore.persistOctagonJoinAttempt(.ATTEMPTED)

        let healthCheckCallback2 = self.expectation(description: "healthCheckCallback callback occurs")
        cuttlefishContext.checkOctagonHealth(false) { error in
            XCTAssertNotNil(error, "error should be present; device is not healthy")
            healthCheckCallback2.fulfill()
        }
        self.wait(for: [healthCheckCallback2], timeout: 10)

        #if os(tvOS)
        XCTAssertEqual(self.cuttlefishContext.postedRepairCFU, false, "Should not have posted a CFU on aTV")
        #else
        XCTAssertEqual(self.cuttlefishContext.postedRepairCFU, true, "Should have posted a CFU")
        #endif
    }

    func testHealthCheckSecurityDStateNOTTrusted() throws {
        let containerName = OTCKContainerName
        let contextName = OTDefaultContext

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        //now let's ruin account state, and say we are untrusted
        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName)
            accountState.trustState = OTAccountMetadataClassC_TrustState(rawValue: 1)!
            try accountState.saveToKeychain(forContainer: containerName, contextID: contextName)
            XCTAssertEqual(1, accountState.trustState.rawValue, "Saved account state should have the same peer ID that prepare returned")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        cuttlefishContext.checkOctagonHealth(false) { error in
            XCTAssertNil(error, "error should be nil")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        self.assertEnters(context: cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: cuttlefishContext)

        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(withContainer: containerName, context: contextName) {
            dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? Dictionary<String, AnyObject>
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? Dictionary<String, AnyObject>
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? Array<String>
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 1, "should be 1 peer ids")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        self.verifyDatabaseMocks()
        self.assertEnters(context: cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testHealthCheckTrustedPeersHelperStateNOTTrusted() throws {
        let containerName = OTCKContainerName
        let contextName = OTDefaultContext

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName)
            XCTAssertEqual(2, accountState.trustState.rawValue, "Saved account state should be trusted")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        var healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        cuttlefishContext.checkOctagonHealth(false) { error in
            XCTAssertNil(error, "error should be nil")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        // now lets completely wipe cuttlefish state
        let resetCallback = self.expectation(description: "resetCallback callback occurs")
        self.tphClient.localReset(withContainer: containerName, context: contextName) { error in
            XCTAssertNil(error, "error should be nil")
            resetCallback.fulfill()
        }
        self.wait(for: [resetCallback], timeout: 10)

        healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        cuttlefishContext.checkOctagonHealth(false) { error in
            XCTAssertNotNil(error, "error should not be nil")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        #if !os(tvOS)
        XCTAssertEqual(cuttlefishContext.postedRepairCFU, true, "Should have posted a CFU")
        #else
        XCTAssertFalse(cuttlefishContext.postedRepairCFU, "aTV should not have posted a CFU, as there's no iphone to recover from")
        #endif
    }

    func responseTestsSetup() throws -> (OTCuttlefishContext, String) {
        let containerName = OTCKContainerName
        let contextName = OTDefaultContext

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        let originalCliqueIdentifier = clique.cliqueMemberIdentifier

        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName)
            XCTAssertEqual(2, accountState.trustState.rawValue, "saved account should be trusted")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        // Reset any CFUs we've done so far
        self.otFollowUpController.postedFollowUp = false

        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(containerName, context: contextName, skipRateLimitingCheck: false) { error in
            XCTAssertNil(error, "error should be nil")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(withContainer: containerName, context: contextName) {
            dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? Dictionary<String, AnyObject>
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        self.verifyDatabaseMocks()
        self.assertEnters(context: cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        return (cuttlefishContext, originalCliqueIdentifier!)
    }

    func testCuttlefishResponseNoAction() throws {
        self.fakeCuttlefishServer.returnNoActionResponse = true
        let (cuttlefishContext, _) = try responseTestsSetup()
        XCTAssertFalse(self.otFollowUpController.postedFollowUp, "should not have posted a CFU")
        XCTAssertEqual(cuttlefishContext.postedRepairCFU, false, "should not have posted a CFU")
    }

    func testCuttlefishResponseRepairAccount() throws {
        self.fakeCuttlefishServer.returnRepairAccountResponse = true
        let (_, _) = try responseTestsSetup()
        XCTAssertTrue(self.otFollowUpController.postedFollowUp, "should have posted a CFU")
    }

    func testCuttlefishResponseRepairEscrow() throws {
        self.fakeCuttlefishServer.returnRepairEscrowResponse = true
        OTMockSecEscrowRequest.self.populateStatuses = false
        let (cuttlefishContext, _) = try responseTestsSetup()
        XCTAssertTrue(self.otFollowUpController.postedFollowUp, "should have posted a CFU")
        XCTAssertEqual(cuttlefishContext.postedEscrowRepairCFU, true, "should have posted an escrow CFU")
    }

    func testCuttlefishResponseResetOctagon() throws {
        let contextName = OTDefaultContext
        let containerName = OTCKContainerName
        self.fakeCuttlefishServer.returnResetOctagonResponse = true
        let (cuttlefishContext, cliqueIdentifier) = try responseTestsSetup()

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        var newCliqueIdentifier: String?
        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(withContainer: containerName, context: contextName) {
            dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? Dictionary<String, AnyObject>
            newCliqueIdentifier = egoSelf!["peerID"]! as? String
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)
        self.assertEnters(context: cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        XCTAssertNotEqual(cliqueIdentifier, newCliqueIdentifier, "should have reset octagon")

        self.verifyDatabaseMocks()
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testCuttlefishResponseError() throws {
        self.fakeCuttlefishServer.returnRepairErrorResponse = FakeCuttlefishServer.makeCloudKitCuttlefishError(code: .changeTokenExpired)

        let (cuttlefishContext, _) = try responseTestsSetup()
        XCTAssertEqual(cuttlefishContext.postedRepairCFU, false, "should not have posted an account repair CFU")
        XCTAssertEqual(cuttlefishContext.postedEscrowRepairCFU, false, "should not have posted an escrow repair CFU")
    }

    func testHealthCheckBeforeStateMachineStarts() throws {
        let contextName = OTDefaultContext
        let containerName = OTCKContainerName
        let cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: contextName)

        cuttlefishContext.stateMachine.setWatcherTimeout(2 * NSEC_PER_SEC)

        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(containerName, context: contextName, skipRateLimitingCheck: false) { error in
            XCTAssertNotNil(error, "Should be an error calling 'healthCheck'")
            XCTAssertEqual(error!._domain, CKKSResultErrorDomain, "Error domain should be CKKSResultErrorDomain")
            XCTAssertEqual(error!._code, CKKSResultTimedOut, "Error result should be CKKSResultTimedOut")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)
        self.startCKAccountStatusMock()

        cuttlefishContext.stateMachine.setWatcherTimeout(60 * NSEC_PER_SEC)

        cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let otcliqueContext = OTConfigurationContext()
        otcliqueContext.context = contextName
        otcliqueContext.dsid = "1234"
        otcliqueContext.altDSID = self.mockAuthKit.altDSID!
        otcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: cuttlefishContext)

        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName)
            XCTAssertEqual(2, accountState.trustState.rawValue, "saved account should be trusted")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(withContainer: containerName, context: contextName) {
            dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? Dictionary<String, AnyObject>
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        self.verifyDatabaseMocks()
        self.assertEnters(context: cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testHealthCheckAfterCloudKitAccountStateChange() throws {
        let containerName = OTCKContainerName
        let contextName = OTDefaultContext

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(containerName, context: contextName, skipRateLimitingCheck: false) { error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error! as NSError).code, 30, "Error code should be 30")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)
        self.assertEnters(context: cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName)
            XCTAssertEqual(2, accountState.trustState.rawValue, "saved account should be trusted")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(withContainer: containerName, context: contextName) {
            dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? Dictionary<String, AnyObject>
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        self.verifyDatabaseMocks()
        self.assertEnters(context: cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testHealthCheckAfterLandingInUntrusted() throws {
        let containerName = OTCKContainerName
        let contextName = OTDefaultContext

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(containerName, context: contextName, skipRateLimitingCheck: false) { error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error! as NSError).code, 30, "Error code should be 30")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName)
            XCTAssertEqual(2, accountState.trustState.rawValue, "saved account should be trusted")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(withContainer: containerName, context: contextName) {
            dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? Dictionary<String, AnyObject>
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        self.verifyDatabaseMocks()
        self.assertEnters(context: cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testHealthCheckNoAccount() throws {
        // Device is signed out
        self.mockAuthKit.altDSID = nil
        let containerName = OTCKContainerName
        let contextName = OTDefaultContext

        // but CK is going to win the race, and tell us everything is fine first
        self.accountStatus = .noAccount
        self.startCKAccountStatusMock()
        self.accountStateTracker.notifyCKAccountStatusChangeAndWaitForSignal()

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        let cuttlefishContext = self.manager.context(forContainerName: containerName, contextID: contextName)
        cuttlefishContext.stateMachine.setWatcherTimeout(2 * NSEC_PER_SEC)

        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(containerName, context: contextName, skipRateLimitingCheck: false) { error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error! as NSError).code, 3, "Error code should be 3")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
    }

    func testHealthCheckWhenLocked() throws {

        let containerName = OTCKContainerName
        let contextName = OTDefaultContext

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName)
            XCTAssertEqual(2, accountState.trustState.rawValue, "saved account should be trusted")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        self.aksLockState = true
        self.lockStateTracker.recheck()

        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(containerName, context: contextName, skipRateLimitingCheck: false) { error in
            XCTAssertNil(error, "error should be nil")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(withContainer: containerName, context: contextName) {
            dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? Dictionary<String, AnyObject>
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        self.verifyDatabaseMocks()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForUnlock, within: 10 * NSEC_PER_SEC)
    }

    func testLastHealthCheckPersistedTime() throws {
        let containerName = OTCKContainerName
        let contextName = OTDefaultContext

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName)
            XCTAssertEqual(2, accountState.trustState.rawValue, "saved account should be trusted")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        var before: UInt64 = 0
        var healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(containerName, context: contextName, skipRateLimitingCheck: false) { error in
            XCTAssertNil(error, "error should be nil")
            do {
                let state = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext)
                XCTAssertNotNil(state)
                XCTAssertNotNil(state.lastHealthCheckup, "last Health Check should not be nil")
                before = state.lastHealthCheckup
            } catch {
                XCTFail("error loading from keychain: \(error)")
            }
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(withContainer: containerName, context: contextName) {
            dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? Dictionary<String, AnyObject>
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        self.verifyDatabaseMocks()
        self.assertEnters(context: cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        sleep(5)

        healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(containerName, context: contextName, skipRateLimitingCheck: false) { error in
            XCTAssertNil(error, "error should be nil")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        var after: UInt64 = 0
        do {
            let state = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext)
            XCTAssertNotNil(state)
            XCTAssertNotNil(state.lastHealthCheckup, "last Health Check should not be nil")
            after = state.lastHealthCheckup
        } catch {
            XCTFail("error loading from keychain: \(error)")
        }

        XCTAssertEqual(before, after, "time stamp should not have changed")

        var healthCheckMinusTwoDays: UInt64 = 0
        //update the last health check to something way in the past
        do {
            let state = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext)
            state.lastHealthCheckup = state.lastHealthCheckup - 172800000 /* 2 days of seconds * 1000*/
            healthCheckMinusTwoDays = state.lastHealthCheckup
            XCTAssertNoThrow(try state.saveToKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext), "saving to the keychain should work")
        } catch {
            XCTFail("error loading from keychain: \(error)")
        }

        sleep(2)

        //check health again, should be updated
        var updatedHealthCheck: UInt64 = 0
        healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(containerName, context: contextName, skipRateLimitingCheck: false) { error in
            XCTAssertNil(error, "error should be nil")
            do {
                let state = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext)
                XCTAssertNotNil(state)
                XCTAssertNotNil(state.lastHealthCheckup, "last Health Check should not be nil")
                updatedHealthCheck = state.lastHealthCheckup
            } catch {
                XCTFail("error loading from keychain: \(error)")
            }
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        XCTAssertTrue(updatedHealthCheck > healthCheckMinusTwoDays, "time stamp should have changed")
    }

    func testHealthCheckAfterFailedJoinWithVoucher() throws {
        let initiatorContextID = "initiator-context-id"
        let bottlerContext = self.manager.context(forContainerName: OTCKContainerName,
                                                  contextID: initiatorContextID,
                                                  sosAdapter: self.mockSOSAdapter,
                                                  authKitAdapter: self.mockAuthKit2,
                                                  lockStateTracker: self.lockStateTracker,
                                                  accountStateTracker: self.accountStateTracker,
                                                  deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1",
                                                                                                    deviceName: "test-bottler-iphone-2",
                                                                                                    serialNumber: "456",
                                                                                                    osVersion: "iOS (fake version)"))

        bottlerContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        self.assertEnters(context: bottlerContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = initiatorContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = self.mockAuthKit.altDSID!
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: bottlerContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: bottlerContext)

        let entropy = try self.loadSecret(label: clique.cliqueMemberIdentifier!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchy(inCloudKit: self.manateeZoneID)
        try self.putSelfTLKShareInCloudKit(context: bottlerContext, zoneID: self.manateeZoneID)

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // cheat: a bottle restore can only succeed after a fetch occurs
        self.sendContainerChange(context: self.cuttlefishContext)

        // Before you call joinWithBottle, you need to call fetchViableBottles.
        let fetchViableExpectation = self.expectation(description: "fetchViableBottles callback occurs")
        self.cuttlefishContext.rpcFetchAllViableBottles { viable, _, error in
            XCTAssertNil(error, "should be no error fetching viable bottles")
            XCTAssert(viable?.contains(bottle.bottleID) ?? false, "The bottle we're about to restore should be viable")
            fetchViableExpectation.fulfill()
        }
        self.wait(for: [fetchViableExpectation], timeout: 10)

        let ckError = FakeCuttlefishServer.makeCloudKitCuttlefishError(code: .resultGraphNotFullyReachable)
        self.fakeCuttlefishServer.nextJoinErrors.append(ckError)

        let joinListenerExpectation = self.expectation(description: "joinWithVoucherExpectation callback occurs")
        self.fakeCuttlefishServer.joinListener = { request in
            joinListenerExpectation.fulfill()
            return nil
        }

        let healthExpectation = self.expectation(description: "health check callback occurs")
        self.fakeCuttlefishServer.healthListener = { request in
            healthExpectation.fulfill()
            return nil
        }

        let joinWithBottleExpectation = self.expectation(description: "joinWithBottle callback occurs")
        self.cuttlefishContext.join(withBottle: bottle.bottleID, entropy: entropy!, bottleSalt: self.otcliqueContext.altDSID) { error in
            XCTAssertNotNil(error, "error should not be nil")
            joinWithBottleExpectation.fulfill()
        }

        self.wait(for: [joinWithBottleExpectation], timeout: 100)
        self.wait(for: [joinListenerExpectation], timeout: 100)
        self.wait(for: [healthExpectation], timeout: 100)
        self.fakeCuttlefishServer.joinListener = nil
        self.fakeCuttlefishServer.healthListener = nil

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
    }

    func testCuttlefishDontPostEscrowCFUDueToPendingPrecord() throws {
        self.fakeCuttlefishServer.returnRepairEscrowResponse = true
        OTMockSecEscrowRequest.self.populateStatuses = true
        let (cuttlefishContext, _) = try responseTestsSetup()
        XCTAssertEqual(cuttlefishContext.postedEscrowRepairCFU, false, "should NOT have posted an escrow CFU")
    }

    func testHealthCheckWhileLocked() throws {
        let containerName = OTCKContainerName
        let contextName = OTDefaultContext

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        self.aksLockState = true
        self.lockStateTracker.recheck()

        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(containerName, context: contextName, skipRateLimitingCheck: false) { error in
            XCTAssertNil(error, "error should be nil")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        self.verifyDatabaseMocks()
        self.assertEnters(context: cuttlefishContext, state: OctagonStateWaitForUnlock, within: 10 * NSEC_PER_SEC)
    }

    func testHealthCheckWhileSA() throws {
        // Account is SA
        self.mockAuthKit.hsa2 = false

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForHSA2, within: 10 * NSEC_PER_SEC)

        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(OTCKContainerName, context: OTDefaultContext, skipRateLimitingCheck: false) { error in
            XCTAssertNil(error, "error should be nil")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        self.assertEnters(context: cuttlefishContext, state: OctagonStateWaitForHSA2, within: 10 * NSEC_PER_SEC)
    }
}

#endif
