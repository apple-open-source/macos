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

        if self.mockDeviceInfo.mockModelID.contains("AppleTV") || self.mockDeviceInfo.mockModelID.contains("AudioAccessory") {
            self.intendedCKKSZones = Set([
                CKRecordZone.ID(zoneName: "LimitedPeersAllowed"),
                CKRecordZone.ID(zoneName: "ProtectedCloudStorage"),
            ])
        } else {
            self.intendedCKKSZones = Set([
                CKRecordZone.ID(zoneName: "LimitedPeersAllowed"),
                CKRecordZone.ID(zoneName: "Contacts"),
                CKRecordZone.ID(zoneName: "Groups"),
                CKRecordZone.ID(zoneName: "Manatee"),
                CKRecordZone.ID(zoneName: "Mail"),
                CKRecordZone.ID(zoneName: "MFi"),
                CKRecordZone.ID(zoneName: "Passwords"),
                CKRecordZone.ID(zoneName: "Photos"),
                CKRecordZone.ID(zoneName: "SecureObjectSync"),
                CKRecordZone.ID(zoneName: "Backstop"),
            ])
        }

        super.setUp()

        // Allow ourselves to add safari items
        self.previousKeychainAccessGroups = (SecAccessGroupsGetCurrent()?.takeUnretainedValue()) as? [String]
        SecAccessGroupsSetCurrent((self.previousKeychainAccessGroups + [
            "com.apple.cfnetwork",
            "com.apple.sbd",
            "com.apple.cfnetwork-recently-deleted",
            "com.apple.password-manager-recently-deleted",
            "com.apple.webkit.webauthn-recently-deleted",
            "com.apple.password-manager.personal",
            "com.apple.password-manager.personal-recently-deleted",
            "com.apple.password-manager.generated-passwords",
            "com.apple.password-manager.generated-passwords-recently-deleted",
        ]) as CFArray)
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

    func testHandleMFiItemAdd() throws {
#if os(tvOS)
        throw XCTSkip("aTV does not participate in MFi view")
#else
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        self.expectCKModifyItemRecords(1, currentKeyPointerRecords: 1, zoneID: CKRecordZone.ID(zoneName: "MFi"))
        self.addGenericPassword("asdf",
                                account: "MFi-test",
                                viewHint: kSecAttrViewHintMFi as String)

        self.verifyDatabaseMocks()
#endif // tvos test skip
    }

    func testHandleMailItemAdd() throws {
#if os(tvOS)
        throw XCTSkip("aTV does not participate in Mail view")
#else
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        self.expectCKModifyItemRecords(1, currentKeyPointerRecords: 1, zoneID: CKRecordZone.ID(zoneName: "Mail"))
        self.addGenericPassword("asdf",
                                account: "Mail-test",
                                viewHint: kSecAttrViewHintMail as String)

        self.verifyDatabaseMocks()
#endif // tvos test skip
    }

    func testHandleSafariGeneratedIems() throws {
#if os(tvOS)
        throw XCTSkip("aTV cannot set user-controllable views")
#else
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        // `Passwords` is a user-controllable view, so we must enable syncing
        // for those views if we want our items to upload.
        let clique = self.cliqueFor(context: self.cuttlefishContext)
        self.assertModifyUserViews(clique: clique, intendedSyncStatus: true)

        // We expect four records to be uploaded to the view, one for each
        // recently deleted access group.
        self.expectCKModifyItemRecords(3, currentKeyPointerRecords: 1, zoneID: self.passwordsZoneID)

        // Ensure that all uploads happen in a batch
        self.defaultCKKS.holdOutgoingQueueOperation = Operation()

        self.addGenericPassword("password",
                                account: "jane_eyre",
                                access: kSecAttrAccessibleWhenUnlocked as String,
                                viewHint: nil,
                                accessGroup: "com.apple.password-manager.generated-passwords",
                                expecting: errSecSuccess,
                                message: "Add item to recently deleted credentials access group")

        self.addRandomPrivateKey(withAccessGroup: "com.apple.password-manager.generated-passwords-recently-deleted",
                                 message: "Add key to recently deleted WebAuthn keys access group")

        self.addGenericPassword("personal-sidecar",
                                account: "jane_eyre",
                                access: kSecAttrAccessibleWhenUnlocked as String,
                                viewHint: nil,
                                accessGroup: "com.apple.password-manager.generated-passwords-recently-deleted",
                                expecting: errSecSuccess,
                                message: "Add item to recently deleted personal sidecars access group")

        if let op = self.defaultCKKS.holdOutgoingQueueOperation {
            self.operationQueue.add(op)
        }

        self.verifyDatabaseMocks()
#endif
    }

    func testHandleSafariRecentlyDeletedItems() throws {
#if os(tvOS)
        throw XCTSkip("aTV cannot set user-controllable views")
#else
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        // `Passwords` is a user-controllable view, so we must enable syncing
        // for those views if we want our items to upload.
        let clique = self.cliqueFor(context: self.cuttlefishContext)
        self.assertModifyUserViews(clique: clique, intendedSyncStatus: true)

        // We expect four records to be uploaded to the view, one for each
        // recently deleted access group.
        self.expectCKModifyItemRecords(4, currentKeyPointerRecords: 1, zoneID: self.passwordsZoneID)

        self.addGenericPassword("password",
                                account: "jane_eyre",
                                access: kSecAttrAccessibleWhenUnlocked as String,
                                viewHint: nil,
                                accessGroup: "com.apple.cfnetwork-recently-deleted",
                                expecting: errSecSuccess,
                                message: "Add item to recently deleted credentials access group")

        self.addGenericPassword("sidecar",
                                account: "jane_eyre",
                                access: kSecAttrAccessibleWhenUnlocked as String,
                                viewHint: nil,
                                accessGroup: "com.apple.password-manager-recently-deleted",
                                expecting: errSecSuccess,
                                message: "Add item to recently deleted credential sidecars access group")

        self.addRandomPrivateKey(withAccessGroup: "com.apple.webkit.webauthn-recently-deleted",
                                 message: "Add key to recently deleted WebAuthn keys access group")

        self.addGenericPassword("personal-sidecar",
                                account: "jane_eyre",
                                access: kSecAttrAccessibleWhenUnlocked as String,
                                viewHint: nil,
                                accessGroup: "com.apple.password-manager.personal-recently-deleted",
                                expecting: errSecSuccess,
                                message: "Add item to recently deleted personal sidecars access group")

        self.verifyDatabaseMocks()
#endif // tvos test skip
    }

    func testHandleSafariPersonalSidecarItem() throws {
#if os(tvOS)
        throw XCTSkip("aTV cannot set user-controllable views")
#else
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        self.assertModifyUserViews(clique: clique, intendedSyncStatus: true)

        self.expectCKModifyItemRecords(1, currentKeyPointerRecords: 1, zoneID: self.passwordsZoneID)

        self.addGenericPassword("personal-sidecar",
                                account: "jane_eyre",
                                access: kSecAttrAccessibleWhenUnlocked as String,
                                viewHint: nil,
                                accessGroup: "com.apple.password-manager.personal",
                                expecting: errSecSuccess,
                                message: "Add item to personal sidecars access group")

        self.verifyDatabaseMocks()
#endif // tvos test skip
    }

    func testHandleContactsItemAdd() throws {
#if os(tvOS)
        throw XCTSkip("aTV does not participate in Mail view")
#else
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        self.expectCKModifyItemRecords(1, currentKeyPointerRecords: 1, zoneID: CKRecordZone.ID(zoneName: "Contacts"))
        self.addGenericPassword("asdf",
                                account: "Contacts-test",
                                viewHint: kSecAttrViewHintContacts as String)

        self.verifyDatabaseMocks()
#endif // tvos test skip
    }

    func testHandlePhotosItemAdd() throws {
#if os(tvOS)
        throw XCTSkip("aTV does not participate in Mail view")
#else
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        self.expectCKModifyItemRecords(1, currentKeyPointerRecords: 1, zoneID: CKRecordZone.ID(zoneName: "Photos"))
        self.addGenericPassword("asdf",
                                account: "Photos-test",
                                viewHint: kSecAttrViewHintPhotos as String)

        self.verifyDatabaseMocks()
#endif // tvos test skip
    }

    func testHandleGroupsItemAdd() throws {
#if os(tvOS)
        throw XCTSkip("aTV does not participate in Groups view")
#else
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        self.expectCKModifyItemRecords(1, currentKeyPointerRecords: 1, zoneID: CKRecordZone.ID(zoneName: "Groups"))
        self.addGenericPassword("asdf",
                                account: "Groups-test",
                                viewHint: kSecAttrViewHintGroups as String)

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

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        self.verifyDatabaseMocks()
    }

    func testHandleCKKSKeyHierarchyConflictOnEstablishWithKeysArrivingLater() throws {
        self.startCKAccountStatusMock()

        // Right after CKKS fetches for the first time, insert a new key hierarchy into CloudKit
        // But, whoever makes these key hierarchies doesn't participate in Octagon, so the TLK arrives later via sos
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

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLK, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.saveTLKMaterialToKeychain()
        self.verifyDatabaseMocks()
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        self.verifyDatabaseMocks()
    }

    func testUserControllableViewManagedTrue() throws {
        #if os(tvOS) || os(watchOS)
        return
        #endif

        self.startCKAccountStatusMock()

        self.assertResetAndBecomeTrustedInDefaultContext()
        let clique = self.cliqueFor(context: self.cuttlefishContext)
        self.assertModifyUserViews(clique: clique, intendedSyncStatus: true)

        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: true)

        self.mcAdapterPlaceholder.keychainAllowed = true
        let peer2Context = self.makeInitiatorContext(contextID: "peer2")
        let peer2Clique = self.cliqueFor(context: peer2Context)
        self.assertJoinViaEscrowRecovery(joiningContext: peer2Context, sponsor: self.cuttlefishContext)

        self.assertFetchUserControllableViewsSyncStatus(clique: peer2Clique, status: true)
    }

    func testUserControllableViewManagedFalse() throws {
        #if os(tvOS) || os(watchOS)
        return
        #endif

        self.startCKAccountStatusMock()

        self.assertResetAndBecomeTrustedInDefaultContext()
        let clique = self.cliqueFor(context: self.cuttlefishContext)
        self.assertModifyUserViews(clique: clique, intendedSyncStatus: true)

        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: true)

        self.mcAdapterPlaceholder.keychainAllowed = false
        let peer2Context = self.makeInitiatorContext(contextID: "peer2")
        let peer2Clique = self.cliqueFor(context: peer2Context)
        self.assertJoinViaEscrowRecovery(joiningContext: peer2Context, sponsor: self.cuttlefishContext)

        self.assertFetchUserControllableViewsSyncStatus(clique: peer2Clique, status: false)
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
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.syncUserControllableViews, .FOLLOWING, "CKKS should be configured to follow")

        XCTAssertTrue(self.defaultCKKS.syncingPolicy?.syncUserControllableViewsAsBoolean() ?? false, "CKKS should be syncing user views")
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: true)

        // Since the API should fail, we expect not to have the notification sent
        let ucvStatusChangeNotificationExpectation = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: OTUserControllableViewStatusChanged))
        ucvStatusChangeNotificationExpectation.isInverted = true

        // tvOS won't ever disable the user views, so the API should fail
        XCTAssertThrowsError(try clique.setUserControllableViewsSyncStatus(false), "Should be an error setting user-visible sync status")
        XCTAssertThrowsError(try clique.setUserControllableViewsSyncStatus(true), "Should be an error setting user-visible sync status")

        self.wait(for: [ucvStatusChangeNotificationExpectation], timeout: 1)

        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.syncUserControllableViews, .FOLLOWING, "CKKS should be configured to follow peer's opinions of sync user views")
        XCTAssertTrue(self.defaultCKKS.syncingPolicy?.syncUserControllableViewsAsBoolean() ?? false, "CKKS should be syncing user views")
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: true)

        #else

        #if !os(watchOS)
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.syncUserControllableViews, .DISABLED, "CKKS should be configured to not sync user views")

        // And disabling it again doesn't write to the server
        self.assertModifyUserViewsWithNoPeerUpdate(clique: clique, intendedSyncStatus: false)
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.syncUserControllableViews, .DISABLED, "CKKS should be configured to not sync user views")
        #else
        // Watches, since some support this UI, and others don't, get special handling
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.syncUserControllableViews, .FOLLOWING, "CKKS should be configured to follow")
        XCTAssertTrue(self.defaultCKKS.syncingPolicy?.syncUserControllableViewsAsBoolean() ?? false, "CKKS should be syncing user views")
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: true)

        self.assertModifyUserViews(clique: clique, intendedSyncStatus: false)
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.syncUserControllableViews, .DISABLED, "CKKS should be configured to not sync user views")
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

        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)

        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: false)

        // And how about enabling it? That should upload the password
        self.expectCKModifyItemRecords(1, currentKeyPointerRecords: 1, zoneID: self.passwordsZoneID)
        self.assertModifyUserViews(clique: clique, intendedSyncStatus: true)
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.syncUserControllableViews, .ENABLED, "CKKS should be configured to sync user views")
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: true)
        self.verifyDatabaseMocks()

        // And enabling it again doesn't write to the server
        self.assertModifyUserViewsWithNoPeerUpdate(clique: clique, intendedSyncStatus: true)
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.syncUserControllableViews, .ENABLED, "CKKS should be configured to sync user views")
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: true)

        // And we can turn it off again
        self.assertModifyUserViews(clique: clique, intendedSyncStatus: false)
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.syncUserControllableViews, .DISABLED, "CKKS should be configured to not sync user views")
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
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.syncUserControllableViews, .FOLLOWING, "CKKS should be configured to follow")

        self.assertModifyUserViews(clique: clique, intendedSyncStatus: false)
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.syncUserControllableViews, .DISABLED, "CKKS should be configured to not sync user views")

        // Add a peer
        let peer2Context = self.makeInitiatorContext(contextID: "peer2")
        self.assertJoinViaEscrowRecovery(joiningContext: peer2Context, sponsor: self.cuttlefishContext)

        // A new watch peer will be FOLLOWING, so this API will return true
        self.assertFetchUserControllableViewsSyncStatus(clique: self.cliqueFor(context: peer2Context), status: true)

        #else  // iOS, macOS

        // By default, the sync status is "off"
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: false)

        // Add a peer
        print("BEGINNING ESCROW JOIN")
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
        try self.skipOnRecoveryKeyNotSupported()

        self.mockSOSAdapter!.safariViewEnabled = true

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
            let container = try self.tphClient.getContainer(with: try XCTUnwrap(self.cuttlefishContext.activeAccount))

            let (_, newSyncingPolicy, updateError) = container.updateSync(test: self,
                                                                          syncUserControllableViews: .UNKNOWN)
            XCTAssertNil(updateError, "Should be no error performing update")
            XCTAssertNotNil(newSyncingPolicy, "Should have a syncing policy")
            XCTAssertEqual(newSyncingPolicy?.syncUserControllableViews, .UNKNOWN, "Should now have 'unknown' user controlled views setting")

            self.defaultCKKS.setCurrentSyncingPolicy(newSyncingPolicy)

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
        self.mockSOSAdapter!.setSOSEnabled(false)

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
            let container = try self.tphClient.getContainer(with: try XCTUnwrap(self.cuttlefishContext.activeAccount))

            let (_, newSyncingPolicy, updateError) = container.updateSync(test: self,
                                                                          syncUserControllableViews: .UNKNOWN)
            XCTAssertNil(updateError, "Should be no error performing update")
            XCTAssertNotNil(newSyncingPolicy, "Should have a syncing policy")
            XCTAssertEqual(newSyncingPolicy?.syncUserControllableViews, .UNKNOWN, "Should now have 'unknown' user controlled views setting")

            self.defaultCKKS.setCurrentSyncingPolicy(newSyncingPolicy)

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
        try self.skipOnRecoveryKeyNotSupported()

        self.mockSOSAdapter!.safariViewEnabled = true

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
            let container = try self.tphClient.getContainer(with: try XCTUnwrap(self.cuttlefishContext.activeAccount))

            let (_, newSyncingPolicy, updateError) = container.updateSync(test: self,
                                                                          syncUserControllableViews: .UNKNOWN)
            XCTAssertNil(updateError, "Should be no error performing update")
            XCTAssertNotNil(newSyncingPolicy, "Should have a syncing policy")
            XCTAssertEqual(newSyncingPolicy?.syncUserControllableViews, .UNKNOWN, "Should now have 'unknown' user controlled views setting")

            self.defaultCKKS.setCurrentSyncingPolicy(newSyncingPolicy)

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

    func testHandleFailureUpgradingPeerToHaveUserSyncableViewsOpinion() throws {
        self.mockSOSAdapter!.safariViewEnabled = true

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
            let container = try self.tphClient.getContainer(with: try XCTUnwrap(self.cuttlefishContext.activeAccount))

            let (_, newSyncingPolicy, updateError) = container.updateSync(test: self,
                                                                          syncUserControllableViews: .UNKNOWN)
            XCTAssertNil(updateError, "Should be no error performing update")
            XCTAssertNotNil(newSyncingPolicy, "Should have a syncing policy")
            XCTAssertEqual(newSyncingPolicy?.syncUserControllableViews, .UNKNOWN, "Should now have 'unknown' user controlled views setting")

            self.defaultCKKS.setCurrentSyncingPolicy(newSyncingPolicy)

            try self.cuttlefishContext.accountMetadataStore.persistAccountChanges { metadata in
                metadata.setTPSyncingPolicy(newSyncingPolicy)
                return metadata
            }
        }
        self.assertFetchUserControllableViewsSyncStatus(clique: clique, status: false)

        // Upon daemon restart, Octagon should notice and attempt to update its opinion
        // But, that fails the first time
        let fastUpdateTrustExpectation = self.expectation(description: "fast updateTrust")
        let retriedUpdateTrustExpectation = self.expectation(description: "retried updateTrust")
        let restartAt = Date()

        self.fakeCuttlefishServer.updateListener = { _ in
            let now = Date()

            // Entering this block twice will fail the test.
            // The rate limiter in OTCuttlefishContext is set to retry after 10seconds,
            // so 7s was chosen to allow for some slop in timing/slow restarts.
            // If update() is called twice within 7s, the test will fail.
            if restartAt.distance(to: now) < 7 {
                fastUpdateTrustExpectation.fulfill()
                return FakeCuttlefishServer.makeCloudKitCuttlefishError(code: .testGeneratedFailure)
            } else {
                retriedUpdateTrustExpectation.fulfill()
                return nil
            }
        }

        self.cuttlefishContext = self.simulateRestart(context: self.cuttlefishContext)
        self.wait(for: [fastUpdateTrustExpectation], timeout: 20)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(10 * NSEC_PER_SEC), "State machine should pause")

        self.wait(for: [retriedUpdateTrustExpectation], timeout: 20)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(10 * NSEC_PER_SEC), "State machine should pause")
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

        // Octagon won't help to make new TLKs, so we will delete the zones and then bail.
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateWaitForTrust, within: 10 * NSEC_PER_SEC)
    }

    func testUserControllableViewsSyncingStatusNoAccount() throws {
        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitingForCloudKitAccount, within: 10 * NSEC_PER_SEC)

        let fetchExpectation = self.expectation(description: "fetch user controllable views syncing status returns")
        self.cuttlefishContext.rpcFetchUserControllableViewsSyncingStatus { isSyncing, error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertFalse(isSyncing, "should not be syncing with no account")
            fetchExpectation.fulfill()
        }
        self.wait(for: [fetchExpectation], timeout: 10)

        let setExpectation = self.expectation(description: "set user controllable views syncing status quickly")
        self.cuttlefishContext.rpcSetUserControllableViewsSyncingStatus(true) { isSyncing, error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertFalse(isSyncing, "should not be syncing with no account")
            setExpectation.fulfill()
        }
        self.wait(for: [setExpectation], timeout: 10)

        self.startCKAccountStatusMock()
    }

    func testFetchUserControllableViewsSyncingStatusOctagonStateUntrusted() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let fetchExpectation = self.expectation(description: "fetch user controllable views syncing status returns")
        self.cuttlefishContext.rpcFetchUserControllableViewsSyncingStatus { isSyncing, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertFalse(isSyncing, "should not be syncing untrusted")
            fetchExpectation.fulfill()
        }
        self.wait(for: [fetchExpectation], timeout: 10)
    }

    func testFetchUserControllableViewsSyncingStatusError() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let op = OctagonStateTransitionOperation(name: "force-error-state", entering: OctagonStateError)

        let sourceState: Set = [OctagonStateUntrusted]
        let reply: (Error?) -> Void = {_ in
        }

        self.cuttlefishContext.stateMachine.doSimpleStateMachineRPC("force-error-state", op: op, sourceStates: sourceState, reply: reply)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateError, within: 10 * NSEC_PER_SEC)

        let fetchExpectation = self.expectation(description: "fetch user controllable views syncing status returns")
        self.cuttlefishContext.rpcFetchUserControllableViewsSyncingStatus { isSyncing, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertFalse(isSyncing, "should not be syncing in the error state")
            fetchExpectation.fulfill()
        }
        self.wait(for: [fetchExpectation], timeout: 10)
    }

    func testSignInWithDelayedHSA2StatusAndAttemptFetchUserControllableViewsSyncingStatus() throws {
        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter!.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // Device is signed out
        self.mockAuthKit.removePrimaryAccount()

        self.startCKAccountStatusMock()

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        // Sign in occurs, but HSA2 status isn't here yet
        let newAltDSID = UUID().uuidString

        let account = CloudKitAccount(altDSID: newAltDSID, persona: nil, hsa2: false, demo: false, accountStatus: .available)
        self.mockAuthKit.add(account)

        XCTAssertNoThrow(try self.cuttlefishContext.accountAvailable(newAltDSID), "Sign-in shouldn't error")

        // Octagon should go into 'waitforhsa2'
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDPCapableSecurityLevel, within: 10 * NSEC_PER_SEC)

        let fetchExpectation = self.expectation(description: "fetch user controllable views syncing status returns")
        self.cuttlefishContext.rpcFetchUserControllableViewsSyncingStatus { isSyncing, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertFalse(isSyncing, "should not be syncing with non hsa2 account")
            fetchExpectation.fulfill()
        }
        self.wait(for: [fetchExpectation], timeout: 2)
    }

    func testWaitForPriorityViewAPIAfterEstablish() throws {
        self.startCKAccountStatusMock()

        self.assertResetAndBecomeTrustedInDefaultContext()

        let waitExpectation = self.expectation(description: "wait should end")

        self.cuttlefishContext.rpcWaitForPriorityViewKeychainDataRecovery { error in
            XCTAssertNil(error, "Should have no error waiting for CKKS Priority zones to recover all data")
            waitExpectation.fulfill()
        }

        self.wait(for: [waitExpectation], timeout: 10)

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)
        XCTAssertNoThrow(try cliqueBridge.waitForPriorityViewKeychainDataRecovery(), "Should be able to use OctagonTrust API")
    }

    func testWaitForPriorityViewAPIDuringJoinWithSlowCKFetch() throws {
        // CKKS will be very, very slow.
        self.holdCloudKitFetches()

        let allSharesContext = self.makeInitiatorContext(contextID: "allShares", authKitAdapter: self.mockAuthKit3)

        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrusted(context: allSharesContext)

        self.putFakeKeyHierarchiesInCloudKit()
        self.putFakeDeviceStatusesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: allSharesContext)

        self.assertJoinViaEscrowRecovery(joiningContext: self.cuttlefishContext, sponsor: allSharesContext)

        let returnedTooEarlyExpectation = self.expectation(description: "wait returned too early")
        returnedTooEarlyExpectation.isInverted = true
        let returnedCorrectlyExpectation = self.expectation(description: "wait returned correctly")

        self.cuttlefishContext.rpcWaitForPriorityViewKeychainDataRecovery { error in
            XCTAssertNil(error, "Should have no error waiting for CKKS Priority zones to recover all data")

            returnedTooEarlyExpectation.fulfill()
            returnedCorrectlyExpectation.fulfill()
        }

        // First, ensure that the wait occurs while the fetch hasn't completed
        self.wait(for: [returnedTooEarlyExpectation], timeout: 4)

        // Let the fetch complete: the wait should then end
        self.releaseCloudKitFetchHold()
        self.wait(for: [returnedCorrectlyExpectation], timeout: 10)

        // And CKKS should enter ready!
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
        self.assertTLKSharesInCloudKit(receiver: self.cuttlefishContext, sender: self.cuttlefishContext)
    }
}

#endif // OCTAGON
