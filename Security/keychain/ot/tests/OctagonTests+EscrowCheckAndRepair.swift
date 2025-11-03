#if OCTAGON

import FeatureFlags

class OctagonEscrowCheckAndRepairTests: OctagonTestsBase {
    func testEscrowCheckTV() throws {
#if !os(tvOS)
        try XCTSkipIf(true, "this test only applies to TV")
#endif
        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        _ = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        let escrowCheckExpectation = self.expectation(description: "escrowCheck callback occurs")
        self.manager.escrowCheck(OTControlArguments(configuration: self.otcliqueContext), isBackgroundCheck: false) { response, error in
            XCTAssertNil(response, "response should be nil")
            XCTAssertNotNil(error, "error should not be nil")
            let nsError = error! as NSError
            XCTAssertEqual(nsError.domain, OctagonErrorDomain, "error domain should match")
            XCTAssertEqual(nsError.code, OctagonError.noEscrowCheckOnTV.rawValue, "error code should match")
            escrowCheckExpectation.fulfill()
        }
        self.wait(for: [escrowCheckExpectation], timeout: 10)
    }

    func testEscrowCheck() throws {
#if os(tvOS)
        try XCTSkipIf(true, "no escrow check on TV")
#endif

        // make sure enum values are all correct
        XCTAssertEqual(EscrowRepairReason.recordRepairReasonUnknown.rawValue, OTEscrowCheckRepairReason.unknown.rawValue)
        XCTAssertEqual(EscrowRepairReason.recordOk.rawValue, OTEscrowCheckRepairReason.recordOK.rawValue)
        XCTAssertEqual(EscrowRepairReason.noRecordMatchingPeer.rawValue, OTEscrowCheckRepairReason.noRecordMatchingPeer.rawValue)
        XCTAssertEqual(EscrowRepairReason.noRecordMatchingPasscodeGeneration.rawValue, OTEscrowCheckRepairReason.noRecordMatchingPasscodeGeneration.rawValue)
        XCTAssertEqual(EscrowRepairReason.noRecordMatchingRecoverable.rawValue, OTEscrowCheckRepairReason.noRecordMatchingRecoverable.rawValue)
        XCTAssertEqual(EscrowRepairReason.recordNeedsMigration.rawValue, OTEscrowCheckRepairReason.recordNeedsMigration.rawValue)

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

        // Do some actual tests
        let escrowCheckAndValidateResponse = { (validateResponse: @escaping (OTEscrowCheckCallResult) -> Void) in
            let escrowCheckCallback = self.expectation(description: "escrowCheck callback occurs")
            self.manager.escrowCheck(OTControlArguments(configuration: self.otcliqueContext), isBackgroundCheck: false) { response, error in
                XCTAssertNotNil(response, "response should not be nil")
                XCTAssertNil(error, "error should be nil")
                if let response {
                    XCTAssertFalse(response.secureTermsNeeded) // TODO: always false, this is a client-side decision (for now)
                    XCTAssertEqual(response.repairReason, self.fakeCuttlefishServer.returnEscrowCheckRepairReason.rawValue)
                    validateResponse(response)
                }
                escrowCheckCallback.fulfill()
            }
            self.wait(for: [escrowCheckCallback], timeout: 10)
            self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        }

        let escrowCheckAndExpectError = {
            let escrowCheckCallback = self.expectation(description: "escrowCheck (error) callback occurs")
            self.manager.escrowCheck(OTControlArguments(configuration: self.otcliqueContext), isBackgroundCheck: false) { response, error in
                XCTAssertNil(response, "response should be nil")
                XCTAssertNotNil(error, "error should not be nil")
                escrowCheckCallback.fulfill()
            }
            self.wait(for: [escrowCheckCallback], timeout: 10)
            self.assertEnters(context: self.cuttlefishContext, state: $0, within: 10 * NSEC_PER_SEC)
        }

        // Lock state shouldn't affect anything, pretend device is locked for all tests.
        self.aksLockState = true

        // Successful check (clear CFUs)
        XCTAssertNoThrow(try self.cuttlefishContext.followupHandler.postFollowUp(.offlinePasscodeChange, activeAccount: try XCTUnwrap(self.cuttlefishContext.activeAccount)))
        XCTAssertTrue(self.cuttlefishContext.followupHandler.hasPosted(.offlinePasscodeChange))
        escrowCheckAndValidateResponse { response in
            XCTAssertFalse(response.needsReenroll)
            XCTAssertTrue(response.octagonTrusted)
            XCTAssertNil(response.moveRequest)
            XCTAssertFalse(SecMockAKS.cacheFlowEnabled())

            // Ensure that CFU is cleared.
            XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.offlinePasscodeChange))
        }
        self.cuttlefishContext.followupHandler.clearAllPostedFlags()

        // Client-side error (should return to Ready state)
        self.fakeCuttlefishServer.returnEscrowCheckError = FakeCuttlefishServer.makeCloudKitCuttlefishError(code: .changeTokenExpired)
        escrowCheckAndExpectError(OctagonStateReady)
        self.fakeCuttlefishServer.returnEscrowCheckError = nil

        // Error from server (don't clear CFUs)
        XCTAssertNoThrow(try self.cuttlefishContext.followupHandler.postFollowUp(.offlinePasscodeChange, activeAccount: try XCTUnwrap(self.cuttlefishContext.activeAccount)))
        XCTAssertTrue(self.cuttlefishContext.followupHandler.hasPosted(.offlinePasscodeChange))
        self.fakeCuttlefishServer.returnEscrowCheckNa = true
        escrowCheckAndValidateResponse { response in
            XCTAssertFalse(response.needsReenroll)
            XCTAssertFalse(response.octagonTrusted)
            XCTAssertNil(response.moveRequest)
            XCTAssertFalse(SecMockAKS.cacheFlowEnabled())

            // Ensure that CFU is NOT cleared on error.
            XCTAssertTrue(self.cuttlefishContext.followupHandler.hasPosted(.offlinePasscodeChange))
        }
        self.cuttlefishContext.followupHandler.clearAllPostedFlags()
        self.fakeCuttlefishServer.returnEscrowCheckNa = false

        // Various federation move tests
        do {
            self.fakeCuttlefishServer.returnEscrowCheckNeedsRepair = true
            self.fakeCuttlefishServer.returnEscrowCheckRepairReason = .recordNeedsMigration
            self.fakeCuttlefishServer.returnEscrowCheckMoveRequest = true

            // Move is allowed (no CFU, cache flow enabled)
            escrowCheckAndValidateResponse { response in
                XCTAssertTrue(response.needsReenroll)
                XCTAssertTrue(response.octagonTrusted)
                XCTAssertNotNil(response.moveRequest)
                XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.secureTerms))
                if isFeatureEnabled(SecurityFeatures.EscrowCheckMigration) {
                    XCTAssertTrue(SecMockAKS.cacheFlowEnabled())
                } else {
                    XCTAssertFalse(SecMockAKS.cacheFlowEnabled())
                }
            }
            SecMockAKS.resetCacheFlow()

            // Terms needed (CFU, cache flow not enabled)
            self.mockSecureBackupAdapter.moveError = NSError(domain: kCloudServicesErrorDomain, code: Int(kCloudServicesMissingSecureTerms.rawValue), userInfo: nil)
            escrowCheckAndValidateResponse { response in
                XCTAssertTrue(response.needsReenroll)
                XCTAssertTrue(response.octagonTrusted)
                XCTAssertNotNil(response.moveRequest)
                if isFeatureEnabled(SecurityFeatures.EscrowCheckMigration) {
                    XCTAssertTrue(self.cuttlefishContext.followupHandler.hasPosted(.secureTerms))
                } else {
                    XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.secureTerms))
                }
                XCTAssertFalse(SecMockAKS.cacheFlowEnabled())
            }
            self.cuttlefishContext.followupHandler.clearAllPostedFlags()
            self.mockSecureBackupAdapter.moveError = nil

            // Error (no CFU, cache flow not enabled)
            self.mockSecureBackupAdapter.moveError = NSError(domain: kCloudServicesErrorDomain, code: Int(kCloudServicesUnknownFederation.rawValue), userInfo: nil)
            escrowCheckAndValidateResponse { response in
                XCTAssertTrue(response.needsReenroll)
                XCTAssertTrue(response.octagonTrusted)
                XCTAssertNotNil(response.moveRequest)
                XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.secureTerms))
                XCTAssertFalse(SecMockAKS.cacheFlowEnabled())
            }
            self.mockSecureBackupAdapter.moveError = nil

            self.fakeCuttlefishServer.returnEscrowCheckNeedsRepair = false
            self.fakeCuttlefishServer.returnEscrowCheckRepairReason = .recordRepairReasonUnknown
            self.fakeCuttlefishServer.returnEscrowCheckMoveRequest = false
        }

        // Normal repair
        self.fakeCuttlefishServer.returnEscrowCheckNeedsRepair = true
        escrowCheckAndValidateResponse { response in
            XCTAssertTrue(response.needsReenroll)
            XCTAssertTrue(response.octagonTrusted)
            XCTAssertNil(response.moveRequest)
            XCTAssertFalse(response.repairDisabled)         // repair NOT disabled, thus
            XCTAssertTrue(SecMockAKS.cacheFlowEnabled())    // repair was actually triggered!
        }
        self.fakeCuttlefishServer.returnEscrowCheckNeedsRepair = false
        SecMockAKS.resetCacheFlow()

        // Normal repair, but disabled
        self.fakeCuttlefishServer.returnEscrowCheckNeedsRepair = true
        self.fakeCuttlefishServer.returnEscrowCheckRepairDisabled = true
        escrowCheckAndValidateResponse { response in
            XCTAssertTrue(response.needsReenroll)
            XCTAssertTrue(response.octagonTrusted)
            XCTAssertNil(response.moveRequest)
            XCTAssertTrue(response.repairDisabled)          // repair disabled, thus
            XCTAssertFalse(SecMockAKS.cacheFlowEnabled())   // repair was not triggered
        }
        self.fakeCuttlefishServer.returnEscrowCheckNeedsRepair = false
        self.fakeCuttlefishServer.returnEscrowCheckRepairDisabled = false

        // Pretend that we succeeded silently so that we'll be rate-limited.
        XCTAssertNoThrow(try self.cuttlefishContext.accountMetadataStore.persistLastEscrowRepairAttempted(Date.now))
        SecMockAKS.resetCacheFlow()

        self.fakeCuttlefishServer.returnEscrowCheckNeedsRepair = true
        // Rate-limited repair attempt (CFU). No test for CFU, because in this case, the CFU is posted by CoreCDP. radar:148048053
        escrowCheckAndValidateResponse { response in
            XCTAssertTrue(response.needsReenroll)
            XCTAssertTrue(response.octagonTrusted)
            XCTAssertNil(response.moveRequest)
            XCTAssertFalse(SecMockAKS.cacheFlowEnabled())
        }

        // Rate-limited on an older version, so we should repair anyway.
        XCTAssertNoThrow(try self.cuttlefishContext.accountMetadataStore._persistEscrowRepairAttemptVersion(-1))
        escrowCheckAndValidateResponse { response in
            XCTAssertTrue(response.needsReenroll)
            XCTAssertTrue(response.octagonTrusted)
            XCTAssertNil(response.moveRequest)
            XCTAssertTrue(SecMockAKS.cacheFlowEnabled())
        }
        self.fakeCuttlefishServer.returnEscrowCheckNeedsRepair = false

        self.aksLockState = false
    }
    func testErrorCodes() throws {

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        _ = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        self.mockSecureBackupAdapter.enableError = NSError(domain: kCloudServicesErrorDomain, code: Int(kCloudServicesBadParametersError.rawValue), userInfo: nil)
        var pendingFlagCondition = try XCTUnwrap(self.cuttlefishContext.stateMachine.flags.condition(forFlag: OctagonFlagPasscodeStashAvailable))
        self.cuttlefishContext.passcodeStashAvailable(NSNumber(value: 2))
        XCTAssertEqual(0, pendingFlagCondition.wait(10 * NSEC_PER_SEC), "State machine should have handled the notification")
        self.cuttlefishContext.startOctagonStateMachine()
        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            XCTAssertEqual(0, accountState.lastEscrowRepairAttempted, "lastEscrowRepairAttempted should be 0")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        var underlyingError = NSError(domain: "securityd", code: Int(errSecInteractionNotAllowed), userInfo: nil)
        self.mockSecureBackupAdapter.enableError = NSError(domain: TrustedPeersHelperErrorDomain, code: Int(TrustedPeersHelperErrorCode.failedToLoadSecret.rawValue), userInfo: [NSUnderlyingErrorKey: underlyingError])
        pendingFlagCondition = try XCTUnwrap(self.cuttlefishContext.stateMachine.flags.condition(forFlag: OctagonFlagPasscodeStashAvailable))
        self.cuttlefishContext.passcodeStashAvailable(NSNumber(value: 2))
        XCTAssertEqual(0, pendingFlagCondition.wait(10 * NSEC_PER_SEC), "State machine should have handled the notification")
        self.cuttlefishContext.startOctagonStateMachine()
        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            XCTAssertEqual(0, accountState.lastEscrowRepairAttempted, "lastEscrowRepairAttempted should be 0")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        underlyingError = NSError(domain: NSURLErrorDomain, code: NSURLErrorNotConnectedToInternet, userInfo: nil)
        self.mockSecureBackupAdapter.enableError = NSError(domain: CKErrorDomain, code: CKError.networkUnavailable.rawValue, userInfo: [NSUnderlyingErrorKey: underlyingError])
        pendingFlagCondition = try XCTUnwrap(self.cuttlefishContext.stateMachine.flags.condition(forFlag: OctagonFlagPasscodeStashAvailable))
        self.cuttlefishContext.passcodeStashAvailable(NSNumber(value: 2))
        XCTAssertEqual(0, pendingFlagCondition.wait(10 * NSEC_PER_SEC), "State machine should have handled the notification")
        self.cuttlefishContext.startOctagonStateMachine()
        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            XCTAssertEqual(0, accountState.lastEscrowRepairAttempted, "lastEscrowRepairAttempted should be 0")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        underlyingError = NSError(domain: NSURLErrorDomain, code: NSURLErrorUnknown, userInfo: nil)
        self.mockSecureBackupAdapter.enableError = NSError(domain: CKErrorDomain, code: CKError.networkUnavailable.rawValue, userInfo: [NSUnderlyingErrorKey: underlyingError])
        pendingFlagCondition = try XCTUnwrap(self.cuttlefishContext.stateMachine.flags.condition(forFlag: OctagonFlagPasscodeStashAvailable))
        self.cuttlefishContext.passcodeStashAvailable(NSNumber(value: 2))
        XCTAssertEqual(0, pendingFlagCondition.wait(10 * NSEC_PER_SEC), "State machine should have handled the notification")
        self.cuttlefishContext.startOctagonStateMachine()
        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            XCTAssertEqual(0, accountState.lastEscrowRepairAttempted, "lastEscrowRepairAttempted should be 0")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        underlyingError = NSError(domain: NSURLErrorDomain, code: NSURLErrorCannotFindHost, userInfo: nil)
        self.mockSecureBackupAdapter.enableError = NSError(domain: CKErrorDomain, code: CKError.networkUnavailable.rawValue, userInfo: [NSUnderlyingErrorKey: underlyingError])
        pendingFlagCondition = try XCTUnwrap(self.cuttlefishContext.stateMachine.flags.condition(forFlag: OctagonFlagPasscodeStashAvailable))
        self.cuttlefishContext.passcodeStashAvailable(NSNumber(value: 2))
        XCTAssertEqual(0, pendingFlagCondition.wait(10 * NSEC_PER_SEC), "State machine should have handled the notification")
        self.cuttlefishContext.startOctagonStateMachine()
        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            XCTAssertEqual(0, accountState.lastEscrowRepairAttempted, "lastEscrowRepairAttempted should be 0")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        underlyingError = NSError(domain: NSURLErrorDomain, code: NSURLErrorCannotConnectToHost, userInfo: nil)
        self.mockSecureBackupAdapter.enableError = NSError(domain: CKErrorDomain, code: CKError.networkUnavailable.rawValue, userInfo: [NSUnderlyingErrorKey: underlyingError])

        pendingFlagCondition = try XCTUnwrap(self.cuttlefishContext.stateMachine.flags.condition(forFlag: OctagonFlagPasscodeStashAvailable))
        self.cuttlefishContext.passcodeStashAvailable(NSNumber(value: 2))
        XCTAssertEqual(0, pendingFlagCondition.wait(10 * NSEC_PER_SEC), "State machine should have handled the notification")
        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            XCTAssertEqual(0, accountState.lastEscrowRepairAttempted, "lastEscrowRepairAttempted should be 0")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        underlyingError = NSError(domain: NSURLErrorDomain, code: NSURLErrorDNSLookupFailed, userInfo: nil)
        self.mockSecureBackupAdapter.enableError = NSError(domain: CKErrorDomain, code: CKError.networkUnavailable.rawValue, userInfo: [NSUnderlyingErrorKey: underlyingError])
        pendingFlagCondition = try XCTUnwrap(self.cuttlefishContext.stateMachine.flags.condition(forFlag: OctagonFlagPasscodeStashAvailable))
        self.cuttlefishContext.passcodeStashAvailable(NSNumber(value: 2))
        XCTAssertEqual(0, pendingFlagCondition.wait(10 * NSEC_PER_SEC), "State machine should have handled the notification")
        self.cuttlefishContext.startOctagonStateMachine()
        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            XCTAssertEqual(0, accountState.lastEscrowRepairAttempted, "lastEscrowRepairAttempted should be 0")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        underlyingError = NSError(domain: NSURLErrorDomain, code: NSURLErrorCannotLoadFromNetwork, userInfo: nil)
        self.mockSecureBackupAdapter.enableError = NSError(domain: CKErrorDomain, code: CKError.networkUnavailable.rawValue, userInfo: [NSUnderlyingErrorKey: underlyingError])
        pendingFlagCondition = try XCTUnwrap(self.cuttlefishContext.stateMachine.flags.condition(forFlag: OctagonFlagPasscodeStashAvailable))
        self.cuttlefishContext.passcodeStashAvailable(NSNumber(value: 2))
        XCTAssertEqual(0, pendingFlagCondition.wait(10 * NSEC_PER_SEC), "State machine should have handled the notification")
        self.cuttlefishContext.startOctagonStateMachine()
        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            XCTAssertEqual(0, accountState.lastEscrowRepairAttempted, "lastEscrowRepairAttempted should be 0")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        underlyingError = NSError(domain: NSURLErrorDomain, code: NSURLErrorInternationalRoamingOff, userInfo: nil)
        self.mockSecureBackupAdapter.enableError = NSError(domain: CKErrorDomain, code: CKError.networkUnavailable.rawValue, userInfo: [NSUnderlyingErrorKey: underlyingError])
        pendingFlagCondition = try XCTUnwrap(self.cuttlefishContext.stateMachine.flags.condition(forFlag: OctagonFlagPasscodeStashAvailable))
        self.cuttlefishContext.passcodeStashAvailable(NSNumber(value: 2))
        XCTAssertEqual(0, pendingFlagCondition.wait(10 * NSEC_PER_SEC), "State machine should have handled the notification")
        self.cuttlefishContext.startOctagonStateMachine()
        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            XCTAssertEqual(0, accountState.lastEscrowRepairAttempted, "lastEscrowRepairAttempted should be 0")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        // test to expect rate limiting

        underlyingError = NSError(domain: NSURLErrorDomain, code: NSURLErrorBadURL, userInfo: nil)
        self.mockSecureBackupAdapter.enableError = NSError(domain: CKErrorDomain, code: CKError.networkUnavailable.rawValue, userInfo: [NSUnderlyingErrorKey: underlyingError])
        pendingFlagCondition = try XCTUnwrap(self.cuttlefishContext.stateMachine.flags.condition(forFlag: OctagonFlagPasscodeStashAvailable))
        self.cuttlefishContext.passcodeStashAvailable(NSNumber(value: 2))
        XCTAssertEqual(0, pendingFlagCondition.wait(10 * NSEC_PER_SEC), "State machine should have handled the notification")
        self.cuttlefishContext.startOctagonStateMachine()
        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            XCTAssertNotEqual(0, accountState.lastEscrowRepairAttempted, "lastEscrowRepairAttempted should not be 0")
        } catch {
            XCTFail("error loading account state: \(error)")
        }
    }

    func testIgnorePasscodeStashWhenUntrusted() throws {
        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // Untrusted. Assert that notification is ignored (no flag set).
        let unexpectedCondition = self.cuttlefishContext.stateMachine.flags.condition(forFlag: OctagonFlagPasscodeStashAvailable)
        self.cuttlefishContext.passcodeStashAvailable(NSNumber(value: 2))
        XCTAssertNotEqual(0, unexpectedCondition.wait(1 * NSEC_PER_SEC), "State machine should NOT have handled the notification")

        // Get trusted.
        _ = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        // Assert that the notification is NOT ignored (flag is set).
        let expectedCondition = try XCTUnwrap(self.cuttlefishContext.stateMachine.flags.condition(forFlag: OctagonFlagPasscodeStashAvailable))
        self.cuttlefishContext.passcodeStashAvailable(NSNumber(value: 2))
        XCTAssertEqual(0, expectedCondition.wait(10 * NSEC_PER_SEC), "State machine should have handled the notification")
    }
}

#endif
