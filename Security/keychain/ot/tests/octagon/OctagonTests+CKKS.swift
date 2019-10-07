#if OCTAGON

class OctagonCKKSTests: OctagonTestsBase {
    func testHandleCKKSKeyHierarchyConflictOnEstablish() throws {
        self.startCKAccountStatusMock()

        // Right after CKKS fetches for the first time, insert a new key hierarchy into CloudKit
        self.silentFetchesAllowed = false
        self.expectCKFetchAndRun(beforeFinished: {
            self.putFakeKeyHierarchy(inCloudKit: self.manateeZoneID)
            self.putFakeDeviceStatus(inCloudKit: self.manateeZoneID)
            self.silentFetchesAllowed = true
        })

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

        // and all subCKKSes should enter waitfortlk, as they don't have the TLKs uploaded by the other peer
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLK, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func testHandleCKKSKeyHierarchyConflictOnEstablishWithKeys() throws {
        self.startCKAccountStatusMock()

        // Right after CKKS fetches for the first time, insert a new key hierarchy into CloudKit
        self.silentFetchesAllowed = false
        self.expectCKFetchAndRun(beforeFinished: {
            self.putFakeKeyHierarchy(inCloudKit: self.manateeZoneID)
            self.putFakeDeviceStatus(inCloudKit: self.manateeZoneID)
            self.saveTLKMaterial(toKeychain: self.manateeZoneID)
            self.silentFetchesAllowed = true
        })

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

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        self.verifyDatabaseMocks()
    }
}

#endif // OCTAGON
