import CoreData
import XCTest

#if OCTAGON
import CloudKitCode
import InternalSwiftProtobuf

class FakeCKOperationRunner: CKOperationRunner {
    var server: FakeCuttlefishServer

    init(server: FakeCuttlefishServer) {
        self.server = server
    }

    func add<RequestType, ResponseType>(_ operation: CKCodeOperation<RequestType, ResponseType>) where RequestType: Message, ResponseType: Message {
        if let request = operation.request as? ResetRequest {
            if let completionBlock = operation.codeOperationResultBlock as? ((Result<ResetResponse, Error>) -> Void) {
                self.server.reset(request, completion: completionBlock)
            }
        } else if let request = operation.request as? EstablishRequest {
            if let completionBlock = operation.codeOperationResultBlock as? ((Result<EstablishResponse, Error>) -> Void) {
                self.server.establish(request, completion: completionBlock)
            }
        } else if let request = operation.request as? JoinWithVoucherRequest {
            if let completionBlock = operation.codeOperationResultBlock as? ((Result<JoinWithVoucherResponse, Error>) -> Void) {
                self.server.joinWithVoucher(request, completion: completionBlock)
            }
        } else if let request = operation.request as? UpdateTrustRequest {
            if let completionBlock = operation.codeOperationResultBlock as? ((Result<UpdateTrustResponse, Error>) -> Void) {
                self.server.updateTrust(request, completion: completionBlock)
            }
        } else if let request = operation.request as? SetRecoveryKeyRequest {
            if let completionBlock = operation.codeOperationResultBlock as? ((Result<SetRecoveryKeyResponse, Error>) -> Void) {
                self.server.setRecoveryKey(request, completion: completionBlock)
            }
        } else if let request = operation.request as? FetchChangesRequest {
            if let completionBlock = operation.codeOperationResultBlock as? ((Result<FetchChangesResponse, Error>) -> Void) {
                self.server.fetchChanges(request, completion: completionBlock)
            }
        } else if let request = operation.request as? FetchViableBottlesRequest {
            if let completionBlock = operation.codeOperationResultBlock as? ((Result<FetchViableBottlesResponse, Error>) -> Void) {
                self.server.fetchViableBottles(request, completion: completionBlock)
            }
        } else if let request = operation.request as? FetchPolicyDocumentsRequest {
            if let completionBlock = operation.codeOperationResultBlock as? ((Result<FetchPolicyDocumentsResponse, Error>) -> Void) {
                self.server.fetchPolicyDocuments(request, completion: completionBlock)
            }
        } else if let request = operation.request as? ValidatePeersRequest {
            if let completionBlock = operation.codeOperationResultBlock as? ((Result<ValidatePeersResponse, Error>) -> Void) {
                self.server.validatePeers(request, completion: completionBlock)
            }
        } else if let request = operation.request as? ReportHealthRequest {
            if let completionBlock = operation.codeOperationResultBlock as? ((Result<ReportHealthResponse, Error>) -> Void) {
                self.server.reportHealth(request, completion: completionBlock)
            }
        } else if let request = operation.request as? PushHealthInquiryRequest {
            if let completionBlock = operation.codeOperationResultBlock as? ((Result<PushHealthInquiryResponse, Error>) -> Void) {
                self.server.pushHealthInquiry(request, completion: completionBlock)
            }
        } else if let request = operation.request as? GetRepairActionRequest {
            if let completionBlock = operation.codeOperationResultBlock as? ((Result<GetRepairActionResponse, Error>) -> Void) {
                self.server.getRepairAction(request, completion: completionBlock)
            }
        } else if let request = operation.request as? GetSupportAppInfoRequest {
            if let completionBlock = operation.codeOperationResultBlock as? ((Result<GetSupportAppInfoResponse, Error>) -> Void) {
                self.server.getSupportAppInfo(request, completion: completionBlock)
            }
        } else if let request = operation.request as? GetClubCertificatesRequest {
            if let completionBlock = operation.codeOperationResultBlock as? ((Result<GetClubCertificatesResponse, Error>) -> Void) {
                self.server.getClubCertificates(request, completion: completionBlock)
            }
        } else if let request = operation.request as? FetchSOSiCloudIdentityRequest {
            if let completionBlock = operation.codeOperationResultBlock as? ((Result<FetchSOSiCloudIdentityResponse, Error>) -> Void) {
                self.server.fetchSosiCloudIdentity(request, completion: completionBlock)
            }
        } else if let request = operation.request as? ResetAccountCDPContentsRequest {
            if let completionBlock = operation.codeOperationResultBlock as? ((Result<ResetAccountCDPContentsResponse, Error>) -> Void) {
                self.server.resetAccountCdpcontents(request, completion: completionBlock)
            }
        } else if let request = operation.request as? AddCustodianRecoveryKeyRequest {
            if let completionBlock = operation.codeOperationResultBlock as? ((Result<AddCustodianRecoveryKeyResponse, Error>) -> Void) {
                self.server.addCustodianRecoveryKey(request, completion: completionBlock)
            }
        } else if let request = operation.request as? FetchRecoverableTLKSharesRequest {
            if let completionBlock = operation.codeOperationResultBlock as? ((Result<FetchRecoverableTLKSharesResponse, Error>) -> Void) {
                self.server.fetchRecoverableTlkshares(request, completion: completionBlock)
            }
        } else {
            abort()
        }
    }
}

class FakeCuttlefishCKOperationRunner: ContainerNameToCKOperationRunner {
    var server: FakeCuttlefishServer

    init(server: FakeCuttlefishServer) {
        self.server = server
    }

    func client(containerName _: String) -> CKOperationRunner {
        return FakeCKOperationRunner(server: self.server)
    }
}

class OTMockDeviceInfoAdapter: OTDeviceInformationAdapter {
    var mockModelID: String
    var mockDeviceName: String?
    var mockOsVersion: String
    var mockSerialNumber: String

    init(modelID: String, deviceName: String?, serialNumber: String, osVersion: String) {
        self.mockModelID = modelID
        self.mockDeviceName = deviceName
        self.mockSerialNumber = serialNumber
        self.mockOsVersion = osVersion
    }

    func modelID() -> String {
        return self.mockModelID
    }

    func deviceName() -> String? {
        return self.mockDeviceName
    }

    func osVersion() -> String {
        return self.mockOsVersion
    }

    func serialNumber() -> String? {
        return self.mockSerialNumber
    }

    func register(forDeviceNameUpdates listener: OTDeviceInformationNameUpdateListener) {
    }
}

class OTMockAuthKitAdapter: OTAuthKitAdapter {

    // A nil altDSID means 'no authkit account'
    var altDSID: String?

    var injectAuthErrorsAtFetchTime: Bool

    var hsa2: Bool
    var isDemoAccount: Bool

    let currentMachineID: String
    var otherDevices: Set<String>
    var excludeDevices: Set<String>

    var machineIDFetchErrors: [NSError] = []

    var fetchCondition: CKKSCondition?

    var fetchInvocations: Int = 0

    var listeners: CKKSListenerCollection<OTAuthKitAdapterNotifier>

    init(altDSID: String?, machineID: String, otherDevices: Set<String>) {
        self.altDSID = altDSID
        self.currentMachineID = machineID
        self.otherDevices = otherDevices
        self.excludeDevices = Set()
        self.hsa2 = true
        self.isDemoAccount = false
        self.listeners = CKKSListenerCollection<OTAuthKitAdapterNotifier>(name: "test-authkit")

        // By default, you can fetch a list you're not on
        self.injectAuthErrorsAtFetchTime = false
    }

    func primaryiCloudAccountAltDSID() throws -> String {
        guard let altDSID = self.altDSID else {
            throw NSError(domain: OctagonErrorDomain,
                          code: OctagonError.authKitNoPrimaryAccount.rawValue,
                          userInfo: nil)
        }
        return altDSID
    }

    func accountIsHSA2(byAltDSID altDSID: String) -> Bool {
        // TODO: do we need to examine altDSID here?
        return self.hsa2
    }
    func accountIsDemoAccount(_ error: NSErrorPointer) -> Bool {
        return self.isDemoAccount
    }

    func machineID() throws -> String {
        // TODO: throw if !accountPresent
        return self.currentMachineID
    }

    func fetchCurrentDeviceList(_ complete: @escaping (Set<String>?, Error?) -> Void) {
        self.fetchInvocations += 1
        if let error = self.machineIDFetchErrors.first {
            self.machineIDFetchErrors.removeFirst()
            complete(nil, error)
            return
        }

        // If this device is actively on the excluded list, return an error
        // But demo accounts can do what they want
        if !self.isDemoAccount &&
            self.injectAuthErrorsAtFetchTime &&
            self.excludeDevices.contains(self.currentMachineID) {
            complete(nil, NSError(domain: AKAppleIDAuthenticationErrorDomain,
                                  code: -7026,
                                  userInfo: [NSLocalizedDescriptionKey: "Injected AKAuthenticationErrorNotPermitted error"]))
            return
        }

        if let fetchCondition = self.fetchCondition {
            fetchCondition.fulfill()
        }

        // TODO: fail if !accountPresent
        complete(self.currentDeviceList(), nil)
    }

    func currentDeviceList() -> Set<String> {
        // Always succeeds.
        return Set([self.currentMachineID]).union(otherDevices).subtracting(excludeDevices)
    }

    func sendIncompleteNotification() {
        self.listeners.iterateListeners {
            $0.incompleteNotificationOfMachineIDListChange()
        }
    }

    func addAndSendNotification(machineID: String) {
        self.otherDevices.insert(machineID)
        self.excludeDevices.remove(machineID)
        self.sendAddNotification(machineID: machineID, altDSID: self.altDSID!)
    }

    func sendAddNotification(machineID: String, altDSID: String) {
        self.listeners.iterateListeners {
            $0.machinesAdded([machineID], altDSID: altDSID)
        }
    }

    func removeAndSendNotification(machineID: String) {
        self.excludeDevices.insert(machineID)
        self.sendRemoveNotification(machineID: machineID, altDSID: self.altDSID!)
    }

    func sendRemoveNotification(machineID: String, altDSID: String) {
        self.listeners.iterateListeners {
            $0.machinesRemoved([machineID], altDSID: altDSID)
        }
    }

    func registerNotification(_ notifier: OTAuthKitAdapterNotifier) {
        self.listeners.registerListener(notifier)
    }
}

class OTMockTooManyPeersAdapter: OTTooManyPeersAdapter {
    var limit: UInt = 10
    var timesPopped: UInt = 0
    var lastPopCount: UInt = 0
    var lastPopLimit: UInt = 0
    var shouldPop: Bool = true
    var shouldPopCount: UInt = 0

    func shouldPopDialog() -> Bool {
        shouldPopCount += 1
        return shouldPop
    }

    func getLimit() -> UInt {
        return limit
    }

    func popDialog(withCount count: UInt, limit: UInt) {
        timesPopped += 1
        lastPopCount = count
        lastPopLimit = limit
    }
}

class OTMockLogger: NSObject, SFAnalyticsProtocol {
    static func logger() -> SFAnalyticsProtocol? {
        return OTMockLogger()
    }

    func logResult(forEvent eventName: String, hardFailure: Bool, result eventResultError: Error?) {
        // pass
    }

    func logResult(forEvent eventName: String, hardFailure: Bool, result eventResultError: Error?, withAttributes attributes: [AnyHashable: Any]? = nil) {
        // pass
    }

    func logSystemMetrics(forActivityNamed eventName: String, withAction action: (() -> Void)? = nil) -> SFAnalyticsActivityTracker? {
        // pass
        return nil
    }

    func startLogSystemMetrics(forActivityNamed eventName: String) -> SFAnalyticsActivityTracker? {
        // pass
        return nil
    }

    func addMultiSampler(forName samplerName: String, withTimeInterval timeInterval: TimeInterval, block: @escaping () -> [String: NSNumber]) -> SFAnalyticsMultiSampler? {
        // in this case, why don't we just try calling the block, just to see what happens?
        // Then, don't do any follow up
        _ = block()
        return nil
    }
}

let OTMockEscrowRequestNotification = Notification.Name("silent-escrow-request-triggered")

class OTMockSecEscrowRequest: NSObject, SecEscrowRequestable {
    static var populateStatuses = false
    var statuses: [String: String] = [:]

    static func request() throws -> SecEscrowRequestable {
        let request = OTMockSecEscrowRequest()
        if populateStatuses == true {
            request.statuses["uuid"] = SecEscrowRequestPendingCertificate
        }
        return request
    }

    func triggerEscrowUpdate(_ reason: String) throws {
        try triggerEscrowUpdate(reason, options: nil)
    }

    func triggerEscrowUpdate(_ reason: String, options: [AnyHashable: Any]?) throws {
        NotificationCenter.default.post(name: OTMockEscrowRequestNotification, object: nil)
    }

    func fetchStatuses() throws -> [AnyHashable: Any] {
        return statuses
    }

    func pendingEscrowUpload(_ error: NSErrorPointer) -> Bool {
        if statuses["uuid"] == SecEscrowRequestPendingCertificate {
            return true
        } else {
            return false
        }
    }

    func escrowCompleted(withinLastSeconds: TimeInterval) -> Bool {
        return false
    }
}

class OctagonTestsBase: CloudKitKeychainSyncingMockXCTest {
    var tmpPath: String!
    var tmpURL: URL!

    var manager: OTManager!
    var cuttlefishContext: OTCuttlefishContext!

    var keychainUpgradeController: KeychainItemUpgradeRequestController!
    var keychainUpgradeServer: KeychainItemUpgradeRequestServer!

    var otcliqueContext: OTConfigurationContext!

    var intendedCKKSZones: Set<CKRecordZone.ID>!
    var manateeZoneID: CKRecordZone.ID!
    var limitedPeersAllowedZoneID: CKRecordZone.ID!
    var passwordsZoneID: CKRecordZone.ID!

    var fakeCuttlefishServer: FakeCuttlefishServer!
    var fakeCuttlefishCreator: FakeCuttlefishCKOperationRunner!
    var tphClient: Client!
    var tphXPCProxy: ProxyXPCConnection!

    var accountAltDSID: String!

    // These three mock authkits will all include each other in their machine ID list
    var mockAuthKit: OTMockAuthKitAdapter!
    var mockAuthKit2: OTMockAuthKitAdapter!
    var mockAuthKit3: OTMockAuthKitAdapter!

    var mockTooManyPeers: OTMockTooManyPeersAdapter!

    var mockDeviceInfo: OTMockDeviceInfoAdapter!

    var otControl: OTControl!
    var otXPCProxy: ProxyXPCConnection!

    var otControlEntitlementBearer: FakeOTControlEntitlementBearer!
    var otControlEntitlementChecker: OTControlProtocol!
    var otControlCLI: OTControlCLI!

    var otFollowUpController: OTMockFollowUpController!

    override static func setUp() {
        UserDefaults.standard.register(defaults: ["com.apple.CoreData.ConcurrencyDebug": 1])

        super.setUp()

        // Turn on NO_SERVER stuff
        securityd_init_local_spi()
    }

    override func setUp() {
        // Create directories first so that anything that writes files will happen in this test's temporary directory.
        self.setUpDirectories()

        // Set the global CKKS bool to TRUE
        SecCKKSEnable()

        // Until we can reasonably run SOS in xctest, this must be off. Note that this makes our tests
        // not accurately reproduce what a real device would do.
        OctagonSetPlatformSupportsSOS(false)

        if self.mockDeviceInfo == nil {
            let actualDeviceAdapter = OTDeviceInformationActualAdapter()
            self.mockDeviceInfo = OTMockDeviceInfoAdapter(modelID: actualDeviceAdapter.modelID(),
                                                          deviceName: actualDeviceAdapter.deviceName(),
                                                          serialNumber: NSUUID().uuidString,
                                                          osVersion: actualDeviceAdapter.osVersion())
        }

        self.manateeZoneID = CKRecordZone.ID(zoneName: "Manatee")
        self.limitedPeersAllowedZoneID = CKRecordZone.ID(zoneName: "LimitedPeersAllowed")
        self.passwordsZoneID = CKRecordZone.ID(zoneName: "Passwords")

        // We'll use this set to limit the views that CKKS brings up in the tests (mostly for performance reasons)
        if self.intendedCKKSZones == nil {
            if self.mockDeviceInfo.mockModelID.contains("AppleTV") ||
                self.mockDeviceInfo.mockModelID.contains("AudioAccessory") ||
                self.mockDeviceInfo.mockModelID.contains("WinPC") {
                self.intendedCKKSZones = Set([
                    self.limitedPeersAllowedZoneID!,
                ])
            } else {
                self.intendedCKKSZones = Set([
                    self.limitedPeersAllowedZoneID!,
                    self.manateeZoneID!,
                ])
            }
        }
        self.ckksZones = NSMutableSet(array: Array(self.intendedCKKSZones))

        // Create the zones, so we can inject them into our fake cuttlefish server
        self.zones = [:]
        self.keys = [:]
        self.ckksZones.forEach { obj in
            let zoneID = obj as! CKRecordZone.ID
            self.zones![zoneID] = FakeCKZone(zone: zoneID)
        }

        // Asserting a type on self.zones seems to duplicate the dictionary, but not deep-copy the contents
        // We'll use them as NSMutableDictionaries, I guess
        self.fakeCuttlefishServer = FakeCuttlefishServer(nil, ckZones: self.zones!, ckksZoneKeys: self.keys!)

        self.fakeCuttlefishCreator = FakeCuttlefishCKOperationRunner(server: self.fakeCuttlefishServer)
        self.tphClient = Client(endpoint: nil, containerMap: ContainerMap(ckCodeOperationRunnerCreator: self.fakeCuttlefishCreator,
                                                                          darwinNotifier: FakeCKKSNotifier.self))

        self.otFollowUpController = OTMockFollowUpController()

        self.mockAuthKit = OTMockAuthKitAdapter(altDSID: UUID().uuidString, machineID: "MACHINE1", otherDevices: ["MACHINE2", "MACHINE3"])
        self.mockAuthKit2 = OTMockAuthKitAdapter(altDSID: self.mockAuthKit.altDSID, machineID: "MACHINE2", otherDevices: ["MACHINE1", "MACHINE3"])
        self.mockAuthKit3 = OTMockAuthKitAdapter(altDSID: self.mockAuthKit.altDSID, machineID: "MACHINE3", otherDevices: ["MACHINE1", "MACHINE2"])

        self.mockTooManyPeers = OTMockTooManyPeersAdapter()

        let tphInterface = TrustedPeersHelperSetupProtocol(NSXPCInterface(with: TrustedPeersHelperProtocol.self))
        self.tphXPCProxy = ProxyXPCConnection(self.tphClient!, interface: tphInterface)

        self.disableConfigureCKKSViewManagerWithViews = true

        // Now, perform further test initialization (including temporary keychain creation - that are created in the test-specific directory tree created by setUpDirectories)
        super.setUp()

        self.defaultCKKS.setSyncingViewsAllowList(Set(self.intendedCKKSZones!.map { $0.zoneName }))

        // Octagon is responsible for creating CKKS views; so we don't create any in the test setup

        // Double-check that the world of zones and views looks like what we expect
        XCTAssertEqual(self.ckksZones as? Set<CKRecordZone.ID>, self.intendedCKKSZones, "should still operate on our expected zones only")
        XCTAssertEqual(self.ckksZones.count, self.zones!.count, "Should have the same number of fake zones as expected zones")

        // Octagon must initialize the views
        self.automaticallyBeginCKKSViewCloudKitOperation = false

        // Octagon requires the self peer keys to be persisted in the keychain
        saveToKeychain(keyPair: self.mockSOSAdapter.selfPeer.signingKey, label: "com.apple.securityd.sossigningkey")
        saveToKeychain(keyPair: self.mockSOSAdapter.selfPeer.encryptionKey, label: "com.apple.securityd.sosencryptionkey")

        // By default, not in SOS when test starts
        // And under octagon, SOS trust is not essential
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCNotInCircle)
        self.mockSOSAdapter.essential = false

        self.cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        self.keychainUpgradeServer = KeychainItemUpgradeRequestServer(lockStateTracker: self.lockStateTracker)
        self.keychainUpgradeController = self.keychainUpgradeServer.controller

        self.otControlEntitlementBearer = FakeOTControlEntitlementBearer()
        self.otControlEntitlementChecker = OctagonXPCEntitlementChecker.create(with: self.manager, entitlementBearer: self.otControlEntitlementBearer)

        self.otXPCProxy = ProxyXPCConnection(self.otControlEntitlementChecker!, interface: OTSetupControlProtocol(NSXPCInterface(with: OTControlProtocol.self)))
        self.otControl = OTControl(connection: self.otXPCProxy.connection(), sync: true)
        self.otControlCLI = OTControlCLI(otControl: self.otControl)

        self.otcliqueContext = OTConfigurationContext()
        self.otcliqueContext.context = OTDefaultContext
        self.otcliqueContext.dsid = "1234"
        self.otcliqueContext.altDSID = self.mockAuthKit.altDSID!
        self.otcliqueContext.otControl = self.otControl
    }

    override func setUpOTManager(_ cloudKitClassDependencies: CKKSCloudKitClassDependencies) -> OTManager {
        self.manager = OTManager(sosAdapter: self.mockSOSAdapter,
                                 authKitAdapter: self.mockAuthKit,
                                 tooManyPeersAdapter: self.mockTooManyPeers,
                                 deviceInformationAdapter: self.mockDeviceInfo,
                                 personaAdapter: self.mockPersonaAdapter,
                                 apsConnectionClass: FakeAPSConnection.self,
                                 escrowRequestClass: OTMockSecEscrowRequest.self,
                                 notifierClass: FakeCKKSNotifier.self,
                                 loggerClass: OTMockLogger.self,
                                 lockStateTracker: CKKSLockStateTracker(provider: self.lockStateProvider),
                                 reachabilityTracker: CKKSReachabilityTracker(),
                                 cloudKitClassDependencies: cloudKitClassDependencies,
                                 cuttlefishXPCConnection: tphXPCProxy.connection(),
                                 cdpd: self.otFollowUpController)
        return self.manager
    }

    override func tearDown() {
        // Just to be sure
        self.verifyDatabaseMocks()

        TestsObjectiveC.clearError()
        TestsObjectiveC.clearLastRowID()
        TestsObjectiveC.clearErrorInsertionDictionary()

        let statusExpectation = self.expectation(description: "status callback occurs")
        self.cuttlefishContext.rpcStatus { _, _ in
            statusExpectation.fulfill()
        }
        self.wait(for: [statusExpectation], timeout: 10)

        self.manager.allContextsDisablePendingFlags()
        self.manager.allContextsHalt()

        if self.cuttlefishContext.stateMachine.currentState != OctagonStateMachineNotStarted {
            XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(20 * NSEC_PER_SEC), "Main cuttlefish context should quiesce before the test ends")
        }

        // Stop all of CKKS
        self.injectedManager!.haltAll()

        XCTAssertTrue(self.manager.allContextsPause(10 * NSEC_PER_SEC), "All cuttlefish contexts should pause")

        do {
            try self.tphClient.containerMap.deleteAllPersistentStores()
        } catch {
            XCTFail("Failed to clean up CoreData databases: \(error)")
        }
        self.tphClient.containerMap.removeAllContainers()

        super.tearDown()

        self.cuttlefishContext = nil
        self.manager = nil

        self.otcliqueContext = nil

        self.manateeZoneID = nil
        self.limitedPeersAllowedZoneID = nil

        self.fakeCuttlefishServer = nil
        self.fakeCuttlefishCreator = nil
        self.tphClient = nil
        self.tphXPCProxy = nil

        self.accountAltDSID = nil

        self.mockAuthKit = nil
        self.mockAuthKit2 = nil
        self.mockAuthKit3 = nil

        self.mockDeviceInfo = nil

        self.otControl = nil
        self.otXPCProxy = nil

        self.otControlEntitlementBearer = nil
        self.otControlEntitlementChecker = nil
        self.otControlCLI = nil

        self.otFollowUpController = nil
        self.keychainUpgradeController.persistentReferenceUpgrader.cancel()
        self.keychainUpgradeController = nil
    }

    override func managedViewList() -> Set<String> {
        // We only want to return the 'base' set of views here; not the full set.
        #if !os(tvOS)
        return Set(["LimitedPeersAllowed", "Manatee"])
        #else
        return Set(["LimitedPeersAllowed"])
        #endif
    }

    func fetchEgoPeerID() -> String {
        return self.fetchEgoPeerID(context: self.cuttlefishContext)
    }

    func fetchEgoPeerID(context: OTCuttlefishContext) -> String {
        var ret: String!
        let fetchPeerIDExpectation = self.expectation(description: "fetchPeerID callback occurs")
        context.rpcFetchEgoPeerID { peerID, fetchError in
            XCTAssertNil(fetchError, "should not error fetching ego peer ID")
            XCTAssertNotNil(peerID, "Should have a peer ID")
            ret = peerID
            fetchPeerIDExpectation.fulfill()
        }
        self.wait(for: [fetchPeerIDExpectation], timeout: 10)
        return ret
    }

    func setAllowListToCurrentAuthKit(container: String, context: String, accountIsDemo: Bool) {
        let allowListExpectation = self.expectation(description: "set allow list callback occurs")
        let honorIDMSListChanges = accountIsDemo ? false : true
        self.tphClient.setAllowedMachineIDsWithContainer(container,
                                                         context: context,
                                                         allowedMachineIDs: self.mockAuthKit.currentDeviceList(), honorIDMSListChanges: honorIDMSListChanges) { _, error in
                                                            XCTAssertNil(error, "Should be no error setting allow list")
                                                            allowListExpectation.fulfill()
        }
        self.wait(for: [allowListExpectation], timeout: 10)
    }

    func saveToKeychain(keyPair: _SFECKeyPair, label: String) {
        let status = SecItemAdd([kSecValueRef as String: keyPair.secKey!,
                                 kSecClass as String: kSecClassKey as String,
                                 kSecAttrApplicationTag as String: label.data(using: .utf8)!,
                                 kSecUseDataProtectionKeychain as String: true, ] as CFDictionary, nil)
        XCTAssertEqual(status, errSecSuccess, "Should be able to save a key in the keychain")
    }

    func tmpStoreDescription(name: String) -> NSPersistentStoreDescription {
        let tmpStoreURL = URL(fileURLWithPath: name, relativeTo: tmpURL)
        return NSPersistentStoreDescription(url: tmpStoreURL)
    }

    func assertEnters(context: OTCuttlefishContext, state: String, within: UInt64, file: StaticString = #file, line: UInt = #line) {
        XCTAssertEqual(0, (context.stateMachine.stateConditions[state]!).wait(within), "State machine should enter '\(state)'", file: file, line: line)
        if state == OctagonStateReady || state == OctagonStateUntrusted {
            XCTAssertEqual(0, context.stateMachine.paused.wait(10 * NSEC_PER_SEC), "State machine should pause soon", file: file, line: line)
        }
    }

    func assertEnters(stateMachine: OctagonStateMachine, state: String, within: UInt64, file: StaticString = #file, line: UInt = #line) {
        XCTAssertEqual(0, (stateMachine.stateConditions[state]!).wait(within), "State machine should enter '\(state)'", file: file, line: line)
        if state == OctagonStateReady || state == OctagonStateUntrusted {
            XCTAssertEqual(0, stateMachine.paused.wait(10 * NSEC_PER_SEC), "State machine should pause soon", file: file, line: line)
        }
    }

    func assertConsidersSelfTrusted(context: OTCuttlefishContext, isLocked: Bool = false, file: StaticString = #file, line: UInt = #line) {
        XCTAssertEqual(context.currentMemoizedTrustState(), .TRUSTED, "Trust state (for \(context)) should be trusted", file: file, line: line)

        let accountMetadata = try! context.accountMetadataStore.loadOrCreateAccountMetadata()
        XCTAssertEqual(accountMetadata.attemptedJoin, .ATTEMPTED, "Should have 'attempted a join'", file: file, line: line)

        let statusexpectation = self.expectation(description: "trust status returns")
        let configuration = OTOperationConfiguration()
        configuration.timeoutWaitForCKAccount = 500 * NSEC_PER_MSEC
        context.rpcTrustStatus(configuration) { egoStatus, egoPeerID, _, isLocked, _, _   in
            XCTAssertEqual(egoStatus, .in, "Self peer (for \(context)) should be trusted", file: file, line: line)
            XCTAssertNotNil(egoPeerID, "Should have a peerID", file: file, line: line)
            XCTAssertEqual(isLocked, isLocked, "should be \(isLocked)", file: file, line: line)
            statusexpectation.fulfill()
        }
        self.wait(for: [statusexpectation], timeout: 10)
    }

    func assertConsidersSelfTrustedCachedAccountStatus(context: OTCuttlefishContext, file: StaticString = #file, line: UInt = #line) {
        XCTAssertEqual(context.currentMemoizedTrustState(), .TRUSTED, "Trust state (for \(context)) should be trusted", file: file, line: line)

        let cachedStatusexpectation = self.expectation(description: "(cached) trust status returns")
        let configuration = OTOperationConfiguration()
        configuration.useCachedAccountStatus = true
        configuration.timeoutWaitForCKAccount = 500 * NSEC_PER_MSEC

        context.rpcTrustStatus(configuration) { egoStatus, egoPeerID, _, _, _, _ in
            XCTAssertEqual(egoStatus, .in, "Cached self peer (for \(context)) should be trusted", file: file, line: line)
            XCTAssertNotNil(egoPeerID, "Should have a (cached) peerID", file: file, line: line)
            cachedStatusexpectation.fulfill()
        }
        self.wait(for: [cachedStatusexpectation], timeout: 10)

        let cliqueConfiguration = OTConfigurationContext()
        cliqueConfiguration.context = context.contextID
        cliqueConfiguration.dsid = "1234"
        cliqueConfiguration.altDSID = self.mockAuthKit.altDSID!
        cliqueConfiguration.otControl = self.otControl
        let otclique = OTClique(contextData: cliqueConfiguration)

        let status = otclique.fetchStatus(nil)
        XCTAssertEqual(status, .in, "OTClique API should return (trusted)", file: file, line: line)

        configuration.useCachedAccountStatus = false
        let statusexpectation = self.expectation(description: "(cached) trust status returns")
        context.rpcTrustStatus(configuration) { egoStatus, egoPeerID, _, _, _, _ in
            XCTAssertEqual(egoStatus, .in, "Self peer (for \(context)) should be trusted", file: file, line: line)
            XCTAssertNotNil(egoPeerID, "Should have a peerID", file: file, line: line)
            statusexpectation.fulfill()
        }
        self.wait(for: [statusexpectation], timeout: 10)
    }

    func assertConsidersSelfUntrusted(context: OTCuttlefishContext, file: StaticString = #file, line: UInt = #line) {
        XCTAssertTrue(context.currentMemoizedTrustState() == .UNTRUSTED || context.currentMemoizedTrustState() == .UNKNOWN, "Trust state (for \(context)) should be untrusted or unknown", file: file, line: line)
        let statusexpectation = self.expectation(description: "trust status returns")
        let configuration = OTOperationConfiguration()
        configuration.timeoutWaitForCKAccount = 500 * NSEC_PER_MSEC
        context.rpcTrustStatus(configuration) { egoStatus, _, _, _, _, _ in
            // TODO: separate 'untrusted' and 'no trusted peers for account yet'
            XCTAssertTrue([.notIn, .absent].contains(egoStatus), "Self peer (for \(context)) should be distrusted or absent, is \(egoStatus)", file: file, line: line)
            statusexpectation.fulfill()
        }
        self.wait(for: [statusexpectation], timeout: 10)
    }

    func assertConsidersSelfWaitingForCDP(context: OTCuttlefishContext, file: StaticString = #file, line: UInt = #line) {
        XCTAssertEqual(context.currentMemoizedTrustState(), .UNKNOWN, "Trust state (for \(context)) should be unknown", file: file, line: line)
        let statusexpectation = self.expectation(description: "trust status returns")
        let configuration = OTOperationConfiguration()
        configuration.timeoutWaitForCKAccount = 500 * NSEC_PER_MSEC
        context.rpcTrustStatus(configuration) { egoStatus, _, _, _, _, _ in
            // TODO: separate 'untrusted' and 'no trusted peers for account yet'
            XCTAssertTrue([.notIn, .absent].contains(egoStatus), "Self peer (for \(context)) should be distrusted or absent", file: file, line: line)
            statusexpectation.fulfill()
        }
        self.wait(for: [statusexpectation], timeout: 10)

        XCTAssertEqual(self.fetchCDPStatus(context: context), .disabled, "CDP status should be 'disabled'", file: file, line: line)
    }

    func assertAccountAvailable(context: OTCuttlefishContext, file: StaticString = #file, line: UInt = #line) {
        XCTAssertEqual(context.currentMemoizedAccountState(), .ACCOUNT_AVAILABLE, "Account state (for \(context)) should be 'available''", file: file, line: line)
    }

    func assertNoAccount(context: OTCuttlefishContext, file: StaticString = #file, line: UInt = #line) {
        XCTAssertEqual(context.currentMemoizedAccountState(), .NO_ACCOUNT, "Account state (for \(context)) should be no account", file: file, line: line)
    }

    func fetchCDPStatus(context: OTCuttlefishContext) -> OTCDPStatus {
        let config = OTConfigurationContext()
        config.context = context.contextID
        config.otControl = self.otControl

        var error: NSError?
        let cdpstatus = OTClique.getCDPStatus(config, error: &error)
        XCTAssertNil(error, "Should have no error fetching CDP status")

        return cdpstatus
    }

    func assertTrusts(context: OTCuttlefishContext, includedPeerIDCount: Int, excludedPeerIDCount: Int, file: StaticString = #file, line: UInt = #line) {
        let dumpCallback = self.expectation(description: "dump callback occurs")
        self.tphClient.dumpEgoPeer(withContainer: context.containerName, context: context.contextID) { _, _, _, dynamicInfo, error in
            XCTAssertNil(error, "should be no error", file: file, line: line)
            XCTAssertNotNil(dynamicInfo, "Should be a dynamic info", file: file, line: line)
            XCTAssertEqual(dynamicInfo!.includedPeerIDs.count, includedPeerIDCount, "should be \(includedPeerIDCount) included peer ids", file: file, line: line)
            XCTAssertEqual(dynamicInfo!.excludedPeerIDs.count, excludedPeerIDCount, "should be \(excludedPeerIDCount) excluded peer ids", file: file, line: line)

            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)
    }

    func restartCKKSViews() {
        self.defaultCKKS = self.injectedManager!.restartCKKSAccountSyncWithoutSettingPolicy(self.defaultCKKS)
        self.cuttlefishContext.reset(ckks: self.defaultCKKS)
    }

    func sendAllCKKSTrustedPeersChanged() {
        self.defaultCKKS.trustedPeerSetChanged(nil)
    }

    func assert(viewState: CKKSKeychainViewState, enters: String, within: UInt64, file: StaticString = #file, line: UInt = #line) {
        do {
            let x: CKKSCondition = try XCTUnwrap(viewState.keyHierarchyConditions[enters])
            XCTAssertEqual(0, x.wait(within), "CKKS key state should enter '\(enters)' (currently '\(viewState.viewKeyHierarchyState)')", file: file, line: line)
        } catch {
            XCTFail("Unexpected throw: \(error)", file: file, line: line)
        }
    }

    func assertAllCKKSViews(enter: String, ckksState: String? = nil, within: UInt64, filter: ((CKRecordZone.ID) -> Bool)? = nil, file: StaticString = #file, line: UInt = #line) {
        let f: ((CKRecordZone.ID) -> Bool) = filter ?? { (_: CKRecordZone.ID) in return true }

        self.ckksZones.filter { f($0 as! CKRecordZone.ID) }.forEach { expectedView in
            if let viewState = self.defaultCKKS.operationDependencies.viewState(forName: (expectedView as! CKRecordZone.ID).zoneName) {
                self.assert(viewState: viewState, enters: enter, within: within)
            } else {
                XCTFail("Should have a view matching '\(expectedView)'", file: file, line: line)
            }
        }

        // Instead of changing every test, let's provide some extra waiting here
        if let ckksState = ckksState {
            self.assertCKKSStateMachine(enters: ckksState, within: within)
        } else {
            switch enter {
            case SecCKKSZoneKeyStateReady:
                self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC)
            case SecCKKSZoneKeyStateWaitForTrust:
                self.assertCKKSStateMachine(enters: CKKSStateWaitForTrust, within: 10 * NSEC_PER_SEC)
            case SecCKKSZoneKeyStateWaitForTLKCreation:
                self.assertCKKSStateMachine(enters: CKKSStateWaitForTrust, within: 10 * NSEC_PER_SEC)
            case SecCKKSZoneKeyStateLoggedOut:
                self.assertCKKSStateMachine(enters: CKKSStateLoggedOut, within: 10 * NSEC_PER_SEC)
            default:
                break
            }
        }
    }

    func assertCKKSStateMachine(enters: String, within: UInt64, file: StaticString = #file, line: UInt = #line) {
        XCTAssertEqual(0, ( self.defaultCKKS.stateMachine.stateConditions[enters]!).wait(within),
                       "CKKS state machine should enter '\(enters)' (currently '\(self.defaultCKKS.stateMachine.currentState)')",
                       file: file, line: line)
    }

    func pauseCKKSStateMachine(entering: String) {
        self.defaultCKKS.stateMachine.testPause(afterEntering: entering)
    }

    func releaseCKKSStateMachine(from: String) {
        self.defaultCKKS.stateMachine.testReleasePause(from)
    }

    func assertAllCKKSViewsUploadKeyHierarchy(tlkShares: UInt, file: StaticString = #file, line: UInt = #line) {
        for expectedView in self.ckksZones {
            self.expectCKModifyKeyRecords(3, currentKeyPointerRecords: 3, tlkShareRecords: tlkShares, zoneID: expectedView as! CKRecordZone.ID)
        }
    }

    func assertAllCKKSViewsUpload(tlkShares: UInt, filter: ((CKRecordZone.ID) -> Bool)? = nil, file: StaticString = #file, line: UInt = #line) {
        let f: ((CKRecordZone.ID) -> Bool) = filter ?? { (_: CKRecordZone.ID) in return true }

        self.ckksZones.filter { f($0 as! CKRecordZone.ID) }.forEach { expectedView in
            self.expectCKModifyKeyRecords(0, currentKeyPointerRecords: 0, tlkShareRecords: tlkShares, zoneID: expectedView as! CKRecordZone.ID)
        }
    }

    func putFakeKeyHierarchiesInCloudKit(filter: ((CKRecordZone.ID) -> Bool)? = nil) {
        let f: ((CKRecordZone.ID) -> Bool) = filter ?? { (_: CKRecordZone.ID) in return true }

        self.ckksZones.filter { f($0 as! CKRecordZone.ID) }.forEach { zone in
            self.putFakeKeyHierarchy(inCloudKit: zone as! CKRecordZone.ID)
        }
    }

    func putSelfTLKSharesInCloudKit(filter: ((CKRecordZone.ID) -> Bool)? = nil) {
        let f: ((CKRecordZone.ID) -> Bool) = filter ?? { (_: CKRecordZone.ID) in return true }

        self.ckksZones.filter { f($0 as! CKRecordZone.ID) }.forEach { zone in
            self.putSelfTLKShares(inCloudKit: zone as! CKRecordZone.ID)
        }
    }

    func putFakeDeviceStatusesInCloudKit(filter: ((CKRecordZone.ID) -> Bool)? = nil) {
        let f: ((CKRecordZone.ID) -> Bool) = filter ?? { (_: CKRecordZone.ID) in return true }

        self.ckksZones.filter { f($0 as! CKRecordZone.ID) }.forEach { zone in
            self.putFakeDeviceStatus(inCloudKit: zone as! CKRecordZone.ID)
        }
    }

    func saveTLKMaterialToKeychain(filter: ((CKRecordZone.ID) -> Bool)? = nil) {
        let f: ((CKRecordZone.ID) -> Bool) = filter ?? { (_: CKRecordZone.ID) in return true }

        self.ckksZones.filter { f($0 as! CKRecordZone.ID) }.forEach { zone in
            self.saveTLKMaterial(toKeychain: zone as! CKRecordZone.ID)
        }
    }

    func resetAllCKKSViews(file: StaticString = #file, line: UInt = #line) {
        let resetExpectation = self.expectation(description: "rpcResetCloudKit callback occurs")
        self.injectedManager!.rpcResetCloudKit(nil, reason: "octagon=unit-test") { error in
            XCTAssertNil(error, "Error should be nil?", file: file, line: line)
            resetExpectation.fulfill()
        }

        self.wait(for: [resetExpectation], timeout: 30)
    }

    func loadPeerKeys(context: OTCuttlefishContext) throws -> OctagonSelfPeerKeys {
        let metadata = try context.accountMetadataStore.loadOrCreateAccountMetadata()
        return try loadEgoKeysSync(peerID: metadata.peerID)
    }

    func putSelfTLKSharesInCloudKit(context: OTCuttlefishContext, filter: ((CKRecordZone.ID) -> Bool)? = nil) throws {
    let f: ((CKRecordZone.ID) -> Bool) = filter ?? { (_: CKRecordZone.ID) in return true }

        try self.ckksZones.filter { f($0 as! CKRecordZone.ID) }.forEach { zone in
            try self.putTLKShareInCloudKit(to: context, from: context, zoneID: zone as! CKRecordZone.ID)
        }
    }

    func putAllTLKSharesInCloudKit(to: OTCuttlefishContext, from: OTCuttlefishContext) throws {
        try self.ckksZones.forEach { zone in
            try self.putTLKShareInCloudKit(to: to, from: from, zoneID: zone as! CKRecordZone.ID)
        }
    }

    func putTLKShareInCloudKit(to: OTCuttlefishContext, from: OTCuttlefishContext, zoneID: CKRecordZone.ID) throws {
        self.putTLKShareInCloudKit(to: try self.loadPeerKeys(context: to), from: try self.loadPeerKeys(context: from), zoneID: zoneID)
    }

    func putAllTLKSharesInCloudKit(to: CKKSPeer, from: CKKSSelfPeer) {
        self.ckksZones.forEach { zone in
            self.putTLKShareInCloudKit(to: to, from: from, zoneID: zone as! CKRecordZone.ID)
        }
    }

    func putTLKShareInCloudKit(to: CKKSPeer, from: CKKSSelfPeer, zoneID: CKRecordZone.ID) {
        let zoneKeys = self.keys![zoneID] as! ZoneKeys
        self.putTLKShare(inCloudKit: zoneKeys.tlk!, from: from, to: to, zoneID: zoneID)
    }

    func putCustodianTLKSharesInCloudKit(crk: CustodianRecoveryKey) {
        self.putAllTLKSharesInCloudKit(to: crk.peerKeys, from: crk.peerKeys)
    }

    func putCustodianTLKSharesInCloudKit(crk: CustodianRecoveryKey, sender: OTCuttlefishContext) throws {
        self.putAllTLKSharesInCloudKit(to: crk.peerKeys, from: try self.loadPeerKeys(context: sender))
    }

    func custodianSelfTLKSharesInCloudKit(crk: CustodianRecoveryKey) -> Bool {
        return self.tlkSharesInCloudKit(receiverPeerID: crk.peerKeys.peerID,
                                        senderPeerID: crk.peerKeys.peerID)
    }

    func custodianTLKSharesInCloudKit(crk: CustodianRecoveryKey, sender: OTCuttlefishContext) throws -> Bool {
        let metadata = try sender.accountMetadataStore.loadOrCreateAccountMetadata()

        return self.tlkSharesInCloudKit(receiverPeerID: crk.peerKeys.peerID,
                                        senderPeerID: metadata.peerID)
    }

    func putInheritanceTLKSharesInCloudKit(irk: InheritanceKey) {
        self.putAllTLKSharesInCloudKit(to: irk.peerKeys, from: irk.peerKeys)
    }

    func putInheritanceTLKSharesInCloudKit(irk: InheritanceKey, sender: OTCuttlefishContext) throws {
        self.putAllTLKSharesInCloudKit(to: irk.peerKeys, from: try self.loadPeerKeys(context: sender))
    }

    func inheritanceSelfTLKSharesInCloudKit(irk: InheritanceKey) -> Bool {
        return self.tlkSharesInCloudKit(receiverPeerID: irk.peerKeys.peerID,
                                        senderPeerID: irk.peerKeys.peerID)
    }

    func inheritanceTLKSharesInCloudKit(irk: InheritanceKey, sender: OTCuttlefishContext) throws -> Bool {
        let metadata = try sender.accountMetadataStore.loadOrCreateAccountMetadata()

        return self.tlkSharesInCloudKit(receiverPeerID: irk.peerKeys.peerID,
                                        senderPeerID: metadata.peerID)
    }

    func putRecoveryKeyTLKSharesInCloudKit(recoveryKey: String, salt: String) throws {
        let recoveryKeys = try RecoveryKey(recoveryKeyString: recoveryKey, recoverySalt: salt)
        self.putAllTLKSharesInCloudKit(to: recoveryKeys.peerKeys, from: recoveryKeys.peerKeys)
    }

    func putRecoveryKeyTLKSharesInCloudKit(recoveryKey: String, salt: String, sender: OTCuttlefishContext) throws {
        let recoveryKeys = try RecoveryKey(recoveryKeyString: recoveryKey, recoverySalt: salt)
        let senderPeerKeys = try self.loadPeerKeys(context: sender)

        self.putAllTLKSharesInCloudKit(to: recoveryKeys.peerKeys, from: senderPeerKeys)
    }

    func recoveryKeyTLKSharesInCloudKit(recoveryKey: String, salt: String) throws -> Bool {
        let recoveryKeys = try RecoveryKey(recoveryKeyString: recoveryKey, recoverySalt: salt)

        return self.tlkSharesInCloudKit(receiverPeerID: recoveryKeys.peerKeys.peerID,
                                        senderPeerID: recoveryKeys.peerKeys.peerID)
    }

    func recoveryKeyTLKSharesInCloudKit(recoveryKey: String, salt: String, sender: OTCuttlefishContext) throws -> Bool {
        let recoveryKeys = try RecoveryKey(recoveryKeyString: recoveryKey, recoverySalt: salt)
        let senderAccountMetadata = try sender.accountMetadataStore.loadOrCreateAccountMetadata()

        return self.tlkSharesInCloudKit(receiverPeerID: recoveryKeys.peerKeys.peerID,
                                        senderPeerID: senderAccountMetadata.peerID)
    }

    func assertSelfTLKSharesInCloudKit(context: OTCuttlefishContext, file: StaticString = #file, line: UInt = #line) {
        let accountMetadata = try! context.accountMetadataStore.loadOrCreateAccountMetadata()
        self.assertSelfTLKSharesInCloudKit(peerID: accountMetadata.peerID)
    }

    func assertSelfTLKSharesInCloudKit(peerID: String, file: StaticString = #file, line: UInt = #line) {
        self.assertTLKSharesInCloudKit(receiverPeerID: peerID, senderPeerID: peerID)
    }

    func assertSelfTLKSharesNotInCloudKit(peerID: String, file: StaticString = #file, line: UInt = #line) {
        self.assertTLKSharesNotInCloudKit(receiverPeerID: peerID, senderPeerID: peerID)
    }

    func assertTLKSharesInCloudKit(receiver: OTCuttlefishContext, sender: OTCuttlefishContext, file: StaticString = #file, line: UInt = #line) {
        let receiverAccountMetadata = try! receiver.accountMetadataStore.loadOrCreateAccountMetadata()
        let senderAccountMetadata = try! sender.accountMetadataStore.loadOrCreateAccountMetadata()

        self.assertTLKSharesInCloudKit(receiverPeerID: receiverAccountMetadata.peerID,
                                       senderPeerID: senderAccountMetadata.peerID)
    }

    func assertTLKSharesInCloudKit(receiverPeerID: String, senderPeerID: String, file: StaticString = #file, line: UInt = #line) {
        XCTAssertTrue(self.tlkSharesInCloudKit(receiverPeerID: receiverPeerID, senderPeerID: senderPeerID), "All views should have a self TLK uploaded", file: file, line: line)
    }

    func assertTLKSharesNotInCloudKit(receiverPeerID: String, senderPeerID: String, file: StaticString = #file, line: UInt = #line) {
        XCTAssertTrue(self.tlkSharesNotInCloudKit(receiverPeerID: receiverPeerID, senderPeerID: senderPeerID), "All views should NOT have a self TLK uploaded", file: file, line: line)
    }

    func tlkSharesInCloudKit(receiverPeerID: String, senderPeerID: String) -> Bool {
        let tlkContains: [Bool] = self.ckksZones.map {
            let zoneID: CKRecordZone.ID = $0 as! CKRecordZone.ID
            return self.tlkShareInCloudKit(receiverPeerID: receiverPeerID, senderPeerID: senderPeerID, zoneID: zoneID)
        }

        guard !tlkContains.isEmpty else {
            return false
        }

        // return true if all entries in tlkContains is true
        return tlkContains.allSatisfy { $0 == true }
    }

    func tlkSharesNotInCloudKit(receiverPeerID: String, senderPeerID: String) -> Bool {
        let tlkContains: [Bool] = self.ckksZones.map {
            let zoneID: CKRecordZone.ID = $0 as! CKRecordZone.ID
            return self.tlkShareInCloudKit(receiverPeerID: receiverPeerID, senderPeerID: senderPeerID, zoneID: zoneID)
        }

        guard !tlkContains.isEmpty else {
            return false
        }

        // return true if all entries in tlkContains is false
        return tlkContains.allSatisfy { $0 == false }
    }

    func tlkShareInCloudKit(receiverPeerID: String, senderPeerID: String, zoneID: CKRecordZone.ID) -> Bool {
        guard let zone = self.zones![zoneID] as? FakeCKZone else {
            return false
        }

        let tlkShares = zone.currentDatabase.allValues.filter { ($0 as? CKRecord)?.recordType == SecCKRecordTLKShareType }.map { CKKSTLKShareRecord(ckRecord: $0 as! CKRecord) }

        for share in tlkShares {
            if share.share.receiverPeerID == receiverPeerID && share.senderPeerID == senderPeerID {
                // Found one!
                return true
            }
        }
        return false
    }

    func assertTLKShareInCloudKit(receiverPeerID: String, senderPeerID: String, zoneID: CKRecordZone.ID, file: StaticString = #file, line: UInt = #line) {
        XCTAssertTrue(self.tlkShareInCloudKit(receiverPeerID: receiverPeerID, senderPeerID: senderPeerID, zoneID: zoneID),
                      "Should have found a TLKShare for peerID \(String(describing: receiverPeerID)) sent by \(String(describing: senderPeerID)) for \(zoneID)")
    }

    func assertMIDList(context: OTCuttlefishContext,
                       allowed: Set<String>,
                       disallowed: Set<String> = Set(),
                       unknown: Set<String> = Set(),
                       file: StaticString = #file,
                       line: UInt = #line) {
        let container = try! self.tphClient.getContainer(withContainer: context.containerName, context: context.contextID)
        container.moc.performAndWait {
            let midList = container.onqueueCurrentMIDList()
            XCTAssertEqual(midList.machineIDs(in: .allowed), allowed, "Model's allowed list should match pattern", file: file, line: line)
            XCTAssertEqual(midList.machineIDs(in: .disallowed), disallowed, "Model's disallowed list should match pattern", file: file, line: line)
            XCTAssertEqual(midList.machineIDs(in: .unknown), unknown, "Model's unknown list should match pattern", file: file, line: line)
        }

        for allowedMID in allowed {
            var err: NSError?
            let onList = context.machineID(onMemoizedList: allowedMID, error: &err)

            XCTAssertNil(err, "Should not have failed determining memoized list state", file: file, line: line)
            XCTAssertTrue(onList, "MID on allowed list should return 'is on list'", file: file, line: line)

            do {
                let egoPeerStatus = try context.egoPeerStatus()

                let numberOfPeersWithMID = egoPeerStatus.peerCountsByMachineID[allowedMID] ?? NSNumber(0)
                XCTAssert(numberOfPeersWithMID.intValue >= 0, "Should have a non-negative number for number of peers with the allowed MID", file: file, line: line)
            } catch {
                XCTFail("Should not have failed fetching the number of peers with a mid: \(error)", file: file, line: line)
            }
        }

        for disallowedMID in disallowed {
            var err: NSError?
            let onList = context.machineID(onMemoizedList: disallowedMID, error: &err)

            XCTAssertNil(err, "Should not have failed determining memoized list state", file: file, line: line)
            XCTAssertFalse(onList, "MID on allowed list should return 'not on list'", file: file, line: line)
        }
    }

    func loadSecret(label: String) throws -> (Data?) {
        var secret: Data?

        let query: [CFString: Any] = [
            kSecClass: kSecClassInternetPassword,
            kSecAttrAccessGroup: "com.apple.security.octagon",
            kSecAttrDescription: label,
            kSecReturnAttributes: true,
            kSecReturnData: true,
            kSecAttrSynchronizable: false,
            kSecMatchLimit: kSecMatchLimitOne,
        ]

        var result: CFTypeRef?
        let status = SecItemCopyMatching(query as CFDictionary, &result)

        if status != errSecSuccess || result == nil {
            throw ContainerError.failedToLoadSecret(errorCode: Int(status))
        }

        if result != nil {
            if let dictionary = result as? [CFString: Any] {
                secret = dictionary[kSecValueData] as? Data
            } else {
                throw ContainerError.failedToLoadSecretDueToType
            }
        }
        return secret
    }

    func removeEgoKeysTest(peerID: String) throws -> Bool {
        var resultSema = DispatchSemaphore(value: 0)

        let keychainManager = _SFKeychainManager.default()

        var retresultForSigningDeletion: Bool = false
        var reterrorForSigningDeletion: Error?

        // remove signing keys
        keychainManager.removeItem(withIdentifier: signingKeyIdentifier(peerID: peerID)) { result, error in
            retresultForSigningDeletion = result
            reterrorForSigningDeletion = error
            resultSema.signal()
        }

        resultSema.wait()

        if let error = reterrorForSigningDeletion {
            throw error
        }

        if retresultForSigningDeletion == false {
            return retresultForSigningDeletion
        }

        // now let's do the same thing with the encryption keys
        resultSema = DispatchSemaphore(value: 0)
        var retresultForEncryptionDeletion: Bool = false
        var reterrorForEncryptionDeletion: Error?

        keychainManager.removeItem(withIdentifier: encryptionKeyIdentifier(peerID: peerID)) { result, error in
            retresultForEncryptionDeletion = result
            reterrorForEncryptionDeletion = error
            resultSema.signal()
        }
        resultSema.wait()

        if let error = reterrorForEncryptionDeletion {
            throw error
        }

        return retresultForEncryptionDeletion && retresultForSigningDeletion
    }

    func sendContainerChange(context: OTCuttlefishContext) {
        context.notifyContainerChange(userInfo: [
            "aps": [
                "content-available": 1,
            ],
            "cf": [
                "c": "com.apple.security.keychain",
                "f": "fake-testing-function",
                "r": "fake-request-uuid",
            ],
        ])
    }

    func waitForPushToArriveAtStateMachine(context: OTCuttlefishContext) throws {
        let apsSender = try XCTUnwrap(self.cuttlefishContext.apsRateLimiter)

        let waitFor = apsSender.operationDependency.completionHandlerDidRunCondition

        XCTAssertEqual(0, waitFor.wait(10 * NSEC_PER_SEC))
    }

    func sendTTRRequest(context: OTCuttlefishContext) {
        context.notifyContainerChange(userInfo: [
            "aps": [
                "content-available": 1,
            ],
            "cf": [
                "c": "com.apple.security.keychain",
                "f": "fake-testing-function",
                "k": "r",
                "a": "title",
                "d": "description",
                "R": "radarNumber",
            ],
        ])
    }

    func sendContainerChangeWaitForFetch(context: OTCuttlefishContext) {
        self.sendContainerChangeWaitForFetchForStates(context: context, states: [OctagonStateReadyUpdated, OctagonStateReady])
    }

    func sendContainerChangeWaitForUntrustedFetch(context: OTCuttlefishContext) {
        self.sendContainerChangeWaitForFetchForStates(context: context, states: [OctagonStateUntrustedUpdated, OctagonStateUntrusted])
    }

    func sendContainerChangeWaitForInheritedFetch(context: OTCuttlefishContext) {
        self.sendContainerChangeWaitForFetchForStates(context: context, states: [OctagonStateInherited])
    }

    // Please ensure that the first state in this list is not the state that the context is currently in
    func sendContainerChangeWaitForFetchForStates(context: OTCuttlefishContext, states: [String]) {

        // If we wait for each condition in order here, we might lose thread races and cause issues.
        // Fetch every state we'll examine (note that this doesn't handle repeated checks for the same state)

        let stateConditions = states.map { ($0, context.stateMachine.stateConditions[$0]!) }

        let updateTrustExpectation = self.expectation(description: "fetchChanges")

        self.fakeCuttlefishServer.fetchChangesListener = { _ in
            self.fakeCuttlefishServer.fetchChangesListener = nil
            updateTrustExpectation.fulfill()
            return nil
        }
        context.notifyContainerChange(nil)
        self.wait(for: [updateTrustExpectation], timeout: 10)

        for (state, stateCondition) in stateConditions {
            XCTAssertEqual(0, stateCondition.wait(10 * NSEC_PER_SEC), "State machine should enter '\(String(describing: state))'")
            if state == OctagonStateReady || state == OctagonStateUntrusted {
                XCTAssertEqual(0, context.stateMachine.paused.wait(10 * NSEC_PER_SEC), "State machine should pause soon")
            }
        }
    }

    func createSOSPeer(peerID: String) -> CKKSSOSSelfPeer {
        let peer = CKKSSOSSelfPeer(sosPeerID: peerID,
                                   encryptionKey: _SFECKeyPair.init(randomKeyPairWith: _SFECKeySpecifier.init(curve: SFEllipticCurve.nistp384))!,
                                   signingKey: _SFECKeyPair.init(randomKeyPairWith: _SFECKeySpecifier.init(curve: SFEllipticCurve.nistp384))!,
                                   viewList: self.managedViewList())

        saveToKeychain(keyPair: peer.signingKey, label: "com.apple.securityd." + peerID + ".sossigningkey")
        saveToKeychain(keyPair: peer.encryptionKey, label: "com.apple.securityd." + peerID + ".sosencryptionkey")

        return peer
    }

    func makeInitiatorDeviceInfoAdapter() -> OTMockDeviceInfoAdapter {
        // Note that the type of your initiator changes based on the platform you're currently on
        return OTMockDeviceInfoAdapter(modelID: self.mockDeviceInfo.modelID(),
                                       deviceName: "test-device-2",
                                       serialNumber: "456",
                                       osVersion: "iSomething (fake version)")
    }

    func makeInitiatorContext(contextID: String) -> OTCuttlefishContext {
        return self.makeInitiatorContext(contextID: contextID, authKitAdapter: self.mockAuthKit2)
    }

    func makeInitiatorContext(contextID: String, authKitAdapter: OTAuthKitAdapter) -> OTCuttlefishContext {
        return self.makeInitiatorContext(contextID: contextID, authKitAdapter: authKitAdapter, sosAdapter: self.mockSOSAdapter)
    }

    func makeInitiatorContext(contextID: String, authKitAdapter: OTAuthKitAdapter, sosAdapter: OTSOSAdapter) -> OTCuttlefishContext {
        // Note that the type of your initiator changes based on the platform you're currently on
        return self.manager.context(forContainerName: OTCKContainerName,
                                    contextID: contextID,
                                    sosAdapter: sosAdapter,
                                    authKitAdapter: authKitAdapter,
                                    tooManyPeersAdapter: self.mockTooManyPeers,
                                    lockStateTracker: self.lockStateTracker,
                                    accountStateTracker: self.accountStateTracker,
                                    deviceInformationAdapter: self.makeInitiatorDeviceInfoAdapter())
    }

    @discardableResult
    func assertResetAndBecomeTrustedInDefaultContext(file: StaticString = #file, line: UInt = #line) -> String {
        let ret = self.assertResetAndBecomeTrusted(context: self.cuttlefishContext)

        // And, the default context runs CKKS:
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC, file: file, line: line)
        self.verifyDatabaseMocks()
        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext, file: file, line: line)

        return ret
    }

    @discardableResult
    func assertResetAndBecomeTrusted(context: OTCuttlefishContext, file: StaticString = #file, line: UInt = #line) -> String {
        context.startOctagonStateMachine()
        XCTAssertNoThrow(try context.setCDPEnabled(), file: file, line: line)
        self.assertEnters(context: context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC, file: file, line: line)

        let trustChangeNotificationExpectation = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: "com.apple.security.octagon.trust-status-change"))
        let cliqueChangedNotificationExpectation = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: OTCliqueChanged))

        // Rejoining and creating a new syncing policy will change this status
        let ucvStatusChangeNotificationExpectation = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: OTUserControllableViewStatusChanged))

        do {
            let arguments = OTConfigurationContext()
            arguments.altDSID = try context.authKitAdapter.primaryiCloudAccountAltDSID()
            arguments.context = context.contextID
            arguments.otControl = self.otControl

            let clique = try OTClique.newFriends(withContextData: arguments, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil", file: file, line: line)
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)", file: file, line: line)
        }

        self.assertEnters(context: context, state: OctagonStateReady, within: 10 * NSEC_PER_SEC, file: file, line: line)
        self.assertConsidersSelfTrusted(context: context, file: file, line: line)

        self.wait(for: [trustChangeNotificationExpectation, cliqueChangedNotificationExpectation, ucvStatusChangeNotificationExpectation], timeout: 10)

        return try! context.accountMetadataStore.getEgoPeerID()
    }

    @discardableResult
    func assertJoinViaEscrowRecovery(joiningContext: OTCuttlefishContext, sponsor: OTCuttlefishContext, file: StaticString = #file, line: UInt = #line) -> String {
        do {
            joiningContext.startOctagonStateMachine()

            let sponsorPeerID = try sponsor.accountMetadataStore.loadOrCreateAccountMetadata().peerID
            XCTAssertNotNil(sponsorPeerID, "sponsorPeerID should not be nil", file: file, line: line)
            let entropy = try self.loadSecret(label: sponsorPeerID!)
            XCTAssertNotNil(entropy, "entropy should not be nil", file: file, line: line)

            let altDSID = try joiningContext.authKitAdapter.primaryiCloudAccountAltDSID()
            XCTAssertNotNil(altDSID, "Should have an altDSID", file: file, line: line)

            let bottles = self.fakeCuttlefishServer.state.bottles.filter { $0.peerID == sponsorPeerID }
            XCTAssertEqual(bottles.count, 1, "Should have a single bottle for the approving peer", file: file, line: line)
            let bottle = bottles[0]

            let cliqueChangedNotificationExpectation = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: OTCliqueChanged))
            let ucvStatusChangeNotificationExpectation = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: OTUserControllableViewStatusChanged))

            let joinWithBottleExpectation = self.expectation(description: "joinWithBottle callback occurs")
            joiningContext.join(withBottle: bottle.bottleID, entropy: entropy!, bottleSalt: altDSID) { error in
                XCTAssertNil(error, "error should be nil", file: file, line: line)
                joinWithBottleExpectation.fulfill()
            }
            self.wait(for: [joinWithBottleExpectation], timeout: 10)

            self.assertEnters(context: joiningContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC, file: file, line: line)
            self.assertConsidersSelfTrusted(context: joiningContext, file: file, line: line)

            self.wait(for: [cliqueChangedNotificationExpectation, ucvStatusChangeNotificationExpectation], timeout: 10)

            return try joiningContext.accountMetadataStore.getEgoPeerID()
        } catch {
            XCTFail("Expected no error: \(error)", file: file, line: line)
            return "failed"
        }
    }

    @discardableResult
    func assertJoinViaProximitySetup(joiningContext: OTCuttlefishContext, sponsor: OTCuttlefishContext, file: StaticString = #file, line: UInt = #line) -> String {
        do {
            joiningContext.startOctagonStateMachine()
            self.assertEnters(context: joiningContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC, file: file, line: line)

            let (sponsorPairingChannel, initiatorPairingChannel) = self.setupPairingChannels(initiator: joiningContext, sponsor: sponsor)

            let firstInitiatorPacket = self.sendPairingExpectingReply(channel: initiatorPairingChannel, packet: nil, reason: "session initialization")
            let sponsorEpochPacket = self.sendPairingExpectingReply(channel: sponsorPairingChannel, packet: firstInitiatorPacket, reason: "sponsor epoch")

            let ucvStatusChangeNotificationExpectation = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: OTUserControllableViewStatusChanged))
            let initiatorIdentityPacket = self.sendPairingExpectingReply(channel: initiatorPairingChannel, packet: sponsorEpochPacket, reason: "initiator identity")
            self.wait(for: [ucvStatusChangeNotificationExpectation], timeout: 10)

            let sponsorVoucherPacket = self.sendPairingExpectingCompletionAndReply(channel: sponsorPairingChannel, packet: initiatorIdentityPacket, reason: "sponsor voucher")

            let cliqueChangedNotificationExpectation = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: OTCliqueChanged))
            self.sendPairingExpectingCompletion(channel: initiatorPairingChannel, packet: sponsorVoucherPacket, reason: "initiator completion")

            self.assertEnters(context: joiningContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC, file: file, line: line)
            self.assertConsidersSelfTrusted(context: joiningContext, file: file, line: line)
            self.wait(for: [cliqueChangedNotificationExpectation], timeout: 10)

            XCTAssertNil(joiningContext.pairingUUID, "pairingUUID should be nil", file: file, line: line)

            return try joiningContext.accountMetadataStore.getEgoPeerID()
        } catch {
            XCTFail("Expected no error: \(error)", file: file, line: line)
            return "failed"
        }
    }

    @discardableResult
    func assertJoinViaEscrowRecoveryFromDefaultContextWithReciprocationAndTLKShares(joiningContext: OTCuttlefishContext, file: StaticString = #file, line: UInt = #line) -> String {
        let joinedPeerID = self.assertJoinViaEscrowRecovery(joiningContext: joiningContext, sponsor: self.cuttlefishContext)

        // And respond from the default context
        self.assertAllCKKSViewsUpload(tlkShares: 1, file: file, line: line)
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC, file: file, line: line)

        self.verifyDatabaseMocks()
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC, file: file, line: line)

        self.verifyDatabaseMocks()
        self.assertCKKSStateMachine(enters: CKKSStateReady, within: 10 * NSEC_PER_SEC, file: file, line: line)
        self.assertTLKSharesInCloudKit(receiver: joiningContext, sender: self.cuttlefishContext, file: file, line: line)

        return joinedPeerID
    }

    func assertSelfOSVersion(_ osVersion: String, file: StaticString = #file, line: UInt = #line) {
        let statusExpectation = self.expectation(description: "status callback occurs")
        self.tphClient.dumpEgoPeer(withContainer: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) { _, _, stableInfo, _, error in
            XCTAssertNil(error, "should be no error dumping ego peer", file: file, line: line)
            XCTAssertEqual(stableInfo?.osVersion, osVersion, "os version should be as required", file: file, line: line)
            statusExpectation.fulfill()
        }

        self.wait(for: [statusExpectation], timeout: 2)
    }
}

class OctagonTests: OctagonTestsBase {
    func testTPHPrepare() {
        self.startCKAccountStatusMock()

        let contextName = "asdf"
        let containerName = "test_container"

        let tphPrepareExpectation = self.expectation(description: "prepare callback occurs")
        tphClient.prepare(withContainer: containerName,
                          context: contextName,
                          epoch: 0,
                          machineID: "asdf",
                          bottleSalt: "123456789",
                          bottleID: UUID().uuidString,
                          modelID: "iPhone9,1",
                          deviceName: "asdf",
                          serialNumber: "1234",
                          osVersion: "asdf",
                          policyVersion: nil,
                          policySecrets: nil,
                          syncUserControllableViews: .UNKNOWN,
                          secureElementIdentity: nil,
                          setting: nil,
                          signingPrivKeyPersistentRef: nil,
                          encPrivKeyPersistentRef: nil) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, _, error in
            XCTAssertNil(error, "Should be no error preparing identity")
            XCTAssertNotNil(peerID, "Should be a peer ID")
            XCTAssertNotNil(permanentInfo, "Should have a permenent info")
            XCTAssertNotNil(permanentInfoSig, "Should have a permanent info signature")
            XCTAssertNotNil(stableInfo, "Should have a stable info")
            XCTAssertNotNil(stableInfoSig, "Should have a stable info signature")

            let adapter = OctagonCKKSPeerAdapter(peerID: peerID!, containerName: containerName, contextID: contextName, cuttlefishXPC: CuttlefishXPCWrapper(cuttlefishXPCConnection: self.tphXPCProxy.connection()))

            do {
                let selves = try adapter.fetchSelfPeers()
                try TestsObjectiveC.testSecKey(selves)
            } catch {
                XCTFail("Test failed: \(error)")
            }

            tphPrepareExpectation.fulfill()
        }

        self.wait(for: [tphPrepareExpectation], timeout: 10)
    }

    func testTPHMultiPrepare() throws {
        self.startCKAccountStatusMock()

        let contextName = OTDefaultContext
        let containerName = OTCKContainerName

        var firstPeerID: String?

        let tphPrepareExpectation = self.expectation(description: "prepare callback occurs")
        tphClient.prepare(withContainer: containerName,
                          context: contextName,
                          epoch: 0,
                          machineID: "asdf",
                          bottleSalt: "123456789",
                          bottleID: UUID().uuidString,
                          modelID: "iPhone9,1",
                          deviceName: "asdf",
                          serialNumber: "1234",
                          osVersion: "asdf",
                          policyVersion: nil,
                          policySecrets: nil,
                          syncUserControllableViews: .UNKNOWN,
                          secureElementIdentity: nil,
                          setting: nil,
                          signingPrivKeyPersistentRef: nil,
                          encPrivKeyPersistentRef: nil) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, _, error in
                            XCTAssertNil(error, "Should be no error preparing identity")
                            XCTAssertNotNil(peerID, "Should be a peer ID")
                            XCTAssertNotNil(permanentInfo, "Should have a permenent info")
                            XCTAssertNotNil(permanentInfoSig, "Should have a permanent info signature")
                            XCTAssertNotNil(stableInfo, "Should have a stable info")
                            XCTAssertNotNil(stableInfoSig, "Should have a stable info signature")
                            firstPeerID = peerID

                            tphPrepareExpectation.fulfill()
        }

        self.wait(for: [tphPrepareExpectation], timeout: 10)

        let tphPrepareExpectation2 = self.expectation(description: "prepare callback occurs")
        tphClient.prepare(withContainer: containerName,
                          context: "a_different_context",
                          epoch: 0,
                          machineID: "asdf",
                          bottleSalt: "123456789",
                          bottleID: UUID().uuidString,
                          modelID: "iPhone9,1",
                          deviceName: "asdf",
                          serialNumber: "1234",
                          osVersion: "asdf",
                          policyVersion: nil,
                          policySecrets: nil,
                          syncUserControllableViews: .UNKNOWN,
                          secureElementIdentity: nil,
                          setting: nil,
                          signingPrivKeyPersistentRef: nil,
                          encPrivKeyPersistentRef: nil) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, _, error in
                            XCTAssertNil(error, "Should be no error preparing identity")
                            XCTAssertNotNil(peerID, "Should be a peer ID")
                            XCTAssertNotNil(permanentInfo, "Should have a permenent info")
                            XCTAssertNotNil(permanentInfoSig, "Should have a permanent info signature")
                            XCTAssertNotNil(stableInfo, "Should have a stable info")
                            XCTAssertNotNil(stableInfoSig, "Should have a stable info signature")

                            XCTAssertNotEqual(peerID, firstPeerID, "Should not be the same peer ID as before")

                            tphPrepareExpectation2.fulfill()
        }

        self.wait(for: [tphPrepareExpectation2], timeout: 10)
    }

    func testLoadToUntrusted() throws {
        self.startCKAccountStatusMock()

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // With no identity but AuthKit reporting an existing iCloud account, Octagon should go directly into 'untrusted'
        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)
    }

    func testLoadToUntrustedIfTPHHasPreparedIdentityOnly() throws {
        self.startCKAccountStatusMock()

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        // Prepare an identity, then pretend like securityd thought it was in the right account
        let containerName = OTCKContainerName
        let contextName = OTDefaultContext

        var selfPeerID: String?
        let prepareExpectation = self.expectation(description: "prepare callback occurs")
        tphClient.prepare(withContainer: containerName,
                          context: contextName,
                          epoch: 0,
                          machineID: "asdf",
                          bottleSalt: "123456789",
                          bottleID: UUID().uuidString,
                          modelID: "iPhone9,1",
                          deviceName: "asdf",
                          serialNumber: "1234",
                          osVersion: "asdf",
                          policyVersion: nil,
                          policySecrets: nil,
                          syncUserControllableViews: .UNKNOWN,
                          secureElementIdentity: nil,
                          setting: nil,
                          signingPrivKeyPersistentRef: nil,
                          encPrivKeyPersistentRef: nil) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, _, error in
                            XCTAssertNil(error, "Should be no error preparing identity")
                            XCTAssertNotNil(peerID, "Should be a peer ID")
                            XCTAssertNotNil(permanentInfo, "Should have a permenent info")
                            XCTAssertNotNil(permanentInfoSig, "Should have a permanent info signature")
                            XCTAssertNotNil(stableInfo, "Should have a stable info")
                            XCTAssertNotNil(stableInfoSig, "Should have a stable info signature")
                            selfPeerID = peerID

                            prepareExpectation.fulfill()
        }
        self.wait(for: [prepareExpectation], timeout: 10)

        let account = OTAccountMetadataClassC()!
        account.peerID = selfPeerID
        account.icloudAccountState = .ACCOUNT_AVAILABLE
        account.trustState = .TRUSTED

        XCTAssertNoThrow(try account.saveToKeychain(forContainer: containerName, contextID: contextName), "Should be no error saving fake account metadata")

        self.cuttlefishContext.startOctagonStateMachine()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        // CKKS should be waiting for assistance
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)
    }

    func testNewFriendsForEmptyAccount() throws {
        self.startCKAccountStatusMock()

        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)

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

        // and the act of calling newFriends should set the CDP bit
        XCTAssertEqual(self.fetchCDPStatus(context: self.cuttlefishContext), .enabled, "CDP status should be 'enabled'")

        // and all subCKKSes should enter ready...
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)

        // Also, CKKS should be configured with the prevailing policy version
        XCTAssertNotNil(self.defaultCKKS.syncingPolicy, "Should have given CKKS a TPPolicy during initialization")
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.version, prevailingPolicyVersion, "Policy given to CKKS should be prevailing policy")

        // TODO: add a CKKS item
    }

    func testNewFriendsWithResetReasonForEmptyAccount() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        let resetExpectation = self.expectation(description: "resetExpectation")

        self.fakeCuttlefishServer.resetListener = {  request in
            self.fakeCuttlefishServer.resetListener = nil
            resetExpectation.fulfill()
            XCTAssertTrue(request.resetReason.rawValue == CuttlefishResetReason.testGenerated.rawValue, "reset reason should be unknown")
            return nil
        }

        do {
            let clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
        }

        self.wait(for: [resetExpectation], timeout: 10)

        // Now, we should be in 'ready'
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertConsidersSelfTrustedCachedAccountStatus(context: self.cuttlefishContext)

        // and all subCKKSes should enter ready...
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
    }

    func testRequestToJoinCircleForEmptyAccount() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique = OTClique(contextData: self.otcliqueContext)

        // Now, call requestToJoin. It should cause an establish to happen
        try clique.requestToJoinCircle()

        // Now, we should be in 'ready'
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertConsidersSelfTrustedCachedAccountStatus(context: self.cuttlefishContext)

        // and all subCKKSes should enter ready...
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)

        // But calling it again shouldn't make establish() occur again
        self.fakeCuttlefishServer.establishListener = { _ in
            XCTFail("establish called at incorrect time")
            return nil
        }

        // call should be a nop
        try clique.requestToJoinCircle()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testCliqueStatusWhileLocked() throws {
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

        // Check that we handled locked devices too
        self.aksLockState = true
        self.lockStateTracker.recheck()
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext, isLocked: true)

        // and then unlocked again
        self.aksLockState = false
        self.lockStateTracker.recheck()
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext, isLocked: false)
    }

    func testDeviceFetchRetry() throws {
        self.startCKAccountStatusMock()
        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let ckError = FakeCuttlefishServer.makeCloudKitCuttlefishError(code: .transactionalFailure)
        self.fakeCuttlefishServer.nextFetchErrors.append(ckError)

        self.sendContainerChange(context: self.cuttlefishContext)

        do {
            let clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
        }

        // Now, we should be in 'ready'
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)

        self.sendContainerChange(context: self.cuttlefishContext)
    }

    func testDeviceFetchRetryFail() throws {
        self.startCKAccountStatusMock()

        self.assertResetAndBecomeTrustedInDefaultContext()

        let ckError = FakeCuttlefishServer.makeCloudKitCuttlefishError(code: .transactionalFailure)
        self.fakeCuttlefishServer.nextFetchErrors.append(ckError)
        self.fakeCuttlefishServer.nextFetchErrors.append(ckError)

        self.sendContainerChangeWaitForFetchForStates(context: self.cuttlefishContext, states: [OctagonStateReadyUpdated])
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 30 * NSEC_PER_SEC)
    }

    func testNewFriendsForEmptyAccountReturnsMoreChanges() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.fakeCuttlefishServer.nextEstablishReturnsMoreChanges = true

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

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)
    }

    func testNewFriendsForEmptyAccountWithoutTLKsResetsZones() throws {
        self.putFakeKeyHierarchiesInCloudKit()
        // But do NOT add them to the keychain

        // CKKS+Octagon should reset the zones and be ready

        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // CKKS should reset the zones after Octagon has entered
        self.silentZoneDeletesAllowed = true

        do {
            let clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
        }

        // Now, we should be in 'ready'
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        // and all subCKKSes should enter ready (eventually)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func testUploadTLKsRetry() throws {
        self.putFakeKeyHierarchiesInCloudKit()
        // But do NOT add them to the keychain

        // CKKS+Octagon should reset the zones and be ready

        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // CKKS should reset the zones after Octagon has entered
        self.silentZoneDeletesAllowed = true

        do {
            let clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
        }

        // Now, we should be in 'ready'
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        let ckError = FakeCuttlefishServer.makeCloudKitCuttlefishError(code: .transactionalFailure)
        self.fakeCuttlefishServer.nextUpdateTrustErrors.append(ckError)

        // and all subCKKSes should enter ready (eventually)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func testUploadTLKsRetryFail() throws {
        // Note that Octagon now resets CKKS during a reset if there aren't keys
        // So, this test needs to fail after Octagon is already in

        // CKKS+Octagon should reset the zones and be ready

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
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 100 * NSEC_PER_SEC)

        // CKKS views reset themselves
        // CKKS should reset the zones after Octagon has entered
        self.silentZoneDeletesAllowed = true

        self.resetAllCKKSViews()

        let ckError = FakeCuttlefishServer.makeCloudKitCuttlefishError(code: .transactionalFailure)
        self.fakeCuttlefishServer.nextUpdateTrustErrors.append(ckError)
        self.fakeCuttlefishServer.nextUpdateTrustErrors.append(ckError)
        self.fakeCuttlefishServer.nextUpdateTrustErrors.append(ckError)
        self.fakeCuttlefishServer.nextUpdateTrustErrors.append(ckError)
        self.fakeCuttlefishServer.nextUpdateTrustErrors.append(ckError)
        self.fakeCuttlefishServer.nextUpdateTrustErrors.append(ckError)

        // these should be failures
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, ckksState: CKKSStateReady, within: 10 * NSEC_PER_SEC)
        self.cuttlefishContext.rpcStatus { _, _ in
        }

        self.verifyDatabaseMocks()
        XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(60 * NSEC_PER_SEC), "Main cuttlefish context should quiesce before the test ends")
    }

    func testNewFriendsForEmptyAccountWithExistingTLKs() throws {
        self.putFakeKeyHierarchiesInCloudKit()
        self.saveTLKMaterialToKeychain()

        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTrust, within: 10 * NSEC_PER_SEC)

        do {
            let clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
        }

        // Now, we should be in 'ready'
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
    }

    func testLoadToReadyOnRestart() throws {
        let startDate = Date()

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

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        // Ensure CKKS has a policy after newFriends
        XCTAssertNotNil(self.defaultCKKS.syncingPolicy, "Should have given CKKS a TPPolicy during initialization")
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.version, prevailingPolicyVersion, "Policy given to CKKS should be prevailing policy")

        let peerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        XCTAssertNotNil(peerID, "Should have a peer ID after making new friends")

        var readyDate = CKKSAnalytics.logger().dateProperty(forKey: OctagonAnalyticsLastKeystateReady)
        XCTAssertNotNil(readyDate, "Should have a ready date")
        XCTAssert(try XCTUnwrap(readyDate) > startDate, "ready date should be after startdate")

        // Now restart the context
        self.manager.removeContext(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.restartCKKSViews()
        self.cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        XCTAssertNil(self.defaultCKKS.syncingPolicy, "CKKS should not have a policy after 'restart'")

        let restartDate = Date()
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.assertConsidersSelfTrustedCachedAccountStatus(context: self.cuttlefishContext)

        let restartedPeerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        XCTAssertNotNil(restartedPeerID, "Should have a peer ID after restarting")

        XCTAssertEqual(peerID, restartedPeerID, "Should have the same peer ID after restarting")
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertNotNil(self.defaultCKKS.syncingPolicy, "Should have given CKKS a TPPolicy after restart")
        XCTAssertEqual(self.defaultCKKS.syncingPolicy?.version, prevailingPolicyVersion, "Policy given to CKKS after restart should be prevailing policy")

        readyDate = CKKSAnalytics.logger().dateProperty(forKey: OctagonAnalyticsLastKeystateReady)
        XCTAssertNotNil(readyDate, "Should have a ready date")
        XCTAssert(try XCTUnwrap(readyDate) > restartDate, "ready date should be after re-startdate")
    }

    func testFillInUnknownAttemptedJoinState() throws {
        self.startCKAccountStatusMock()

        _ = self.assertResetAndBecomeTrustedInDefaultContext()

        try self.cuttlefishContext.accountMetadataStore.persistAccountChanges { metadata in
            metadata.attemptedJoin = .UNKNOWN
            return metadata
        }

        self.manager.removeContext(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.restartCKKSViews()
        self.cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        // And check that the metadata is fixed:
        let metadata = try self.cuttlefishContext.accountMetadataStore.loadOrCreateAccountMetadata()
        XCTAssertEqual(metadata.attemptedJoin, .ATTEMPTED, "Should have attempted a join")
    }

    func testLoadToUntrustedOnRestartIfKeysGone() throws {
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

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        let peerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        XCTAssertNotNil(peerID, "Should have a peer ID after making new friends")

        // Delete the local keys!
        let query: [CFString: Any] = [
            kSecClass: kSecClassKey,
            kSecAttrAccessGroup: "com.apple.security.egoIdentities",
            kSecUseDataProtectionKeychain: true,
        ]

        let status = SecItemDelete(query as CFDictionary)
        XCTAssertEqual(errSecSuccess, status, "should be able to delete every key")

        // Now restart the context
        self.manager.removeContext(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.restartCKKSViews()
        self.cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
    }

    func testLoadToUntrustedOnRestartIfTPHLosesAllData() throws {
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

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        let peerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        XCTAssertNotNil(peerID, "Should have a peer ID after making new friends")

        // TPH goes mad!
        let resetExpectation = self.expectation(description: "reset callback occurs")
        self.tphClient.localReset(withContainer: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) { error in
            XCTAssertNil(error, "Should be no error performing a local reset")
            resetExpectation.fulfill()
        }
        self.wait(for: [resetExpectation], timeout: 10)

        // Now restart the context
        self.manager.removeContext(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.restartCKKSViews()
        self.cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTrust, within: 10 * NSEC_PER_SEC)
    }

    func testLoadToTrustedOnRestartIfMismatchedPeerIDs() throws {
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

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        let peerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        XCTAssertNotNil(peerID, "Should have a peer ID after making new friends")

        try self.cuttlefishContext.accountMetadataStore.persistNewEgoPeerID("mismatched-peer-id")
        let mismatchedPeerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        XCTAssertNotEqual(peerID, mismatchedPeerID, "Peer ID should be ruined")

        // Now restart the context. It should accept TPH's view of the world...
        self.manager.removeContext(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.restartCKKSViews()
        self.cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        let newPeerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        XCTAssertEqual(peerID, newPeerID, "Should now have TPH's peer ID")
    }

    func testFetchPeerID() throws {
        self.startCKAccountStatusMock()

        let clique: OTClique?
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        self.verifyDatabaseMocks()

        let memberIdentifier = try XCTUnwrap(clique).cliqueMemberIdentifier

        let dumpExpectation = self.expectation(description: "dump callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) { dump, error in
            XCTAssertNil(error, "Should be no error dumping data")
            XCTAssertNotNil(dump, "dump should not be nil")
            do {
                let realDump = try XCTUnwrap(dump, "dump should not be nil")
                let egoSelf = try XCTUnwrap(realDump["self"] as? [String: AnyObject], "egoSelf should not be nil")
                let peerID = try XCTUnwrap(egoSelf["peerID"] as? String, "peerID should not be nil")
                XCTAssertEqual(memberIdentifier, peerID, "memberIdentifier should match tph's peer ID")
            } catch {
                XCTFail("Test failed by exception: \(error)")
            }

            dumpExpectation.fulfill()
        }
        self.wait(for: [dumpExpectation], timeout: 10)
    }

    func testLeaveClique() throws {
        self.startCKAccountStatusMock()

        self.assertResetAndBecomeTrustedInDefaultContext()
        XCTAssertTrue(self.mockSOSAdapter.ckks4AllStatusIsSet, "SOS adapter should have been told that CKKS4All is enabled")
        XCTAssertTrue(self.mockSOSAdapter.ckks4AllStatus, "SOS adapter should have been told that CKKS4All is enabled")
        self.mockSOSAdapter.ckks4AllStatusIsSet = false

        // Octagon should refuse to leave
        let clique = self.cliqueFor(context: self.cuttlefishContext)
        XCTAssertThrowsError(try clique.leave(), "Should be an error departing clique when is only peer")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        // until there's another peer around
        let joiningContext = self.makeInitiatorContext(contextID: "joiner", authKitAdapter: self.mockAuthKit2)
        self.assertJoinViaEscrowRecoveryFromDefaultContextWithReciprocationAndTLKShares(joiningContext: joiningContext)

        XCTAssertNoThrow(try clique.leave(), "Should be NO error departing clique")

        // securityd should now consider itself untrusted
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTrust, within: 10 * NSEC_PER_SEC)

        XCTAssertTrue(self.mockSOSAdapter.ckks4AllStatusIsSet, "SOS adapter should have been told that CKKS4All is not enabled")
        XCTAssertFalse(self.mockSOSAdapter.ckks4AllStatus, "SOS adapter should have been told that CKKS4All is not enabled")

        // TODO: an item added here shouldn't sync
    }

    func testLeaveCliqueWhenNotInAccount() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let leaveExpectation = self.expectation(description: "leave callback occurs")

        self.cuttlefishContext.rpcLeaveClique { error in
            XCTAssertNil(error, "error should be nil")
            leaveExpectation.fulfill()
        }
        self.wait(for: [leaveExpectation], timeout: 5)

        // securityd should still consider itself untrusted
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)
    }

    func testLeaveCliqueWhenLocked() throws {
        self.startCKAccountStatusMock()

        self.assertResetAndBecomeTrustedInDefaultContext()
        XCTAssertTrue(self.mockSOSAdapter.ckks4AllStatusIsSet, "SOS adapter should have been told that CKKS4All is enabled")
        XCTAssertTrue(self.mockSOSAdapter.ckks4AllStatus, "SOS adapter should have been told that CKKS4All is enabled")
        self.mockSOSAdapter.ckks4AllStatusIsSet = false

        // Octagon should refuse to leave
        let clique = self.cliqueFor(context: self.cuttlefishContext)
        XCTAssertThrowsError(try clique.leave(), "Should be an error departing clique when is only peer")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        // until there's another peer around
        let joiningContext = self.makeInitiatorContext(contextID: "joiner", authKitAdapter: self.mockAuthKit2)
        self.assertJoinViaEscrowRecoveryFromDefaultContextWithReciprocationAndTLKShares(joiningContext: joiningContext)

        // lock device
        self.lockStateProvider.aksCurrentlyLocked = true
        self.cuttlefishContext.stateMachine.lockStateTracker?.recheck()

        XCTAssertThrowsError(try clique.leave(), "Should be an error departing clique when device is locked")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForUnlock, within: 10 * NSEC_PER_SEC)

        // unlock device
        self.lockStateProvider.aksCurrentlyLocked = false
        self.cuttlefishContext.stateMachine.lockStateTracker?.recheck()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertNoThrow(try clique.leave(), "should be able to leave clique")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
    }

    func testCliqueFriendAPI() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.verifyDatabaseMocks()
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        let peer1ID = fetchEgoPeerID()

        do {
            let peersByID = try clique.peerDeviceNamesByPeerID()
            XCTAssertNotNil(peersByID, "Should have received information on peers")
            XCTAssertTrue(peersByID.isEmpty, "peer1 should report no trusted peers")
        } catch {
            XCTFail("Error thrown: \(error)")
        }

        let peer2DeviceName = "peer2-asdf"

        let peer2ModelID: String
        if self.mockDeviceInfo.mockModelID.contains("AudioAccessory") {
            // HomePods cannot introduce TVs (at least on policy v9)
            peer2ModelID = "AudioAccessory1,1"
        } else {
            peer2ModelID = "AppleTV5,3"
        }

        let peer2DeviceAdapter = OTMockDeviceInfoAdapter(modelID: peer2ModelID,
                                                         deviceName: peer2DeviceName,
                                                         serialNumber: "peer2-asdf",
                                                         osVersion: "tvOS (fake version)")

        // Now, fake up a voucher for the second peer using TPH
        let peer2ContextID = "asdf"
        let peer2 = self.manager.context(forContainerName: OTCKContainerName,
                                         contextID: peer2ContextID,
                                         sosAdapter: self.mockSOSAdapter,
                                         authKitAdapter: self.mockAuthKit2,
                                         tooManyPeersAdapter: self.mockTooManyPeers,
                                         lockStateTracker: self.lockStateTracker,
                                         accountStateTracker: self.accountStateTracker,
                                         deviceInformationAdapter: peer2DeviceAdapter)

        self.setAllowListToCurrentAuthKit(container: OTCKContainerName, context: peer2ContextID, accountIsDemo: false)

        var peer2ID: String!
        let joinExpectation = self.expectation(description: "join callback occurs")
        self.tphClient.prepare(withContainer: OTCKContainerName,
                               context: peer2ContextID,
                               epoch: 1,
                               machineID: self.mockAuthKit2.currentMachineID,
                               bottleSalt: "123456789",
                               bottleID: UUID().uuidString,
                               modelID: peer2DeviceAdapter.modelID(),
                               deviceName: peer2DeviceAdapter.deviceName(),
                               serialNumber: peer2DeviceAdapter.serialNumber(),
                               osVersion: peer2DeviceAdapter.osVersion(),
                               policyVersion: nil,
                               policySecrets: nil,
                               syncUserControllableViews: .UNKNOWN,
                               secureElementIdentity: nil,
                               setting: nil,
                               signingPrivKeyPersistentRef: nil,
                               encPrivKeyPersistentRef: nil) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, _, error in
                                XCTAssertNil(error, "Should be no error preparing identity")
                                XCTAssertNotNil(permanentInfo, "Should have a permanent identity")
                                XCTAssertNotNil(permanentInfoSig, "Should have a permanent identity signature")
                                XCTAssertNotNil(stableInfo, "Should have a stable info")
                                XCTAssertNotNil(stableInfoSig, "Should have a stable info signature")
                                self.tphClient.vouch(withContainer: self.cuttlefishContext.containerName,
                                                     context: self.cuttlefishContext.contextID,
                                                     peerID: peerID!,
                                                     permanentInfo: permanentInfo!,
                                                     permanentInfoSig: permanentInfoSig!,
                                                     stableInfo: stableInfo!,
                                                     stableInfoSig: stableInfoSig!,
                                                     ckksKeys: []) { voucher, voucherSig, error in
                                                        XCTAssertNil(error, "Should be no error vouching")
                                                        XCTAssertNotNil(voucher, "Should have a voucher")
                                                        XCTAssertNotNil(voucherSig, "Should have a voucher signature")
                                                        self.tphClient.join(withContainer: OTCKContainerName,
                                                                            context: peer2ContextID,
                                                                            voucherData: voucher!,
                                                                            voucherSig: voucherSig!,
                                                                            ckksKeys: [],
                                                                            tlkShares: [],
                                                                            preapprovedKeys: []) { peerID, _, _, error in
                                                                                XCTAssertNil(error, "Should be no error joining")
                                                                                XCTAssertNotNil(peerID, "Should have a peerID")
                                                                                peer2ID = peerID
                                                                                joinExpectation.fulfill()
                                                        }
                                }
        }
        self.wait(for: [joinExpectation], timeout: 10)

        // Lie about the current account state
        let account = OTAccountMetadataClassC()!
        account.peerID = peer2ID
        account.icloudAccountState = .ACCOUNT_AVAILABLE
        account.trustState = .TRUSTED
        account.attemptedJoin = . ATTEMPTED
        XCTAssertNoThrow(try account.saveToKeychain(forContainer: OTCKContainerName, contextID: peer2ContextID), "Should be no error saving fake account metadata")

        peer2.startOctagonStateMachine()
        self.assertEnters(context: peer2, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: peer2)

        // Now that we've lied enough...
        self.assertAllCKKSViewsUpload(tlkShares: 1) { $0.zoneName == "LimitedPeersAllowed" }
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer2ID)),
                      "peer 1 should trust peer 2")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer2ID, opinion: .trusts, target: peer1ID)),
                      "peer 2 should trust peer 1")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // But does peer1 claim to know peer2?
        do {
            let peersByID = try clique.peerDeviceNamesByPeerID()
            XCTAssertNotNil(peersByID, "Should have received information on peers")

            guard let peerName = peersByID[peer2ID] else {
                throw NSError(domain: "missing information" as String, code: 0, userInfo: nil)
            }

            XCTAssertEqual(peerName, peer2DeviceName, "peer2 device name (as reported by peer1) should be correct")
        } catch {
            XCTFail("Error thrown: \(error)")
        }

        XCTAssertNoThrow(try clique.removeFriends(inClique: [peer2ID]), "Shouldn't error removing peer2 from trust")
        XCTAssertFalse(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer2ID)),
                       "peer 1 should not trust peer 2")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .excludes, target: peer2ID)),
                      "peer 1 should exclude trust peer 2")

        do {
            let peersByID = try clique.peerDeviceNamesByPeerID()
            XCTAssertNotNil(peersByID, "Should have received information on peers")
            XCTAssertTrue(peersByID.isEmpty, "peer1 should report no trusted peers")
        } catch {
            XCTFail("Error thrown: \(error)")
        }
    }

    func testOTCliqueOctagonAuthoritativeTrustResponse() throws {
        self.startCKAccountStatusMock()

        let absentClique = OTClique(contextData: self.otcliqueContext)
        let absentStatus = absentClique.fetchStatus(nil)
        XCTAssertEqual(absentStatus, CliqueStatus.absent, "clique should return Absent")

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")

            let status = clique.fetchStatus(nil)
            XCTAssertEqual(status, CliqueStatus.in, "clique should return In")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.verifyDatabaseMocks()

        let joiningContext = self.makeInitiatorContext(contextID: "joiner", authKitAdapter: self.mockAuthKit2)
        self.assertJoinViaEscrowRecoveryFromDefaultContextWithReciprocationAndTLKShares(joiningContext: joiningContext)

        XCTAssertNoThrow(try clique.leave(), "Should be no error departing clique")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        let status = clique.fetchStatus(nil)
        XCTAssertEqual(status, CliqueStatus.notIn, "clique should return Not In")

        let newOTCliqueContext = OTConfigurationContext()
        newOTCliqueContext.context = "newCliqueContext"
        newOTCliqueContext.dsid = self.otcliqueContext.dsid
        newOTCliqueContext.altDSID = self.otcliqueContext.altDSID
        newOTCliqueContext.otControl = self.otcliqueContext.otControl
        let newClique: OTClique

        do {
            newClique = try OTClique.newFriends(withContextData: newOTCliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(newClique, "newClique should not be nil")

            OctagonSetPlatformSupportsSOS(true)
            let status = newClique.fetchStatus(nil)
            XCTAssertEqual(status, CliqueStatus.in, "clique should return In")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }
        let newContext = self.manager.context(forContainerName: OTCKContainerName, contextID: newOTCliqueContext.context)
        self.assertEnters(context: newContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testCLIStatus() {
        self.startCKAccountStatusMock()

        // Run status without anything in the account
        self.otControlCLI.status(OTCKContainerName,
                                 context: OTDefaultContext,
                                 json: false)

        do {
            _ = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        } catch {
            XCTFail("failed to make new friends: \(error)")
        }

        // run status again with only a self peer
        self.otControlCLI.status(OTCKContainerName,
                                 context: OTDefaultContext,
                                 json: false)
    }

    func testFailingStateTransitionWatcher() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        // Set up a watcher that we expect to fail...
        let path = OctagonStateTransitionPath(from: [
            OctagonStateResetAndEstablish: [
                OctagonStateBottleJoinVouchWithBottle: [
                        OctagonStateResetAndEstablish: OctagonStateTransitionPathStep.success(),
                    ],
                ],
            ])
        let watcher = OctagonStateTransitionWatcher(named: "should-fail",
                                                    serialQueue: self.cuttlefishContext.queue,
                                                    path: try XCTUnwrap(path),
                                                    initialRequest: nil)
        self.cuttlefishContext.stateMachine.register(watcher)

        let watcherCompleteOperationExpectation = self.expectation(description: "watcherCompleteOperationExpectation returns")
        let watcherFinishOp = CKKSResultOperation.named("should-fail-cleanup") {
            XCTAssertNotNil(watcher.result.error, "watcher should have errored")
            watcherCompleteOperationExpectation.fulfill()
        }

        watcherFinishOp.addDependency(watcher.result)
        self.operationQueue.addOperation(watcherFinishOp)

        // and now cause the watcher to fail...
        let resetAndEstablishExpectation = self.expectation(description: "resetAndEstablishExpectation returns")
        self.cuttlefishContext.rpcResetAndEstablish(.testGenerated) { resetError in
            XCTAssertNil(resetError, "should be no error resetting and establishing")
            resetAndEstablishExpectation.fulfill()
        }
        self.wait(for: [resetAndEstablishExpectation], timeout: 10)

        // and the watcher should finish, too
        self.wait(for: [watcherCompleteOperationExpectation], timeout: 10)

        self.verifyDatabaseMocks()
    }

    func testTimingOutStateTransitionWatcher() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let stateTransitionOp = OctagonStateTransitionOperation(name: "will-never-run",
                                                                entering: OctagonStateReady)
        let requestNever = OctagonStateTransitionRequest("name",
                                                         sourceStates: Set([OctagonStateWaitForHSA2]),
                                                         serialQueue: self.cuttlefishContext.queue,
                                                         timeout: 1 * NSEC_PER_SEC,
                                                         transitionOp: stateTransitionOp)

        // Set up a watcher that we expect to fail due to its initial transition op timing out...
        let path = OctagonStateTransitionPath(from: [
            OctagonStateResetAndEstablish: [
                OctagonStateBottleJoinVouchWithBottle: [
                    OctagonStateResetAndEstablish: OctagonStateTransitionPathStep.success(),
                ],
            ],
        ])
        let watcher = OctagonStateTransitionWatcher(named: "should-fail",
                                                    serialQueue: self.cuttlefishContext.queue,
                                                    path: try XCTUnwrap(path),
                                                    initialRequest: (requestNever as! OctagonStateTransitionRequest<CKKSResultOperation & OctagonStateTransitionOperationProtocol>))
        self.cuttlefishContext.stateMachine.register(watcher)

        let watcherCompleteOperationExpectation = self.expectation(description: "watcherCompleteOperationExpectation returns")
        let watcherFinishOp = CKKSResultOperation.named("should-fail-cleanup") {
            XCTAssertNotNil(watcher.result.error, "watcher should have errored")
            watcherCompleteOperationExpectation.fulfill()
        }

        watcherFinishOp.addDependency(watcher.result)
        self.operationQueue.addOperation(watcherFinishOp)

        // and the watcher should finish, too
        self.wait(for: [watcherCompleteOperationExpectation], timeout: 10)

        self.verifyDatabaseMocks()
    }

    func testLeaveCliqueAndPostCFU() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.verifyDatabaseMocks()

        let joiningContext = self.makeInitiatorContext(contextID: "joiner", authKitAdapter: self.mockAuthKit2)
        self.assertJoinViaEscrowRecoveryFromDefaultContextWithReciprocationAndTLKShares(joiningContext: joiningContext)

        XCTAssertNoThrow(try clique.leave(), "Should be no error departing clique")

        // securityd should now consider itself untrusted
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        // As a bonus, calling leave again should be fast (and not error)
        XCTAssertNoThrow(try clique.leave(), "Should be no error departing clique (again)")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let cfuExpectation = self.expectation(description: "cfu callback occurs")
        self.cuttlefishContext.followupHandler.clearAllPostedFlags()
        self.cuttlefishContext.checkTrustStatusAndPostRepairCFUIfNecessary { _, posted, _, _, error in
            #if !os(tvOS)
            XCTAssertTrue(posted, "posted should be true")
            #else
            XCTAssertFalse(posted, "posted should be false on tvOS; there aren't any iphones around to repair it")
            #endif
            XCTAssertNil(error, "error should be nil")
            cfuExpectation.fulfill()
        }
        self.wait(for: [cfuExpectation], timeout: 10)
    }

    func testLostIdentityAndPostCFU() throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)
        self.verifyDatabaseMocks()

        var cfuExpectation = self.expectation(description: "cfu callback occurs")
        self.cuttlefishContext.followupHandler.clearAllPostedFlags()
        self.cuttlefishContext.checkTrustStatusAndPostRepairCFUIfNecessary { _, posted, _, _, error in
            #if !os(tvOS)
            XCTAssertFalse(posted, "posted should be false")
            #else
            XCTAssertFalse(posted, "posted should be false on tvOS; there aren't any iphones around to repair it")
            #endif
            XCTAssertNil(error, "error should be nil")
            cfuExpectation.fulfill()
        }
        self.wait(for: [cfuExpectation], timeout: 10)

        // now remove the identity's key pair from the keychain

        let peerID = try XCTUnwrap(clique).cliqueMemberIdentifier
        let result = try self.removeEgoKeysTest(peerID: peerID!)
        XCTAssertTrue(result, "result should be true")

        cfuExpectation = self.expectation(description: "cfu callback occurs")
        self.cuttlefishContext.followupHandler.clearAllPostedFlags()
        self.cuttlefishContext.checkTrustStatusAndPostRepairCFUIfNecessary { _, posted, _, _, error in
            #if !os(tvOS)
            XCTAssertTrue(posted, "posted should be true")
            #else
            XCTAssertFalse(posted, "posted should be false on tvOS; there aren't any iphones around to repair it")
            #endif
            XCTAssertNil(error, "error should be nil")
            cfuExpectation.fulfill()
        }
        self.wait(for: [cfuExpectation], timeout: 10)
    }

    func testDeviceLockedDuringAccountRetrieval() throws {
        // Tell SOS that it is absent, so we don't enable CDP on bringup
        self.mockSOSAdapter.circleStatus = SOSCCStatus(kSOSCCCircleAbsent)

        self.startCKAccountStatusMock()

        let initiatorContext = self.manager.context(forContainerName: OTCKContainerName,
                                                    contextID: OTDefaultContext,
                                                    sosAdapter: self.mockSOSAdapter,
                                                    authKitAdapter: self.mockAuthKit2,
                                                    tooManyPeersAdapter: self.mockTooManyPeers,
                                                    lockStateTracker: self.lockStateTracker,
                                                    accountStateTracker: self.accountStateTracker,
                                                    deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-SOS-iphone", serialNumber: "456", osVersion: "iOS (fake version)"))

        // Pre-create some Account metadata, so that Octagon will experience a keychain locked error when loading
        try initiatorContext.accountMetadataStore.persistAccountChanges { metadata in
            return metadata
        }

        // Lock all AKS classes
        self.aksLockState = true
        SecMockAKS.lockClassA_C()
        self.lockStateTracker.recheck()

        initiatorContext.startOctagonStateMachine()
        self.assertEnters(context: initiatorContext, state: OctagonStateWaitForClassCUnlock, within: 10 * NSEC_PER_SEC)

        self.aksLockState = false
        self.lockStateTracker.recheck()

        self.assertEnters(context: initiatorContext, state: OctagonStateWaitForCDP, within: 10 * NSEC_PER_SEC)

        XCTAssertNoThrow(try initiatorContext.setCDPEnabled())
        self.assertEnters(context: initiatorContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
    }

    func testCuttlefishUpdateRemovesEgoPeer() throws {
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

        // and all subCKKSes should enter ready...
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // Now, Cuttlefish decides we no longer exist?

        self.fakeCuttlefishServer.deleteAllPeers()
        self.sendContainerChange(context: self.cuttlefishContext)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
    }

    func testFetchViewList() throws {
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

        #if !os(tvOS)
        let expectedViews = Set([
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
            "MFi",
            "Passwords",
            "ProtectedCloudStorage",
            "SecureObjectSync",
            "WiFi",
        ])
        #else
        let expectedViews = Set(["LimitedPeersAllowed",
                                 "Home",
                                 "ProtectedCloudStorage",
                                 "WiFi", ])
        #endif

        let getViewsExpectation = self.expectation(description: "getViews callback happens")
        self.tphClient.fetchCurrentPolicy(withContainer: OTCKContainerName, context: OTDefaultContext, modelIDOverride: nil, isInheritedAccount: false) { outPolicy, _, error in
            XCTAssertNil(error, "should not have failed")
            XCTAssertEqual(expectedViews, outPolicy!.viewList)
            getViewsExpectation.fulfill()
        }
        self.wait(for: [getViewsExpectation], timeout: 10)
    }

    func testNotifications() throws {
        let contextName = OTDefaultContext
        let containerName = OTCKContainerName

        let untrustedNotification = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: "com.apple.security.octagon.trust-status-change"))

        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.wait(for: [untrustedNotification], timeout: 2)

        let trustedNotification = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: "com.apple.security.octagon.trust-status-change"))

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
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        self.wait(for: [trustedNotification], timeout: 2)
    }

    func testDeviceNameNotifications() throws {
        self.startCKAccountStatusMock()

        self.assertResetAndBecomeTrustedInDefaultContext()

        let newDeviceName = "new-name-testDeviceNameNotifications"
        self.mockDeviceInfo.mockDeviceName = newDeviceName

        let updateTrustExpectation = self.expectation(description: "updateTrust")
        self.fakeCuttlefishServer.updateListener = { request in
            XCTAssertTrue(request.hasStableInfoAndSig, "updateTrust request should have a stableInfo info")
            let newStableInfo = TPPeerStableInfo(data: request.stableInfoAndSig.peerStableInfo, sig: request.stableInfoAndSig.sig)
            XCTAssertNotNil(newStableInfo, "should be able to make a stableInfo info from protobuf")
            XCTAssertEqual(newStableInfo?.deviceName, newDeviceName, "name should be updated")
            updateTrustExpectation.fulfill()

            return nil
        }

        self.cuttlefishContext.deviceNameUpdated()
        self.wait(for: [updateTrustExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        let statusExpectation = self.expectation(description: "status callback occurs")
        self.tphClient.dumpEgoPeer(withContainer: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) { _, _, stableInfo, _, error in
            XCTAssertNil(error, "should be no error dumping ego peer")
            XCTAssertEqual(stableInfo?.deviceName, newDeviceName, "device name should be updated")
            statusExpectation.fulfill()
        }
        self.wait(for: [statusExpectation], timeout: 2)

        // Receiving a push shouldn't cause another update to be sent
        self.fakeCuttlefishServer.updateListener = { _ in
            XCTFail("Shouldn't have received another updateTrust")
            return nil
        }

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testUpdateDeviceOSVersionOnRestart() throws {
        self.startCKAccountStatusMock()

        self.assertResetAndBecomeTrustedInDefaultContext()

        let differentVersion = "iOS (different version)"
        self.mockDeviceInfo.mockOsVersion = differentVersion

        let updateTrustExpectation = self.expectation(description: "updateTrust")
        self.fakeCuttlefishServer.updateListener = { request in
            XCTAssertTrue(request.hasStableInfoAndSig, "updateTrust request should have a stableInfo info")
            let newStableInfo = TPPeerStableInfo(data: request.stableInfoAndSig.peerStableInfo, sig: request.stableInfoAndSig.sig)
            XCTAssertNotNil(newStableInfo, "should be able to make a stableInfo info from protobuf")
            XCTAssertEqual(newStableInfo?.osVersion, differentVersion, "version should be updated")

            updateTrustExpectation.fulfill()
            return nil
        }

        self.aksLockState = true
        self.lockStateTracker.recheck()

        // re-start
        self.manager.removeContext(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateWaitForUnlock, within: 10 * NSEC_PER_SEC)

        self.aksLockState = false
        self.lockStateTracker.recheck()

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.wait(for: [updateTrustExpectation], timeout: 10)

        // Double check that version
        self.assertSelfOSVersion(differentVersion)

        // Second restart should not trigger an update
        self.fakeCuttlefishServer.updateListener = { _ in
            XCTFail("Shouldn't have received another updateTrust")
            return nil
        }

        // re-start
        self.manager.removeContext(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testUpdateDeviceOSVersionOnContainerUpdate() throws {
        self.startCKAccountStatusMock()

        self.assertResetAndBecomeTrustedInDefaultContext()

        let differentVersion = "iOS (different version)"
        self.mockDeviceInfo.mockOsVersion = differentVersion

        let updateTrustExpectation = self.expectation(description: "updateTrust")
        self.fakeCuttlefishServer.updateListener = { request in
            XCTAssertTrue(request.hasStableInfoAndSig, "updateTrust request should have a stableInfo info")
            let newStableInfo = TPPeerStableInfo(data: request.stableInfoAndSig.peerStableInfo, sig: request.stableInfoAndSig.sig)
            XCTAssertNotNil(newStableInfo, "should be able to make a stableInfo info from protobuf")
            XCTAssertEqual(newStableInfo?.osVersion, differentVersion, "version should be updated")

            updateTrustExpectation.fulfill()
            return nil
        }
        self.cuttlefishContext.notifyContainerChange(nil)
        self.wait(for: [updateTrustExpectation], timeout: 10)

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        self.assertSelfOSVersion(differentVersion)

        // Receiving a push shouldn't cause another update to be sent
        self.fakeCuttlefishServer.updateListener = { _ in
            XCTFail("Shouldn't have received another updateTrust")
            return nil
        }

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testAPSRateLimiter() throws {
        let untrustedNotification = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: "com.apple.security.octagon.trust-status-change"))

        self.startCKAccountStatusMock()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.cuttlefishContext.startOctagonStateMachine()

        // Octagon will fetch once to determine its trust state
        let trustStateFetchExpectation = self.expectation(description: "trust state fetch occurs")
        self.fakeCuttlefishServer.fetchChangesListener = { [unowned self] _ in
            self.fakeCuttlefishServer.fetchChangesListener = nil
            trustStateFetchExpectation.fulfill()
            return nil
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.wait(for: [untrustedNotification], timeout: 2)

        self.wait(for: [trustStateFetchExpectation], timeout: 10)

        let fetchExpectation = self.expectation(description: "fetch occurs")
        fetchExpectation.expectedFulfillmentCount = 2
        self.fakeCuttlefishServer.fetchChangesListener = { _ in
            fetchExpectation.fulfill()
            return nil
        }

        self.cuttlefishContext.notifyContainerChange(nil)

        self.cuttlefishContext.notifyContainerChange(nil)

        self.cuttlefishContext.notifyContainerChange(nil)

        self.cuttlefishContext.notifyContainerChange(nil)

        self.cuttlefishContext.notifyContainerChange(nil)

        self.wait(for: [fetchExpectation], timeout: 10)
        XCTAssertEqual(self.fakeCuttlefishServer.fetchChangesCalledCount, 3, "fetchChanges should have been called 3 times")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)
    }

    func testAPSExpectTwoFetchChanges() {
        let untrustedNotification = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: "com.apple.security.octagon.trust-status-change"))

        self.startCKAccountStatusMock()

        // Set the CDP bit before the test begins, so we don't have to fetch to discover CDP status
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())

        self.cuttlefishContext.startOctagonStateMachine()

        // Octagon will fetch once to determine its trust state
        let trustStateFetchExpectation = self.expectation(description: "trust state fetch occurs")
        self.fakeCuttlefishServer.fetchChangesListener = { [unowned self] _ in
            self.fakeCuttlefishServer.fetchChangesListener = nil
            trustStateFetchExpectation.fulfill()
            return nil
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.wait(for: [untrustedNotification], timeout: 2)

        self.wait(for: [trustStateFetchExpectation], timeout: 10)

        let fetchExpectation = self.expectation(description: "fetch occurs")
        fetchExpectation.expectedFulfillmentCount = 2
        self.fakeCuttlefishServer.fetchChangesListener = { _ in
            fetchExpectation.fulfill()
            return nil
        }

        self.cuttlefishContext.notifyContainerChange(nil)
        self.cuttlefishContext.notifyContainerChange(nil)
        self.cuttlefishContext.notifyContainerChange(nil)
        self.cuttlefishContext.notifyContainerChange(nil)

        self.wait(for: [fetchExpectation], timeout: 10)
        XCTAssertEqual(self.fakeCuttlefishServer.fetchChangesCalledCount, 3, "fetchChanges should have been called 3 times")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)
    }

    func testTTRTrusted() throws {
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

        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)

        self.isTTRRatelimited = false
        self.ttrExpectation = self.expectation(description: "trigger TTR")

        self.sendTTRRequest(context: self.cuttlefishContext)

        self.wait(for: [try XCTUnwrap(self.ttrExpectation)], timeout: 10)
    }

    func testPersistRefSchedulerLessThan100Items() throws {

        TestsObjectiveC.addNRandomKeychainItemsWithoutUpgradedPersistentRefs(3)

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "should NOT be upgraded")

        self.startCKAccountStatusMock()

        self.keychainUpgradeController.triggerKeychainItemUpdateRPC { error in
            XCTAssertNil(error, "error should be nil")
        }

        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateUpgradePersistentRef, within: 10 * NSEC_PER_SEC)

        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateNothingToDo, within: 10 * NSEC_PER_SEC)

        XCTAssertTrue(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should be upgraded")
        XCTAssertEqual(TestsObjectiveC.lastRowID(), NSNumber(value: 3), "last rowID should be 3")
        XCTAssertTrue(TestsObjectiveC.expectXNumber(ofItemsUpgraded: 3), "should be 3 upgraded")
        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateNothingToDo, within: 10 * NSEC_PER_SEC)
    }

    func testPersistRefSchedulerMoreThan100Items() throws {
        TestsObjectiveC.addNRandomKeychainItemsWithoutUpgradedPersistentRefs(150)

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "should NOT be upgraded")

        self.startCKAccountStatusMock()

        self.keychainUpgradeController.triggerKeychainItemUpdateRPC { error in
            XCTAssertNil(error, "error should be nil")
        }
        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateUpgradePersistentRef, within: 10 * NSEC_PER_SEC)

        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateNothingToDo, within: 10 * NSEC_PER_SEC)

        XCTAssertTrue(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "should be upgraded")
        XCTAssertNil(self.keychainUpgradeController.persistentReferenceUpgrader.nextFireTime, "nextFireTime should be nil")
        XCTAssertEqual(TestsObjectiveC.lastRowID(), NSNumber(value: 150), "last rowID should be 150")
        XCTAssertTrue(TestsObjectiveC.expectXNumber(ofItemsUpgraded: 150), "should be 150 upgraded")
    }

    func testPersistRefSchedulerLessThan100ItemsErrSecInteractionNotAllowed() throws {
        TestsObjectiveC.addNRandomKeychainItemsWithoutUpgradedPersistentRefs(3)

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "should NOT be upgraded")
        TestsObjectiveC.setError(errSecInteractionNotAllowed)

        self.startCKAccountStatusMock()

        // device is locked
        self.aksLockState = true
        self.lockStateProvider.aksCurrentlyLocked = true
        self.lockStateTracker.recheck()

        self.keychainUpgradeController.triggerKeychainItemUpdateRPC { error in
            XCTAssertNil(error, "error should be nil")
        }

        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateWaitForUnlock, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should NOT be upgraded")
        XCTAssertNil(TestsObjectiveC.lastRowID(), "last rowID should be nil")
        XCTAssertTrue(TestsObjectiveC.expectXNumber(ofItemsUpgraded: 0), "should be 0 upgraded")

        XCTAssertNil(self.keychainUpgradeController.persistentReferenceUpgrader.nextFireTime, "nextFireTime should be nil")

        // now the device is unlocked
        TestsObjectiveC.clearError()
        self.aksLockState = false
        self.lockStateProvider.aksCurrentlyLocked = false
        self.lockStateTracker.recheck()

        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateUpgradePersistentRef, within: 10 * NSEC_PER_SEC)

        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateNothingToDo, within: 10 * NSEC_PER_SEC)

        XCTAssertTrue(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should be upgraded")
        XCTAssertEqual(TestsObjectiveC.lastRowID(), NSNumber(value: 3), "last rowID should be 3")
        XCTAssertTrue(TestsObjectiveC.expectXNumber(ofItemsUpgraded: 3), "should be 3 upgraded")
        XCTAssertNil(self.keychainUpgradeController.persistentReferenceUpgrader.nextFireTime, "nextFireTime should be nil")
    }

    func testPersistRefSchedulerMoreThan100ItemsErrSecInteractionNotAllowed() throws {
        TestsObjectiveC.addNRandomKeychainItemsWithoutUpgradedPersistentRefs(200)

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "should NOT be upgraded")
        TestsObjectiveC.setError(errSecInteractionNotAllowed)

        self.startCKAccountStatusMock()

        // device is locked
        self.aksLockState = true
        self.lockStateTracker.recheck()
        self.lockStateProvider.aksCurrentlyLocked = true

        self.keychainUpgradeController.triggerKeychainItemUpdateRPC { error in
            XCTAssertNil(error, "error should be nil")
        }

        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateWaitForUnlock, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should NOT be upgraded")

        XCTAssertNil(self.keychainUpgradeController.persistentReferenceUpgrader.nextFireTime, "nextFireTime should be nil")
        XCTAssertNil(TestsObjectiveC.lastRowID(), "last rowID should be nil")
        XCTAssertTrue(TestsObjectiveC.expectXNumber(ofItemsUpgraded: 0), "should be 0 upgraded")

        // now the device is unlocked
        TestsObjectiveC.clearError()
        self.aksLockState = false
        self.lockStateProvider.aksCurrentlyLocked = false
        self.lockStateTracker.recheck()

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should NOT be upgraded")

        XCTAssertNil(self.keychainUpgradeController.persistentReferenceUpgrader.nextFireTime, "nextFireTime should be nil")

        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateUpgradePersistentRef, within: 10 * NSEC_PER_SEC)

        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateNothingToDo, within: 10 * NSEC_PER_SEC)

        XCTAssertTrue(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should be upgraded")
        XCTAssertEqual(TestsObjectiveC.lastRowID(), NSNumber(value: 200), "last rowID should be 200")
        XCTAssertTrue(TestsObjectiveC.expectXNumber(ofItemsUpgraded: 200), "should be 200 upgraded")

        XCTAssertNil(self.keychainUpgradeController.persistentReferenceUpgrader.nextFireTime, "nextFireTime should be nil")
    }

    func testPersistRefSchedulerMoreThan100ItemsRandomErrSecInteractionNotAllowed() throws {
        TestsObjectiveC.addNRandomKeychainItemsWithoutUpgradedPersistentRefs(200)

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "should NOT be upgraded")
        TestsObjectiveC.setErrorAtRowID(errSecInteractionNotAllowed)

        self.startCKAccountStatusMock()

        // device is locked
        self.aksLockState = true
        self.lockStateProvider.aksCurrentlyLocked = true
        self.lockStateTracker.recheck()

        self.keychainUpgradeController.triggerKeychainItemUpdateRPC { error in
            XCTAssertNil(error, "error should be nil")
        }
        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateWaitForUnlock, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should NOT be upgraded")
        XCTAssertNil(self.keychainUpgradeController.persistentReferenceUpgrader.nextFireTime, "nextFireTime should be nil")
        XCTAssertNil(TestsObjectiveC.lastRowID(), "last rowID should be nil")
        XCTAssertTrue(TestsObjectiveC.expectXNumber(ofItemsUpgraded: 0), "should be 0 upgraded")

        // now the device is unlocked
        TestsObjectiveC.clearError()
        TestsObjectiveC.clearErrorInsertionDictionary()

        self.aksLockState = false
        self.lockStateProvider.aksCurrentlyLocked = false
        self.lockStateTracker.recheck()

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should NOT be upgraded")

        XCTAssertNil(self.keychainUpgradeController.persistentReferenceUpgrader.nextFireTime, "nextFireTime should be nil")

        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateUpgradePersistentRef, within: 10 * NSEC_PER_SEC)

        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateNothingToDo, within: 10 * NSEC_PER_SEC)

        XCTAssertTrue(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should be upgraded")
        XCTAssertEqual(TestsObjectiveC.lastRowID(), NSNumber(value: 200), "last rowID should be 200")
        XCTAssertTrue(TestsObjectiveC.expectXNumber(ofItemsUpgraded: 200), "should be 200 upgraded")
        XCTAssertNil(self.keychainUpgradeController.persistentReferenceUpgrader.nextFireTime, "nextFireTime should be nil")
    }

    // shared tests for decode, auth needed, not available
    func sharedTestsForLessThan100Items(errorCode: Int32) throws {
        TestsObjectiveC.addNRandomKeychainItemsWithoutUpgradedPersistentRefs(3)

        TestsObjectiveC.setError(errorCode)

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "should NOT be upgraded")

        self.startCKAccountStatusMock()

        // start the state machine
        self.keychainUpgradeController.triggerKeychainItemUpdateRPC { error in
            XCTAssertNil(error, "error should be nil")
        }
        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateNothingToDo, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should NOT be upgraded")
        XCTAssertNil(self.keychainUpgradeController.persistentReferenceUpgrader.nextFireTime, "nextFireTime should be nil")
        XCTAssertEqual(TestsObjectiveC.lastRowID(), NSNumber(value: 3), "last rowID should be 3")
        XCTAssertTrue(TestsObjectiveC.expectXNumber(ofItemsUpgraded: 0), "should be 0 upgraded")

        // begin steps to simulate a secd restart where errors are empty, last rowid is empty
        // clear the error and rowID
        TestsObjectiveC.clearError()
        TestsObjectiveC.clearLastRowID()

        // Now restart the context
        self.manager.removeContext(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should NOT be upgraded")

        // start the state machine
        self.keychainUpgradeController.triggerKeychainItemUpdateRPC { error in
            XCTAssertNil(error, "error should be nil")
        }

        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateUpgradePersistentRef, within: 10 * NSEC_PER_SEC)

        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateNothingToDo, within: 10 * NSEC_PER_SEC)

        // now all items are upgraded
        XCTAssertTrue(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should be upgraded")
        XCTAssertEqual(TestsObjectiveC.lastRowID(), NSNumber(value: 3), "last rowID should be 3")
        XCTAssertTrue(TestsObjectiveC.expectXNumber(ofItemsUpgraded: 3), "should be 3 upgraded")
        XCTAssertNil(self.keychainUpgradeController.persistentReferenceUpgrader.nextFireTime, "nextFireTime should be nil")

        // Now restart the context again, check last rowID should be the same
        self.manager.removeContext(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        self.keychainUpgradeController.triggerKeychainItemUpdateRPC { error in
            XCTAssertNil(error, "error should be nil")
        }

        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateNothingToDo, within: 10 * NSEC_PER_SEC)

        XCTAssertNil(self.keychainUpgradeController.persistentReferenceUpgrader.nextFireTime, "nextFireTime should be nil")
        XCTAssertTrue(TestsObjectiveC.expectXNumber(ofItemsUpgraded: 3), "should be 3 upgraded")
        XCTAssertTrue(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should be upgraded")
        XCTAssertEqual(TestsObjectiveC.lastRowID(), NSNumber(value: 3), "last rowID should still be 3")
    }

    func sharedTestsForLessThan100ItemsTimeoutAndNotReady(errorCode: Int32) throws {
        SecKeychainSetOverrideStaticPersistentRefsIsEnabled(false)

        TestsObjectiveC.addNRandomKeychainItemsWithoutUpgradedPersistentRefs(3)

        TestsObjectiveC.setError(errorCode)

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "should NOT be upgraded")

        self.startCKAccountStatusMock()

        let refCondition = self.keychainUpgradeController.persistentReferenceUpgrader

        let callbackExpectation = self.expectation(description: "callback occurs")

        let callback = CKKSResultOperation.named("callback") {
            callbackExpectation.fulfill()
        }
        callback.timeout(10 * NSEC_PER_SEC)

        callback.addDependency(refCondition.operationDependency)
        self.operationQueue.addOperation(callback)

        self.keychainUpgradeController.triggerKeychainItemUpdateRPC { error in
            XCTAssertNil(error, "error should be nil")
        }
        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateUpgradePersistentRef, within: 10 * NSEC_PER_SEC)
        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateWaitForTrigger, within: 10 * NSEC_PER_SEC)
        XCTAssertNotNil(self.keychainUpgradeController.persistentReferenceUpgrader.nextFireTime, "nextFireTime should NOT be nil")

        self.wait(for: [callbackExpectation], timeout: 10)

        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateUpgradePersistentRef, within: 10 * NSEC_PER_SEC)
        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateWaitForTrigger, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should NOT be upgraded")
        XCTAssertNil(TestsObjectiveC.lastRowID(), "last rowID should be nil")
        XCTAssertTrue(TestsObjectiveC.expectXNumber(ofItemsUpgraded: 0), "should be 0 upgraded")

        // begin steps to simulate a secd restart where errors are empty, last rowid is empty
        // clear the error and rowID
        TestsObjectiveC.clearError()
        TestsObjectiveC.clearLastRowID()

        // Now restart the context
        self.manager.removeContext(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should NOT be upgraded")

        let refConditionAfterRestart = self.keychainUpgradeController.persistentReferenceUpgrader
        let afterRestartCallbackExpectation = self.expectation(description: "after restart callback occurs")
        let afterRestartCallback = CKKSResultOperation.named("after restart callback") {
            afterRestartCallbackExpectation.fulfill()
        }
        afterRestartCallback.timeout(10 * NSEC_PER_SEC)

        afterRestartCallback.addDependency(refConditionAfterRestart.operationDependency)
        self.operationQueue.addOperation(afterRestartCallback)
        // start the state machine
        self.keychainUpgradeController.triggerKeychainItemUpdateRPC { error in
            XCTAssertNil(error, "error should be nil")
        }
        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateUpgradePersistentRef, within: 10 * NSEC_PER_SEC)
        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateNothingToDo, within: 10 * NSEC_PER_SEC)

        self.wait(for: [afterRestartCallbackExpectation], timeout: 10)

        XCTAssertTrue(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should be upgraded")
        XCTAssertEqual(TestsObjectiveC.lastRowID(), NSNumber(value: 3), "last rowID should be 3")
        XCTAssertTrue(TestsObjectiveC.expectXNumber(ofItemsUpgraded: 3), "should be 3 upgraded")
        XCTAssertNil(self.keychainUpgradeController.persistentReferenceUpgrader.nextFireTime, "nextFireTime should be nil")
    }

    // for decode, authneeded, not available errors
    func sharedTestsForMoreThan100Items(errorCode: Int32) throws {
        TestsObjectiveC.addNRandomKeychainItemsWithoutUpgradedPersistentRefs(200)

        TestsObjectiveC.setError(errorCode)

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "should NOT be upgraded")

        self.startCKAccountStatusMock()

        // start the state machine
        self.keychainUpgradeController.triggerKeychainItemUpdateRPC { error in
            XCTAssertNil(error, "error should be nil")
        }
        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateUpgradePersistentRef, within: 10 * NSEC_PER_SEC)
        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateNothingToDo, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should NOT be upgraded")
        XCTAssertNil(self.keychainUpgradeController.persistentReferenceUpgrader.nextFireTime, "nextFireTime should be nil")
        XCTAssertEqual(TestsObjectiveC.lastRowID(), NSNumber(value: 200), "last rowID should be 200")
        XCTAssertTrue(TestsObjectiveC.expectXNumber(ofItemsUpgraded: 0), "should be 0 upgraded")

        // at this point secd has scanned all items and skipped over all the items

        // begin steps to simulate a secd restart where errors are empty, last rowid is empty
        // clear the error and rowID
        TestsObjectiveC.clearError()
        TestsObjectiveC.clearLastRowID()

        // Now restart the context
        self.manager.removeContext(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should NOT be upgraded")

        // start the state machine
        self.keychainUpgradeController.triggerKeychainItemUpdateRPC { error in
            XCTAssertNil(error, "error should be nil")
        }
        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateUpgradePersistentRef, within: 10 * NSEC_PER_SEC)
        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateNothingToDo, within: 10 * NSEC_PER_SEC)

        XCTAssertTrue(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should be upgraded")
        XCTAssertEqual(TestsObjectiveC.lastRowID(), NSNumber(value: 200), "last rowID should be 200")
        XCTAssertTrue(TestsObjectiveC.expectXNumber(ofItemsUpgraded: 200), "should be 200 upgraded")
        XCTAssertNil(self.keychainUpgradeController.persistentReferenceUpgrader.nextFireTime, "nextFireTime should be nil")

        // Now restart the context again, check last rowID should be the same
        self.manager.removeContext(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        self.keychainUpgradeController.triggerKeychainItemUpdateRPC { error in
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertEqual(TestsObjectiveC.lastRowID(), NSNumber(value: 200), "last rowID should still be 200")
        XCTAssertTrue(TestsObjectiveC.expectXNumber(ofItemsUpgraded: 200), "should be 200 upgraded")
        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateNothingToDo, within: 10 * NSEC_PER_SEC)
        XCTAssertNil(self.keychainUpgradeController.persistentReferenceUpgrader.nextFireTime, "nextFireTime should be nil")
        XCTAssertTrue(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should be upgraded")
    }

    func sharedTestsForMoreThan100ItemsTimeoutAndNotReady(errorCode: Int32) throws {
        TestsObjectiveC.addNRandomKeychainItemsWithoutUpgradedPersistentRefs(200)

        TestsObjectiveC.setError(errorCode)

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "should NOT be upgraded")

        self.startCKAccountStatusMock()

        let refCondition = self.keychainUpgradeController.persistentReferenceUpgrader

        // setup the operation dependency
        var callbackExpectation = self.expectation(description: "callback occurs")
        var callback = CKKSResultOperation.named("callback") {
            callbackExpectation.fulfill()
        }
        callback.timeout(10 * NSEC_PER_SEC)

        callback.addDependency(refCondition.operationDependency)
        self.operationQueue.addOperation(callback)

        // start the state machine
        self.keychainUpgradeController.triggerKeychainItemUpdateRPC { error in
            XCTAssertNil(error, "error should be nil")
        }
        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateUpgradePersistentRef, within: 10 * NSEC_PER_SEC)
        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateWaitForTrigger, within: 10 * NSEC_PER_SEC)

        self.wait(for: [callbackExpectation], timeout: 10)

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should NOT be upgraded")
        XCTAssertNotNil(self.keychainUpgradeController.persistentReferenceUpgrader.nextFireTime, "nextFireTime should NOT be nil")
        XCTAssertNil(TestsObjectiveC.lastRowID(), "should be nil")
        XCTAssertTrue(TestsObjectiveC.expectXNumber(ofItemsUpgraded: 0), "should be 0 upgraded")

        // setup the operation dependency for expected trigger
        callbackExpectation = self.expectation(description: "second batch callback occurs")
        callback = CKKSResultOperation.named("second batch callback") {
            callbackExpectation.fulfill()
        }
        callback.timeout(10 * NSEC_PER_SEC)

        callback.addDependency(refCondition.operationDependency)
        self.operationQueue.addOperation(callback)

        self.wait(for: [callbackExpectation], timeout: 10)

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should NOT be upgraded")
        XCTAssertNotNil(self.keychainUpgradeController.persistentReferenceUpgrader.nextFireTime, "nextFireTime should NOT be nil")
        XCTAssertNil(TestsObjectiveC.lastRowID(), "should be nil")
        XCTAssertTrue(TestsObjectiveC.expectXNumber(ofItemsUpgraded: 0), "should be 0 upgraded")

        // at this point secd has scanned all items and skipped over all the items

        // begin steps to simulate a secd restart where errors are empty, last rowid is empty
        // clear the error and rowID
        TestsObjectiveC.clearError()
        TestsObjectiveC.clearLastRowID()

        // Now restart the context
        self.manager.removeContext(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should NOT be upgraded")

        let refConditionAfterRestart = self.keychainUpgradeController.persistentReferenceUpgrader
        let afterRestartCallbackExpectation = self.expectation(description: "after restart callback occurs")
        let afterRestartCallback = CKKSResultOperation.named("after restart callback") {
            afterRestartCallbackExpectation.fulfill()
        }
        afterRestartCallback.timeout(10 * NSEC_PER_SEC)

        afterRestartCallback.addDependency(refConditionAfterRestart.operationDependency)
        self.operationQueue.addOperation(afterRestartCallback)
        // start the state machine
        self.keychainUpgradeController.triggerKeychainItemUpdateRPC { error in
            XCTAssertNil(error, "error should be nil")
        }
        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateUpgradePersistentRef, within: 10 * NSEC_PER_SEC)
        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateNothingToDo, within: 10 * NSEC_PER_SEC)

        self.wait(for: [afterRestartCallbackExpectation], timeout: 10)

        XCTAssertTrue(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should be upgraded")
        XCTAssertEqual(TestsObjectiveC.lastRowID(), NSNumber(value: 200), "last rowID should be 200")
        XCTAssertTrue(TestsObjectiveC.expectXNumber(ofItemsUpgraded: 200), "should be 200 upgraded")
        XCTAssertNil(self.keychainUpgradeController.persistentReferenceUpgrader.nextFireTime, "nextFireTime should be nil")
    }

    // for decode, authneeded, not available errors
    func sharedTestsForMoreThan100ItemsRandomInsertion(errorCode: Int32) throws {
        TestsObjectiveC.addNRandomKeychainItemsWithoutUpgradedPersistentRefs(200)

        TestsObjectiveC.setErrorAtRowID(errorCode)

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "should NOT be upgraded")

        self.startCKAccountStatusMock()

        // start the state machine
        self.keychainUpgradeController.triggerKeychainItemUpdateRPC { error in
            XCTAssertNil(error, "error should be nil")
        }
        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateNothingToDo, within: 10 * NSEC_PER_SEC)

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should NOT be upgraded")
        XCTAssertNil(self.keychainUpgradeController.persistentReferenceUpgrader.nextFireTime, "nextFireTime should be nil")
        XCTAssertEqual(TestsObjectiveC.lastRowID(), NSNumber(value: 200), "last rowID should be 200")
        XCTAssertTrue(TestsObjectiveC.expectXNumber(ofItemsUpgraded: 199), "should be 199 upgraded") // all but 150

        // at this point secd has scanned all items and skipped over all the items

        // begin steps to simulate a secd restart where errors are empty, last rowid is empty
        // clear the error and rowID
        TestsObjectiveC.clearError()
        TestsObjectiveC.clearLastRowID()
        TestsObjectiveC.clearErrorInsertionDictionary()

        // Now restart the context
        self.manager.removeContext(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should NOT be upgraded")

        // start the state machine
        self.keychainUpgradeController.triggerKeychainItemUpdateRPC { error in
            XCTAssertNil(error, "error should be nil")
        }

        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateUpgradePersistentRef, within: 10 * NSEC_PER_SEC)
        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateNothingToDo, within: 10 * NSEC_PER_SEC)

        // only rowID 150 wasn't upgraded before and it should be the last looked at rowID
        XCTAssertTrue(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should be upgraded")
        XCTAssertEqual(TestsObjectiveC.lastRowID(), NSNumber(value: 150), "last rowID should be 150")
        XCTAssertTrue(TestsObjectiveC.expectXNumber(ofItemsUpgraded: 200), "should be 200 upgraded")
        XCTAssertNil(self.keychainUpgradeController.persistentReferenceUpgrader.nextFireTime, "nextFireTime should be nil")

        // Now restart the context again, check last rowID should be the same
        self.manager.removeContext(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        self.keychainUpgradeController.triggerKeychainItemUpdateRPC { error in
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertEqual(TestsObjectiveC.lastRowID(), NSNumber(value: 150), "last rowID should still be 150")
        XCTAssertTrue(TestsObjectiveC.expectXNumber(ofItemsUpgraded: 200), "should be 200 upgraded")

        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateNothingToDo, within: 10 * NSEC_PER_SEC)

        XCTAssertNil(self.keychainUpgradeController.persistentReferenceUpgrader.nextFireTime, "nextFireTime should be nil")
        XCTAssertEqual(TestsObjectiveC.lastRowID(), NSNumber(value: 150), "last rowID should still be 150")
        XCTAssertTrue(TestsObjectiveC.expectXNumber(ofItemsUpgraded: 200), "should be 200 upgraded")
        XCTAssertTrue(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should be upgraded")
    }

    func sharedTestsForMoreThan100ItemsRandomInsertionNotReadyAndTimeout(errorCode: Int32) throws {
        TestsObjectiveC.addNRandomKeychainItemsWithoutUpgradedPersistentRefs(200)

        TestsObjectiveC.setErrorAtRowID(errorCode)

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "should NOT be upgraded")

        self.startCKAccountStatusMock()

        let refCondition = self.keychainUpgradeController.persistentReferenceUpgrader

        // setup the operation dependency
        let callbackExpectation = self.expectation(description: "callback occurs")
        let callback = CKKSResultOperation.named("callback") {
            callbackExpectation.fulfill()
        }
        callback.timeout(10 * NSEC_PER_SEC)

        callback.addDependency(refCondition.operationDependency)
        self.operationQueue.addOperation(callback)

        // start the state machine
        self.keychainUpgradeController.triggerKeychainItemUpdateRPC { error in
            XCTAssertNil(error, "error should be nil")
        }
        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateWaitForTrigger, within: 10 * NSEC_PER_SEC)

        self.wait(for: [callbackExpectation], timeout: 10)

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should NOT be upgraded")
        XCTAssertNotNil(self.keychainUpgradeController.persistentReferenceUpgrader.nextFireTime, "nextFireTime should NOT be nil")
        XCTAssertEqual(TestsObjectiveC.lastRowID(), NSNumber(value: 149), "last rowID should be 149")
        XCTAssertTrue(TestsObjectiveC.expectXNumber(ofItemsUpgraded: 149), "should be 149 upgraded")

        // at this point secd has scanned all items and skipped over all the items

        // begin steps to simulate a secd restart where errors are empty, last rowid is empty
        // clear the error and rowID
        TestsObjectiveC.clearError()
        TestsObjectiveC.clearLastRowID()
        TestsObjectiveC.clearErrorInsertionDictionary()

        // Now restart the context
        self.manager.removeContext(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        XCTAssertFalse(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should NOT be upgraded")

        // setup the second attempt upgrading items operation dependency
        let refConditionAfterRestart = self.keychainUpgradeController.persistentReferenceUpgrader
        let afterRestartCallbackExpectation = self.expectation(description: "after restart callback occurs")
        let afterRestartCallback = CKKSResultOperation.named("after restart callback") {
            afterRestartCallbackExpectation.fulfill()
        }
        afterRestartCallback.timeout(10 * NSEC_PER_SEC)

        afterRestartCallback.addDependency(refConditionAfterRestart.operationDependency)
        self.operationQueue.addOperation(afterRestartCallback)
        // start the state machine
        self.keychainUpgradeController.triggerKeychainItemUpdateRPC { error in
            XCTAssertNil(error, "error should be nil")
        }
        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateUpgradePersistentRef, within: 10 * NSEC_PER_SEC)
        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateNothingToDo, within: 10 * NSEC_PER_SEC)

        self.wait(for: [afterRestartCallbackExpectation], timeout: 10)

        // only rowID 150 wasn't upgraded before and it should be the last looked at rowID
        XCTAssertTrue(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should be upgraded")
        XCTAssertEqual(TestsObjectiveC.lastRowID(), NSNumber(value: 200), "last rowID should be 200")
        XCTAssertTrue(TestsObjectiveC.expectXNumber(ofItemsUpgraded: 200), "should be 200 upgraded")
        XCTAssertNil(self.keychainUpgradeController.persistentReferenceUpgrader.nextFireTime, "nextFireTime should be nil")

        // Now restart the context again, check last rowID should be the same
        self.manager.removeContext(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        self.keychainUpgradeController.triggerKeychainItemUpdateRPC { error in
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertEqual(TestsObjectiveC.lastRowID(), NSNumber(value: 200), "last rowID should still be 200")
        XCTAssertTrue(TestsObjectiveC.expectXNumber(ofItemsUpgraded: 200), "should be 200 upgraded")

        self.assertEnters(stateMachine: self.keychainUpgradeController.stateMachine, state: KeychainItemUpgradeRequestStateNothingToDo, within: 10 * NSEC_PER_SEC)

        XCTAssertNil(self.keychainUpgradeController.persistentReferenceUpgrader.nextFireTime, "nextFireTime should be nil")
        XCTAssertEqual(TestsObjectiveC.lastRowID(), NSNumber(value: 200), "last rowID should still be 200")
        XCTAssertTrue(TestsObjectiveC.expectXNumber(ofItemsUpgraded: 200), "should be 200 upgraded")
        XCTAssertTrue(TestsObjectiveC.checkAllPersistentRefBeenUpgraded(), "all items should be upgraded")
    }

    /* all same behavior tests for kAKSReturnNotReady, kAKSReturnTimeout */

    func testPersistRefSchedulerLessThan100ItemskAKSReturnNotReady() throws {
        try self.sharedTestsForLessThan100ItemsTimeoutAndNotReady(errorCode: Int32(kAKSReturnNotReady))
    }

    func testPersistRefSchedulerMoreThan100ItemskAKSReturnNotReady() throws {
        try self.sharedTestsForMoreThan100ItemsTimeoutAndNotReady(errorCode: Int32(kAKSReturnNotReady))
    }

    func testPersistRefSchedulerMoreThan100ItemskAKSReturnNotReadyRandomInsertion() throws {
        try self.sharedTestsForMoreThan100ItemsRandomInsertionNotReadyAndTimeout(errorCode: Int32(kAKSReturnNotReady))
    }

    func testPersistRefSchedulerLessThan100ItemskAKSReturnTimeout() throws {
        try self.sharedTestsForLessThan100ItemsTimeoutAndNotReady(errorCode: Int32(kAKSReturnTimeout))
    }

    func testPersistRefSchedulerMoreThan100ItemskAKSReturnTimeout() throws {
        try self.sharedTestsForMoreThan100ItemsTimeoutAndNotReady(errorCode: Int32(kAKSReturnTimeout))
    }

    func testPersistRefSchedulerMoreThan100ItemskAKSReturnTimeoutRandomInsertion() throws {
        try self.sharedTestsForMoreThan100ItemsRandomInsertionNotReadyAndTimeout(errorCode: Int32(kAKSReturnTimeout))
    }

    /* all same behavior tests for errSecDecode, errSecAuthNeeded, and errSecNotAvailable*/
    func testPersistRefSchedulerLessThan100ItemsErrSecDecode() throws {
        try self.sharedTestsForLessThan100Items(errorCode: errSecDecode)
    }

    func testPersistRefSchedulerMoreThan100ItemsErrSecDecode() throws {
        try self.sharedTestsForMoreThan100Items(errorCode: errSecDecode)
    }

    func testPersistRefSchedulerMoreThan100ItemsErrSecDecodeRandomInsertion() throws {
        try self.sharedTestsForMoreThan100ItemsRandomInsertion(errorCode: Int32(errSecDecode))
    }

    func testPersistRefSchedulerLessThan100ItemsErrSecAuthNeeded() throws {
        try self.sharedTestsForLessThan100Items(errorCode: Int32(errSecAuthNeeded))
    }

    func testPersistRefSchedulerMoreThan100ItemsErrSecAuthNeeded() throws {
        try self.sharedTestsForMoreThan100Items(errorCode: Int32(errSecAuthNeeded))
    }

    func testPersistRefSchedulerMoreThan100ItemsErrSecAuthNeededRandomInsertion() throws {
        try self.sharedTestsForMoreThan100ItemsRandomInsertion(errorCode: Int32(errSecAuthNeeded))
    }

    func testPersistRefSchedulerLessThan100ItemsErrSecNotAvailable() throws {
        try self.sharedTestsForLessThan100Items(errorCode: errSecNotAvailable)
    }

    func testPersistRefSchedulerMoreThan100ItemsErrSecNotAvailable() throws {
        try self.sharedTestsForMoreThan100Items(errorCode: errSecNotAvailable)
    }

    func testPersistRefSchedulerMoreThan100ItemsErrSecNotAvailableRandomInsertion() throws {
        try self.sharedTestsForMoreThan100ItemsRandomInsertion(errorCode: errSecNotAvailable)
    }
}

class OctagonTestsOverrideModelBase: OctagonTestsBase {
    struct TestCase {
        let model: String
        let success: Bool
        let manateeTLKs: Bool
        let limitedTLKs: Bool
    }

    func assertTLKs(expectation: TestCase, receiverPeerID: String, senderPeerID: String, file: StaticString = #file, line: UInt = #line) {
        let haveManateeTLK = self.tlkShareInCloudKit(receiverPeerID: receiverPeerID,
                                                     senderPeerID: senderPeerID,
                                                     zoneID: self.manateeZoneID)
        let haveLimitedPeersAllowedTLK = self.tlkShareInCloudKit(receiverPeerID: receiverPeerID,
                                                                 senderPeerID: senderPeerID,
                                                                 zoneID: self.limitedPeersAllowedZoneID)

        XCTAssertEqual(haveManateeTLK, expectation.manateeTLKs, "manatee should be what's expected: \(expectation)", file: file, line: line)
        XCTAssertEqual(haveLimitedPeersAllowedTLK, expectation.limitedTLKs, "limited should be what's expected: \(expectation)", file: file, line: line)
    }

    func _testVouchers(expectations: [TestCase]) throws {
        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        let ckksKeys: [CKKSKeychainBackedKeySet] = self.intendedCKKSZones.compactMap { zoneID in
            let currentKeySet = CKKSCurrentKeySet.load(forZone: zoneID)
            return try! currentKeySet.asKeychainBackedSet()
        }

        let senderPeerID = fetchEgoPeerID()

        for testCase in expectations {
            let model = testCase.model
            let expectedSuccess = testCase.success
            // Now, fake up a voucher for the second peer using TPH
            let peer2ContextID = "asdf"
            _ = self.manager.context(forContainerName: OTCKContainerName,
                                     contextID: peer2ContextID,
                                     sosAdapter: self.mockSOSAdapter,
                                     authKitAdapter: self.mockAuthKit2,
                                     tooManyPeersAdapter: self.mockTooManyPeers,
                                     lockStateTracker: self.lockStateTracker,
                                     accountStateTracker: self.accountStateTracker,
                                     deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-SOS-iphone", serialNumber: "456", osVersion: "iOS (fake version)"))

            self.setAllowListToCurrentAuthKit(container: OTCKContainerName, context: peer2ContextID, accountIsDemo: false)

            let peer2DeviceName = "peer2-asdf"
            let joinExpectation = self.expectation(description: "join callback occurs")
            self.tphClient.prepare(withContainer: OTCKContainerName,
                                   context: peer2ContextID,
                                   epoch: 1,
                                   machineID: self.mockAuthKit.currentMachineID,
                                   bottleSalt: "123456789",
                                   bottleID: UUID().uuidString,
                                   modelID: model,
                                   deviceName: peer2DeviceName,
                                   serialNumber: "1234",
                                   osVersion: "something",
                                   policyVersion: nil,
                                   policySecrets: nil,
                                   syncUserControllableViews: .UNKNOWN,
                                   secureElementIdentity: nil,
                                   setting: nil,
                                   signingPrivKeyPersistentRef: nil,
                                   encPrivKeyPersistentRef: nil) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, _, error in
                                    XCTAssertNil(error, "Should be no error preparing identity")
                                    XCTAssertNotNil(permanentInfo, "Should have a permanent identity")
                                    XCTAssertNotNil(permanentInfoSig, "Should have a permanent identity signature")
                                    XCTAssertNotNil(stableInfo, "Should have a stable info")
                                    XCTAssertNotNil(stableInfoSig, "Should have a stable info signature")
                                    if expectedSuccess {
                                        self.tphClient.vouch(withContainer: self.cuttlefishContext.containerName,
                                                             context: self.cuttlefishContext.contextID,
                                                             peerID: peerID!,
                                                             permanentInfo: permanentInfo!,
                                                             permanentInfoSig: permanentInfoSig!,
                                                             stableInfo: stableInfo!,
                                                             stableInfoSig: stableInfoSig!,
                                                             ckksKeys: ckksKeys) { voucher, voucherSig, error in
                                                                XCTAssertNil(error, "Should be no error vouching")
                                                                XCTAssertNotNil(voucher, "Should have a voucher")
                                                                XCTAssertNotNil(voucherSig, "Should have a voucher signature")

                                                                self.assertTLKs(expectation: testCase,
                                                                                receiverPeerID: peerID!,
                                                                                senderPeerID: senderPeerID)

                                                                self.tphClient.join(withContainer: OTCKContainerName,
                                                                                    context: peer2ContextID,
                                                                                    voucherData: voucher!,
                                                                                    voucherSig: voucherSig!,
                                                                                    ckksKeys: [],
                                                                                    tlkShares: [],
                                                                                    preapprovedKeys: []) { peerID, _, _, error in
                                                                                        XCTAssertNil(error, "Should be no error joining")
                                                                                        XCTAssertNotNil(peerID, "Should have a peerID")
                                                                                        joinExpectation.fulfill()
                                                                }
                                        }
                                    } else {
                                        self.tphClient.vouch(withContainer: self.cuttlefishContext.containerName,
                                                             context: self.cuttlefishContext.contextID,
                                                             peerID: peerID!,
                                                             permanentInfo: permanentInfo!,
                                                             permanentInfoSig: permanentInfoSig!,
                                                             stableInfo: stableInfo!,
                                                             stableInfoSig: stableInfoSig!,
                                                             ckksKeys: []) { voucher, voucherSig, error in
                                                                XCTAssertNil(voucher, "voucher should be nil")
                                                                XCTAssertNil(voucherSig, "voucherSig should be nil")
                                                                XCTAssertNotNil(error, "error should be non nil")

                                                                self.assertTLKs(expectation: testCase,
                                                                                receiverPeerID: peerID!,
                                                                                senderPeerID: senderPeerID)

                                                                joinExpectation.fulfill()
                                        }
                                    }
            }
            self.wait(for: [joinExpectation], timeout: 10)
        }
    }

    func _testJoin(expectations: [TestCase]) throws {
        self.startCKAccountStatusMock()
        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let clique: OTClique
        do {
            clique = try OTClique.newFriends(withContextData: self.otcliqueContext, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
            throw error
        }

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        let peer1ID = fetchEgoPeerID()
        let peer1Keys: OctagonSelfPeerKeys = try loadEgoKeysSync(peerID: peer1ID)

        for testCase in expectations {
            let model = testCase.model
            let expectedSuccess = testCase.success

            let peer2ContextID = "asdf"
            let peer2DeviceName = "peer2-device-name"
            var peer2ID: String!

            self.setAllowListToCurrentAuthKit(container: OTCKContainerName, context: peer2ContextID, accountIsDemo: false)

            let joinExpectation = self.expectation(description: "join callback occurs")
            self.tphClient.prepare(withContainer: OTCKContainerName,
                                   context: peer2ContextID,
                                   epoch: 1,
                                   machineID: self.mockAuthKit2.currentMachineID,
                                   bottleSalt: "123456789",
                                   bottleID: UUID().uuidString,
                                   modelID: model,
                                   deviceName: peer2DeviceName,
                                   serialNumber: "1234",
                                   osVersion: "something",
                                   policyVersion: nil,
                                   policySecrets: nil,
                                   syncUserControllableViews: .UNKNOWN,
                                   secureElementIdentity: nil,
                                   setting: nil,
                                   signingPrivKeyPersistentRef: nil,
                                   encPrivKeyPersistentRef: nil) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, _, error in
                                    XCTAssertNil(error, "Should be no error preparing identity")
                                    XCTAssertNotNil(permanentInfo, "Should have a permanent identity")
                                    XCTAssertNotNil(permanentInfoSig, "Should have a permanent identity signature")
                                    XCTAssertNotNil(stableInfo, "Should have a stable info")
                                    XCTAssertNotNil(stableInfoSig, "Should have a stable info signature")
                                    peer2ID = peerID

                                    var voucher: TPVoucher?
                                    do {
                                        try voucher = TPVoucher(reason: TPVoucherReason.secureChannel,
                                                                beneficiaryID: peer2ID,
                                                                sponsorID: peer1ID,
                                                                signing: peer1Keys.signingKey)
                                    } catch {
                                        XCTFail("should not throw: \(error)")
                                    }
                                    self.tphClient.join(withContainer: OTCKContainerName,
                                                        context: peer2ContextID,
                                                        voucherData: voucher!.data,
                                                        voucherSig: voucher!.sig,
                                                        ckksKeys: [],
                                                        tlkShares: [],
                                                        preapprovedKeys: []) { peerID, _, _, error in
                                                            if expectedSuccess {
                                                                XCTAssertNil(error, "expected success")
                                                                XCTAssertNotNil(peerID, "peerID should be set")
                                                            } else {
                                                                XCTAssertNotNil(error, "error should not be nil")
                                                                XCTAssertNil(peerID, "peerID should not be set")
                                                            }
                                                            joinExpectation.fulfill()
                                    }
            }
            self.wait(for: [joinExpectation], timeout: 10)

            if expectedSuccess {
                let updateTrustExpectation = self.expectation(description: "updateTrust")
                self.fakeCuttlefishServer.updateListener = { request in
                    XCTAssertEqual(peer1ID, request.peerID, "updateTrust request should be for peer 1")
                    XCTAssertTrue(request.hasDynamicInfoAndSig, "updateTrust request should have a dynamic info")
                    let newDynamicInfo = TPPeerDynamicInfo(data: request.dynamicInfoAndSig.peerDynamicInfo, sig: request.dynamicInfoAndSig.sig)
                    XCTAssertNotNil(newDynamicInfo, "should be able to make a dynamic info from protobuf")

                    XCTAssert(newDynamicInfo!.includedPeerIDs.contains(peer2ID), "Peer1 should trust peer2")

                    updateTrustExpectation.fulfill()
                    return nil
                }

                if testCase.manateeTLKs {
                    self.expectCKModifyKeyRecords(0, currentKeyPointerRecords: 0, tlkShareRecords: 1, zoneID: self.manateeZoneID)
                }
                if testCase.limitedTLKs {
                    self.expectCKModifyKeyRecords(0, currentKeyPointerRecords: 0, tlkShareRecords: 1, zoneID: self.limitedPeersAllowedZoneID)
                }

                self.cuttlefishContext.notifyContainerChange(nil)

                self.wait(for: [updateTrustExpectation], timeout: 100)
                self.fakeCuttlefishServer.updateListener = nil

                self.sendAllCKKSTrustedPeersChanged()
                self.verifyDatabaseMocks()
                self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
            }

            self.assertTLKs(expectation: testCase,
                            receiverPeerID: peer2ID,
                            senderPeerID: peer1ID)
        }
    }
}

class OctagonTestsOverrideModelTV: OctagonTestsOverrideModelBase {
    override func setUp() {
        self.mockDeviceInfo = OTMockDeviceInfoAdapter(modelID: "AppleTV5,3",
                                                      deviceName: "intro-TV",
                                                      serialNumber: "456",
                                                      osVersion: "tvOS (whatever TV version)")

        super.setUp()
    }

    func testVoucherFromTV() throws {
        try self._testVouchers(expectations: [TestCase(model: "AppleTV5,3", success: true, manateeTLKs: false, limitedTLKs: true),
                                              TestCase(model: "MacFoo", success: false, manateeTLKs: false, limitedTLKs: false),
                                              TestCase(model: "Watch17", success: false, manateeTLKs: false, limitedTLKs: false),
                                              TestCase(model: "WinPC0,0", success: false, manateeTLKs: false, limitedTLKs: false),
                                             ])
    }

    func testJoinFromTV() throws {
        try self._testJoin(expectations: [TestCase(model: "AppleTV5,3", success: true, manateeTLKs: false, limitedTLKs: true),
                                          TestCase(model: "MacFoo", success: false, manateeTLKs: false, limitedTLKs: false),
                                          TestCase(model: "Watch17", success: false, manateeTLKs: false, limitedTLKs: false),
                                          TestCase(model: "WinPC0,0", success: false, manateeTLKs: false, limitedTLKs: false),
                                         ])
    }
}

class OctagonTestsOverrideModelWindows: OctagonTestsOverrideModelBase {
    override func setUp() {
        self.mockDeviceInfo = OTMockDeviceInfoAdapter(modelID: "WinPC0,0",
                                                      deviceName: "intro-windows",
                                                      serialNumber: "456",
                                                      osVersion: "Windows (whatever Windows version)")

        super.setUp()
    }

    func testVoucherFromTV() throws {
        try self._testVouchers(expectations: [
            TestCase(model: "WinPC0,0", success: false, manateeTLKs: false, limitedTLKs: false),
            TestCase(model: "AppleTV5,3", success: false, manateeTLKs: false, limitedTLKs: false),
            TestCase(model: "MacFoo", success: false, manateeTLKs: false, limitedTLKs: false),
            TestCase(model: "Watch17", success: false, manateeTLKs: false, limitedTLKs: false),
        ])
    }

    func testJoinFromTV() throws {
        try self._testJoin(expectations: [
            TestCase(model: "WinPC0,0", success: false, manateeTLKs: false, limitedTLKs: false),
            TestCase(model: "AppleTV5,3", success: false, manateeTLKs: false, limitedTLKs: false),
            TestCase(model: "MacFoo", success: false, manateeTLKs: false, limitedTLKs: false),
            TestCase(model: "Watch17", success: false, manateeTLKs: false, limitedTLKs: false),
        ])
    }
}

class OctagonTestsOverrideModelMac: OctagonTestsOverrideModelBase {
    override func setUp() {
        self.mockDeviceInfo = OTMockDeviceInfoAdapter(modelID: "Mac17",
                                                      deviceName: "macbook",
                                                      serialNumber: "456",
                                                      osVersion: "OSX 11")
        super.setUp()
    }

    func testVoucherFromMac() throws {
        try self._testVouchers(expectations: [TestCase(model: "AppleTV5,3", success: true, manateeTLKs: false, limitedTLKs: true),
                                              TestCase(model: "MacFoo", success: true, manateeTLKs: true, limitedTLKs: true),
                                              TestCase(model: "Watch17", success: true, manateeTLKs: true, limitedTLKs: true),
                                              TestCase(model: "WinPC0,0", success: true, manateeTLKs: false, limitedTLKs: true),
                                             ])
    }

    func testJoinFromMac() throws {
        try self._testJoin(expectations: [TestCase(model: "AppleTV5,3", success: true, manateeTLKs: false, limitedTLKs: true),
                                          TestCase(model: "MacFoo", success: true, manateeTLKs: true, limitedTLKs: true),
                                          TestCase(model: "Watch17", success: true, manateeTLKs: true, limitedTLKs: true),
                                          TestCase(model: "WinPC0,0", success: true, manateeTLKs: false, limitedTLKs: true),
                                         ])
    }
}

#endif // OCTAGON
