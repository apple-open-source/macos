import Foundation

class OctagonCKKSConfigurationTestsPolicyEnabledTests: OctagonTestsBase {
    override func setUp() {
        if self.mockDeviceInfo == nil {
            let actualDeviceAdapter = OTDeviceInformationActualAdapter()
            self.mockDeviceInfo = OTMockDeviceInfoAdapter(modelID: actualDeviceAdapter.modelID(),
                                                          deviceName: actualDeviceAdapter.deviceName(),
                                                          serialNumber: NSUUID().uuidString,
                                                          osVersion: actualDeviceAdapter.osVersion())
        }

        // Most tests will use a much smaller list of views. But not us! Go wild!
        if self.mockDeviceInfo.mockModelID.contains("AppleTV") || self.mockDeviceInfo.mockModelID.contains("AudioAccessory") {
            self.intendedCKKSZones = Set([
                CKRecordZone.ID(zoneName: "Home"),
                CKRecordZone.ID(zoneName: "LimitedPeersAllowed"),
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
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        let viewList = try XCTUnwrap(self.cuttlefishContext.ckks).viewList

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
        self.startCKAccountStatusMock()
        self.assertResetAndBecomeTrustedInDefaultContext()

        XCTAssertNotNil(self.defaultCKKS.syncingPolicy, "Should have given CKKS a TPPolicy during initialization")
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.version, prevailingPolicyVersion, "Policy given to CKKS should be prevailing policy")

        self.defaultCKKS = self.injectedManager!.restartCKKSAccountSyncWithoutSettingPolicy(self.defaultCKKS)
        self.cuttlefishContext.reset(ckks: self.defaultCKKS)
        XCTAssertNil(self.defaultCKKS.syncingPolicy, "CKKS policy should be reset (by the test)")

        self.otControl.refetchCKKSPolicy(nil, contextID: self.cuttlefishContext.contextID) { error in
            XCTAssertNil(error, "Should be no error refetching the CKKS policy")
        }

        XCTAssertNotNil(self.defaultCKKS.syncingPolicy, "Should have given CKKS a TPPolicy during refetch")
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.version, prevailingPolicyVersion, "Policy given to CKKS should be prevailing policy")
    }
}
