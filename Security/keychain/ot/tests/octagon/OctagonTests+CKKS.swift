#if OCTAGON

class OctagonCKKSTests: OctagonTestsBase {
    var previousKeychainAccessGroups: [String]!

    override func setUp() {
        // These tests would like to examine the behavior of a CKKS user-controlled-view
        if self.mockDeviceInfo == nil {
            let actualDeviceAdapter = OTDeviceInformationActualAdapter()
            self.mockDeviceInfo = OTMockDeviceInfoAdapter(modelID: actualDeviceAdapter.modelID(),
                                                          deviceName: actualDeviceAdapter.deviceName(),
                                                          serialNumber: NSUUID().uuidString,
                                                          osVersion: actualDeviceAdapter.osVersion())
        }

        if self.mockDeviceInfo.mockModelID.contains("AppleTV") {
            self.intendedCKKSZones = Set([
                CKRecordZone.ID(zoneName: "LimitedPeersAllowed"),
            ])
        } else {
            self.intendedCKKSZones = Set([
                CKRecordZone.ID(zoneName: "LimitedPeersAllowed"),
                CKRecordZone.ID(zoneName: "Manatee"),
                CKRecordZone.ID(zoneName: "Passwords"),
                CKRecordZone.ID(zoneName: "SecureObjectSync"),
                CKRecordZone.ID(zoneName: "Backstop"),
            ])
        }

        super.setUp()

        XCTAssertTrue(self.cuttlefishContext.viewManager!.useCKKSViewsFromPolicy(), "CKKS should be configured to listen to policy-based views")

        // Allow ourselves to add safari items
        self.previousKeychainAccessGroups = (SecAccessGroupsGetCurrent()?.takeUnretainedValue()) as? [String]
        SecAccessGroupsSetCurrent((self.previousKeychainAccessGroups + ["com.apple.cfnetwork", "com.apple.sbd"]) as CFArray)
    }

    override func tearDown() {
        SecAccessGroupsSetCurrent(self.previousKeychainAccessGroups as CFArray)
        super.tearDown()
    }

    func testHandleSBDItemAddSort() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        #if os(tvOS)
        // do not continue: TVs don't sync sos items.
        #else

        self.expectCKModifyItemRecords(1, currentKeyPointerRecords: 1, zoneID: CKRecordZone.ID(zoneName: "SecureObjectSync"))
        self.addGenericPassword("asdf",
                                account: "SBD-test",
                                accessGroup: "com.apple.sbd")

        self.verifyDatabaseMocks()
        #endif // tvos test skip
    }

    func testHandleCKKSKeyHierarchyConflictOnEstablish() throws {
        self.startCKAccountStatusMock()

        // Right after CKKS fetches for the first time, insert a new key hierarchy into CloudKit
        self.silentFetchesAllowed = false
        self.expectCKFetchAndRun {
            self.putFakeKeyHierarchiesInCloudKit()
            self.putFakeDeviceStatusesInCloudKit()
            self.silentFetchesAllowed = true
        }

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

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

        // and all subCKKSes should enter waitfortlk, as they don't have the TLKs uploaded by the other peer
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLK, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func testHandleCKKSKeyHierarchyConflictOnEstablishWithKeys() throws {
        self.startCKAccountStatusMock()

        // Right after CKKS fetches for the first time, insert a new key hierarchy into CloudKit
        self.silentFetchesAllowed = false
        self.expectCKFetchAndRun {
            self.putFakeKeyHierarchiesInCloudKit()
            self.putFakeDeviceStatusesInCloudKit()
            self.saveTLKMaterialToKeychain()
            self.silentFetchesAllowed = true
        }

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

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

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        self.verifyDatabaseMocks()
    }

    func testUserControllableViewStatusAPI() throws {
        self.startCKAccountStatusMock()

        let establishExpectation = self.expectation(description: "establish")
        self.fakeCuttlefishServer.establishListener = { request in
            XCTAssertTrue(request.peer.hasStableInfoAndSig, "updateTrust request should have a stableInfo info")
            let newStableInfo = request.peer.stableInfoAndSig.stableInfo()
            #if os(tvOS) || os(watchOS)
            XCTAssertEqual(newStableInfo.syncUserControllableViews, .FOLLOWING, "User-controllable views should be 'following'")
            #else
            XCTAssertEqual(newStableInfo.syncUserControllableViews, .DISABLED, "User-controllable views should be disabled")
            #endif
            establishExpectation.fulfill()
            return nil
        }

        self.assertResetAndBecomeTrustedInDefaultContext()
        let clique = self.cliqueFor(context: self.cuttlefishContext)

        self.wait(for: [establishExpectation], timeout: 10)
        self.fakeCuttlefishServer.establishListener = nil

        #if os(tvOS)
        XCTAssertEqual(self.injectedManager?.policy?.syncUserControllableViews, .FOLLOWING, "CKKS should be configured to follow")

        XCTAssertTrue(self.injectedManager?.policy?.syncUserControllableViewsAsBoolean() ?? false, "CKKS should be syncing user views")
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: true)

        // Watches won't ever disable the user views, so the API should fail
        XCTAssertThrowsError(try clique.setUserControllableViewsSyncStatus(false), "Should be an error setting user-visible sync status")
        XCTAssertThrowsError(try clique.setUserControllableViewsSyncStatus(true), "Should be an error setting user-visible sync status")

        XCTAssertEqual(self.injectedManager?.policy?.syncUserControllableViews, .FOLLOWING, "CKKS should be configured to follow peer's opinions of sync user views")
        XCTAssertTrue(self.injectedManager?.policy?.syncUserControllableViewsAsBoolean() ?? false, "CKKS should be syncing user views")
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: true)

        #else

        #if !os(watchOS)
        XCTAssertEqual(self.injectedManager?.policy?.syncUserControllableViews, .DISABLED, "CKKS should be configured to not sync user views")

        // And disabling it again doesn't write to the server
        self.assertModifyUserViewsWithNoPeerUpdate(clique: clique, intendedSyncStatus: false)
        XCTAssertEqual(self.injectedManager?.policy?.syncUserControllableViews, .DISABLED, "CKKS should be configured to not sync user views")
        #else
        // Watches, since some support this UI, and others don't, get special handling
        XCTAssertEqual(self.injectedManager?.policy?.syncUserControllableViews, .FOLLOWING, "CKKS should be configured to follow")
        XCTAssertTrue(self.injectedManager?.policy?.syncUserControllableViewsAsBoolean() ?? false, "CKKS should be syncing user views")
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: true)

        self.assertModifyUserViews(clique: clique, intendedSyncStatus: false)
        XCTAssertEqual(self.injectedManager?.policy?.syncUserControllableViews, .DISABLED, "CKKS should be configured to not sync user views")
        #endif // watchOS

        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: false)

        // Manatee items should upload
        self.expectCKModifyItemRecords(1, currentKeyPointerRecords: 1, zoneID: self.manateeZoneID)
        self.addGenericPassword("asdf", account: "account-delete-me", viewHint: "Manatee")
        self.verifyDatabaseMocks()

        // But Passwords items should not
        self.addGenericPassword("asdf",
                                account: "account-apple.com",
                                access: kSecAttrAccessibleWhenUnlocked as String,
                                viewHint: nil,
                                accessGroup: "com.apple.cfnetwork",
                                expecting: errSecSuccess,
                                message: "Should be able to add a CFNetwork keychain item")
        let passwordsView = self.injectedManager?.findView("Passwords")
        XCTAssertNotNil(passwordsView, "Should have a passwords view")
        passwordsView!.waitForOperations(of: CKKSOutgoingQueueOperation.self)

        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: false)

        // And how about enabling it? That should upload the password
        self.expectCKModifyItemRecords(1, currentKeyPointerRecords: 1, zoneID: self.passwordsZoneID)
        self.assertModifyUserViews(clique: clique, intendedSyncStatus: true)
        XCTAssertEqual(self.injectedManager?.policy?.syncUserControllableViews, .ENABLED, "CKKS should be configured to sync user views")
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: true)
        self.verifyDatabaseMocks()

        // And enabling it again doesn't write to the server
        self.assertModifyUserViewsWithNoPeerUpdate(clique: clique, intendedSyncStatus: true)
        XCTAssertEqual(self.injectedManager?.policy?.syncUserControllableViews, .ENABLED, "CKKS should be configured to sync user views")
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: true)

        // And we can turn it off again
        self.assertModifyUserViews(clique: clique, intendedSyncStatus: false)
        XCTAssertEqual(self.injectedManager?.policy?.syncUserControllableViews, .DISABLED, "CKKS should be configured to not sync user views")
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: false)

        #endif // !os(tvOS)
    }

    func testJoinFollowsUserViewStatus() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()
        let clique = self.cliqueFor(context: self.cuttlefishContext)

        #if os(tvOS)
        // For tvs, this is always on
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: true)

        // Add a peer
        let peer2Context = self.makeInitiatorContext(contextID: "peer2")
        self.assertJoinViaEscrowRecovery(joiningContext: peer2Context, sponsor: self.cuttlefishContext)

        // The new peer should automatically have turned on user view syncing
        self.assertFetchUserControllableViewsSyncStatus(clique: self.cliqueFor(context: peer2Context), status: true)

        // TVs can't ever modify this value
        let peer2Clique = self.cliqueFor(context: peer2Context)
        XCTAssertThrowsError(try peer2Clique.setUserControllableViewsSyncStatus(false), "Should be an error setting user-visible sync status")
        XCTAssertThrowsError(try peer2Clique.setUserControllableViewsSyncStatus(true), "Should be an error setting user-visible sync status")
        self.assertFetchUserControllableViewsSyncStatus(clique: peer2Clique, status: true)

        #elseif os(watchOS)
        // Watches are following by default. Turn the status off for the rest of the test
        XCTAssertEqual(self.injectedManager?.policy?.syncUserControllableViews, .FOLLOWING, "CKKS should be configured to follow")

        self.assertModifyUserViews(clique: clique, intendedSyncStatus: false)
        XCTAssertEqual(self.injectedManager?.policy?.syncUserControllableViews, .DISABLED, "CKKS should be configured to not sync user views")

        // Add a peer
        let peer2Context = self.makeInitiatorContext(contextID: "peer2")
        self.assertJoinViaEscrowRecovery(joiningContext: peer2Context, sponsor: self.cuttlefishContext)

        // A new watch peer will be FOLLOWING, so this API will return true
        self.assertFetchUserControllableViewsSyncStatus(clique: self.cliqueFor(context: peer2Context), status: true)

        #else  // iOS, macOS

        // By default, the sync status is "off"
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: false)

        // Add a peer
        let peer2Context = self.makeInitiatorContext(contextID: "peer2")
        self.assertJoinViaEscrowRecovery(joiningContext: peer2Context, sponsor: self.cuttlefishContext)

        // The new peer should automatically have disabled syncing
        self.assertFetchUserControllableViewsSyncStatus(clique: self.cliqueFor(context: peer2Context), status: false)

        self.assertModifyUserViews(clique: self.cliqueFor(context: peer2Context), intendedSyncStatus: true)
        self.assertFetchUserControllableViewsSyncStatus(clique: self.cliqueFor(context: peer2Context), status: true)
        #endif  // iOS, macOS

        // And a third peer joins!
        let peer3Context = self.makeInitiatorContext(contextID: "peer3", authKitAdapter: self.mockAuthKit3)
        self.assertJoinViaEscrowRecovery(joiningContext: peer3Context, sponsor: peer2Context)

        #if os(tvOS) || os(watchOS)
        // Watches and TVs won't ever turn this off (without extra UI), so a freshly joined peer will report this status as true
        self.assertFetchUserControllableViewsSyncStatus(clique: self.cliqueFor(context: peer3Context), status: true)
        #else
        // Peer3, by default, should have disabled the user controllable views (following peer1)
        self.assertFetchUserControllableViewsSyncStatus(clique: self.cliqueFor(context: peer3Context), status: false)
        #endif
    }

    func testUpgradePeerToHaveUserSyncableViewsOpinionViaAskingSOS() throws {
        self.mockSOSAdapter.safariViewEnabled = true

        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()
        let clique = self.cliqueFor(context: self.cuttlefishContext)

        #if os(tvOS) || os(watchOS)
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: true)
        #else
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: false)
        #endif

        // Now, fake that the peer no longer has this opinion:
        do {
            let container = try self.tphClient.getContainer(withContainer: self.cuttlefishContext.containerName,
                                                            context: self.cuttlefishContext.contextID)

            let (_, newSyncingPolicy, updateError) = container.updateSync(test: self,
                                                                          syncUserControllableViews: .UNKNOWN)
            XCTAssertNil(updateError, "Should be no error performing update")
            XCTAssertNotNil(newSyncingPolicy, "Should have a syncing policy")
            XCTAssertEqual(newSyncingPolicy?.syncUserControllableViews, .UNKNOWN, "Should now have 'unknown' user controlled views setting")

            self.injectedManager?.setCurrentSyncingPolicy(newSyncingPolicy)

            try self.cuttlefishContext.accountMetadataStore.persistAccountChanges { metadata in
                metadata.setTPSyncingPolicy(newSyncingPolicy)
                return metadata
            }
        }
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: false)

        // Upon daemon restart, Octagon should notice and update its opinion
        let updateTrustExpectation = self.expectation(description: "updateTrust")
        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()
            #if os(tvOS) || os(watchOS)
            XCTAssertEqual(newStableInfo.syncUserControllableViews, .FOLLOWING, "CKKS should be configured to follow peer's user views")
            #else
            XCTAssertEqual(newStableInfo.syncUserControllableViews, .ENABLED, "CKKS should be configured to sync user views (following SOS's lead)")
            #endif
            updateTrustExpectation.fulfill()
            return nil
        }

        self.cuttlefishContext = self.simulateRestart(context: self.cuttlefishContext)
        self.wait(for: [updateTrustExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(10 * NSEC_PER_SEC), "State machine should pause")
    }

    func testUpgradePeerToHaveUserSyncableViewsOpinionViaPeersOpinion() throws {
        self.mockSOSAdapter.sosEnabled = false

        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()
        let clique = self.cliqueFor(context: self.cuttlefishContext)

        #if os(tvOS) || os(watchOS)
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: true)
        #else
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: false)
        #endif

        // Now, fake that the peer no longer has this opinion:
        do {
            let container = try self.tphClient.getContainer(withContainer: self.cuttlefishContext.containerName,
                                                            context: self.cuttlefishContext.contextID)

            let (_, newSyncingPolicy, updateError) = container.updateSync(test: self,
                                                                          syncUserControllableViews: .UNKNOWN)
            XCTAssertNil(updateError, "Should be no error performing update")
            XCTAssertNotNil(newSyncingPolicy, "Should have a syncing policy")
            XCTAssertEqual(newSyncingPolicy?.syncUserControllableViews, .UNKNOWN, "Should now have 'unknown' user controlled views setting")

            self.injectedManager?.setCurrentSyncingPolicy(newSyncingPolicy)

            try self.cuttlefishContext.accountMetadataStore.persistAccountChanges { metadata in
                metadata.setTPSyncingPolicy(newSyncingPolicy)
                return metadata
            }
        }
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: false)

        // Upon daemon restart, Octagon should notice and update its opinion
        let updateTrustExpectation = self.expectation(description: "updateTrust")
        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()
            #if os(tvOS) || os(watchOS)
            XCTAssertEqual(newStableInfo.syncUserControllableViews, .FOLLOWING, "CKKS should be configured to follow peer's user views")
            #else
            XCTAssertEqual(newStableInfo.syncUserControllableViews, .DISABLED, "CKKS should be configured to disable user views, since no peers have it actively enabled")
            #endif
            updateTrustExpectation.fulfill()
            return nil
        }

        self.cuttlefishContext = self.simulateRestart(context: self.cuttlefishContext)
        self.wait(for: [updateTrustExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(10 * NSEC_PER_SEC), "State machine should pause")
    }

    func testUpgradePeerToHaveUserSyncableViewsOpinionWhileLocked() throws {
        self.mockSOSAdapter.safariViewEnabled = true

        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()
        let clique = self.cliqueFor(context: self.cuttlefishContext)

        #if os(tvOS) || os(watchOS)
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: true)
        #else
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: false)
        #endif

        // Now, fake that the peer no longer has this opinion:
        do {
            let container = try self.tphClient.getContainer(withContainer: self.cuttlefishContext.containerName,
                                                            context: self.cuttlefishContext.contextID)

            let (_, newSyncingPolicy, updateError) = container.updateSync(test: self,
                                                                          syncUserControllableViews: .UNKNOWN)
            XCTAssertNil(updateError, "Should be no error performing update")
            XCTAssertNotNil(newSyncingPolicy, "Should have a syncing policy")
            XCTAssertEqual(newSyncingPolicy?.syncUserControllableViews, .UNKNOWN, "Should now have 'unknown' user controlled views setting")

            self.injectedManager?.setCurrentSyncingPolicy(newSyncingPolicy)

            try self.cuttlefishContext.accountMetadataStore.persistAccountChanges { metadata in
                metadata.setTPSyncingPolicy(newSyncingPolicy)
                return metadata
            }
        }
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: false)

        // Upon daemon restart, Octagon should notice and update its opinion

        self.aksLockState = true
        self.lockStateTracker.recheck()

        self.fakeCuttlefishServer.updateListener = { _ in
            XCTFail("No update should happen while the device is locked")
            return nil
        }

        // We should get to 'wait for unlock'
        self.cuttlefishContext = self.simulateRestart(context: self.cuttlefishContext)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForUnlock, within: 100 * NSEC_PER_SEC)

        let updateTrustExpectation = self.expectation(description: "updateTrust")
        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()
            #if os(tvOS) || os(watchOS)
            XCTAssertEqual(newStableInfo.syncUserControllableViews, .FOLLOWING, "CKKS should be configured to follow peer's user views")
            #else
            XCTAssertEqual(newStableInfo.syncUserControllableViews, .ENABLED, "CKKS should be configured to sync user views (following SOS's lead)")
            #endif
            updateTrustExpectation.fulfill()
            return nil
        }

        // and once we unlock, the update should go through
        self.aksLockState = false
        self.lockStateTracker.recheck()

        self.wait(for: [updateTrustExpectation], timeout: 10)
    }

    func testAssistCKKSTLKUploadWhenMissingFromOctagon() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        // Another peer resets Octagon, but we miss the push
        let reset = self.makeInitiatorContext(contextID: "reset")
        self.assertResetAndBecomeTrusted(context: reset)

        // This should cause the original peer to notice that it's no longer trusted
        self.silentZoneDeletesAllowed = true
        self.resetAllCKKSViews()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTrust, within: 100 * NSEC_PER_SEC)
    }
}

#endif // OCTAGON
