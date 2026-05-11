#if OCTAGON

import FeatureFlags

class OctagonEscrowCheckAndRepairTests: OctagonTestsBase {
    func assertValidiCSC(exists: Bool, context: OTCuttlefishContext,
                         file: StaticString = #file,
                         line: UInt = #line) async throws {
        // First, find the escrow record that matches this context
        let escrowInformations = self.fakeCuttlefishServer.allCurrentEscrowRecords()

        let (_, bottleID, _) = try await context.fetchEscrowContents()

        let er = escrowInformations.first { $0.escrowInformationMetadata.bottleID == bottleID }
        if exists {
            XCTAssertNotNil(er, "should have found an escrow record matching context bottleID")
        } else {
            XCTAssertNil(er, "should not have found an escrow record matching context bottleID")
        }
    }

    @discardableResult
    func assertCachedEscrowRecord(exists: Bool,
                                  context: OTCuttlefishContext,
                                  file: StaticString = #file,
                                  line: UInt = #line) throws -> OTSerializedPlistEscrowRecord? {
        let state = try context.accountMetadataStore.loadOrCreateAccountMetadata()

        let record = state.escrowRecordCache?.serializedRecord

        if exists {
            if let r = record {
                XCTAssertGreaterThan(r.count, 0, "Should have had some escrow record in the cache")

                // The cached record is a OTSerializedPlistEscrowRecord, stored as a NSKeyedArchiver
                let unarchiver = try NSKeyedUnarchiver(forReadingFrom: r)
                return unarchiver.decodeObject(of: OTSerializedPlistEscrowRecord.self, forKey: NSKeyedArchiveRootObjectKey)
            } else {
                XCTFail("Should have had some bytes for a serialized record")
                return nil
            }
        } else {
            // Protobufs can be weird. Missing or empty is okay here.
            if let r = record {
                XCTAssertEqual(r.count, 0, "Should have had 0 escrow record bytes in the cache")
            } else {
                XCTAssertNil(record, "Should have had no escrow record in the cache")
            }

            return nil
        }
    }

    func performEscrowCheck(context: OTCuttlefishContext,
                            needsReenroll: Bool,
                            rateLimitState: OTEscrowCheckRateLimitState,
                            repairReason: EscrowRepairReason,
                            daysLeftOnRateLimit: Int,
                            expectCacheFlowEnabled: Bool,
                            file: StaticString = #file,
                            line: UInt = #line) throws {
        do {
            SecMockAKS.resetCacheFlow()

            let escrowCheckCallback = self.expectation(description: "escrowCheck callback occurs")

            self.manager.escrowCheck(self.otcontrolArgumentsFor(context: context), isBackgroundCheck: false) { response, error in
                XCTAssertNotNil(response, "response should not be nil")
                XCTAssertNil(error, "error should be nil")
                if let response {
                    XCTAssertEqual(response.needsReenroll, needsReenroll)
                    XCTAssertEqual(response.rateLimitState, rateLimitState.rawValue)
                    XCTAssertEqual(response.repairReason, repairReason.rawValue)
                    XCTAssertEqual(response.daysLeftOnRateLimit, daysLeftOnRateLimit)
                }
                escrowCheckCallback.fulfill()
            }

            self.wait(for: [escrowCheckCallback], timeout: 10)

            XCTAssertEqual(SecMockAKS.cacheFlowEnabled(), OCTAGON_PLATFORM_SUPPORTS_CACHE_FLOW != 0 ? expectCacheFlowEnabled : false)
        }
    }

    func enableCacheFlowAndUnlockDevice() {
        SecMockAKS.enableCacheFlow()
        self.mockAppleKeyStoreAdapter.deviceUnlockedByPasscode()
    }

    enum EscrowCacheState: CaseIterable {
        case rateLimitedNoRecord
        case rateLimitedOldVersion
        case rateLimitedPasscodeMismatch
        case rateLimitedWithValidRecord
    }

    func performWithConditions(escrowCacheState: EscrowCacheState, _ action: () throws -> Void) throws {
        let escrowRecordCache = OTAccountMetadataClassCEscrowRecordCache()!
        escrowRecordCache.serializedRecord = nil
        escrowRecordCache.passcodeGeneration = 1 // mockaks always reports 1
        escrowRecordCache.bottleID = self.fakeCuttlefishServer.state.bottles[0].bottleID
        escrowRecordCache.cacheTimestamp = UInt64(Date.now.timeIntervalSince1970) * 1000
        escrowRecordCache.cacheVersion = UInt64(ESCROW_REPAIR_CURRENT_VERSION)

        switch escrowCacheState {
        case .rateLimitedNoRecord:
            break
        case .rateLimitedOldVersion:
            escrowRecordCache.cacheVersion = UInt64(ESCROW_REPAIR_CURRENT_VERSION) - 1
        case .rateLimitedPasscodeMismatch:
            escrowRecordCache.passcodeGeneration = 42
        case .rateLimitedWithValidRecord:
            let fakeSerializedRecord = OTSerializedPlistEscrowRecord()
            fakeSerializedRecord.label = "com.apple.test"
            fakeSerializedRecord.blob = Data(count: 10)
            fakeSerializedRecord.metadata = Data(count: 10)

            escrowRecordCache.serializedRecord = try NSKeyedArchiver.archivedData(withRootObject: fakeSerializedRecord, requiringSecureCoding: true)
        }

        try self.cuttlefishContext.accountMetadataStore.persistAccountChanges { metadata in
            XCTAssertNil(metadata.escrowRecordCache, "cache should not exist")
            metadata.escrowRecordCache = escrowRecordCache
            return metadata
        }

        XCTAssertNoThrow(try action())

        try self.cuttlefishContext.accountMetadataStore.persistAccountChanges { metadata in
            metadata.escrowRecordCache = nil
            return metadata
        }
    }

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

        do {
            let escrowCheckCallback = self.expectation(description: "escrowCheck callback occurs")
            self.manager.escrowCheck(OTControlArguments(configuration: self.otcliqueContext), isBackgroundCheck: false) { response, error in
                XCTAssertNil(response, "response should be nil")
                XCTAssertNotNil(error, "error should not be nil")

                let nserror = error as! NSError
                XCTAssertEqual(nserror.domain, ContainerError.errorDomain)
                XCTAssertEqual(nserror.code, ContainerError.noPreparedIdentity.errorCode)
                escrowCheckCallback.fulfill()
            }
            self.wait(for: [escrowCheckCallback], timeout: 10)
            self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        }

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

        self.fakeCuttlefishServer.returnEscrowCheckRepairReason = .recordOk

        escrowCheckAndValidateResponse { response in
            XCTAssertFalse(response.needsReenroll)
            XCTAssertEqual(response.octagonTrusted, OctagonTrustStatus.trusted.rawValue)
            XCTAssertNil(response.moveRequest)
            XCTAssertFalse(SecMockAKS.cacheFlowEnabled())
            XCTAssertEqual(response.rateLimitState, OTEscrowCheckRateLimitState.notRateLimited.rawValue)

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
        self.fakeCuttlefishServer.returnEscrowCheckRepairReason = .recordRepairReasonUnknown

        escrowCheckAndValidateResponse { response in
            XCTAssertFalse(response.needsReenroll)
            XCTAssertEqual(response.octagonTrusted, OctagonTrustStatus.unknown.rawValue)
            XCTAssertNil(response.moveRequest)
            XCTAssertFalse(SecMockAKS.cacheFlowEnabled())
            XCTAssertEqual(response.rateLimitState, OTEscrowCheckRateLimitState.notRateLimited.rawValue)

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
                XCTAssertEqual(response.octagonTrusted, OctagonTrustStatus.trusted.rawValue)
                XCTAssertNotNil(response.moveRequest)
                XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.secureTerms))
                XCTAssertEqual(response.rateLimitState, OTEscrowCheckRateLimitState.notRateLimited.rawValue)
                if isFeatureEnabled(SecurityFeatures.EscrowCheckMigration) {
                    XCTAssertTrue(SecMockAKS.cacheFlowEnabled())
                } else {
                    XCTAssertFalse(SecMockAKS.cacheFlowEnabled())
                }
            }
            SecMockAKS.resetCacheFlow()
            try self.cuttlefishContext.accountMetadataStore.persistEscrowRecordCache(nil)

            // Terms needed (CFU, cache flow not enabled)
            self.mockSecureBackupAdapter.moveError = NSError(domain: kCloudServicesErrorDomain, code: Int(kCloudServicesMissingSecureTerms.rawValue), userInfo: nil)
            escrowCheckAndValidateResponse { response in
                XCTAssertTrue(response.needsReenroll)
                XCTAssertEqual(response.octagonTrusted, OctagonTrustStatus.trusted.rawValue)
                XCTAssertNotNil(response.moveRequest)
                XCTAssertEqual(response.rateLimitState, OTEscrowCheckRateLimitState.notRateLimited.rawValue)
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
                XCTAssertEqual(response.octagonTrusted, OctagonTrustStatus.trusted.rawValue)
                XCTAssertNotNil(response.moveRequest)
                XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.secureTerms))
                XCTAssertFalse(SecMockAKS.cacheFlowEnabled())
                XCTAssertEqual(response.rateLimitState, OTEscrowCheckRateLimitState.notRateLimited.rawValue)
            }
            self.mockSecureBackupAdapter.moveError = nil

            self.fakeCuttlefishServer.returnEscrowCheckNeedsRepair = false
            self.fakeCuttlefishServer.returnEscrowCheckRepairReason = .recordRepairReasonUnknown
            self.fakeCuttlefishServer.returnEscrowCheckMoveRequest = false
        }

        try self.cuttlefishContext.accountMetadataStore.persistEscrowRecordCache(nil)

        // Graph needs validation repair
        self.fakeCuttlefishServer.returnEscrowCheckGraphNeedsRepair = true
        escrowCheckAndValidateResponse { response in
            XCTAssertTrue(response.needsReenroll)
            XCTAssertEqual(response.octagonTrusted, OctagonTrustStatus.graphNeedsRepair.rawValue)
            XCTAssertNil(response.moveRequest)
            XCTAssertFalse(response.repairDisabled)         // repair NOT disabled, thus
            XCTAssertEqual(SecMockAKS.cacheFlowEnabled(), OCTAGON_PLATFORM_SUPPORTS_CACHE_FLOW != 0)    // repair was triggered
            XCTAssertEqual(response.rateLimitState, OTEscrowCheckRateLimitState.notRateLimited.rawValue)
        }
        self.fakeCuttlefishServer.returnEscrowCheckGraphNeedsRepair = false

        try self.cuttlefishContext.accountMetadataStore.persistEscrowRecordCache(nil)

        // Peer untrusted in Cuttlefish repair
        self.fakeCuttlefishServer.returnEscrowCheckPeerNotTrusted = true
        escrowCheckAndValidateResponse { response in
            XCTAssertTrue(response.needsReenroll)
            XCTAssertEqual(response.octagonTrusted, OctagonTrustStatus.notTrustedCuttlefish.rawValue)
            XCTAssertNil(response.moveRequest)
            XCTAssertFalse(response.repairDisabled)         // repair NOT disabled, thus
            XCTAssertEqual(SecMockAKS.cacheFlowEnabled(), OCTAGON_PLATFORM_SUPPORTS_CACHE_FLOW != 0)    // repair was triggered
            XCTAssertEqual(response.rateLimitState, OTEscrowCheckRateLimitState.notRateLimited.rawValue)
        }
        self.fakeCuttlefishServer.returnEscrowCheckPeerNotTrusted = false

        // Normal repair
        self.fakeCuttlefishServer.returnEscrowCheckNeedsRepair = true
        escrowCheckAndValidateResponse { response in
            XCTAssertTrue(response.needsReenroll)
            XCTAssertEqual(response.octagonTrusted, OctagonTrustStatus.trusted.rawValue)
            XCTAssertNil(response.moveRequest)
            XCTAssertFalse(response.repairDisabled)         // repair NOT disabled, thus
            XCTAssertEqual(SecMockAKS.cacheFlowEnabled(), OCTAGON_PLATFORM_SUPPORTS_CACHE_FLOW != 0)    // repair was actually triggered!
            XCTAssertEqual(response.rateLimitState, OTEscrowCheckRateLimitState.notRateLimited.rawValue)
        }
        self.fakeCuttlefishServer.returnEscrowCheckNeedsRepair = false
        SecMockAKS.resetCacheFlow()
        try self.cuttlefishContext.accountMetadataStore.persistEscrowRecordCache(nil)

        // Normal repair, but disabled
        self.fakeCuttlefishServer.returnEscrowCheckNeedsRepair = true
        self.fakeCuttlefishServer.returnEscrowCheckRepairDisabled = true
        escrowCheckAndValidateResponse { response in
            XCTAssertTrue(response.needsReenroll)
            XCTAssertEqual(response.octagonTrusted, OctagonTrustStatus.trusted.rawValue)
            XCTAssertNil(response.moveRequest)
            XCTAssertTrue(response.repairDisabled)          // repair disabled, thus
            XCTAssertFalse(SecMockAKS.cacheFlowEnabled())   // repair was not triggered
            XCTAssertEqual(response.rateLimitState, OTEscrowCheckRateLimitState.notRateLimited.rawValue)
        }
        self.fakeCuttlefishServer.returnEscrowCheckNeedsRepair = false
        self.fakeCuttlefishServer.returnEscrowCheckRepairDisabled = false
        SecMockAKS.resetCacheFlow()
        try self.cuttlefishContext.accountMetadataStore.persistEscrowRecordCache(nil)

        // Rate-limited repair attempt (CFU). No test for CFU, because in this case, the CFU is posted by CoreCDP. radar:148048053
        self.fakeCuttlefishServer.expectRateLimitListener = { request in
            XCTAssertTrue(request.rateLimited > 0, "Expected a rate-limit in request")
            return nil
        }
        self.fakeCuttlefishServer.returnEscrowCheckNeedsRepair = true
        XCTAssertNoThrow(try performWithConditions(escrowCacheState: .rateLimitedNoRecord) {
            escrowCheckAndValidateResponse { response in
                XCTAssertTrue(response.needsReenroll)
                XCTAssertEqual(response.octagonTrusted, OctagonTrustStatus.trusted.rawValue)
                XCTAssertNil(response.moveRequest)
                XCTAssertFalse(SecMockAKS.cacheFlowEnabled())
                XCTAssertEqual(response.rateLimitState, OTEscrowCheckRateLimitState.rateLimited.rawValue)
            }
        })
        self.fakeCuttlefishServer.expectRateLimitListener = nil
        self.fakeCuttlefishServer.returnEscrowCheckNeedsRepair = false
        SecMockAKS.resetCacheFlow()
        try self.cuttlefishContext.accountMetadataStore.persistEscrowRecordCache(nil)

        // Rate-limited on an older version, so we should repair anyway.
        self.fakeCuttlefishServer.expectRateLimitListener = { request in
            XCTAssertTrue(request.rateLimited == 0, "Expected a rate-limit in request")
            return nil
        }
        self.fakeCuttlefishServer.returnEscrowCheckNeedsRepair = true
        XCTAssertNoThrow(try performWithConditions(escrowCacheState: .rateLimitedOldVersion) {
            escrowCheckAndValidateResponse { response in
                XCTAssertTrue(response.needsReenroll)
                XCTAssertEqual(response.octagonTrusted, OctagonTrustStatus.trusted.rawValue)
                XCTAssertNil(response.moveRequest)
                XCTAssertEqual(SecMockAKS.cacheFlowEnabled(), OCTAGON_PLATFORM_SUPPORTS_CACHE_FLOW != 0)
                XCTAssertEqual(response.rateLimitState, OTEscrowCheckRateLimitState.notRateLimited.rawValue)
            }
        })
        self.fakeCuttlefishServer.expectRateLimitListener = nil
        self.fakeCuttlefishServer.returnEscrowCheckNeedsRepair = false
        SecMockAKS.resetCacheFlow()

        self.aksLockState = false
    }
    func testIgnorePasscodeStashWhenUntrusted() throws {
        try XCTSkipIf(OCTAGON_PLATFORM_SUPPORTS_CACHE_FLOW == 0)

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // Untrusted. Assert that notification is ignored (record not generated, no flag set).
        let unexpectedCondition = self.cuttlefishContext.stateMachine.flags.condition(forFlag: OctagonFlagCachedEscrowRecordAvailable)
        enableCacheFlowAndUnlockDevice()
        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            XCTAssertNil(accountState.escrowRecordCache?.serializedRecord, "should be no serialized record")
        } catch {
            XCTFail("error loading account state: \(error)")
        }
        XCTAssertNotEqual(0, unexpectedCondition.wait(1 * NSEC_PER_SEC), "State machine should NOT have handled the notification")

        // Get trusted.
        _ = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        // Assert that the notification is NOT ignored (record is generated, flag is set).
        let expectedCondition = try XCTUnwrap(self.cuttlefishContext.stateMachine.flags.condition(forFlag: OctagonFlagCachedEscrowRecordAvailable))
        enableCacheFlowAndUnlockDevice()
        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            XCTAssertNotNil(accountState.escrowRecordCache?.serializedRecord, "should be a serialized record")
        } catch {
            XCTFail("error loading account state: \(error)")
        }
        XCTAssertEqual(0, expectedCondition.wait(10 * NSEC_PER_SEC), "State machine should have handled the notification")
    }

    func testRepairRateLimited() throws {
        try XCTSkipIf(OCTAGON_PLATFORM_SUPPORTS_CACHE_FLOW == 0)

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        _ = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        var afterRepairTimestamp: UInt64 = 0

        // kick off a repair to set the rate limit
        var pendingFlagCondition = try XCTUnwrap(self.cuttlefishContext.stateMachine.flags.condition(forFlag: OctagonFlagCachedEscrowRecordAvailable))
        enableCacheFlowAndUnlockDevice()
        XCTAssertEqual(0, pendingFlagCondition.wait(10 * NSEC_PER_SEC), "State machine should have handled the notification")
        XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(20 * NSEC_PER_SEC), "Paused condition should be fulfilled")
        XCTAssertTrue(self.cuttlefishContext.stateMachine.isPaused(), "State machine should consider itself paused")
        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            XCTAssertNotEqual(0, accountState.lastEscrowRepairAttempted, "lastEscrowRepairAttempted should not be 0")
            afterRepairTimestamp = accountState.lastEscrowRepairAttempted
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        // a second repair operation comes through, this time the rate limiting check when receiving the AKS notification should stop the operation from happening.
        pendingFlagCondition = try XCTUnwrap(self.cuttlefishContext.stateMachine.flags.condition(forFlag: OctagonFlagCachedEscrowRecordAvailable))
        enableCacheFlowAndUnlockDevice()
        XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(20 * NSEC_PER_SEC), "Paused condition should be fulfilled")
        XCTAssertNotEqual(0, pendingFlagCondition.wait(1 * NSEC_PER_SEC), "State machine should not have handled the notification")
        XCTAssertTrue(self.cuttlefishContext.stateMachine.isPaused(), "State machine should consider itself paused")
        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            XCTAssertEqual(afterRepairTimestamp, accountState.lastEscrowRepairAttempted, "lastEscrowRepairAttempted should not change")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        // a passcode change comes through now, should override the rate limit
        pendingFlagCondition = try XCTUnwrap(self.cuttlefishContext.stateMachine.flags.condition(forFlag: OctagonFlagCachedEscrowRecordAvailable))
        self.mockAppleKeyStoreAdapter.devicePasscodeChanged()
        XCTAssertEqual(0, pendingFlagCondition.wait(10 * NSEC_PER_SEC), "State machine should have handled the notification")
        XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(20 * NSEC_PER_SEC), "Paused condition should be fulfilled")
        XCTAssertTrue(self.cuttlefishContext.stateMachine.isPaused(), "State machine should consider itself paused")
        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)
            XCTAssertLessThan(afterRepairTimestamp, accountState.lastEscrowRepairAttempted, "lastEscrowRepairAttempted should be more recent")
        } catch {
            XCTFail("error loading account state: \(error)")
        }
    }

    func testEscrowRecordCacheBehavior() throws {
        try XCTSkipIf(OCTAGON_PLATFORM_SUPPORTS_CACHE_FLOW == 0)

        // test setup boilerplate
        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        _ = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        // perform actual test
        for state in EscrowCacheState.allCases {
            let expectRepair: Bool

            switch state {
            case .rateLimitedNoRecord:
                expectRepair = false
            case .rateLimitedOldVersion:
                expectRepair = true
            case .rateLimitedPasscodeMismatch:
                expectRepair = true
            case .rateLimitedWithValidRecord:
                expectRepair = true
            }

            XCTAssertNoThrow(try performWithConditions(escrowCacheState: state) {
                let pendingFlagCondition = try XCTUnwrap(self.cuttlefishContext.stateMachine.flags.condition(forFlag: OctagonFlagCachedEscrowRecordAvailable))
                enableCacheFlowAndUnlockDevice()
                XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(5 * NSEC_PER_SEC), "\(state): Paused condition should be fulfilled")
                XCTAssertTrue(self.cuttlefishContext.stateMachine.isPaused(), "\(state): State machine should consider itself paused")
                if expectRepair {
                    XCTAssertEqual(0, pendingFlagCondition.wait(1 * NSEC_PER_SEC), "\(state): State machine should have handled the notification")
                } else {
                    XCTAssertNotEqual(0, pendingFlagCondition.wait(1 * NSEC_PER_SEC), "\(state): State machine should not have handled the notification")
                }
                do {
                    let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil)

                    XCTAssertNil(accountState.escrowRecordCache.serializedRecord, "\(state): cache should not contain a stored record")
                    XCTAssertEqual(accountState.escrowRecordCache.passcodeGeneration, 1, "\(state): cache passcode generation should be current")
                    XCTAssertEqual(accountState.escrowRecordCache.bottleID, self.fakeCuttlefishServer.state.bottles[0].bottleID, "\(state): cache bottleID should match")
                    XCTAssertNotEqual(accountState.escrowRecordCache.cacheTimestamp, 0, "\(state): cache timestamp not be 0")
                    XCTAssertEqual(accountState.escrowRecordCache.cacheVersion, UInt64(ESCROW_REPAIR_CURRENT_VERSION), "\(state): cache version current")

                    if expectRepair {
                        XCTAssertNotEqual(0, accountState.lastEscrowRepairAttempted, "\(state): lastEscrowRepairAttempted should not be 0")
                    } else {
                        XCTAssertEqual(0, accountState.lastEscrowRepairAttempted, "\(state): lastEscrowRepairAttempted should be 0")
                    }
                } catch {
                    XCTFail("\(state): error loading account state: \(error)")
                }
            })
        }
    }

    func testEscrowRecordPerformHealingAfterSetup() async throws {
#if os(tvOS)
        try XCTSkipIf(true, "no escrow check on TV")
#endif
        try XCTSkipIf(OCTAGON_PLATFORM_SUPPORTS_CACHE_FLOW == 0)

        // We want peers to join in a non-escrow state!
        self.fakeCuttlefishServer.automaticallyCreateEscrowRecordsOnJoin = false

        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        // This shouldn't exist, because FakeCuttlefish won't have made it
        try await self.assertValidiCSC(exists: false, context: self.cuttlefishContext)

        // These results should be data-driven
        self.fakeCuttlefishServer.returnEscrowCheckRepairReason = EscrowRepairReason.noRecordMatchingPeer
        self.fakeCuttlefishServer.returnEscrowCheckNeedsRepair = true

        try self.performEscrowCheck(context: self.cuttlefishContext,
                                    needsReenroll: true,
                                    rateLimitState: .notRateLimited,
                                    repairReason: .noRecordMatchingPeer,
                                    daysLeftOnRateLimit: 0,
                                    expectCacheFlowEnabled: true)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        // A passcode entry arrives. A new record should be uploaded.
        do {
            let recordUploadCallback = self.expectation(description: "Record upload should be called")
            self.mockSecureBackupAdapter.storeRecordListener = { _ in
                // TODO: remove when the response is data-driven
                self.fakeCuttlefishServer.returnEscrowCheckNeedsRepair = false
                recordUploadCallback.fulfill()
                return nil
            }

            self.mockAppleKeyStoreAdapter.deviceUnlockedByPasscode()

            await fulfillment(of: [recordUploadCallback], timeout: 10)
            self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        }

        try await self.assertValidiCSC(exists: true, context: self.cuttlefishContext)

        try self.performEscrowCheck(context: self.cuttlefishContext,
                                    needsReenroll: false,
                                    rateLimitState: .rateLimited,
                                    repairReason: .recordOk,
                                    daysLeftOnRateLimit: 180,
                                    expectCacheFlowEnabled: false)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        // Ensure that no record is uploaded, even if the passcode arrives
        do {
            let recordUploadCallback = self.expectation(description: "Record upload should not be called")
            recordUploadCallback.isInverted = true
            self.mockSecureBackupAdapter.storeRecordListener = { _ in
                recordUploadCallback.fulfill()
                return nil
            }

            self.mockAppleKeyStoreAdapter.deviceUnlockedByPasscode()

            self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
            await fulfillment(of: [recordUploadCallback], timeout: 1)
        }


        // And the record is lost once again
        self.fakeCuttlefishServer.removeAllEscrowRecords()

        // These results should be data-driven
        self.fakeCuttlefishServer.returnEscrowCheckRepairReason = EscrowRepairReason.noRecordMatchingPeer
        self.fakeCuttlefishServer.returnEscrowCheckNeedsRepair = true

        try self.performEscrowCheck(context: self.cuttlefishContext,
                                    needsReenroll: true,
                                    rateLimitState: .rateLimited,
                                    repairReason: .noRecordMatchingPeer,
                                    daysLeftOnRateLimit: 180,
                                    expectCacheFlowEnabled: false)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        // A passcode entry arrives. A new record should not be uploaded.
        do {
            let recordUploadCallback = self.expectation(description: "Record upload should not be called")
            recordUploadCallback.isInverted = true
            self.mockSecureBackupAdapter.storeRecordListener = { _ in
                recordUploadCallback.fulfill()
                return nil
            }

            self.mockAppleKeyStoreAdapter.deviceUnlockedByPasscode()

            self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
            await fulfillment(of: [recordUploadCallback], timeout: 1)
        }

        // But if a passcode change occurs, it should happen again!
        SecMockAKS.incrementPasscodeGeneration()

        // Double-check: the rate-limit should be gone
        try self.performEscrowCheck(context: self.cuttlefishContext,
                                    needsReenroll: true,
                                    rateLimitState: .notRateLimited,
                                    repairReason: .noRecordMatchingPeer,
                                    daysLeftOnRateLimit: 0,
                                    expectCacheFlowEnabled: true)

        // A changed passcode arrives. A new record should be uploaded.
        do {
            let recordUploadCallback = self.expectation(description: "Record upload should be called")
            self.mockSecureBackupAdapter.storeRecordListener = { _ in
                // TODO: remove when the response is data-driven
                self.fakeCuttlefishServer.returnEscrowCheckNeedsRepair = false
                recordUploadCallback.fulfill()
                return nil
            }

            self.mockAppleKeyStoreAdapter.devicePasscodeChanged()

            self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
            await fulfillment(of: [recordUploadCallback], timeout: 1)
        }

        try self.performEscrowCheck(context: self.cuttlefishContext,
                                    needsReenroll: false,
                                    rateLimitState: .rateLimited,
                                    repairReason: .recordOk,
                                    daysLeftOnRateLimit: 180,
                                    expectCacheFlowEnabled: false)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testRecordReupload() async throws {
#if os(tvOS)
        try XCTSkipIf(true, "no escrow check on TV")
#endif

        try XCTSkipIf(OCTAGON_PLATFORM_SUPPORTS_CACHE_FLOW == 0)

        // We want peers to join in a non-escrow state!
        self.fakeCuttlefishServer.automaticallyCreateEscrowRecordsOnJoin = false

        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        // This shouldn't exist, because FakeCuttlefish won't have made it
        try await self.assertValidiCSC(exists: false, context: self.cuttlefishContext)

        // These results should be data-driven
        self.fakeCuttlefishServer.returnEscrowCheckRepairReason = EscrowRepairReason.noRecordMatchingPeer
        self.fakeCuttlefishServer.returnEscrowCheckNeedsRepair = true

        try self.performEscrowCheck(context: self.cuttlefishContext,
                                    needsReenroll: true,
                                    rateLimitState: .notRateLimited,
                                    repairReason: .noRecordMatchingPeer,
                                    daysLeftOnRateLimit: 0,
                                    expectCacheFlowEnabled: true)

        try self.assertCachedEscrowRecord(exists: false, context: self.cuttlefishContext)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        var uploadedRecord: OTSerializedPlistEscrowRecord?

        // A passcode entry arrives. A new record should be created and uploaded, but the upload fails.
        do {
            let recordUploadCallback = self.expectation(description: "Record upload should be called")
            self.mockSecureBackupAdapter.storeRecordListener = { recordCandidate in
                // Preserve blob for later examination
                uploadedRecord = recordCandidate

                recordUploadCallback.fulfill()
                return NSError(domain: kCloudServicesErrorDomain, code: Int(kCloudServicesNetworkError.rawValue), userInfo: nil)
            }

            self.mockAppleKeyStoreAdapter.deviceUnlockedByPasscode()

            await fulfillment(of: [recordUploadCallback], timeout: 10)
            self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        }

        XCTAssertNotNil(uploadedRecord?.blob, "should have some blob whose upload was attempted")
        XCTAssertGreaterThan(uploadedRecord?.blob.count ?? 0, 1, "should have some blob whose upload was attempted")
        do {
            let cachedSerializedRecord = try XCTUnwrap(self.assertCachedEscrowRecord(exists: true, context: self.cuttlefishContext))
            XCTAssertEqual(uploadedRecord?.blob, cachedSerializedRecord.blob, "cached blob should be the same as the one that was sent")
        }

        try await self.assertValidiCSC(exists: false, context: self.cuttlefishContext)

        // Now, try another escrow attempt, using the cached record
        do {
            let recordUploadCallback = self.expectation(description: "Record upload should be called")
            self.mockSecureBackupAdapter.storeRecordListener = { recordCandidate in
                XCTAssertEqual(uploadedRecord?.blob, recordCandidate.blob, "Blob should match the previous upload attempt")

                // TODO: remove when the response is data-driven
                self.fakeCuttlefishServer.returnEscrowCheckNeedsRepair = false

                recordUploadCallback.fulfill()
                return nil
            }

            try self.performEscrowCheck(context: self.cuttlefishContext,
                                        needsReenroll: true,
                                        rateLimitState: .validCacheRateLimited,
                                        repairReason: .noRecordMatchingPeer,
                                        daysLeftOnRateLimit: 180,
                                        expectCacheFlowEnabled: false)
            self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

            await fulfillment(of: [recordUploadCallback], timeout: 10)
            self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        }

        // At this point, we should have successfully repaired - good iCSC, cached record deleted.
        try await self.assertValidiCSC(exists: true, context: self.cuttlefishContext)
        do {
            try self.assertCachedEscrowRecord(exists: false, context: self.cuttlefishContext)
        }

        try self.performEscrowCheck(context: self.cuttlefishContext,
                                    needsReenroll: false,
                                    rateLimitState: .rateLimited,
                                    repairReason: .recordOk,
                                    daysLeftOnRateLimit: 180,
                                    expectCacheFlowEnabled: false)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testRateLimitEndingIfPeerChanges() async throws {
#if os(tvOS)
        try XCTSkipIf(true, "no escrow check on TV")
#endif

        try XCTSkipIf(OCTAGON_PLATFORM_SUPPORTS_CACHE_FLOW == 0)

        self.fakeCuttlefishServer.automaticallyCreateEscrowRecordsOnJoin = false

        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        // This shouldn't exist, because FakeCuttlefish won't have made it
        try await self.assertValidiCSC(exists: false, context: self.cuttlefishContext)

        // These results should be data-driven
        self.fakeCuttlefishServer.returnEscrowCheckRepairReason = EscrowRepairReason.noRecordMatchingPeer
        self.fakeCuttlefishServer.returnEscrowCheckNeedsRepair = true

        try self.performEscrowCheck(context: self.cuttlefishContext,
                                    needsReenroll: true,
                                    rateLimitState: .notRateLimited,
                                    repairReason: .noRecordMatchingPeer,
                                    daysLeftOnRateLimit: 0,
                                    expectCacheFlowEnabled: true)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        // A passcode entry arrives. A new record should be created and uploaded, but the upload fails.

        var recordBlob: Data?
        do {
            let recordUploadCallback = self.expectation(description: "Record upload should be called")
            self.mockSecureBackupAdapter.storeRecordListener = { recordCandidate in
                // Preserve blob for later examination
                recordBlob = recordCandidate.blob

                recordUploadCallback.fulfill()
                return NSError(domain: kCloudServicesErrorDomain, code: Int(kCloudServicesNetworkError.rawValue), userInfo: nil)
            }

            self.mockAppleKeyStoreAdapter.deviceUnlockedByPasscode()

            await fulfillment(of: [recordUploadCallback], timeout: 10)
            self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        }

        // Now we still need to repair our escrow record, but we have a candidate
        do {
            let recordUploadCallback = self.expectation(description: "Record upload should be called")
            self.mockSecureBackupAdapter.storeRecordListener = { recordCandidate in
                // Preserve blob for later examination
                recordBlob = recordCandidate.blob

                recordUploadCallback.fulfill()
                return NSError(domain: kCloudServicesErrorDomain, code: Int(kCloudServicesNetworkError.rawValue), userInfo: nil)
            }

            // This alone should attempt repair.
            try self.performEscrowCheck(context: self.cuttlefishContext,
                                        needsReenroll: true,
                                        rateLimitState: .validCacheRateLimited,
                                        repairReason: .noRecordMatchingPeer,
                                        daysLeftOnRateLimit: 180,
                                        expectCacheFlowEnabled: false)

            await fulfillment(of: [recordUploadCallback], timeout: 10)
            self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

            // iCSC should remain invalid, cached record should remain
            try await self.assertValidiCSC(exists: false, context: self.cuttlefishContext)
            do {
                try self.assertCachedEscrowRecord(exists: true, context: self.cuttlefishContext)
            }
        }

        // Now, for whatever reason, the bottleID changes. This should only ever happen if the peer changes.
        // To simulate this, perform an RPD without creating an escrow record.
        self.assertResetAndBecomeTrusted(context: self.cuttlefishContext, assertStartsUntrusted: false)
        try await self.assertValidiCSC(exists: false, context: self.cuttlefishContext)

        // And just like that, we're not rate-limited anymore
        try self.performEscrowCheck(context: self.cuttlefishContext,
                                    needsReenroll: true,
                                    rateLimitState: .notRateLimited,
                                    repairReason: .noRecordMatchingPeer,
                                    daysLeftOnRateLimit: 0,
                                    expectCacheFlowEnabled: true)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        // and getting the passcode again will create a record (and it definitely isn't the record we tried before)

        do {
            let recordUploadCallback = self.expectation(description: "Record upload should be called")
            self.mockSecureBackupAdapter.storeRecordListener = { recordCandidate in
                // Preserve blob for later examination
                XCTAssertNotEqual(recordBlob, recordCandidate.blob, "Record uploaded after RPD should be different than record attempted before")

                recordUploadCallback.fulfill()
                return nil
            }

            self.mockAppleKeyStoreAdapter.deviceUnlockedByPasscode()

            await fulfillment(of: [recordUploadCallback], timeout: 10)
            self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        }

        // These results should be data-driven
        self.fakeCuttlefishServer.returnEscrowCheckRepairReason = EscrowRepairReason.recordOk
        self.fakeCuttlefishServer.returnEscrowCheckNeedsRepair = false

        // And everything is fixed!
        try await self.assertValidiCSC(exists: true, context: self.cuttlefishContext)
        try self.performEscrowCheck(context: self.cuttlefishContext,
                                    needsReenroll: false,
                                    rateLimitState: .rateLimited,
                                    repairReason: .recordOk,
                                    daysLeftOnRateLimit: 180,
                                    expectCacheFlowEnabled: false)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testUnsupportedPlatforms() async throws {
#if os(tvOS)
        try XCTSkipIf(true, "tvOS is unsupported, but not via OCTAGON_PLATFORM_SUPPORTS_CACHE_FLOW")
#endif

        // We want peers to join in a non-escrow state!
        self.fakeCuttlefishServer.automaticallyCreateEscrowRecordsOnJoin = false

        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        // This shouldn't exist, because FakeCuttlefish won't have made it
        try await self.assertValidiCSC(exists: false, context: self.cuttlefishContext)

        // These results should be data-driven
        self.fakeCuttlefishServer.returnEscrowCheckRepairReason = EscrowRepairReason.noRecordMatchingPeer
        self.fakeCuttlefishServer.returnEscrowCheckNeedsRepair = true

        // First test: Escrow check will not enable cache flow on unsupported platforms
        try self.performEscrowCheck(context: self.cuttlefishContext,
                                    needsReenroll: true,
                                    rateLimitState: .notRateLimited,
                                    repairReason: .noRecordMatchingPeer,
                                    daysLeftOnRateLimit: 0,
                                    expectCacheFlowEnabled: OCTAGON_PLATFORM_SUPPORTS_CACHE_FLOW != 0)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        SecMockAKS.resetCacheFlow()

        // Second test: Passcode change ignored on unsupported platforms
        do {
            let recordUploadCallback = self.expectation(description: "Record upload should be called")
            recordUploadCallback.isInverted = OCTAGON_PLATFORM_SUPPORTS_CACHE_FLOW == 0
            self.mockSecureBackupAdapter.storeRecordListener = { _ in
                // TODO: remove when the response is data-driven
                self.fakeCuttlefishServer.returnEscrowCheckNeedsRepair = false
                recordUploadCallback.fulfill()
                return nil
            }

            self.mockAppleKeyStoreAdapter.devicePasscodeChanged()

            self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
            await fulfillment(of: [recordUploadCallback], timeout: 1)
        }
    }
}

#endif
