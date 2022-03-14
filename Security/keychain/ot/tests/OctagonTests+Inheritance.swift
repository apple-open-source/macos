#if OCTAGON

@objcMembers
class OctagonInheritanceTests: OctagonTestsBase {
    override func setUp() {
        // Please don't make the SOS API calls, no matter what
        OctagonSetSOSFeatureEnabled(false)

        super.setUp()

        // Set this to what it normally is. Each test can muck with it, if they like
#if os(macOS) || os(iOS)
        OctagonSetPlatformSupportsSOS(true)
        self.manager.setSOSEnabledForPlatformFlag(true)
#else
        self.manager.setSOSEnabledForPlatformFlag(false)
        OctagonSetPlatformSupportsSOS(false)
#endif
    }

    func testBase32Bad() throws {
        let badExamples: [[UInt8]] = [
            [],
            [0, 0, 0, 0, 0, 0],
        ]
        for e in badExamples {
            XCTAssertNil(OTInheritanceKey.base32(e, len: e.count))
        }
    }

    func testPrintableBadChecksumSize() throws {
        XCTAssertThrowsError(try OTInheritanceKey.printable(with: Data(), checksumSize: 33),
            "bad checksum size should fail")
    }

    func testParseBase32BadChecksumSize() throws {
        for size in [1, 33] {
            XCTAssertThrowsError(try OTInheritanceKey.parseBase32("", checksumSize: size),
                "bad checksum size should fail")
        }
    }

    func testPrintable() throws {
        let samples: [([UInt8], Int, String)] = [
            ([], 0, ""),
            ([0x00], 0, "AA"),
            ([0x00], 1, "ABZA"),
            ([0x00, 0x00], 0, "AAAA"),
            ([0x00, 0x00, 0x00], 0, "AAAA-A"),
        ]
        for e in samples {
            let (inArray, checksumSize, out) = e
            let actualOut = try OTInheritanceKey.printable(with: Data(inArray), checksumSize: checksumSize)
            XCTAssertEqual(actualOut, out, "\(inArray) failed")
        }
    }

    func testParseBase32() throws {
        let samples: [(String, Int, [UInt8])] = [
            ("", 0, []),
            ("AA", 0, [0x00]),
            ("ABZA", 1, [0x00]),
            ("AAAA", 0, [0x00, 0x00]),
            ("AAAA-A", 0, [0x00, 0x00, 0x00]),
        ]
        for e in samples {
            let (inString, checksumSize, outArray) = e
            let actualOut = try OTInheritanceKey.parseBase32(inString, checksumSize: checksumSize)
            XCTAssertEqual(Array(actualOut), outArray, "\(inString) failed")
        }
    }

    func testPrintableE2E() throws {
        let samples: [[UInt8]] = [
            [],
            [0x00],
            [0x01],
            [0x00, 0xFF],
            [0x01, 0x02, 0x03],
            [0xFF, 0xFE, 0xFD, 0xFC, 0xFB, 0xFA, 0xF9, 0xF8, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01],
        ]
        for e in samples {
            let enc = try OTInheritanceKey.printable(with: Data(e), checksumSize: 7)
            let dec = try OTInheritanceKey.parseBase32(enc, checksumSize: 7)
            XCTAssertEqual(Array(dec), e, "printable + parseBase32 should be identity function")
        }
    }

    func testBase32Good() throws {
        let examples: [([UInt8], String)] = [
            ([0], "AA"),
            ([0, 0], "AAAA"),
            ([0, 0, 0], "AAAAA"),
            ([0, 0, 0, 0], "AAAAAAA"),
            ([0, 0, 0, 0, 0], "AAAAAAAA"),
            ([0xFF], "96"),
            ([0xFF, 0xFF], "999S"),
            ([0xFF, 0xFF, 0xFF], "99998"),
            ([0xFF, 0xFF, 0xFF, 0xFF], "9999992"),
            ([0xFF, 0xFF, 0xFF, 0xFF, 0xFF], "99999999"),
            ([0xA6, 0x6A, 0xC3, 0x3C, 0x41], "W3XNGRCB"),
        ]
        for e in examples {
            let (inData, expectedOut) = e
            let actualOut = OTInheritanceKey.base32(inData, len: inData.count)
            XCTAssertEqual(actualOut, expectedOut, "\(inData) failed")
        }
    }

    func testUnbase32Bad() throws {
        let badExamples: [String] = [
            "",
            "X",
            "XXX",
            "XXXXXX",
            "XXXXXXXXX",
            "a01I!@#$",
        ]
        for e in badExamples {
            let a: [UInt8] = e.utf8.map { UInt8($0) }
            XCTAssertNil(OTInheritanceKey.unbase32(a, len: a.count))
        }
    }

    func testUnbase32Good() throws {
        let examples: [([UInt8], String)] = [
            ([0], "AA"),
            ([0, 0], "AAAA"),
            ([0, 0, 0], "AAAAA"),
            ([0, 0, 0, 0], "AAAAAAA"),
            ([0, 0, 0, 0, 0], "AAAAAAAA"),
            ([0xFF], "96"),
            ([0xFF, 0xFF], "999S"),
            ([0xFF, 0xFF, 0xFF], "99998"),
            ([0xFF, 0xFF, 0xFF, 0xFF], "9999992"),
            ([0xFF, 0xFF, 0xFF, 0xFF, 0xFF], "99999999"),
            ([0xA6, 0x6A, 0xC3, 0x3C, 0x41], "W3XNGRCB"),
        ]
        for e in examples {
            let (expectedOut, inStr) = e
            let a: [UInt8] = inStr.utf8.map { UInt8($0) }
            let actualOut = OTInheritanceKey.unbase32(a, len: a.count)
            XCTAssertEqual(Array(actualOut ?? Data()), expectedOut, "\(inStr) failed")
        }
    }

    func testIKWrapUnwrapBinary() throws {
        let uuid = UUID()

        let ik1 = try OTInheritanceKey(uuid: uuid)
        let ik2 = try OTInheritanceKey(wrappedKeyData: ik1.wrappedKeyData,
                                       wrappingKeyData: ik1.wrappingKeyData,
                                       uuid: uuid)
        XCTAssertTrue(ik1.isRecoveryKeyEqual(ik2), "first IK and reconstructed should match")
    }

    func testIKWrapUnwrapString() throws {
        let uuid = UUID()

        let ik1 = try OTInheritanceKey(uuid: uuid)
        let ik2 = try OTInheritanceKey(wrappedKeyString: ik1.wrappedKeyString,
                                       wrappingKeyData: ik1.wrappingKeyData,
                                       uuid: uuid)
        XCTAssertTrue(ik1.isRecoveryKeyEqual(ik2), "first IK and reconstructed should match")
    }

    func testIKWrapUnwrapString2() throws {
        let uuid = UUID()

        let ik1 = try OTInheritanceKey(uuid: uuid)
        let ik2 = try OTInheritanceKey(wrappedKeyData: ik1.wrappedKeyData,
                                       wrappingKeyString: ik1.wrappingKeyString,
                                       uuid: uuid)
        XCTAssertTrue(ik1.isRecoveryKeyEqual(ik2), "first IK and reconstructed should match")
    }

    func assertMatches(string: String, re: String, msg: String, file: StaticString = #file, line: UInt = #line) {
        let r = string.range(of: re, options: .regularExpression)
        XCTAssertNotNil(r, "failed to match re (\(re)) to string (\(string)): \(msg)", file: file, line: line)
    }

    func testIKStringFormat() throws {
        let A = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"
        let uuid = UUID()
        let ik = try OTInheritanceKey(uuid: uuid)
        XCTAssertEqual(16, ik.claimTokenData!.count, "claim token length")
        XCTAssertEqual(32, ik.wrappingKeyData.count, "wrapping key length")
        self.assertMatches(string: ik.claimTokenString!,
            re: "^[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}$",
            msg: "claim token string does not match")
        self.assertMatches(string: ik.wrappingKeyString,
            re: "^[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}$",
            msg: "wrapping key string does not match")
        self.assertMatches(string: ik.wrappedKeyString,
            re: """
            ^[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-\
            [\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-\
            [\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-\
            [\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-[\(A)]{4}-\
            [\(A)]{4}-[\(A)]{4}$
            """,
            msg: "wrapped key string does not match")
    }

    func testIKUnwrapBadWrappingKeyLength() throws {
        let sizes: [Int] = stride(from: 0, to: 31, by: 1) + [33]
        for size in sizes {
            let uuid = UUID()
            let ik1 = try OTInheritanceKey(uuid: uuid)
            XCTAssertThrowsError(try OTInheritanceKey(wrappedKeyData: ik1.wrappedKeyData,
                                                      wrappingKeyData: Data(count: size),
                                                      uuid: uuid),
                                                      "wrappingKey of \(size) should fail")
        }
    }

    func testIKUnwrapBadWrappedKeyLength() throws {
        let sizes: [Int] = stride(from: 0, to: 71, by: 1) + [73]
        for size in sizes {
            let uuid = UUID()
            let ik1 = try OTInheritanceKey(uuid: uuid)
            XCTAssertThrowsError(try OTInheritanceKey(wrappedKeyData: Data(count: size),
                                                      wrappingKeyData: ik1.wrappingKeyData,
                                                      uuid: uuid),
                                                      "wrappedKey of \(size) should fail")
        }
    }

    func testIKDataLengths() throws {
        let uuid = UUID()
        let ik = try OTInheritanceKey(uuid: uuid)
        XCTAssertEqual(32, ik.wrappingKeyData.count, "wrapping key data wrong size")
        XCTAssertEqual(72, ik.wrappedKeyData.count, "wrapped key data wrong size")
    }

    func createEstablishContext(contextID: String) -> OTCuttlefishContext {
        return self.manager.context(forContainerName: OTCKContainerName,
                                    contextID: contextID,
                                    sosAdapter: self.mockSOSAdapter,
                                    authKitAdapter: self.mockAuthKit2,
                                    tooManyPeersAdapter: self.mockTooManyPeers,
                                    lockStateTracker: self.lockStateTracker,
                                    accountStateTracker: self.accountStateTracker,
                                    deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-IK-iphone", serialNumber: "456", osVersion: "iOS (fake version)"))
    }

    @discardableResult
    func createAndSetInheritanceRecoveryKey(context: OTCuttlefishContext) throws -> (OTInheritanceKey, InheritanceKey) {
        var retirk: OTInheritanceKey?
        let createInheritanceKeyExpectation = self.expectation(description: "createInheritanceKey returns")

        self.manager.createInheritanceKey(OTCKContainerName, contextID: context.contextID, uuid: nil) { irk, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNotNil(irk, "irk should be non-nil")
            XCTAssertNotNil(irk?.uuid, "uuid should be non-nil")
            retirk = irk
            createInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [createInheritanceKeyExpectation], timeout: 10)

        let otirk = try XCTUnwrap(retirk)

        self.assertEnters(context: context, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        let container = try self.tphClient.getContainer(withContainer: context.containerName, context: context.contextID)
        let custodian = try XCTUnwrap(container.model.findCustodianRecoveryKey(with: otirk.uuid))

        let irkWithKeys = try InheritanceKey(tpInheritance: custodian,
                                             recoveryKeyData: otirk.recoveryKeyData,
                                             recoverySalt: "")

        return (otirk, irkWithKeys)
    }

    func testCreateInheritanceTLKSharesDuringCreation() throws {
        self.manager.setSOSEnabledForPlatformFlag(false)
        self.startCKAccountStatusMock()

        self.assertResetAndBecomeTrustedInDefaultContext()

        // This flag gates whether or not we'll error while setting the recovery key
        self.manager.setSOSEnabledForPlatformFlag(true)

        let (_, irk) = try self.createAndSetInheritanceRecoveryKey(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.assertTLKSharesInCloudKit(receiverPeerID: irk.peerID, senderPeerID: irk.peerID)
    }

    func testResetAndEstablishClearsAccountMetadata() throws {
        let contextName = OTDefaultContext
        let containerName = OTCKContainerName

        self.manager.setSOSEnabledForPlatformFlag(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = self.mockAuthKit2.altDSID!
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        let establishedPeerID = self.fetchEgoPeerID(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        self.manager.setSOSEnabledForPlatformFlag(true)

        let (otirk, irk) = try self.createAndSetInheritanceRecoveryKey(context: establishContext)

        self.putInheritanceTLKSharesInCloudKit(irk: irk)
        self.sendContainerChangeWaitForFetch(context: establishContext)

        // Now, join from a new device
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: self.cuttlefishContext)

        let trustedNotification = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: "com.apple.security.octagon.trust-status-change"))
        let joinWithInheritanceKeyExpectation = self.expectation(description: "joinWithInheritanceKey callback occurs")
        self.cuttlefishContext.join(with: otirk) {error in
            XCTAssertNil(error, "error should be nil")
            joinWithInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithInheritanceKeyExpectation], timeout: 20)

        // Ensure CKKS has a read only policy
        XCTAssertNotNil(self.defaultCKKS.syncingPolicy, "Should have given CKKS a TPPolicy during initialization")
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.version, prevailingPolicyVersion, "Policy given to CKKS should be prevailing policy")
        XCTAssertTrue(self.defaultCKKS.syncingPolicy!.isInheritedAccount, "Syncing policy should be read only")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateInherited, within: 10 * NSEC_PER_SEC)
        self.wait(for: [trustedNotification], timeout: 10)

        let otOperationConfiguration = OTOperationConfiguration()
        self.cuttlefishContext.rpcTrustStatus(otOperationConfiguration) { status, _, _, _, _, _ in
            XCTAssertEqual(status, .in, "Self peer should be trusted")
        }

        let joinedPeerID = self.fetchEgoPeerID(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        // And check the current state of the world
        XCTAssertFalse(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: joinedPeerID, opinion: .trusts, target: joinedPeerID)),
                      "joined peer should not trust itself")
        XCTAssertFalse(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: joinedPeerID, opinion: .trusts, target: establishedPeerID)),
                      "joined peer should not trust establish peer")

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: establishedPeerID, opinion: .trusts, target: establishedPeerID)),
                      "establish peer should trust itself")
        XCTAssertFalse(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: establishedPeerID, opinion: .trusts, target: joinedPeerID)),
                      "establish peer should not trust joined peer")

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: irk.peerID, senderPeerID: joinedPeerID), "Should be no shares to the irk; it already has some")
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: joinedPeerID, senderPeerID: irk.peerID), "Should be no shares from a irk to a peer")
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: establishedPeerID, senderPeerID: irk.peerID), "Should be no shares from a irk to a peer")

        // let's ensure the inheritance bit is set.
        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName)
            XCTAssertTrue(accountState.isInheritedAccount, "isInheritedAccount should be YES")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        // now there is an IK set! Let's call reset and establish
        let resetAndEstablishExpectation = self.expectation(description: "resetAndEstablish callback occurs")

        self.cuttlefishContext.rpcResetAndEstablish(.testGenerated) { resetError in
            XCTAssertNil(resetError, "should be no error resetting and establishing")
            resetAndEstablishExpectation.fulfill()
        }
        self.wait(for: [resetAndEstablishExpectation], timeout: 10)

        _ = try self.cuttlefishContext.accountAvailable("13453464")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // now let's ensure the inheritance bit is gone.
        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName)
            XCTAssertFalse(accountState.isInheritedAccount, "isInheritedAccount should be NO")
        } catch {
            XCTFail("error loading account state: \(error)")
        }
    }

    func testAccountNoLongerAvailableClearsInheritanceBit() throws {
        let contextName = OTDefaultContext
        let containerName = OTCKContainerName

        self.manager.setSOSEnabledForPlatformFlag(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = self.mockAuthKit2.altDSID!
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        let establishedPeerID = self.fetchEgoPeerID(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        self.manager.setSOSEnabledForPlatformFlag(true)

        let (otirk, irk) = try self.createAndSetInheritanceRecoveryKey(context: establishContext)

        self.putInheritanceTLKSharesInCloudKit(irk: irk)
        self.sendContainerChangeWaitForFetch(context: establishContext)

        // Now, join from a new device
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: self.cuttlefishContext)

        let trustedNotification = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: "com.apple.security.octagon.trust-status-change"))
        let joinWithInheritanceKeyExpectation = self.expectation(description: "joinWithInheritanceKey callback occurs")
        self.cuttlefishContext.join(with: otirk) {error in
            XCTAssertNil(error, "error should be nil")
            joinWithInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithInheritanceKeyExpectation], timeout: 20)

        // Ensure CKKS has a read only policy
        XCTAssertNotNil(self.defaultCKKS.syncingPolicy, "Should have given CKKS a TPPolicy during initialization")
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.version, prevailingPolicyVersion, "Policy given to CKKS should be prevailing policy")
        XCTAssertTrue(self.defaultCKKS.syncingPolicy!.isInheritedAccount, "Syncing policy should be read only")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateInherited, within: 10 * NSEC_PER_SEC)
        self.wait(for: [trustedNotification], timeout: 10)

        let otOperationConfiguration = OTOperationConfiguration()
        self.cuttlefishContext.rpcTrustStatus(otOperationConfiguration) { status, _, _, _, _, _ in
            XCTAssertEqual(status, .in, "Self peer should be trusted")
        }

        let joinedPeerID = self.fetchEgoPeerID(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        // And check the current state of the world
        XCTAssertFalse(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: joinedPeerID, opinion: .trusts, target: joinedPeerID)),
                      "joined peer should not trust itself")
        XCTAssertFalse(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: joinedPeerID, opinion: .trusts, target: establishedPeerID)),
                      "joined peer should not trust establish peer")

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: establishedPeerID, opinion: .trusts, target: establishedPeerID)),
                      "establish peer should trust itself")
        XCTAssertFalse(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: establishedPeerID, opinion: .trusts, target: joinedPeerID)),
                      "establish peer should not trust joined peer")

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: irk.peerID, senderPeerID: joinedPeerID), "Should be no shares to the irk; it already has some")
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: joinedPeerID, senderPeerID: irk.peerID), "Should be no shares from a irk to a peer")
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: establishedPeerID, senderPeerID: irk.peerID), "Should be no shares from a irk to a peer")

        // let's ensure the inheritance bit is set.
        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName)
            XCTAssertTrue(accountState.isInheritedAccount, "isInheritedAccount should be YES")
        } catch {
            XCTFail("error loading account state: \(error)")
        }

        XCTAssertNoThrow(try self.cuttlefishContext.accountNoLongerAvailable(), "sign-out shouldn't error")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)
        self.assertNoAccount(context: self.cuttlefishContext)
        XCTAssertEqual(self.fetchCDPStatus(context: self.cuttlefishContext), .unknown, "CDP status should be 'unknown'")

        // now let's ensure the inheritance bit is gone.
        do {
            let accountState = try OTAccountMetadataClassC.loadFromKeychain(forContainer: containerName, contextID: contextName)
            XCTAssertFalse(accountState.isInheritedAccount, "isInheritedAccount should be NO")
        } catch {
            XCTFail("error loading account state: \(error)")
        }
    }

    func testJoinWithInheritanceKeyWithCKKSConflict() throws {
        self.startCKAccountStatusMock()

        let remote = self.createEstablishContext(contextID: "remote")
        self.assertResetAndBecomeTrusted(context: remote)

#if os(tvOS) || os(watchOS)
        self.manager.setSOSEnabledForPlatformFlag(true)
        let (otirk, irk) = try self.createAndSetInheritanceRecoveryKey(context: remote)
        self.manager.setSOSEnabledForPlatformFlag(false)
#else
        let (otirk, irk) = try self.createAndSetInheritanceRecoveryKey(context: remote)
#endif
        self.sendContainerChangeWaitForFetch(context: remote)

        self.silentFetchesAllowed = false
        self.expectCKFetchAndRun {
            self.putFakeKeyHierarchiesInCloudKit()
            self.putFakeDeviceStatusesInCloudKit()
            self.silentFetchesAllowed = true
        }

        let preflightJoinWithInheritanceKeyExpectation = self.expectation(description: "preflightJoinWithInheritanceKey callback occurs")
        remote.preflightJoin(with: otirk) { error in
            XCTAssertNil(error, "error should be nil")
            preflightJoinWithInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [preflightJoinWithInheritanceKeyExpectation], timeout: 20)

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: self.cuttlefishContext)

        let joinWithInheritanceKeyExpectation = self.expectation(description: "joinWithInheritanceKey callback occurs")
        self.cuttlefishContext.join(with: otirk) {error in
            XCTAssertNil(error, "error should be nil")
            joinWithInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithInheritanceKeyExpectation], timeout: 20)

        XCTAssertEqual(self.cuttlefishContext.currentMemoizedTrustState(), .TRUSTED, "Trust state should be trusted")

        let recoveryContextPeerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        let remoteContextPeerID = try remote.accountMetadataStore.getEgoPeerID()

        XCTAssertFalse(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: recoveryContextPeerID, opinion: .trusts, target: remoteContextPeerID)),
                      "joined peer should not trust the remote peer")
        XCTAssertFalse(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: recoveryContextPeerID, opinion: .trusts, target: irk.peerID)),
                      "joined peer should not trust inheritance peer ID")

        // WaitForTLK means no shares for anyone
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: recoveryContextPeerID, senderPeerID: recoveryContextPeerID))
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: irk.peerID, senderPeerID: recoveryContextPeerID))
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: remoteContextPeerID, senderPeerID: recoveryContextPeerID))
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: irk.peerID, senderPeerID: recoveryContextPeerID))

        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: recoveryContextPeerID, senderPeerID: irk.peerID), "Should be no shares from the irk to any context")
    }

    func testirkRecoveryRecoversCKKSCreatedShares() throws {
        self.startCKAccountStatusMock()

        let remote = self.createEstablishContext(contextID: "remote")
        self.assertResetAndBecomeTrusted(context: remote)

#if os(tvOS) || os(watchOS)
        self.manager.setSOSEnabledForPlatformFlag(true)
        let (otirk, irk) = try self.createAndSetInheritanceRecoveryKey(context: remote)
        self.manager.setSOSEnabledForPlatformFlag(false)
#else
        let (otirk, irk) = try self.createAndSetInheritanceRecoveryKey(context: remote)
#endif

        // And TLKShares for the RK are sent from the Octagon peer
        self.putFakeKeyHierarchiesInCloudKit()
        self.putFakeDeviceStatusesInCloudKit()

        try self.putSelfTLKSharesInCloudKit(context: remote)
        try self.putInheritanceTLKSharesInCloudKit(irk: irk, sender: remote)
        XCTAssertTrue(try self.inheritanceTLKSharesInCloudKit(irk: irk, sender: remote))

        let preflightJoinWithInheritanceKeyExpectation = self.expectation(description: "preflightJoinWithInheritanceKey callback occurs")
        remote.preflightJoin(with: otirk) { error in
            XCTAssertNil(error, "error should be nil")
            preflightJoinWithInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [preflightJoinWithInheritanceKeyExpectation], timeout: 20)

        // Now, join! This should recover the TLKs.
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let joinWithInheritanceKeyExpectation = self.expectation(description: "joinWithInheritanceKey callback occurs")
        self.cuttlefishContext.join(with: otirk) {error in
            XCTAssertNil(error, "error should be nil")
            joinWithInheritanceKeyExpectation.fulfill()
        }

        self.wait(for: [joinWithInheritanceKeyExpectation], timeout: 20)

        // Ensure CKKS has a read only policy
        XCTAssertNotNil(self.defaultCKKS.syncingPolicy, "Should have given CKKS a TPPolicy during initialization")
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.version, prevailingPolicyVersion, "Policy given to CKKS should be prevailing policy")
        XCTAssertTrue(self.defaultCKKS.syncingPolicy!.isInheritedAccount, "Syncing policy should be read only")

        XCTAssertEqual(self.cuttlefishContext.currentMemoizedTrustState(), .TRUSTED, "Trust state should be trusted")
        self.verifyDatabaseMocks()
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        let joinedPeerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        let remotePeerID = try remote.accountMetadataStore.getEgoPeerID()

        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: joinedPeerID, senderPeerID: remotePeerID), "Remote peer isn't running CKKS in tests; should not send us shares")
        XCTAssertFalse(try self.inheritanceTLKSharesInCloudKit(irk: irk, sender: self.cuttlefishContext), "Joined peer should not send new TLKShares to irk")

        self.assertSelfTLKSharesNotInCloudKit(peerID: joinedPeerID)
    }

    func testRecoverTLKSharesSentToirkBeforeCKKSFetchCompletes() throws {
        self.manager.setSOSEnabledForPlatformFlag(false)
        self.startCKAccountStatusMock()

        let remote = self.createEstablishContext(contextID: "remote")
        self.assertResetAndBecomeTrusted(context: remote)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: remote)
        self.assertSelfTLKSharesInCloudKit(context: remote)

        self.manager.setSOSEnabledForPlatformFlag(true)
        let (otirk, irk) = try self.createAndSetInheritanceRecoveryKey(context: remote)

        self.putInheritanceTLKSharesInCloudKit(irk: irk)

        let preflightJoinWithInheritanceKeyExpectation = self.expectation(description: "preflightJoinWithInheritanceKey callback occurs")
        remote.preflightJoin(with: otirk) { error in
            XCTAssertNil(error, "error should be nil")
            preflightJoinWithInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [preflightJoinWithInheritanceKeyExpectation], timeout: 20)

        // Now, join from a new device
        // Simulate CKKS fetches taking forever. In practice, this is caused by many round-trip fetches to CK happening over minutes.
        self.holdCloudKitFetches()

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // device succeeds joining after restore
        self.mockSOSAdapter.joinAfterRestoreResult = true
        // reset to offering on the mock adapter is by default set to false so this should cause cascading failures resulting in a cfu
        self.mockSOSAdapter.joinAfterRestoreCircleStatusOverride = true
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCRequestPending)
        self.mockSOSAdapter.resetToOfferingCircleStatusOverride = true

        let joinWithInheritanceKeyExpectation = self.expectation(description: "joinWithInheritanceKey callback occurs")
        self.cuttlefishContext.join(with: otirk) {error in
            XCTAssertNil(error, "error should be nil")
            joinWithInheritanceKeyExpectation.fulfill()
        }

        self.wait(for: [joinWithInheritanceKeyExpectation], timeout: 20)

        // Ensure CKKS has a read only policy
        XCTAssertNotNil(self.defaultCKKS.syncingPolicy, "Should have given CKKS a TPPolicy during initialization")
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.version, prevailingPolicyVersion, "Policy given to CKKS should be prevailing policy")
        XCTAssertTrue(self.defaultCKKS.syncingPolicy!.isInheritedAccount, "Syncing policy should be read only")

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateFetch, within: 10 * NSEC_PER_SEC)

        // When Octagon is creating itself TLKShares as part of the escrow recovery, CKKS will get into the right state without any uploads

        self.releaseCloudKitFetchHold()
        self.verifyDatabaseMocks()

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        let remotePeerID = try remote.accountMetadataStore.getEgoPeerID()
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: irk.peerID, senderPeerID: remotePeerID), "Should be no shares from peer to irk; as irk has self-shares")
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: remotePeerID, senderPeerID: irk.peerID), "Should be no shares from irk to peer")
        XCTAssertEqual(self.mockSOSAdapter.circleStatus, SOSCCStatus(kSOSCCRequestPending), "SOS should be Request Pending")
    }

    func testAddInheritanceKey() throws {
        self.startCKAccountStatusMock()

        self.putFakeKeyHierarchiesInCloudKit()
        self.silentZoneDeletesAllowed = true

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(self.mockAuthKit.currentDeviceList().isEmpty, "should not have zero devices")

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(self.cuttlefishContext.currentMemoizedTrustState(), .TRUSTED, "Trust state should be trusted")
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        self.manager.setSOSEnabledForPlatformFlag(true)

        let createInheritanceKeyExpectation = self.expectation(description: "createInheritanceKeyExpectation returns")
        self.manager.createInheritanceKey(OTCKContainerName, contextID: self.otcliqueContext.context, uuid: UUID()) { irk, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNotNil(irk, "irk should be non-nil")
            XCTAssertNotNil(irk?.uuid, "uuid should be non-nil")
            createInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [createInheritanceKeyExpectation], timeout: 10)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func testAddInheritanceKeyAndCKKSTLKSharesHappen() throws {
#if os(tvOS) || os(watchOS)
        self.startCKAccountStatusMock()
        throw XCTSkip("Apple TVs and watches will not set recovery key")
#else

        self.startCKAccountStatusMock()

        // To get into a state where we don't upload the TLKShares to each RK on RK creation, put Octagon into a waitfortlk state
        // Right after CKKS fetches for the first time, insert a new key hierarchy into CloudKit
        self.silentFetchesAllowed = false
        self.expectCKFetchAndRun {
            self.putFakeKeyHierarchiesInCloudKit()
            self.putFakeDeviceStatusesInCloudKit()
            self.silentFetchesAllowed = true
        }

        do {
            let clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
        }

        // Now, we should be in 'ready'
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(self.cuttlefishContext.currentMemoizedTrustState(), .TRUSTED, "Trust state should be trusted")

        // and all subCKKSes should enter waitfortlk, as they don't have the TLKs uploaded by the other peer
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLK, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // And a inheritance key is set
        var retirk: OTInheritanceKey?
        let createInheritanceKeyExpectation = self.expectation(description: "createInheritanceKeyExpectation returns")
        self.manager.createInheritanceKey(OTCKContainerName, contextID: self.otcliqueContext.context, uuid: nil) { irk, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNotNil(irk, "irk should be non-nil")
            XCTAssertNotNil(irk?.uuid, "uuid should be non-nil")
            retirk = irk
            createInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [createInheritanceKeyExpectation], timeout: 10)

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLK, within: 10 * NSEC_PER_SEC)

        // and now, all TLKs arrive! CKKS should upload two shares: one for itself, and one for the inheritance key
        self.assertAllCKKSViewsUpload(tlkShares: 2)
        self.saveTLKMaterialToKeychain()

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        let container = try self.tphClient.getContainer(withContainer: self.cuttlefishContext.containerName, context: self.otcliqueContext.context)
        let inheritor = try XCTUnwrap(container.model.findCustodianRecoveryKey(with: retirk!.uuid), "Should be able to find the irk we just created")
        let cuttlefishContextPeerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        self.assertTLKSharesInCloudKit(receiverPeerID: inheritor.peerID, senderPeerID: cuttlefishContextPeerID)
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: cuttlefishContextPeerID, senderPeerID: inheritor.peerID), "irk should not create TLKShares to existing peers")
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: inheritor.peerID, senderPeerID: inheritor.peerID), "Should be no shares from the irk to itself")
        self.assertSelfTLKSharesNotInCloudKit(peerID: inheritor.peerID)

#endif // tvOS || watchOS
    }

    func testAddInheritanceKeyUUID() throws {

        self.startCKAccountStatusMock()

        self.putFakeKeyHierarchiesInCloudKit()
        self.silentZoneDeletesAllowed = true

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(self.mockAuthKit.currentDeviceList().isEmpty, "should not have zero devices")

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(self.cuttlefishContext.currentMemoizedTrustState(), .TRUSTED, "Trust state should be trusted")
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)

        self.manager.setSOSEnabledForPlatformFlag(true)

        let uuid = UUID()

        let createInheritanceKeyExpectation = self.expectation(description: "createInheritanceKeyExpectation returns")
        self.manager.createInheritanceKey(OTCKContainerName, contextID: self.otcliqueContext.context, uuid: uuid) { irk, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNotNil(irk, "irk should be non-nil")
            XCTAssertNotNil(irk?.uuid, "uuid should be non-nil")
            createInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [createInheritanceKeyExpectation], timeout: 10)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func testInheritanceKeyTrust() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(self.mockAuthKit.currentDeviceList().isEmpty, "should not have zero devices")

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        XCTAssertEqual(self.cuttlefishContext.currentMemoizedTrustState(), .TRUSTED, "Trust state should be trusted")
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)

        self.manager.setSOSEnabledForPlatformFlag(true)

        let uuid = UUID()

        let createInheritanceKeyExpectation = self.expectation(description: "createInheritanceKeyExpectation returns")
        self.manager.createInheritanceKey(OTCKContainerName, contextID: self.otcliqueContext.context, uuid: uuid) { irk, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNotNil(irk, "irk should be non-nil")
            XCTAssertNotNil(irk?.uuid, "uuid should be non-nil")
            createInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [createInheritanceKeyExpectation], timeout: 10)

        self.assertTrusts(context: self.cuttlefishContext, includedPeerIDCount: 2, excludedPeerIDCount: 0)
        XCTAssertEqual(self.cuttlefishContext.currentMemoizedTrustState(), .TRUSTED, "Trust state should be trusted")
        self.verifyDatabaseMocks()
    }

    func testJoinWithInheritanceKey() throws {
        self.manager.setSOSEnabledForPlatformFlag(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = self.mockAuthKit2.altDSID!
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        let establishedPeerID = self.fetchEgoPeerID(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        self.manager.setSOSEnabledForPlatformFlag(true)

        let (otirk, irk) = try self.createAndSetInheritanceRecoveryKey(context: establishContext)

        self.putInheritanceTLKSharesInCloudKit(irk: irk)
        self.sendContainerChangeWaitForFetch(context: establishContext)

        let preflightJoinWithInheritanceKeyExpectation = self.expectation(description: "preflightJoinWithInheritanceKey callback occurs")
        establishContext.preflightJoin(with: otirk) { error in
            XCTAssertNil(error, "error should be nil")
            preflightJoinWithInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [preflightJoinWithInheritanceKeyExpectation], timeout: 20)

        // Now, join from a new device
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: self.cuttlefishContext)

        let trustedNotification = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: "com.apple.security.octagon.trust-status-change"))
        let joinWithInheritanceKeyExpectation = self.expectation(description: "joinWithInheritanceKey callback occurs")
        self.cuttlefishContext.join(with: otirk) {error in
            XCTAssertNil(error, "error should be nil")
            joinWithInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithInheritanceKeyExpectation], timeout: 20)

        // Ensure CKKS has a read only policy
        XCTAssertNotNil(self.defaultCKKS.syncingPolicy, "Should have given CKKS a TPPolicy during initialization")
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.version, prevailingPolicyVersion, "Policy given to CKKS should be prevailing policy")
        XCTAssertTrue(self.defaultCKKS.syncingPolicy!.isInheritedAccount, "Syncing policy should be read only")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateInherited, within: 10 * NSEC_PER_SEC)
        self.wait(for: [trustedNotification], timeout: 10)

        let otOperationConfiguration = OTOperationConfiguration()
        self.cuttlefishContext.rpcTrustStatus(otOperationConfiguration) { status, _, _, _, _, _ in
            XCTAssertEqual(status, .in, "Self peer should be trusted")
        }

        let joinedPeerID = self.fetchEgoPeerID(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        // And check the current state of the world
        XCTAssertFalse(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: joinedPeerID, opinion: .trusts, target: joinedPeerID)),
                      "joined peer should not trust itself")
        XCTAssertFalse(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: joinedPeerID, opinion: .trusts, target: establishedPeerID)),
                      "joined peer should not trust establish peer")

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: establishedPeerID, opinion: .trusts, target: establishedPeerID)),
                      "establish peer should trust itself")
        XCTAssertFalse(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: establishedPeerID, opinion: .trusts, target: joinedPeerID)),
                      "establish peer should not trust joined peer")

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: irk.peerID, senderPeerID: joinedPeerID), "Should be no shares to the irk; it already has some")
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: joinedPeerID, senderPeerID: irk.peerID), "Should be no shares from a irk to a peer")
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: establishedPeerID, senderPeerID: irk.peerID), "Should be no shares from a irk to a peer")
    }

    func testJoinWithInheritanceKeyAltdsid() throws {
        self.manager.setSOSEnabledForPlatformFlag(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = self.mockAuthKit2.altDSID!
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        let establishedPeerID = self.fetchEgoPeerID(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        self.manager.setSOSEnabledForPlatformFlag(true)

        let (otirk, irk) = try self.createAndSetInheritanceRecoveryKey(context: establishContext)

        self.putInheritanceTLKSharesInCloudKit(irk: irk)
        self.sendContainerChangeWaitForFetch(context: establishContext)

        let preflightJoinWithInheritanceKeyExpectation = self.expectation(description: "preflightJoinWithInheritanceKey callback occurs")
        establishContext.preflightJoin(with: otirk) { error in
            XCTAssertNil(error, "error should be nil")
            preflightJoinWithInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [preflightJoinWithInheritanceKeyExpectation], timeout: 20)

        // Now, join from a new device
        self.mockAuthKit.altDSID = UUID().uuidString
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: self.cuttlefishContext)

        let trustedNotification = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: "com.apple.security.octagon.trust-status-change"))
        let joinWithInheritanceKeyExpectation = self.expectation(description: "joinWithInheritanceKey callback occurs")
        self.cuttlefishContext.join(with: otirk) {error in
            XCTAssertNil(error, "error should be nil")
            joinWithInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithInheritanceKeyExpectation], timeout: 20)

        // Ensure CKKS has a read only policy
        XCTAssertNotNil(self.defaultCKKS.syncingPolicy, "Should have given CKKS a TPPolicy during initialization")
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.version, prevailingPolicyVersion, "Policy given to CKKS should be prevailing policy")
        XCTAssertTrue(self.defaultCKKS.syncingPolicy!.isInheritedAccount, "Syncing policy should be read only")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateInherited, within: 10 * NSEC_PER_SEC)
        self.wait(for: [trustedNotification], timeout: 10)

        let otOperationConfiguration = OTOperationConfiguration()
        self.cuttlefishContext.rpcTrustStatus(otOperationConfiguration) { status, _, _, _, _, _ in
            XCTAssertEqual(status, .in, "Self peer should be trusted")
        }

        let joinedPeerID = self.fetchEgoPeerID(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        // And check the current state of the world
        XCTAssertFalse(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: joinedPeerID, opinion: .trusts, target: joinedPeerID)),
                      "joined peer should not trust itself")
        XCTAssertFalse(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: joinedPeerID, opinion: .trusts, target: establishedPeerID)),
                      "joined peer should not trust establish peer")

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: establishedPeerID, opinion: .trusts, target: establishedPeerID)),
                      "establish peer should trust itself")
        XCTAssertFalse(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: establishedPeerID, opinion: .trusts, target: joinedPeerID)),
                      "establish peer should not trust joined peer")

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: irk.peerID, senderPeerID: joinedPeerID), "Should be no shares to the irk; it already has some")
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: joinedPeerID, senderPeerID: irk.peerID), "Should be no shares from a irk to a peer")
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: establishedPeerID, senderPeerID: irk.peerID), "Should be no shares from a irk to a peer")
    }

    func testJoinWithInheritanceKeyWithClique() throws {
        self.manager.setSOSEnabledForPlatformFlag(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = self.mockAuthKit2.altDSID!
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        let establishedPeerID = self.fetchEgoPeerID(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        self.manager.setSOSEnabledForPlatformFlag(true)

        let (otirk, irk) = try self.createAndSetInheritanceRecoveryKey(context: establishContext)

        self.putInheritanceTLKSharesInCloudKit(irk: irk)
        self.sendContainerChangeWaitForFetch(context: establishContext)

        // Now, join from a new device
        let newCliqueContext = OTConfigurationContext()
        newCliqueContext.context = OTDefaultContext
        newCliqueContext.dsid = self.otcliqueContext.dsid
        newCliqueContext.altDSID = self.mockAuthKit.altDSID!
        newCliqueContext.otControl = self.otControl

        let recoveryContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        recoveryContext.startOctagonStateMachine()
        self.assertEnters(context: recoveryContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: recoveryContext)

        let joinWithInheritanceKeyExpectation = self.expectation(description: "joinWithInheritanceKey callback occurs")
        OTClique.recoverOctagon(usingInheritanceKey: newCliqueContext, inheritanceKey: otirk) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithInheritanceKeyExpectation], timeout: 20)

        // Ensure CKKS has a read only policy
        XCTAssertNotNil(self.defaultCKKS.syncingPolicy, "Should have given CKKS a TPPolicy during initialization")
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.version, prevailingPolicyVersion, "Policy given to CKKS should be prevailing policy")
        XCTAssertTrue(self.defaultCKKS.syncingPolicy!.isInheritedAccount, "Syncing policy should be read only")

        let joinedPeerID = self.fetchEgoPeerID(context: recoveryContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        // And check the current state of the world
        XCTAssertFalse(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: joinedPeerID, opinion: .trusts, target: joinedPeerID)),
                      "joined peer should not trust itself")
        XCTAssertFalse(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: joinedPeerID, opinion: .trusts, target: establishedPeerID)),
                      "joined peer should not trust establish peer")

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: establishedPeerID, opinion: .trusts, target: establishedPeerID)),
                      "establish peer should trust itself")
        XCTAssertFalse(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: establishedPeerID, opinion: .trusts, target: joinedPeerID)),
                      "establish peer should not trust joined peer")

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: joinedPeerID, opinion: .trusts, target: irk.peerID)),
                      "establish peer should not trust joined peer")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: establishedPeerID, opinion: .trusts, target: irk.peerID)),
                      "establish peer should not trust joined peer")

        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: irk.peerID, senderPeerID: joinedPeerID), "Should be no shares to the irk; it already has some")
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: joinedPeerID, senderPeerID: irk.peerID), "Should be no shares from a irk to a peer")
        XCTAssertFalse(self.tlkSharesInCloudKit(receiverPeerID: establishedPeerID, senderPeerID: irk.peerID), "Should be no shares from a irk to a peer")
    }

    func testJoinWithInheritanceRecoveryKeyBadUUID() throws {
        self.manager.setSOSEnabledForPlatformFlag(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = self.mockAuthKit2.altDSID!
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        self.manager.setSOSEnabledForPlatformFlag(true)

        var retirk: OTInheritanceKey?
        let createInheritanceKeyExpectation = self.expectation(description: "createInheritanceKeyExpectation returns")
        let uuid = UUID()
        self.manager.createInheritanceKey(OTCKContainerName, contextID: bottlerotcliqueContext.context, uuid: uuid) { irk, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNotNil(irk, "irk should be non-nil")
            XCTAssertNotNil(irk?.uuid, "uuid should be non-nil")
            retirk = irk
            createInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [createInheritanceKeyExpectation], timeout: 10)
        let recoveryKeyData = retirk!.recoveryKeyData

        let container = try self.tphClient.getContainer(withContainer: OTCKContainerName, context: bottlerotcliqueContext.context)
        let custodian = try XCTUnwrap(container.model.findCustodianRecoveryKey(with: retirk!.uuid))

        let ikWithKeys = try InheritanceKey(tpInheritance: custodian, recoveryKeyData: recoveryKeyData, recoverySalt: "")

        try self.putInheritanceTLKSharesInCloudKit(irk: ikWithKeys, sender: establishContext)
        self.sendContainerChangeWaitForFetch(context: establishContext)

        let newUUID = UUID()
        let irk2 = try OTInheritanceKey(uuid: newUUID)

        let preflightJoinWithInheritanceKeyExpectation = self.expectation(description: "preflightJoinWithInheritanceKey callback occurs")
        establishContext.preflightJoin(with: irk2) { error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual("com.apple.security.trustedpeers.container", (error! as NSError).domain, "error domain mismatch")
            XCTAssertEqual((error! as NSError).code, ContainerError.recoveryKeysNotEnrolled.errorCode, "error code mismatch")
            preflightJoinWithInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [preflightJoinWithInheritanceKeyExpectation], timeout: 20)

        // Now, join from a new device
        let recoveryContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        recoveryContext.startOctagonStateMachine()
        self.assertEnters(context: recoveryContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: recoveryContext)

        let joinWithInheritanceKeyExpectation = self.expectation(description: "joinWithInheritanceKey callback occurs")

        recoveryContext.join(with: irk2) { error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual("com.apple.security.trustedpeers.container", (error! as NSError).domain, "error domain mismatch")
            XCTAssertEqual((error! as NSError).code, ContainerError.recoveryKeysNotEnrolled.errorCode, "error code mismatch")
            joinWithInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithInheritanceKeyExpectation], timeout: 20)

        // Ensure CKKS has a read only policy
        XCTAssertNotNil(self.defaultCKKS.syncingPolicy, "Should have given CKKS a TPPolicy during initialization")
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.version, prevailingPolicyVersion, "Policy given to CKKS should be prevailing policy")

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTrust, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateWaitForTrust, within: 10 * NSEC_PER_SEC)
    }

    func testJoinWithInheritanceKeyBadKey() throws {
        self.manager.setSOSEnabledForPlatformFlag(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = self.mockAuthKit2.altDSID!
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        self.manager.setSOSEnabledForPlatformFlag(true)

        var retirk: OTInheritanceKey?
        let createInheritanceKeyExpectation = self.expectation(description: "createInheritanceKeyExpectation returns")
        let uuid = UUID()
        self.manager.createInheritanceKey(OTCKContainerName, contextID: bottlerotcliqueContext.context, uuid: uuid) { irk, error in
            XCTAssertNil(error, "error should be nil")
            XCTAssertNotNil(irk, "irk should be non-nil")
            XCTAssertNotNil(irk?.uuid, "uuid should be non-nil")
            retirk = irk
            createInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [createInheritanceKeyExpectation], timeout: 10)
        let recoveryKeyData = retirk!.recoveryKeyData

        let container = try self.tphClient.getContainer(withContainer: self.cuttlefishContext.containerName, context: bottlerotcliqueContext.context)
        let custodian = try XCTUnwrap(container.model.findCustodianRecoveryKey(with: retirk!.uuid))

        let crkWithKeys = try InheritanceKey(tpInheritance: custodian, recoveryKeyData: recoveryKeyData, recoverySalt: "")

        try self.putInheritanceTLKSharesInCloudKit(irk: crkWithKeys, sender: establishContext)
        self.sendContainerChangeWaitForFetch(context: establishContext)

        let anotherRecoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(anotherRecoveryKey, "SecKCreateRecoveryKeyString failed")
        let irk2 = try OTInheritanceKey(uuid: retirk!.uuid)

        let preflightJoinWithInheritanceKeyExpectation = self.expectation(description: "preflightJoinWithInheritanceKey callback occurs")
        establishContext.preflightJoin(with: irk2) { error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual("com.apple.security.trustedpeers.container", (error! as NSError).domain, "error domain mismatch")
            XCTAssertEqual((error! as NSError).code, ContainerError.failedToCreateRecoveryKey.errorCode, "error code mismatch")
            preflightJoinWithInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [preflightJoinWithInheritanceKeyExpectation], timeout: 20)
        // Now, join from a new device
        let recoveryContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        recoveryContext.startOctagonStateMachine()
        self.assertEnters(context: recoveryContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: recoveryContext)

        let joinWithInheritanceKeyExpectation = self.expectation(description: "joinWithInheritanceKeyExpectation callback occurs")

        recoveryContext.join(with: irk2) { error in
            XCTAssertNotNil(error, "error should not be nil")
            XCTAssertEqual("com.apple.security.trustedpeers.container", (error! as NSError).domain, "error domain mismatch")
            XCTAssertEqual((error! as NSError).code, ContainerError.failedToCreateRecoveryKey.errorCode, "error code mismatch")
            joinWithInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithInheritanceKeyExpectation], timeout: 20)

        // Ensure CKKS has a read only policy
        XCTAssertNotNil(self.defaultCKKS.syncingPolicy, "Should have given CKKS a TPPolicy during initialization")
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.version, prevailingPolicyVersion, "Policy given to CKKS should be prevailing policy")

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTrust, within: 10 * NSEC_PER_SEC)
        self.assertCKKSStateMachine(enters: CKKSStateWaitForTrust, within: 10 * NSEC_PER_SEC)
    }

    func testJoinWithInheritanceKeyThenLoadToInheritedOnRestart() throws {
        let startDate = Date()

        self.manager.setSOSEnabledForPlatformFlag(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = self.mockAuthKit2.altDSID!
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        self.manager.setSOSEnabledForPlatformFlag(true)

        let (otirk, irk) = try self.createAndSetInheritanceRecoveryKey(context: establishContext)

        self.putInheritanceTLKSharesInCloudKit(irk: irk)
        self.sendContainerChangeWaitForFetch(context: establishContext)

        let preflightJoinWithInheritanceKeyExpectation = self.expectation(description: "preflightJoinWithInheritanceKey callback occurs")
        establishContext.preflightJoin(with: otirk) { error in
            XCTAssertNil(error, "error should be nil")
            preflightJoinWithInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [preflightJoinWithInheritanceKeyExpectation], timeout: 20)

        // Now, join from a new device
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: self.cuttlefishContext)

        let trustedNotification = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: "com.apple.security.octagon.trust-status-change"))
        let joinWithInheritanceKeyExpectation = self.expectation(description: "joinWithInheritanceKey callback occurs")
        self.cuttlefishContext.join(with: otirk) {error in
            XCTAssertNil(error, "error should be nil")
            joinWithInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithInheritanceKeyExpectation], timeout: 20)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateInherited, within: 10 * NSEC_PER_SEC)
        self.wait(for: [trustedNotification], timeout: 10)

        let otOperationConfiguration = OTOperationConfiguration()
        self.cuttlefishContext.rpcTrustStatus(otOperationConfiguration) { status, _, _, _, _, _ in
            XCTAssertEqual(status, .in, "Self peer should be trusted")
        }

        // Ensure CKKS has a read only policy
        XCTAssertNotNil(self.defaultCKKS.syncingPolicy, "Should have given CKKS a TPPolicy during initialization")
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.version, prevailingPolicyVersion, "Policy given to CKKS should be prevailing policy")
        XCTAssertTrue(self.defaultCKKS.syncingPolicy!.isInheritedAccount, "Syncing policy should be read only")

        let peerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        XCTAssertNotNil(peerID, "Should have a peer ID after inheritance join")

        var readyDate = CKKSAnalytics.logger().dateProperty(forKey: OctagonAnalyticsLastKeystateReady)
        XCTAssertNotNil(readyDate, "Should have a ready date")
        XCTAssert(try XCTUnwrap(readyDate) > startDate, "ready date should be after startdate")

        // Now restart the context
        self.manager.removeContext(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.restartCKKSViews()
        self.cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        XCTAssertNil(self.defaultCKKS.syncingPolicy, "CKKS should not have a policy after 'restart'")

        let trustedNotification2 = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: "com.apple.security.octagon.trust-status-change"))
        let restartDate = Date()
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateInherited, within: 10 * NSEC_PER_SEC)
        self.wait(for: [trustedNotification2], timeout: 10)

        XCTAssertEqual(self.cuttlefishContext.currentMemoizedTrustState(), .TRUSTED, "Trust state should be trusted")

        self.cuttlefishContext.rpcTrustStatus(otOperationConfiguration) { status, _, _, _, _, _ in
            XCTAssertEqual(status, .in, "Self peer should be trusted")
        }

        let restartedPeerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        XCTAssertNotNil(restartedPeerID, "Should have a peer ID after restarting")

        XCTAssertEqual(peerID, restartedPeerID, "Should have the same peer ID after restarting")
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertNotNil(self.defaultCKKS.syncingPolicy, "Should have given CKKS a TPPolicy after restart")
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.version, prevailingPolicyVersion, "Policy given to CKKS after restart should be prevailing policy")
        XCTAssertTrue(self.defaultCKKS.syncingPolicy!.isInheritedAccount, "Syncing policy should be read only")

        readyDate = CKKSAnalytics.logger().dateProperty(forKey: OctagonAnalyticsLastKeystateReady)
        XCTAssertNotNil(readyDate, "Should have a ready date")
        XCTAssert(try XCTUnwrap(readyDate) > restartDate, "ready date should be after re-startdate")
    }

    func testJoinWithInheritanceNoCKKSWrites() throws {
        self.manager.setSOSEnabledForPlatformFlag(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = self.mockAuthKit2.altDSID!
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        self.manager.setSOSEnabledForPlatformFlag(true)

        let (otirk, irk) = try self.createAndSetInheritanceRecoveryKey(context: establishContext)

        self.putInheritanceTLKSharesInCloudKit(irk: irk)
        self.sendContainerChangeWaitForFetch(context: establishContext)

        let preflightJoinWithInheritanceKeyExpectation = self.expectation(description: "preflightJoinWithInheritanceKey callback occurs")
        establishContext.preflightJoin(with: otirk) { error in
            XCTAssertNil(error, "error should be nil")
            preflightJoinWithInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [preflightJoinWithInheritanceKeyExpectation], timeout: 20)

        // Now, join from a new device
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: self.cuttlefishContext)

        let trustedNotification = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: "com.apple.security.octagon.trust-status-change"))
        let joinWithInheritanceKeyExpectation = self.expectation(description: "joinWithInheritanceKey callback occurs")
        self.cuttlefishContext.join(with: otirk) {error in
            XCTAssertNil(error, "error should be nil")
            joinWithInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithInheritanceKeyExpectation], timeout: 20)

        // Ensure CKKS has a read only policy
        XCTAssertNotNil(self.defaultCKKS.syncingPolicy, "Should have given CKKS a TPPolicy during initialization")
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.version, prevailingPolicyVersion, "Policy given to CKKS should be prevailing policy")
        XCTAssertTrue(self.defaultCKKS.syncingPolicy!.isInheritedAccount, "Syncing policy should be read only")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateInherited, within: 10 * NSEC_PER_SEC)
        self.wait(for: [trustedNotification], timeout: 10)

        let otOperationConfiguration = OTOperationConfiguration()
        self.cuttlefishContext.rpcTrustStatus(otOperationConfiguration) { status, _, _, _, _, _ in
            XCTAssertEqual(status, .in, "Self peer should be trusted")
        }

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        // Ensure the test machinery sees the transitions
        self.defaultCKKS.stateMachine.testPause(afterEntering: CKKSStateProcessOutgoingQueue)
        self.addGenericPassword("asdf", account: "account-delete-me", viewHint: "LimitedPeersAllowed")
        self.assertCKKSStateMachine(enters: CKKSStateProcessOutgoingQueue, within: 10 * NSEC_PER_SEC)
        self.defaultCKKS.stateMachine.testReleasePause(CKKSStateProcessOutgoingQueue)

        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)

        self.verifyDatabaseMocks()
    }

    func putLimitedPeersAllowedItemsInCloudKit() throws {
        let ckr = self.createFakeRecord(self.limitedPeersAllowedZoneID, recordName: "7B598D31-F9C5-481E-98AC-5A507ACB2D85")
        let limitedPeersAllowedID = self.zones![self.limitedPeersAllowedZoneID!] as! FakeCKZone
        limitedPeersAllowedID.add(toZone: ckr)
    }

    func testJoinWithInheritanceDownloadCKKSItemsAndDoesntWrite() throws {
        self.manager.setSOSEnabledForPlatformFlag(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = self.mockAuthKit2.altDSID!
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putLimitedPeersAllowedItemsInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        self.manager.setSOSEnabledForPlatformFlag(true)

        let (otirk, irk) = try self.createAndSetInheritanceRecoveryKey(context: establishContext)

        self.putInheritanceTLKSharesInCloudKit(irk: irk)
        self.sendContainerChangeWaitForFetch(context: establishContext)

        let preflightJoinWithInheritanceKeyExpectation = self.expectation(description: "preflightJoinWithInheritanceKey callback occurs")
        establishContext.preflightJoin(with: otirk) { error in
            XCTAssertNil(error, "error should be nil")
            preflightJoinWithInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [preflightJoinWithInheritanceKeyExpectation], timeout: 20)

        // Now, join from a new device
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: self.cuttlefishContext)

        let trustedNotification = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: "com.apple.security.octagon.trust-status-change"))
        let joinWithInheritanceKeyExpectation = self.expectation(description: "joinWithInheritanceKey callback occurs")
        self.cuttlefishContext.join(with: otirk) {error in
            XCTAssertNil(error, "error should be nil")
            joinWithInheritanceKeyExpectation.fulfill()
        }

        self.wait(for: [joinWithInheritanceKeyExpectation], timeout: 20)

        // Ensure CKKS has a read only policy
        XCTAssertNotNil(self.defaultCKKS.syncingPolicy, "Should have given CKKS a TPPolicy during initialization")
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.version, prevailingPolicyVersion, "Policy given to CKKS should be prevailing policy")
        XCTAssertTrue(self.defaultCKKS.syncingPolicy!.isInheritedAccount, "Syncing policy should be read only")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateInherited, within: 10 * NSEC_PER_SEC)
        self.wait(for: [trustedNotification], timeout: 10)

        let otOperationConfiguration = OTOperationConfiguration()
        self.cuttlefishContext.rpcTrustStatus(otOperationConfiguration) { status, _, _, _, _, _ in
            XCTAssertEqual(status, .in, "Self peer should be trusted")
        }

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        // Ensure the test machinery sees the transitions
        self.defaultCKKS.stateMachine.testPause(afterEntering: CKKSStateProcessOutgoingQueue)
        self.addGenericPassword("fail", account: "failed-attempt-to-add-account", viewHint: "LimitedPeersAllowed")
        self.assertCKKSStateMachine(enters: CKKSStateProcessOutgoingQueue, within: 10 * NSEC_PER_SEC)
        self.defaultCKKS.stateMachine.testReleasePause(CKKSStateProcessOutgoingQueue)

        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)

        self.findGenericPassword("account-delete-me", expecting: errSecSuccess)

        self.verifyDatabaseMocks()
    }

    func testSetUserControllableViewsInheritedAccount() throws {

        self.manager.setSOSEnabledForPlatformFlag(false)
        self.startCKAccountStatusMock()

        let establishContextID = "establish-context-id"
        let establishContext = self.createEstablishContext(contextID: establishContextID)

        establishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try establishContext.setCDPEnabled())
        self.assertEnters(context: establishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = establishContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = self.mockAuthKit2.altDSID!
        bottlerotcliqueContext.otControl = self.otControl
        do {
            clique = try OTClique.newFriends(withContextData: bottlerotcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
            XCTAssertNotNil(clique.cliqueMemberIdentifier, "Should have a member identifier after a clique newFriends call")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: establishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: establishContext)

        // Fake that this peer also created some TLKShares for itself
        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: establishContext)
        self.assertSelfTLKSharesInCloudKit(context: establishContext)

        self.manager.setSOSEnabledForPlatformFlag(true)

        let (otirk, irk) = try self.createAndSetInheritanceRecoveryKey(context: establishContext)

        self.putInheritanceTLKSharesInCloudKit(irk: irk)
        self.sendContainerChangeWaitForFetch(context: establishContext)

        let preflightJoinWithInheritanceKeyExpectation = self.expectation(description: "preflightJoinWithInheritanceKey callback occurs")
        establishContext.preflightJoin(with: otirk) { error in
            XCTAssertNil(error, "error should be nil")
            preflightJoinWithInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [preflightJoinWithInheritanceKeyExpectation], timeout: 20)

        // Now, join from a new device
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: self.cuttlefishContext)

        let trustedNotification = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: "com.apple.security.octagon.trust-status-change"))
        let joinWithInheritanceKeyExpectation = self.expectation(description: "joinWithInheritanceKey callback occurs")
        self.cuttlefishContext.join(with: otirk) {error in
            XCTAssertNil(error, "error should be nil")
            joinWithInheritanceKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithInheritanceKeyExpectation], timeout: 20)
        self.fakeCuttlefishServer.establishListener = nil

        let fetchExpectation = self.expectation(description: "fetchExpectation callback occurs")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateInherited, within: 10 * NSEC_PER_SEC)
        self.wait(for: [trustedNotification], timeout: 10)

        let otOperationConfiguration = OTOperationConfiguration()
        self.cuttlefishContext.rpcTrustStatus(otOperationConfiguration) { status, _, _, _, _, _ in
            XCTAssertEqual(status, .in, "Self peer should be trusted")
        }

        self.cuttlefishContext.rpcSetUserControllableViewsSyncingStatus(false) { syncing, error in
            XCTAssertNotNil(error, "error should not be nil")
            let nsError: NSError = error! as NSError
            XCTAssertEqual(nsError.code, OctagonError.userControllableViewsUnavailable.rawValue, "error should be equal to OctagonErrorUserControllableViewsUnavailable")
            XCTAssertFalse(syncing, "syncing should be false")
            fetchExpectation.fulfill()
        }
        self.wait(for: [fetchExpectation], timeout: 20)
    }
}
#endif
