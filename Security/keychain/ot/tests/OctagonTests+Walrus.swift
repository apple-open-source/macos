#if OCTAGON

import Foundation

class OctagonWalrusTests: OctagonTestsBase {
    override func setUp() {
        super.setUp()

        self.otControlEntitlementBearer.entitlements[kSecEntitlementPrivateOctagonWalrus] = true
    }

    func testWalrusAPIRequiresEntitlement() throws {
        self.otControlEntitlementBearer.entitlements.removeAll()

        self.startCKAccountStatusMock()

        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        let setting = OTAccountSettings()!
        let walrus = OTWalrus()!
        walrus.enabled = true
        setting.walrus = walrus

        XCTAssertThrowsError(try cliqueBridge.setAccountSetting(setting)) { error in
            let nserror = error as NSError
            XCTAssertEqual(nserror.code, Int(errSecMissingEntitlement), "Error should be errSecMissingEntitlement")
        }
    }

    func testSetWalrusAfterEstablish() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        let setting = OTAccountSettings()!
        let walrus = OTWalrus()!
        walrus.enabled = true
        setting.walrus = walrus

        // Set walrus
        let setExpectation = self.expectation(description: "walrus expectation")
        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()
            XCTAssertEqual(newStableInfo.walrusSetting!.value, setting.walrus.enabled, "walrus should be set correctly")

            setExpectation.fulfill()
            return nil
        }

        XCTAssertNoThrow(try cliqueBridge.setAccountSetting(setting), "Should be able to successfully set walrus setting")
        self.wait(for: [setExpectation], timeout: 10)
        self.fakeCuttlefishServer.updateListener = nil
    }

    func testWalrusInSyncTwoPeers() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        let setting = OTAccountSettings()!
        let walrus = OTWalrus()!
        walrus.enabled = true
        setting.walrus = walrus

        // Set walrus
        let setExpectation = self.expectation(description: "walrus expectation")
        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()
            XCTAssertEqual(newStableInfo.walrusSetting!.value, setting.walrus.enabled, "walrus should be set correctly")

            setExpectation.fulfill()
            return nil
        }

        XCTAssertNoThrow(try cliqueBridge.setAccountSetting(setting), "Should be able to successfully set walrus setting")
        self.wait(for: [setExpectation], timeout: 10)
        self.fakeCuttlefishServer.updateListener = nil

        let joiningContext = self.makeInitiatorContext(contextID: "peer2", authKitAdapter: self.mockAuthKit2)
        self.assertJoinViaEscrowRecovery(joiningContext: joiningContext, sponsor: self.cuttlefishContext)

        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: joiningContext)

        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertConsidersSelfTrusted(context: joiningContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        var dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            let walrus = stableInfo!["walrus"] as! NSDictionary
            XCTAssertNotNil(walrus, "walrus should not be nil")
            XCTAssertEqual(walrus["clock"] as! Int, 0, "walrus clock should be 0")
            XCTAssertEqual(walrus["value"] as! Int, 1, "walrus value should be 1")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(joiningContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            let walrus = stableInfo!["walrus"] as! NSDictionary
            XCTAssertNotNil(walrus, "walrus should not be nil")
            XCTAssertEqual(walrus["clock"] as! Int, 0, "walrus clock should be 0")
            XCTAssertEqual(walrus["value"] as! Int, 1, "walrus value should be 1")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)
    }

    // test two peers, one updating walrus setting 11 times
    func testWalrusInSyncBetweenTwoPeersAfterManyUpdates() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        let setting = OTAccountSettings()!
        let walrus = OTWalrus()!
        walrus.enabled = true
        setting.walrus = walrus

        // Set walrus
        let setExpectation = self.expectation(description: "walrus expectation")
        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()
            XCTAssertEqual(newStableInfo.walrusSetting!.value, setting.walrus.enabled, "walrus should be set correctly")

            setExpectation.fulfill()
            return nil
        }

        XCTAssertNoThrow(try cliqueBridge.setAccountSetting(setting), "Should be able to successfully set walrus setting")
        self.wait(for: [setExpectation], timeout: 10)
        self.fakeCuttlefishServer.updateListener = nil

        let peer2 = self.makeInitiatorContext(contextID: "peer2", authKitAdapter: self.mockAuthKit2)
        self.assertJoinViaEscrowRecovery(joiningContext: peer2, sponsor: self.cuttlefishContext)

        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: peer2)

        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertConsidersSelfTrusted(context: peer2)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        var dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            let walrus = stableInfo!["walrus"] as! NSDictionary
            XCTAssertNotNil(walrus, "walrus should not be nil")
            XCTAssertEqual(walrus["clock"] as! Int, 0, "walrus clock should be 0")
            XCTAssertEqual(walrus["value"] as! Int, 1, "walrus value should be 1")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(peer2.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            let walrus = stableInfo!["walrus"] as! NSDictionary
            XCTAssertNotNil(walrus, "walrus should not be nil")
            XCTAssertEqual(walrus["clock"] as! Int, 0, "walrus clock should be 0")
            XCTAssertEqual(walrus["value"] as! Int, 1, "walrus value should be 1")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        for i in 1...10 {
            // Set walrus
            let setting = OTAccountSettings()!
            let walrus = OTWalrus()!
            if i % 2 == 0 {
                walrus.enabled = true
            } else {
                walrus.enabled = false
            }
            setting.walrus = walrus

            let setExpectation = self.expectation(description: "walrus expectation")
            self.fakeCuttlefishServer.updateListener = { request in
                let newStableInfo = request.stableInfoAndSig.stableInfo()
                XCTAssertEqual(newStableInfo.walrusSetting!.value, setting.walrus.enabled, "walrus should be set correctly")

                setExpectation.fulfill()
                return nil
            }

            XCTAssertNoThrow(try cliqueBridge.setAccountSetting(setting), "Should be able to successfully set walrus setting")
            self.wait(for: [setExpectation], timeout: 10)
            self.fakeCuttlefishServer.updateListener = nil
            self.sendContainerChangeWaitForFetch(context: peer2)
            self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

            var dumpCallback = self.expectation(description: "dumpCallback callback occurs")
            self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
                XCTAssertNotNil(dump, "dump should not be nil")
                let egoSelf = dump!["self"] as? [String: AnyObject]
                XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
                let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
                XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
                let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
                XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
                let walrus = stableInfo!["walrus"] as! NSDictionary
                XCTAssertNotNil(walrus, "walrus should not be nil")
                XCTAssertEqual(walrus["clock"] as! Int, i, String(format: "walrus clock should be %d", i))
                if i % 2 == 0 {
                    XCTAssertEqual(walrus["value"] as! Int, 1, "walrus value should be 1")
                } else {
                    XCTAssertEqual(walrus["value"] as! Int, 0, "walrus value should be 0")
                }
                let included = dynamicInfo!["included"] as? [String]
                XCTAssertNotNil(included, "included should not be nil")
                XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
                dumpCallback.fulfill()
            }
            self.wait(for: [dumpCallback], timeout: 10)

            dumpCallback = self.expectation(description: "dumpCallback callback occurs")
            self.tphClient.dump(with: try XCTUnwrap(peer2.activeAccount)) { dump, _ in
                XCTAssertNotNil(dump, "dump should not be nil")
                let egoSelf = dump!["self"] as? [String: AnyObject]
                XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
                let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
                XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
                let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
                XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
                let walrus = stableInfo!["walrus"] as! NSDictionary
                XCTAssertNotNil(walrus, "walrus should not be nil")
                XCTAssertEqual(walrus["clock"] as! Int, i, String(format: "walrus clock should be %d", i))
                if i % 2 == 0 {
                    XCTAssertEqual(walrus["value"] as! Int, 1, "walrus value should be 1")
                } else {
                    XCTAssertEqual(walrus["value"] as! Int, 0, "walrus value should be 0")
                }
                let included = dynamicInfo!["included"] as? [String]
                XCTAssertNotNil(included, "included should not be nil")
                XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
                dumpCallback.fulfill()
            }
            self.wait(for: [dumpCallback], timeout: 10)
        }
        self.verifyDatabaseMocks()
    }

    // test very out of date peers
    func testWalrusInSyncOneVeryOutOfDatePeer() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        let setting = OTAccountSettings()!
        let walrus = OTWalrus()!
        walrus.enabled = true
        setting.walrus = walrus

        // Set walrus
        var setExpectation = self.expectation(description: "walrus expectation")
        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()
            XCTAssertEqual(newStableInfo.walrusSetting!.value, setting.walrus.enabled, "walrus should be set correctly")

            setExpectation.fulfill()
            return nil
        }

        XCTAssertNoThrow(try cliqueBridge.setAccountSetting(setting), "Should be able to successfully set walrus setting")
        self.wait(for: [setExpectation], timeout: 10)
        self.fakeCuttlefishServer.updateListener = nil

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        let peer2 = self.makeInitiatorContext(contextID: "peer2", authKitAdapter: self.mockAuthKit2)
        self.assertJoinViaEscrowRecovery(joiningContext: peer2, sponsor: self.cuttlefishContext)

        self.assertEnters(context: peer2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: peer2)

        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertConsidersSelfTrusted(context: peer2)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // peer 2 sets a different setting
        walrus.enabled = false
        setting.walrus = walrus
        setExpectation = self.expectation(description: "walrus expectation")
        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()
            XCTAssertEqual(newStableInfo.walrusSetting!.value, setting.walrus.enabled, "walrus should be set correctly")

            setExpectation.fulfill()
            return nil
        }

        let secondSetExpectation = self.expectation(description: "walrus expectation again")
        peer2.rpcSetAccountSetting(setting) { error in
            XCTAssertNil(error, "error should be nil")
            secondSetExpectation.fulfill()
        }
        self.wait(for: [secondSetExpectation], timeout: 10)
        self.wait(for: [setExpectation], timeout: 10)
        self.fakeCuttlefishServer.updateListener = nil

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: peer2)

        walrus.enabled = true
        setting.walrus = walrus
        setExpectation = self.expectation(description: "walrus expectation")
        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()
            XCTAssertEqual(newStableInfo.walrusSetting!.value, setting.walrus.enabled, "walrus should be set correctly")

            setExpectation.fulfill()
            return nil
        }

        XCTAssertNoThrow(try cliqueBridge.setAccountSetting(setting), "Should be able to successfully set walrus setting")
        self.wait(for: [setExpectation], timeout: 10)
        self.fakeCuttlefishServer.updateListener = nil

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: peer2)

        var dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            let walrus = stableInfo!["walrus"] as! NSDictionary
            XCTAssertNotNil(walrus, "walrus should not be nil")
            XCTAssertEqual(walrus["clock"] as! Int, 2, "walrus clock should be 2")
            XCTAssertEqual(walrus["value"] as! Int, 1, "walrus value should be 1")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(peer2.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            let walrus = stableInfo!["walrus"] as! NSDictionary
            XCTAssertNotNil(walrus, "walrus should not be nil")
            XCTAssertEqual(walrus["clock"] as! Int, 2, "walrus clock should be 2")
            XCTAssertEqual(walrus["value"] as! Int, 1, "walrus value should be 1")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        for i in 1...10 {
            // Set walrus
            let setting = OTAccountSettings()!
            let walrus = OTWalrus()!
            if i % 2 == 0 {
                walrus.enabled = true
            } else {
                walrus.enabled = false
            }
            setting.walrus = walrus

            let setExpectation = self.expectation(description: "walrus expectation")
            self.fakeCuttlefishServer.updateListener = { request in
                let newStableInfo = request.stableInfoAndSig.stableInfo()
                XCTAssertEqual(newStableInfo.walrusSetting!.value, setting.walrus.enabled, "walrus should be set correctly")

                setExpectation.fulfill()
                return nil
            }

            XCTAssertNoThrow(try cliqueBridge.setAccountSetting(setting), "Should be able to successfully set walrus setting")
            self.wait(for: [setExpectation], timeout: 10)
            self.fakeCuttlefishServer.updateListener = nil
            self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

            let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
            self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
                XCTAssertNotNil(dump, "dump should not be nil")
                let egoSelf = dump!["self"] as? [String: AnyObject]
                XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
                let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
                XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
                let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
                XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
                let walrus = stableInfo!["walrus"] as! NSDictionary
                XCTAssertNotNil(walrus, "walrus should not be nil")
                XCTAssertEqual(walrus["clock"] as! Int, i + 2, String(format: "walrus clock should be %d", i + 1))
                if i % 2 == 0 {
                    XCTAssertEqual(walrus["value"] as! Int, 1, "walrus value should be 1")
                } else {
                    XCTAssertEqual(walrus["value"] as! Int, 0, "walrus value should be 0")
                }
                let included = dynamicInfo!["included"] as? [String]
                XCTAssertNotNil(included, "included should not be nil")
                XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
                dumpCallback.fulfill()
            }
            self.wait(for: [dumpCallback], timeout: 10)
        }

        self.sendContainerChangeWaitForFetch(context: peer2)
        dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(peer2.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            let walrus = stableInfo!["walrus"] as! NSDictionary
            XCTAssertNotNil(walrus, "walrus should not be nil")
            XCTAssertEqual(walrus["clock"] as! Int, 12, "walrus clock should be 12")
            XCTAssertEqual(walrus["value"] as! Int, 1, "walrus value should be 1")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)
        self.verifyDatabaseMocks()
    }

    // this test forces two peers to have the same clock but different values at the same time as a stable changes update
    func testPeerTakesWalrusStableChangesAfterWalrusValueConflictButEqualClocks() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        let peer2 = self.makeInitiatorContext(contextID: "peer2", authKitAdapter: self.mockAuthKit2)
        self.assertJoinViaEscrowRecovery(joiningContext: peer2, sponsor: self.cuttlefishContext)

        self.assertEnters(context: peer2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: peer2)

        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertConsidersSelfTrusted(context: peer2)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertEnters(context: peer2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        let setting = OTAccountSettings()!
        let walrus = OTWalrus()!
        walrus.enabled = true
        setting.walrus = walrus

        let peer1SetExpectation = self.expectation(description: "peer 1 expectation")
        self.fakeCuttlefishServer.updateListener = { _ in
            peer1SetExpectation.fulfill()
            return nil
        }

        XCTAssertNoThrow(try cliqueBridge.setAccountSetting(setting), "Should not throw an error")
        self.wait(for: [peer1SetExpectation], timeout: 10)
        self.fakeCuttlefishServer.updateListener = nil
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: peer2)

        let container2 = try! self.tphClient.getContainer(with: try XCTUnwrap(peer2.activeAccount))
        let peersFromContainer2 = container2.model.allPeers()
        XCTAssertNotNil(peersFromContainer2, "peersFromContainer2 should not be nil")

        let firstPeer = peersFromContainer2[0]
        let firstPeerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        if firstPeer.permanentInfo.peerID == firstPeerID {
            firstPeer.stableInfo!.walrusSetting!.value = false
            firstPeer.stableInfo!.walrusSetting!.clock = 1

            let secondPeer = peersFromContainer2[1]
            secondPeer.stableInfo!.walrusSetting!.value = true
            secondPeer.stableInfo!.walrusSetting!.clock = 1
        } else {
            firstPeer.stableInfo!.walrusSetting!.value = true
            firstPeer.stableInfo!.walrusSetting!.clock = 1

            let secondPeer = peersFromContainer2[1]
            secondPeer.stableInfo!.walrusSetting!.value = false
            secondPeer.stableInfo!.walrusSetting!.clock = 1
        }

        let peer2SetExpectation = self.expectation(description: "peer 2 expectation")
        self.fakeCuttlefishServer.updateListener = { request in
            XCTAssertTrue(request.hasStableInfoAndSig, "updateTrust request should have a stableInfo info")
            let newStableInfo = TPPeerStableInfo(data: request.stableInfoAndSig.peerStableInfo, sig: request.stableInfoAndSig.sig)
            XCTAssertNotNil(newStableInfo, "should be able to make a stableInfo info from protobuf")
            XCTAssertFalse(newStableInfo!.walrusSetting!.value, "walrus setting should be false")
            peer2SetExpectation.fulfill()
            return nil
        }

        walrus.enabled = false
        setting.walrus = walrus

        let secondSetExpectation = self.expectation(description: "walrus expectation again")
        peer2.rpcSetAccountSetting(setting) { error in
            XCTAssertNil(error, "error should be nil")
            secondSetExpectation.fulfill()
        }
        self.wait(for: [secondSetExpectation], timeout: 10)
        self.wait(for: [peer2SetExpectation], timeout: 10)
        self.fakeCuttlefishServer.updateListener = nil

        var retSetting: OTAccountSettings?
        var fetchExpectation = self.expectation(description: "walrus expectation again")
        peer2.rpcFetchAccountSettings { setting, _ in
            XCTAssertNotNil(setting, "setting should not be nil")
            retSetting = setting
            fetchExpectation.fulfill()
        }
        self.wait(for: [fetchExpectation], timeout: 10)

        XCTAssertFalse(retSetting!.walrus!.enabled, "walrus should be disabled")

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: peer2)

        retSetting = nil
        fetchExpectation = self.expectation(description: "walrus expectation again")
        peer2.rpcFetchAccountSettings { setting, _ in
            XCTAssertNotNil(setting, "setting should not be nil")
            retSetting = setting
            fetchExpectation.fulfill()
        }
        self.wait(for: [fetchExpectation], timeout: 10)

        XCTAssertFalse(retSetting!.walrus!.enabled, "walrus should not be enabled")

        retSetting = nil
        fetchExpectation = self.expectation(description: "walrus expectation again")
        self.cuttlefishContext.rpcFetchAccountSettings { setting, _ in
            XCTAssertNotNil(setting, "setting should not be nil")
            retSetting = setting
            fetchExpectation.fulfill()
        }
        self.wait(for: [fetchExpectation], timeout: 10)

        XCTAssertFalse(retSetting!.walrus!.enabled, "walrus should not be enabled")
        self.verifyDatabaseMocks()
    }

    // this test forces two peers to have the same clock but different values at the time of a fetch changes and empty stable changes
    func testPeerForcesWalrusEnabledWhenTwoPeersConflictOnWalrusValuesAndSameClock() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        let peer2 = self.makeInitiatorContext(contextID: "peer2", authKitAdapter: self.mockAuthKit2)
        self.assertJoinViaEscrowRecovery(joiningContext: peer2, sponsor: self.cuttlefishContext)

        self.assertEnters(context: peer2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: peer2)

        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertConsidersSelfTrusted(context: peer2)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertEnters(context: peer2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        let setting = OTAccountSettings()!
        let walrus = OTWalrus()!
        walrus.enabled = true
        setting.walrus = walrus

        let peer1SetExpectation = self.expectation(description: "peer 1 expectation")
        self.fakeCuttlefishServer.updateListener = { _ in
            peer1SetExpectation.fulfill()
            return nil
        }

        XCTAssertNoThrow(try cliqueBridge.setAccountSetting(setting), "Should not throw an error")
        self.wait(for: [peer1SetExpectation], timeout: 10)
        self.fakeCuttlefishServer.updateListener = nil
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: peer2)

        let container2 = try! self.tphClient.getContainer(with: try XCTUnwrap(peer2.activeAccount))
        let peersFromContainer2 = container2.model.allPeers()
        XCTAssertNotNil(peersFromContainer2, "peersFromContainer2 should not be nil")

        let firstPeer = peersFromContainer2[0]
        let firstPeerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        if firstPeer.permanentInfo.peerID == firstPeerID {
            firstPeer.stableInfo!.walrusSetting!.value = false
            firstPeer.stableInfo!.walrusSetting!.clock = 1

            let secondPeer = peersFromContainer2[1]
            secondPeer.stableInfo!.walrusSetting!.value = true
            secondPeer.stableInfo!.walrusSetting!.clock = 1
        } else {
            firstPeer.stableInfo!.walrusSetting!.value = true
            firstPeer.stableInfo!.walrusSetting!.clock = 1

            let secondPeer = peersFromContainer2[1]
            secondPeer.stableInfo!.walrusSetting!.value = false
            secondPeer.stableInfo!.walrusSetting!.clock = 1
        }

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: peer2)

        var retSetting: OTAccountSettings?
        var fetchExpectation = self.expectation(description: "walrus expectation again")
        peer2.rpcFetchAccountSettings { setting, _ in
            XCTAssertNotNil(setting, "setting should not be nil")
            retSetting = setting
            fetchExpectation.fulfill()
        }
        self.wait(for: [fetchExpectation], timeout: 10)

        XCTAssertTrue(retSetting!.walrus!.enabled, "walrus should be enabled")

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: peer2)

        retSetting = nil
        fetchExpectation = self.expectation(description: "walrus expectation again")
        peer2.rpcFetchAccountSettings { setting, _ in
            XCTAssertNotNil(setting, "setting should not be nil")
            retSetting = setting
            fetchExpectation.fulfill()
        }
        self.wait(for: [fetchExpectation], timeout: 10)

        XCTAssertTrue(retSetting!.walrus!.enabled, "walrus should be enabled")

        retSetting = nil
        fetchExpectation = self.expectation(description: "walrus expectation again")
        self.cuttlefishContext.rpcFetchAccountSettings { setting, _ in
            XCTAssertNotNil(setting, "setting should not be nil")
            retSetting = setting
            fetchExpectation.fulfill()
        }
        self.wait(for: [fetchExpectation], timeout: 10)

        XCTAssertTrue(retSetting!.walrus!.enabled, "walrus should be enabled")

        self.verifyDatabaseMocks()
    }

    func testAttemptSetWalrusAndFail() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        let setting = OTAccountSettings()!
        let walrus = OTWalrus()!
        walrus.enabled = true
        setting.walrus = walrus

        // Attempt to set walrus
        let setExpectation = self.expectation(description: "walrus expectation")
        setExpectation.isInverted = true
        self.fakeCuttlefishServer.updateListener = { _ in
            setExpectation.fulfill()
            return nil
        }
        let container = try self.tphClient.getContainer(with: try XCTUnwrap(self.cuttlefishContext.activeAccount))
        container.testDontSetAccountSetting = true

        XCTAssertThrowsError(try cliqueBridge.setAccountSetting(setting), "This should throw an error setting walrus account setting")
        self.wait(for: [setExpectation], timeout: 1)
        self.fakeCuttlefishServer.updateListener = nil
    }

    func testWebAccessAPIRequiresEntitlement() throws {
        self.otControlEntitlementBearer.entitlements.removeAll()

        self.startCKAccountStatusMock()

        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        let setting = OTAccountSettings()!
        let webAccess = OTWebAccess()!
        webAccess.enabled = true
        setting.webAccess = webAccess

        XCTAssertThrowsError(try cliqueBridge.setAccountSetting(setting)) { error in
            let nserror = error as NSError
            XCTAssertEqual(nserror.code, Int(errSecMissingEntitlement), "Error should be errSecMissingEntitlement")
        }
    }

    func testSetWebAccessAfterEstablish() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        let setting = OTAccountSettings()!
        let webAccess = OTWebAccess()!
        webAccess.enabled = true
        setting.webAccess = webAccess

        // Set web access
        let setExpectation = self.expectation(description: "web access expectation")
        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()
            XCTAssertEqual(newStableInfo.webAccess!.value, setting.webAccess.enabled, "webAccess should be set correctly")

            setExpectation.fulfill()
            return nil
        }

        XCTAssertNoThrow(try cliqueBridge.setAccountSetting(setting), "Should be able to successfully set webAccess setting")
        self.wait(for: [setExpectation], timeout: 10)
        self.fakeCuttlefishServer.updateListener = nil
    }

    func testWebAccessInSyncTwoPeers() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        let setting = OTAccountSettings()!
        let webAccess = OTWebAccess()!
        webAccess.enabled = true
        setting.webAccess = webAccess

        // Set web access
        let setExpectation = self.expectation(description: "web access expectation")
        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()
            XCTAssertEqual(newStableInfo.webAccess!.value, setting.webAccess.enabled, "webAccess should be set correctly")

            setExpectation.fulfill()
            return nil
        }

        XCTAssertNoThrow(try cliqueBridge.setAccountSetting(setting), "Should be able to successfully set webAccess setting")
        self.wait(for: [setExpectation], timeout: 10)
        self.fakeCuttlefishServer.updateListener = nil

        let joiningContext = self.makeInitiatorContext(contextID: "peer2", authKitAdapter: self.mockAuthKit2)
        self.assertJoinViaEscrowRecovery(joiningContext: joiningContext, sponsor: self.cuttlefishContext)

        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: joiningContext)

        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertConsidersSelfTrusted(context: joiningContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        var dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            let webAccess = stableInfo!["web_access"] as! NSDictionary
            XCTAssertNotNil(webAccess, "webAccess should not be nil")
            XCTAssertEqual(webAccess["clock"] as! Int, 0, "webAccess clock should be 0")
            XCTAssertEqual(webAccess["value"] as! Int, 1, "webAccess value should be 1")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(joiningContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            let webAccess = stableInfo!["web_access"] as! NSDictionary
            XCTAssertNotNil(webAccess, "webAccess should not be nil")
            XCTAssertEqual(webAccess["clock"] as! Int, 0, "webAccess clock should be 0")
            XCTAssertEqual(webAccess["value"] as! Int, 1, "webAccess value should be 1")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)
    }

    // test two peers, one updating webAccess setting 11 times
    func testWebAccessInSyncBetweenTwoPeersAfterManyUpdates() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        let setting = OTAccountSettings()!
        let webAccess = OTWebAccess()!
        webAccess.enabled = true
        setting.webAccess = webAccess

        // Set webAccess
        let setExpectation = self.expectation(description: "webAccess expectation")
        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()
            XCTAssertEqual(newStableInfo.webAccess!.value, setting.webAccess.enabled, "webAccess should be set correctly")

            setExpectation.fulfill()
            return nil
        }

        XCTAssertNoThrow(try cliqueBridge.setAccountSetting(setting), "Should be able to successfully set web access setting")
        self.wait(for: [setExpectation], timeout: 10)
        self.fakeCuttlefishServer.updateListener = nil

        let peer2 = self.makeInitiatorContext(contextID: "peer2", authKitAdapter: self.mockAuthKit2)
        self.assertJoinViaEscrowRecovery(joiningContext: peer2, sponsor: self.cuttlefishContext)

        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: peer2)

        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertConsidersSelfTrusted(context: peer2)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        var dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            let webAccess = stableInfo!["web_access"] as! NSDictionary
            XCTAssertNotNil(webAccess, "webAccess should not be nil")
            XCTAssertEqual(webAccess["clock"] as! Int, 0, "webAccess clock should be 0")
            XCTAssertEqual(webAccess["value"] as! Int, 1, "webAccess value should be 1")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(peer2.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            let webAccess = stableInfo!["web_access"] as! NSDictionary
            XCTAssertNotNil(webAccess, "webAccess should not be nil")
            XCTAssertEqual(webAccess["clock"] as! Int, 0, "webAccess clock should be 0")
            XCTAssertEqual(webAccess["value"] as! Int, 1, "webAccess value should be 1")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        for i in 1...10 {
            // Set webAccess
            let setting = OTAccountSettings()!
            let webAccess = OTWebAccess()!
            if i % 2 == 0 {
                webAccess.enabled = true
            } else {
                webAccess.enabled = false
            }
            setting.webAccess = webAccess

            let setExpectation = self.expectation(description: "webAccess expectation")
            self.fakeCuttlefishServer.updateListener = { request in
                let newStableInfo = request.stableInfoAndSig.stableInfo()
                XCTAssertEqual(newStableInfo.webAccess!.value, setting.webAccess.enabled, "webAccess should be set correctly")
                setExpectation.fulfill()
                return nil
            }

            XCTAssertNoThrow(try cliqueBridge.setAccountSetting(setting), "Should be able to successfully set web access setting")
            self.wait(for: [setExpectation], timeout: 10)
            self.fakeCuttlefishServer.updateListener = nil
            self.sendContainerChangeWaitForFetch(context: peer2)
            self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

            var dumpCallback = self.expectation(description: "dumpCallback callback occurs")
            self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
                XCTAssertNotNil(dump, "dump should not be nil")
                let egoSelf = dump!["self"] as? [String: AnyObject]
                XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
                let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
                XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
                let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
                XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
                let webAccess = stableInfo!["web_access"] as! NSDictionary
                XCTAssertNotNil(webAccess, "webAccess should not be nil")
                XCTAssertEqual(webAccess["clock"] as! Int, i, String(format: "webAccess clock should be %d", i))
                if i % 2 == 0 {
                    XCTAssertEqual(webAccess["value"] as! Int, 1, "webAccess value should be 1")
                } else {
                    XCTAssertEqual(webAccess["value"] as! Int, 0, "webAccess value should be 0")
                }
                let included = dynamicInfo!["included"] as? [String]
                XCTAssertNotNil(included, "included should not be nil")
                XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
                dumpCallback.fulfill()
            }
            self.wait(for: [dumpCallback], timeout: 10)

            dumpCallback = self.expectation(description: "dumpCallback callback occurs")
            self.tphClient.dump(with: try XCTUnwrap(peer2.activeAccount)) { dump, _ in
                XCTAssertNotNil(dump, "dump should not be nil")
                let egoSelf = dump!["self"] as? [String: AnyObject]
                XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
                let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
                XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
                let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
                XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
                let webAccess = stableInfo!["web_access"] as! NSDictionary
                XCTAssertNotNil(webAccess, "webAccess should not be nil")
                XCTAssertEqual(webAccess["clock"] as! Int, i, String(format: "webAccess clock should be %d", i))
                if i % 2 == 0 {
                    XCTAssertEqual(webAccess["value"] as! Int, 1, "webAccess value should be 1")
                } else {
                    XCTAssertEqual(webAccess["value"] as! Int, 0, "webAccess value should be 0")
                }
                let included = dynamicInfo!["included"] as? [String]
                XCTAssertNotNil(included, "included should not be nil")
                XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
                dumpCallback.fulfill()
            }
            self.wait(for: [dumpCallback], timeout: 10)
        }
        self.verifyDatabaseMocks()
    }

    // test very out of date peers
    func testWebAccessInSyncOneVeryOutOfDatePeer() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        let setting = OTAccountSettings()!
        let webAccess = OTWebAccess()!
        webAccess.enabled = true
        setting.webAccess = webAccess

        // Set webAccess
        var setExpectation = self.expectation(description: "webAccess expectation")
        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()
            XCTAssertEqual(newStableInfo.webAccess!.value, setting.webAccess.enabled, "webAccess should be set correctly")
            setExpectation.fulfill()
            return nil
        }

        XCTAssertNoThrow(try cliqueBridge.setAccountSetting(setting), "Should be able to successfully set webAccess setting")
        self.wait(for: [setExpectation], timeout: 10)
        self.fakeCuttlefishServer.updateListener = nil

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        let peer2 = self.makeInitiatorContext(contextID: "peer2", authKitAdapter: self.mockAuthKit2)
        self.assertJoinViaEscrowRecovery(joiningContext: peer2, sponsor: self.cuttlefishContext)

        self.assertEnters(context: peer2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: peer2)

        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertConsidersSelfTrusted(context: peer2)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // peer 2 sets a different setting
        webAccess.enabled = false
        setting.webAccess = webAccess
        setExpectation = self.expectation(description: "webAccess expectation")
        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()
            XCTAssertEqual(newStableInfo.webAccess!.value, setting.webAccess.enabled, "webAccess should be set correctly")

            setExpectation.fulfill()
            return nil
        }

        let secondSetExpectation = self.expectation(description: "webAccess expectation again")
        peer2.rpcSetAccountSetting(setting) { error in
            XCTAssertNil(error, "error should be nil")
            secondSetExpectation.fulfill()
        }
        self.wait(for: [secondSetExpectation], timeout: 10)
        self.wait(for: [setExpectation], timeout: 10)
        self.fakeCuttlefishServer.updateListener = nil

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: peer2)

        webAccess.enabled = true
        setting.webAccess = webAccess
        setExpectation = self.expectation(description: "webAccess expectation")
        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()
            XCTAssertEqual(newStableInfo.webAccess!.value, setting.webAccess.enabled, "webAccess should be set correctly")

            setExpectation.fulfill()
            return nil
        }

        XCTAssertNoThrow(try cliqueBridge.setAccountSetting(setting), "Should be able to successfully set webAccess setting")
        self.wait(for: [setExpectation], timeout: 10)
        self.fakeCuttlefishServer.updateListener = nil

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: peer2)

        var dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            let webAccess = stableInfo!["web_access"] as! NSDictionary
            XCTAssertNotNil(webAccess, "webAccess should not be nil")
            XCTAssertEqual(webAccess["clock"] as! Int, 2, "webAccess clock should be 2")
            XCTAssertEqual(webAccess["value"] as! Int, 1, "webAccess value should be 1")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(peer2.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            let webAccess = stableInfo!["web_access"] as! NSDictionary
            XCTAssertNotNil(webAccess, "webAccess should not be nil")
            XCTAssertEqual(webAccess["clock"] as! Int, 2, "webAccess clock should be 2")
            XCTAssertEqual(webAccess["value"] as! Int, 1, "webAccess value should be 1")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)

        for i in 1...10 {
            // Set webAccess
            let setting = OTAccountSettings()!
            let webAccess = OTWebAccess()!
            if i % 2 == 0 {
                webAccess.enabled = true
            } else {
                webAccess.enabled = false
            }
            setting.webAccess = webAccess

            let setExpectation = self.expectation(description: "webAccess expectation")
            self.fakeCuttlefishServer.updateListener = { request in
                let newStableInfo = request.stableInfoAndSig.stableInfo()
                XCTAssertEqual(newStableInfo.webAccess!.value, setting.webAccess.enabled, "webAccess should be set correctly")

                setExpectation.fulfill()
                return nil
            }

            XCTAssertNoThrow(try cliqueBridge.setAccountSetting(setting), "Should be able to successfully set webAccess setting")
            self.wait(for: [setExpectation], timeout: 10)
            self.fakeCuttlefishServer.updateListener = nil
            self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

            let dumpCallback = self.expectation(description: "dumpCallback callback occurs")
            self.tphClient.dump(with: try XCTUnwrap(self.cuttlefishContext.activeAccount)) { dump, _ in
                XCTAssertNotNil(dump, "dump should not be nil")
                let egoSelf = dump!["self"] as? [String: AnyObject]
                XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
                let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
                XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
                let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
                XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
                let webAccess = stableInfo!["web_access"] as! NSDictionary
                XCTAssertNotNil(webAccess, "webAccess should not be nil")
                XCTAssertEqual(webAccess["clock"] as! Int, i + 2, String(format: "webAccess clock should be %d", i + 2))
                if i % 2 == 0 {
                    XCTAssertEqual(webAccess["value"] as! Int, 1, "webAccess value should be 1")
                } else {
                    XCTAssertEqual(webAccess["value"] as! Int, 0, "webAccess value should be 0")
                }
                let included = dynamicInfo!["included"] as? [String]
                XCTAssertNotNil(included, "included should not be nil")
                XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
                dumpCallback.fulfill()
            }
            self.wait(for: [dumpCallback], timeout: 10)
        }

        self.sendContainerChangeWaitForFetch(context: peer2)
        dumpCallback = self.expectation(description: "dumpCallback callback occurs")
        self.tphClient.dump(with: try XCTUnwrap(peer2.activeAccount)) { dump, _ in
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let dynamicInfo = egoSelf!["dynamicInfo"] as? [String: AnyObject]
            XCTAssertNotNil(dynamicInfo, "dynamicInfo should not be nil")
            let stableInfo = egoSelf!["stableInfo"] as? [String: AnyObject]
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")
            let webAccess = stableInfo!["web_access"] as! NSDictionary
            XCTAssertNotNil(webAccess, "webAccess should not be nil")
            XCTAssertEqual(webAccess["clock"] as! Int, 12, "webAccess clock should be 12")
            XCTAssertEqual(webAccess["value"] as! Int, 1, "webAccess value should be 1")
            let included = dynamicInfo!["included"] as? [String]
            XCTAssertNotNil(included, "included should not be nil")
            XCTAssertEqual(included!.count, 2, "should be 2 peer ids")
            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)
        self.verifyDatabaseMocks()
    }

    func testAttemptSetWebAccessAndFail() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        let setting = OTAccountSettings()!
        let webAccess = OTWebAccess()!
        webAccess.enabled = true
        setting.webAccess = webAccess

        // Attempt to set webAccess
        let setExpectation = self.expectation(description: "webAccess expectation")
        setExpectation.isInverted = true
        self.fakeCuttlefishServer.updateListener = { _ in
            setExpectation.fulfill()
            return nil
        }
        let container = try self.tphClient.getContainer(with: try XCTUnwrap(self.cuttlefishContext.activeAccount))
        container.testDontSetAccountSetting = true

        XCTAssertThrowsError(try cliqueBridge.setAccountSetting(setting), "This should throw an error setting webAccess account setting")
        self.wait(for: [setExpectation], timeout: 1)
        self.fakeCuttlefishServer.updateListener = nil
    }

    func testFetchWalrusAccountSettings() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        let setting = OTAccountSettings()!
        let walrus = OTWalrus()!
        walrus.enabled = true
        setting.walrus = walrus

        // Attempt to set walrus
        let setExpectation = self.expectation(description: "walrus expectation")
        self.fakeCuttlefishServer.updateListener = { _ in
            setExpectation.fulfill()
            return nil
        }

        XCTAssertNoThrow(try cliqueBridge.setAccountSetting(setting), "Should not error setting account setting")
        self.wait(for: [setExpectation], timeout: 1)
        self.fakeCuttlefishServer.updateListener = nil

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        var fetchedSetting: OTAccountSettings?
        XCTAssertNoThrow(fetchedSetting = try cliqueBridge.fetchAccountSettings(), "Should not throw an error fetching account settings")
        XCTAssertNotNil(fetchedSetting, "fetched setting should not be nil")
        XCTAssertTrue(fetchedSetting?.walrus?.enabled == true, "walrus should be enabled")
        XCTAssertTrue(fetchedSetting?.webAccess?.enabled == false, "webAccess should not be enabled")
    }

    func testFetchWebAccessSetting() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        let setting = OTAccountSettings()!
        let webAccess = OTWebAccess()!
        webAccess.enabled = true
        setting.webAccess = webAccess

        // Attempt to set webAccess
        let setExpectation = self.expectation(description: "webAccess expectation")
        self.fakeCuttlefishServer.updateListener = { _ in
            setExpectation.fulfill()
            return nil
        }

        XCTAssertNoThrow(try cliqueBridge.setAccountSetting(setting), "Should not error setting account setting")
        self.wait(for: [setExpectation], timeout: 1)
        self.fakeCuttlefishServer.updateListener = nil

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        var fetchedSetting: OTAccountSettings?
        XCTAssertNoThrow(fetchedSetting = try cliqueBridge.fetchAccountSettings(), "Should not throw an error fetching account settings")
        XCTAssertNotNil(fetchedSetting, "fetched setting should not be nil")
        XCTAssertTrue(fetchedSetting?.walrus?.enabled == false, "walrus should not be enabled")
        XCTAssertTrue(fetchedSetting?.webAccess?.enabled == true, "webAccess should be enabled")
    }

    func testFetchEmptyAccountSettings() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        var fetchedSetting: OTAccountSettings?
        XCTAssertNoThrow(fetchedSetting = try cliqueBridge.fetchAccountSettings(), "Should not throw an error fetching account settings")
        XCTAssertNotNil(fetchedSetting, "fetched setting should not be nil")
        XCTAssertTrue(fetchedSetting?.walrus?.enabled == false, "walrus should not be enabled")
        XCTAssertTrue(fetchedSetting?.webAccess?.enabled == false, "webAccess should not be enabled")
    }

    // this test forces two peers to have the same clock but different values at the same time as a stable changes update
    func testPeerTakesWebAccessStableChangesAfterWebAccessValueConflictButEqualClocks() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        let peer2 = self.makeInitiatorContext(contextID: "peer2", authKitAdapter: self.mockAuthKit2)
        self.assertJoinViaEscrowRecovery(joiningContext: peer2, sponsor: self.cuttlefishContext)

        self.assertEnters(context: peer2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: peer2)

        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertConsidersSelfTrusted(context: peer2)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertEnters(context: peer2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        let setting = OTAccountSettings()!
        let webAccess = OTWebAccess()!
        webAccess.enabled = false
        setting.webAccess = webAccess

        let peer1SetExpectation = self.expectation(description: "peer 1 expectation")
        self.fakeCuttlefishServer.updateListener = { _ in
            peer1SetExpectation.fulfill()
            return nil
        }

        XCTAssertNoThrow(try cliqueBridge.setAccountSetting(setting), "Should not throw an error")
        self.wait(for: [peer1SetExpectation], timeout: 10)
        self.fakeCuttlefishServer.updateListener = nil
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: peer2)

        let container2 = try! self.tphClient.getContainer(with: try XCTUnwrap(peer2.activeAccount))
        let peersFromContainer2 = container2.model.allPeers()
        XCTAssertNotNil(peersFromContainer2, "peersFromContainer2 should not be nil")

        let firstPeer = peersFromContainer2[0]
        let firstPeerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        if firstPeer.permanentInfo.peerID == firstPeerID {
            firstPeer.stableInfo!.webAccess!.value = false
            firstPeer.stableInfo!.webAccess!.clock = 1

            let secondPeer = peersFromContainer2[1]
            secondPeer.stableInfo!.webAccess!.value = true
            secondPeer.stableInfo!.webAccess!.clock = 1
        } else {
            firstPeer.stableInfo!.webAccess!.value = true
            firstPeer.stableInfo!.webAccess!.clock = 1

            let secondPeer = peersFromContainer2[1]
            secondPeer.stableInfo!.webAccess!.value = false
            secondPeer.stableInfo!.webAccess!.clock = 1
        }

        webAccess.enabled = false
        setting.webAccess = webAccess

        let peer2SetExpectation = self.expectation(description: "peer 2 expectation")
        self.fakeCuttlefishServer.updateListener = { request in
            XCTAssertTrue(request.hasStableInfoAndSig, "updateTrust request should have a stableInfo info")
            let newStableInfo = TPPeerStableInfo(data: request.stableInfoAndSig.peerStableInfo, sig: request.stableInfoAndSig.sig)
            XCTAssertNotNil(newStableInfo, "should be able to make a stableInfo info from protobuf")
            XCTAssertFalse(newStableInfo!.webAccess!.value, "webAccess setting should be false")
            peer2SetExpectation.fulfill()
            return nil
        }

        let secondSetExpectation = self.expectation(description: "webAccess expectation again")
        peer2.rpcSetAccountSetting(setting) { error in
            XCTAssertNil(error, "error should be nil")
            secondSetExpectation.fulfill()
        }
        self.wait(for: [secondSetExpectation], timeout: 10)
        self.wait(for: [peer2SetExpectation], timeout: 10)
        self.fakeCuttlefishServer.updateListener = nil

        var retSetting: OTAccountSettings?
        var fetchExpectation = self.expectation(description: "webAccess expectation again")
        peer2.rpcFetchAccountSettings { setting, _ in
            XCTAssertNotNil(setting, "setting should not be nil")
            retSetting = setting
            fetchExpectation.fulfill()
        }
        self.wait(for: [fetchExpectation], timeout: 10)

        XCTAssertFalse(retSetting!.webAccess!.enabled, "webAccess should be disabled")

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: peer2)

        retSetting = nil
        fetchExpectation = self.expectation(description: "webAccess expectation again")
        peer2.rpcFetchAccountSettings { setting, _ in
            XCTAssertNotNil(setting, "setting should not be nil")
            retSetting = setting
            fetchExpectation.fulfill()
        }
        self.wait(for: [fetchExpectation], timeout: 10)

        XCTAssertFalse(retSetting!.walrus!.enabled, "webAccess should not be enabled")

        retSetting = nil
        fetchExpectation = self.expectation(description: "webAccess expectation again")
        self.cuttlefishContext.rpcFetchAccountSettings { setting, _ in
            XCTAssertNotNil(setting, "setting should not be nil")
            retSetting = setting
            fetchExpectation.fulfill()
        }
        self.wait(for: [fetchExpectation], timeout: 10)

        XCTAssertFalse(retSetting!.webAccess!.enabled, "webAccess should not be enabled")

        self.verifyDatabaseMocks()
    }

    // this test forces two peers to have the same clock but different values at the time of a fetch changes and empty stable changes
    func testPeerForcesWebAccessEnabledWhenTwoPeersConflictOnWebAccessValuesAndSameClock() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        let peer2 = self.makeInitiatorContext(contextID: "peer2", authKitAdapter: self.mockAuthKit2)
        self.assertJoinViaEscrowRecovery(joiningContext: peer2, sponsor: self.cuttlefishContext)

        self.assertEnters(context: peer2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: peer2)

        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertConsidersSelfTrusted(context: peer2)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertEnters(context: peer2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        let setting = OTAccountSettings()!
        let webAccess = OTWebAccess()!
        webAccess.enabled = true
        setting.webAccess = webAccess

        let peer1SetExpectation = self.expectation(description: "peer 1 expectation")
        self.fakeCuttlefishServer.updateListener = { _ in
            peer1SetExpectation.fulfill()
            return nil
        }

        XCTAssertNoThrow(try cliqueBridge.setAccountSetting(setting), "Should not throw an error")
        self.wait(for: [peer1SetExpectation], timeout: 10)
        self.fakeCuttlefishServer.updateListener = nil
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: peer2)

        let container2 = try! self.tphClient.getContainer(with: try XCTUnwrap(peer2.activeAccount))
        let peersFromContainer2 = container2.model.allPeers()
        XCTAssertNotNil(peersFromContainer2, "peersFromContainer2 should not be nil")

        let firstPeer = peersFromContainer2[0]
        let firstPeerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        if firstPeer.permanentInfo.peerID == firstPeerID {
            firstPeer.stableInfo!.webAccess!.value = false
            firstPeer.stableInfo!.webAccess!.clock = 1

            let secondPeer = peersFromContainer2[1]
            secondPeer.stableInfo!.webAccess!.value = true
            secondPeer.stableInfo!.webAccess!.clock = 1
        } else {
            firstPeer.stableInfo!.webAccess!.value = true
            firstPeer.stableInfo!.webAccess!.clock = 1

            let secondPeer = peersFromContainer2[1]
            secondPeer.stableInfo!.webAccess!.value = false
            secondPeer.stableInfo!.webAccess!.clock = 1
        }

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: peer2)

        var retSetting: OTAccountSettings?
        var fetchExpectation = self.expectation(description: "webAccess expectation again")
        peer2.rpcFetchAccountSettings { setting, _ in
            XCTAssertNotNil(setting, "setting should not be nil")
            retSetting = setting
            fetchExpectation.fulfill()
        }
        self.wait(for: [fetchExpectation], timeout: 10)

        XCTAssertTrue(retSetting!.webAccess!.enabled, "webAccess should be enabled")

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: peer2)

        retSetting = nil
        fetchExpectation = self.expectation(description: "webAccess expectation again")
        peer2.rpcFetchAccountSettings { setting, _ in
            XCTAssertNotNil(setting, "setting should not be nil")
            retSetting = setting
            fetchExpectation.fulfill()
        }
        self.wait(for: [fetchExpectation], timeout: 10)

        XCTAssertTrue(retSetting!.webAccess!.enabled, "webAccess should be enabled")

        retSetting = nil
        fetchExpectation = self.expectation(description: "webAccess expectation again")
        self.cuttlefishContext.rpcFetchAccountSettings { setting, _ in
            XCTAssertNotNil(setting, "setting should not be nil")
            retSetting = setting
            fetchExpectation.fulfill()
        }
        self.wait(for: [fetchExpectation], timeout: 10)

        XCTAssertTrue(retSetting!.webAccess!.enabled, "webAccess should be enabled")

        self.verifyDatabaseMocks()
    }

    func testFetchAccountWideSettingsOnePeerSet() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        let setting = OTAccountSettings()!
        let walrus = OTWalrus()!
        walrus.enabled = true
        setting.walrus = walrus

        // Attempt to set walrus
        let setExpectation = self.expectation(description: "walrus expectation")
        self.fakeCuttlefishServer.updateListener = { _ in
            setExpectation.fulfill()
            return nil
        }

        XCTAssertNoThrow(try cliqueBridge.setAccountSetting(setting), "Should not error setting account setting")
        self.wait(for: [setExpectation], timeout: 1)
        self.fakeCuttlefishServer.updateListener = nil

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        var fetchedSetting: OTAccountSettings?
        XCTAssertNoThrow(fetchedSetting = try OctagonTrustCliqueBridge.fetchAccountWideSettings(self.otcliqueContext), "Should not throw an error fetching account settings")
        XCTAssertNotNil(fetchedSetting, "fetched setting should not be nil")
        XCTAssertTrue(fetchedSetting?.walrus?.enabled == true, "walrus should be enabled")
        XCTAssertTrue(fetchedSetting?.webAccess?.enabled == false, "webAccess should not be enabled")
    }

    func testFetchAccountWideSettingsTwoPeerSet() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        let setting = OTAccountSettings()!
        let walrus = OTWalrus()!
        walrus.enabled = true
        setting.walrus = walrus

        // Attempt to set walrus
        var setExpectation = self.expectation(description: "walrus expectation")
        self.fakeCuttlefishServer.updateListener = { _ in
            setExpectation.fulfill()
            return nil
        }

        XCTAssertNoThrow(try cliqueBridge.setAccountSetting(setting), "Should not error setting account setting")
        self.wait(for: [setExpectation], timeout: 1)
        self.fakeCuttlefishServer.updateListener = nil

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        let joiningContext = self.makeInitiatorContext(contextID: "peer2", authKitAdapter: self.mockAuthKit2)
        self.assertJoinViaEscrowRecovery(joiningContext: joiningContext, sponsor: self.cuttlefishContext)

        self.assertAllCKKSViewsUpload(tlkShares: 1)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.sendContainerChangeWaitForFetch(context: joiningContext)

        self.assertConsidersSelfTrusted(context: joiningContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        walrus.enabled = false
        setting.walrus = walrus

        setExpectation = self.expectation(description: "second peer sets walrus expectation")

        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()
            XCTAssertEqual(newStableInfo.walrusSetting!.value, setting.walrus.enabled, "walrus should be set correctly")

            setExpectation.fulfill()
            return nil
        }

        let secondSetExpectation = self.expectation(description: "walrus expectation again")
        joiningContext.rpcSetAccountSetting(setting) { error in
            XCTAssertNil(error, "error should be nil")
            secondSetExpectation.fulfill()
        }

        self.wait(for: [secondSetExpectation], timeout: 10)
        self.wait(for: [setExpectation], timeout: 10)
        self.fakeCuttlefishServer.updateListener = nil

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.sendContainerChangeWaitForFetch(context: joiningContext)

        var fetchedSetting: OTAccountSettings?
        XCTAssertNoThrow(fetchedSetting = try OctagonTrustCliqueBridge.fetchAccountWideSettings(self.otcliqueContext), "Should not throw an error fetching account settings")
        XCTAssertNotNil(fetchedSetting, "fetched setting should not be nil")
        XCTAssertTrue(fetchedSetting?.walrus?.enabled == false, "walrus should be disabled")
        XCTAssertTrue(fetchedSetting?.webAccess?.enabled == false, "webAccess should not be enabled")

        let secondPeerContext = OTConfigurationContext()
        secondPeerContext.context = "peer2"
        secondPeerContext.dsid = "1234"
        secondPeerContext.altDSID = try XCTUnwrap(self.mockAuthKit.primaryAltDSID())
        secondPeerContext.otControl = self.otControl

        XCTAssertNoThrow(fetchedSetting = try OctagonTrustCliqueBridge.fetchAccountWideSettings(secondPeerContext), "Should not throw an error fetching account settings")
        XCTAssertNotNil(fetchedSetting, "fetched setting should not be nil")
        XCTAssertTrue(fetchedSetting?.walrus?.enabled == false, "walrus should be disabled")
        XCTAssertTrue(fetchedSetting?.webAccess?.enabled == false, "webAccess should not be enabled")
    }

    func testFetchAccountWideSettingsNoPeers() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        var fetchedSetting: OTAccountSettings?
        XCTAssertThrowsError(fetchedSetting = try OctagonTrustCliqueBridge.fetchAccountWideSettings(self.otcliqueContext), "Should throw an error fetching account settings") { error in 
            let nserror = error as NSError
            XCTAssertEqual(nserror.code, OctagonError.noAccountSettingsSet.rawValue, "error code")
            XCTAssertEqual(nserror.domain, OctagonErrorDomain, "error domain")
        }
        XCTAssertNil(fetchedSetting, "fetched setting should be nil")
    }

    func testFetchAccountWideSettingsForceFalse() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        var fetchedSetting: OTAccountSettings?
        XCTAssertThrowsError(fetchedSetting = try OctagonTrustCliqueBridge.fetchAccountWideSettings(withForceFetch: false, configuration:self.otcliqueContext), "Should throw an error fetching account settings") { error in 
            let nserror = error as NSError
            XCTAssertEqual(nserror.code, OctagonError.noAccountSettingsSet.rawValue, "error code")
            XCTAssertEqual(nserror.domain, OctagonErrorDomain, "error domain")
        }
        XCTAssertNil(fetchedSetting, "fetched setting should be nil")
    }

    func testFetchAccountWideSettingsForceTrue() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        var fetchedSetting: OTAccountSettings?
        XCTAssertThrowsError(fetchedSetting = try OctagonTrustCliqueBridge.fetchAccountWideSettings(withForceFetch: true, configuration:self.otcliqueContext), "Should throw an error fetching account settings") { error in 
            let nserror = error as NSError
            XCTAssertEqual(nserror.code, OctagonError.noAccountSettingsSet.rawValue, "error code")
            XCTAssertEqual(nserror.domain, OctagonErrorDomain, "error domain")
        }
        XCTAssertNil(fetchedSetting, "fetched setting should be nil")
    }

    func testFetchAccountWideSettingsForceTrueNetworkFailure() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let fetchChangesExpectations = self.expectation(description: "fetch changes")
        self.fakeCuttlefishServer.fetchChangesListener = { _ in
            fetchChangesExpectations.fulfill()
            self.fakeCuttlefishServer.fetchChangesListener = nil
            return NSError(domain: NSURLErrorDomain, code: NSURLErrorNotConnectedToInternet, userInfo: nil)
        }

        var fetchedSetting: OTAccountSettings?
        XCTAssertThrowsError(fetchedSetting = try OctagonTrustCliqueBridge.fetchAccountWideSettings(withForceFetch: true, configuration:self.otcliqueContext), "Should throw an error fetching account settings") { error in 
            let nserror = error as NSError
            XCTAssertEqual(nserror.code, NSURLErrorNotConnectedToInternet, "error code")
            XCTAssertEqual(nserror.domain, NSURLErrorDomain, "error domain")
        }
        XCTAssertNil(fetchedSetting, "fetched setting should be nil")
        self.wait(for: [fetchChangesExpectations], timeout: 10)
    }

    func testFailToSetSettingsWithNetworkFailure() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        do {
            let setting = OTAccountSettings()!
            setting.walrus = OTWalrus()!
            setting.walrus.enabled = true

            let setWalrusExpectation = self.expectation(description: "walrus expectation")
            self.fakeCuttlefishServer.updateListener = { _ in
                setWalrusExpectation.fulfill()
                return NSError(domain: NSURLErrorDomain, code: NSURLErrorNotConnectedToInternet, userInfo: nil)
            }

            XCTAssertThrowsError(try cliqueBridge.setAccountSetting(setting), "Should not be able to successfully set walrus setting when there's no network connectivity") { error in
                let nserror = error as NSError
                XCTAssertEqual(nserror.domain, OctagonErrorDomain, "Error should be OctagonErrorDomain")
                XCTAssertEqual(nserror.code, OctagonError.failedToSetWalrus.rawValue, "Error code should be OctagonErrorFailedToSetWalrus")

                let underlying = nserror.userInfo[NSUnderlyingErrorKey] as? NSError
                XCTAssertNotNil(underlying, "Should have an underlying error")
                XCTAssertEqual(underlying?.domain ?? "", NSURLErrorDomain, "Underlying error should be NSURLErrorDomain")
                XCTAssertEqual(underlying?.code ?? 0, NSURLErrorNotConnectedToInternet, "Underlying error code should be NSURLErrorNotConnectedToInternet")
            }
            self.wait(for: [setWalrusExpectation], timeout: 10)
            self.fakeCuttlefishServer.updateListener = nil
        }

        do {
            let setting = OTAccountSettings()!
            setting.webAccess = OTWebAccess()!
            setting.webAccess.enabled = true

            let setWebAccessExpectation = self.expectation(description: "walrus expectation")
            self.fakeCuttlefishServer.updateListener = { _ in
                setWebAccessExpectation.fulfill()
                return NSError(domain: NSURLErrorDomain, code: NSURLErrorNotConnectedToInternet, userInfo: nil)
            }

            XCTAssertThrowsError(try cliqueBridge.setAccountSetting(setting), "Should not be able to successfully set web access setting when there's no network connectivity") { error in
                let nserror = error as NSError
                XCTAssertEqual(nserror.domain, OctagonErrorDomain, "Error should be OctagonErrorDomain")
                XCTAssertEqual(nserror.code, OctagonError.failedToSetWebAccess.rawValue, "Error code should be OctagonErrorFailedToSetWebAccess")

                let underlying = nserror.userInfo[NSUnderlyingErrorKey] as? NSError
                XCTAssertNotNil(underlying, "Should have an underlying error")
                XCTAssertEqual(underlying?.domain ?? "", NSURLErrorDomain, "Underlying error should be NSURLErrorDomain")
                XCTAssertEqual(underlying?.code ?? 0, NSURLErrorNotConnectedToInternet, "Underlying error code should be NSURLErrorNotConnectedToInternet")
            }
            self.wait(for: [setWebAccessExpectation], timeout: 10)
            self.fakeCuttlefishServer.updateListener = nil
        }
    }

    func testResetProtectedDataPersistAccountSettingsWalrusEnabled() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        let firstCliqueIdentifier = clique.cliqueMemberIdentifier

        let setting = OTAccountSettings()!
        let walrus = OTWalrus()!
        walrus.enabled = true
        setting.walrus = walrus

        // Attempt to set walrus
        let setExpectation = self.expectation(description: "walrus expectation")
        self.fakeCuttlefishServer.updateListener = { _ in
            setExpectation.fulfill()
            return nil
        }

        XCTAssertNoThrow(try cliqueBridge.setAccountSetting(setting), "Should not error setting account setting")
        self.wait(for: [setExpectation], timeout: 1)
        self.fakeCuttlefishServer.updateListener = nil

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        var fetchedSetting: OTAccountSettings?
        XCTAssertNoThrow(fetchedSetting = try cliqueBridge.fetchAccountSettings(), "Should not throw an error fetching account settings")
        XCTAssertNotNil(fetchedSetting, "fetched setting should not be nil")
        XCTAssertTrue(fetchedSetting?.walrus?.enabled == true, "walrus should be enabled")
        XCTAssertTrue(fetchedSetting?.webAccess?.enabled == false, "webAccess should not be enabled")

        let otcliqueContext = OTConfigurationContext()
        otcliqueContext.context = self.cuttlefishContext.contextID
        otcliqueContext.altDSID = try XCTUnwrap(self.cuttlefishContext.activeAccount?.altDSID)
        otcliqueContext.otControl = self.otControl
        otcliqueContext.dsid = "13453464"
        otcliqueContext.authenticationAppleID = "appleID"
        otcliqueContext.passwordEquivalentToken = "petpetpetpetpet"
        otcliqueContext.ckksControl = self.ckksControl
        otcliqueContext.sbd = OTMockSecureBackup(bottleID: nil, entropy: nil)

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

        var reFetchedSettings: OTAccountSettings?
        let newCliqueBridge = OctagonTrustCliqueBridge(clique: newClique)

        XCTAssertNoThrow(reFetchedSettings = try newCliqueBridge.fetchAccountSettings(), "Should not throw an error fetching account settings")
        XCTAssertNotNil(reFetchedSettings, "fetched setting should not be nil")

        XCTAssertEqual(fetchedSetting, reFetchedSettings, "account settings should be equal")
        self.verifyDatabaseMocks()
    }

    func testResetProtectedDataPersistAccountSettingsWebAccessEnabled() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        let firstCliqueIdentifier = clique.cliqueMemberIdentifier

        let setting = OTAccountSettings()!
        let webAccess = OTWebAccess()!
        webAccess.enabled = true
        setting.webAccess = webAccess

        // Attempt to set webaccess
        let setExpectation = self.expectation(description: "webaccess expectation")
        self.fakeCuttlefishServer.updateListener = { _ in
            setExpectation.fulfill()
            return nil
        }

        XCTAssertNoThrow(try cliqueBridge.setAccountSetting(setting), "Should not error setting account setting")
        self.wait(for: [setExpectation], timeout: 1)
        self.fakeCuttlefishServer.updateListener = nil

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        var fetchedSetting: OTAccountSettings?
        XCTAssertNoThrow(fetchedSetting = try cliqueBridge.fetchAccountSettings(), "Should not throw an error fetching account settings")
        XCTAssertNotNil(fetchedSetting, "fetched setting should not be nil")
        XCTAssertTrue(fetchedSetting?.walrus?.enabled == false, "walrus should not be enabled")
        XCTAssertTrue(fetchedSetting?.webAccess?.enabled == true, "webAccess should be enabled")

        let otcliqueContext = OTConfigurationContext()
        otcliqueContext.context = self.cuttlefishContext.contextID
        otcliqueContext.altDSID = try XCTUnwrap(self.cuttlefishContext.activeAccount?.altDSID)
        otcliqueContext.otControl = self.otControl
        otcliqueContext.dsid = "13453464"
        otcliqueContext.authenticationAppleID = "appleID"
        otcliqueContext.passwordEquivalentToken = "petpetpetpetpet"
        otcliqueContext.ckksControl = self.ckksControl
        otcliqueContext.sbd = OTMockSecureBackup(bottleID: nil, entropy: nil)

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

        var reFetchedSettings: OTAccountSettings?
        let newCliqueBridge = OctagonTrustCliqueBridge(clique: newClique)

        XCTAssertNoThrow(reFetchedSettings = try newCliqueBridge.fetchAccountSettings(), "Should not throw an error fetching account settings")
        XCTAssertNotNil(reFetchedSettings, "fetched setting should not be nil")

        XCTAssertEqual(fetchedSetting, reFetchedSettings, "account settings should be equal")
        self.verifyDatabaseMocks()
    }

    func testResetProtectedDataPersistAccountSettingsWebAccessAndWalrusEnabled() throws {
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        let firstCliqueIdentifier = clique.cliqueMemberIdentifier

        let setting = OTAccountSettings()!
        let webAccess = OTWebAccess()!
        webAccess.enabled = true
        setting.webAccess = webAccess

        let walrus = OTWalrus()!
        walrus.enabled = true
        setting.walrus = walrus

        // Attempt to set webaccess
        let setExpectation = self.expectation(description: "webaccess expectation")
        self.fakeCuttlefishServer.updateListener = { _ in
            setExpectation.fulfill()
            return nil
        }

        XCTAssertNoThrow(try cliqueBridge.setAccountSetting(setting), "Should not error setting account setting")
        self.wait(for: [setExpectation], timeout: 1)
        self.fakeCuttlefishServer.updateListener = nil

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        var fetchedSetting: OTAccountSettings?
        XCTAssertNoThrow(fetchedSetting = try cliqueBridge.fetchAccountSettings(), "Should not throw an error fetching account settings")
        XCTAssertNotNil(fetchedSetting, "fetched setting should not be nil")
        XCTAssertTrue(fetchedSetting?.walrus?.enabled == true, "walrus should be enabled")
        XCTAssertTrue(fetchedSetting?.webAccess?.enabled == true, "webAccess should be enabled")

        let otcliqueContext = OTConfigurationContext()
        otcliqueContext.context = self.cuttlefishContext.contextID
        otcliqueContext.altDSID = try XCTUnwrap(self.cuttlefishContext.activeAccount?.altDSID)
        otcliqueContext.otControl = self.otControl
        otcliqueContext.dsid = "13453464"
        otcliqueContext.authenticationAppleID = "appleID"
        otcliqueContext.passwordEquivalentToken = "petpetpetpetpet"
        otcliqueContext.ckksControl = self.ckksControl
        otcliqueContext.sbd = OTMockSecureBackup(bottleID: nil, entropy: nil)

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

        var reFetchedSettings: OTAccountSettings?
        let newCliqueBridge = OctagonTrustCliqueBridge(clique: newClique)

        XCTAssertNoThrow(reFetchedSettings = try newCliqueBridge.fetchAccountSettings(), "Should not throw an error fetching account settings")
        XCTAssertNotNil(reFetchedSettings, "fetched setting should not be nil")

        XCTAssertEqual(fetchedSetting, reFetchedSettings, "account settings should be equal")
        self.verifyDatabaseMocks()
    }

    // Test:
    // peer A           peer B          peer C
    //                  walrus on
    //                  walrus off
    // walrus on
    //                                 join RK
    //                                 check W

    func testWalrusAfterRKJoinDifferentPeer() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        SecCKKSSetTestSkipTLKShareHealing(true)

        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)

        let recoveryKey = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        OctagonSetSOSFeatureEnabled(true)

        let createRecoveryExpectation = self.expectation(description: "createRecoveryExpectation returns")
        self.manager.createRecoveryKey(self.otcontrolArgumentsFor(context: self.cuttlefishContext), recoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            createRecoveryExpectation.fulfill()
        }
        self.wait(for: [createRecoveryExpectation], timeout: 10)

        try self.putRecoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        let peer2 = self.makeInitiatorContext(contextID: "peer2", authKitAdapter: self.mockAuthKit2)
        self.assertJoinViaEscrowRecovery(joiningContext: peer2, sponsor: self.cuttlefishContext)

        let setting = OTAccountSettings()!
        let walrus = OTWalrus()!
        walrus.enabled = true
        setting.walrus = walrus

        // Set walrus on peer2
        let setExpectation = self.expectation(description: "walrus set expectation")
        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()
            XCTAssertEqual(newStableInfo.walrusSetting!.value, setting.walrus.enabled, "walrus should be set correctly")

            setExpectation.fulfill()
            return nil
        }

        let peer2SetExpectation1 = self.expectation(description: "peer2 first walrus set")
        peer2.rpcSetAccountSetting(setting) { error in
            XCTAssertNil(error, "error should be nil")
            peer2SetExpectation1.fulfill()
        }
        self.wait(for: [peer2SetExpectation1], timeout: 10)
        self.wait(for: [setExpectation], timeout: 10)
        self.fakeCuttlefishServer.updateListener = nil

        // Unset walrus on peer2
        walrus.enabled = false

        let unsetExpectation = self.expectation(description: "walrus unset expectation")
        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()
            XCTAssertEqual(newStableInfo.walrusSetting!.value, setting.walrus.enabled, "walrus should be set correctly")

            unsetExpectation.fulfill()
            return nil
        }

        let peer2SetExpectation2 = self.expectation(description: "peer2 walrus unset")
        peer2.rpcSetAccountSetting(setting) { error in
            XCTAssertNil(error, "error should be nil")
            peer2SetExpectation2.fulfill()
        }
        self.wait(for: [peer2SetExpectation2], timeout: 10)
        self.wait(for: [unsetExpectation], timeout: 10)
        self.fakeCuttlefishServer.updateListener = nil

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        // Set walrus on peer1
        walrus.enabled = true
        let set2Expectation = self.expectation(description: "walrus set 2 expectation")
        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()
            XCTAssertEqual(newStableInfo.walrusSetting!.value, setting.walrus.enabled, "walrus should be set correctly")

            set2Expectation.fulfill()
            return nil
        }
        XCTAssertNoThrow(try cliqueBridge.setAccountSetting(setting), "Should be able to successfully set walrus setting")
        self.wait(for: [set2Expectation], timeout: 10)
        self.fakeCuttlefishServer.updateListener = nil
        
        // Now, join from a new device
        let recoveryContext = self.makeInitiatorContext(contextID: "recovery", authKitAdapter: self.mockAuthKit2)

        recoveryContext.startOctagonStateMachine()
        self.assertEnters(context: recoveryContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.sendContainerChangeWaitForUntrustedFetch(context: recoveryContext)

        let joinWithRecoveryKeyExpectation = self.expectation(description: "joinWithRecoveryKey callback occurs")
        recoveryContext.join(withRecoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithRecoveryKeyExpectation], timeout: 20)

        var retSettings: OTAccountSettings?
        let fetchExpectation = self.expectation(description: "account setting fetch")
        self.cuttlefishContext.rpcFetchAccountSettings { settings, error in
            XCTAssertNotNil(setting, "setting should not be nil")
            XCTAssertNil(error, "error should be nil")
            retSettings = settings
            fetchExpectation.fulfill()            
        }
        self.wait(for: [fetchExpectation], timeout: 10)

        XCTAssertTrue(retSettings!.walrus!.enabled, "walrus should be enabled")

        self.verifyDatabaseMocks()
    }

    // Test:
    // peer A           peer B          peer C
    //                  walrus on
    //                  walrus off
    // walrus on
    // join RK
    // check W

    func testWalrusAfterRKJoinSamePeer() throws {
        try self.skipOnRecoveryKeyNotSupported()
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        SecCKKSSetTestSkipTLKShareHealing(true)

        self.putFakeKeyHierarchiesInCloudKit()
        try self.putSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)

        let recoveryKey = SecPasswordGenerate(SecPasswordType(kSecPasswordTypeiCloudRecoveryKey), nil, nil)! as String
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        OctagonSetSOSFeatureEnabled(true)

        let createRecoveryExpectation = self.expectation(description: "createRecoveryExpectation returns")
        self.manager.createRecoveryKey(self.otcontrolArgumentsFor(context: self.cuttlefishContext), recoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            createRecoveryExpectation.fulfill()
        }
        self.wait(for: [createRecoveryExpectation], timeout: 10)

        try self.putRecoveryKeyTLKSharesInCloudKit(recoveryKey: recoveryKey, salt: try XCTUnwrap(self.mockAuthKit.primaryAltDSID()))
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        let peer2 = self.makeInitiatorContext(contextID: "peer2", authKitAdapter: self.mockAuthKit2)
        self.assertJoinViaEscrowRecovery(joiningContext: peer2, sponsor: self.cuttlefishContext)

        let setting = OTAccountSettings()!
        let walrus = OTWalrus()!
        walrus.enabled = true
        setting.walrus = walrus

        // Set walrus on peer2
        let setExpectation = self.expectation(description: "walrus set expectation")
        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()
            XCTAssertEqual(newStableInfo.walrusSetting!.value, setting.walrus.enabled, "walrus should be set correctly")

            setExpectation.fulfill()
            return nil
        }

        let peer2SetExpectation1 = self.expectation(description: "peer2 first walrus set")
        peer2.rpcSetAccountSetting(setting) { error in
            XCTAssertNil(error, "error should be nil")
            peer2SetExpectation1.fulfill()
        }
        self.wait(for: [peer2SetExpectation1], timeout: 10)
        self.wait(for: [setExpectation], timeout: 10)
        self.fakeCuttlefishServer.updateListener = nil

        // Unset walrus on peer2
        walrus.enabled = false

        let unsetExpectation = self.expectation(description: "walrus unset expectation")
        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()
            XCTAssertEqual(newStableInfo.walrusSetting!.value, setting.walrus.enabled, "walrus should be set correctly")

            unsetExpectation.fulfill()
            return nil
        }

        let peer2SetExpectation2 = self.expectation(description: "peer2 walrus unset")
        peer2.rpcSetAccountSetting(setting) { error in
            XCTAssertNil(error, "error should be nil")
            peer2SetExpectation2.fulfill()
        }
        self.wait(for: [peer2SetExpectation2], timeout: 10)
        self.wait(for: [unsetExpectation], timeout: 10)
        self.fakeCuttlefishServer.updateListener = nil

        let clique = self.cliqueFor(context: self.cuttlefishContext)
        let cliqueBridge = OctagonTrustCliqueBridge(clique: clique)

        // Set walrus on peer1
        walrus.enabled = true
        let set2Expectation = self.expectation(description: "walrus set 2 expectation")
        self.fakeCuttlefishServer.updateListener = { request in
            let newStableInfo = request.stableInfoAndSig.stableInfo()
            XCTAssertEqual(newStableInfo.walrusSetting!.value, setting.walrus.enabled, "walrus should be set correctly")

            set2Expectation.fulfill()
            return nil
        }
        XCTAssertNoThrow(try cliqueBridge.setAccountSetting(setting), "Should be able to successfully set walrus setting")
        self.wait(for: [set2Expectation], timeout: 10)
        self.fakeCuttlefishServer.updateListener = nil
        
        // Now, join from original context

        let joinWithRecoveryKeyExpectation = self.expectation(description: "joinWithRecoveryKey callback occurs")
        self.cuttlefishContext.join(withRecoveryKey: recoveryKey) { error in
            XCTAssertNil(error, "error should be nil")
            joinWithRecoveryKeyExpectation.fulfill()
        }
        self.wait(for: [joinWithRecoveryKeyExpectation], timeout: 20)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        var retSettings: OTAccountSettings?
        let fetchExpectation = self.expectation(description: "account setting fetch")
        self.cuttlefishContext.rpcFetchAccountSettings { settings, error in
            XCTAssertNotNil(setting, "setting should not be nil")
            XCTAssertNil(error, "error should be nil")
            retSettings = settings
            fetchExpectation.fulfill()            
        }
        self.wait(for: [fetchExpectation], timeout: 10)

        XCTAssertTrue(retSettings!.walrus!.enabled, "walrus should be enabled")

        self.verifyDatabaseMocks()
    }
}

#endif
