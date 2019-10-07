#if OCTAGON

class OctagonResetTests: OctagonTestsBase {
    func testAccountAvailableAndHandleExternalCall() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        _ = try self.cuttlefishContext.accountAvailable("13453464")

        self.cuttlefishContext.rpcResetAndEstablish() { resetError in
            XCTAssertNil(resetError, "should be no error resetting and establishing")
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func testExernalCallAndAccountAvailable() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.cuttlefishContext.rpcResetAndEstablish() { resetError in
            XCTAssertNil(resetError, "should be no error resetting and establishing")
        }

        _ = try self.cuttlefishContext.accountAvailable("13453464")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func testCallingAccountAvailableDuringResetAndEstablish() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.cuttlefishContext.rpcResetAndEstablish() { resetError in
            XCTAssertNil(resetError, "should be no error resetting and establishing")
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateResetAndEstablish, within: 1 * NSEC_PER_SEC)

        _ = try self.cuttlefishContext.accountAvailable("13453464")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func testResetAndEstablishWithEscrow() throws {
        let contextName = OTDefaultContext
        let containerName = OTCKContainerName

        self.startCKAccountStatusMock()

        // Before resetAndEstablish, there shouldn't be any stored account state
               XCTAssertThrowsError(try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName), "Before doing anything, loading a non-existent account state should fail")

        let resetAndEstablishExpectation = self.expectation(description: "resetAndEstablish callback occurs")
        let escrowRequestNotification = expectation(forNotification: OTMockEscrowRequestNotification,
                                                    object: nil,
                                                    handler: nil)
        self.manager.resetAndEstablish(containerName,
                                       context: contextName,
                                       altDSID: "new altDSID") { resetError in
                                        XCTAssertNil(resetError, "Should be no error calling resetAndEstablish")
                                        resetAndEstablishExpectation.fulfill()
        }

        self.wait(for: [resetAndEstablishExpectation], timeout: 10)
        self.wait(for: [escrowRequestNotification], timeout: 5)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        let selfPeerID = try self.cuttlefishContext.accountMetadataStore.loadOrCreateAccountMetadata().peerID

        // After resetAndEstablish, you should be able to see the persisted account state
        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName)
            XCTAssertEqual(selfPeerID, accountState.peerID, "Saved account state should have the same peer ID that prepare returned")
        } catch {
            XCTFail("error loading account state: \(error)")
        }
    }

    func testResetAndEstablishStopsCKKS() throws {
        let contextName = OTDefaultContext
        let containerName = OTCKContainerName

        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        do {
            let clique = try OTClique.newFriends(withContextData: self.otcliqueContext)
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

        // CKKS should pass through "waitfortrust" during a reset
        let waitfortrusts = self.ckksViews.compactMap { view in
            (view as! CKKSKeychainView).keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTrust] as? CKKSCondition
        }
        XCTAssert(waitfortrusts.count > 0, "Should have at least one waitfortrust condition")

        let resetAndEstablishExpectation = self.expectation(description: "resetAndEstablish callback occurs")
        let escrowRequestNotification = expectation(forNotification: OTMockEscrowRequestNotification,
                                                    object: nil,
                                                    handler: nil)
        self.manager.resetAndEstablish(containerName,
                                       context: contextName,
                                       altDSID: "new altDSID") { resetError in
                                        XCTAssertNil(resetError, "Should be no error calling resetAndEstablish")
                                        resetAndEstablishExpectation.fulfill()
        }

        self.wait(for: [resetAndEstablishExpectation], timeout: 10)
        self.wait(for: [escrowRequestNotification], timeout: 5)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        // CKKS should have all gone into waitfortrust during that time
        for condition in waitfortrusts {
            XCTAssertEqual(0, condition.wait(10*NSEC_PER_MSEC), "CKKS should have entered waitfortrust")
        }
    }

    func testOctagonResetAlsoResetsCKKSViewsMissingTLKs() {
        self.putFakeKeyHierarchy(inCloudKit: self.manateeZoneID)

        let zoneKeys = self.keys![self.manateeZoneID] as? ZoneKeys
        XCTAssertNotNil(zoneKeys, "Should have some zone keys")
        XCTAssertNotNil(zoneKeys?.tlk, "Should have a tlk in the original key set")

        self.startCKAccountStatusMock()
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTrust, within: 10 * NSEC_PER_SEC)

        self.silentZoneDeletesAllowed = true

        do {
            _ = try OTClique.newFriends(withContextData: self.otcliqueContext)
        } catch {
            XCTFail("failed to make new friends: \(error)")
        }

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        let laterZoneKeys = self.keys![self.manateeZoneID] as? ZoneKeys
        XCTAssertNotNil(laterZoneKeys, "Should have some zone keys")
        XCTAssertNotNil(laterZoneKeys?.tlk, "Should have a tlk in the newly created keyset")
        XCTAssertNotEqual(zoneKeys?.tlk?.uuid, laterZoneKeys?.tlk?.uuid, "CKKS zone should now have different keys")
    }

    func testOctagonResetIgnoresOldRemoteDevicesWithKeysAndResetsCKKS() {
        // CKKS has no keys, and there's another device claiming to have them already, but it's old
        self.putFakeKeyHierarchy(inCloudKit: self.manateeZoneID)
        self.putFakeDeviceStatus(inCloudKit: self.manateeZoneID)

        (self.zones![self.manateeZoneID]! as! FakeCKZone).currentDatabase.allValues.forEach { record in
            let r = record as! CKRecord
            if(r.recordType == SecCKRecordDeviceStateType) {
                r.creationDate = NSDate.distantPast
                r.modificationDate = NSDate.distantPast
            }
        }

        let zoneKeys = self.keys![self.manateeZoneID] as? ZoneKeys
        XCTAssertNotNil(zoneKeys, "Should have some zone keys")
        XCTAssertNotNil(zoneKeys?.tlk, "Should have a tlk in the original key set")

        self.startCKAccountStatusMock()
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.silentZoneDeletesAllowed = true

        do {
            _ = try OTClique.newFriends(withContextData: self.otcliqueContext)
        } catch {
            XCTFail("failed to make new friends: \(error)")
        }

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        let laterZoneKeys = self.keys![self.manateeZoneID] as? ZoneKeys
        XCTAssertNotNil(laterZoneKeys, "Should have some zone keys")
        XCTAssertNotNil(laterZoneKeys?.tlk, "Should have a tlk in the newly created keyset")
        XCTAssertNotEqual(zoneKeys?.tlk?.uuid, laterZoneKeys?.tlk?.uuid, "CKKS zone should now have different keys")
    }

    func testOctagonResetWithRemoteDevicesWithKeysDoesNotResetCKKS() {
        // CKKS has no keys, and there's another device claiming to have them already, so CKKS won't immediately reset it
        self.putFakeKeyHierarchy(inCloudKit: self.manateeZoneID)
        self.putFakeDeviceStatus(inCloudKit: self.manateeZoneID)

        let zoneKeys = self.keys![self.manateeZoneID] as? ZoneKeys
        XCTAssertNotNil(zoneKeys, "Should have some zone keys")
        XCTAssertNotNil(zoneKeys?.tlk, "Should have a tlk in the original key set")

        self.startCKAccountStatusMock()
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.silentZoneDeletesAllowed = true

        do {
            _ = try OTClique.newFriends(withContextData: self.otcliqueContext)
        } catch {
            XCTFail("failed to make new friends: \(error)")
        }

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLK, within: 10 * NSEC_PER_SEC)

        let laterZoneKeys = self.keys![self.manateeZoneID] as? ZoneKeys
        XCTAssertNotNil(laterZoneKeys, "Should have some zone keys")
        XCTAssertNotNil(laterZoneKeys?.tlk, "Should have a tlk in the newly created keyset")
        XCTAssertEqual(zoneKeys?.tlk?.uuid, laterZoneKeys?.tlk?.uuid, "CKKS zone should now have the same keys")
    }

    func testOctagonResetWithTLKsDoesNotResetCKKS() {
        // CKKS has the keys keys
        self.putFakeKeyHierarchy(inCloudKit: self.manateeZoneID)
        self.saveTLKMaterial(toKeychain:self.manateeZoneID)

        let zoneKeys = self.keys![self.manateeZoneID] as? ZoneKeys
        XCTAssertNotNil(zoneKeys, "Should have some zone keys")
        XCTAssertNotNil(zoneKeys?.tlk, "Should have a tlk in the original key set")

        self.startCKAccountStatusMock()
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        do {
            _ = try OTClique.newFriends(withContextData: self.otcliqueContext)
        } catch {
            XCTFail("failed to make new friends: \(error)")
        }
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        let laterZoneKeys = self.keys![self.manateeZoneID] as? ZoneKeys
        XCTAssertNotNil(laterZoneKeys, "Should have some zone keys")
        XCTAssertNotNil(laterZoneKeys?.tlk, "Should have a tlk in the newly created keyset")
        XCTAssertEqual(zoneKeys?.tlk?.uuid, laterZoneKeys?.tlk?.uuid, "CKKS zone should now have the same keys")
    }

    func testOctagonResetAndEstablishFail() throws {
        // Make sure if establish fail we end up in untrusted instead of error
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        _ = try self.cuttlefishContext.accountAvailable("13453464")

        let establishExpectation = self.expectation(description: "establishExpectation")
        let resetExpectation = self.expectation(description: "resetExpectation")

        self.fakeCuttlefishServer.establishListener = {  [unowned self] request in
            self.fakeCuttlefishServer.establishListener = nil
            establishExpectation.fulfill()

            return CKPrettyError(domain: CKInternalErrorDomain,
                                 code: CKInternalErrorCode.errorInternalPluginError.rawValue,
                                 userInfo: [NSUnderlyingErrorKey: NSError(domain: CuttlefishErrorDomain,
                                                                          code: CuttlefishErrorCode.establishFailed.rawValue,
                                                                          userInfo: nil)])

        }

        self.cuttlefishContext.rpcResetAndEstablish() { resetError in
            resetExpectation.fulfill()
            XCTAssertNotNil(resetError, "should error resetting and establishing")
        }
        self.wait(for: [establishExpectation, resetExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

}

#endif // OCTAGON
