import XCTest

#if OCTAGON

class OctagonAccountMetadataClassCPersistenceTests: CloudKitKeychainSyncingMockXCTest {
    override static func setUp() {
        super.setUp()

        // Turn on NO_SERVER stuff
        securityd_init_local_spi()
    }

    func testSaveAndLoad() throws {
        XCTAssertThrowsError(try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext),
                             "Before doing anything, loading a non-existent account state should fail")

        let state: OTAccountMetadataClassC! = OTAccountMetadataClassC()
        state.peerID = "asdf"
        state.icloudAccountState = .ACCOUNT_AVAILABLE
        state.trustState = .TRUSTED
        state.cdpState = .ENABLED

        XCTAssertNoThrow(try state.saveToKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext), "saving to the keychain should work")

        do {
            let state2 = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext)
            XCTAssertNotNil(state2)
            XCTAssertEqual(state2.peerID, state.peerID, "peer ID persists through keychain")
            XCTAssertEqual(state2.icloudAccountState, state.icloudAccountState, "account state persists through keychain")
            XCTAssertEqual(state2.trustState, state.trustState, "trust state persists through keychain")
            XCTAssertEqual(state2.cdpState, state.cdpState, "cdp state persists through keychain")
        } catch {
            XCTFail("error loading from keychain: \(error)")
        }
    }

    func testSilentOverwrite() throws {
        XCTAssertThrowsError(try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext),
                             "Before doing anything, loading a non-existent account state should fail")

        let state: OTAccountMetadataClassC! = OTAccountMetadataClassC()
        state.peerID = "asdf"
        state.icloudAccountState = .ACCOUNT_AVAILABLE
        state.trustState = .TRUSTED

        XCTAssertNoThrow(try state.saveToKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext), "saving to the keychain should work")

        state.peerID = "no wait another peer id"
        state.icloudAccountState = .ACCOUNT_AVAILABLE
        state.trustState = .UNTRUSTED

        XCTAssertNoThrow(try state.saveToKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext), "saving to the keychain should work")

        do {
            let state2 = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext)
            XCTAssertNotNil(state2)
            XCTAssertEqual(state2.peerID, "no wait another peer id", "peer ID persists through keychain")
            XCTAssertEqual(state2.icloudAccountState, .ACCOUNT_AVAILABLE, "account state persists through keychain")
            XCTAssertEqual(state2.trustState, .UNTRUSTED, "trust state persists through keychain")
        } catch {
            XCTFail("error loading from keychain: \(error)")
        }
    }

    func testContainerIndependence() throws {
        let state1: OTAccountMetadataClassC! = OTAccountMetadataClassC()
        state1.peerID = "asdf"
        state1.icloudAccountState = .ACCOUNT_AVAILABLE
        state1.trustState = .TRUSTED

        let state2: OTAccountMetadataClassC! = OTAccountMetadataClassC()
        state2.peerID = "anotherPeerID"
        state2.icloudAccountState = .ACCOUNT_AVAILABLE
        state2.trustState = .UNTRUSTED

        XCTAssertNoThrow(try state1.saveToKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext), "saving to the keychain should work")
        XCTAssertNoThrow(try state2.saveToKeychain(forContainer: "second_container", contextID: OTDefaultContext), "saving to the keychain should work")

        do {
            let state1reloaded = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext)
            XCTAssertNotNil(state1reloaded)
            XCTAssertEqual(state1reloaded.peerID, state1.peerID, "peer ID persists through keychain")
            XCTAssertEqual(state1reloaded.icloudAccountState, state1.icloudAccountState, "account state persists through keychain")
            XCTAssertEqual(state1reloaded.trustState, state1.trustState, "trust state persists through keychain")
        } catch {
            XCTFail("error loading state1 from keychain: \(error)")
        }

        do {
            let state2reloaded = try OTAccountMetadataClassC.loadFromKeychain(forContainer: "second_container", contextID: OTDefaultContext)
            XCTAssertNotNil(state2reloaded)
            XCTAssertEqual(state2reloaded.peerID, state2.peerID, "peer ID persists through keychain")
            XCTAssertEqual(state2reloaded.icloudAccountState, state2.icloudAccountState, "account state persists through keychain")
            XCTAssertEqual(state2reloaded.trustState, state2.trustState, "trust state persists through keychain")
        } catch {
            XCTFail("error loading state2 from keychain: \(error)")
        }
    }

    func testContextIndependence() throws {
        let state1: OTAccountMetadataClassC! = OTAccountMetadataClassC()
        state1.peerID = "asdf"
        state1.icloudAccountState = .ACCOUNT_AVAILABLE
        state1.trustState = .TRUSTED

        let state2: OTAccountMetadataClassC! = OTAccountMetadataClassC()
        state2.peerID = "anotherPeerID"
        state2.icloudAccountState = .ACCOUNT_AVAILABLE
        state2.trustState = .UNTRUSTED

        XCTAssertNoThrow(try state1.saveToKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext), "saving to the keychain should work")
        XCTAssertNoThrow(try state2.saveToKeychain(forContainer: OTCKContainerName, contextID: "second_context"), "saving to the keychain should work")

        do {
            let state1reloaded = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext)
            XCTAssertNotNil(state1reloaded)
            XCTAssertEqual(state1reloaded.peerID, state1.peerID, "peer ID persists through keychain")
            XCTAssertEqual(state1reloaded.icloudAccountState, state1.icloudAccountState, "account state persists through keychain")
            XCTAssertEqual(state1reloaded.trustState, state1.trustState, "trust state persists through keychain")
        } catch {
            XCTFail("error loading state1 from keychain: \(error)")
        }

        do {
            let state2reloaded = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: "second_context")
            XCTAssertNotNil(state2reloaded)
            XCTAssertEqual(state2reloaded.peerID, state2.peerID, "peer ID persists through keychain")
            XCTAssertEqual(state2reloaded.icloudAccountState, state2.icloudAccountState, "account state persists through keychain")
            XCTAssertEqual(state2reloaded.trustState, state2.trustState, "trust state persists through keychain")
        } catch {
            XCTFail("error loading state2 from keychain: \(error)")
        }
    }

    func testLoadCorruptedAccountState() throws {
        XCTAssertThrowsError(try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext),
                             "Before doing anything, loading a non-existent account state should fail")

        let state: OTAccountMetadataClassC! = OTAccountMetadataClassC()
        state.peerID = "asdf"
        state.icloudAccountState = .ACCOUNT_AVAILABLE
        state.trustState = .TRUSTED

        XCTAssertNoThrow(try TestsObjectiveC.saveCoruptDataToKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext), "saving to the keychain should work")

        do {
            let state2 = try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext)
            XCTAssertNotNil(state2)
            XCTAssertNil(state2.peerID, "peerID should be nil")
            XCTAssertEqual(state2.icloudAccountState, OTAccountMetadataClassC_AccountState.UNKNOWN, "account state should be OTAccountMetadataClassC_AccountState_UNKNOWN")
            XCTAssertEqual(state2.trustState, OTAccountMetadataClassC_TrustState.UNKNOWN, "trust state should be OTAccountMetadataClassC_TrustState_UNKNOWN")
        } catch {
            XCTFail("error loading from keychain: \(error)")
        }
    }

    func testDeleteFromKeychain() throws {
        let state: OTAccountMetadataClassC! = OTAccountMetadataClassC()
        state.peerID = "asdf"
        state.icloudAccountState = .ACCOUNT_AVAILABLE
        state.trustState = .TRUSTED
        XCTAssertNoThrow(try state.saveToKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext), "saving to the keychain should work")

        let deleted: Bool = try OTAccountMetadataClassC.deleteFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext)
        XCTAssertTrue(deleted, "deleteFromKeychain should return true")
        XCTAssertThrowsError(try OTAccountMetadataClassC.loadFromKeychain(forContainer: OTCKContainerName, contextID: OTDefaultContext))
    }
}

#endif // OCTAGON
