#if OCTAGON

@objcMembers
class OctagonEscrowRecordTests: OctagonTestsBase {

    func testFetchEscrowRecord() throws {
        OctagonSetOptimizationEnabled(true)
        OctagonSetEscrowRecordFetchEnabled(true)

        let initiatorContextID = "initiator-context-id"
        let bottlerContext = self.makeInitiatorContext(contextID: initiatorContextID)

        bottlerContext.startOctagonStateMachine()
        let ckacctinfo = CKAccountInfo()
        ckacctinfo.accountStatus = .available
        ckacctinfo.hasValidCredentials = true
        ckacctinfo.accountPartition = .production

        bottlerContext.cloudkitAccountStateChange(nil, to: ckacctinfo)
        XCTAssertNoThrow(try bottlerContext.setCDPEnabled())
        self.assertEnters(context: bottlerContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = initiatorContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = self.mockAuthKit.altDSID!
        bottlerotcliqueContext.otControl = self.otControl
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

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords: [OTEscrowRecord] = escrowRecordDatas.map { OTEscrowRecord(data: $0) }
            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 1, "should be 1 escrow record")
            let reduced = escrowRecords.compactMap { $0.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
    }

    func testViableBottleCachingAfterJoin() throws {
        OctagonSetOptimizationEnabled(true)
        OctagonSetEscrowRecordFetchEnabled(true)

        let initiatorContextID = "initiator-context-id"
        let bottlerContext = self.makeInitiatorContext(contextID: initiatorContextID)

        bottlerContext.startOctagonStateMachine()
        let ckacctinfo = CKAccountInfo()
        ckacctinfo.accountStatus = .available
        ckacctinfo.hasValidCredentials = true
        ckacctinfo.accountPartition = .production

        bottlerContext.cloudkitAccountStateChange(nil, to: ckacctinfo)
        XCTAssertNoThrow(try bottlerContext.setCDPEnabled())
        self.assertEnters(context: bottlerContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = initiatorContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = self.mockAuthKit.altDSID!
        bottlerotcliqueContext.otControl = self.otControl
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

        let joinWithBottleExpectation = self.expectation(description: "joinWithBottle callback occurs")
        self.cuttlefishContext.join(withBottle: bottle.bottleID, entropy: entropy!, bottleSalt: self.otcliqueContext.altDSID!) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithBottleExpectation.fulfill()
        }

        self.wait(for: [joinWithBottleExpectation], timeout: 100)

        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(withContainer: OTCKContainerName, context: OTDefaultContext) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        self.verifyDatabaseMocks()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertTLKSharesInCloudKit(receiver: self.cuttlefishContext, sender: self.cuttlefishContext)

        //now call fetchviablebottles, we should get the uncached version
        let fetchUnCachedViableBottlesExpectation = self.expectation(description: "fetch UnCached ViableBottles")

        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            fetchUnCachedViableBottlesExpectation.fulfill()
            return nil
        }

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }

            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 2, "should be 2 escrow records")
            let reduced = escrowRecords.compactMap { $0!.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchUnCachedViableBottlesExpectation], timeout: 1)

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }

            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 2, "should be 2 escrow records")
            let reduced = escrowRecords.compactMap { $0!.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }

        //now call fetchviablebottles, we should get the cached version
        let fetchViableBottlesExpectation = self.expectation(description: "fetch Cached ViableBottles")
        fetchViableBottlesExpectation.isInverted = true

        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            fetchViableBottlesExpectation.fulfill()
            return nil
        }
        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }
            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 2, "should be 2 escrow record")
            let reduced = escrowRecords.compactMap { $0!.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchViableBottlesExpectation], timeout: 1)
    }

    func testCachedEscrowRecordFetch() throws {
        OctagonSetOptimizationEnabled(true)
        OctagonSetEscrowRecordFetchEnabled(true)

        let initiatorContextID = "initiator-context-id"
        let bottlerContext = self.makeInitiatorContext(contextID: initiatorContextID)

        bottlerContext.startOctagonStateMachine()
        let ckacctinfo = CKAccountInfo()
        ckacctinfo.accountStatus = .available
        ckacctinfo.hasValidCredentials = true
        ckacctinfo.accountPartition = .production

        bottlerContext.cloudkitAccountStateChange(nil, to: ckacctinfo)
        XCTAssertNoThrow(try bottlerContext.setCDPEnabled())
        self.assertEnters(context: bottlerContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = initiatorContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = self.mockAuthKit.altDSID!
        bottlerotcliqueContext.otControl = self.otControl
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
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let joinWithBottleExpectation = self.expectation(description: "joinWithBottle callback occurs")
        self.cuttlefishContext.join(withBottle: bottle.bottleID, entropy: entropy!, bottleSalt: self.otcliqueContext.altDSID!) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithBottleExpectation.fulfill()
        }

        self.wait(for: [joinWithBottleExpectation], timeout: 100)

        let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(withContainer: OTCKContainerName, context: OTDefaultContext) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        self.verifyDatabaseMocks()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertTLKSharesInCloudKit(receiver: self.cuttlefishContext, sender: self.cuttlefishContext)

        //now call fetchviablebottles, we should get the uncached version
        let fetchUnCachedViableBottlesExpectation = self.expectation(description: "fetch UnCached ViableBottles")

        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            fetchUnCachedViableBottlesExpectation.fulfill()
            return nil
        }

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }
            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 2, "should be 2 escrow records")
            let reduced = escrowRecords.compactMap { $0!.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchUnCachedViableBottlesExpectation], timeout: 1)

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }
            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 2, "should be 2 escrow records")
            let reduced = escrowRecords.compactMap { $0!.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        //now call fetchviablebottles, we should get the cached version
        let fetchViableBottlesExpectation = self.expectation(description: "fetch Cached ViableBottles")
        fetchViableBottlesExpectation.isInverted = true

        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            fetchViableBottlesExpectation.fulfill()
            return nil
        }
        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }
            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 2, "should be 2 escrow records")
            let reduced = escrowRecords.compactMap { $0!.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchViableBottlesExpectation], timeout: 1)

        let container = try self.tphClient.getContainer(withContainer: self.cuttlefishContext.containerName, context: initiatorContextID)
        container.escrowCacheTimeout = 1

        //sleep to invalidate the cache
        sleep(1)

        //now call fetchviablebottles, we should get the uncached version
        let uncachedViableBottlesFetchExpectation = self.expectation(description: "fetch Uncached ViableBottles")
        let fetchBottlesFromCuttlefishFetchExpectation = self.expectation(description: "fetch bottles from cuttlefish expectation")

        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            fetchBottlesFromCuttlefishFetchExpectation.fulfill()
            return nil
        }
        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }
            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 2, "should be 2 escrow records")
            let reduced = escrowRecords.compactMap { $0!.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
            uncachedViableBottlesFetchExpectation.fulfill()
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchBottlesFromCuttlefishFetchExpectation], timeout: 10)
        self.wait(for: [uncachedViableBottlesFetchExpectation], timeout: 10)
    }

    func testForcedEscrowRecordFetch() throws {
        OctagonSetOptimizationEnabled(true)
        OctagonSetEscrowRecordFetchEnabled(true)

        let initiatorContextID = "initiator-context-id"
        let bottlerContext = self.makeInitiatorContext(contextID: initiatorContextID)

        bottlerContext.startOctagonStateMachine()
        let ckacctinfo = CKAccountInfo()
        ckacctinfo.accountStatus = .available
        ckacctinfo.hasValidCredentials = true
        ckacctinfo.accountPartition = .production

        bottlerContext.cloudkitAccountStateChange(nil, to: ckacctinfo)
        XCTAssertNoThrow(try bottlerContext.setCDPEnabled())
        self.assertEnters(context: bottlerContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = initiatorContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = self.mockAuthKit.altDSID!
        bottlerotcliqueContext.otControl = self.otControl
        bottlerotcliqueContext.overrideEscrowCache = false

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
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let joinWithBottleExpectation = self.expectation(description: "joinWithBottle callback occurs")
        self.cuttlefishContext.join(withBottle: bottle.bottleID, entropy: entropy!, bottleSalt: self.otcliqueContext.altDSID!) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithBottleExpectation.fulfill()
        }

        self.wait(for: [joinWithBottleExpectation], timeout: 100)

        self.verifyDatabaseMocks()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertTLKSharesInCloudKit(receiver: self.cuttlefishContext, sender: self.cuttlefishContext)

        //now call fetchviablebottles, we should get records from cuttlefish
        let fetchUnCachedViableBottlesExpectation = self.expectation(description: "fetch UnCached ViableBottles")

        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            fetchUnCachedViableBottlesExpectation.fulfill()
            return nil
        }

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }
            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 2, "should be 2 escrow records")
            let reduced = escrowRecords.compactMap { $0!.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchUnCachedViableBottlesExpectation], timeout: 1)

        //set the override to force an escrow record fetch
        bottlerotcliqueContext.overrideEscrowCache = true

        //now call fetchviablebottles, we should get records from cuttlefish
        let fetchViableBottlesExpectation = self.expectation(description: "fetch forced ViableBottles")

        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            fetchViableBottlesExpectation.fulfill()
            return nil
        }
        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }
            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 2, "should be 2 escrow records")
            let reduced = escrowRecords.compactMap { $0!.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchViableBottlesExpectation], timeout: 10)
    }

    func testSignInWithEscrowPrecachingEnabled() throws {
        self.startCKAccountStatusMock()

        let contextName = OTDefaultContext
        let containerName = OTCKContainerName
        OctagonSetOptimizationEnabled(true)
        OctagonSetEscrowRecordFetchEnabled(true)

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        //expect fetch escrow record fetch
        let fetchViableBottlesExpectation = self.expectation(description: "fetch Cached ViableBottles")
        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            fetchViableBottlesExpectation.fulfill()
            return nil
        }

        // Device is signed out
        self.mockAuthKit.altDSID = nil
        self.mockAuthKit.hsa2 = false

        // With no account, Octagon should go directly into 'NoAccount'
        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateNoAccount, within: 10 * NSEC_PER_SEC)

        let newAltDSID = UUID().uuidString
        self.mockAuthKit.altDSID = newAltDSID
        self.mockAuthKit.hsa2 = true
        XCTAssertNoThrow(try self.cuttlefishContext.accountAvailable(newAltDSID), "Sign-in shouldn't error")

        let signInExpectation = self.expectation(description: "signing in expectation")

        self.manager.sign(in: newAltDSID, container: containerName, context: contextName) { error in
            XCTAssertNil(error, "error should not be nil")
            signInExpectation.fulfill()
        }

        self.wait(for: [signInExpectation], timeout: 10)
        self.wait(for: [fetchViableBottlesExpectation], timeout: 10)
    }

    func testLegacyEscrowRecordFetch() throws {
        OctagonSetOptimizationEnabled(true)
        OctagonSetEscrowRecordFetchEnabled(true)
        OctagonSetPlatformSupportsSOS(true)

        self.startCKAccountStatusMock()

        let initiatorContextID = "joiner"
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = OTDefaultContext
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = self.mockAuthKit.altDSID!
        self.mockAuthKit.hsa2 = true
        bottlerotcliqueContext.otControl = self.otControl

        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCInCircle)
        XCTAssertTrue(OctagonPerformSOSUpgrade(), "SOS upgrade should be on")

        // SOS TLK shares will be uploaded after the establish
        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        self.verifyDatabaseMocks()

        let joinerContext = self.makeInitiatorContext(contextID: initiatorContextID)
        self.assertJoinViaEscrowRecovery(joiningContext: joinerContext, sponsor: self.cuttlefishContext)

        let container = try self.tphClient.getContainer(withContainer: self.cuttlefishContext.containerName, context: OTDefaultContext)

        //now call fetchviablebottles, we should get the uncached version
        let fetchUnCachedViableBottlesExpectation = self.expectation(description: "fetch UnCached ViableBottles")

        self.fakeCuttlefishServer.injectLegacyEscrowRecords = true
        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            fetchUnCachedViableBottlesExpectation.fulfill()
            return nil
        }

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }

            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")

            XCTAssertEqual(escrowRecords.count, 3, "should be 3 escrow records")
            let recordsWithBottles = escrowRecords.filter { $0!.escrowInformationMetadata.bottleId != "" }
            XCTAssertEqual(recordsWithBottles.count, 2, "should be 2 escrow records with a bottleID")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchUnCachedViableBottlesExpectation], timeout: 1)

        //now call fetchviablebottles, we should get the cached version
        let fetchViableBottlesExpectation = self.expectation(description: "fetch Cached ViableBottles")
        fetchViableBottlesExpectation.isInverted = true

        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            let legacy = container.containerMO.legacyEscrowRecords as! Set<EscrowRecordMO>
            let partial = container.containerMO.partiallyViableEscrowRecords as! Set<EscrowRecordMO>
            let full = container.containerMO.fullyViableEscrowRecords as! Set<EscrowRecordMO>

            XCTAssertEqual(legacy.count, 1, "legacy escrowRecords should contain 1 record")
            XCTAssertEqual(partial.count, 1, "partially viable escrowRecords should contain 1 record")
            XCTAssertEqual(full.count, 1, "fully viable escrowRecords should contain 1 record")

            fetchViableBottlesExpectation.fulfill()
            return nil
        }
        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }
            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")

            XCTAssertEqual(escrowRecords.count, 3, "should be 3 escrow record")
            let recordsWithBottles = escrowRecords.filter { $0!.escrowInformationMetadata.bottleId != "" }
            XCTAssertEqual(recordsWithBottles.count, 2, "should be 2 escrow records with a bottleID")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchViableBottlesExpectation], timeout: 1)

        //check cache is empty after escrow fetch timeout expires
        container.escrowCacheTimeout = 1
        sleep(1)

        //now call fetchviablebottles, we should get the uncached version, check there's 0 cached records
        let fetchViableBottlesAfterExpiredTimeoutExpectation = self.expectation(description: "fetch Cached ViableBottles expectaiton after timeout")
        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            XCTAssertEqual(container.containerMO.legacyEscrowRecords as? Set<EscrowRecordMO>, [], "legacy escrowRecords should be empty")
            XCTAssertEqual(container.containerMO.partiallyViableEscrowRecords as? Set<EscrowRecordMO>, [], "partially viable escrowRecords should be empty")
            XCTAssertEqual(container.containerMO.fullyViableEscrowRecords as? Set<EscrowRecordMO>, [], "fully viable escrowRecords should be empty")

            fetchViableBottlesAfterExpiredTimeoutExpectation.fulfill()
            return nil
        }

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }
            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 3, "should be 3 escrow record")
            let recordsWithBottles = escrowRecords.filter { $0!.escrowInformationMetadata.bottleId != "" }
            XCTAssertEqual(recordsWithBottles.count, 2, "should be 2 escrow records with a bottleID")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchViableBottlesAfterExpiredTimeoutExpectation], timeout: 10)
    }

    func testEmptyEscrowRecords() throws {
        OctagonSetOptimizationEnabled(true)
        OctagonSetEscrowRecordFetchEnabled(true)

        self.fakeCuttlefishServer.includeEscrowRecords = false
        self.fakeCuttlefishServer.injectLegacyEscrowRecords = false

        let initiatorContextID = "initiator-context-id"
        let bottlerContext = self.makeInitiatorContext(contextID: initiatorContextID)

        bottlerContext.startOctagonStateMachine()
        let ckacctinfo = CKAccountInfo()
        ckacctinfo.accountStatus = .available
        ckacctinfo.hasValidCredentials = true
        ckacctinfo.accountPartition = .production

        bottlerContext.cloudkitAccountStateChange(nil, to: ckacctinfo)
        XCTAssertNoThrow(try bottlerContext.setCDPEnabled())
        self.assertEnters(context: bottlerContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = initiatorContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = self.mockAuthKit.altDSID!
        bottlerotcliqueContext.otControl = self.otControl
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

        let container = try self.tphClient.getContainer(withContainer: self.cuttlefishContext.containerName, context: initiatorContextID)

        let fetchViableBottlesAfterExpiredTimeoutExpectation = self.expectation(description: "fetch Cached ViableBottles expectaiton after timeout")

        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            container.moc.performAndWait {
                XCTAssertEqual(container.containerMO.legacyEscrowRecords as? Set<EscrowRecordMO>, [], "legacy escrowRecords should be empty")
                XCTAssertEqual(container.containerMO.partiallyViableEscrowRecords as? Set<EscrowRecordMO>, [], "partially viable escrowRecords should be empty")
                XCTAssertEqual(container.containerMO.fullyViableEscrowRecords as? Set<EscrowRecordMO>, [], "fully viable escrowRecords should be empty")
            }
            fetchViableBottlesAfterExpiredTimeoutExpectation.fulfill()
            return nil
        }
        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            XCTAssertEqual(escrowRecordDatas.count, 0, "should be 0 escrow records")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchViableBottlesAfterExpiredTimeoutExpectation], timeout: 10)
        container.moc.performAndWait {
            XCTAssertEqual(container.containerMO.legacyEscrowRecords as? Set<EscrowRecordMO>, [], "legacy escrowRecords should be empty")
            XCTAssertEqual(container.containerMO.partiallyViableEscrowRecords as? Set<EscrowRecordMO>, [], "partially viable escrowRecords should be empty")
            XCTAssertEqual(container.containerMO.fullyViableEscrowRecords as? Set<EscrowRecordMO>, [], "fully viable escrowRecords should be empty")
        }
    }

    func testRemoveEscrowCache() throws {
        OctagonSetOptimizationEnabled(true)
        OctagonSetEscrowRecordFetchEnabled(true)

        let initiatorContextID = "initiator-context-id"
        let bottlerContext = self.makeInitiatorContext(contextID: initiatorContextID)

        bottlerContext.startOctagonStateMachine()
        let ckacctinfo = CKAccountInfo()
        ckacctinfo.accountStatus = .available
        ckacctinfo.hasValidCredentials = true
        ckacctinfo.accountPartition = .production

        bottlerContext.cloudkitAccountStateChange(nil, to: ckacctinfo)
        XCTAssertNoThrow(try bottlerContext.setCDPEnabled())
        self.assertEnters(context: bottlerContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        let bottlerotcliqueContext = OTConfigurationContext()
        bottlerotcliqueContext.context = initiatorContextID
        bottlerotcliqueContext.dsid = "1234"
        bottlerotcliqueContext.altDSID = self.mockAuthKit.altDSID!
        bottlerotcliqueContext.otControl = self.otControl
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

        let bottle = self.fakeCuttlefishServer.state.bottles[0]

        self.cuttlefishContext.startOctagonStateMachine()
        self.startCKAccountStatusMock()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords: [OTEscrowRecord] = escrowRecordDatas.map { OTEscrowRecord(data: $0) }
            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 1, "should be 1 escrow record")
            let reduced = escrowRecords.compactMap { $0.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }

        let removeExpectation = self.expectation(description: "remove expectation")
        self.manager.invalidateEscrowCache(OTCKContainerName, contextID: initiatorContextID) { error in
            XCTAssertNil(error, "error should not be nil")
            removeExpectation.fulfill()
        }
        self.wait(for: [removeExpectation], timeout: 10)

        let container = try self.tphClient.getContainer(withContainer: self.cuttlefishContext.containerName, context: initiatorContextID)

        let fetchViableBottlesAfterCacheRemovalExpectation = self.expectation(description: "fetchViableBottles expectation after cache removal")
        self.fakeCuttlefishServer.fetchViableBottlesListener = { request in
            self.fakeCuttlefishServer.fetchViableBottlesListener = nil
            XCTAssertEqual(container.containerMO.legacyEscrowRecords as? Set<EscrowRecordMO>, [], "legacy escrowRecords should be empty")
            XCTAssertEqual(container.containerMO.partiallyViableEscrowRecords as? Set<EscrowRecordMO>, [], "partially viable escrowRecords should be empty")
            XCTAssertEqual(container.containerMO.fullyViableEscrowRecords as? Set<EscrowRecordMO>, [], "fully viable escrowRecords should be empty")

            fetchViableBottlesAfterCacheRemovalExpectation.fulfill()
            return nil
        }

        do {
            let escrowRecordDatas = try OTClique.fetchEscrowRecordsInternal(bottlerotcliqueContext)
            let escrowRecords = escrowRecordDatas.map { OTEscrowRecord(data: $0) }
            XCTAssertNotNil(escrowRecords, "escrowRecords should not be nil")
            XCTAssertEqual(escrowRecords.count, 1, "should be 1 escrow records")
            let reduced = escrowRecords.compactMap { $0!.escrowInformationMetadata.bottleId }
            XCTAssert(reduced.contains(bottle.bottleID), "The bottle we're about to restore should be viable")
        } catch {
            XCTFail("Shouldn't have errored fetching escrow records: \(error)")
            throw error
        }
        self.wait(for: [fetchViableBottlesAfterCacheRemovalExpectation], timeout: 10)

    }
}

#endif
