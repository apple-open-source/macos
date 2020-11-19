#if OCTAGON

class OctagonResetTests: OctagonTestsBase {
    func testAccountAvailableAndHandleExternalCall() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        _ = try self.cuttlefishContext.accountAvailable("13453464")

        self.cuttlefishContext.rpcResetAndEstablish(.testGenerated) { resetError in
            XCTAssertNil(resetError, "should be no error resetting and establishing")
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func testExernalCallAndAccountAvailable() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.cuttlefishContext.rpcResetAndEstablish(.testGenerated) { resetError in
            XCTAssertNil(resetError, "should be no error resetting and establishing")
        }

        _ = try self.cuttlefishContext.accountAvailable("13453464")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func testCallingAccountAvailableDuringResetAndEstablish() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.cuttlefishContext.rpcResetAndEstablish(.testGenerated) { resetError in
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
                                       altDSID: "new altDSID",
                                       resetReason: .testGenerated) { resetError in
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
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
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

        // and all subCKKSes should enter ready...
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // CKKS should pass through "waitfortrust" during a reset
        let waitfortrusts = self.injectedManager!.views.allValues.map { view in
            (view as! CKKSKeychainView).keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTrust]!
        }
        XCTAssert(!waitfortrusts.isEmpty, "Should have at least one waitfortrust condition")

        let resetAndEstablishExpectation = self.expectation(description: "resetAndEstablish callback occurs")
        let escrowRequestNotification = expectation(forNotification: OTMockEscrowRequestNotification,
                                                    object: nil,
                                                    handler: nil)
        self.manager.resetAndEstablish(containerName,
                                       context: contextName,
                                       altDSID: "new altDSID",
                                       resetReason: .testGenerated) { resetError in
                                        XCTAssertNil(resetError, "Should be no error calling resetAndEstablish")
                                        resetAndEstablishExpectation.fulfill()
        }

        self.wait(for: [resetAndEstablishExpectation], timeout: 10)
        self.wait(for: [escrowRequestNotification], timeout: 5)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        // CKKS should have all gone into waitfortrust during that time
        for condition in waitfortrusts {
            XCTAssertEqual(0, condition.wait(10 * NSEC_PER_MSEC), "CKKS should have entered waitfortrust")
        }
    }

    func testOctagonResetAlsoResetsCKKSViewsMissingTLKs() {
        self.putFakeKeyHierarchiesInCloudKit()
        self.putFakeDeviceStatusesInCloudKit()

        let zoneKeys = self.keys![self.limitedPeersAllowedZoneID!] as? ZoneKeys
        XCTAssertNotNil(zoneKeys, "Should have some zone keys")
        XCTAssertNotNil(zoneKeys?.tlk, "Should have a tlk in the original key set")

        self.startCKAccountStatusMock()
        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTrust, within: 10 * NSEC_PER_SEC)

        self.silentZoneDeletesAllowed = true

        do {
            _ = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
        } catch {
            XCTFail("failed to make new friends: \(error)")
        }

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        let laterZoneKeys = self.keys![self.limitedPeersAllowedZoneID!] as? ZoneKeys
        XCTAssertNotNil(laterZoneKeys, "Should have some zone keys")
        XCTAssertNotNil(laterZoneKeys?.tlk, "Should have a tlk in the newly created keyset")
        XCTAssertNotEqual(zoneKeys?.tlk?.uuid, laterZoneKeys?.tlk?.uuid, "CKKS zone should now have different keys")
    }

    func testOctagonResetIgnoresOldRemoteDevicesWithKeysAndResetsCKKS() {
        // CKKS has no keys, and there's another device claiming to have them already, but it's old
        self.putFakeKeyHierarchiesInCloudKit()
        self.putFakeDeviceStatusesInCloudKit()

        #if !os(tvOS)
        (self.zones![self.manateeZoneID!]! as! FakeCKZone).currentDatabase.allValues.forEach { record in
            let r = record as! CKRecord
            if r.recordType == SecCKRecordDeviceStateType {
                r.creationDate = NSDate.distantPast
                r.modificationDate = NSDate.distantPast
            }
        }
        #endif
        (self.zones![self.limitedPeersAllowedZoneID!]! as! FakeCKZone).currentDatabase.allValues.forEach { record in
            let r = record as! CKRecord
            if r.recordType == SecCKRecordDeviceStateType {
                r.creationDate = NSDate.distantPast
                r.modificationDate = NSDate.distantPast
            }
        }

        #if !os(tvOS)
        let zoneKeys = self.keys![self.manateeZoneID!] as? ZoneKeys
        XCTAssertNotNil(zoneKeys, "Should have some zone keys for Manatee")
        XCTAssertNotNil(zoneKeys?.tlk, "Should have a tlk in the original key set for Manatee")
        #endif

        let lpZoneKeys = self.keys![self.limitedPeersAllowedZoneID!] as? ZoneKeys
        XCTAssertNotNil(lpZoneKeys, "Should have some zone keys for LimitedPeers")
        XCTAssertNotNil(lpZoneKeys?.tlk, "Should have a tlk in the original key set for LimitedPeers")

        self.startCKAccountStatusMock()
        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.silentZoneDeletesAllowed = true

        do {
            _ = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
        } catch {
            XCTFail("failed to make new friends: \(error)")
        }

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        #if !os(tvOS)
        let laterZoneKeys = self.keys![self.manateeZoneID!] as? ZoneKeys
        XCTAssertNotNil(laterZoneKeys, "Should have some zone keys for Manatee")
        XCTAssertNotNil(laterZoneKeys?.tlk, "Should have a tlk in the newly created keyset for Manatee")
        XCTAssertNotEqual(zoneKeys?.tlk?.uuid, laterZoneKeys?.tlk?.uuid, "CKKS zone should now have different keys for Manatee")
        #else
        let laterZoneKeys = self.keys![self.manateeZoneID!] as? ZoneKeys
        XCTAssertNil(laterZoneKeys, "Should have no Manatee zone keys for aTV")
        #endif

        let laterLpZoneKeys = self.keys![self.limitedPeersAllowedZoneID!] as? ZoneKeys
        XCTAssertNotNil(laterLpZoneKeys, "Should have some zone keys for LimitedPeers")
        XCTAssertNotNil(laterLpZoneKeys?.tlk, "Should have a tlk in the newly created keyset for LimitedPeers")
        XCTAssertNotEqual(lpZoneKeys?.tlk?.uuid, laterLpZoneKeys?.tlk?.uuid, "CKKS zone should now have different keys for LimitedPeers")
    }

    func testOctagonResetWithRemoteDevicesWithKeysDoesNotResetCKKS() {
        // CKKS has no keys, and there's another device claiming to have them already, so CKKS won't immediately reset it.
        // But, Octagon will!
        self.putFakeKeyHierarchiesInCloudKit()
        self.putFakeDeviceStatusesInCloudKit()

        #if !os(tvOS)
        let zoneKeys = self.keys![self.manateeZoneID!] as? ZoneKeys
        XCTAssertNotNil(zoneKeys, "Should have some zone keys")
        XCTAssertNotNil(zoneKeys?.tlk, "Should have a tlk in the original key set")
        #endif

        let lpZoneKeys = self.keys![self.limitedPeersAllowedZoneID!] as? ZoneKeys
        XCTAssertNotNil(lpZoneKeys, "Should have some zone keys for LimitedPeers")
        XCTAssertNotNil(lpZoneKeys?.tlk, "Should have a tlk in the original key set for LimitedPeers")

        self.startCKAccountStatusMock()
        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.silentZoneDeletesAllowed = true

        do {
            _ = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
        } catch {
            XCTFail("failed to make new friends: \(error)")
        }

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        #if !os(tvOS)
        let laterZoneKeys = self.keys![self.manateeZoneID!] as? ZoneKeys
        XCTAssertNotNil(laterZoneKeys, "Should have some zone keys")
        XCTAssertNotNil(laterZoneKeys?.tlk, "Should have a tlk in the newly created keyset")
        XCTAssertNotEqual(zoneKeys?.tlk?.uuid, laterZoneKeys?.tlk?.uuid, "CKKS zone should not have the same keys - a reset should have occurred")
        #else
        let laterZoneKeys = self.keys![self.manateeZoneID!] as? ZoneKeys
        XCTAssertNil(laterZoneKeys, "Should have no Manatee zone keys for aTV")
        #endif

        let lpLaterZoneKeys = self.keys![self.limitedPeersAllowedZoneID!] as? ZoneKeys
        XCTAssertNotNil(lpLaterZoneKeys, "Should have some zone keys for LimitedPeersAllowed")
        XCTAssertNotNil(lpLaterZoneKeys?.tlk, "Should have a tlk in the newly created keyset for LimitedPeersAllowed")
        XCTAssertNotEqual(lpZoneKeys?.tlk?.uuid, lpLaterZoneKeys?.tlk?.uuid, "CKKS zone should not have the same keys for LimitedPeersAllowed - a reset should have occurred")
    }

    func testOctagonResetWithTLKsDoesNotResetCKKS() {
        // CKKS has the keys keys
        self.putFakeKeyHierarchiesInCloudKit()
        self.saveTLKMaterialToKeychain()

        #if !os(tvOS)
        let zoneKeys = self.keys![self.manateeZoneID!] as? ZoneKeys
        XCTAssertNotNil(zoneKeys, "Should have some zone keys")
        XCTAssertNotNil(zoneKeys?.tlk, "Should have a tlk in the original key set")
        #endif

        let lpZoneKeys = self.keys![self.limitedPeersAllowedZoneID!] as? ZoneKeys
        XCTAssertNotNil(lpZoneKeys, "Should have some zone keys for LimitedPeers")
        XCTAssertNotNil(lpZoneKeys?.tlk, "Should have a tlk in the original key set for LimitedPeers")

        self.startCKAccountStatusMock()
        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        do {
            _ = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
        } catch {
            XCTFail("failed to make new friends: \(error)")
        }
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        #if !os(tvOS)
        let laterZoneKeys = self.keys![self.manateeZoneID!] as? ZoneKeys
        XCTAssertNotNil(laterZoneKeys, "Should have some zone keys")
        XCTAssertNotNil(laterZoneKeys?.tlk, "Should have a tlk in the newly created keyset")
        XCTAssertEqual(zoneKeys?.tlk?.uuid, laterZoneKeys?.tlk?.uuid, "CKKS zone should now have the same keys")
        #else
        let laterZoneKeys = self.keys![self.manateeZoneID!] as? ZoneKeys
        XCTAssertNil(laterZoneKeys, "Should have no Manatee zone keys for aTV")
        #endif

        let lpLaterZoneKeys = self.keys![self.limitedPeersAllowedZoneID!] as? ZoneKeys
        XCTAssertNotNil(lpLaterZoneKeys, "Should have some zone keys for LimitedPeersAllowed")
        XCTAssertNotNil(lpLaterZoneKeys?.tlk, "Should have a tlk in the newly created keyset for LimitedPeersAllowed")
        XCTAssertEqual(lpZoneKeys?.tlk?.uuid, lpLaterZoneKeys?.tlk?.uuid, "CKKS zone should now have the same keys for LimitedPeersAllowed")
    }

    func testOctagonResetAndEstablishFail() throws {
        // Make sure if establish fail we end up in untrusted instead of error
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        _ = try self.cuttlefishContext.accountAvailable("13453464")

        let establishExpectation = self.expectation(description: "establishExpectation")
        let resetExpectation = self.expectation(description: "resetExpectation")

        self.fakeCuttlefishServer.establishListener = {  [unowned self] request in
            self.fakeCuttlefishServer.establishListener = nil
            establishExpectation.fulfill()

            return FakeCuttlefishServer.makeCloudKitCuttlefishError(code: .establishFailed)
        }

        self.cuttlefishContext.rpcResetAndEstablish(.testGenerated) { resetError in
            resetExpectation.fulfill()
            XCTAssertNotNil(resetError, "should error resetting and establishing")
        }
        self.wait(for: [establishExpectation, resetExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // And after this reset, can we put together an escrow record?
        let escrowContentsExpectation = self.expectation(description: "fetchEscrowContents")
        self.cuttlefishContext.fetchEscrowContents { entropy, bottleID, signingPubKey, error in
            XCTAssertNil(error, "Should be no error fetching escrow contents")

            XCTAssertNotNil(entropy, "Should have some entropy")
            XCTAssertNotNil(bottleID, "Should have some bottleID")
            XCTAssertNotNil(signingPubKey, "Should have some signing public key")

            escrowContentsExpectation.fulfill()
        }
        self.wait(for: [escrowContentsExpectation], timeout: 10)
    }

    func testResetReasonUnknown() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        _ = try self.cuttlefishContext.accountAvailable("13453464")

        let resetExpectation = self.expectation(description: "resetExpectation")

        self.fakeCuttlefishServer.resetListener = {  request in
            self.fakeCuttlefishServer.resetListener = nil
            resetExpectation.fulfill()
            XCTAssertTrue(request.resetReason.rawValue == CuttlefishResetReason.unknown.rawValue, "reset reason should be unknown")
            return nil
        }

        let establishAndResetExpectation = self.expectation(description: "resetExpectation")
        self.cuttlefishContext.rpcResetAndEstablish(.unknown) { resetError in
            establishAndResetExpectation.fulfill()
            XCTAssertNil(resetError, "should not error resetting and establishing")
        }
        self.wait(for: [establishAndResetExpectation, resetExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func testResetReasonUserInitiatedReset() throws {
        // Make sure if establish fail we end up in untrusted instead of error
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        _ = try self.cuttlefishContext.accountAvailable("13453464")

        let resetExpectation = self.expectation(description: "resetExpectation")

        self.fakeCuttlefishServer.resetListener = {  request in
            self.fakeCuttlefishServer.resetListener = nil
            resetExpectation.fulfill()
            XCTAssertTrue(request.resetReason.rawValue == CuttlefishResetReason.userInitiatedReset.rawValue, "reset reason should be user initiated reset")
            return nil
        }

        let establishAndResetExpectation = self.expectation(description: "resetExpectation")
        let clique: OTClique
        let recoverykeyotcliqueContext = OTConfigurationContext()
        recoverykeyotcliqueContext.context = OTDefaultContext
        recoverykeyotcliqueContext.dsid = "13453464"
        recoverykeyotcliqueContext.altDSID = self.mockAuthKit.altDSID!
        recoverykeyotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: recoverykeyotcliqueContext, resetReason: .userInitiatedReset)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
            establishAndResetExpectation.fulfill()
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }
        self.wait(for: [establishAndResetExpectation, resetExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func testResetReasonRecoveryKey() throws {
        // Make sure if establish fail we end up in untrusted instead of error
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        _ = try self.cuttlefishContext.accountAvailable("13453464")

        let recoveryKey = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        let newCliqueContext = OTConfigurationContext()
        newCliqueContext.context = OTDefaultContext
        newCliqueContext.dsid = self.otcliqueContext.dsid
        newCliqueContext.altDSID = self.mockAuthKit.altDSID!
        newCliqueContext.otControl = self.otControl

        // Calling with an unknown RK only resets if the local device is SOS capable, so pretend it is
        OctagonSetSOSFeatureEnabled(false)
        #if os(macOS) || os(iOS)
        OctagonSetPlatformSupportsSOS(true)
        self.manager.setSOSEnabledForPlatformFlag(true)

        let resetExpectation = self.expectation(description: "resetExpectation")

        self.fakeCuttlefishServer.resetListener = {  request in
            self.fakeCuttlefishServer.resetListener = nil
            resetExpectation.fulfill()
            XCTAssertTrue(request.resetReason.rawValue == CuttlefishResetReason.recoveryKey.rawValue, "reset reason should be recovery key")
            return nil
        }
        #else
        self.manager.setSOSEnabledForPlatformFlag(false)
        OctagonSetPlatformSupportsSOS(false)
        #endif

        let joinWithRecoveryKeyExpectation = self.expectation(description: "joinWithRecoveryKeyExpectation callback occurs")
        TestsObjectiveC.recoverOctagon(usingData: newCliqueContext, recoveryKey: recoveryKey) { error in
            #if os(macOS) || os(iOS)
            XCTAssertNil(error, "error should be nil")
            #else
            XCTAssertNotNil(error, "error should not be nil")
            #endif
            joinWithRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithRecoveryKeyExpectation], timeout: 20)

        #if os(macOS) || os(iOS)
        self.wait(for: [resetExpectation], timeout: 10)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        #else
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        #endif
        self.verifyDatabaseMocks()
    }

    func testResetReasonNoValidBottle() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let initiatorContext = self.manager.context(forContainerName: OTCKContainerName,
                                                    contextID: "restoreContext",
                                                    sosAdapter: OTSOSMissingAdapter(),
                                                    authKitAdapter: self.mockAuthKit2,
                                                    lockStateTracker: self.lockStateTracker,
                                                    accountStateTracker: self.accountStateTracker,
                                                    deviceInformationAdapter: self.makeInitiatorDeviceInfoAdapter())

        initiatorContext.startOctagonStateMachine()
        let newOTCliqueContext = OTConfigurationContext()
        newOTCliqueContext.context = OTDefaultContext
        newOTCliqueContext.dsid = self.otcliqueContext.dsid
        newOTCliqueContext.altDSID = self.otcliqueContext.altDSID
        newOTCliqueContext.otControl = self.otcliqueContext.otControl
        newOTCliqueContext.sbd = OTMockSecureBackup(bottleID: nil, entropy: nil)

        let resetExpectation = self.expectation(description: "resetExpectation callback occurs")
        self.fakeCuttlefishServer.resetListener = {  request in
            self.fakeCuttlefishServer.resetListener = nil
            resetExpectation.fulfill()
            XCTAssertTrue(request.resetReason.rawValue == CuttlefishResetReason.noBottleDuringEscrowRecovery.rawValue, "reset reason should be no bottle during escrow recovery")
            return nil
        }

        let newClique: OTClique
        do {
            newClique = try OTClique.performEscrowRecovery(withContextData: newOTCliqueContext, escrowArguments: [:])
            XCTAssertNotNil(newClique, "newClique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored recovering: \(error)")
            throw error
        }
        self.wait(for: [resetExpectation], timeout: 10)
    }

    func testResetReasonHealthCheck() throws {
        let containerName = OTCKContainerName
        let contextName = OTDefaultContext

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
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

        // Reset any CFUs we've done so far
        self.otFollowUpController.postedFollowUp = false

        let resetExpectation = self.expectation(description: "resetExpectation callback occurs")
        self.fakeCuttlefishServer.resetListener = {  request in
            self.fakeCuttlefishServer.resetListener = nil
            resetExpectation.fulfill()
            XCTAssertTrue(request.resetReason.rawValue == CuttlefishResetReason.healthCheck.rawValue, "reset reason should be health check")
            return nil
        }
        self.fakeCuttlefishServer.returnResetOctagonResponse = true
        self.aksLockState = false
        self.lockStateTracker.recheck()

        let healthCheckCallback = self.expectation(description: "healthCheckCallback callback occurs")
        self.manager.healthCheck(containerName, context: contextName, skipRateLimitingCheck: false) { error in
            XCTAssertNil(error, "error should be nil")
            healthCheckCallback.fulfill()
        }
        self.wait(for: [healthCheckCallback, resetExpectation], timeout: 10)

        self.assertEnters(context: cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(withContainer: containerName, context: contextName) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)
    }

    func testLegacyJoinCircleDoesNotReset() throws {
        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let establishAndResetExpectation = self.expectation(description: "resetExpectation")
        let clique: OTClique
        let recoverykeyotcliqueContext = OTConfigurationContext()
        recoverykeyotcliqueContext.context = OTDefaultContext
        recoverykeyotcliqueContext.dsid = "13453464"
        recoverykeyotcliqueContext.altDSID = self.mockAuthKit.altDSID!
        recoverykeyotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: recoverykeyotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
            establishAndResetExpectation.fulfill()
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }
        self.wait(for: [establishAndResetExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.fakeCuttlefishServer.resetListener = {  request in
            XCTFail("requestToJoinCircle should not reset Octagon")
            return nil
        }

        do {
            _ = try clique.requestToJoinCircle()
        } catch {
            XCTFail("Shouldn't have errored requesting to join circle: \(error)")
            throw error
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testResetReasonTestGenerated() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        _ = try self.cuttlefishContext.accountAvailable("13453464")

        let resetExpectation = self.expectation(description: "resetExpectation")

        self.fakeCuttlefishServer.resetListener = {  request in
            self.fakeCuttlefishServer.resetListener = nil
            resetExpectation.fulfill()
            XCTAssertTrue(request.resetReason.rawValue == CuttlefishResetReason.testGenerated.rawValue, "reset reason should be test generated")
            return nil
        }

        let establishAndResetExpectation = self.expectation(description: "resetExpectation")
        self.cuttlefishContext.rpcResetAndEstablish(.testGenerated) { resetError in
            establishAndResetExpectation.fulfill()
            XCTAssertNil(resetError, "should not error resetting and establishing")
        }
        self.wait(for: [establishAndResetExpectation, resetExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func testCliqueResetAllSPI() throws {
        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()
        OctagonSetSOSFeatureEnabled(false)

        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let establishAndResetExpectation = self.expectation(description: "resetExpectation")
        let clique: OTClique
        let otcliqueContext = OTConfigurationContext()
        var firstCliqueIdentifier: String?

        otcliqueContext.context = OTDefaultContext
        otcliqueContext.dsid = "13453464"
        otcliqueContext.altDSID = self.mockAuthKit.altDSID!
        otcliqueContext.authenticationAppleID = "appleID"
        otcliqueContext.passwordEquivalentToken = "petpetpetpetpet"
        otcliqueContext.otControl = self.otControl
        otcliqueContext.ckksControl = self.ckksControl
        otcliqueContext.sbd = OTMockSecureBackup(bottleID: nil, entropy: nil)

        do {
            clique = try OTClique.newFriends(withContextData: otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
            firstCliqueIdentifier = clique.cliqueMemberIdentifier
            establishAndResetExpectation.fulfill()
        } catch {
            XCTFail("Shouldn't have errored making new friends everything: \(error)")
            throw error
        }
        self.wait(for: [establishAndResetExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.silentZoneDeletesAllowed = true

        let newClique: OTClique
        do {
            newClique = try OTClique.resetProtectedData(otcliqueContext)
            XCTAssertNotEqual(newClique.cliqueMemberIdentifier, firstCliqueIdentifier, "clique identifiers should be different")
        } catch {
            XCTFail("Shouldn't have errored resetting everything: \(error)")
            throw error
        }
        XCTAssertNotNil(newClique, "newClique should not be nil")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func testCliqueResetProtectedDataHandlingInMultiPeerCircle() throws {
        OctagonSetSOSFeatureEnabled(false)
        self.startCKAccountStatusMock()

        var firstCliqueIdentifier: String?

        let clique: OTClique
        let cliqueContextConfiguration = OTConfigurationContext()
        cliqueContextConfiguration.context = OTDefaultContext
        cliqueContextConfiguration.dsid = "13453464"
        cliqueContextConfiguration.altDSID = self.mockAuthKit.altDSID!
        cliqueContextConfiguration.authenticationAppleID = "appleID"
        cliqueContextConfiguration.passwordEquivalentToken = "petpetpetpetpet"
        cliqueContextConfiguration.otControl = self.otControl
        cliqueContextConfiguration.ckksControl = self.ckksControl
        cliqueContextConfiguration.sbd = OTMockSecureBackup(bottleID: nil, entropy: nil)
        do {
            clique = try OTClique.newFriends(withContextData: cliqueContextConfiguration, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
            firstCliqueIdentifier = clique.cliqueMemberIdentifier
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        let entropy = try self.loadSecret(label: clique.cliqueMemberIdentifier!)
        XCTAssertNotNil(entropy, "entropy should not be nil")

        let bottleJoinerContextID = "bottleJoiner"
        let joinerContext = self.manager.context(forContainerName: OTCKContainerName, contextID: bottleJoinerContextID)

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        joinerContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()
        self.assertEnters(context: joinerContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // Before you call joinWithBottle, you need to call fetchViableBottles.
        let fetchViableExpectation = self.expectation(description: "fetchViableBottles callback occurs")
        joinerContext.rpcFetchAllViableBottles { viable, _, error in
            XCTAssertNil(error, "should be no error fetching viable bottles")
            XCTAssert(viable?.contains(bottle.bottleID) ?? false, "The bottle we're about to restore should be viable")
            fetchViableExpectation.fulfill()
        }
        self.wait(for: [fetchViableExpectation], timeout: 10)

        let joinWithBottleExpectation = self.expectation(description: "joinWithBottle callback occurs")
        joinerContext.join(withBottle: bottle.bottleID, entropy: entropy!, bottleSalt: self.mockAuthKit.altDSID!) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithBottleExpectation.fulfill()
        }

        self.wait(for: [joinWithBottleExpectation], timeout: 10)

        self.assertEnters(context: joinerContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: joinerContext)

        self.silentZoneDeletesAllowed = true

        let newClique: OTClique
        do {
            newClique = try OTClique.resetProtectedData(cliqueContextConfiguration)
            XCTAssertNotEqual(newClique.cliqueMemberIdentifier, firstCliqueIdentifier, "clique identifiers should be different")
        } catch {
            XCTFail("Shouldn't have errored resetting everything: \(error)")
            throw error
        }
        XCTAssertNotNil(newClique, "newClique should not be nil")

        self.sendContainerChangeWaitForFetchForStates(context: joinerContext, states: [OctagonStateUntrusted])
        self.sendContainerChangeWaitForFetchForStates(context: self.cuttlefishContext, states: [OctagonStateReady])
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        let statusExpectation = self.expectation(description: "status callback occurs")
        let configuration = OTOperationConfiguration()

        joinerContext.rpcTrustStatus(configuration) { egoStatus, _, _, _, _, _ in
            XCTAssertEqual(.notIn, egoStatus, "cliqueStatus should be 'Not In'")
            statusExpectation.fulfill()
        }
        self.wait(for: [statusExpectation], timeout: 10)
    }

    func testAcceptResetRemovesSelfPeer() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let otherPeer = self.makeInitiatorContext(contextID: "peer2")
        self.assertResetAndBecomeTrusted(context: otherPeer)

        // And we get told about the reset
        self.sendContainerChangeWaitForFetchForStates(context: self.cuttlefishContext,
                                                      states: [OctagonStateReadyUpdated,
                                                               OctagonStateUntrusted, ])

        XCTAssertEqual(self.cuttlefishContext.stateMachine.paused.wait(5 * NSEC_PER_SEC), 0, "State machine should have paused")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 1 * NSEC_PER_SEC)
    }
}

#endif // OCTAGON
