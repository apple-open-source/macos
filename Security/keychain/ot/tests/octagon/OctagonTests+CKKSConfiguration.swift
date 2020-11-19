import Foundation

class OctagonCKKSConfigurationTestsPolicyDisabled: OctagonTestsBase {
    // Pre-configure some things, so the OctagonTests will only operate on these views
    override func setUp() {
        if self.mockDeviceInfo == nil {
            let actualDeviceAdapter = OTDeviceInformationActualAdapter()
            self.mockDeviceInfo = OTMockDeviceInfoAdapter(modelID: actualDeviceAdapter.modelID(),
                                                          deviceName: actualDeviceAdapter.deviceName(),
                                                          serialNumber: NSUUID().uuidString,
                                                          osVersion: actualDeviceAdapter.osVersion())
        }

        // With the policy disabled, we only want to operate on a few zones
        if self.mockDeviceInfo.mockModelID.contains("AppleTV") {
            self.intendedCKKSZones = Set([
                CKRecordZone.ID(zoneName: "LimitedPeersAllowed"),
            ])
        } else {
            self.intendedCKKSZones = Set([
                CKRecordZone.ID(zoneName: "LimitedPeersAllowed"),
                CKRecordZone.ID(zoneName: "Manatee"),
            ])
        }

        self.setCKKSViewsFromPolicyToNo = true

        super.setUp()

        XCTAssertFalse(self.cuttlefishContext.viewManager!.useCKKSViewsFromPolicy(), "CKKS should not be configured to listen to policy-based views")
    }

    func testMergedViewListOff() throws {
        XCTAssertFalse(self.cuttlefishContext.viewManager!.useCKKSViewsFromPolicy(), "CKKS should not be configured to listen to policy-based views")

        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let viewList = self.cuttlefishContext.viewManager!.viewList()
        #if !os(tvOS)
        let expected = Set<String>(["Manatee", "LimitedPeersAllowed"])
        #else
        let expected = Set<String>(["LimitedPeersAllowed"])
        #endif
        XCTAssertEqual(expected, viewList)
    }
}

class OctagonCKKSConfigurationTestsPolicyEnabled: OctagonTestsBase {
    override func setUp() {
        if self.mockDeviceInfo == nil {
            let actualDeviceAdapter = OTDeviceInformationActualAdapter()
            self.mockDeviceInfo = OTMockDeviceInfoAdapter(modelID: actualDeviceAdapter.modelID(),
                                                          deviceName: actualDeviceAdapter.deviceName(),
                                                          serialNumber: NSUUID().uuidString,
                                                          osVersion: actualDeviceAdapter.osVersion())
        }

        // Most tests will use a much smaller list of views. But not us! Go wild!
        if self.mockDeviceInfo.mockModelID.contains("AppleTV") {
            self.intendedCKKSZones = Set([
                CKRecordZone.ID(zoneName: "Home"),
                self.limitedPeersAllowedZoneID!,
                CKRecordZone.ID(zoneName: "WiFi"),
            ])
        } else {
            self.intendedCKKSZones = Set([
                CKRecordZone.ID(zoneName: "ApplePay"),
                CKRecordZone.ID(zoneName: "Applications"),
                CKRecordZone.ID(zoneName: "AutoUnlock"),
                CKRecordZone.ID(zoneName: "Backstop"),
                CKRecordZone.ID(zoneName: "CreditCards"),
                CKRecordZone.ID(zoneName: "DevicePairing"),
                CKRecordZone.ID(zoneName: "Engram"),
                CKRecordZone.ID(zoneName: "Health"),
                CKRecordZone.ID(zoneName: "Home"),
                CKRecordZone.ID(zoneName: "LimitedPeersAllowed"),
                CKRecordZone.ID(zoneName: "Manatee"),
                CKRecordZone.ID(zoneName: "Passwords"),
                CKRecordZone.ID(zoneName: "ProtectedCloudStorage"),
                CKRecordZone.ID(zoneName: "SecureObjectSync"),
                CKRecordZone.ID(zoneName: "WiFi"),
             ])
         }

        super.setUp()
    }

    func testMergedViewListOn() throws {
        XCTAssertTrue(self.cuttlefishContext.viewManager!.useCKKSViewsFromPolicy(), "CKKS should be configured to listen to policy-based views")

        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let viewList = self.cuttlefishContext.viewManager!.viewList()

        #if !os(tvOS)
        let expected = Set<String>([
            "ApplePay",
            "Applications",
            "AutoUnlock",
            "Backstop",
            "CreditCards",
            "DevicePairing",
            "Engram",
            "Health",
            "Home",
            "LimitedPeersAllowed",
            "Manatee",
            "Passwords",
            "ProtectedCloudStorage",
            "SecureObjectSync",
            "WiFi",
        ])
        #else
        let expected = Set<String>(["LimitedPeersAllowed",
                                    "Home",
                                    "WiFi", ])
        #endif
        XCTAssertEqual(expected, viewList)
    }

    func testPolicyResetRPC() throws {
        XCTAssertTrue(self.cuttlefishContext.viewManager!.useCKKSViewsFromPolicy(), "CKKS should be configured to listen to policy-based views")

        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        XCTAssertNotNil(self.injectedManager?.policy, "Should have given CKKS a TPPolicy during initialization")
        XCTAssertEqual(self.injectedManager?.policy?.version, prevailingPolicyVersion, "Policy given to CKKS should be prevailing policy")

        self.injectedManager!.resetSyncingPolicy()
        XCTAssertNil(self.injectedManager?.policy, "CKKS policy should be reset (by the test)")

        self.otControl.refetchCKKSPolicy(nil, contextID: self.cuttlefishContext.contextID) { error in
            XCTAssertNil(error, "Should be no error refetching the CKKS policy")
        }

        XCTAssertNotNil(self.injectedManager?.policy, "Should have given CKKS a TPPolicy during refetch")
        XCTAssertEqual(self.injectedManager?.policy?.version, prevailingPolicyVersion, "Policy given to CKKS should be prevailing policy")
    }
}
