import CoreData
import XCTest

#if OCTAGON
import CloudKitCode

class FakeCuttlefishInvocableCreator: ContainerNameToCuttlefishInvocable {
    let server: FakeCuttlefishServer

    init(server: FakeCuttlefishServer) {
        self.server = server
    }

    func client(container: String) -> CloudKitCode.Invocable {
        // TODO: this class should probably separate containers somehow.
        return self.server
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

    func serialNumber() -> String {
        return self.mockSerialNumber
    }

    func register(forDeviceNameUpdates listener: OTDeviceInformationNameUpdateListener) {
    }
}

class OTMockAuthKitAdapter: OTAuthKitAdapter {

    // A nil altDSID means 'no authkit account'
    var altDSID: String?

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
    }

    func primaryiCloudAccountAltDSID() throws -> String {
        guard let altDSID = self.altDSID else {
            throw NSError(domain: OctagonErrorDomain,
                          code: OctagonError.OTAuthKitNoPrimaryAccount.rawValue,
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
}

class OctagonTestsBase: CloudKitKeychainSyncingMockXCTest {

    var tmpPath: String!
    var tmpURL: URL!

    var manager: OTManager!
    var cuttlefishContext: OTCuttlefishContext!

    var otcliqueContext: OTConfigurationContext!

    var intendedCKKSZones: Set<CKRecordZone.ID>!
    var manateeZoneID: CKRecordZone.ID!
    var limitedPeersAllowedZoneID: CKRecordZone.ID!

    var fakeCuttlefishServer: FakeCuttlefishServer!
    var fakeCuttlefishCreator: FakeCuttlefishInvocableCreator!
    var tphClient: Client!
    var tphXPCProxy: ProxyXPCConnection!

    var accountAltDSID: String!

    // These three mock authkits will all include each other in their machine ID list
    var mockAuthKit: OTMockAuthKitAdapter!
    var mockAuthKit2: OTMockAuthKitAdapter!
    var mockAuthKit3: OTMockAuthKitAdapter!

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
        // Set the global bool to TRUE
        OctagonSetIsEnabled(true)

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

        // We'll use this set to limit the views that CKKS brings up in the tests (mostly for performance reasons)
        if self.intendedCKKSZones == nil {
            if self.mockDeviceInfo.mockModelID.contains("AppleTV") {
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

        self.fakeCuttlefishCreator = FakeCuttlefishInvocableCreator(server: self.fakeCuttlefishServer)
        self.tphClient = Client(endpoint: nil, containerMap: ContainerMap(invocableCreator: fakeCuttlefishCreator))

        self.otFollowUpController = OTMockFollowUpController()

        self.mockAuthKit = OTMockAuthKitAdapter(altDSID: UUID().uuidString, machineID: "MACHINE1", otherDevices: ["MACHINE2", "MACHINE3"])
        self.mockAuthKit2 = OTMockAuthKitAdapter(altDSID: self.mockAuthKit.altDSID, machineID: "MACHINE2", otherDevices: ["MACHINE1", "MACHINE3"])
        self.mockAuthKit3 = OTMockAuthKitAdapter(altDSID: self.mockAuthKit.altDSID, machineID: "MACHINE3", otherDevices: ["MACHINE1", "MACHINE2"])

        let tphInterface = TrustedPeersHelperSetupProtocol(NSXPCInterface(with: TrustedPeersHelperProtocol.self))
        self.tphXPCProxy = ProxyXPCConnection(self.tphClient!, interface: tphInterface)

        self.disableConfigureCKKSViewManagerWithViews = true

        // Now, perform further test initialization (including temporary keychain creation)
        super.setUp()

        self.injectedManager!.setSyncingViewsAllowList(Set(self.intendedCKKSZones!.map { $0.zoneName }))

        // Ensure we've made the CKKSKeychainView objects we're interested in
        self.ckksZones.forEach { obj in
            let zoneID = obj as! CKRecordZone.ID
            self.ckksViews.add(self.injectedManager!.findOrCreateView(zoneID.zoneName))
        }

        // Double-check that the world of zones and views looks like what we expect
        XCTAssertEqual(self.ckksZones as? Set<CKRecordZone.ID>, self.intendedCKKSZones, "should still operate on our expected zones only")
        XCTAssertEqual(self.ckksZones.count, self.ckksViews.count, "Should have the same number of views as expected zones")
        XCTAssertEqual(self.ckksZones.count, self.zones!.count, "Should have the same number of fake zones as expected zones")

        XCTAssertEqual(Set(self.ckksViews.map { ($0 as! CKKSKeychainView).zoneName }),
                       Set(self.ckksZones.map { ($0 as! CKRecordZone.ID).zoneName }),
                       "ckksViews should match ckksZones")

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
                                 deviceInformationAdapter: self.mockDeviceInfo,
                                 apsConnectionClass: FakeAPSConnection.self,
                                 escrowRequestClass: OTMockSecEscrowRequest.self,
                                 loggerClass: OTMockLogger.self,
                                 lockStateTracker: CKKSLockStateTracker(),
                                 cloudKitClassDependencies: cloudKitClassDependencies,
                                 cuttlefishXPCConnection: tphXPCProxy.connection(),
                                 cdpd: self.otFollowUpController)
        return self.manager
    }

    override func tearDown() {
        // Just to be sure
        self.verifyDatabaseMocks()

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
    }

    override func managedViewList() -> Set<String> {
        if(self.overrideUseCKKSViewsFromPolicy) {
            let viewNames = self.ckksZones.map { ($0 as! CKRecordZone.ID).zoneName }
            return Set(viewNames)
        } else {
            // We only want to return the 'base' set of views here; not the full set.
            // This should go away when CKKS4A is enabled...
            #if !os(tvOS)
            return Set(["LimitedPeersAllowed", "Manatee"])
            #else
            return Set(["LimitedPeersAllowed"])
            #endif
        }
    }

    func fetchEgoPeerID() -> String {
        var ret: String!
        let fetchPeerIDExpectation = self.expectation(description: "fetchPeerID callback occurs")
        self.cuttlefishContext.rpcFetchEgoPeerID { peerID, fetchError in
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

    func assertEnters(context: OTCuttlefishContext, state: String, within: UInt64) {
        XCTAssertEqual(0, (context.stateMachine.stateConditions[state] as! CKKSCondition).wait(within), "State machine should enter '\(state)'")
        if state == OctagonStateReady || state == OctagonStateUntrusted {
            XCTAssertEqual(0, context.stateMachine.paused.wait(10 * NSEC_PER_SEC), "State machine should pause soon")
        }
    }

    func assertPendingFlagHandled(context: OTCuttlefishContext, pendingFlag: String, within: UInt64) {
        XCTAssertEqual(0, (context.stateMachine.flags.condition(forFlag: pendingFlag)).wait(within), "State machine should have handled '\(pendingFlag)'")
    }

    func assertConsidersSelfTrusted(context: OTCuttlefishContext, isLocked: Bool = false) {
        XCTAssertEqual(context.currentMemoizedTrustState(), .TRUSTED, "Trust state (for \(context)) should be trusted")

        let accountMetadata = try! context.accountMetadataStore.loadOrCreateAccountMetadata()
        XCTAssertEqual(accountMetadata.attemptedJoin, .ATTEMPTED, "Should have 'attempted a join'")

        let statusexpectation = self.expectation(description: "trust status returns")
        let configuration = OTOperationConfiguration()
        configuration.timeoutWaitForCKAccount = 500 * NSEC_PER_MSEC
        context.rpcTrustStatus(configuration) { egoStatus, egoPeerID, _, isLocked, _  in
            XCTAssertEqual(egoStatus, .in, "Self peer (for \(context)) should be trusted")
            XCTAssertNotNil(egoPeerID, "Should have a peerID")
            XCTAssertEqual(isLocked, isLocked, "should be \(isLocked)")
            statusexpectation.fulfill()
        }
        self.wait(for: [statusexpectation], timeout: 10)
    }

    func assertConsidersSelfTrustedCachedAccountStatus(context: OTCuttlefishContext) {
        XCTAssertEqual(context.currentMemoizedTrustState(), .TRUSTED, "Trust state (for \(context)) should be trusted")

        let cachedStatusexpectation = self.expectation(description: "(cached) trust status returns")
        let configuration = OTOperationConfiguration()
        configuration.useCachedAccountStatus = true
        configuration.timeoutWaitForCKAccount = 500 * NSEC_PER_MSEC

        context.rpcTrustStatus(configuration) { egoStatus, egoPeerID, _, _, _ in
            XCTAssertEqual(egoStatus, .in, "Cached self peer (for \(context)) should be trusted")
            XCTAssertNotNil(egoPeerID, "Should have a (cached) peerID")
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
        XCTAssertEqual(status, .in, "OTClique API should return (trusted)")

        configuration.useCachedAccountStatus = false
        let statusexpectation = self.expectation(description: "(cached) trust status returns")
        context.rpcTrustStatus(configuration) { egoStatus, egoPeerID, _, _, _ in
            XCTAssertEqual(egoStatus, .in, "Self peer (for \(context)) should be trusted")
            XCTAssertNotNil(egoPeerID, "Should have a peerID")
            statusexpectation.fulfill()
        }
        self.wait(for: [statusexpectation], timeout: 10)
    }

    func assertConsidersSelfUntrusted(context: OTCuttlefishContext) {
        XCTAssertEqual(context.currentMemoizedTrustState(), .UNTRUSTED, "Trust state (for \(context)) should be untrusted")
        let statusexpectation = self.expectation(description: "trust status returns")
        let configuration = OTOperationConfiguration()
        configuration.timeoutWaitForCKAccount = 500 * NSEC_PER_MSEC
        context.rpcTrustStatus(configuration) { egoStatus, _, _, _, _ in
            // TODO: separate 'untrusted' and 'no trusted peers for account yet'
            XCTAssertTrue([.notIn, .absent].contains(egoStatus), "Self peer (for \(context)) should be distrusted or absent")
            statusexpectation.fulfill()
        }
        self.wait(for: [statusexpectation], timeout: 10)
    }

    func assertConsidersSelfWaitingForCDP(context: OTCuttlefishContext) {
        XCTAssertEqual(context.currentMemoizedTrustState(), .UNKNOWN, "Trust state (for \(context)) should be unknown")
        let statusexpectation = self.expectation(description: "trust status returns")
        let configuration = OTOperationConfiguration()
        configuration.timeoutWaitForCKAccount = 500 * NSEC_PER_MSEC
        context.rpcTrustStatus(configuration) { egoStatus, _, _, _, _ in
            // TODO: separate 'untrusted' and 'no trusted peers for account yet'
            XCTAssertTrue([.notIn, .absent].contains(egoStatus), "Self peer (for \(context)) should be distrusted or absent")
            statusexpectation.fulfill()
        }
        self.wait(for: [statusexpectation], timeout: 10)

        XCTAssertEqual(self.fetchCDPStatus(context: context), .disabled, "CDP status should be 'disabled'")
    }

    func assertAccountAvailable(context: OTCuttlefishContext) {
        XCTAssertEqual(context.currentMemoizedAccountState(), .ACCOUNT_AVAILABLE, "Account state (for \(context)) should be 'available''")
    }

    func assertNoAccount(context: OTCuttlefishContext) {
        XCTAssertEqual(context.currentMemoizedAccountState(), .NO_ACCOUNT, "Account state (for \(context)) should be no account")
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

    func assertTrusts(context: OTCuttlefishContext, includedPeerIDCount: Int, excludedPeerIDCount: Int) {
        let dumpCallback = self.expectation(description: "dump callback occurs")
        self.tphClient.dumpEgoPeer(withContainer: context.containerName, context: context.contextID) { _, _, _, dynamicInfo, error in
            XCTAssertNil(error, "should be no error")
            XCTAssertNotNil(dynamicInfo, "Should be a dynamic info")
            XCTAssertEqual(dynamicInfo!.includedPeerIDs.count, includedPeerIDCount, "should be \(includedPeerIDCount) included peer ids")
            XCTAssertEqual(dynamicInfo!.excludedPeerIDs.count, excludedPeerIDCount, "should be \(excludedPeerIDCount) excluded peer ids")

            dumpCallback.fulfill()
        }
        self.wait(for: [dumpCallback], timeout: 10)
    }

    func restartCKKSViews() {
        let viewNames = self.ckksViews.map { ($0 as! CKKSKeychainView).zoneName }
        self.ckksViews.removeAllObjects()

        self.injectedManager!.resetSyncingPolicy()

        for view in viewNames {
            self.ckksViews.add(self.injectedManager!.restartZone(view))
        }
    }

    func sendAllCKKSTrustedPeersChanged() {
        self.ckksViews.forEach { view in
            (view as! CKKSKeychainView).trustedPeerSetChanged(nil)
        }
    }

    func sendAllCKKSViewsZoneChanged() {
        for expectedView in self.ckksZones {
            let view = self.injectedManager?.findView((expectedView as! CKRecordZone.ID).zoneName)
            XCTAssertNotNil(view, "Should have a view '\(expectedView)'")
            view!.notifyZoneChange(nil)
        }
    }

    func assert(ckks: CKKSKeychainView, enters: String, within: UInt64) {
        XCTAssertEqual(0, (ckks.keyHierarchyConditions[enters] as! CKKSCondition).wait(within), "CKKS state machine should enter '\(enters)' (currently '\(ckks.keyHierarchyState)')")
    }

    func assertAllCKKSViews(enter: String, within: UInt64) {
        for expectedView in self.ckksZones {
            let view = self.injectedManager?.findView((expectedView as! CKRecordZone.ID).zoneName)
            XCTAssertNotNil(view, "Should have a view '\(expectedView)'")
            self.assert(ckks: view!, enters: enter, within: within)
        }
    }

    func assertAllCKKSViewsUploadKeyHierarchy(tlkShares: UInt) {
        for expectedView in self.ckksZones {
            let view = self.injectedManager?.findView((expectedView as! CKRecordZone.ID).zoneName)
            XCTAssertNotNil(view, "Should have a view '\(expectedView)'")
            self.expectCKModifyKeyRecords(3, currentKeyPointerRecords: 3, tlkShareRecords: tlkShares, zoneID: view!.zoneID)
        }
    }

    func assertAllCKKSViewsUpload(tlkShares: UInt) {
        for expectedView in self.ckksZones {
            self.expectCKModifyKeyRecords(0, currentKeyPointerRecords: 0, tlkShareRecords: tlkShares, zoneID: expectedView as! CKRecordZone.ID)
        }
    }

    func putFakeKeyHierarchiesInCloudKit() {
        self.ckksZones.forEach { zone in
            self.putFakeKeyHierarchy(inCloudKit: zone as! CKRecordZone.ID)
        }
    }

    func putSelfTLKSharesInCloudKit() {
        self.ckksZones.forEach { zone in
            self.putSelfTLKShares(inCloudKit: zone as! CKRecordZone.ID)
        }
    }

    func putFakeDeviceStatusesInCloudKit() {
        self.ckksZones.forEach { zone in
            self.putFakeDeviceStatus(inCloudKit: zone as! CKRecordZone.ID)
        }
    }

    func saveTLKMaterialToKeychain() {
        self.ckksZones.forEach { zone in
            self.saveTLKMaterial(toKeychain: zone as! CKRecordZone.ID)
        }
    }

    func resetAllCKKSViews() {
        let resetExpectation = self.expectation(description: "rpcResetCloudKit callback occurs")
        self.injectedManager!.rpcResetCloudKit(nil, reason: "octagon=unit-test") { error in
            XCTAssertNil(error, "Error should be nil?")
            resetExpectation.fulfill()
        }

        self.wait(for: [resetExpectation], timeout: 30)
    }

    func putSelfTLKSharesInCloudKit(context: OTCuttlefishContext) throws {
        try self.ckksZones.forEach { zone in
            try self.putTLKShareInCloudKit(to: context, from: context, zoneID: zone as! CKRecordZone.ID)
        }
    }

    func putAllTLKSharesInCloudKit(to: OTCuttlefishContext, from: OTCuttlefishContext) throws {
        try self.ckksZones.forEach { zone in
            try self.putTLKShareInCloudKit(to: to, from: from, zoneID: zone as! CKRecordZone.ID)
        }
    }

    func putTLKShareInCloudKit(to: OTCuttlefishContext, from: OTCuttlefishContext, zoneID: CKRecordZone.ID) throws {
        let fromAccountMetadata = try from.accountMetadataStore.loadOrCreateAccountMetadata()
        let fromPeerKeys: OctagonSelfPeerKeys = try loadEgoKeysSync(peerID: fromAccountMetadata.peerID)

        let toAccountMetadata = try to.accountMetadataStore.loadOrCreateAccountMetadata()
        let toPeerKeys: OctagonSelfPeerKeys = try loadEgoKeysSync(peerID: toAccountMetadata.peerID)

        let zoneKeys = self.keys![zoneID] as! ZoneKeys
        self.putTLKShare(inCloudKit: zoneKeys.tlk!, from: fromPeerKeys, to: toPeerKeys, zoneID: zoneID)
    }

    func putRecoveryKeyTLKSharesInCloudKit(recoveryKey: String, salt: String) throws {
        try self.ckksZones.forEach { zone in
            try self.putRecoveryKeyTLKShareInCloudKit(recoveryKey: recoveryKey, salt: salt, zoneID: zone as! CKRecordZone.ID)
        }
    }

    func putRecoveryKeyTLKShareInCloudKit(recoveryKey: String, salt: String, zoneID: CKRecordZone.ID) throws {
        let recoveryKeys = try RecoveryKey(recoveryKeyString: recoveryKey, recoverySalt: salt)

        let zoneKeys = self.keys![zoneID] as! ZoneKeys
        self.putTLKShare(inCloudKit: zoneKeys.tlk!, from: recoveryKeys.peerKeys, to: recoveryKeys.peerKeys, zoneID: zoneID)
    }

    func assertSelfTLKSharesInCloudKit(context: OTCuttlefishContext) {
        let accountMetadata = try! context.accountMetadataStore.loadOrCreateAccountMetadata()
        self.assertSelfTLKSharesInCloudKit(peerID: accountMetadata.peerID)
    }

    func assertSelfTLKSharesInCloudKit(peerID: String) {
        self.assertTLKSharesInCloudKit(receiverPeerID: peerID, senderPeerID: peerID)
    }

    func assertTLKSharesInCloudKit(receiver: OTCuttlefishContext, sender: OTCuttlefishContext) {
        let receiverAccountMetadata = try! receiver.accountMetadataStore.loadOrCreateAccountMetadata()
        let senderAccountMetadata = try! sender.accountMetadataStore.loadOrCreateAccountMetadata()

        self.assertTLKSharesInCloudKit(receiverPeerID: receiverAccountMetadata.peerID,
                                       senderPeerID: senderAccountMetadata.peerID)
    }

    func assertTLKSharesInCloudKit(receiverPeerID: String, senderPeerID: String) {
        for case let view as CKKSKeychainView in self.ckksViews {
            XCTAssertNoThrow(try self.assertTLKShareInCloudKit(receiverPeerID: receiverPeerID, senderPeerID: senderPeerID, zoneID: view.zoneID), "view \(view) should have a self TLK uploaded")
        }
    }

    func tlkShareInCloudKit(receiverPeerID: String, senderPeerID: String, zoneID: CKRecordZone.ID) throws -> Bool {
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

    func assertTLKShareInCloudKit(receiverPeerID: String, senderPeerID: String, zoneID: CKRecordZone.ID) throws {
        XCTAssertTrue(try self.tlkShareInCloudKit(receiverPeerID: receiverPeerID, senderPeerID: senderPeerID, zoneID: zoneID),
                      "Should have found a TLKShare for peerID \(String(describing: receiverPeerID)) sent by \(String(describing: senderPeerID)) for \(zoneID)")
    }

    func assertMIDList(context: OTCuttlefishContext,
                       allowed: Set<String>,
                       disallowed: Set<String> = Set(),
                       unknown: Set<String> = Set()) {
        let container = try! self.tphClient.getContainer(withContainer: context.containerName, context: context.contextID)
        container.moc.performAndWait {
            let midList = container.onqueueCurrentMIDList()
            XCTAssertEqual(midList.machineIDs(in: .allowed), allowed, "Model's allowed list should match pattern")
            XCTAssertEqual(midList.machineIDs(in: .disallowed), disallowed, "Model's disallowed list should match pattern")
            XCTAssertEqual(midList.machineIDs(in: .unknown), unknown, "Model's unknown list should match pattern")
        }

        for allowedMID in allowed {
            var err: NSError?
            let onList = context.machineID(onMemoizedList: allowedMID, error: &err)

            XCTAssertNil(err, "Should not have failed determining memoized list state")
            XCTAssertTrue(onList, "MID on allowed list should return 'is on list'")

            do {
                let number = try context.numberOfPeersInModel(withMachineID: allowedMID)
                XCTAssert(number.intValue >= 0, "Should have a non-negative number for numberOfPeersInModel")
            } catch {
                XCTFail("Should not have failed fetching the number of peers with a mid: \(error)")
            }
        }

        for disallowedMID in disallowed {
            var err: NSError?
            let onList = context.machineID(onMemoizedList: disallowedMID, error: &err)

            XCTAssertNil(err, "Should not have failed determining memoized list state")
            XCTAssertFalse(onList, "MID on allowed list should return 'not on list'")
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

    // Please ensure that the first state in this list is not the state that the context is currently in
    func sendContainerChangeWaitForFetchForStates(context: OTCuttlefishContext, states: [String]) {
        // Pull the first state out before sending the notification
        let firstState = states.first!
        let firstCondition = (context.stateMachine.stateConditions[firstState] as! CKKSCondition)

        let updateTrustExpectation = self.expectation(description: "fetchChanges")

        self.fakeCuttlefishServer.fetchChangesListener = { request in
            self.fakeCuttlefishServer.fetchChangesListener = nil
            updateTrustExpectation.fulfill()
            return nil
        }
        context.notifyContainerChange(nil)
        self.wait(for: [updateTrustExpectation], timeout: 10)

        // Wait for the previously acquired first state, then wait for each in turn
        XCTAssertEqual(0, firstCondition.wait(10 * NSEC_PER_SEC), "State machine should enter '\(String(describing: firstState))'")
        for state in states.dropFirst() {
            self.assertEnters(context: context, state: state, within: 10 * NSEC_PER_SEC)
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
                                    lockStateTracker: self.lockStateTracker,
                                    accountStateTracker: self.accountStateTracker,
                                    deviceInformationAdapter: self.makeInitiatorDeviceInfoAdapter())
    }

    @discardableResult
    func assertResetAndBecomeTrustedInDefaultContext() -> String {
        let ret = self.assertResetAndBecomeTrusted(context: self.cuttlefishContext)

        // And, the default context runs CKKS:
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()
        self.assertSelfTLKSharesInCloudKit(context: self.cuttlefishContext)

        return ret
    }

    @discardableResult
    func assertResetAndBecomeTrusted(context: OTCuttlefishContext) -> String {
        context.startOctagonStateMachine()
        XCTAssertNoThrow(try context.setCDPEnabled())
        self.assertEnters(context: context, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        do {
            let arguments = OTConfigurationContext()
            arguments.altDSID = try context.authKitAdapter.primaryiCloudAccountAltDSID()
            arguments.context = context.contextID
            arguments.otControl = self.otControl

            let clique = try OTClique.newFriends(withContextData: arguments, resetReason: .testGenerated)
            XCTAssertNotNil(clique, "Clique should not be nil")
        } catch {
            XCTFail("Shouldn't have errored making new friends: \(error)")
        }

        self.assertEnters(context: context, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: context)

        return try! context.accountMetadataStore.getEgoPeerID()
    }

    @discardableResult
    func assertJoinViaEscrowRecovery(joiningContext: OTCuttlefishContext, sponsor: OTCuttlefishContext) -> String {
        do {
            joiningContext.startOctagonStateMachine()

            let sponsorPeerID = try sponsor.accountMetadataStore.loadOrCreateAccountMetadata().peerID
            XCTAssertNotNil(sponsorPeerID, "sponsorPeerID should not be nil")
            let entropy = try self.loadSecret(label: sponsorPeerID!)
            XCTAssertNotNil(entropy, "entropy should not be nil")

            let altDSID = try joiningContext.authKitAdapter.primaryiCloudAccountAltDSID()
            XCTAssertNotNil(altDSID, "Should have an altDSID")

            let bottles = self.fakeCuttlefishServer.state.bottles.filter { $0.peerID == sponsorPeerID }
            XCTAssertEqual(bottles.count, 1, "Should have a single bottle for the approving peer")
            let bottle = bottles[0]

            let joinWithBottleExpectation = self.expectation(description: "joinWithBottle callback occurs")
            joiningContext.join(withBottle: bottle.bottleID, entropy: entropy!, bottleSalt: altDSID) { error in
                XCTAssertNil(error, "error should be nil")
                joinWithBottleExpectation.fulfill()
            }
            self.wait(for: [joinWithBottleExpectation], timeout: 10)

            self.assertEnters(context: joiningContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
            self.assertConsidersSelfTrusted(context: joiningContext)

            return try joiningContext.accountMetadataStore.getEgoPeerID()
        } catch {
            XCTFail("Expected no error: \(error)")
            return "failed"
        }
    }

    func assertJoinViaProximitySetup(joiningContext: OTCuttlefishContext, sponsor: OTCuttlefishContext) -> String {
        do {
            joiningContext.startOctagonStateMachine()
            self.assertEnters(context: joiningContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

            let (sponsorPairingChannel, initiatorPairingChannel) = self.setupPairingChannels(initiator: joiningContext, sponsor: sponsor)

            let firstInitiatorPacket = self.sendPairingExpectingReply(channel: initiatorPairingChannel, packet: nil, reason: "session initialization")
            let sponsorEpochPacket = self.sendPairingExpectingReply(channel: sponsorPairingChannel, packet: firstInitiatorPacket, reason: "sponsor epoch")
            let initiatorIdentityPacket = self.sendPairingExpectingReply(channel: initiatorPairingChannel, packet: sponsorEpochPacket, reason: "initiator identity")
            let sponsorVoucherPacket = self.sendPairingExpectingCompletionAndReply(channel: sponsorPairingChannel, packet: initiatorIdentityPacket, reason: "sponsor voucher")
            self.sendPairingExpectingCompletion(channel: initiatorPairingChannel, packet: sponsorVoucherPacket, reason: "initiator completion")

            self.assertEnters(context: joiningContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
            self.assertConsidersSelfTrusted(context: joiningContext)

            return try joiningContext.accountMetadataStore.getEgoPeerID()
        } catch {
            XCTFail("Expected no error: \(error)")
            return "failed"
        }
    }

    func assertSelfOSVersion(_ osVersion: String) {

        let statusExpectation = self.expectation(description: "status callback occurs")
        self.tphClient.dumpEgoPeer(withContainer: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) { _, _, stableInfo, _, error in
            XCTAssertNil(error, "should be no error dumping ego peer")
            XCTAssertEqual(stableInfo?.osVersion, osVersion, "os version should be as required")
            statusExpectation.fulfill()
        }

        self.wait(for: [statusExpectation], timeout: 2)
    }
}

class OctagonTests: OctagonTestsBase {
    func testTPHPrepare() {
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
                          signingPrivKeyPersistentRef: nil,
                          encPrivKeyPersistentRef: nil) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, _, _, error in
                            XCTAssertNil(error, "Should be no error preparing identity")
                            XCTAssertNotNil(peerID, "Should be a peer ID")
                            XCTAssertNotNil(permanentInfo, "Should have a permenent info")
                            XCTAssertNotNil(permanentInfoSig, "Should have a permanent info signature")
                            XCTAssertNotNil(stableInfo, "Should have a stable info")
                            XCTAssertNotNil(stableInfoSig, "Should have a stable info signature")

                            tphPrepareExpectation.fulfill()
        }

        self.wait(for: [tphPrepareExpectation], timeout: 10)
    }

    func testTPHMultiPrepare() throws {
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
                          signingPrivKeyPersistentRef: nil,
                          encPrivKeyPersistentRef: nil) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, _, _, error in
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
                          signingPrivKeyPersistentRef: nil,
                          encPrivKeyPersistentRef: nil) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, _, _, error in
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
                          signingPrivKeyPersistentRef: nil,
                          encPrivKeyPersistentRef: nil) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, _, _, error in
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
        XCTAssertNotNil(self.injectedManager?.policy, "Should have given CKKS a TPPolicy during initialization")
        XCTAssertEqual(self.injectedManager?.policy?.version, prevailingPolicyVersion, "Policy given to CKKS should be prevailing policy")

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
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKUpload, within: 10 * NSEC_PER_SEC)
        self.cuttlefishContext.rpcStatus { _, _ in
            }

        self.verifyDatabaseMocks()
        XCTAssertEqual(0, self.cuttlefishContext.stateMachine.paused.wait(60 * NSEC_PER_SEC), "Main cuttlefish context should quiesce before the test ends")
    }

    func testNewFriendsForEmptyAccountWithTLKs() throws {
        self.putFakeKeyHierarchiesInCloudKit()
        self.saveTLKMaterialToKeychain()

        self.startCKAccountStatusMock()

        // CKKS should go into 'logged out', as Octagon hasn't told it to go yet
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateLoggedOut, within: 10 * NSEC_PER_SEC)

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTrust, within: 10 * NSEC_PER_SEC)

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

        // and all subCKKSes should enter ready upload...
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
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
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfTrusted(context: self.cuttlefishContext)

        // Ensure CKKS has a policy after newFriends
        XCTAssertNotNil(self.injectedManager?.policy, "Should have given CKKS a TPPolicy during initialization")
        XCTAssertEqual(self.injectedManager?.policy?.version, prevailingPolicyVersion, "Policy given to CKKS should be prevailing policy")

        let peerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        XCTAssertNotNil(peerID, "Should have a peer ID after making new friends")

        var readyDate = CKKSAnalytics.logger().dateProperty(forKey: OctagonAnalyticsLastKeystateReady)
        XCTAssertNotNil(readyDate, "Should have a ready date")
        XCTAssert(readyDate! > startDate, "ready date should be after startdate")

        // Now restart the context
        self.manager.removeContext(forContainerName: OTCKContainerName, contextID: OTDefaultContext)
        self.restartCKKSViews()
        self.cuttlefishContext = self.manager.context(forContainerName: OTCKContainerName, contextID: OTDefaultContext)

        XCTAssertNil(self.injectedManager?.policy, "CKKS should not have a policy after 'restart'")

        let restartDate = Date()
        self.cuttlefishContext.startOctagonStateMachine()
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        self.assertConsidersSelfTrustedCachedAccountStatus(context: self.cuttlefishContext)

        let restartedPeerID = try self.cuttlefishContext.accountMetadataStore.getEgoPeerID()
        XCTAssertNotNil(restartedPeerID, "Should have a peer ID after restarting")

        XCTAssertEqual(peerID, restartedPeerID, "Should have the same peer ID after restarting")
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        XCTAssertNotNil(self.injectedManager?.policy, "Should have given CKKS a TPPolicy after restart")
        XCTAssertEqual(self.injectedManager?.policy?.version, prevailingPolicyVersion, "Policy given to CKKS after restart should be prevailing policy")

        readyDate = CKKSAnalytics.logger().dateProperty(forKey: OctagonAnalyticsLastKeystateReady)
        XCTAssertNotNil(readyDate, "Should have a ready date")
        XCTAssert(readyDate! > restartDate, "ready date should be after re-startdate")
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

    func testRestoreToNewClique() throws {
        self.startCKAccountStatusMock()

        do {
            _ = try OTClique.performEscrowRecovery(withContextData: self.otcliqueContext, escrowArguments: [:])
            XCTFail("performEscrowRecovery is not currently testable, as we haven't mocked out SBD.")
        } catch {
            XCTAssert(true, "Should have errored restoring backup with no arguments: \(error)")
        }
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

        let memberIdentifier = clique!.cliqueMemberIdentifier

        let dumpExpectation = self.expectation(description: "dump callback occurs")
        self.tphClient.dump(withContainer: self.cuttlefishContext.containerName, context: self.cuttlefishContext.contextID) { dump, error in
            XCTAssertNil(error, "Should be no error dumping data")
            XCTAssertNotNil(dump, "dump should not be nil")
            let egoSelf = dump!["self"] as? [String: AnyObject]
            XCTAssertNotNil(egoSelf, "egoSelf should not be nil")
            let peerID = egoSelf!["peerID"] as? String
            XCTAssertNotNil(peerID, "peerID should not be nil")
            XCTAssertEqual(memberIdentifier, peerID, "memberIdentifier should match tph's peer ID")

            dumpExpectation.fulfill()
        }
        self.wait(for: [dumpExpectation], timeout: 10)
    }

    func testLeaveClique() throws {
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
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)
        self.verifyDatabaseMocks()

        // Technically, it's a server-side cuttlefish error for the last signed-in peer to leave. But, for now, just go for it.
        XCTAssertNoThrow(try clique.leave(), "Should be no error departing clique")

        // securityd should now consider itself untrusted
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTrust, within: 10 * NSEC_PER_SEC)

        // TODO: an item added here shouldn't sync
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
        assertAllCKKSViews(enter: SecCKKSZoneKeyStateReady, within: 10 * NSEC_PER_SEC)

        let peer1ID = fetchEgoPeerID()

        do {
            let peersByID = try clique.peerDeviceNamesByPeerID()
            XCTAssertNotNil(peersByID, "Should have received information on peers")
            XCTAssertTrue(peersByID.isEmpty, "peer1 should report no trusted peers")
        } catch {
            XCTFail("Error thrown: \(error)")
        }

        let peer2DeviceName = "peer2-asdf"

        let peer2DeviceAdapter = OTMockDeviceInfoAdapter(modelID: "AppleTV5,3",
                                                         deviceName: peer2DeviceName,
                                                         serialNumber: "peer2-asdf",
                                                         osVersion: "tvOS (fake version)")

        // Now, fake up a voucher for the second peer using TPH
        let peer2ContextID = "asdf"
        let peer2 = self.manager.context(forContainerName: OTCKContainerName,
                                         contextID: peer2ContextID,
                                         sosAdapter: self.mockSOSAdapter,
                                         authKitAdapter: self.mockAuthKit2,
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
                               signingPrivKeyPersistentRef: nil,
                               encPrivKeyPersistentRef: nil) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, _, _, error in
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
                                                                            preapprovedKeys: []) { peerID, _, _, _, error in
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
        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)

        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer1ID, opinion: .trusts, target: peer2ID)),
                      "peer 1 should trust peer 2")
        XCTAssertTrue(self.fakeCuttlefishServer.assertCuttlefishState(FakeCuttlefishAssertion(peer: peer2ID, opinion: .trusts, target: peer1ID)),
                      "peer 2 should trust peer 1")

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
        OctagonAuthoritativeTrustSetIsEnabled(true)

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

        // Technically, it's a server-side cuttlefish error for the last signed-in peer to leave. But, for now, just go for it.
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
                                                    path: path!,
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
                                                    path: path!,
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

        OctagonAuthoritativeTrustSetIsEnabled(true)
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

        // Technically, it's a server-side cuttlefish error for the last signed-in peer to leave. But, for now, just go for it.
        XCTAssertNoThrow(try clique.leave(), "Should be no error departing clique")

        // securityd should now consider itself untrusted
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertConsidersSelfUntrusted(context: self.cuttlefishContext)

        // As a bonus, calling leave again should be fast (and not error)
        XCTAssertNoThrow(try clique.leave(), "Should be no error departing clique (again)")
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)

        let cfuExpectation = self.expectation(description: "cfu callback occurs")
        self.cuttlefishContext.followupHandler.clearAllPostedFlags()
        self.cuttlefishContext.checkTrustStatusAndPostRepairCFUIfNecessary { _, posted, _, error in
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

        self.aksLockState = true
        self.lockStateTracker.recheck()

        let initiatorContext = self.manager.context(forContainerName: OTCKContainerName,
                                                    contextID: OTDefaultContext,
                                                    sosAdapter: self.mockSOSAdapter,
                                                    authKitAdapter: self.mockAuthKit2,
                                                    lockStateTracker: self.lockStateTracker,
                                                    accountStateTracker: self.accountStateTracker,
                                                    deviceInformationAdapter: OTMockDeviceInfoAdapter(modelID: "iPhone9,1", deviceName: "test-SOS-iphone", serialNumber: "456", osVersion: "iOS (fake version)"))

        let accountMetadataStoreActual = initiatorContext.accountMetadataStore //grab the previously instantiated account state holder

        initiatorContext.setAccountStateHolder(self.accountMetaDataStore) // set the mocked account state holder

        initiatorContext.startOctagonStateMachine()
        self.assertEnters(context: initiatorContext, state: OctagonStateWaitForUnlock, within: 10 * NSEC_PER_SEC)

        initiatorContext.setAccountStateHolder(accountMetadataStoreActual) //re-set the actual store
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
        let expectedViews = Set(["ProtectedCloudStorage",
                                 "AutoUnlock",
                                 "LimitedPeersAllowed",
                                 "SecureObjectSync",
                                 "DevicePairing",
                                 "Engram",
                                 "Home",
                                 "Applications",
                                 "WiFi",
                                 "Health",
                                 "Manatee",
                                 // <rdar://problem/57810109> Cuttlefish: remove Safari prefix from view names
                                 "SafariCreditCards",
                                 "SafariPasswords",
                                 "ApplePay", ])
        #else
        let expectedViews = Set(["LimitedPeersAllowed",
                                 "Home",
                                 "WiFi", ])
        #endif

        let getViewsExpectation = self.expectation(description: "getViews callback happens")
        self.tphClient.fetchCurrentPolicy(withContainer: OTCKContainerName, context: OTDefaultContext) { outViews, _, error in
            XCTAssertNil(error, "should not have failed")
            XCTAssertEqual(expectedViews, Set(outViews!))
            getViewsExpectation.fulfill()
        }
        self.wait(for: [getViewsExpectation], timeout: 10)
    }

    let octagonNotificationName = "com.apple.security.octagon.trust-status-change"

    func testNotifications() throws {

        let contextName = OTDefaultContext
        let containerName = OTCKContainerName

        let untrustedNotification = XCTDarwinNotificationExpectation(notificationName: octagonNotificationName)

        self.startCKAccountStatusMock()

        self.cuttlefishContext.startOctagonStateMachine()
        XCTAssertNoThrow(try self.cuttlefishContext.setCDPEnabled())
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.wait(for: [untrustedNotification], timeout: 2)

        let trustedNotification = XCTDarwinNotificationExpectation(notificationName: octagonNotificationName)

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
        self.fakeCuttlefishServer.updateListener = { request in
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
        self.fakeCuttlefishServer.updateListener = { request in
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
        self.fakeCuttlefishServer.updateListener = { request in
            XCTFail("Shouldn't have received another updateTrust")
            return nil
        }

        self.sendContainerChangeWaitForFetch(context: self.cuttlefishContext)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)
    }

    func testAPSRateLimiter() throws {

        let untrustedNotification = XCTDarwinNotificationExpectation(notificationName: octagonNotificationName)

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
        XCTAssertEqual(self.fakeCuttlefishServer.fetchChangesCalledCount, 2, "fetchChanges should have been called 1 times")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)
    }

    func testAPSExpectOneFetchChanges() {
        let untrustedNotification = XCTDarwinNotificationExpectation(notificationName: octagonNotificationName)

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
        self.fakeCuttlefishServer.fetchChangesListener = { _ in
            fetchExpectation.fulfill()
            return nil
        }

        self.cuttlefishContext.notifyContainerChange(nil)
        self.cuttlefishContext.notifyContainerChange(nil)
        self.cuttlefishContext.notifyContainerChange(nil)
        self.cuttlefishContext.notifyContainerChange(nil)

        self.wait(for: [fetchExpectation], timeout: 10)
        XCTAssertEqual(self.fakeCuttlefishServer.fetchChangesCalledCount, 2, "fetchChanges should have been called 1 times")

        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateUntrusted, within: 10 * NSEC_PER_SEC)
        self.assertAllCKKSViews(enter: SecCKKSZoneKeyStateWaitForTLKCreation, within: 10 * NSEC_PER_SEC)
    }

    func testEnsureNonDefaultTimeOut() {
        self.startCKAccountStatusMock()
        OctagonSetPlatformSupportsSOS(true)

        let initiatorPiggybackingConfig = OTJoiningConfiguration(protocolType: OTProtocolPiggybacking,
                                                                 uniqueDeviceID: "initiator",
                                                                 uniqueClientID: "acceptor",
                                                                 pairingUUID: UUID().uuidString,
                                                                 containerName: OTCKContainerName,
                                                                 contextID: OTDefaultContext,
                                                                 epoch: 1,
                                                                 isInitiator: true)

        let resetAndEstablishExpectation = self.expectation(description: "resetAndEstablish callback occurs")
        self.manager.resetAndEstablish(OTCKContainerName,
                                       context: OTDefaultContext,
                                       altDSID: "new altDSID",
                                       resetReason: .testGenerated) { resetError in
                                        XCTAssertNil(resetError, "Should be no error calling resetAndEstablish")
                                        resetAndEstablishExpectation.fulfill()
        }

        self.wait(for: [resetAndEstablishExpectation], timeout: 10)
        self.assertEnters(context: self.cuttlefishContext, state: OctagonStateReady, within: 10 * NSEC_PER_SEC)

        /*begin message passing*/
        let rpcFirstInitiatorJoiningMessageCallBack = self.expectation(description: "Creating prepare message callback")
        self.manager.rpcPrepareIdentityAsApplicant(with: initiatorPiggybackingConfig) { pID, pI, pIS, sI, sIS, error in
            XCTAssertNil(pID, "peerID should not be nil")
            XCTAssertNil(pI, "permanentInfo should not be nil")
            XCTAssertNil(pIS, "permanentInfoSig should not be nil")
            XCTAssertNil(sI, "stableInfo should not be nil")
            XCTAssertNil(sIS, "stableInfoSig should not be nil")

            XCTAssertNotNil(error, "error should not be nil")
            rpcFirstInitiatorJoiningMessageCallBack.fulfill()
        }
        //default value for ops is 10 seconds, ensuring the rpc times out in 2 seconds which is the new value set on the request
        self.wait(for: [rpcFirstInitiatorJoiningMessageCallBack], timeout: 3)

        //this call uses the default value and should timeout in 10 seconds
        let upgradeCallback = self.expectation(description: "attempting sos upgrade callback")
        self.manager.attemptSosUpgrade(OTCKContainerName, context: OTDefaultContext) { error in
            XCTAssertNotNil(error, "error should not be nil")
            upgradeCallback.fulfill()
        }
        self.wait(for: [upgradeCallback], timeout: 11)
    }

    func testTTRTrusted() {

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

        self.wait(for: [self.ttrExpectation!], timeout: 10)
    }
}

class OctagonTestsOverrideModelBase: OctagonTestsBase {
    struct TestCase {
        let model: String
        let success: Bool
        let manateeTLKs: Bool
        let limitedTLKs: Bool
    }

    func assertTLKs(expectation: TestCase, receiverPeerID: String, senderPeerID: String) throws {
        let haveManateeTLK = try self.tlkShareInCloudKit(receiverPeerID: receiverPeerID,
                                                         senderPeerID: senderPeerID,
                                                         zoneID: self.manateeZoneID)
        let haveLimitedPeersAllowedTLK = try self.tlkShareInCloudKit(receiverPeerID: receiverPeerID,
                                                                     senderPeerID: senderPeerID,
                                                                     zoneID: self.limitedPeersAllowedZoneID)

        XCTAssertEqual(haveManateeTLK, expectation.manateeTLKs, "manatee should be what's expected: \(expectation)")
        XCTAssertEqual(haveLimitedPeersAllowedTLK, expectation.limitedTLKs, "limited should be what's expected: \(expectation)")
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

        let ckksKeys: [CKKSKeychainBackedKeySet] = self.ckksViews.compactMap { view in
            let viewName = (view as! CKKSKeychainView).zoneName
            let currentKeySet = CKKSCurrentKeySet.load(forZone: CKRecordZone.ID(zoneName: viewName))
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
                                   signingPrivKeyPersistentRef: nil,
                                   encPrivKeyPersistentRef: nil) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, _, _, error in
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

                                                                try! self.assertTLKs(expectation: testCase,
                                                                                     receiverPeerID: peerID!,
                                                                                     senderPeerID: senderPeerID)

                                                                self.tphClient.join(withContainer: OTCKContainerName,
                                                                                    context: peer2ContextID,
                                                                                    voucherData: voucher!,
                                                                                    voucherSig: voucherSig!,
                                                                                    ckksKeys: [],
                                                                                    tlkShares: [],
                                                                                    preapprovedKeys: []) { peerID, _, _, _, error in
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

                                                                try! self.assertTLKs(expectation: testCase,
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
                                   signingPrivKeyPersistentRef: nil,
                                   encPrivKeyPersistentRef: nil) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, _, _, error in
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
                                                        preapprovedKeys: []) { peerID, _, _, _, error in
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

            try self.assertTLKs(expectation: testCase,
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
                                              TestCase(model: "Watch17", success: false, manateeTLKs: false, limitedTLKs: false), ])
    }

    func testJoinFromTV() throws {
        try self._testJoin(expectations: [TestCase(model: "AppleTV5,3", success: true, manateeTLKs: false, limitedTLKs: true),
                                          TestCase(model: "MacFoo", success: false, manateeTLKs: false, limitedTLKs: false),
                                          TestCase(model: "Watch17", success: false, manateeTLKs: false, limitedTLKs: false), ])
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
                                              TestCase(model: "Watch17", success: true, manateeTLKs: true, limitedTLKs: true), ])
    }

    func testJoinFromMac() throws {
        try self._testJoin(expectations: [TestCase(model: "AppleTV5,3", success: true, manateeTLKs: false, limitedTLKs: true),
                                          TestCase(model: "MacFoo", success: true, manateeTLKs: true, limitedTLKs: true),
                                          TestCase(model: "Watch17", success: true, manateeTLKs: true, limitedTLKs: true), ])
    }
}

#endif // OCTAGON
