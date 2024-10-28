#if OCTAGON

import Foundation

class OctagonPersonaTests: OctagonTestsBase {
    override func setUp() {
        OctagonSetSupportsPersonaMultiuser(true)
        super.setUp()
    }

    override func tearDown() {
        OctagonClearSupportsPersonaMultiuserOverride()
        TestsObjectiveC.clearInvocationCount()
        super.tearDown()
    }

    func testFetchContextsFromPrimaryPersona() throws {
        if TestsObjectiveC.isPlatformHomepod() {
            throw XCTSkip("HomePod does not support non-primary personas")
        }
        OctagonSetSupportsPersonaMultiuser(true)

        let primaryAltDSID = try XCTUnwrap(self.mockAuthKit.primaryAccount()?.altDSID)

        let secondAltDSID = "second_altdsid"
        let secondPersonaString = UUID().uuidString

        let primaryContext = try self.manager.context(forClientRPC: OTControlArguments(),
                                                      createIfMissing: true,
                                                      allowNonPrimaryAccounts: false)
        XCTAssertEqual(primaryContext.contextID, "defaultContext", "contextID on primary account should be exactly what was passed in")
        XCTAssertNil(primaryContext.activeAccount, "Should not set up an activeAccount on new contexts for the 'primary' persona")

        let primaryContextByAltDSID = try self.manager.context(forClientRPC: OTControlArguments(altDSID: primaryAltDSID),
                                                               createIfMissing: false,
                                                               allowNonPrimaryAccounts: false)
        XCTAssertEqual(primaryContext, primaryContextByAltDSID, "Fetching the primary account by altDSID should get the same object")

        let secondPrimaryContext = try self.manager.context(forClientRPC: OTControlArguments(containerName: OTCKContainerName,
                                                                                             contextID: "alternate_primary",
                                                                                             altDSID: nil),
                                                            createIfMissing: true,
                                                            allowNonPrimaryAccounts: false)
        XCTAssertEqual(secondPrimaryContext.contextID, "alternate_primary", "Fetching an alterante context on the  primary account should use contextID normally")
        XCTAssertNotNil(secondPrimaryContext.activeAccount, "Should set up an activeAccount on new alternate contexts for the 'primary' persona")

        XCTAssertThrowsError(try self.manager.context(forClientRPC: OTControlArguments(altDSID: secondAltDSID),
                                                      createIfMissing: true,
                                                      allowNonPrimaryAccounts: false)) { error in
            let nserror = error as NSError
            XCTAssertEqual(nserror.domain, OctagonErrorDomain, "Domain should be OctagonErrorDomain")
            XCTAssertEqual(nserror.code, OctagonError.noAppleAccount.rawValue, "Code should be NoAppleAccount")
        }

        let account = CloudKitAccount(altDSID: secondAltDSID, persona: secondPersonaString, hsa2: true, demo: false, accountStatus: .available, isPrimary: false, isDataSeparated: true)

        self.mockAuthKit.add(account)

        // Now we try to fetch the second context from the primary thread
        XCTAssertThrowsError(try self.manager.context(forClientRPC: OTControlArguments(altDSID: secondAltDSID),
                                                      createIfMissing: true,
                                                      allowNonPrimaryAccounts: false)) { error in
            let nserror = error as NSError
            XCTAssertEqual(nserror.domain, OctagonErrorDomain, "Domain should be OctagonErrorDomain")
            XCTAssertEqual(nserror.code, OctagonError.notSupported.rawValue, "Code should be not supported if allowNonPrimaryAccounts is false")
        }

        // Now we try to fetch the second context from the primary thread
        let secondContext = try self.manager.context(forClientRPC: OTControlArguments(altDSID: secondAltDSID),
                                                     createIfMissing: true,
                                                     allowNonPrimaryAccounts: true)
        XCTAssertNotNil(secondContext.activeAccount, "Should be an active account on a secondary altDSID")
        XCTAssertEqual(secondContext.contextID, "defaultContext_\(secondAltDSID)", "Actual contextID on secondary context should include altDSID")
    }

    func testFetchContextsFromSecondaryPersona() throws {
        if TestsObjectiveC.isPlatformHomepod() {
            throw XCTSkip("HomePod does not support non-primary personas")
        }
        let primaryAltDSID = try XCTUnwrap(self.mockAuthKit.primaryAltDSID())

        let secondAltDSID = "second_altdsid"
        let secondPersonaString = UUID().uuidString

        self.mockPersonaAdapter!.currentPersonaString = secondPersonaString
        self.mockPersonaAdapter!.isDefaultPersona = false

        XCTAssertThrowsError(try self.manager.context(forClientRPC: OTControlArguments(),
                                                      createIfMissing: true,
                                                      allowNonPrimaryAccounts: false)) { error in
            let nserror = error as NSError
            XCTAssertEqual(nserror.domain, OctagonErrorDomain, "Domain should be OctagonErrorDomain")
            XCTAssertEqual(nserror.code, OctagonError.noAppleAccount.rawValue, "Code should be correct")
        }

        let account = CloudKitAccount(altDSID: secondAltDSID, persona: secondPersonaString, hsa2: true, demo: false, accountStatus: .available, isPrimary: false, isDataSeparated: true)
        self.mockAuthKit.add(account)

        XCTAssertThrowsError(try self.manager.context(forClientRPC: OTControlArguments(),
                                                      createIfMissing: true,
                                                      allowNonPrimaryAccounts: false)) { error in
            let nserror = error as NSError
            XCTAssertEqual(nserror.domain, OctagonErrorDomain, "Domain should be OctagonErrorDomain")
            XCTAssertEqual(nserror.code, OctagonError.notSupported.rawValue, "Code should be not supported if allowNonPrimaryAccounts is false")
        }

        let secondContext = try self.manager.context(forClientRPC: OTControlArguments(),
                                                     createIfMissing: true,
                                                     allowNonPrimaryAccounts: true)
        XCTAssertNotNil(secondContext.activeAccount, "Should be an active account on a secondary context")
        XCTAssertEqual(secondContext.contextID, "defaultContext_\(secondAltDSID)", "Actual contextID on secondary context should include altDSID")

        // from the second persona, you shouldn't be able to find the primary altDSID
        XCTAssertThrowsError(try self.manager.context(forClientRPC: OTControlArguments(altDSID: primaryAltDSID),
                                                      createIfMissing: true,
                                                      allowNonPrimaryAccounts: false)) { error in
            let nserror = error as NSError
            XCTAssertEqual(nserror.domain, OctagonErrorDomain, "Domain should be OctagonErrorDomain")
            XCTAssertEqual(nserror.code, OctagonError.noAppleAccount.rawValue, "Code should be noAppleAccount if asking for the primary altDSID from a secondary persona")
        }
    }

    func testSignInFromNondefaultAltDSID() throws {
        if TestsObjectiveC.isPlatformHomepod() {
            throw XCTSkip("HomePod does not support non-primary personas")
        }
        let secondAccountAltDSID = UUID().uuidString
        let account = CloudKitAccount(altDSID: secondAccountAltDSID, persona: UUID().uuidString, hsa2: true, demo: false, accountStatus: .available, isPrimary: false, isDataSeparated: true)

        self.mockAuthKit.add(account)

        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // Primary account is signed out
        self.mockAuthKit.removeAccount(forPersona: OTMockPersonaAdapter.defaultMockPersonaString())

        self.startCKAccountStatusMock()

        // With no account, Octagon should go directly into 'NoAccount' while the persona adapter isn't configured
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // Sign in occurs for a non-primary altDSID, on the main persona, and is rejected because of the platform flag
        OctagonSetSupportsPersonaMultiuser(false)

        do {
            let signinExpectation = self.expectation(description: "signIn occurs")
            self.injectedOTManager?.appleAccountSigned(in: OTControlArguments(altDSID: secondAccountAltDSID)) { error in
                XCTAssertNotNil(error, "Sign in should have errored")
                if let nserror = error as NSError? {
                    XCTAssertEqual(nserror.domain, OctagonErrorDomain, "Error should be from OctagonErrorDomain")
                    XCTAssertEqual(nserror.code, OctagonError.notSupported.rawValue, "Error should be OctagonErrorNotSupported")
                }
                signinExpectation.fulfill()
            }
            self.wait(for: [signinExpectation], timeout: 3)

            let statusExpectation = self.expectation(description: "status occurs")
            self.injectedOTManager?.status(OTControlArguments(altDSID: secondAccountAltDSID)) { _, error in
                XCTAssertNotNil(error, "Status should have errored")
                if let nserror = error as NSError? {
                    XCTAssertEqual(nserror.domain, OctagonErrorDomain, "Error should be from OctagonErrorDomain")
                    XCTAssertEqual(nserror.code, OctagonError.notSupported.rawValue, "Error should be OctagonErrorNotSupported")
                }
                statusExpectation.fulfill()
            }
            self.wait(for: [statusExpectation], timeout: 3)
        }

        // But if we support other personas, this should do something
        OctagonSetSupportsPersonaMultiuser(true)

        do {
            let signinExpectation = self.expectation(description: "signIn occurs")
            self.injectedOTManager?.appleAccountSigned(in: OTControlArguments(altDSID: secondAccountAltDSID)) { error in
                XCTAssertNil(error, "Sign in should not have errored")
                signinExpectation.fulfill()
            }
            self.wait(for: [signinExpectation], timeout: 3)

            let statusExpectation = self.expectation(description: "status occurs")
            self.injectedOTManager?.status(OTControlArguments(altDSID: secondAccountAltDSID)) { _, error in
                XCTAssertNil(error, "Status should not have errored")
                statusExpectation.fulfill()
            }
            self.wait(for: [statusExpectation], timeout: 3)

            let secondaryAccountContext = try self.manager.context(forClientRPC: OTControlArguments(altDSID: secondAccountAltDSID),
                                                                   createIfMissing: false,
                                                                   allowNonPrimaryAccounts: true)

            // Check that we can load metadata for this context:
            let accountMetadata = try secondaryAccountContext.accountMetadataStore.loadOrCreateAccountMetadata()
            XCTAssertEqual(accountMetadata.altDSID, secondAccountAltDSID, "Metadata altDSID is correct")

            let activeAccount = try XCTUnwrap(secondaryAccountContext.activeAccount)
            XCTAssertEqual(secondaryAccountContext.contextID, "\(OTDefaultContext)_\(secondAccountAltDSID)", "contextID should match expected value")
            XCTAssertEqual(activeAccount.octagonContextID, "\(OTDefaultContext)_\(secondAccountAltDSID)", "account contextID should match expected value")
        }

        // And another sign-in doesn't change any contextIDs
        do {
            let signinExpectation = self.expectation(description: "signIn occurs")
            self.injectedOTManager?.appleAccountSigned(in: OTControlArguments(altDSID: secondAccountAltDSID)) { error in
                XCTAssertNil(error, "Sign in should not have errored")
                signinExpectation.fulfill()
            }
            self.wait(for: [signinExpectation], timeout: 3)

            let statusExpectation = self.expectation(description: "status occurs")
            self.injectedOTManager?.status(OTControlArguments(altDSID: secondAccountAltDSID)) { _, error in
                XCTAssertNil(error, "Status should not have errored")
                statusExpectation.fulfill()
            }
            self.wait(for: [statusExpectation], timeout: 3)

            let secondaryAccountContext = try self.manager.context(forClientRPC: OTControlArguments(altDSID: secondAccountAltDSID),
                                                                   createIfMissing: false,
                                                                   allowNonPrimaryAccounts: true)

            // Check that we can load metadata for this context:
            let accountMetadata = try secondaryAccountContext.accountMetadataStore.loadOrCreateAccountMetadata()
            XCTAssertEqual(accountMetadata.altDSID, secondAccountAltDSID, "Metadata altDSID is correct")

            let activeAccount = try XCTUnwrap(secondaryAccountContext.activeAccount)
            XCTAssertEqual(secondaryAccountContext.contextID, "\(OTDefaultContext)_\(secondAccountAltDSID)", "contextID should match expected value")
            XCTAssertEqual(activeAccount.octagonContextID, "\(OTDefaultContext)_\(secondAccountAltDSID)", "account contextID should match expected value")
        }
    }

    func testSignInFromNondefaultPersona() throws {
        if TestsObjectiveC.isPlatformHomepod() {
            throw XCTSkip("HomePod does not support non-primary personas")
        }
        let secondAccountAltDSID = UUID().uuidString
        let secondAccountPersona = UUID().uuidString
        let account = CloudKitAccount(altDSID: secondAccountAltDSID, persona: secondAccountPersona, hsa2: true, demo: false, accountStatus: .available, isPrimary: false, isDataSeparated: true)

        self.mockAuthKit.add(account)
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // Primary account is signed out
        self.mockAuthKit.removeAccount(forPersona: OTMockPersonaAdapter.defaultMockPersonaString())
        self.startCKAccountStatusMock()

        // With no account, Octagon should go directly into 'NoAccount' while the persona adapter isn't configured
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // Sign in occurs from some other Persona, and is rejected because of the platform flag
        self.mockPersonaAdapter!.currentPersonaString = secondAccountPersona
        self.mockPersonaAdapter!.isDefaultPersona = false
        OctagonSetSupportsPersonaMultiuser(false)

        do {
            let signinExpectation = self.expectation(description: "signIn occurs")
            self.injectedOTManager?.appleAccountSigned(in: OTControlArguments()) { error in
                XCTAssertNotNil(error, "Sign in should have errored")
                if let nserror = error as NSError? {
                    XCTAssertEqual(nserror.domain, OctagonErrorDomain, "Error should be from OctagonErrorDomain")
                    XCTAssertEqual(nserror.code, OctagonError.notSupported.rawValue, "Error should be from OctagonErrorNotSupported")
                }
                signinExpectation.fulfill()
            }
            self.wait(for: [signinExpectation], timeout: 3)

            let statusExpectation = self.expectation(description: "status occurs")
            self.injectedOTManager?.status(OTControlArguments()) { _, error in
                XCTAssertNotNil(error, "Status should have errored")
                if let nserror = error as NSError? {
                    XCTAssertEqual(nserror.domain, OctagonErrorDomain, "Error should be from OctagonErrorDomain")
                    XCTAssertEqual(nserror.code, OctagonError.notSupported.rawValue, "Error should be from OctagonErrorNotSupported")
                }
                statusExpectation.fulfill()
            }
            self.wait(for: [statusExpectation], timeout: 3)
        }

        // But if we support other personas, this should do something
        OctagonSetSupportsPersonaMultiuser(true)

        do {
            let signinExpectation = self.expectation(description: "signIn occurs")
            self.injectedOTManager?.appleAccountSigned(in: OTControlArguments()) { error in
                XCTAssertNil(error, "Sign in should not have errored")
                signinExpectation.fulfill()
            }
            self.wait(for: [signinExpectation], timeout: 3)

            let statusExpectation = self.expectation(description: "status occurs")
            self.injectedOTManager?.status(OTControlArguments()) { _, error in
                XCTAssertNil(error, "Status should not have errored")
                statusExpectation.fulfill()
            }
            self.wait(for: [statusExpectation], timeout: 3)
        }
    }

    func testMultiUserCFU() throws {
        if TestsObjectiveC.isPlatformHomepod() {
            throw XCTSkip("HomePod does not support non-primary personas")
        }
        OctagonSetSupportsPersonaMultiuser(true)
        let secondAccountAltDSID = UUID().uuidString
        let account = CloudKitAccount(altDSID: secondAccountAltDSID, persona: UUID().uuidString, hsa2: true, demo: false, accountStatus: .available, isPrimary: false, isDataSeparated: true)
        self.mockAuthKit.add(account)

        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // Primary account is signed out
        self.mockAuthKit.removeAccount(forPersona: OTMockPersonaAdapter.defaultMockPersonaString())

        self.startCKAccountStatusMock()

        // With no account, Octagon should go directly into 'NoAccount' while the persona adapter isn't configured
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        do {
            let signinExpectation = self.expectation(description: "signIn occurs")
            self.injectedOTManager?.appleAccountSigned(in: OTControlArguments(altDSID: secondAccountAltDSID)) { error in
                XCTAssertNil(error, "Sign in should not have errored")
                signinExpectation.fulfill()
            }
            self.wait(for: [signinExpectation], timeout: 3)

            let statusExpectation = self.expectation(description: "status occurs")
            self.injectedOTManager?.status(OTControlArguments(altDSID: secondAccountAltDSID)) { _, error in
                XCTAssertNil(error, "Status should not have errored")
                statusExpectation.fulfill()
            }
            self.wait(for: [statusExpectation], timeout: 3)

            let secondaryAccountContext = try self.manager.context(forClientRPC: OTControlArguments(altDSID: secondAccountAltDSID),
                                                                   createIfMissing: false,
                                                                   allowNonPrimaryAccounts: true)

            // Check that we can load metadata for this context:
            let accountMetadata = try secondaryAccountContext.accountMetadataStore.loadOrCreateAccountMetadata()
            XCTAssertEqual(accountMetadata.altDSID, secondAccountAltDSID, "Metadata altDSID is correct")

            let activeAccount = try XCTUnwrap(secondaryAccountContext.activeAccount)
            XCTAssertEqual(secondaryAccountContext.contextID, "\(OTDefaultContext)_\(secondAccountAltDSID)", "contextID should match expected value")
            XCTAssertEqual(activeAccount.octagonContextID, "\(OTDefaultContext)_\(secondAccountAltDSID)", "account contextID should match expected value")

            try secondaryAccountContext.followupHandler.postFollowUp(.stateRepair, activeAccount: secondaryAccountContext.activeAccount!)
            XCTAssertTrue(secondaryAccountContext.followupHandler.hasPosted(.stateRepair), "should have posted an repair CFU")
            XCTAssertFalse(self.cuttlefishContext.followupHandler.hasPosted(.stateRepair), "should NOT have posted an repair CFU")
        }
    }

    func testRestartSecdStateMachinesInflatePerAccount() throws {
        if TestsObjectiveC.isPlatformHomepod() {
            throw XCTSkip("HomePod does not support non-primary personas")
        }
        OctagonSetSupportsPersonaMultiuser(true)
        let secondAccountAltDSID = UUID().uuidString

        let account = CloudKitAccount(altDSID: secondAccountAltDSID, persona: UUID().uuidString, hsa2: true, demo: false, accountStatus: .available, isPrimary: false, isDataSeparated: true)
        self.mockAuthKit.add(account)

        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // Primary account is signed out
        self.mockAuthKit.removeAccount(forPersona: OTMockPersonaAdapter.defaultMockPersonaString())
        self.startCKAccountStatusMock()

        let primaryAccountContext = try self.manager.context(forClientRPC: OTControlArguments(),
                                                             createIfMissing: false,
                                                             allowNonPrimaryAccounts: true)
        // With no account, Octagon should go directly into 'NoAccount' while the persona adapter isn't configured
        primaryAccountContext.startOctagonStateMachine()
        self.assertEnters(context: primaryAccountContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        do {
            let signinExpectation = self.expectation(description: "signIn occurs")
            self.injectedOTManager?.appleAccountSigned(in: OTControlArguments(altDSID: secondAccountAltDSID)) { error in
                XCTAssertNil(error, "Sign in should not have errored")
                signinExpectation.fulfill()
            }
            self.wait(for: [signinExpectation], timeout: 3)

            let statusExpectation = self.expectation(description: "status occurs")
            self.injectedOTManager?.status(OTControlArguments(altDSID: secondAccountAltDSID)) { _, error in
                XCTAssertNil(error, "Status should not have errored")
                statusExpectation.fulfill()
            }
            self.wait(for: [statusExpectation], timeout: 3)

            let secondaryAccountContext = try self.manager.context(forClientRPC: OTControlArguments(altDSID: secondAccountAltDSID),
                                                                   createIfMissing: false,
                                                                   allowNonPrimaryAccounts: true)

            // Check that we can load metadata for this context:
            let accountMetadata = try secondaryAccountContext.accountMetadataStore.loadOrCreateAccountMetadata()
            XCTAssertEqual(accountMetadata.altDSID, secondAccountAltDSID, "Metadata altDSID is correct")

            let activeAccount = try XCTUnwrap(secondaryAccountContext.activeAccount)
            XCTAssertEqual(secondaryAccountContext.contextID, "\(OTDefaultContext)_\(secondAccountAltDSID)", "contextID should match expected value")
            XCTAssertEqual(activeAccount.octagonContextID, "\(OTDefaultContext)_\(secondAccountAltDSID)", "account contextID should match expected value")
        }

        do {
            let signinExpectation = self.expectation(description: "signIn occurs")
            self.injectedOTManager?.appleAccountSigned(in: OTControlArguments(altDSID: secondAccountAltDSID)) { error in
                XCTAssertNil(error, "Sign in should not have errored")
                signinExpectation.fulfill()
            }
            self.wait(for: [signinExpectation], timeout: 3)

            let statusExpectation = self.expectation(description: "status occurs")
            self.injectedOTManager?.status(OTControlArguments(altDSID: secondAccountAltDSID)) { _, error in
                XCTAssertNil(error, "Status should not have errored")
                statusExpectation.fulfill()
            }
            self.wait(for: [statusExpectation], timeout: 3)

            let secondaryAccountContext = try self.manager.context(forClientRPC: OTControlArguments(altDSID: secondAccountAltDSID),
                                                                   createIfMissing: false,
                                                                   allowNonPrimaryAccounts: true)

            // Check that we can load metadata for this context:
            let accountMetadata = try secondaryAccountContext.accountMetadataStore.loadOrCreateAccountMetadata()
            XCTAssertEqual(accountMetadata.altDSID, secondAccountAltDSID, "Metadata altDSID is correct")

            let activeAccount = try XCTUnwrap(secondaryAccountContext.activeAccount)
            XCTAssertEqual(secondaryAccountContext.contextID, "\(OTDefaultContext)_\(secondAccountAltDSID)", "contextID should match expected value")
            XCTAssertEqual(activeAccount.octagonContextID, "\(OTDefaultContext)_\(secondAccountAltDSID)", "account contextID should match expected value")

            // simulate restart
            self.manager.removeContext(forContainerName: secondaryAccountContext.containerName, contextID: secondaryAccountContext.contextID)
            self.manager.initializeOctagon()

            let reFetchedSecondAccount = try self.manager.context(forClientRPC: OTControlArguments(altDSID: secondAccountAltDSID),
                                                                  createIfMissing: false,
                                                                  allowNonPrimaryAccounts: true)

            self.assertEnters(context: reFetchedSecondAccount, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)

            // Check that we can load metadata for this context:
            let secondAccountMetadata = try reFetchedSecondAccount.accountMetadataStore.loadOrCreateAccountMetadata()
            XCTAssertEqual(secondAccountMetadata.altDSID, secondAccountAltDSID, "second metadata altDSID is correct")

            let secondActiveAccount = try XCTUnwrap(reFetchedSecondAccount.activeAccount)
            XCTAssertEqual(reFetchedSecondAccount.contextID, "\(OTDefaultContext)_\( secondAccountAltDSID)", "secondary contextID should match expected value")
            XCTAssertEqual(secondActiveAccount.octagonContextID, "\(OTDefaultContext)_\( secondAccountAltDSID)", "secondary account contextID should match expected value")
        }
    }

    func testResetInNondefaultAltDSID() throws {
        if TestsObjectiveC.isPlatformHomepod() {
            throw XCTSkip("HomePod does not support non-primary personas")
        }
        let secondAccountAltDSID = UUID().uuidString
        let account = CloudKitAccount(altDSID: secondAccountAltDSID, persona: UUID().uuidString, hsa2: true, demo: false, accountStatus: .available, isPrimary: false, isDataSeparated: true)
        self.mockAuthKit.add(account)

        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // Primary account is signed out
        self.mockAuthKit.removeAccount(forPersona: OTMockPersonaAdapter.defaultMockPersonaString())

        self.startCKAccountStatusMock()

        // With no account, Octagon should go directly into 'NoAccount' while the persona adapter isn't configured
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        OctagonSetSupportsPersonaMultiuser(true)

        let secondaryAccountContext: OTCuttlefishContext
        do {
            let signinExpectation = self.expectation(description: "signIn occurs")
            self.injectedOTManager?.appleAccountSigned(in: OTControlArguments(altDSID: secondAccountAltDSID)) { error in
                XCTAssertNil(error, "Sign in should not have errored")
                signinExpectation.fulfill()
            }
            self.wait(for: [signinExpectation], timeout: 3)

            let statusExpectation = self.expectation(description: "status occurs")
            self.injectedOTManager?.status(OTControlArguments(altDSID: secondAccountAltDSID)) { _, error in
                XCTAssertNil(error, "Status should not have errored")
                statusExpectation.fulfill()
            }
            self.wait(for: [statusExpectation], timeout: 3)

            let resetExpectation = self.expectation(description: "status occurs")
            self.injectedOTManager?.resetAndEstablish(OTControlArguments(altDSID: secondAccountAltDSID), resetReason: CuttlefishResetReason.testGenerated) { error in
                XCTAssertNil(error, "resetAndEstablish should not have errored")
                resetExpectation.fulfill()
            }

            self.wait(for: [resetExpectation], timeout: 100)

            secondaryAccountContext = try self.manager.context(forClientRPC: OTControlArguments(altDSID: secondAccountAltDSID),
                                                               createIfMissing: false,
                                                               allowNonPrimaryAccounts: true)
            self.assertEnters(context: secondaryAccountContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
            self.assertConsidersSelfTrusted(context: secondaryAccountContext)

            // Check that we can load metadata for this context:
            let accountMetadata = try secondaryAccountContext.accountMetadataStore.loadOrCreateAccountMetadata()
            XCTAssertEqual(accountMetadata.altDSID, secondAccountAltDSID, "Metadata altDSID is correct")
            let peerID = try XCTUnwrap(accountMetadata.peerID)

            // But, if we create a new metadata store with a different persona state, we can't load it (on platforms that support personas)
            // KEYCHAIN_SUPPORTS_PERSONA_MULTIUSER
            #if !os(tvOS)
            let activeAccount = try XCTUnwrap(secondaryAccountContext.activeAccount)

            do {
                _ = try OTAccountMetadataClassC.loadFromKeychain(forContainer: activeAccount.cloudkitContainerName,
                                                                     contextID: activeAccount.octagonContextID,
                                                                     personaAdapter: self.mockPersonaAdapter!,
                                                                     personaUniqueString: activeAccount.personaUniqueString)
            } catch {
                XCTFail("didn't expect to throw an error")
            }

            #if os(watchOS)
                XCTAssertNoThrow(try OTAccountMetadataClassC.loadFromKeychain(forContainer: activeAccount.cloudkitContainerName, contextID: activeAccount.octagonContextID, personaAdapter: self.mockPersonaAdapter!, personaUniqueString: nil))
            #else // watchOS
                XCTAssertThrowsError(try OTAccountMetadataClassC.loadFromKeychain(forContainer: activeAccount.cloudkitContainerName,
                                                                              contextID: activeAccount.octagonContextID,
                                                                              personaAdapter: self.mockPersonaAdapter!,
                                                                              personaUniqueString: nil))
            #endif // watchOS

            // Ensure that the ego keys are in the right musr.
            self.mockPersonaAdapter!.prepareThreadForKeychainAPIUse(forPersonaIdentifier: activeAccount.personaUniqueString)
            _ = try XCTUnwrap(loadEgoKeysSync(peerID: peerID))

            self.mockPersonaAdapter!.prepareThreadForKeychainAPIUse(forPersonaIdentifier: nil)

            #if os(watchOS)
                XCTAssertNoThrow(try loadEgoKeysSync(peerID: peerID))
            #else // watchOS
                XCTAssertThrowsError(try loadEgoKeysSync(peerID: peerID)) { error in
                    print(error)
                }
            #endif // watchOS

            #endif  // !tvOS
        }

        // and can another context join via escrow recovery?

        do {
            let arguments = OTControlArguments(containerName: OTCKContainerName, contextID: "escrow_joiner", altDSID: secondAccountAltDSID)

            let signinExpectation = self.expectation(description: "signIn occurs")
            self.injectedOTManager?.appleAccountSigned(in: arguments) { error in
                XCTAssertNil(error, "Sign in should not have errored")
                signinExpectation.fulfill()
            }
            self.wait(for: [signinExpectation], timeout: 3)

            let statusExpectation = self.expectation(description: "status occurs")
            self.injectedOTManager?.status(arguments) { _, error in
                XCTAssertNil(error, "Status should not have errored")
                statusExpectation.fulfill()
            }
            self.wait(for: [statusExpectation], timeout: 3)

            self.assertJoinViaOTCliqueEscrowRecovery(joiningArguments: arguments, sponsor: secondaryAccountContext)

            let escrowJoiner = try self.manager.context(forClientRPC: arguments,
                                                        createIfMissing: false,
                                                        allowNonPrimaryAccounts: true)
            self.assertEnters(context: escrowJoiner, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
            self.assertConsidersSelfTrusted(context: escrowJoiner)
        }
    }

    func testSignInFromNondefaultAltDSIDSignOutPrimary() throws {
        if TestsObjectiveC.isPlatformHomepod() {
            throw XCTSkip("HomePod does not support non-primary personas")
        }
        OctagonSetSupportsPersonaMultiuser(true)
        let secondAccountAltDSID = UUID().uuidString
        let account = CloudKitAccount(altDSID: secondAccountAltDSID, persona: UUID().uuidString, hsa2: true, demo: false, accountStatus: .available, isPrimary: false, isDataSeparated: true)
        self.mockAuthKit.add(account)

        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        self.startCKAccountStatusMock()

        let primaryAltDSID = try XCTUnwrap(self.mockAuthKit.primaryAltDSID())
        let primaryContext = try self.manager.context(forClientRPC: OTControlArguments(altDSID: primaryAltDSID),
                                                      createIfMissing: false,
                                                      allowNonPrimaryAccounts: true)
        try primaryContext.setCDPEnabled()
        primaryContext.startOctagonStateMachine()
        self.assertEnters(context: primaryContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        do {
            let secondaryAccountContext = try self.manager.context(forClientRPC: OTControlArguments(altDSID: secondAccountAltDSID),
                                                                   createIfMissing: true,
                                                                   allowNonPrimaryAccounts: true)
            try secondaryAccountContext.setCDPEnabled()
            secondaryAccountContext.startOctagonStateMachine()

            let signinExpectation = self.expectation(description: "signIn occurs")
            self.injectedOTManager?.appleAccountSigned(in: OTControlArguments(altDSID: secondAccountAltDSID)) { error in
                XCTAssertNil(error, "Sign in should not have errored")
                signinExpectation.fulfill()
            }
            self.wait(for: [signinExpectation], timeout: 3)

            self.assertEnters(context: secondaryAccountContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

            let statusExpectation = self.expectation(description: "status occurs")
            self.injectedOTManager?.status(OTControlArguments(altDSID: secondAccountAltDSID)) { _, error in
                XCTAssertNil(error, "Status should not have errored")
                statusExpectation.fulfill()
            }
            self.wait(for: [statusExpectation], timeout: 3)

            // now sign out the primary account
            let signoutExpectation = self.expectation(description: "primary signOut occurs")
            self.injectedOTManager?.appleAccountSignedOut(OTControlArguments(altDSID: primaryAltDSID)) { error in
                XCTAssertNil(error, "Sign out should not have errored")
                signoutExpectation.fulfill()
            }
            self.wait(for: [signoutExpectation], timeout: 3)
            self.assertEnters(context: primaryContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

            // Check that we can load metadata for this context:
            let accountMetadata = try secondaryAccountContext.accountMetadataStore.loadOrCreateAccountMetadata()
            XCTAssertEqual(accountMetadata.altDSID, secondAccountAltDSID, "Metadata altDSID is correct")

            let activeAccount = try XCTUnwrap(secondaryAccountContext.activeAccount)
            XCTAssertEqual(secondaryAccountContext.contextID, "\(OTDefaultContext)_\(secondAccountAltDSID)", "contextID should match expected value")
            XCTAssertEqual(activeAccount.octagonContextID, "\(OTDefaultContext)_\(secondAccountAltDSID)", "account contextID should match expected value")

            self.assertEnters(context: secondaryAccountContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        }

        // And another sign-in doesn't change any contextIDs
        do {
            let signinExpectation = self.expectation(description: "signIn occurs")
            self.injectedOTManager?.appleAccountSigned(in: OTControlArguments(altDSID: secondAccountAltDSID)) { error in
                XCTAssertNil(error, "Sign in should not have errored")
                signinExpectation.fulfill()
            }
            self.wait(for: [signinExpectation], timeout: 3)

            let statusExpectation = self.expectation(description: "status occurs")
            self.injectedOTManager?.status(OTControlArguments(altDSID: secondAccountAltDSID)) { _, error in
                XCTAssertNil(error, "Status should not have errored")
                statusExpectation.fulfill()
            }
            self.wait(for: [statusExpectation], timeout: 3)

            let secondaryAccountContext = try self.manager.context(forClientRPC: OTControlArguments(altDSID: secondAccountAltDSID),
                                                                   createIfMissing: false,
                                                                   allowNonPrimaryAccounts: true)

            // Check that we can load metadata for this context:
            let accountMetadata = try secondaryAccountContext.accountMetadataStore.loadOrCreateAccountMetadata()
            XCTAssertEqual(accountMetadata.altDSID, secondAccountAltDSID, "Metadata altDSID is correct")

            let activeAccount = try XCTUnwrap(secondaryAccountContext.activeAccount)
            XCTAssertEqual(secondaryAccountContext.contextID, "\(OTDefaultContext)_\(secondAccountAltDSID)", "contextID should match expected value")
            XCTAssertEqual(activeAccount.octagonContextID, "\(OTDefaultContext)_\(secondAccountAltDSID)", "account contextID should match expected value")

            self.assertEnters(context: secondaryAccountContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

            // now sign out the secondary account
            let signoutExpectation = self.expectation(description: "second account signed out occurs")
            self.injectedOTManager?.appleAccountSignedOut(OTControlArguments(altDSID: secondAccountAltDSID)) { error in
                XCTAssertNil(error, "Sign out of secondary account should not have errored")
                signoutExpectation.fulfill()
            }
            self.wait(for: [signoutExpectation], timeout: 3)
            self.assertEnters(context: secondaryAccountContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        }

        // now sign into a secondary account, then sign into a primary account
        do {
            // second account signs in
            let signinExpectation = self.expectation(description: "signIn occurs")
            self.injectedOTManager?.appleAccountSigned(in: OTControlArguments(altDSID: secondAccountAltDSID)) { error in
                XCTAssertNil(error, "Sign in should not have errored")
                signinExpectation.fulfill()
            }
            self.wait(for: [signinExpectation], timeout: 3)

            let statusExpectation = self.expectation(description: "status occurs")
            self.injectedOTManager?.status(OTControlArguments(altDSID: secondAccountAltDSID)) { _, error in
                XCTAssertNil(error, "Status should not have errored")
                statusExpectation.fulfill()
            }
            self.wait(for: [statusExpectation], timeout: 3)

            let secondaryAccountContext = try self.manager.context(forClientRPC: OTControlArguments(altDSID: secondAccountAltDSID),
                                                                   createIfMissing: false,
                                                                   allowNonPrimaryAccounts: true)

            // Check that we can load metadata for this context:
            let accountMetadata = try secondaryAccountContext.accountMetadataStore.loadOrCreateAccountMetadata()
            try secondaryAccountContext.setCDPEnabled()

            XCTAssertEqual(accountMetadata.altDSID, secondAccountAltDSID, "Metadata altDSID is correct")

            let activeAccount = try XCTUnwrap(secondaryAccountContext.activeAccount)
            XCTAssertEqual(secondaryAccountContext.contextID, "\(OTDefaultContext)_\(secondAccountAltDSID)", "contextID should match expected value")
            XCTAssertEqual(activeAccount.octagonContextID, "\(OTDefaultContext)_\(secondAccountAltDSID)", "account contextID should match expected value")

            self.assertEnters(context: secondaryAccountContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

            // now sign in the primary account
            let primarySigninExpectation = self.expectation(description: "primary signIn occurs")
            self.injectedOTManager?.appleAccountSigned(in: OTControlArguments(altDSID: primaryAltDSID)) { error in
                XCTAssertNil(error, "Sign out should not have errored")
                primarySigninExpectation.fulfill()
            }
            try primaryContext.setCDPEnabled()

            self.wait(for: [primarySigninExpectation], timeout: 3)
            self.assertEnters(context: primaryContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

            // now sign out the primary account
            let signoutExpectation = self.expectation(description: "primary signOut occurs")
            self.injectedOTManager?.appleAccountSignedOut(OTControlArguments(altDSID: primaryAltDSID)) { error in
                XCTAssertNil(error, "Sign out should not have errored")
                signoutExpectation.fulfill()
            }
            self.wait(for: [signoutExpectation], timeout: 3)
            self.assertEnters(context: primaryContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

            // refetch secondaryAccount's context object
            let refetchedSecondAccountContext = try self.manager.context(forClientRPC: OTControlArguments(altDSID: secondAccountAltDSID),
                                                                         createIfMissing: false,
                                                                         allowNonPrimaryAccounts: true)

            // Check that we can load metadata for this context:
            let refetchedAccountMetadata = try refetchedSecondAccountContext.accountMetadataStore.loadOrCreateAccountMetadata()

            XCTAssertEqual(refetchedAccountMetadata.altDSID, secondAccountAltDSID, "Metadata altDSID is correct")

            let refetchedActiveAccount = try XCTUnwrap(refetchedSecondAccountContext.activeAccount)
            XCTAssertEqual(refetchedSecondAccountContext.contextID, "\(OTDefaultContext)_\(secondAccountAltDSID)", "contextID should match expected value")
            XCTAssertEqual(refetchedActiveAccount.octagonContextID, "\(OTDefaultContext)_\(secondAccountAltDSID)", "account contextID should match expected value")

            self.assertEnters(context: refetchedSecondAccountContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

            // sign primary back in again
            // now sign in the primary account
            let expectation = self.expectation(description: "primary signIn occurs")
            self.injectedOTManager?.appleAccountSigned(in: OTControlArguments(altDSID: primaryAltDSID)) { error in
                XCTAssertNil(error, "Sign out should not have errored")
                expectation.fulfill()
            }
            self.wait(for: [expectation], timeout: 3)
            try primaryContext.setCDPEnabled()

            self.assertEnters(context: primaryContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        }
    }

    func testNondefaultPersonaSignInOutCycle() throws {
        if TestsObjectiveC.isPlatformHomepod() {
            throw XCTSkip("HomePod does not support non-primary personas")
        }
        let secondAccount = CloudKitAccount(altDSID: UUID().uuidString, appleAccountID: UUID().uuidString, persona: UUID().uuidString, hsa2: true, demo: false, accountStatus: .available, isPrimary: false, isDataSeparated: true)

        self.mockAuthKit.add(secondAccount)
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // Primary account is signed out
        self.mockAuthKit.removeAccount(forPersona: OTMockPersonaAdapter.defaultMockPersonaString())
        self.startCKAccountStatusMock()

        // With no account, Octagon should go directly into 'NoAccount' while the persona adapter isn't configured
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        OctagonSetSupportsPersonaMultiuser(true)

        do {
            let signinExpectation = self.expectation(description: "signIn occurs")
            self.injectedOTManager?.appleAccountSigned(in: OTControlArguments(altDSID: secondAccount.altDSID)) { error in
                XCTAssertNil(error, "Sign in should not have errored")
                signinExpectation.fulfill()
            }
            self.wait(for: [signinExpectation], timeout: 3)

            let statusExpectation = self.expectation(description: "status occurs")
            self.injectedOTManager?.status(OTControlArguments(altDSID: secondAccount.altDSID)) { _, error in
                XCTAssertNil(error, "Status should not have errored")
                statusExpectation.fulfill()
            }
            self.wait(for: [statusExpectation], timeout: 3)

            let secondaryAccountContext = try self.manager.context(forClientRPC: OTControlArguments(altDSID: secondAccount.altDSID),
                                                                   createIfMissing: false,
                                                                   allowNonPrimaryAccounts: true)

            secondaryAccountContext.startOctagonStateMachine()

            // Check that we can load metadata for this context:
            let accountMetadata = try secondaryAccountContext.accountMetadataStore.loadOrCreateAccountMetadata()
            XCTAssertEqual(accountMetadata.altDSID, secondAccount.altDSID, "Metadata altDSID is correct")

            let activeAccount = try XCTUnwrap(secondaryAccountContext.activeAccount)
            XCTAssertEqual(secondaryAccountContext.contextID, "\(OTDefaultContext)_\(secondAccount.altDSID)", "contextID should match expected value")
            XCTAssertEqual(activeAccount.octagonContextID, "\(OTDefaultContext)_\(secondAccount.altDSID)", "account contextID should match expected value")
            XCTAssertEqual(activeAccount.appleAccountID, secondAccount.appleAccountID, "activeAccount should be configured with correct accountID")
            XCTAssertEqual(activeAccount.personaUniqueString, secondAccount.persona, "activeAccount should be configured with correct persona")

            self.assertEnters(context: secondaryAccountContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)

            XCTAssertEqual((secondaryAccountContext.accountStateTracker as? CKKSAccountStateTracker)?.container.options.accountOverrideInfo?.altDSID, secondAccount.altDSID, "CKContainer should be configured with correct altDSID")

            // now sign out the secondary account
            let signoutExpectation = self.expectation(description: "second account signed out occurs")
            self.injectedOTManager?.appleAccountSignedOut(OTControlArguments(altDSID: secondAccount.altDSID)) { error in
                XCTAssertNil(error, "Sign out of secondary account should not have errored")
                signoutExpectation.fulfill()
            }
            self.wait(for: [signoutExpectation], timeout: 3)
            self.assertEnters(context: secondaryAccountContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        } catch {
            print(error)
            XCTFail("oh no")
        }

        // The user signs in again, which changes the apple account ID and persona ID
        let secondAccountAgain = CloudKitAccount(altDSID: secondAccount.altDSID, appleAccountID: UUID().uuidString, persona: UUID().uuidString, hsa2: true, demo: false, accountStatus: .available, isPrimary: false, isDataSeparated: true)

        self.mockAuthKit.removeAccount(forPersona: secondAccount.persona)
        self.mockAuthKit.add(secondAccountAgain)

        do {
            let signinExpectation = self.expectation(description: "signIn occurs")
            self.injectedOTManager?.appleAccountSigned(in: OTControlArguments(altDSID: secondAccountAgain.altDSID)) { error in
                XCTAssertNil(error, "Sign in should not have errored")
                signinExpectation.fulfill()
            }
            self.wait(for: [signinExpectation], timeout: 3)

            let statusExpectation = self.expectation(description: "status occurs")
            self.injectedOTManager?.status(OTControlArguments(altDSID: secondAccountAgain.altDSID)) { _, error in
                XCTAssertNil(error, "Status should not have errored")
                statusExpectation.fulfill()
            }
            self.wait(for: [statusExpectation], timeout: 3)

            let secondaryAccountContext = try self.manager.context(forClientRPC: OTControlArguments(altDSID: secondAccountAgain.altDSID),
                                                                   createIfMissing: false,
                                                                   allowNonPrimaryAccounts: true)

            // Check that we can load metadata for this context:
            let accountMetadata = try secondaryAccountContext.accountMetadataStore.loadOrCreateAccountMetadata()
            XCTAssertEqual(accountMetadata.altDSID, secondAccountAgain.altDSID, "Metadata altDSID is correct")

            let activeAccount = try XCTUnwrap(secondaryAccountContext.activeAccount)
            XCTAssertEqual(secondaryAccountContext.contextID, "\(OTDefaultContext)_\(secondAccountAgain.altDSID)", "contextID should match expected value")
            XCTAssertEqual(activeAccount.octagonContextID, "\(OTDefaultContext)_\(secondAccountAgain.altDSID)", "account contextID should match expected value")
            XCTAssertEqual(activeAccount.appleAccountID, secondAccountAgain.appleAccountID, "activeAccount should be configured with correct accountID")
            XCTAssertEqual(activeAccount.personaUniqueString, secondAccountAgain.persona, "activeAccount should be configured with correct persona")

            self.assertEnters(context: secondaryAccountContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)

            XCTAssertEqual((secondaryAccountContext.accountStateTracker as? CKKSAccountStateTracker)?.container.options.accountOverrideInfo?.altDSID, secondAccountAgain.altDSID, "CKContainer should be configured with correct altDSID")
        }
    }

    func testMultipleNonDataSeparatedAccounts() throws {
        if TestsObjectiveC.isPlatformHomepod() {
            throw XCTSkip("HomePod does not support non-primary personas")
        }
        OctagonSetSupportsPersonaMultiuser(true)

        let secondAccountAltDSID = UUID().uuidString
        let secondAccount = CloudKitAccount(altDSID: secondAccountAltDSID, persona: nil, hsa2: true, demo: false, accountStatus: .available, isPrimary: false, isDataSeparated: false)

        self.mockAuthKit.add(secondAccount)

        let thirdAccountAltDSID = UUID().uuidString
        let thirdAccount = CloudKitAccount(altDSID: thirdAccountAltDSID, persona: nil, hsa2: true, demo: false, accountStatus: .available, isPrimary: false, isDataSeparated: false)
        self.mockAuthKit.add(thirdAccount)

        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        self.manager.initializeOctagon()

        let secondContextByAltDSID = try self.manager.context(forClientRPC: OTControlArguments(altDSID: secondAccountAltDSID),
                                                              createIfMissing: false,
                                                              allowNonPrimaryAccounts: true)

        XCTAssertNotNil(secondContextByAltDSID, "second account's context should not be nil")
        XCTAssertTrue(secondContextByAltDSID.contextID.contains(secondAccountAltDSID), "second account contextID should have its altdsid in the name")
        let thirdContextByAltDSID = try self.manager.context(forClientRPC: OTControlArguments(altDSID: thirdAccountAltDSID),
                                                             createIfMissing: false,
                                                             allowNonPrimaryAccounts: true)

        XCTAssertNotNil(thirdContextByAltDSID, "third account's context should not be nil")
        XCTAssertTrue(thirdContextByAltDSID.contextID.contains(thirdAccountAltDSID), "third account contextID should have its altdsid in the name")

        let arguments = OTControlArguments(altDSID: OTMockPersonaAdapter.defaultMockPersonaString())
        let primaryContext = try self.manager.context(forClientRPC: arguments,
                                                      createIfMissing: false,
                                                      allowNonPrimaryAccounts: true)

        XCTAssertNotNil(primaryContext, "primary account's context should not be nil")
        XCTAssertEqual(primaryContext.contextID, OTDefaultContext, "primary account contextID should be equal to OTDefaultContext")
    }

    func testAccountStoreRetryDueToXPCInvalidationError() throws {
        let actualAdapter = OTAccountsActualAdapter()
        TestsObjectiveC.setACAccountStoreWithInvalidationError(actualAdapter)

        XCTAssertThrowsError(try actualAdapter.findAccount(forCurrentThread: self.mockPersonaAdapter!, optionalAltDSID: nil, cloudkitContainerName: OTCKContainerName, octagonContextID: OTDefaultContext), "expect an error to be thrown")

        XCTAssertEqual(TestsObjectiveC.getInvocationCount(), 6, "should have been invoked 6 times")
    }

    func testAccountStoreRetryDueToRandomError() throws {
        let actualAdapter = OTAccountsActualAdapter()
        TestsObjectiveC.setACAccountStoreWithRandomError(actualAdapter)

        XCTAssertThrowsError(try actualAdapter.findAccount(forCurrentThread: self.mockPersonaAdapter!, optionalAltDSID: nil, cloudkitContainerName: OTCKContainerName, octagonContextID: OTDefaultContext), "expect an error to be thrown")

        XCTAssertEqual(TestsObjectiveC.getInvocationCount(), 1, "should have been invoked 1 times")
    }
}

#endif
