#if OCTAGON

class OctagonHealthCheckTests: OctagonTestsBase {
    func testHealthCheckAllTrusted() throws {
        let containerName = OTCKContainerName
        let contextName = OTDefaultContext

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
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
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            XCTAssertEqual(2, accountState.trustState.rawValue, "saved account should be trusted")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(OTControlArguments(configuration: self.otcliqueContext), skipRateLimitingCheck: false, repair: false) { response, error in
            XCTAssertNotNil(response, "response should not be nil")
            XCTAssertNil(error, "error should be nil")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
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

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.cuttlefishContext.checkOctagonHealth(false, repair: false) { response, error in
            XCTAssertNil(response, "response should be nil")
            XCTAssertNil(error, "should be no error")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        #if os(tvOS)
        XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "Should not have posted a CFU on aTV")
        #else
        XCTAssertTrue(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "Should have posted a CFU (due to being untrusted)")
        #endif

        self.verifyDatabaseMocks()
        self.assertEnters(context: cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // Reset flag for remainder of test
        self.cuttlefishContext.followupHandler.clearAllPostedFlags()

        // Set the "have I attempted to join" bit; TVs should still not CFU, but other devices should
        try! self.cuttlefishContext.accountMetadataStore.persistOctagonJoinAttempt(.ATTEMPTED)

        let healthCheckCallback2 = self.expectation(description: "healthCheckCallback callback occurs")
        cuttlefishContext.checkOctagonHealth(false, repair: false) { response, error in
            XCTAssertNil(response, "response should be nil")
            XCTAssertNil(error, "should be no error")
            healthCheckCallback2.fulfill()
        }
        self.wait(for: [healthCheckCallback2], timeout: 10)

        #if os(tvOS)
        XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "Should not have posted a CFU on aTV")
        #else
        XCTAssertTrue(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "Should have posted a CFU")
        #endif
    }

    func testHealthCheckSecurityDStateNOTTrusted() throws {
        let containerName = OTCKContainerName
        let contextName = OTDefaultContext

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
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

        // now let's ruin account state, and say we are untrusted
        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            accountState.trustState = OTAccountMetadataClassC_TrustState(rawValue: 1)!
            try accountState.saveToKeychain(forContainer: containerName, contextID: contextName, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            XCTAssertEqual(1, accountState.trustState.rawValue, "Saved account state should have the same peer ID that prepare returned")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        cuttlefishContext.checkOctagonHealth(false, repair: false) { response, error in
            XCTAssertNotNil(response, "response should not be nil")
            XCTAssertNil(error, "error should be nil")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        self.assertEnters(context: cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: cuttlefishContext)

        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
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

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
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
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            XCTAssertEqual(2, accountState.trustState.rawValue, "Saved account state should be trusted")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        var healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        cuttlefishContext.checkOctagonHealth(false, repair: false) { response, error in
            XCTAssertNotNil(response, "response should not be nil")
            XCTAssertNil(error, "error should be nil")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        // now lets completely wipe cuttlefish state
        let resetCallback = self.expectation(description: "resetCallback callback occurs")
        self.tphClient.localReset(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { error in
            XCTAssertNil(error, "error should be nil")
            resetCallback.fulfill()
        }
        self.wait(for: [resetCallback], timeout: 10)

        healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        cuttlefishContext.checkOctagonHealth(false, repair: false) { response, error in
            XCTAssertNil(response, "response should be nil")
            XCTAssertNil(error, "error should be nil")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        #if !os(tvOS)
        XCTAssertTrue(cuttlefishContext.followupHandler.hasPosted(.stateRepair), "Should have posted a CFU")
        #else
        XCTAssertFalse(cuttlefishContext.followupHandler.hasPosted(.stateRepair), "aTV should not have posted a CFU, as there's no iphone to recover from")
        #endif
    }

    func responseTestsSetup(expectedState: String, expectedResponse: Bool = false) throws -> (OTCuttlefishContext, String) {
        let containerName = OTCKContainerName
        let contextName = OTDefaultContext

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
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
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            XCTAssertEqual(2, accountState.trustState.rawValue, "saved account should be trusted")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        // Reset any CFUs we've done so far
        self.otFollowUpController.postedFollowUp = false
        self.cuttlefishContext.followupHandler.clearAllPostedFlags()

        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(OTControlArguments(configuration: self.otcliqueContext), skipRateLimitingCheck: false, repair: false) { response, error in
            if expectedResponse {
                XCTAssertNotNil(response, "response should be nil")
            } else {
                XCTAssertNil(response, "response should be nil")
            }
            XCTAssertNil(error, "error should be nil")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        self.verifyDatabaseMocks()
        self.assertEnters(context: cuttlefishContext, state: expectedState, within: 10 * NSEC_PER_SEC)

        return (cuttlefishContext, originalCliqueIdentifier!)
    }

    func testCuttlefishResponseNoAction() throws {
        self.fakeCuttlefishServer.returnNoActionResponse = true
        let (cuttlefishContext, _) = try responseTestsSetup(expectedState: OctagonStateReady, expectedResponse: true)
        XCTAssertFalse(self.otFollowUpController.postedFollowUp, "should not have posted a CFU")
        XCTAssertFalse(cuttlefishContext.followupHandler.hasPosted(.stateRepair), "should not have posted a Repair CFU")
    }

    func testCuttlefishResponseRepairAccount() throws {
        self.fakeCuttlefishServer.returnRepairAccountResponse = true
        let (_, _) = try responseTestsSetup(expectedState: OctagonStateReady, expectedResponse: true)
        XCTAssertTrue(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "should have posted a Repair CFU")
    }

    func testCuttlefishResponseRepairEscrow() throws {
        self.fakeCuttlefishServer.returnRepairEscrowResponse = true
        OTMockSecEscrowRequest.self.populateStatuses = false
        let (cuttlefishContext, _) = try responseTestsSetup(expectedState: OctagonStateReady, expectedResponse: true)
        XCTAssertTrue(self.otFollowUpController.postedFollowUp, "should have posted a CFU")
        XCTAssertTrue(cuttlefishContext.followupHandler.hasPosted(.confirmExistingSecret), "should have posted an escrow CFU")
    }

    func testCuttlefishResponseResetOctagon() throws {
        self.fakeCuttlefishServer.returnResetOctagonResponse = true
        let (cuttlefishContext, cliqueIdentifier) = try responseTestsSetup(expectedState: OctagonStateReady, expectedResponse: true)

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        var newCliqueIdentifier: String?
        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
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

    func testCuttlefishResponseLeaveTrust() throws {
        OctagonSetSOSFeatureEnabled(false)

        self.fakeCuttlefishServer.returnLeaveTrustResponse = true
        let (_, _) = try responseTestsSetup(expectedState: OctagonStateUntrusted, expectedResponse: true)

        self.verifyDatabaseMocks()
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTrust, within: 10 * NSEC_PER_SEC)
    }

    func testCuttlefishResponseError() throws {
        self.fakeCuttlefishServer.returnRepairErrorResponse = FakeCuttlefishServer.makeCloudKitCuttlefishError(code: .changeTokenExpired)

        let (cuttlefishContext, _) = try responseTestsSetup(expectedState: OctagonStateReady)
        XCTAssertFalse(cuttlefishContext.followupHandler.hasPosted(.stateRepair), "should not have posted an account repair CFU")
        XCTAssertFalse(cuttlefishContext.followupHandler.hasPosted(.confirmExistingSecret), "should not have posted an escrow repair CFU")
    }

    func testHealthCheckBeforeStateMachineStarts() throws {
        let contextName = OTDefaultContext
        let containerName = OTCKContainerName
        let cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: contextName)

        cuttlefishContext.stateMachine.setWatcherTimeout(2 * NSEC_PER_SEC)

        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(OTControlArguments(configuration: self.otcliqueContext), skipRateLimitingCheck: false, repair: false) { response, error in
            XCTAssertNotNil(error, "Should be an error calling 'healthCheck'")
            XCTAssertEqual(error!._domain, CKKSResultErrorDomain, "Error domain should be CKKSResultErrorDomain")
            XCTAssertEqual(error!._code, CKKSResultTimedOut, "Error result should be CKKSResultTimedOut")
            XCTAssertNil(response, "response should be nil")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)
        self.startCKAccountStatusMock()

        cuttlefishContext.stateMachine.setWatcherTimeout(60 * NSEC_PER_SEC)

        cuttlefishContext.startOctagonStateMachine()

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let otcliqueContext = self.createOTConfigurationContextForTests(contextID: contextName,
                                                            otControl: self.otControl,
                                                            altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))
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
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            XCTAssertEqual(2, accountState.trustState.rawValue, "saved account should be trusted")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
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

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(OTControlArguments(configuration: self.otcliqueContext), skipRateLimitingCheck: false, repair: false) { response, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNil(response, "response should be nil")
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
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            XCTAssertEqual(2, accountState.trustState.rawValue, "saved account should be trusted")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
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

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(OTControlArguments(configuration: self.otcliqueContext), skipRateLimitingCheck: false, repair: false) { response, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNil(response, "response should be nil")
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
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            XCTAssertEqual(2, accountState.trustState.rawValue, "saved account should be trusted")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        self.verifyDatabaseMocks()
        self.assertEnters(context: cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testHealthCheckNoAccount() throws {
        // Device is signed out
        self.mockAuthKit.removePrimaryAccount()
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
        self.manager.healthCheck(OTControlArguments(configuration: self.otcliqueContext), skipRateLimitingCheck: false, repair: false) { response, error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual((error! as NSError).code, OctagonError.noAppleAccount.rawValue, "Error code should be NoAppleAccount")
            XCTAssertNil(response, "response should be nil")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
    }

    func testHealthCheckWaitingForCDP() throws {
        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)

        self.cuttlefishContext.stateMachine.setWatcherTimeout(2 * NSEC_PER_SEC)

        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(OTControlArguments(configuration: self.otcliqueContext), skipRateLimitingCheck: false, repair: false) { response, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNil(response, "response should be nil")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)
    }

    func testHealthCheckRecoversFromWrongWaitingForCDP() throws {
        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)

        // Now, another client creates the circle, but we miss the push
        let remote = self.makeInitiatorContext(contextID: "remote")
        self.assertResetAndBecomeTrusted(context: remote)

        // Now, does the health check get us into Untrusted?
        self.cuttlefishContext.stateMachine.setWatcherTimeout(2 * NSEC_PER_SEC)

        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(OTControlArguments(configuration: self.otcliqueContext), skipRateLimitingCheck: false, repair: false) { response, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNil(response, "response should be nil")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        #if !os(tvOS)
        XCTAssertTrue(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "Octagon should have posted a repair CFU after the health check")
        #else
        XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "posted should be false on tvOS; there aren't any iphones around to repair it")
        #endif
    }

    func testHealthCheckWhenLocked() throws {
        let containerName = OTCKContainerName
        let contextName = OTDefaultContext

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
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
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            XCTAssertEqual(2, accountState.trustState.rawValue, "saved account should be trusted")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        self.aksLockState = true
        self.lockStateTracker.recheck()

        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(OTControlArguments(configuration: self.otcliqueContext), skipRateLimitingCheck: false, repair: false) { response, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNil(response, "response should be nil")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        self.verifyDatabaseMocks()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForUnlock, within: 10 * NSEC_PER_SEC)
    }

    func testHealthCheckBeforeClassCUnlock() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        // Now, the device restarts, and isn't unlocked immediately.
        self.aksLockState = true
        SecMockAKS.lockClassA_C()
        self.lockStateTracker.recheck()

        do {
            _ = try OTAccountMetadataClassC.loadFromKeychain(forContainer: self.cuttlefishContext.containerName, contextID: self.cuttlefishContext.contextID, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            XCTFail("shouldn't have been able to load the class c metadata")
        } catch {
            // We expected this error; fall through
        }

        self.cuttlefishContext = self.simulateRestart(context: self.cuttlefishContext)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForClassCUnlock, within: 10 * NSEC_PER_SEC)

        // A health check should fail, and leave us waiting for the initial unlock
        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(OTControlArguments(configuration: self.otcliqueContext), skipRateLimitingCheck: false, repair: false) { response, error in
            XCTAssertNotNil(error, "error should be present")
            XCTAssertNil(response, "response should be nil")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForClassCUnlock, within: 10 * NSEC_PER_SEC)
    }

    func testLastHealthCheckPersistedTime() throws {
        let containerName = OTCKContainerName
        let contextName = OTDefaultContext

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
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
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            XCTAssertEqual(2, accountState.trustState.rawValue, "saved account should be trusted")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        var before: UInt64 = 0
        var healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(OTControlArguments(configuration: self.otcliqueContext), skipRateLimitingCheck: false, repair: false) { response, error in
            XCTAssertNil(error, "error should be nil")
            do {
                let state = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
                XCTAssertNotNil(state)
                XCTAssertNotNil(state.lastHealthCheckup, "last Health Check should not be nil")
                before = state.lastHealthCheckup
            } catch {
                XCTFail("error loading from keychain: \(error)")
            }
            XCTAssertNotNil(response, "response should not be nil")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        self.verifyDatabaseMocks()
        self.assertEnters(context: cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        sleep(5)

        healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(OTControlArguments(configuration: self.otcliqueContext), skipRateLimitingCheck: false, repair: false) { response, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNil(response, "response should be nil")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        var after: UInt64 = 0
        do {
            let state = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            XCTAssertNotNil(state)
            XCTAssertNotNil(state.lastHealthCheckup, "last Health Check should not be nil")
            after = state.lastHealthCheckup
        } catch {
            XCTFail("error loading from keychain: \(error)")
        }

        XCTAssertEqual(before, after, "time stamp should not have changed")

        var healthCheckMinusTwoDays: UInt64 = 0
        // update the last health check to something way in the past
        do {
            let state = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            state.lastHealthCheckup -= 172800000 /* 2 days of seconds * 1000*/
            healthCheckMinusTwoDays = state.lastHealthCheckup
            XCTAssertNoThrow(try state.saveToKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil), "saving to the keychain should work")
        } catch {
            XCTFail("error loading from keychain: \(error)")
        }

        sleep(2)

        // check health again, should be updated
        var updatedHealthCheck: UInt64 = 0
        healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(OTControlArguments(configuration: self.otcliqueContext), skipRateLimitingCheck: false, repair: false) { response, error in
            XCTAssertNil(error, "error should be nil")
            do {
                let state = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
                XCTAssertNotNil(state)
                XCTAssertNotNil(state.lastHealthCheckup, "last Health Check should not be nil")
                updatedHealthCheck = state.lastHealthCheckup
            } catch {
                XCTFail("error loading from keychain: \(error)")
            }
            XCTAssertNotNil(response, "response should not be nil")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        XCTAssertTrue(updatedHealthCheck > healthCheckMinusTwoDays, "time stamp should have changed")
    }

    func testHealthCheckAfterFailedJoinWithVoucher() throws {
        let initiatorContextID = "initiator-context-id"
        let bottlerContext = self.manager.context(forContainerName: OTCKContainerName,
                                                  contextID: initiatorContextID,
                                                  sosAdapter: self.mockSOSAdapter!,
                                                  accountsAdapter: self.mockAuthKit2,
                                                  authKitAdapter: self.mockAuthKit2,
                                                  tooManyPeersAdapter: self.mockTooManyPeers,
                                                  tapToRadarAdapter: self.mockTapToRadar,
                                                  lockStateTracker: self.lockStateTracker,
                                                  deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1",
                                                                                                    deviceName: "test-bottler-iphone-2",
                                                                                                    serialNumber: "456",
                                                                                                    osVersion: "iOS (fake version)"))

        bottlerContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        XCTAssertNoThrow(try bottlerContext.setCDPEnabled())
        self.assertEnters(context: bottlerContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = self.createOTConfigurationContextForTests(contextID: initiatorContextID,
                                                                               otControl: self.otControl,
                                                                               altDSID: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))
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
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: bottlerContext)

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // cheat: a bottle restore can only succeed after a fetch occurs
        self.sendContainerChange(context: self.cuttlefishContext)

        // Before you call joinWithBottle, you need to call fetchViableBottles.
        let fetchViableExpectation = self.expectation(description: "fetchViableBottles callback occurs")
        self.cuttlefishContext.rpcFetchAllViableBottles(from: .default) { viable, _, error in
            XCTAssertNil(error, "should be no error fetching viable bottles")
            XCTAssert(viable?.contains(bottle.bottleID) ?? false, "The bottle we're about to restore should be viable")
            fetchViableExpectation.fulfill()
        }
        self.wait(for: [fetchViableExpectation], timeout: 10)

        let ckError = FakeCuttlefishServer.makeCloudKitCuttlefishError(code: .resultGraphNotFullyReachable)
        self.fakeCuttlefishServer.nextJoinErrors.append(ckError)

        let joinListenerExpectation = self.expectation(description: "joinWithVoucherExpectation callback occurs")
        self.fakeCuttlefishServer.joinListener = { _ in
            joinListenerExpectation.fulfill()
            return nil
        }

        let healthExpectation = self.expectation(description: "health check callback occurs")
        self.fakeCuttlefishServer.healthListener = { _ in
            healthExpectation.fulfill()
            return nil
        }

        let joinWithBottleExpectation = self.expectation(description: "joinWithBottle callback occurs")
        self.cuttlefishContext.join(withBottle: bottle.bottleID, entropy: entropy!, bottleSalt: self.otcliqueContext.altDSID!) { error in
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
        let (cuttlefishContext, _) = try responseTestsSetup(expectedState: OctagonStateReady, expectedResponse: true)
        XCTAssertFalse(cuttlefishContext.followupHandler.hasPosted(.confirmExistingSecret), "should NOT have posted an escrow CFU")
    }

    func testHealthCheckWhileLocked() throws {
        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
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
        self.manager.healthCheck(OTControlArguments(configuration: self.otcliqueContext), skipRateLimitingCheck: false, repair: false) { response, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNil(response, "response should be nil")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        self.verifyDatabaseMocks()
        self.assertEnters(context: cuttlefishContext, state: OctagonStateWaitForUnlock, within: 10 * NSEC_PER_SEC)
    }

    func testHealthCheckWhileSA() throws {
        // Account is SA
        let primaryAccount = try XCTUnwrap(self.mockAuthKit.primaryAccount())
        let account = CloudKitAccount(altDSID: primaryAccount.altDSID, persona: nil, hsa2: false, demo: false, accountStatus: .available)
        self.mockAuthKit.add(account)

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDPCapableSecurityLevel, within: 10 * NSEC_PER_SEC)

        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(OTControlArguments(configuration: self.otcliqueContext), skipRateLimitingCheck: false, repair: false) { response, error in
            XCTAssertNil(response, "response should be nil")
            XCTAssertNotNil(error, "should have an error when health-checking when Octagon thinks the account is SA")
            if let error = error as? NSError {
                XCTAssertEqual(error.domain, OctagonErrorDomain, "error domain should be Octagon")
                XCTAssertEqual(error.code, OctagonError.unsupportedAccount.rawValue, "error code should complain about account type")
            }
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        self.assertEnters(context: cuttlefishContext, state: OctagonStateWaitForCDPCapableSecurityLevel, within: 10 * NSEC_PER_SEC)
    }

    func testHealthCheckWhileSAAfterMissingHSA2Notification() throws {
        // Account is SA
        let primaryAccount = try XCTUnwrap(self.mockAuthKit.primaryAccount())
        self.mockAuthKit.add(CloudKitAccount(altDSID: primaryAccount.altDSID, persona: nil, hsa2: false, demo: false, accountStatus: .available))
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDPCapableSecurityLevel, within: 10 * NSEC_PER_SEC)

        // Not CDP-capable means no CKKS syncing policy is loaded
        XCTAssertNil(self.defaultCKKS.syncingPolicy)

        // the account becomes HSA2, but we miss the notification for whatever reason
        self.mockAuthKit.removePrimaryAccount()
        self.mockAuthKit.add(CloudKitAccount(altDSID: primaryAccount.altDSID, persona: nil, hsa2: true, demo: false, accountStatus: .available))

        // The health check fires...
        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(OTControlArguments(configuration: self.otcliqueContext), skipRateLimitingCheck: false, repair: false) { result, error in
            XCTAssertNil(result, "should have no result")
            XCTAssertNotNil(error, "should have an error when health-checking when Octagon thinks the account is SA")
            if let error = error as? NSError {
                XCTAssertEqual(error.domain, OctagonErrorDomain, "error domain should be Octagon")
                XCTAssertEqual(error.code, OctagonError.unsupportedAccount.rawValue, "error code should complain about account type")
            }
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 30)

        self.assertEnters(context: cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)
        XCTAssertNotNil(self.defaultCKKS.syncingPolicy, "CKKS syncing policy should be loaded after health check finds a new account")
    }

    func testRPCTrustStatusReturnsIsLocked() throws {
        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
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

        let configuration = OTOperationConfiguration()

        let statusexpectation = self.expectation(description: "status callback occurs")
        self.cuttlefishContext.rpcTrustStatus(configuration) { egoStatus, egoPeerID, _, _, isLocked, error   in
            XCTAssertEqual(egoStatus, .in, "Self peer for OTDefaultContext should be trusted")
            XCTAssertNotNil(egoPeerID, "Should have a peerID")
            XCTAssert(isLocked, "should be locked")
            XCTAssertNil(error, "error should be nil")
            statusexpectation.fulfill()
        }
        self.wait(for: [statusexpectation], timeout: 10)

        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(OTControlArguments(configuration: self.otcliqueContext), skipRateLimitingCheck: false, repair: false) { response, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNil(response, "response should be nil")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForUnlock, within: 10 * NSEC_PER_SEC)
    }

    func testBecomeUntrustedResultsInWaitForUnlock() throws {
        self.startCKAccountStatusMock()

        // First, peer 1 establishes, preapproving both peer2 and peer3. Then, peer2 and peer3 join and harmonize.
        // Peer1 is never told about the follow-on joins.
        // Then, the test can begin.

        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCInCircle)

        let peer2SOSMockPeer = self.createSOSPeer(peerID: "peer2ID")
        let peer3SOSMockPeer = self.createSOSPeer(peerID: "peer3ID")

        self.mockSOSAdapter!.trustedPeers.add(peer2SOSMockPeer)
        self.mockSOSAdapter!.trustedPeers.add(peer3SOSMockPeer)

        // Due to how everything is shaking out, SOS TLKShares will be uploaded in a second transaction after Octagon uploads its TLKShares
        // This isn't great: <rdar://problem/49080104> Octagon: upload SOS TLKShares alongside initial key hierarchy
        self.assertAllCKKSViewsUpload(tlkShares: 3)

        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        let peer1ID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // peer2
        let peer2mockSOS = CKKSMockSOSPresentAdapter(selfPeer: peer2SOSMockPeer, trustedPeers: self.mockSOSAdapter!.allPeers(), essential: false)
        let peer2 = self.makeInitiatorContext(contextID: "peer2", authKitAdapter: self.mockAuthKit2, sosAdapter: peer2mockSOS)

        peer2.startOctagonStateMachine()
        self.assertEnters(context: peer2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: peer2)
        let peer2ID = try peer2.accountMetadataStore.getEgoPeerID()

        // peer3
        let peer3mockSOS = CKKSMockSOSPresentAdapter(selfPeer: peer3SOSMockPeer, trustedPeers: self.mockSOSAdapter!.allPeers(), essential: false)
        let peer3 = self.makeInitiatorContext(contextID: "peer3", authKitAdapter: self.mockAuthKit3, sosAdapter: peer3mockSOS)

        peer3.startOctagonStateMachine()
        self.assertEnters(context: peer3, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: peer3)
        let peer3ID = try peer3.accountMetadataStore.getEgoPeerID()

        // Now, tell peer2 about peer3's join
        self.sendContainerChangeWaitForFetch(context: peer2)

        // Peer 1 should preapprove both peers.
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trustsByPreapproval, target: peer2ID)),
                      "peer 1 should trust peer 2 by preapproval")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trustsByPreapproval, target: peer2ID)),
                      "peer 1 should trust peer 3 by preapproval")

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer2ID, opinion: .trusts, target: peer1ID)),
                      "peer 2 should trust peer 1")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer2ID, opinion: .trusts, target: peer3ID)),
                      "peer 2 should trust peer 3")

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer3ID, opinion: .trusts, target: peer1ID)),
                      "peer 3 should trust peer 1")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer3ID, opinion: .trusts, target: peer2ID)),
                      "peer 3 should trust peer 2")

        // Now, the test can begin. Peer2 decides it rules the world.
        let removalExpectation = self.expectation(description: "removal occurs")
        peer2.rpcRemoveFriends(inClique: [peer1ID, peer3ID]) { removeError in
            XCTAssertNil(removeError, "Should be no error removing peer1 and peer3")
            removalExpectation.fulfill()
        }
        self.wait(for: [removalExpectation], timeout: 5)
        self.assertEnters(context: peer2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: peer2)

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer2ID, opinion: .excludes, target: peer1ID)),
                      "peer 2 should distrust peer 1")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer2ID, opinion: .excludes, target: peer3ID)),
                      "peer 2 should distrust peer 3")

        // And we notify peer3 about this, and it should become sad
        let updateTrustExpectation = self.expectation(description: "fetchChanges")

        // Ths push will only be delivered when the device is unlocked, so simulate the device locking during the fetch
        self.fakeCuttlefishServer.fetchChangesListener = { _ in
            self.fakeCuttlefishServer.fetchChangesListener = nil

            self.aksLockState = true
            self.lockStateTracker.recheck()

            updateTrustExpectation.fulfill()
            return nil
        }
        peer3.notifyContainerChange(nil)
        self.wait(for: [updateTrustExpectation], timeout: 10)

        self.assertEnters(context: peer3, state: OctagonStateWaitForUnlock, within: 10 * NSEC_PER_SEC)

        self.aksLockState = false
        self.lockStateTracker.recheck()

        self.assertEnters(context: peer3, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: peer3)
    }

    func testEvaluateTPHOctagonTrustWhenLocked() throws {
        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
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

        // forcing the test to hit a lock state error which should not return an error or a response
        self.pauseOctagonStateMachine(context: self.cuttlefishContext, entering: OctagonStateTPHTrustCheck)

        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(OTControlArguments(configuration: self.otcliqueContext), skipRateLimitingCheck: false, repair: false) { response, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNil(response, "response should be nil")
            healthCheckCallback.fulfill()
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateTPHTrustCheck, within: 10 * NSEC_PER_SEC)

        self.aksLockState = true
        self.lockStateTracker.recheck()
        self.releaseOctagonStateMachine(context: self.cuttlefishContext, from: OctagonStateTPHTrustCheck)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForUnlock, within: 10 * NSEC_PER_SEC)

        self.wait(for: [healthCheckCallback], timeout: 10)

        self.aksLockState = false
        self.lockStateTracker.recheck()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testEvaluateSecdOctagonTrust() throws {
        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        self.assertResetAndBecomeTrustedInDefaultContext()

        // Join with another peer, so that the depart will work
        let joiningContext = self.makeInitiatorContext(contextID: "joiner", authKitAdapter: self.mockAuthKit2)
        self.assertJoinViaEscrowRecoveryFromDefaultContextWithReciprocationAndTLKShares(joiningContext: joiningContext)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        let leaveExpectation = self.expectation(description: "rpcLeaveClique returns")
        self.cuttlefishContext.rpcLeaveClique { leaveError in
            XCTAssertNil(leaveError, "Should be no error leaving")
            leaveExpectation.fulfill()
        }
        self.wait(for: [leaveExpectation], timeout: 10)

        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(OTControlArguments(configuration: self.otcliqueContext), skipRateLimitingCheck: false, repair: false) { response, error in
            XCTAssertNotNil(error, "error should not be nil")

            if let error = error as? NSError {
                XCTAssertEqual(error.domain, OctagonErrorDomain)
                XCTAssertEqual(error.code, OctagonError.unexpectedStateTransition.rawValue, "error should come from state machine")
                let underlyingError = error.userInfo[NSUnderlyingErrorKey] as? NSError
                XCTAssertNotNil(underlyingError, "should be some underlying error")
                XCTAssertEqual(underlyingError?.domain, OctagonStateTransitionErrorDomain, "underlying error should be from the octagon state machine")
                XCTAssertEqual(underlyingError?.code, ((OTStates.octagonStateMap())[OctagonStatePostRepairCFU])?.intValue, "error code should match value in map for unknown state 'PostRepairCFU'")
            } else {
                XCTFail("unable to turn error into NSError")
            }

            XCTAssertNil(response, "response should be nil")
            healthCheckCallback.fulfill()
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStatePostRepairCFU, within: 10 * NSEC_PER_SEC)

        self.aksLockState = true
        self.lockStateTracker.recheck()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForUnlock, within: 10 * NSEC_PER_SEC)

        self.wait(for: [healthCheckCallback], timeout: 10)

        self.aksLockState = false
        self.lockStateTracker.recheck()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
    }

    func testCachedTrustStateUpdate() throws {
        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
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
        try self.cuttlefishContext.accountMetadataStore.persistAccountChanges { metadata in
            metadata.trustState = .UNKNOWN
            return metadata
        }
        let configuration = OTOperationConfiguration()
        configuration.useCachedAccountStatus = true
        let cachedStatusexpectation = self.expectation(description: "(cached) trust status returns")
        self.cuttlefishContext.rpcTrustStatus(configuration) { status, _, _, _, _, _ in
            XCTAssertEqual(CliqueStatus.absent, status)
            cachedStatusexpectation.fulfill()
        }
        self.wait(for: [cachedStatusexpectation], timeout: 10)

        configuration.useCachedAccountStatus = false
        let cachedStatusexpectation2 = self.expectation(description: "trust status returns")
        self.cuttlefishContext.rpcTrustStatus(configuration) { status, _, _, _, _, _ in
            XCTAssertEqual(CliqueStatus.in, status)
            cachedStatusexpectation2.fulfill()
        }
        self.wait(for: [cachedStatusexpectation2], timeout: 10)

        configuration.useCachedAccountStatus = true
        let cachedStatusexpectation3 = self.expectation(description: "trust status returns")
        self.cuttlefishContext.rpcTrustStatus(configuration) { status, _, _, _, _, _ in
            XCTAssertEqual(CliqueStatus.in, status)
            cachedStatusexpectation3.fulfill()
        }
        self.wait(for: [cachedStatusexpectation3], timeout: 10)
    }
}

#endif
