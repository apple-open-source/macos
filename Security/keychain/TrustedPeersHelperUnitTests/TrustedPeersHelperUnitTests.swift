//
//  TrustedPeersHelperUnitTests.swift
//  TrustedPeersHelperUnitTests
//
//  Created by Ben Williamson on 5/1/18.
//

import CoreData
import InternalSwiftProtobuf
import XCTest

let testDSID = "123456789"

let signingKey_384 = Data(base64Encoded: "BOQbPoiBnzuA0Cgc2QegjKGJqDtpkRenHwAxkYKJH1xELdaoIh8ifSch8sl18tpBYVUpEfdxz2ZSKif+dx7UPfu8WeTtpHkqm3M+9PTjr/KNNJCSR1PQNB5Jh+sRiQ+cpJnoTzm+IZSIukylamAcL3eA0nMUM0Zc2u4TijrbTgVND22WzSirUkwSK3mA/prk9A==")

let encryptionKey_384 = Data(base64Encoded: "BE1RuazBWmSEx0XVGhobbrdSE6fRQOrUrYEQnBkGl4zJq9GCeRoYvbuWNYFcOH0ijCRz9pYILsTn3ajT1OknlvcKmuQ7SeoGWzk9cBZzT5bBEwozn2gZxn80DQoDkmejywlH3D0/cuV6Bxexu5KMAFGqg6eN6th4sQABL5EuI9zKPuxHStM/b9B1LyqcnRKQHA==")

let symmetricKey_384 = Data(base64Encoded: "MfHje3Y/mWV0q+grjwZ4VxuqB7OreYHLxYkeeCiNjjY=")

let recovery_signingKey_384 = Data(base64Encoded: "BK5nrmP6oitJHtGV2Josk5cUKnG3pqxgEP8uzyPtNXgAMNHZoDKwCKFXpUzQSgbYiR4G2XZY2Q0+qSCKN7YSY2KNKE0hM9p4GvABBmAWKW/O9eFd5ugKQWisn25a/7nieIw8CQ81hBDR7R/vBpfLVtzE8ieRA8JPGqulQ5RdLcClFrD3B8BPJAZpLv4OP1CLDA==")

let recovery_encryptionKey_384 = Data(base64Encoded: "BKkZpYHTbMi2yrWFo+ErM3HbcYJCngPuWDYoVUD7egKkmiHFvv1Bsk0j/Dcj3xTR12vj5QOpZQV3GzE5estf75BV+EZz1cjUUSi/MysfpKsqEbwYrhIEkmeyMGr7CVWQWRLR2LnoihnQajvWi1LmO0AoDl3+LzVgTJBjjDQ5ANyw0Yv1EgOgBvZsLA9UTN4oAg==")

let twoDaysInThePast = -86400 * 2

class TrustedPeersHelperUnitTests: XCTestCase {
    var tmpPath: String!
    var tmpURL: URL!
    var cuttlefish: FakeCuttlefishServer!
    var mcAdapterPlaceholder: OTManagedConfigurationAdapter!

    var manateeKeySet: CKKSKeychainBackedKeySet!

    override static func setUp() {
        super.setUp()

#if SEC_XR
        TPSetBecomeiPadOverride(false)
#endif

        SecTapToRadar.disableTTRsEntirely()

        UserDefaults.standard.register(defaults: ["com.apple.CoreData.ConcurrencyDebug": 1])

        // Turn on NO_SERVER stuff
        securityd_init_local_spi()

        SecCKKSDisable()
        KCSharingSetChangeTrackingEnabled(false)
    }

    override func setUp() {
        super.setUp()

        autoreleasepool {
            let testName = self.name.components(separatedBy: CharacterSet(charactersIn: " ]"))[1]
            cuttlefish = FakeCuttlefishServer(nil, ckZones: [:], ckksZoneKeys: [:])
            mcAdapterPlaceholder = FakeManagedConfiguration()

            // Make a new fake keychain
            tmpPath = String(format: "/tmp/%@-%X", testName, Int.random(in: 0..<1000000))
            tmpURL = URL(fileURLWithPath: tmpPath, isDirectory: true)
            do {
                try FileManager.default.createDirectory(atPath: String(format: "%@/Library/Keychains", tmpPath), withIntermediateDirectories: true, attributes: nil)
                SecSetCustomHomeURLString(tmpPath as CFString)
                SecKeychainDbReset(nil)
            } catch {
                XCTFail("setUp failed: \(error)")
            }
        }

        // Actually load the database.
        kc_with_dbt(true, nil) { _ in
            false
        }

        // Now that the keychain is alive, perform test setup
        do {
            self.manateeKeySet = try self.makeFakeKeyHierarchy(zoneID: CKRecordZone.ID(zoneName: "Manatee"))
        } catch {
            XCTFail("Creation of fake key hierarchies failed: \(error)")
        }
    }

    override func tearDown() {
        // Put teardown code here. This method is called after the invocation of each test method in the class.
        self.cuttlefish = nil
        self.manateeKeySet = nil

        autoreleasepool {
            if let nskeychainDir: NSURL = SecCopyHomeURL(), let keychainDir: URL = nskeychainDir as URL? {
                CKKSAnalytics.logger().removeState()

                SecItemDataSourceFactoryReleaseAll()
                SecKeychainDbForceClose()
                SecKeychainDbReset(nil)

                // Only perform the destructive step if the url matches what we expect!
                let testName = self.name.components(separatedBy: CharacterSet(charactersIn: " ]"))[1]
                if keychainDir.path.hasPrefix("/tmp/" + testName) {
                    do {
                        try FileManager.default.removeItem(at: keychainDir)
                    } catch {
                        print("Failed to remove keychain directory: \(error)")
                    }
                }
            }
        }
        super.tearDown()
    }

    override static func tearDown() {
        KCSharingClearChangeTrackingEnabledOverride()

#if SEC_XR
        TPClearBecomeiPadOverride()
#endif
    }

    func makeFakeKeyHierarchy(zoneID: CKRecordZone.ID) throws -> CKKSKeychainBackedKeySet {
        return try autoreleasepool {
            // Remember, these keys come into TPH having round-tripped through an NSEncoding
            let tlk = try CKKSKeychainBackedKey.randomKeyWrapped(bySelf: zoneID)
            let classA = try CKKSKeychainBackedKey.randomKeyWrapped(byParent: tlk, keyclass: SecCKKSKeyClassA)
            let classC = try CKKSKeychainBackedKey.randomKeyWrapped(byParent: tlk, keyclass: SecCKKSKeyClassC)

            XCTAssertNoThrow(try tlk.saveMaterialToKeychain(), "Should be able to save TLK to keychain")
            XCTAssertNoThrow(try classA.saveMaterialToKeychain(), "Should be able to save classA key to keychain")
            XCTAssertNoThrow(try classC.saveMaterialToKeychain(), "Should be able to save classC key to keychain")

            let tlkData = try NSKeyedArchiver.archivedData(withRootObject: tlk, requiringSecureCoding: true)
            let classAData = try NSKeyedArchiver.archivedData(withRootObject: classA, requiringSecureCoding: true)
            let classCData = try NSKeyedArchiver.archivedData(withRootObject: classC, requiringSecureCoding: true)

            let decodedTLK = try NSKeyedUnarchiver.unarchivedObject(ofClasses: [CKKSKeychainBackedKey.classForKeyedUnarchiver()], from: tlkData) as! CKKSKeychainBackedKey
            let decodedClassA = try NSKeyedUnarchiver.unarchivedObject(ofClasses: [CKKSKeychainBackedKey.classForKeyedUnarchiver()], from: classAData) as! CKKSKeychainBackedKey
            let decodedClassC = try NSKeyedUnarchiver.unarchivedObject(ofClasses: [CKKSKeychainBackedKey.classForKeyedUnarchiver()], from: classCData) as! CKKSKeychainBackedKey

            return CKKSKeychainBackedKeySet(tlk: decodedTLK, classA: decodedClassA, classC: decodedClassC, newUpload: false)
        }
    }

    func assertTLKShareFor(peerID: String, keyUUID: String, zoneID: CKRecordZone.ID) {
        let matches = self.cuttlefish.state.tlkShares[zoneID]?.filter { tlkShare in
            tlkShare.receiver == peerID &&
                tlkShare.keyUuid == keyUUID
        }

        XCTAssertEqual(matches?.count ?? 0, 1, "Should have one tlk share matching \(peerID) and \(keyUUID)")
    }

    func assertNoTLKShareFor(peerID: String, keyUUID: String, zoneID: CKRecordZone.ID) {
        let matches = self.cuttlefish.state.tlkShares[zoneID]?.filter { tlkShare in
            tlkShare.receiver == peerID &&
                tlkShare.keyUuid == keyUUID
        }

        XCTAssertEqual(matches?.count ?? 0, 0, "Should have no tlk share matching \(peerID) and \(keyUUID)")
    }

    func assertTrusts(context: Container, peerIDs: [String]) {
        let state = context.getStateSync(test: self)
        guard let egoPeerID = state.egoPeerID else {
            XCTFail("context should have an ego peer ID")
            return
        }

        guard let dynamicInfo = state.peers[egoPeerID]?.dynamicInfo else {
            XCTFail("No dynamicInfo for ego peer")
            return
        }

        _ = peerIDs.map {
            XCTAssertTrue(dynamicInfo.includedPeerIDs.contains($0), "Peer should trust \($0)")
            XCTAssertFalse(dynamicInfo.excludedPeerIDs.contains($0), "Peer should not distrust \($0)")
        }
    }

    func assertDistrusts(context: Container, peerIDs: [String]) {
        let state = context.getStateSync(test: self)
        guard let egoPeerID = state.egoPeerID else {
            XCTFail("context should have an ego peer ID")
            return
        }

        guard let dynamicInfo = state.peers[egoPeerID]?.dynamicInfo else {
            XCTFail("No dynamicInfo for ego peer")
            return
        }

        _ = peerIDs.map {
            XCTAssertFalse(dynamicInfo.includedPeerIDs.contains($0), "Peer should not trust \($0)")
            XCTAssertTrue(dynamicInfo.excludedPeerIDs.contains($0), "Peer should distrust \($0)")
        }
    }

    func tmpStoreDescription(name: String) -> NSPersistentStoreDescription {
        let tmpStoreURL = URL(fileURLWithPath: name, relativeTo: tmpURL)
        return NSPersistentStoreDescription(url: tmpStoreURL)
    }

    func establish(reload: Bool,
                   contextID: String = OTDefaultContext,
                   allowedMachineIDs: Set<String> = Set(["aaa", "bbb", "ccc"]),
                   accountIsDemo: Bool = false,
                   modelID: String = "iPhone1,1",
                   syncUserControllableViews: TPPBPeerStableInfoUserControllableViewStatus = .UNKNOWN,
                   store: NSPersistentStoreDescription) throws -> (Container, String) {
        var container = try Container(name: ContainerName(container: "test", context: contextID), persistentStoreDescription: store, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: allowedMachineIDs, userInitiatedRemovals: nil, evictedRemovals: nil, unknownReasonRemovals: nil, accountIsDemo: accountIsDemo, listDifference: !allowedMachineIDs.isEmpty), "should be able to set allowed machine IDs")

        let (peerID, permanentInfo, permanentInfoSig, _, _, _, error) = container.prepareSync(test: self,
                                                                                              epoch: 1,
                                                                                              machineID: "aaa",
                                                                                              bottleSalt: "123456789",
                                                                                              bottleID: UUID().uuidString,
                                                                                              modelID: modelID,
                                                                                              syncUserControllableViews: syncUserControllableViews)

        do {
            let state = container.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == peerID }, "should have a bottle for peer")
            let secret = container.loadSecretSync(test: self, label: peerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNotNil(peerID)
        XCTAssertNotNil(permanentInfo)
        XCTAssertNotNil(permanentInfoSig)
        XCTAssertNil(error)

        _ = container.dumpSync(test: self)

        if reload {
            do {
                container = try Container(name: ContainerName(container: "test", context: contextID), persistentStoreDescription: store, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
            } catch {
                XCTFail("Creating container errored: \(error)")
            }
        }

        let cliqueChangedNotificationExpectation = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: OTCliqueChanged))

        let (peerID2, _, _, error2) = container.establishSync(test: self, ckksKeys: [self.manateeKeySet], tlkShares: [], preapprovedKeys: [])
        XCTAssertNil(error2)
        XCTAssertNotNil(peerID2)

        self.wait(for: [cliqueChangedNotificationExpectation], timeout: 1)

        _ = container.dumpSync(test: self)

        return (container, peerID!)
    }

    func testEstablishWithReload() throws {
        let description = tmpStoreDescription(name: "container.db")
        let (container, peerID) = try establish(reload: true, store: description)

        assertTLKShareFor(peerID: peerID, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

        // With no other input, the syncing policy should say to sync user views
        let (policy, _, policyError) = container.fetchCurrentPolicySync(test: self)
        XCTAssertNil(policyError, "Should be no error fetching aPolicy")
        XCTAssertNotNil(policy, "Should have a syncing policy")
        XCTAssertEqual(policy?.syncUserControllableViews, .DISABLED, "Peer should not desire to sync user controllable views (as the client didn't have any input)")
    }

    func testEstablishNoReload() throws {
        let description = tmpStoreDescription(name: "container.db")
        let (container, peerID) = try establish(reload: false, store: description)

        assertTLKShareFor(peerID: peerID, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

        // With no other input, the syncing policy should say to sync user views
        let (policy, _, policyError) = container.fetchCurrentPolicySync(test: self)
        XCTAssertNil(policyError, "Should be no error fetching aPolicy")
        XCTAssertNotNil(policy, "Should have a syncing policy")
        XCTAssertEqual(policy?.syncUserControllableViews, .DISABLED, "Peer should not desire to sync user controllable views (as the client didn't have any input)")
    }

    func testEstablishWithUserSyncableViews() throws {
        let description = tmpStoreDescription(name: "container.db")

        let (container, peerID) = try self.establish(reload: false,
                                                     contextID: OTDefaultContext,
                                                     accountIsDemo: false,
                                                     syncUserControllableViews: .ENABLED,
                                                     store: description)

        assertTLKShareFor(peerID: peerID, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

        // The syncing policy should say not to sync user views
        let (policy, _, policyError) = container.fetchCurrentPolicySync(test: self)
        XCTAssertNil(policyError, "Should be no error fetching aPolicy")
        XCTAssertNotNil(policy, "Should have a syncing policy")

        XCTAssertEqual(policy?.syncUserControllableViews, .ENABLED, "Peer should desire to sync user controllable views (per request)")
    }

    func testEstablishWithoutUserSyncableViews() throws {
        let description = tmpStoreDescription(name: "container.db")

        let (container, peerID) = try self.establish(reload: false,
                                                     contextID: OTDefaultContext,
                                                     accountIsDemo: false,
                                                     syncUserControllableViews: .DISABLED,
                                                     store: description)

        assertTLKShareFor(peerID: peerID, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

        // The syncing policy should say not to sync user views
        let (policy, _, policyError) = container.fetchCurrentPolicySync(test: self)
        XCTAssertNil(policyError, "Should be no error fetching aPolicy")
        XCTAssertNotNil(policy, "Should have a syncing policy")

        XCTAssertEqual(policy?.syncUserControllableViews, .DISABLED, "Peer should not desire to sync user controllable views (per request)")
    }

    func testEstablishWithoutUserSyncableViewsOnWatch() throws {
        let description = tmpStoreDescription(name: "container.db")

        // Watches will listen to the input here. If we set FOLLOWING, it should remain FOLLOWING (as some watches don't have UI to change this value)
        let (container, peerID) = try self.establish(reload: false,
                                                     contextID: OTDefaultContext,
                                                     accountIsDemo: false,
                                                     modelID: "Watch1,1",
                                                     syncUserControllableViews: .FOLLOWING,
                                                     store: description)

        assertTLKShareFor(peerID: peerID, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

        // The syncing policy should say not to sync user views
        let (policy, _, policyError) = container.fetchCurrentPolicySync(test: self)
        XCTAssertNil(policyError, "Should be no error fetching aPolicy")
        XCTAssertNotNil(policy, "Should have a syncing policy")

        XCTAssertEqual(policy?.syncUserControllableViews, .FOLLOWING, "Peer should desire to sync user controllable views (ignoring the request)")
    }

    func testEstablishNotOnAllowListErrors() throws {
        let description = tmpStoreDescription(name: "container.db")
        let container = try Container(name: ContainerName(container: "test", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let (peerID, permanentInfo, permanentInfoSig, _, _, _, error) = container.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = container.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == peerID }, "should have a bottle for peer")
            let secret = container.loadSecretSync(test: self, label: peerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNotNil(peerID)
        XCTAssertNotNil(permanentInfo)
        XCTAssertNotNil(permanentInfoSig)
        XCTAssertNil(error)

        // Note that an empty machine ID list means "all are allowed", so an establish now will succeed

        // Now set up a machine ID list that positively does not have our peer
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa"], userInitiatedRemovals: nil, evictedRemovals: nil, unknownReasonRemovals: nil, accountIsDemo: false), "should be able to set allowed machine IDs")

        let (peerID3, _, _, error3) = container.establishSync(test: self, ckksKeys: [], tlkShares: [], preapprovedKeys: [])
        XCTAssertNotNil(peerID3, "Should get a peer when you establish a now allow-listed peer")
        XCTAssertNil(error3, "Should not get an error when you establish a now allow-listed peer")
    }

    func joinByVoucher(sponsor: Container,
                       containerID: String,
                       machineID: String,
                       machineIDs: Set<String>,
                       accountIsDemo: Bool,
                       store: NSPersistentStoreDescription) throws -> (Container, String) {
        let c = try Container(name: ContainerName(container: containerID, context: OTDefaultContext),
                              persistentStoreDescription: store,
                              darwinNotifier: FakeCKKSNotifier.self,
                              managedConfigurationAdapter: mcAdapterPlaceholder,
                              cuttlefish: cuttlefish)

        XCTAssertNil(c.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: accountIsDemo, listDifference: !machineIDs.isEmpty), "Should be able to set machine IDs")

        print("preparing \(containerID)")
        let (peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, _, error) =
            c.prepareSync(test: self, epoch: 1, machineID: machineID, bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        XCTAssertNil(error)
        XCTAssertNotNil(peerID)
        XCTAssertNotNil(permanentInfo)
        XCTAssertNotNil(permanentInfoSig)
        XCTAssertNotNil(stableInfo)
        XCTAssertNotNil(stableInfoSig)

        do {
            assertNoTLKShareFor(peerID: peerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("\(sponsor) vouches for \(containerID)")
            let (voucherData, voucherSig, vouchError) =
                sponsor.vouchSync(test: self,
                                  peerID: peerID!,
                                  permanentInfo: permanentInfo!,
                                  permanentInfoSig: permanentInfoSig!,
                                  stableInfo: stableInfo!,
                                  stableInfoSig: stableInfoSig!,
                                  ckksKeys: [self.manateeKeySet],
                                  altDSID: nil,
                                  flowID: nil,
                                  deviceSessionID: nil,
                                  canSendMetrics: false)
            XCTAssertNil(vouchError)
            XCTAssertNotNil(voucherData)
            XCTAssertNotNil(voucherSig)

            // As part of the join, the sponsor should have uploaded a tlk share
            assertTLKShareFor(peerID: peerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            let cliqueChangedNotificationExpectation = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: OTCliqueChanged))

            print("\(containerID) joins")
            let (joinedPeerID, _, _, joinError) = c.joinSync(test: self,
                                                             voucherData: voucherData!,
                                                             voucherSig: voucherSig!,
                                                             ckksKeys: [],
                                                             tlkShares: [],
                                                             altDSID: nil,
                                                             flowID: nil,
                                                             deviceSessionID: nil,
                                                             canSendMetrics: false)
            XCTAssertNil(joinError)
            XCTAssertEqual(joinedPeerID, peerID!)

            self.wait(for: [cliqueChangedNotificationExpectation], timeout: 1)
        }

        return (c, peerID!)
    }

    func testJoin() throws {
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        let containerC = try Container(name: ContainerName(container: "c", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa", "bbb", "ccc"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))
        XCTAssertNil(containerC.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, aPolicy, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        XCTAssertNotNil(aPolicy, "Should have a syncing policy coming back from a successful prepare")
        XCTAssertEqual(aPolicy?.version, prevailingPolicyVersion, "Policy coming back from prepare() should be prevailing policy version")
        XCTAssertEqual(aPolicy?.syncUserControllableViews, .UNKNOWN, "Policy coming back from prepare() should not have an opinion on views")

        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error)
        XCTAssertNotNil(aPeerID)
        XCTAssertNotNil(aPermanentInfo)
        XCTAssertNotNil(aPermanentInfoSig)

        print("establishing A")
        do {
            let (peerID, _, _, error) = containerA.establishSync(test: self, ckksKeys: [], tlkShares: [], preapprovedKeys: [])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
        }

        do {
            // With no other input, the syncing policy should say to sync user views
            let (aPolicy, _, aPolicyError) = containerA.fetchCurrentPolicySync(test: self)
            XCTAssertNil(aPolicyError, "Should be no error fetching aPolicy")
            XCTAssertNotNil(aPolicy, "Should have a syncing policy")
            XCTAssertEqual(aPolicy?.syncUserControllableViews, .DISABLED, "Peer should desire to not sync user controllable views (as the client didn't have any input)")
        }

        print("preparing B")
        let (bPeerID, bPermanentInfo, bPermanentInfoSig, bStableInfo, bStableInfoSig, _, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == bPeerID }, "should have a bottle for peer")
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error2)
        XCTAssertNotNil(bPeerID)
        XCTAssertNotNil(bPermanentInfo)
        XCTAssertNotNil(bPermanentInfoSig)

        do {
            assertNoTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
            print("A vouches for B, but doesn't provide any TLKShares")
            let (_, _, errorVouchingWithoutTLKs) =
                containerA.vouchSync(test: self,
                                     peerID: bPeerID!,
                                     permanentInfo: bPermanentInfo!,
                                     permanentInfoSig: bPermanentInfoSig!,
                                     stableInfo: bStableInfo!,
                                     stableInfoSig: bStableInfoSig!,
                                     ckksKeys: [],
                                     altDSID: nil,
                                     flowID: nil,
                                     deviceSessionID: nil,
                                     canSendMetrics: false)
            XCTAssertNil(errorVouchingWithoutTLKs, "Should be no error vouching without uploading TLKShares")
            assertNoTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("A vouches for B, but doesn't only has provisional TLKs at the time")
            let provisionalManateeKeySet = try self.makeFakeKeyHierarchy(zoneID: CKRecordZone.ID(zoneName: "Manatee"))
            provisionalManateeKeySet.newUpload = true

            let (_, _, errorVouchingWithProvisionalTLKs) =
                containerA.vouchSync(test: self,
                                     peerID: bPeerID!,
                                     permanentInfo: bPermanentInfo!,
                                     permanentInfoSig: bPermanentInfoSig!,
                                     stableInfo: bStableInfo!,
                                     stableInfoSig: bStableInfoSig!,
                                     ckksKeys: [provisionalManateeKeySet],
                                     altDSID: nil,
                                     flowID: nil,
                                     deviceSessionID: nil,
                                     canSendMetrics: false)
            XCTAssertNil(errorVouchingWithProvisionalTLKs, "Should be no error vouching without uploading TLKShares for a non-existent key")
            assertNoTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("A vouches for B")
            let (voucherData, voucherSig, error3) =
                containerA.vouchSync(test: self,
                                     peerID: bPeerID!,
                                     permanentInfo: bPermanentInfo!,
                                     permanentInfoSig: bPermanentInfoSig!,
                                     stableInfo: bStableInfo!,
                                     stableInfoSig: bStableInfoSig!,
                                     ckksKeys: [self.manateeKeySet],
                                     altDSID: nil,
                                     flowID: nil,
                                     deviceSessionID: nil,
                                     canSendMetrics: false)
            XCTAssertNil(error3)
            XCTAssertNotNil(voucherData)
            XCTAssertNotNil(voucherSig)

            // As part of the vouch, A should have uploaded a tlkshare for B
            assertTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("B joins")
            let (peerID, _, bPolicy, error) = containerB.joinSync(test: self,
                                                                  voucherData: voucherData!,
                                                                  voucherSig: voucherSig!,
                                                                  ckksKeys: [],
                                                                  tlkShares: [],
                                                                  altDSID: nil,
                                                                  flowID: nil,
                                                                  deviceSessionID: nil,
                                                                  canSendMetrics: false)
            XCTAssertNil(error)
            XCTAssertEqual(peerID, bPeerID!)

            XCTAssertNotNil(bPolicy, "Should have a syncing policy")
            XCTAssertEqual(bPolicy?.syncUserControllableViews, .DISABLED, "Peer should desire to not sync user controllable views (following A's lead)")
        }

        _ = containerA.dumpSync(test: self)
        _ = containerB.dumpSync(test: self)
        _ = containerC.dumpSync(test: self)

        print("preparing C")
        let (cPeerID, cPermanentInfo, cPermanentInfoSig, cStableInfo, cStableInfoSig, _, error4) =
            containerC.prepareSync(test: self, epoch: 1, machineID: "ccc", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerC.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == cPeerID }, "should have a bottle for peer")
            let secret = containerC.loadSecretSync(test: self, label: cPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error4)
        XCTAssertNotNil(cPeerID)
        XCTAssertNotNil(cPermanentInfo)
        XCTAssertNotNil(cPermanentInfoSig)

        do {
            // C, when it joins, will create a new CKKS zone. It should also upload TLKShares for A and B.
            let provisionalEngramKeySet = try self.makeFakeKeyHierarchy(zoneID: CKRecordZone.ID(zoneName: "Engram"))
            provisionalEngramKeySet.newUpload = true

            assertNoTLKShareFor(peerID: cPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
            assertNoTLKShareFor(peerID: cPeerID!, keyUUID: provisionalEngramKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Engram"))

            print("B vouches for C")
            let (voucherData, voucherSig, error) =
                containerB.vouchSync(test: self,
                                     peerID: cPeerID!,
                                     permanentInfo: cPermanentInfo!,
                                     permanentInfoSig: cPermanentInfoSig!,
                                     stableInfo: cStableInfo!,
                                     stableInfoSig: cStableInfoSig!,
                                     ckksKeys: [self.manateeKeySet],
                                     altDSID: nil,
                                     flowID: nil,
                                     deviceSessionID: nil,
                                     canSendMetrics: false)
            XCTAssertNil(error)
            XCTAssertNotNil(voucherData)
            XCTAssertNotNil(voucherSig)

            assertTLKShareFor(peerID: cPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("C joins")
            let (peerID, _, cPolicy, error2) = containerC.joinSync(test: self,
                                                                   voucherData: voucherData!,
                                                                   voucherSig: voucherSig!,
                                                                   ckksKeys: [self.manateeKeySet, provisionalEngramKeySet],
                                                                   tlkShares: [],
                                                                   altDSID: nil,
                                                                   flowID: nil,
                                                                   deviceSessionID: nil,
                                                                   canSendMetrics: false)
            XCTAssertNil(error2)
            XCTAssertEqual(peerID, cPeerID!)

            XCTAssertNotNil(cPolicy, "Should have a syncing policy")
            XCTAssertEqual(cPolicy?.syncUserControllableViews, .DISABLED, "Peer should desire to not sync user controllable views (following A and B's lead)")

            assertTLKShareFor(peerID: cPeerID!, keyUUID: provisionalEngramKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Engram"))
            assertTLKShareFor(peerID: aPeerID!, keyUUID: provisionalEngramKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Engram"))
            assertTLKShareFor(peerID: bPeerID!, keyUUID: provisionalEngramKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Engram"))
        }

        print("A updates")
        do {
            let cliqueChangedNotificationExpectation = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: OTCliqueChanged))

            let (_, _, error) = containerA.updateSync(test: self)
            XCTAssertNil(error)

            self.wait(for: [cliqueChangedNotificationExpectation], timeout: 1)
        }

        do {
            let state = containerA.getStateSync(test: self)
            let a = state.peers[aPeerID!]!
            XCTAssertTrue(a.dynamicInfo!.includedPeerIDs.contains(cPeerID!))
        }

        _ = containerA.dumpSync(test: self)
        _ = containerB.dumpSync(test: self)
        _ = containerC.dumpSync(test: self)
    }

    func testJoinWithEnabledUserControllableViews() throws {
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa", "bbb", "ccc"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, aPolicy, error) =
            containerA.prepareSync(test: self,
                                   epoch: 1,
                                   machineID: "aaa",
                                   bottleSalt: "123456789",
                                   bottleID: UUID().uuidString,
                                   modelID: "iPhone1,1",
                                   syncUserControllableViews: .ENABLED)
        XCTAssertNotNil(aPolicy, "Should have a syncing policy coming back from a successful prepare")
        XCTAssertEqual(aPolicy?.version, prevailingPolicyVersion, "Policy coming back from prepare() should be prevailing policy version")
        XCTAssertEqual(aPolicy?.syncUserControllableViews, .ENABLED, "Policy coming back from prepare() should already have an opinion of user view syncing")

        XCTAssertNil(error)
        XCTAssertNotNil(aPeerID)
        XCTAssertNotNil(aPermanentInfo)
        XCTAssertNotNil(aPermanentInfoSig)

        print("establishing A")
        do {
            let (peerID, _, _, error) = containerA.establishSync(test: self, ckksKeys: [], tlkShares: [], preapprovedKeys: [])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
        }

        do {
            let (aPolicy, _, aPolicyError) = containerA.fetchCurrentPolicySync(test: self)
            XCTAssertNil(aPolicyError, "Should be no error fetching aPolicy")
            XCTAssertNotNil(aPolicy, "Should have a syncing policy")
            XCTAssertEqual(aPolicy?.syncUserControllableViews, .ENABLED, "Peer should desire to sync user controllable views (as per request)")
        }

        print("preparing B")
        let (bPeerID, bPermanentInfo, bPermanentInfoSig, bStableInfo, bStableInfoSig, _, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == bPeerID }, "should have a bottle for peer")
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error2)
        XCTAssertNotNil(bPeerID)
        XCTAssertNotNil(bPermanentInfo)
        XCTAssertNotNil(bPermanentInfoSig)

        do {
            assertNoTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
            print("A vouches for B, but doesn't provide any TLKShares")
            let (_, _, errorVouchingWithoutTLKs) =
                containerA.vouchSync(test: self,
                                     peerID: bPeerID!,
                                     permanentInfo: bPermanentInfo!,
                                     permanentInfoSig: bPermanentInfoSig!,
                                     stableInfo: bStableInfo!,
                                     stableInfoSig: bStableInfoSig!,
                                     ckksKeys: [],
                                     altDSID: nil,
                                     flowID: nil,
                                     deviceSessionID: nil,
                                     canSendMetrics: false)
            XCTAssertNil(errorVouchingWithoutTLKs, "Should be no error vouching without uploading TLKShares")
            assertNoTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("A vouches for B, but doesn't only has provisional TLKs at the time")
            let provisionalManateeKeySet = try self.makeFakeKeyHierarchy(zoneID: CKRecordZone.ID(zoneName: "Manatee"))
            provisionalManateeKeySet.newUpload = true

            let (_, _, errorVouchingWithProvisionalTLKs) =
                containerA.vouchSync(test: self,
                                     peerID: bPeerID!,
                                     permanentInfo: bPermanentInfo!,
                                     permanentInfoSig: bPermanentInfoSig!,
                                     stableInfo: bStableInfo!,
                                     stableInfoSig: bStableInfoSig!,
                                     ckksKeys: [provisionalManateeKeySet],
                                     altDSID: nil,
                                     flowID: nil,
                                     deviceSessionID: nil,
                                     canSendMetrics: false)
            XCTAssertNil(errorVouchingWithProvisionalTLKs, "Should be no error vouching without uploading TLKShares for a non-existent key")
            assertNoTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("A vouches for B")
            let (voucherData, voucherSig, error3) =
                containerA.vouchSync(test: self,
                                     peerID: bPeerID!,
                                     permanentInfo: bPermanentInfo!,
                                     permanentInfoSig: bPermanentInfoSig!,
                                     stableInfo: bStableInfo!,
                                     stableInfoSig: bStableInfoSig!,
                                     ckksKeys: [self.manateeKeySet],
                                     altDSID: nil,
                                     flowID: nil,
                                     deviceSessionID: nil,
                                     canSendMetrics: false)
            XCTAssertNil(error3)
            XCTAssertNotNil(voucherData)
            XCTAssertNotNil(voucherSig)

            // As part of the vouch, A should have uploaded a tlkshare for B
            assertTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("B joins")
            let (peerID, _, bPolicy, error) = containerB.joinSync(test: self,
                                                                  voucherData: voucherData!,
                                                                  voucherSig: voucherSig!,
                                                                  ckksKeys: [],
                                                                  tlkShares: [],
                                                                  altDSID: nil,
                                                                  flowID: nil,
                                                                  deviceSessionID: nil,
                                                                  canSendMetrics: false)
            XCTAssertNil(error)
            XCTAssertEqual(peerID, bPeerID!)

            XCTAssertNotNil(bPolicy, "Should have a syncing policy")
            XCTAssertEqual(bPolicy?.syncUserControllableViews, .ENABLED, "Peer should desire to sync user controllable views (following A's lead)")
        }
    }

    func testJoinWithoutAllowListErrors() throws {
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let (peerID, permanentInfo, permanentInfoSig, _, _, _, error) = containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == peerID }, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: peerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error, "Should not have an error after preparing A")
        XCTAssertNotNil(peerID, "Should have a peer ID after preparing A")
        XCTAssertNotNil(permanentInfo, "Should have a permanent info after preparing A")
        XCTAssertNotNil(permanentInfoSig, "Should have a signature after preparing A")

        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa"], accountIsDemo: false), "should be able to set allowed machine IDs")

        let (peerID2, _, _, error2) = containerA.establishSync(test: self, ckksKeys: [], tlkShares: [], preapprovedKeys: [])
        XCTAssertNotNil(peerID2, "Should get a peer when you establish a now allow-listed peer")
        XCTAssertNil(error2, "Should not get an error when you establish a now allow-listed peer")

        print("preparing B")
        let (bPeerID, bPermanentInfo, bPermanentInfoSig, bStableInfo, bStableInfoSig, _, errorPrepareB) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == peerID }, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: peerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(errorPrepareB, "Should not have an error after preparing B")
        XCTAssertNotNil(bPeerID, "Should have a peer ID after preparing B")
        XCTAssertNotNil(bPermanentInfo, "Should have a permanent info after preparing B")
        XCTAssertNotNil(bPermanentInfoSig, "Should have a signature after preparing B")

        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa"], accountIsDemo: false), "should be able to set allowed machine IDs on container B")

        do {
            print("A vouches for B")
            let (voucherData, voucherSig, error3) =
                containerA.vouchSync(test: self,
                                     peerID: bPeerID!,
                                     permanentInfo: bPermanentInfo!,
                                     permanentInfoSig: bPermanentInfoSig!,
                                     stableInfo: bStableInfo!,
                                     stableInfoSig: bStableInfoSig!,
                                     ckksKeys: [],
                                     altDSID: nil,
                                     flowID: nil,
                                     deviceSessionID: nil,
                                     canSendMetrics: false)
            XCTAssertNil(error3, "Should be no error vouching for B")
            XCTAssertNotNil(voucherData, "Should have a voucher from A")
            XCTAssertNotNil(voucherSig, "Should have a signature from A")

            print("B joins")
            let (peerID, _, _, error) = containerB.joinSync(test: self,
                                                            voucherData: voucherData!,
                                                            voucherSig: voucherSig!,
                                                            ckksKeys: [],
                                                            tlkShares: [],
                                                            altDSID: nil,
                                                            flowID: nil,
                                                            deviceSessionID: nil,
                                                            canSendMetrics: false)
            XCTAssertNotNil(error, "Should have an error joining with an unapproved machine ID")
            XCTAssertNil(peerID, "Should not receive a peer ID joining with an unapproved machine ID")
        }
    }

    func testReset() throws {
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, _, error) = containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error)
        XCTAssertNotNil(aPeerID)
        XCTAssertNotNil(aPermanentInfo)
        XCTAssertNotNil(aPermanentInfoSig)

        print("establishing A")
        do {
            assertNoTLKShareFor(peerID: aPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
            let (peerID, _, _, error) = containerA.establishSync(test: self, ckksKeys: [self.manateeKeySet], tlkShares: [], preapprovedKeys: [])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
            assertTLKShareFor(peerID: aPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
        }

        print("reset A")
        do {
            let cliqueChangedNotificationExpectation = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: OTCliqueChanged))

            let error = containerA.resetSync(resetReason: .testGenerated, test: self)
            XCTAssertNil(error)

            self.wait(for: [cliqueChangedNotificationExpectation], timeout: 1)
        }
        do {
            let (dict, error) = containerA.dumpSync(test: self)
            XCTAssertNil(error)
            XCTAssertNotNil(dict)
            let peers: [Any] = dict!["peers"] as! [Any]
            XCTAssertEqual(0, peers.count)
        }
    }

    func testResetLocal() throws {
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))

        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, _, error) = containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error, "Should be no error preparing an identity")
        XCTAssertNotNil(aPeerID, "Should have a peer ID after preparing")
        XCTAssertNotNil(aPermanentInfo, "Should have a permanentInfo after preparing")
        XCTAssertNotNil(aPermanentInfoSig, "Should have a permanentInfoSign after preparing")

        do {
            let (dict, error) = containerA.dumpSync(test: self)
            XCTAssertNil(error, "Should be no error dumping")
            XCTAssertNotNil(dict, "Should receive a dump dictionary")

            let selfInfo: [AnyHashable: Any]? = dict!["self"] as! [AnyHashable: Any]?
            XCTAssertNotNil(selfInfo, "Should have a self dictionary")

            let selfPeer: String? = selfInfo!["peerID"] as! String?
            XCTAssertNotNil(selfPeer, "self peer should be part of the dump")
        }

        do {
            // A local reset, since it doesn't fetch from Cuttlefish, should _not_ generate a change notification
            let cliqueChangedNotificationExpectation = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: OTCliqueChanged))
            cliqueChangedNotificationExpectation.isInverted = true

            let error = containerA.localResetSync(test: self)
            XCTAssertNil(error, "local-reset shouldn't error")
            containerA.moc.performAndWait {
                let peers = containerA.containerMO.peers as! Set<PeerMO>
                XCTAssertEqual(peers.count, 0, "peers should be empty ")
            }

            self.wait(for: [cliqueChangedNotificationExpectation], timeout: 1)
        }
        do {
            let (dict, error) = containerA.dumpSync(test: self)

            XCTAssertNil(error, "Should be no error dumping")
            XCTAssertNotNil(dict, "Should receive a dump dictionary")

            let selfInfo: [AnyHashable: Any]? = dict!["self"] as! [AnyHashable: Any]?
            XCTAssertNotNil(selfInfo, "Should have a self dictionary")

            let selfPeer: String? = selfInfo!["peerID"] as! String?
            XCTAssertNil(selfPeer, "self peer should not be part of the dump")
        }
    }

    func testReplayAttack() throws {
        let description = tmpStoreDescription(name: "container.db")
        var containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        let containerC = try Container(name: ContainerName(container: "c", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), accountIsDemo: false))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), accountIsDemo: false))
        XCTAssertNil(containerC.setAllowedMachineIDsSync(test: self, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), accountIsDemo: false))

        print("preparing")
        let (peerID, _, _, _, _, _, _) = containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == peerID }, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: peerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
        }
        let (bPeerID, bPermanentInfo, bPermanentInfoSig, bStableInfo, bStableInfoSig, _, _) = containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == bPeerID }, "should have a bottle for peer")
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
        }
        let (cPeerID, cPermanentInfo, cPermanentInfoSig, cStableInfo, cStableInfoSig, _, _) = containerC.prepareSync(test: self, epoch: 1, machineID: "ccc", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerC.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == cPeerID }, "should have a bottle for peer")
            let secret = containerC.loadSecretSync(test: self, label: cPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
        }
        print("establishing A")
        _ = containerA.establishSync(test: self, ckksKeys: [], tlkShares: [], preapprovedKeys: [])

        do {
            print("A vouches for B")
            let (voucherData, voucherSig, _) = containerA.vouchSync(test: self,
                                                                    peerID: bPeerID!,
                                                                    permanentInfo: bPermanentInfo!,
                                                                    permanentInfoSig: bPermanentInfoSig!,
                                                                    stableInfo: bStableInfo!,
                                                                    stableInfoSig: bStableInfoSig!,
                                                                    ckksKeys: [],
                                                                    altDSID: nil,
                                                                    flowID: nil,
                                                                    deviceSessionID: nil,
                                                                    canSendMetrics: false)

            print("B joins")
            _ = containerB.joinSync(test: self,
                                    voucherData: voucherData!,
                                    voucherSig: voucherSig!,
                                    ckksKeys: [],
                                    tlkShares: [],
                                    altDSID: nil,
                                    flowID: nil,
                                    deviceSessionID: nil,
                                    canSendMetrics: false)
        }

        print("A updates")
        _ = containerA.updateSync(test: self)
        let earlyClock: TPCounter
        do {
            let state = containerA.getStateSync(test: self)
            let b = state.peers[bPeerID!]!
            earlyClock = b.dynamicInfo!.clock
        }

        // Take a snapshot
        let snapshot = cuttlefish.state

        do {
            print("B vouches for C")
            let (voucherData, voucherSig, _) = containerB.vouchSync(test: self, peerID: cPeerID!,
                                                                    permanentInfo: cPermanentInfo!,
                                                                    permanentInfoSig: cPermanentInfoSig!,
                                                                    stableInfo: cStableInfo!,
                                                                    stableInfoSig: cStableInfoSig!,
                                                                    ckksKeys: [],
                                                                    altDSID: nil,
                                                                    flowID: nil,
                                                                    deviceSessionID: nil,
                                                                    canSendMetrics: false)

            print("C joins")
            _ = containerC.joinSync(test: self,
                                    voucherData: voucherData!,
                                    voucherSig: voucherSig!,
                                    ckksKeys: [],
                                    tlkShares: [],
                                    altDSID: nil,
                                    flowID: nil,
                                    deviceSessionID: nil,
                                    canSendMetrics: false)
        }

        print("B updates")
        _ = containerB.updateSync(test: self)

        print("A updates")
        _ = containerA.updateSync(test: self)
        let lateClock: TPCounter
        do {
            let state = containerA.getStateSync(test: self)
            let b = state.peers[bPeerID!]!
            lateClock = b.dynamicInfo!.clock
            XCTAssertTrue(earlyClock < lateClock)
        }

        print("Reverting cuttlefish to the snapshot")
        cuttlefish.state = snapshot
        cuttlefish.makeSnapshot()

        print("A updates, fetching the old snapshot from cuttlefish")
        _ = containerA.updateSync(test: self)

        print("Reload A. Now we see whether it persisted the replayed snapshot in the previous step.")
        containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        do {
            let state = containerA.getStateSync(test: self)
            let b = state.peers[bPeerID!]!
            XCTAssertEqual(lateClock, b.dynamicInfo!.clock)
        }
    }

    func testFetchPolicyDocuments() throws {
        // 1 is known locally via builtin, 3 is not but is known to cuttlefish

        let missingTuple = TPPolicyVersion(version: 900, hash: "not a hash")

        let currentPolicyOptional = builtInPolicyDocuments.first { $0.version.versionNumber == prevailingPolicyVersion.versionNumber }
        XCTAssertNotNil(currentPolicyOptional, "Should have one current policy")
        let currentPolicy = currentPolicyOptional!

        let newPolicy = currentPolicy.clone(withVersionNumber: currentPolicy.version.versionNumber + 1)
        self.cuttlefish.policyOverlay.append(newPolicy)

        let policy1Tuple = TPPolicyVersion(version: 1, hash: "SHA256:TLXrcQmY4ue3oP5pCX1pwsi9BF8cKfohlJBilCroeBs=")
        let policy1Data = Data(base64Encoded: "CAESDgoGaVBob25lEgRmdWxsEgwKBGlQYWQSBGZ1bGwSCwoDTWFjEgRmdWxsEgwKBGlNYWMSBGZ1bGwSDQoHQXBwbGVUVhICdHYS" +
                                              "DgoFV2F0Y2gSBXdhdGNoGhEKCVBDU0VzY3JvdxIEZnVsbBoXCgRXaUZpEgRmdWxsEgJ0dhIFd2F0Y2gaGQoRU2FmYXJpQ3JlZGl0" +
                                              "Q2FyZHMSBGZ1bGwiDAoEZnVsbBIEZnVsbCIUCgV3YXRjaBIEZnVsbBIFd2F0Y2giDgoCdHYSBGZ1bGwSAnR2")!

        let description = tmpStoreDescription(name: "container.db")
        let container = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        // nothing
        let (response1, error1) = container.fetchPolicyDocumentsSync(test: self, versions: [])
        XCTAssertNil(error1, "No error querying for an empty list")
        XCTAssertEqual(response1, [:], "Received empty dictionary")

        // local stuff
        do {
            let (response2, error2) = container.fetchPolicyDocumentsSync(test: self, versions: Set([policy1Tuple]))
            XCTAssertNil(error2, "No error getting locally known policy document")
            XCTAssertEqual(response2?.count, 1, "Got one response for request for one locally known policy")
            XCTAssert(response2?.keys.contains(policy1Tuple) ?? false, "Should have retrieved request for policy1")
            XCTAssertEqual(response2?[policy1Tuple], policy1Data, "retrieved data matches known data")
        }

        do {
            let knownPolicies = container.model.allRegisteredPolicyVersions()
            XCTAssert(knownPolicies.contains(policy1Tuple), "TPModel should know about policy 1")
            XCTAssertFalse(knownPolicies.contains(newPolicy.version), "TPModel should not know about newPolicy")
        }

        // fetch remote
        do {
            let (response3, error3) = container.fetchPolicyDocumentsSync(test: self, versions: [policy1Tuple, newPolicy.version])
            XCTAssertNil(error3, "No error fetching local + remote policy")
            XCTAssertEqual(response3?.count, 2, "Got two responses for local+remote policy request")

            XCTAssert(response3?.keys.contains(policy1Tuple) ?? false, "Should have retrieved request for policy1")
            XCTAssertEqual(response3?[policy1Tuple], policy1Data, "retrieved data matches known data")

            XCTAssert(response3?.keys.contains(newPolicy.version) ?? false, "Should have retrieved request for newPolicy")
            XCTAssertEqual(response3?[newPolicy.version], newPolicy.protobuf, "retrieved data matches known data")
        }

        // invalid version
        do {
            let (response4, error4) = container.fetchPolicyDocumentsSync(test: self, versions: Set([missingTuple]))
            XCTAssertNil(response4, "No response for wrong [version: hash] combination")
            XCTAssertNotNil(error4, "Expected error fetching invalid policy version")
        }

        // valid + invalid
        do {
            let (response5, error5) = container.fetchPolicyDocumentsSync(test: self, versions: Set([missingTuple,
                                                                                                    policy1Tuple,
                                                                                                    newPolicy.version, ]))
            XCTAssertNil(response5, "No response for valid + unknown [version: hash] combination")
            XCTAssertNotNil(error5, "Expected error fetching valid + invalid policy version")
        }

        do {
            let knownPolicies = container.model.allRegisteredPolicyVersions()
            XCTAssert(knownPolicies.contains(policy1Tuple), "TPModel should know about policy 1")
            XCTAssert(knownPolicies.contains(newPolicy.version), "TPModel should know about newPolicy")
        }

        do {
            let reloadedContainer = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
            let reloadedKnownPolicies = reloadedContainer.model.allRegisteredPolicyVersions()
            XCTAssert(reloadedKnownPolicies.contains(policy1Tuple), "TPModel should know about policy 1 after restart")
            XCTAssert(reloadedKnownPolicies.contains(newPolicy.version), "TPModel should know about newPolicy after a restart")
        }
    }

    func testEscrowKeys() throws {
        XCTAssertThrowsError(try EscrowKeys.retrieveEscrowKeysFromKeychain(label: "hash"), "retrieveEscrowKeysFromKeychain should throw error")
        XCTAssertThrowsError(try EscrowKeys.findEscrowKeysForLabel(label: "hash"), "findEscrowKeysForLabel should throw error")

        let secretString = "i'm a secret!"
        XCTAssertNotNil(secretString, "secretString should not be nil")

        let secretData: Data? = secretString.data(using: .utf8)
        XCTAssertNotNil(secretData, "secretData should not be nil")

        let keys = try EscrowKeys(secret: secretData!, bottleSalt: "123456789")
        XCTAssertNotNil(keys, "keys should not be nil")

        XCTAssertNotNil(keys.secret, "secret should not be nil")
        XCTAssertNotNil(keys.bottleSalt, "bottleSalt should not be nil")
        XCTAssertNotNil(keys.encryptionKey, "encryptionKey should not be nil")
        XCTAssertNotNil(keys.signingKey, "signingKey should not be nil")
        XCTAssertNotNil(keys.symmetricKey, "symmetricKey should not be nil")

        let hash = try EscrowKeys.hashEscrowedSigningPublicKey(keyData: keys.signingKey.publicKey().spki())
        XCTAssertNotNil(hash, "hash should not be nil")

        let result = try EscrowKeys.storeEscrowedSigningKeyPair(keyData: keys.signingKey.keyData, label: "Signing Key")
        XCTAssertTrue(result, "result should be true")

        let escrowKey = try EscrowKeys.retrieveEscrowKeysFromKeychain(label: hash)
        XCTAssertNotNil(escrowKey, "escrowKey should not be nil")

        let (signingKey, encryptionKey, symmetricKey) = try EscrowKeys.findEscrowKeysForLabel(label: hash)
        XCTAssertNotNil(signingKey, "signingKey should not be nil")
        XCTAssertNotNil(encryptionKey, "encryptionKey should not be nil")
        XCTAssertNotNil(symmetricKey, "symmetricKey should not be nil")
    }

    func testEscrowKeyTestVectors() {
        let secretString = "I'm a secretI'm a secretI'm a secretI'm a secretI'm a secretI'm a secret"

        let secret = secretString.data(using: .utf8)

        do {
            let testv1 = try EscrowKeys.generateEscrowKey(keyType: EscrowKeyType.kOTEscrowKeySigning, masterSecret: secret!, bottleSalt: testDSID)
            XCTAssertEqual(testv1, signingKey_384, "signing keys should match")

            let testv2 = try EscrowKeys.generateEscrowKey(keyType: EscrowKeyType.kOTEscrowKeyEncryption, masterSecret: secret!, bottleSalt: testDSID)
            XCTAssertEqual(testv2, encryptionKey_384, "encryption keys should match")

            let testv3 = try EscrowKeys.generateEscrowKey(keyType: EscrowKeyType.kOTEscrowKeySymmetric, masterSecret: secret!, bottleSalt: testDSID)
            XCTAssertEqual(testv3, symmetricKey_384, "symmetric keys should match")

            let newSecretString = "I'm f secretI'm a secretI'm a secretI'm a secretI'm a secretI'm a secret"
            let newSecret = newSecretString.data(using: .utf8)

            let testv4 = try EscrowKeys.generateEscrowKey(keyType: EscrowKeyType.kOTEscrowKeySigning, masterSecret: newSecret!, bottleSalt: testDSID)
            XCTAssertNotEqual(testv4, signingKey_384, "signing keys should not match")

            let testv5 = try EscrowKeys.generateEscrowKey(keyType: EscrowKeyType.kOTEscrowKeyEncryption, masterSecret: newSecret!, bottleSalt: testDSID)
            XCTAssertNotEqual(testv5, encryptionKey_384, "encryption keys should not match")

            let testv6 = try EscrowKeys.generateEscrowKey(keyType: EscrowKeyType.kOTEscrowKeySymmetric, masterSecret: newSecret!, bottleSalt: testDSID)
            XCTAssertNotEqual(testv6, symmetricKey_384, "symmetric keys should not match")
        } catch {
            XCTFail("error testing escrow key test vectors \(error)")
        }
    }

    func testRecoveryKeyTestVectors() {
        let secretString = "I'm a secretI'm a secretI'm a secretI'm a secretI'm a secretI'm a secret"

        let secret = secretString.data(using: .utf8)

        do {
            let testv1 = try RecoveryKeySet.generateRecoveryKey(keyType: RecoveryKeyType.kOTRecoveryKeySigning, masterSecret: secret!, recoverySalt: testDSID)
            XCTAssertEqual(testv1, recovery_signingKey_384, "signing keys should match")

            let testv2 = try RecoveryKeySet.generateRecoveryKey(keyType: RecoveryKeyType.kOTRecoveryKeyEncryption, masterSecret: secret!, recoverySalt: testDSID)
            XCTAssertEqual(testv2, recovery_encryptionKey_384, "encryption keys should match")

            let newSecretString = "I'm f secretI'm a secretI'm a secretI'm a secretI'm a secretI'm a secret"
            let newSecret = newSecretString.data(using: .utf8)

            let testv4 = try RecoveryKeySet.generateRecoveryKey(keyType: RecoveryKeyType.kOTRecoveryKeySigning, masterSecret: newSecret!, recoverySalt: testDSID)
            XCTAssertNotEqual(testv4, recovery_signingKey_384, "signing keys should not match")

            let testv5 = try RecoveryKeySet.generateRecoveryKey(keyType: RecoveryKeyType.kOTRecoveryKeyEncryption, masterSecret: newSecret!, recoverySalt: testDSID)
            XCTAssertNotEqual(testv5, recovery_encryptionKey_384, "encryption keys should not match")
        } catch {
            XCTFail("error testing RecoveryKey test vectors \(error)")
        }
    }

    func testJoiningWithBottle() throws {
        var bottleA: ContainerState.Bottle
        var entropy: Data
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa", "bbb"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, _, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            var state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")

            bottleA = state.bottles.removeFirst()

            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNotNil(aPeerID)
        XCTAssertNotNil(aPermanentInfo)
        XCTAssertNotNil(aPermanentInfoSig)

        print("establishing A")
        do {
            let cliqueChangedNotificationExpectation = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: OTCliqueChanged))

            assertNoTLKShareFor(peerID: aPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
            let (peerID, _, _, error) = containerA.establishSync(test: self, ckksKeys: [self.manateeKeySet], tlkShares: [], preapprovedKeys: [])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
            assertTLKShareFor(peerID: aPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            self.wait(for: [cliqueChangedNotificationExpectation], timeout: 1)
        }
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            entropy = secret!
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }

        _ = containerB.updateSync(test: self)

        print("preparing B")
        let (bPeerID, _, _, _, _, _, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == bPeerID }, "should have a bottle for peer")
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error2)

        do {
            print("B prepares to join via bottle")

            let (bottlePeerID, policy, _, errorPreflight) = containerB.preflightVouchWithBottleSync(test: self, bottleID: bottleA.bottleID!)
            XCTAssertNil(errorPreflight, "Should be no error preflighting a vouch with bottle")
            XCTAssertEqual(bottlePeerID, aPeerID, "Bottle should be for peer A")
            XCTAssertNotNil(policy, "Should have a policy")

            let (voucherData, voucherSig, _, _, error3) = containerB.vouchWithBottleSync(test: self, b: bottleA.bottleID!, entropy: entropy, bottleSalt: "123456789", tlkShares: [])

            XCTAssertNil(error3)
            XCTAssertNotNil(voucherData)
            XCTAssertNotNil(voucherSig)

            // Before B joins, there should be no TLKShares for B
            assertNoTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            let cliqueChangedNotificationExpectation = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: OTCliqueChanged))

            print("B joins")
            let (peerID, _, _, error) = containerB.joinSync(test: self, voucherData: voucherData!, voucherSig: voucherSig!, ckksKeys: [self.manateeKeySet], tlkShares: [], altDSID: nil, flowID: nil, deviceSessionID: nil, canSendMetrics: false)
            XCTAssertNil(error)
            XCTAssertEqual(peerID, bPeerID!)

            self.wait(for: [cliqueChangedNotificationExpectation], timeout: 1)

            // But afterward, it has one!
            assertTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
        }
    }

    func testJoiningWithBottleAndEmptyBottleSalt() throws {
        var bottleA: ContainerState.Bottle
        var entropy: Data
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa", "bbb"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, _, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            var state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")

            bottleA = state.bottles.removeFirst()

            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNotNil(aPeerID)
        XCTAssertNotNil(aPermanentInfo)
        XCTAssertNotNil(aPermanentInfoSig)

        print("establishing A")
        do {
            assertNoTLKShareFor(peerID: aPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
            let (peerID, _, _, error) = containerA.establishSync(test: self, ckksKeys: [self.manateeKeySet], tlkShares: [], preapprovedKeys: [])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
            assertTLKShareFor(peerID: aPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
        }
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            entropy = secret!
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }

        _ = containerB.updateSync(test: self)

        print("preparing B")
        let (bPeerID, _, _, _, _, _, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == bPeerID }, "should have a bottle for peer")
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error2)

        do {
            print("B prepares to join via bottle")

            let (bottlePeerID, policy, _, errorPreflight) = containerB.preflightVouchWithBottleSync(test: self, bottleID: bottleA.bottleID!)
            XCTAssertNil(errorPreflight, "Should be no error preflighting a vouch with bottle")
            XCTAssertEqual(bottlePeerID, aPeerID, "Bottle should be for peer A")
            XCTAssertNotNil(policy, "Should have a policy")

            let (voucherData, voucherSig, _, _, error3) = containerB.vouchWithBottleSync(test: self, b: bottleA.bottleID!, entropy: entropy, bottleSalt: "123456789", tlkShares: [])

            XCTAssertNil(error3)
            XCTAssertNotNil(voucherData)
            XCTAssertNotNil(voucherSig)

            // Before B joins, there should be no TLKShares for B
            assertNoTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("B joins")
            let (peerID, _, _, error) = containerB.joinSync(test: self, voucherData: voucherData!, voucherSig: voucherSig!, ckksKeys: [self.manateeKeySet], tlkShares: [], altDSID: nil, flowID: nil, deviceSessionID: nil, canSendMetrics: false)
            XCTAssertNil(error)
            XCTAssertEqual(peerID, bPeerID!)

            // But afterward, it has one!
            assertTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
        }
    }

    func testJoiningWithWrongEscrowRecordForBottle() throws {
        var entropy: Data
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa", "bbb"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, _, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error)
        XCTAssertNotNil(aPeerID)
        XCTAssertNotNil(aPermanentInfo)
        XCTAssertNotNil(aPermanentInfoSig)

        print("establishing A")
        do {
            let (peerID, _, _, error) = containerA.establishSync(test: self, ckksKeys: [self.manateeKeySet], tlkShares: [], preapprovedKeys: [])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
        }
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            entropy = secret!
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }

        _ = containerB.updateSync(test: self)

        print("preparing B")
        let (bPeerID, _, _, _, _, _, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == bPeerID }, "should have a bottle for peer")
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error2)

        do {
            print("B joins via bottle")

            let (bottlePeerID, policy, _, errorPreflight) = containerB.preflightVouchWithBottleSync(test: self, bottleID: "wrong escrow record")
            XCTAssertNotNil(errorPreflight, "Should be an error preflighting bottle that doesn't exist")
            XCTAssertNil(bottlePeerID, "peerID should be nil for no bottle")
            XCTAssertNil(policy, "Should not have a policy")

            let (voucherData, voucherSig, _, _, error3) = containerB.vouchWithBottleSync(test: self, b: "wrong escrow record", entropy: entropy, bottleSalt: "123456789", tlkShares: [])

            XCTAssertNotNil(error3)
            XCTAssertNil(voucherData)
            XCTAssertNil(voucherSig)
        }
    }

    func testJoiningWithWrongBottle() throws {
        var bottleB: ContainerState.Bottle
        var entropy: Data
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa", "bbb"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, _, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error)
        XCTAssertNotNil(aPeerID)
        XCTAssertNotNil(aPermanentInfo)
        XCTAssertNotNil(aPermanentInfoSig)

        print("establishing A")
        do {
            let (peerID, _, _, error) = containerA.establishSync(test: self, ckksKeys: [], tlkShares: [], preapprovedKeys: [])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
        }
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            entropy = secret!
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }

        print("preparing B")
        let (bPeerID, _, _, _, _, _, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            var state = containerB.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == bPeerID }, "should have a bottle for peer")
            bottleB = state.bottles.removeFirst()
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error2)

        do {
            print("B joins via bottle")

            let (bottlePeerID, policy, _, errorPreflight) = containerB.preflightVouchWithBottleSync(test: self, bottleID: bottleB.bottleID!)
            XCTAssertNotNil(errorPreflight, "Should be an error preflighting bottle that doesn't correspond to a peer")
            XCTAssertNil(bottlePeerID, "Should have no peer for invalid bottle")
            XCTAssertNil(policy, "Should not have a policy")

            let (voucherData, voucherSig, _, _, error3) = containerB.vouchWithBottleSync(test: self, b: bottleB.bottleID!, entropy: entropy, bottleSalt: "123456789", tlkShares: [])

            XCTAssertNotNil(error3)
            XCTAssertNil(voucherData)
            XCTAssertNil(voucherSig)
        }
    }

    func testJoiningWithBottleAndBadSalt() throws {
        var bottleA: ContainerState.Bottle
        var entropy: Data
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa", "bbb"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, _, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            var state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            bottleA = state.bottles.removeFirst()
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error)
        XCTAssertNotNil(aPeerID)
        XCTAssertNotNil(aPermanentInfo)
        XCTAssertNotNil(aPermanentInfoSig)

        print("establishing A")
        do {
            let (peerID, _, _, error) = containerA.establishSync(test: self, ckksKeys: [], tlkShares: [], preapprovedKeys: [])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
        }
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            entropy = secret!
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }

        _ = containerB.updateSync(test: self)

        print("preparing B")
        let (bPeerID, _, _, _, _, _, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == bPeerID }, "should have a bottle for peer")
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error2)

        do {
            print("B joins via bottle")

            let (bottlePeerID, policy, _, errorPreflight) = containerB.preflightVouchWithBottleSync(test: self, bottleID: bottleA.bottleID!)
            XCTAssertNil(errorPreflight, "Should be no error preflighting a vouch with bottle")
            XCTAssertEqual(bottlePeerID, aPeerID, "Bottle should be for peer A")
            XCTAssertNotNil(policy, "Should have a policy")

            let (voucherData, voucherSig, _, _, error3) = containerB.vouchWithBottleSync(test: self, b: bottleA.bottleID!, entropy: entropy, bottleSalt: "987654321", tlkShares: [])

            XCTAssertNotNil(error3)
            XCTAssertNil(voucherData)
            XCTAssertNil(voucherSig)
        }
    }

    func testJoiningWithBottleAndBadSecret() throws {
        var bottleA: ContainerState.Bottle
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa", "bbb"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, _, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            var state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            bottleA = state.bottles.removeFirst()
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error)
        XCTAssertNotNil(aPeerID)
        XCTAssertNotNil(aPermanentInfo)
        XCTAssertNotNil(aPermanentInfoSig)

        print("establishing A")
        do {
            let (peerID, _, _, error) = containerA.establishSync(test: self, ckksKeys: [], tlkShares: [], preapprovedKeys: [])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
        }
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }

        _ = containerB.updateSync(test: self)

        print("preparing B")
        let (bPeerID, _, _, _, _, _, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == bPeerID }, "should have a bottle for peer")
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error2)

        do {
            print("B joins via bottle")

            let (bottlePeerID, policy, _, errorPreflight) = containerB.preflightVouchWithBottleSync(test: self, bottleID: bottleA.bottleID!)
            XCTAssertNil(errorPreflight, "Should be no error preflighting a vouch with bottle")
            XCTAssertEqual(bottlePeerID, aPeerID, "Bottle should be for peer A")
            XCTAssertNotNil(policy, "Should have a policy")

            let (voucherData, voucherSig, _, _, error3) = containerB.vouchWithBottleSync(test: self, b: bottleA.bottleID!, entropy: Data(count: Int(OTMasterSecretLength)), bottleSalt: "123456789", tlkShares: [])

            XCTAssertNotNil(error3)
            XCTAssertNil(voucherData)
            XCTAssertNil(voucherSig)
        }
    }

    func testJoiningWithNoFetchAllBottles() throws {
        var bottleA: ContainerState.Bottle
        var entropy: Data
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa", "bbb"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, _, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            var state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            entropy = secret!
            bottleA = state.bottles.removeFirst()
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error)
        XCTAssertNotNil(aPeerID)
        XCTAssertNotNil(aPermanentInfo)
        XCTAssertNotNil(aPermanentInfoSig)

        print("establishing A")
        do {
            let (peerID, _, _, error) = containerA.establishSync(test: self, ckksKeys: [], tlkShares: [], preapprovedKeys: [])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
        }
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }

        print("preparing B")
        let (bPeerID, _, _, _, _, _, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == bPeerID }, "should have a bottle for peer")
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error2)

        do {
            print("B joins via bottle")

            self.cuttlefish.fetchViableBottlesError.append(FakeCuttlefishServer.makeCloudKitCuttlefishError(code: .changeTokenExpired))

            let (bottlePeerID, policy, _, errorPreflight) = containerB.preflightVouchWithBottleSync(test: self, bottleID: bottleA.bottleID!)
            XCTAssertNotNil(errorPreflight, "Should be an error preflighting a vouch with bottle with a fetch error")
            XCTAssertNil(bottlePeerID, "peerID should be nil")
            XCTAssertNil(policy, "Should not have a policy")

            self.cuttlefish.fetchViableBottlesError.append(FakeCuttlefishServer.makeCloudKitCuttlefishError(code: .changeTokenExpired))

            let (voucherData, voucherSig, _, _, error3) = containerB.vouchWithBottleSync(test: self, b: bottleA.bottleID!, entropy: entropy, bottleSalt: "123456789", tlkShares: [])

            XCTAssertNotNil(error3)
            XCTAssertNil(voucherData)
            XCTAssertNil(voucherSig)
        }
    }

    func testJoinByPreapproval() throws {
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        let containerC = try Container(name: ContainerName(container: "c", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa", "bbb", "ccc"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))
        XCTAssertNil(containerC.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, aPolicy, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        XCTAssertNil(error)
        XCTAssertNotNil(aPeerID)
        XCTAssertNotNil(aPermanentInfo)
        XCTAssertNotNil(aPermanentInfoSig)

        XCTAssertNotNil(aPolicy, "Should have a policy coming back from a successful prepare")
        XCTAssertEqual(aPolicy?.version, prevailingPolicyVersion, "Policy coming back from prepare() should be prevailing policy version")
        XCTAssertEqual(aPolicy?.syncUserControllableViews, .UNKNOWN, "Policy shouldn't yet know whether we want to sync user views")

        print("preparing B")
        let (bPeerID, bPermanentInfo, bPermanentInfoSig, _, _, _, error2) =
            containerB.prepareSync(test: self,
                                   epoch: 1,
                                   machineID: "bbb",
                                   bottleSalt: "123456789",
                                   bottleID: UUID().uuidString,
                                   modelID: "iPhone1,1",
                                   syncUserControllableViews: .DISABLED)
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == bPeerID }, "should have a bottle for peer")
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error2)
        XCTAssertNotNil(bPeerID)
        XCTAssertNotNil(bPermanentInfo)
        XCTAssertNotNil(bPermanentInfoSig)

        print("preparing C")
        let (cPeerID, cPermanentInfo, cPermanentInfoSig, _, _, _, cPrepareError) =
            containerC.prepareSync(test: self,
                                   epoch: 1,
                                   machineID: "ccc",
                                   bottleSalt: "123456789",
                                   bottleID: UUID().uuidString,
                                   modelID: "iPhone1,1",
                                   syncUserControllableViews: .ENABLED)
        do {
            let state = containerC.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == cPeerID }, "should have a bottle for peer")
            let secret = containerC.loadSecretSync(test: self, label: cPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(cPrepareError)
        XCTAssertNotNil(cPeerID)
        XCTAssertNotNil(cPermanentInfo)
        XCTAssertNotNil(cPermanentInfoSig)

        // Now, A establishes preapproving B & C
        // Note: secd is responsible for passing in TLKShares to these preapproved keys in sosTLKShares

        let aPermanentInfoParsed = TPPeerPermanentInfo(peerID: aPeerID!, data: aPermanentInfo!, sig: aPermanentInfoSig!, keyFactory: TPECPublicKeyFactory())
        XCTAssertNotNil(aPermanentInfoParsed, "Should have parsed A's permanent info")

        let bPermanentInfoParsed = TPPeerPermanentInfo(peerID: bPeerID!, data: bPermanentInfo!, sig: bPermanentInfoSig!, keyFactory: TPECPublicKeyFactory())
        XCTAssertNotNil(bPermanentInfoParsed, "Should have parsed B's permanent info")

        let cPermanentInfoParsed = TPPeerPermanentInfo(peerID: cPeerID!, data: cPermanentInfo!, sig: cPermanentInfoSig!, keyFactory: TPECPublicKeyFactory())
        XCTAssertNotNil(cPermanentInfoParsed, "Should have parsed C's permanent info")

        print(bPermanentInfoParsed!.signingPubKey.spki().base64EncodedString())
        print(cPermanentInfoParsed!.signingPubKey.spki().base64EncodedString())

        print("establishing A")
        do {
            let (peerID, _, _, error) = containerA.establishSync(test: self,
                                                                 ckksKeys: [self.manateeKeySet],
                                                                 tlkShares: [],
                                                                 preapprovedKeys: [bPermanentInfoParsed!.signingPubKey.spki(),
                                                                                   cPermanentInfoParsed!.signingPubKey.spki(), ])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)

            let (aPolicy, _, aPolicyError) = containerA.fetchCurrentPolicySync(test: self)
            XCTAssertNil(aPolicyError, "Should be no error fetching aPolicy")
            XCTAssertNotNil(aPolicy, "Should have a syncing policy")
            XCTAssertEqual(aPolicy?.syncUserControllableViews, .DISABLED, "A should desire to not ync user controllable views (as the client didn't have any input)")
        }

        do {
            assertNoTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("B joins by preapproval, and uploads all TLKShares that it has")
            let (bJoinedPeerID, _, bPolicy, bJoinedError) = containerB.preapprovedJoinSync(test: self,
                                                                                          ckksKeys: [self.manateeKeySet],
                                                                                          tlkShares: [],
                                                                                          preapprovedKeys: [aPermanentInfoParsed!.signingPubKey.spki(),
                                                                                                            cPermanentInfoParsed!.signingPubKey.spki(), ])
            XCTAssertNil(bJoinedError, "Should be no error joining by preapproval")
            XCTAssertNotNil(bJoinedPeerID, "Should have a peer ID out of join")
            XCTAssertNotNil(bPolicy, "Should have a policy back from preapprovedjoin")
            XCTAssertEqual(bPolicy?.syncUserControllableViews, .DISABLED, "Policy should say not to sync user controllable views")

            assertTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
        }

        do {
            assertNoTLKShareFor(peerID: cPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("B joins by preapproval, and uploads all TLKShares that it has")
            let (cJoinedPeerID, _, cPolicy, cJoinedError) = containerC.preapprovedJoinSync(test: self,
                                                                                          ckksKeys: [self.manateeKeySet],
                                                                                          tlkShares: [],
                                                                                          preapprovedKeys: [aPermanentInfoParsed!.signingPubKey.spki(),
                                                                                                            bPermanentInfoParsed!.signingPubKey.spki(), ])
            XCTAssertNil(cJoinedError, "Should be no error joining by preapproval")
            XCTAssertNotNil(cJoinedPeerID, "Should have a peer ID out of join")
            XCTAssertEqual(cPeerID, cJoinedPeerID, "PeerID after joining should match")
            XCTAssertNotNil(cPolicy, "Should have a policy back from preapprovedjoin")
            XCTAssertEqual(cPolicy?.syncUserControllableViews, .ENABLED, "Policy should say to sync user controllable views")

            assertTLKShareFor(peerID: cPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
        }

        _ = containerA.dumpSync(test: self)
        _ = containerB.dumpSync(test: self)
        _ = containerC.dumpSync(test: self)
    }

    func testDepartLastTrustedPeer() throws {
        let description = tmpStoreDescription(name: "container.db")
        let (container, peerID) = try establish(reload: false, store: description)

        XCTAssertNotNil(container.departByDistrustingSelfSync(test: self), "Should be an error distrusting self (when it's illegal to do so)")

        // But, having no trusted peers is not a valid state. So, this peer must still trust itself...
        assertTrusts(context: container, peerIDs: [peerID])
    }

    func testDepart() throws {
        let store = tmpStoreDescription(name: "container.db")
        let (c, peerID1) = try establish(reload: false, store: store)

        let (c2, peerID2) = try joinByVoucher(sponsor: c,
                                              containerID: "second",
                                              machineID: "bbb",
                                              machineIDs: ["aaa", "bbb", "ccc"], accountIsDemo: false,
                                              store: store)

        let (_, _, cUpdateError) = c.updateSync(test: self)
        XCTAssertNil(cUpdateError, "Should be able to update first container")
        assertTrusts(context: c, peerIDs: [peerID1, peerID2])

        XCTAssertNil(c.departByDistrustingSelfSync(test: self), "Should be no error distrusting self")
        assertDistrusts(context: c, peerIDs: [peerID1])
        assertTrusts(context: c, peerIDs: [])

        let (_, _, c2UpdateError) = c2.updateSync(test: self)
        XCTAssertNil(c2UpdateError, "Should be able to update second container")
        assertDistrusts(context: c2, peerIDs: [peerID1])
        assertTrusts(context: c2, peerIDs: [peerID2])
    }

    func testDistrustPeers() throws {
        let store = tmpStoreDescription(name: "container.db")
        let (c, peerID1) = try establish(reload: false, store: store)

        let (c2, peerID2) = try joinByVoucher(sponsor: c,
                                              containerID: "second",
                                              machineID: "bbb",
                                              machineIDs: ["aaa", "bbb", "ccc"], accountIsDemo: false,
                                              store: store)

        let (c3, peerID3) = try joinByVoucher(sponsor: c,
                                              containerID: "third",
                                              machineID: "ccc",
                                              machineIDs: ["aaa", "bbb", "ccc"], accountIsDemo: false,
                                              store: store)

        let (_, _, cUpdateError) = c.updateSync(test: self)
        XCTAssertNil(cUpdateError, "Should be able to update first container")
        assertTrusts(context: c, peerIDs: [peerID1, peerID2, peerID3])

        // You can't distrust yourself via peerID.
        XCTAssertNotNil(c.distrustSync(test: self, peerIDs: Set([peerID1, peerID2, peerID3])), "Should error trying to distrust yourself via peer ID")
        assertTrusts(context: c, peerIDs: [peerID1, peerID2, peerID3])

        // Passing in nonsense should error too
        XCTAssertNotNil(c.distrustSync(test: self, peerIDs: Set(["not a real peer ID"])), "Should error when passing in unknown peer IDs")

        // Now, distrust both peers.
        XCTAssertNil(c.distrustSync(test: self, peerIDs: Set([peerID2, peerID3])), "Should be no error distrusting peers")
        assertDistrusts(context: c, peerIDs: [peerID2, peerID3])

        // peers should accept their fates
        let (_, _, c2UpdateError) = c2.updateSync(test: self)
        XCTAssertNil(c2UpdateError, "Should be able to update second container")
        assertDistrusts(context: c2, peerIDs: [peerID2])

        let (_, _, c3UpdateError) = c3.updateSync(test: self)
        XCTAssertNil(c3UpdateError, "Should be able to update third container")
        assertDistrusts(context: c3, peerIDs: [peerID3])
    }

    func testFetchWithBadChangeToken() throws {
        let (c, peerID1) = try establish(reload: false, store: tmpStoreDescription(name: "container.db"))

        // But all that goes away, and a new peer establishes
        self.cuttlefish.state = FakeCuttlefishServer.State()
        let (_, peerID2) = try establish(reload: false, contextID: "second", store: tmpStoreDescription(name: "container-peer2.db"))

        // And the first container fetches again, which should succeed
        self.cuttlefish.nextFetchErrors.append(FakeCuttlefishServer.makeCloudKitCuttlefishError(code: .changeTokenExpired))
        let (_, _, updateError) = c.updateSync(test: self)
        XCTAssertNil(updateError, "Update should have succeeded")

        // and c's model should only include peerID2
        c.moc.performAndWait {
            XCTAssertEqual(c.model.peerCount(), 1, "Model should have one peer")
            XCTAssert(c.model.hasPeer(withID: peerID2), "Model should contain peer 2")
            XCTAssertFalse(c.model.hasPeer(withID: peerID1), "Model should no longer contain peer 1 (ego peer)")
        }
    }

    func testFetchEscrowContents() throws {
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        let (entropyA, bottleIDA, spkiA, errorA) = containerA.fetchEscrowContentsSync(test: self)
        XCTAssertNotNil(errorA, "Should be an error fetching escrow contents")
        XCTAssertEqual(errorA.debugDescription, "Optional(TrustedPeersHelperUnitTests.ContainerError.noPreparedIdentity)", "error should be no prepared identity")
        XCTAssertNil(entropyA, "Should not have some entropy to bottle")
        XCTAssertNil(bottleIDA, "Should not have a bottleID")
        XCTAssertNil(spkiA, "Should not have an SPKI")

        let (c, peerID) = try establish(reload: false, store: description)
        XCTAssertNotNil(peerID, "establish should return a peer id")

        let (entropy, bottleID, spki, error) = c.fetchEscrowContentsSync(test: self)
        XCTAssertNil(error, "Should be no error fetching escrow contents")
        XCTAssertNotNil(entropy, "Should have some entropy to bottle")
        XCTAssertNotNil(bottleID, "Should have a bottleID")
        XCTAssertNotNil(spki, "Should have an SPKI")
    }

    func testBottles() {
        do {
            let peerSigningKey = _SFECKeyPair.init(randomKeyPairWith: _SFECKeySpecifier.init(curve: SFEllipticCurve.nistp384))!
            let peerEncryptionKey = _SFECKeyPair.init(randomKeyPairWith: _SFECKeySpecifier.init(curve: SFEllipticCurve.nistp384))!
            let bottle = try BottledPeer(peerID: "peerID", bottleID: UUID().uuidString, peerSigningKey: peerSigningKey, peerEncryptionKey: peerEncryptionKey, bottleSalt: "123456789")

            let keys = bottle.escrowKeys
            XCTAssertNotNil(keys, "keys should not be nil")

            XCTAssertNotNil(bottle, "bottle should not be nil")

            XCTAssertNotNil(bottle.escrowSigningPublicKeyHash(), "escrowSigningPublicKeyHash should not be nil")

            XCTAssertThrowsError(try BottledPeer.verifyBottleSignature(data: bottle.contents, signature: bottle.signatureUsingPeerKey, pubKey: keys.signingKey.publicKey() as! _SFECPublicKey))
            XCTAssertThrowsError(try BottledPeer.verifyBottleSignature(data: bottle.contents, signature: bottle.signatureUsingEscrowKey, pubKey: peerSigningKey.publicKey() as! _SFECPublicKey))

            XCTAssertNotNil(BottledPeer.signingOperation(), "signing operation should not be nil")

            let verifyBottleEscrowSignature = try BottledPeer.verifyBottleSignature(data: bottle.contents, signature: bottle.signatureUsingEscrowKey, pubKey: keys.signingKey.publicKey() as! _SFECPublicKey)
            XCTAssertNotNil(verifyBottleEscrowSignature, "verifyBottleEscrowSignature should not be nil")

            let verifyBottlePeerSignature = try BottledPeer.verifyBottleSignature(data: bottle.contents, signature: bottle.signatureUsingPeerKey, pubKey: peerSigningKey.publicKey() as! _SFECPublicKey)
            XCTAssertNotNil(verifyBottlePeerSignature, "verifyBottlePeerSignature should not be nil")

            let deserializedBottle = try BottledPeer(contents: bottle.contents, secret: bottle.secret, bottleSalt: "123456789", signatureUsingEscrow: bottle.signatureUsingEscrowKey, signatureUsingPeerKey: bottle.signatureUsingPeerKey)
            XCTAssertNotNil(deserializedBottle, "deserializedBottle should not be nil")

            XCTAssertEqual(deserializedBottle.contents, bottle.contents, "bottle data should be equal")
        } catch {
            XCTFail("error testing bottles \(error)")
        }
    }

    func testFetchBottles() throws {
        let store = tmpStoreDescription(name: "container.db")
        let (c, _) = try establish(reload: false, store: store)

        let (bottles, _, fetchError) = c.fetchViableBottlesSync(test: self)
        XCTAssertNil(fetchError, "should be no error fetching viable bottles")
        XCTAssertNotNil(bottles, "should have fetched some bottles")
        XCTAssertEqual(bottles!.count, 1, "should have fetched one bottle")

        do {
            let state = c.getStateSync(test: self)
            XCTAssertEqual(state.bottles.count, 1, "first container should have a bottle for peer")
            XCTAssertEqual(state.escrowRecords.count, 1, "first container should have an escrow record for peer")
        }

        let c2 = try Container(name: ContainerName(container: "test", context: "newcomer"), persistentStoreDescription: store, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: self.cuttlefish)
        do {
            let state = c2.getStateSync(test: self)
            XCTAssertEqual(state.bottles.count, 0, "before fetch, second container should not have any stored bottles")
            XCTAssertEqual(state.escrowRecords.count, 0, "before fetch, second container should not have any escrow records")
        }

        let (c2bottles, _, c2FetchError) = c2.fetchViableBottlesSync(test: self)
        XCTAssertNil(c2FetchError, "should be no error fetching viable bottles")
        XCTAssertNotNil(c2bottles, "should have fetched some bottles")
        XCTAssertEqual(c2bottles!.count, 1, "should have fetched one bottle")

        do {
            let state = c2.getStateSync(test: self)
            XCTAssertEqual(state.bottles.count, 1, "after fetch, second container should have one stored bottles")
            XCTAssertEqual(state.escrowRecords.count, 1, "after fetch, second container should have one escrow record")
        }
    }

    func testFetchBottlesAfterCacheExpires() throws {
        var bottleA: ContainerState.Bottle
        var entropy: Data
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa", "bbb"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, _, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            var state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")

            bottleA = state.bottles.removeFirst()

            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNotNil(aPeerID)
        XCTAssertNotNil(aPermanentInfo)
        XCTAssertNotNil(aPermanentInfoSig)

        print("establishing A")
        do {
            assertNoTLKShareFor(peerID: aPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
            let (peerID, _, _, error) = containerA.establishSync(test: self, ckksKeys: [self.manateeKeySet], tlkShares: [], preapprovedKeys: [])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
            assertTLKShareFor(peerID: aPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
        }
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            entropy = secret!
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }

        let (bottles, _, fetchError) = containerA.fetchViableBottlesSync(test: self)
        XCTAssertNil(fetchError, "should be no error fetching viable bottles")
        XCTAssertNotNil(bottles, "should have fetched some bottles")
        XCTAssertEqual(bottles!.count, 1, "should have fetched one bottle")

        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertEqual(state.bottles.count, 1, "first container should have a bottle for peer")
            XCTAssertEqual(state.escrowRecords.count, 1, "first container should have an escrow record for peer")
        }

        // have another peer join
        _ = containerB.updateSync(test: self)

        print("preparing B")
        let (bPeerID, _, _, _, _, _, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == bPeerID }, "should have a bottle for peer")
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error2)

        do {
            print("B prepares to join via bottle")

            let (bottlePeerID, policy, _, errorPreflight) = containerB.preflightVouchWithBottleSync(test: self, bottleID: bottleA.bottleID!)
            XCTAssertNil(errorPreflight, "Should be no error preflighting a vouch with bottle")
            XCTAssertEqual(bottlePeerID, aPeerID, "Bottle should be for peer A")
            XCTAssertNotNil(policy, "Should have a policy")

            let (voucherData, voucherSig, _, _, error3) = containerB.vouchWithBottleSync(test: self, b: bottleA.bottleID!, entropy: entropy, bottleSalt: "123456789", tlkShares: [])

            XCTAssertNil(error3)
            XCTAssertNotNil(voucherData)
            XCTAssertNotNil(voucherSig)

            // Before B joins, there should be no TLKShares for B
            assertNoTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("B joins")
            let (peerID, _, _, error) = containerB.joinSync(test: self, voucherData: voucherData!, voucherSig: voucherSig!, ckksKeys: [self.manateeKeySet], tlkShares: [], altDSID: nil, flowID: nil, deviceSessionID: nil, canSendMetrics: false)
            XCTAssertNil(error)
            XCTAssertEqual(peerID, bPeerID!)

            // But afterward, it has one!
            assertTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
        }

        // now fetch bottles and we should get the cached version
        let (_, _, _) = containerA.fetchViableBottlesSync(test: self)
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertEqual(state.bottles.count, 1, "first container should have a bottle for peer")
            XCTAssertEqual(state.escrowRecords.count, 1, "first container should have an escrow record for peer")
        }

        sleep(2)

        // now fetch bottles again after the cache expired
        containerA.escrowCacheTimeout = 2.0

        let (_, _, _) = containerA.fetchViableBottlesSync(test: self)
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertEqual(state.bottles.count, 2, "container A should have 2 bottles")
            XCTAssertEqual(state.escrowRecords.count, 2, "container A should have 2 escrow records")
        }
    }

    func testTrustStatus() throws {
        let store = tmpStoreDescription(name: "container.db")

        let preC = try Container(name: ContainerName(container: "preC", context: "preCContext"),
                                 persistentStoreDescription: store,
                                 darwinNotifier: FakeCKKSNotifier.self,
                                 managedConfigurationAdapter: mcAdapterPlaceholder,
                                 cuttlefish: self.cuttlefish)
        let (preEgoStatus, precStatusError) = preC.trustStatusSync(test: self)
        XCTAssertNil(precStatusError, "No error fetching status")
        XCTAssertEqual(preEgoStatus.egoStatus, .unknown, "Before establish, trust status should be 'unknown'")
        XCTAssertNil(preEgoStatus.egoPeerID, "should not have a peer ID")
        XCTAssertEqual(preEgoStatus.numberOfPeersInOctagon, 0, "should not see number of peers")
        XCTAssertFalse(preEgoStatus.isExcluded, "should be excluded")

        let (c, _) = try establish(reload: false, store: store)
        let (cEgoStatus, cStatusError) = c.trustStatusSync(test: self)
        XCTAssertNil(cStatusError, "Should be no error fetching trust status directly after establish")
        XCTAssertEqual(cEgoStatus.egoStatus, [.fullyReciprocated, .selfTrust], "After establish, should be fully reciprocated")
        XCTAssertNotNil(cEgoStatus.egoPeerID, "should have a peer ID")
        XCTAssertEqual(cEgoStatus.numberOfPeersInOctagon, 1, "should be 1 peer")
        XCTAssertFalse(cEgoStatus.isExcluded, "should not be excluded")

        let c2 = try Container(name: ContainerName(container: "differentContainer", context: "a different context"),
                               persistentStoreDescription: store,
                               darwinNotifier: FakeCKKSNotifier.self,
                               managedConfigurationAdapter: mcAdapterPlaceholder,
                               cuttlefish: self.cuttlefish)

        let (egoStatus, statusError) = c2.trustStatusSync(test: self)
        XCTAssertNil(statusError, "No error fetching status")
        XCTAssertEqual(egoStatus.egoStatus, .excluded, "After establish, other container should be 'excluded'")
        XCTAssertNil(egoStatus.egoPeerID, "should not have a peerID")
        XCTAssertEqual(egoStatus.numberOfPeersInOctagon, 1, "should be 1 peer")
        XCTAssertTrue(egoStatus.isExcluded, "should not be excluded")
    }

    func testTrustStatusWhenMissingIdentityKeys() throws {
        let store = tmpStoreDescription(name: "container.db")
        let (c, _) = try establish(reload: false, store: store)
        let (cEgoStatus, cStatusError) = c.trustStatusSync(test: self)
        XCTAssertNil(cStatusError, "Should be no error fetching trust status directly after establish")
        XCTAssertEqual(cEgoStatus.egoStatus, [.fullyReciprocated, .selfTrust], "After establish, should be fully reciprocated")
        XCTAssertNotNil(cEgoStatus.egoPeerID, "should have a peer ID")
        XCTAssertEqual(cEgoStatus.numberOfPeersInOctagon, 1, "should be 1 peer")
        XCTAssertFalse(cEgoStatus.isExcluded, "should not be excluded")

        let result = try removeEgoKeysSync(peerID: cEgoStatus.egoPeerID!)
        XCTAssertTrue(result, "result should be true")

        let (distrustedStatus, distrustedError) = c.trustStatusSync(test: self)
        XCTAssertNotNil(distrustedError, "error should not be nil")
        XCTAssertEqual(distrustedStatus.egoStatus, [.excluded], "trust status should be excluded")
        XCTAssertTrue(distrustedStatus.isExcluded, "should be excluded")
    }

    func testSetRecoveryKey() throws {
        let store = tmpStoreDescription(name: "container.db")

        let c = try Container(name: ContainerName(container: "c", context: "context"),
                              persistentStoreDescription: store,
                              darwinNotifier: FakeCKKSNotifier.self,
                              managedConfigurationAdapter: mcAdapterPlaceholder,
                              cuttlefish: self.cuttlefish)

        let machineIDs = Set(["aaa", "bbb", "ccc"])
        XCTAssertNil(c.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))

        print("preparing peer A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, _, error) =
            c.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = c.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")
            let secret = c.loadSecretSync(test: self, label: aPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error)
        XCTAssertNotNil(aPeerID)
        XCTAssertNotNil(aPermanentInfo)
        XCTAssertNotNil(aPermanentInfoSig)

        print("establishing A")
        do {
            let (peerID, _, _, error) = c.establishSync(test: self, ckksKeys: [], tlkShares: [], preapprovedKeys: nil)
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
        }
        let recoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        let (_, setRecoveryError) = c.setRecoveryKeySync(test: self, recoveryKey: recoveryKey!, recoverySalt: "altDSID", ckksKeys: [])
        XCTAssertNil(setRecoveryError, "error should be nil")
    }

    func testCreateCustodianRecoveryKey() throws {
        let store = tmpStoreDescription(name: "container.db")

        let c = try Container(name: ContainerName(container: "c", context: "context"),
                              persistentStoreDescription: store,
                              darwinNotifier: FakeCKKSNotifier.self,
                              managedConfigurationAdapter: mcAdapterPlaceholder,
                              cuttlefish: self.cuttlefish)

        let machineIDs = Set(["aaa"])
        XCTAssertNil(c.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))

        print("preparing peer A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, _, error) =
            c.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = c.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")
            let secret = c.loadSecretSync(test: self, label: aPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error)
        XCTAssertNotNil(aPeerID)
        XCTAssertNotNil(aPermanentInfo)
        XCTAssertNotNil(aPermanentInfoSig)

        print("establishing A")
        do {
            let (peerID, _, _, error) = c.establishSync(test: self, ckksKeys: [], tlkShares: [], preapprovedKeys: nil)
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
        }
        let recoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        let limitedPeers = try self.makeFakeKeyHierarchy(zoneID: CKRecordZone.ID(zoneName: "LimitedPeersAllowed"))
        self.cuttlefish.fakeCKZones[CKRecordZone.ID(zoneName: "LimitedPeersAllowed")] = FakeCKZone(zone: CKRecordZone.ID(zoneName: "LimitedPeersAllowed"))
        let passwords = try self.makeFakeKeyHierarchy(zoneID: CKRecordZone.ID(zoneName: "Passwords"))
        self.cuttlefish.fakeCKZones[CKRecordZone.ID(zoneName: "Passwords")] = FakeCKZone(zone: CKRecordZone.ID(zoneName: "Passwords"))
        let ckksKeys = [limitedPeers, passwords]

        let (records, _, createCustodianRecoveryKeyError) = c.createCustodianRecoveryKeySync(test: self,
                                                                                             recoveryString: recoveryKey!,
                                                                                             salt: "altDSID",
                                                                                             ckksKeys: ckksKeys,
                                                                                             kind: TPPBCustodianRecoveryKey_Kind.INHERITANCE_KEY,
                                                                                             uuid: UUID())
        XCTAssertNil(createCustodianRecoveryKeyError, "error should be nil")
        XCTAssertNotNil(records, "records should not be nil")
        XCTAssertEqual(1, records!.count, "expect one record")
        XCTAssertEqual("LimitedPeersAllowed", records![0].recordID.zoneID.zoneName, "expect LimitedPeersAllowed ckks keys")
    }

    func roundTripThroughSetValueTransformer(set: Set<String>) {
        let t = SetValueTransformer()

        let transformedSet = t.transformedValue(set) as? Data
        XCTAssertNotNil(transformedSet, "SVT should return some data")

        let recoveredSet = t.reverseTransformedValue(transformedSet) as? Set<String>
        XCTAssertNotNil(recoveredSet, "SVT should return some recovered set")

        XCTAssertEqual(set, recoveredSet, "Recovered set should be the same as original")
    }

    func testSetValueTransformer() {
        roundTripThroughSetValueTransformer(set: Set<String>([]))
        roundTripThroughSetValueTransformer(set: Set<String>(["asdf"]))
        roundTripThroughSetValueTransformer(set: Set<String>(["asdf", "three", "test"]))
    }

    func testGetRepairSuggestion() throws {
        let store = tmpStoreDescription(name: "container.db")

        let c = try Container(name: ContainerName(container: "c", context: "context"),
                              persistentStoreDescription: store,
                              darwinNotifier: FakeCKKSNotifier.self,
                              managedConfigurationAdapter: mcAdapterPlaceholder,
                              cuttlefish: self.cuttlefish)

        let machineIDs = Set(["aaa", "bbb", "ccc"])
        XCTAssertNil(c.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))

        print("preparing peer A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, _, error) =
            c.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = c.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")
            let secret = c.loadSecretSync(test: self, label: aPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error)
        XCTAssertNotNil(aPeerID)
        XCTAssertNotNil(aPermanentInfo)
        XCTAssertNotNil(aPermanentInfoSig)

        print("establishing A")
        do {
            let (peerID, _, _, error) = c.establishSync(test: self, ckksKeys: [], tlkShares: [], preapprovedKeys: nil)
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
        }
        let (response, healthError) = c.requestHealthCheckSync(requiresEscrowCheck: true, repair: false, test: self)
        XCTAssertFalse(response!.postRepairCFU, "expected postRepairCFU")
        XCTAssertFalse(response!.postEscrowCFU, "expected postEscrowCFU")
        XCTAssertFalse(response!.resetOctagon, "expected resetOctagon")
        XCTAssertFalse(response!.leaveTrust, "expected leaveTrust")
        XCTAssertNil(healthError)
    }

    func testFetchChangesFailDuringVouchWithBottle() throws {
        var bottleA: ContainerState.Bottle
        var entropy: Data
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa", "bbb"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, _, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            var state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")

            bottleA = state.bottles.removeFirst()

            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNotNil(aPeerID)
        XCTAssertNotNil(aPermanentInfo)
        XCTAssertNotNil(aPermanentInfoSig)

        print("establishing A")
        do {
            assertNoTLKShareFor(peerID: aPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
            let (peerID, _, _, error) = containerA.establishSync(test: self, ckksKeys: [self.manateeKeySet], tlkShares: [], preapprovedKeys: [])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
            assertTLKShareFor(peerID: aPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
        }
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            entropy = secret!
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }

        _ = containerB.updateSync(test: self)

        print("preparing B")
        let (bPeerID, _, _, _, _, _, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == bPeerID }, "should have a bottle for peer")
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error2)

        do {
            print("B prepares to join via bottle")

            let (bottlePeerID, policy, _, errorPreflight) = containerB.preflightVouchWithBottleSync(test: self, bottleID: bottleA.bottleID!)
            XCTAssertNil(errorPreflight, "Should be no error preflighting a vouch with bottle")
            XCTAssertEqual(bottlePeerID, aPeerID, "Bottle should be for peer A")
            XCTAssertNotNil(policy, "Should have a policy")

            let (voucherData, voucherSig, _, _, error3) = containerB.vouchWithBottleSync(test: self, b: bottleA.bottleID!, entropy: entropy, bottleSalt: "123456789", tlkShares: [])

            XCTAssertNil(error3)
            XCTAssertNotNil(voucherData)
            XCTAssertNotNil(voucherSig)

            self.cuttlefish.nextFetchErrors.append(FakeCuttlefishServer.makeCloudKitCuttlefishError(code: .changeTokenExpired))

            // Before B joins, there should be no TLKShares for B
            assertNoTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("B joins")
            let (peerID, _, _, error) = containerB.joinSync(test: self, voucherData: voucherData!, voucherSig: voucherSig!, ckksKeys: [self.manateeKeySet], tlkShares: [], altDSID: nil, flowID: nil, deviceSessionID: nil, canSendMetrics: false)
            XCTAssertNil(error, "Should be no error joining with a fetch error")
            XCTAssertNotNil(peerID, "Should have a peer ID")
        }
    }

    func testDistrustedPeerRecoveryKeyNotSet() throws {
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa", "bbb"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))

        print("preparing peer A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, aStableInfo, aStableInfoSig, _, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error)
        XCTAssertNotNil(aPeerID)
        XCTAssertNotNil(aPermanentInfo)
        XCTAssertNotNil(aPermanentInfoSig)
        XCTAssertNotNil(aStableInfo)
        XCTAssertNotNil(aStableInfoSig)

        print("establishing A")
        do {
            let (peerID, _, _, error) = containerA.establishSync(test: self, ckksKeys: [], tlkShares: [], preapprovedKeys: nil)
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
        }

        print("preparing B")
        let (bPeerID, bPermanentInfo, bPermanentInfoSig, bStableInfo, bStableInfoSig, _, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == bPeerID }, "should have a bottle for peer")
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error2)
        XCTAssertNotNil(bPeerID)
        XCTAssertNotNil(bPermanentInfo)
        XCTAssertNotNil(bPermanentInfoSig)

        do {
            assertNoTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
            print("A vouches for B, but doesn't provide any TLKShares")
            let (_, _, errorVouchingWithoutTLKs) =
                containerA.vouchSync(test: self,
                                     peerID: bPeerID!,
                                     permanentInfo: bPermanentInfo!,
                                     permanentInfoSig: bPermanentInfoSig!,
                                     stableInfo: bStableInfo!,
                                     stableInfoSig: bStableInfoSig!,
                                     ckksKeys: [],
                                     altDSID: nil,
                                     flowID: nil,
                                     deviceSessionID: nil,
                                     canSendMetrics: false)
            XCTAssertNil(errorVouchingWithoutTLKs, "Should be no error vouching without uploading TLKShares")
            assertNoTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("A vouches for B, but doesn't only has provisional TLKs at the time")
            let provisionalManateeKeySet = try self.makeFakeKeyHierarchy(zoneID: CKRecordZone.ID(zoneName: "Manatee"))
            provisionalManateeKeySet.newUpload = true

            let (_, _, errorVouchingWithProvisionalTLKs) =
                containerA.vouchSync(test: self,
                                     peerID: bPeerID!,
                                     permanentInfo: bPermanentInfo!,
                                     permanentInfoSig: bPermanentInfoSig!,
                                     stableInfo: bStableInfo!,
                                     stableInfoSig: bStableInfoSig!,
                                     ckksKeys: [provisionalManateeKeySet],
                                     altDSID: nil,
                                     flowID: nil,
                                     deviceSessionID: nil,
                                     canSendMetrics: false)
            XCTAssertNil(errorVouchingWithProvisionalTLKs, "Should be no error vouching without uploading TLKShares for a non-existent key")
            assertNoTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("A vouches for B")
            let (voucherData, voucherSig, error3) =
                containerA.vouchSync(test: self,
                                     peerID: bPeerID!,
                                     permanentInfo: bPermanentInfo!,
                                     permanentInfoSig: bPermanentInfoSig!,
                                     stableInfo: bStableInfo!,
                                     stableInfoSig: bStableInfoSig!,
                                     ckksKeys: [self.manateeKeySet],
                                     altDSID: nil,
                                     flowID: nil,
                                     deviceSessionID: nil,
                                     canSendMetrics: false)
            XCTAssertNil(error3)
            XCTAssertNotNil(voucherData)
            XCTAssertNotNil(voucherSig)

            // As part of the vouch, A should have uploaded a tlkshare for B
            assertTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("B joins")
            let (peerID, _, _, error) = containerB.joinSync(test: self,
                                                            voucherData: voucherData!,
                                                            voucherSig: voucherSig!,
                                                            ckksKeys: [],
                                                            tlkShares: [],
                                                            altDSID: nil,
                                                            flowID: nil,
                                                            deviceSessionID: nil,
                                                            canSendMetrics: false)
            XCTAssertNil(error)
            XCTAssertEqual(peerID, bPeerID!)
        }

        print("A updates")
        do {
            let (_, _, error) = containerA.updateSync(test: self)
            XCTAssertNil(error)
        }
        print("B updates")
        do {
            let (_, _, error) = containerB.updateSync(test: self)
            XCTAssertNil(error)
        }

        // Now, A distrusts B.
        XCTAssertNil(containerA.distrustSync(test: self, peerIDs: Set([bPeerID!])), "Should be no error distrusting peers")
        assertDistrusts(context: containerA, peerIDs: [bPeerID!])

        let recoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        let (_, setRecoveryError) = containerB.setRecoveryKeySync(test: self, recoveryKey: recoveryKey!, recoverySalt: "altDSID", ckksKeys: [])
        XCTAssertNil(setRecoveryError, "error should be nil")

        print("A updates")
        do {
            let (_, _, error) = containerA.updateSync(test: self)
            XCTAssertNil(error)
        }
        print("B updates")
        do {
            let (_, _, error) = containerB.updateSync(test: self)
            XCTAssertNil(error)
        }

        do {
            let (dict, error) = containerA.dumpSync(test: self)

            XCTAssertNil(error, "Should be no error dumping")
            XCTAssertNotNil(dict, "Should receive a dump dictionary")

            let selfInfo: [AnyHashable: Any]? = dict!["self"] as! [AnyHashable: Any]?
            XCTAssertNotNil(selfInfo, "Should have a self dictionary")

            let stableInfo: [AnyHashable: Any]? = selfInfo!["stableInfo"] as! [AnyHashable: Any]?
            XCTAssertNotNil(stableInfo, "stableInfo should not be nil")

            let recoverySigningPublicKey: Data? = stableInfo!["recovery_signing_public_key"] as! Data?
            XCTAssertNil(recoverySigningPublicKey, "recoverySigningPublicKey should be nil")

            let recoveryEncryptionPublicKey: Data? = stableInfo!["recovery_encryption_public_key"] as! Data?
            XCTAssertNil(recoveryEncryptionPublicKey, "recoveryEncryptionPublicKey should be nil")
        }
    }

    func assert(container: Container,
                allowedMachineIDs: Set<String>,
                disallowedMachineIDs: Set<String>,
                unknownMachineIDs: Set<String> = Set(),
                evictedMachineIDs: Set<String> = Set(),
                unknownReasonMachineIDs: Set<String> = Set(),
                ghostedMachineIDs: Set<String> = Set(),
                persistentStore: NSPersistentStoreDescription,
                cuttlefish: FakeCuttlefishServer) throws {
        container.moc.performAndWait {
            let midList = container.onqueueCurrentMIDList()
            XCTAssertEqual(midList.machineIDs(in: .allowed), allowedMachineIDs, "List of allowed machine IDs should match")
            XCTAssertEqual(midList.machineIDs(in: .disallowed), disallowedMachineIDs, "List of disallowed machine IDs should match")
            XCTAssertEqual(midList.machineIDs(in: .unknown), unknownMachineIDs, "List of unknown machine IDs should match")
            XCTAssertEqual(midList.machineIDs(in: .evicted), evictedMachineIDs, "List of evicted machine IDs should match")
            XCTAssertEqual(midList.machineIDs(in: .unknownReason), unknownReasonMachineIDs, "List of unknown reason machine IDs should match")
            XCTAssertEqual(midList.machineIDs(in: .ghostedFromTDL), ghostedMachineIDs, "List of ghosted machine IDs should match")

            let (fetchedAllowList, fetchErr) = container.fetchAllowedMachineIDsSync(test: self)
            XCTAssertNil(fetchErr, "Should be no error fetching the allowed list")
            XCTAssertEqual(fetchedAllowList, allowedMachineIDs, "A fetched list of allowed machine IDs should match the loaded list")
        }

        // if we reload the container, does it still match?
        let reloadedContainer = try Container(name: ContainerName(container: "test", context: OTDefaultContext), persistentStoreDescription: persistentStore, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        reloadedContainer.moc.performAndWait {
            let reloadedMidList = reloadedContainer.onqueueCurrentMIDList()
            XCTAssertEqual(reloadedMidList.machineIDs(in: .allowed), allowedMachineIDs, "List of allowed machine IDs on a reloaded container should match")
            XCTAssertEqual(reloadedMidList.machineIDs(in: .disallowed), disallowedMachineIDs, "List of disallowed machine IDs on a reloaded container should match")
            XCTAssertEqual(reloadedMidList.machineIDs(in: .unknown), unknownMachineIDs, "List of unknown machine IDs on a reloaded container should match")
            XCTAssertEqual(reloadedMidList.machineIDs(in: .evicted), evictedMachineIDs, "List of evicted machine IDs on a reloaded container should match")
            XCTAssertEqual(reloadedMidList.machineIDs(in: .unknownReason), unknownReasonMachineIDs, "List of unknown reason removed machine IDs on a reloaded container should match")
            XCTAssertEqual(reloadedMidList.machineIDs(in: .ghostedFromTDL), ghostedMachineIDs, "List of ghosted machine IDs on a reloaded container should match")
        }
    }

    func testAllowListManipulation() throws {
        let description = tmpStoreDescription(name: "container.db")
        let container = try Container(name: ContainerName(container: "test", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let (peerID, permanentInfo, permanentInfoSig, _, _, _, error) = container.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")

        XCTAssertNil(error)
        XCTAssertNotNil(peerID)
        XCTAssertNotNil(permanentInfo)
        XCTAssertNotNil(permanentInfoSig)

        try self.assert(container: container, allowedMachineIDs: [], disallowedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ccc"], accountIsDemo: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // C falls off the TDL and becomes a ghost
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")

        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb"]), disallowedMachineIDs: Set([]), unknownMachineIDs: Set([]), ghostedMachineIDs: Set(["ccc"]), persistentStore: description, cuttlefish: self.cuttlefish)

        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            let cccMOs = knownMachineMOs.filter { $0.machineID == "ccc" }
            XCTAssertEqual(cccMOs.count, 1, "Should have one machine MO for ccc")

            let cccMO = cccMOs.first!
            XCTAssertEqual(cccMO.status, Int64(TPMachineIDStatus.ghostedFromTDL.rawValue), "Status of ccc MO should be 'ghostedFromTDL'")
            XCTAssertFalse(cccMO.allowed, "allowed should no longer be a used field")

            cccMO.modified = Date(timeIntervalSinceNow: TimeInterval(twoDaysInThePast))
            try! container.moc.save()
        }
        XCTAssertTrue(container.fullIDMSListWouldBeHelpful(), "Container should think it could use an IDMS list set")

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb"]), disallowedMachineIDs: Set(["ccc"]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "zzz", "kkk"], accountIsDemo: false), "should be able to set allowed machine IDs")

        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "zzz", "kkk"]), disallowedMachineIDs: Set(["ccc"]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // Received a new TDL update from IdMS that disallows ccc and bbb becomes a ghost
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "zzz", "kkk"], userInitiatedRemovals: ["ccc"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")

        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "zzz", "kkk"]), disallowedMachineIDs: Set(["ccc"]), unknownMachineIDs: Set([]), ghostedMachineIDs: Set(["bbb"]), persistentStore: description, cuttlefish: self.cuttlefish)

        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            let bbbMOs = knownMachineMOs.filter { $0.machineID == "bbb" }
            XCTAssertEqual(bbbMOs.count, 1, "Should have one machine MO for bbb")

            let bbbMO = bbbMOs.first!
            XCTAssertEqual(bbbMO.status, Int64(TPMachineIDStatus.ghostedFromTDL.rawValue), "Status of bbb MO should be 'ghostedFromTDL'")
            XCTAssertFalse(bbbMO.allowed, "allowed should no longer be a used field")

            bbbMO.modified = Date(timeIntervalSinceNow: TimeInterval(twoDaysInThePast))
            try! container.moc.save()
        }

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "zzz", "kkk"], accountIsDemo: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "zzz", "kkk"]), disallowedMachineIDs: Set(["ccc", "bbb"]), unknownMachineIDs: Set([]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container should NOT think it could use an IDMS list set: there's machine IDs pending removal")

        // once they're unknown, a full list set will make them disallowed
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "zzz", "kkk"], accountIsDemo: false, listDifference: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "zzz", "kkk"]), disallowedMachineIDs: Set(["bbb", "ccc"]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // Resetting the list to what it is doesn't change the list
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "zzz", "kkk"], accountIsDemo: false, listDifference: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "zzz", "kkk"]), disallowedMachineIDs: Set(["bbb", "ccc"]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // But changing it to something completely new should cause a lot of MID ghosts
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["xxx", "mmm"], accountIsDemo: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["mmm", "xxx"]), disallowedMachineIDs: Set(["bbb", "ccc"]), ghostedMachineIDs: Set(["aaa", "kkk", "zzz"]), persistentStore: description, cuttlefish: self.cuttlefish)

        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            let aaaMOs = knownMachineMOs.filter { $0.machineID == "aaa" }
            XCTAssertEqual(aaaMOs.count, 1, "Should have one machine MO for aaa")

            let aaaMO = aaaMOs.first!
            XCTAssertEqual(aaaMO.status, Int64(TPMachineIDStatus.ghostedFromTDL.rawValue), "Status of aaa MO should be 'ghostedFromTDL'")
            XCTAssertFalse(aaaMO.allowed, "allowed should no longer be a used field")

            aaaMO.modified = Date(timeIntervalSinceNow: TimeInterval(twoDaysInThePast))

            let kkkMOs = knownMachineMOs.filter { $0.machineID == "kkk" }
            XCTAssertEqual(kkkMOs.count, 1, "Should have one machine MO for kkk")

            let kkkMO = kkkMOs.first!
            XCTAssertEqual(kkkMO.status, Int64(TPMachineIDStatus.ghostedFromTDL.rawValue), "Status of kkk MO should be 'ghostedFromTDL'")
            XCTAssertFalse(kkkMO.allowed, "allowed should no longer be a used field")

            kkkMO.modified = Date(timeIntervalSinceNow: TimeInterval(twoDaysInThePast))

            let zzzMOs = knownMachineMOs.filter { $0.machineID == "zzz" }
            XCTAssertEqual(kkkMOs.count, 1, "Should have one machine MO for zzz")

            let zzzMO = zzzMOs.first!
            XCTAssertEqual(zzzMO.status, Int64(TPMachineIDStatus.ghostedFromTDL.rawValue), "Status of zzz MO should be 'ghostedFromTDL'")
            XCTAssertFalse(zzzMO.allowed, "allowed should no longer be a used field")

            zzzMO.modified = Date(timeIntervalSinceNow: TimeInterval(twoDaysInThePast))

            try! container.moc.save()
        }
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["xxx", "mmm"], accountIsDemo: false), "should be able to set allowed machine IDs")

        try self.assert(container: container, allowedMachineIDs: Set(["xxx", "mmm"]), disallowedMachineIDs: Set(["aaa", "zzz", "kkk", "bbb", "ccc"]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // And, readding a previously disallowed machine ID works too
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["xxx", "mmm", "aaa"], accountIsDemo: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["xxx", "mmm", "aaa"]), disallowedMachineIDs: Set(["zzz", "kkk", "bbb", "ccc"]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // A update() before establish() doesn't change the list, since it isn't actually changing anything
        let (_, _, updateError) = container.updateSync(test: self)
        XCTAssertNil(updateError, "Should not be an error updating the container without first establishing")
        try self.assert(container: container, allowedMachineIDs: Set(["xxx", "mmm", "aaa"]), disallowedMachineIDs: Set(["zzz", "kkk", "bbb", "ccc"]), persistentStore: description, cuttlefish: self.cuttlefish)

        let (_, _, _, establishError) = container.establishSync(test: self, ckksKeys: [self.manateeKeySet], tlkShares: [], preapprovedKeys: [])
        XCTAssertNil(establishError, "Should be able to establish() with no error")
        try self.assert(container: container, allowedMachineIDs: Set(["xxx", "mmm", "aaa"]), disallowedMachineIDs: Set(["zzz", "kkk", "bbb", "ccc"]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // But a successful update() does remove all disallowed machine IDs, as they're no longer relevant
        let (_, _, updateError2) = container.updateSync(test: self)
        XCTAssertNil(updateError2, "Should not be an error updating the container after establishing")
        try self.assert(container: container, allowedMachineIDs: Set(["xxx", "mmm", "aaa"]), disallowedMachineIDs: Set([]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // X falls off of the list
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["mmm"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["mmm"]), disallowedMachineIDs: Set([]), ghostedMachineIDs: Set(["aaa", "xxx"]), persistentStore: description, cuttlefish: self.cuttlefish)


        // now X is excluded
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["mmm"], userInitiatedRemovals: ["xxx"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["mmm"]), disallowedMachineIDs: Set(["xxx"]), ghostedMachineIDs: Set(["aaa"]), persistentStore: description, cuttlefish: self.cuttlefish)

        // now A is excluded
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["mmm"], userInitiatedRemovals: ["aaa"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["mmm"]), disallowedMachineIDs: Set(["aaa", "xxx"]), persistentStore: description, cuttlefish: self.cuttlefish)
    }

    func testAllowListManipulationUsingIdMSSetList() throws {
        let description = tmpStoreDescription(name: "container.db")
        let container = try Container(name: ContainerName(container: "test", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        try self.assert(container: container, allowedMachineIDs: [], disallowedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ccc"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // Now, an 'add' comes in for some peers
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ccc", "ddd", "eee"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")

        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc", "ddd", "eee"]), disallowedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // But, the next time we ask IDMS, they still haven't made it to the full list, and in fact, C, D and E have disappeared.
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")

        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            let cccMOs = knownMachineMOs.filter { $0.machineID == "ccc" }
            XCTAssertEqual(cccMOs.count, 1, "Should have one machine MO for ccc")

            let cccMO = cccMOs.first!
            XCTAssertEqual(cccMO.status, Int64(TPMachineIDStatus.ghostedFromTDL.rawValue), "Status of ccc MO should be 'ghostedFromTDL'")
            XCTAssertFalse(cccMO.allowed, "allowed should no longer be a used field")

            cccMO.modified = Date(timeIntervalSinceNow: TimeInterval(twoDaysInThePast))

            let dddMOs = knownMachineMOs.filter { $0.machineID == "ddd" }
            XCTAssertEqual(dddMOs.count, 1, "Should have one machine MO for ddd")

            let dddMO = dddMOs.first!
            XCTAssertEqual(dddMO.status, Int64(TPMachineIDStatus.ghostedFromTDL.rawValue), "Status of ddd MO should be 'ghostedFromTDL'")
            XCTAssertFalse(dddMO.allowed, "allowed should no longer be a used field")

            dddMO.modified = Date(timeIntervalSinceNow: TimeInterval(twoDaysInThePast))

            let eeeMOs = knownMachineMOs.filter { $0.machineID == "eee" }
            XCTAssertEqual(eeeMOs.count, 1, "Should have one machine MO for eee")

            let eeeMO = eeeMOs.first!
            XCTAssertEqual(eeeMO.status, Int64(TPMachineIDStatus.ghostedFromTDL.rawValue), "Status of eee MO should be 'ghostedFromTDL'")
            XCTAssertFalse(eeeMO.allowed, "allowed should no longer be a used field")
            eeeMO.modified = Date(timeIntervalSinceNow: TimeInterval(twoDaysInThePast))

            try! container.moc.save()
        }
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")

        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb"]), disallowedMachineIDs: Set(["ccc", "ddd", "eee"]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // Removing E from the list marks E as disallowed
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ccc", "ddd"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")

        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc", "ddd"]), disallowedMachineIDs: Set(["eee"]), unknownMachineIDs: Set([]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container should not think it could use an IDMS list set")

        // Removing C and D causes these peers to be disallowed
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")

        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb"]), disallowedMachineIDs: Set(["eee"]), ghostedMachineIDs: Set(["ccc", "ddd"]), persistentStore: description, cuttlefish: self.cuttlefish)

        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            let cccMOs = knownMachineMOs.filter { $0.machineID == "ccc" }
            XCTAssertEqual(cccMOs.count, 1, "Should have one machine MO for ccc")

            let cccMO = cccMOs.first!
            XCTAssertEqual(cccMO.status, Int64(TPMachineIDStatus.ghostedFromTDL.rawValue), "Status of ccc MO should be 'ghostedFromTDL'")
            XCTAssertFalse(cccMO.allowed, "allowed should no longer be a used field")

            cccMO.modified = Date(timeIntervalSinceNow: TimeInterval(twoDaysInThePast))

            let dddMOs = knownMachineMOs.filter { $0.machineID == "ddd" }
            XCTAssertEqual(dddMOs.count, 1, "Should have one machine MO for ddd")

            let dddMO = dddMOs.first!
            XCTAssertEqual(dddMO.status, Int64(TPMachineIDStatus.ghostedFromTDL.rawValue), "Status of ddd MO should be 'ghostedFromTDL'")
            XCTAssertFalse(dddMO.allowed, "allowed should no longer be a used field")

            dddMO.modified = Date(timeIntervalSinceNow: TimeInterval(twoDaysInThePast))

            try! container.moc.save()
        }

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")

        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb"]), disallowedMachineIDs: Set(["ccc", "ddd", "eee"]), persistentStore: description, cuttlefish: self.cuttlefish)

        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // X is introduced
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "xxx"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "xxx"]), disallowedMachineIDs: Set(["ccc", "ddd", "eee"]), persistentStore: description, cuttlefish: self.cuttlefish)

        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            knownMachineMOs.forEach {
                if $0.machineID == "xxx" {
                    $0.modified = Date(timeIntervalSinceNow: TimeInterval(twoDaysInThePast))
                }
            }

            try! container.moc.save()
        }

        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // Setting the list again should kick out X, since it was 'added' too long ago
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb"]), disallowedMachineIDs: Set(["ccc", "ddd", "eee"]), ghostedMachineIDs: Set(["xxx"]), persistentStore: description, cuttlefish: self.cuttlefish)

        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            knownMachineMOs.forEach {
                if $0.machineID == "aaa" {
                    $0.status = Int64(TPMachineIDStatus.unknown.rawValue)
                }
            }

            try! container.moc.save()
        }

        // Setting the list again should set A to unknown after it falls off the list
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb"], accountIsDemo: false, listDifference: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["bbb"]), disallowedMachineIDs: Set(["ccc", "ddd", "eee"]), unknownMachineIDs: Set(["aaa"]), ghostedMachineIDs: Set(["xxx"]), persistentStore: description, cuttlefish: self.cuttlefish)

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb"]), disallowedMachineIDs: Set(["ccc", "ddd", "eee"]), unknownMachineIDs: Set([]), ghostedMachineIDs: Set(["xxx"]), persistentStore: description, cuttlefish: self.cuttlefish)
    }

    func testSingleDeviceWillNotDepartWhenTakenOffMIDList() throws {
        let description = tmpStoreDescription(name: "container.db")
        let container = try Container(name: ContainerName(container: "test", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let (peerID, permanentInfo, permanentInfoSig, _, _, _, error) = container.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")

        XCTAssertNil(error)
        XCTAssertNotNil(peerID)
        XCTAssertNotNil(permanentInfo)
        XCTAssertNotNil(permanentInfoSig)

        try self.assert(container: container, allowedMachineIDs: [], disallowedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ccc"], accountIsDemo: false), "should be able to set allowed machine IDs")
        let (_, _, _, establishError) = container.establishSync(test: self, ckksKeys: [self.manateeKeySet], tlkShares: [], preapprovedKeys: [])
        XCTAssertNil(establishError, "Should be able to establish() with no error")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: Set([]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // Removing the peer from the MID list doesn't cause the sole peer to distrust itself
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: [], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")

        try self.assert(container: container, allowedMachineIDs: Set([]), disallowedMachineIDs: Set([]), ghostedMachineIDs: Set(["aaa", "bbb", "ccc"]), persistentStore: description, cuttlefish: self.cuttlefish)

        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            let aaaMOs = knownMachineMOs.filter { $0.machineID == "aaa" }
            XCTAssertEqual(aaaMOs.count, 1, "Should have one machine MO for aaa")

            let aaaMO = aaaMOs.first!
            XCTAssertEqual(aaaMO.status, Int64(TPMachineIDStatus.ghostedFromTDL.rawValue), "Status of aaa MO should be 'ghostedFromTDL'")
            XCTAssertFalse(aaaMO.allowed, "allowed should no longer be a used field")

            aaaMO.modified = Date(timeIntervalSinceNow: TimeInterval(twoDaysInThePast))

            let bbbMOs = knownMachineMOs.filter { $0.machineID == "bbb" }
            XCTAssertEqual(bbbMOs.count, 1, "Should have one machine MO for bbb")

            let bbbMO = bbbMOs.first!
            XCTAssertEqual(bbbMO.status, Int64(TPMachineIDStatus.ghostedFromTDL.rawValue), "Status of bbb MO should be 'ghostedFromTDL'")
            XCTAssertFalse(bbbMO.allowed, "allowed should no longer be a used field")

            bbbMO.modified = Date(timeIntervalSinceNow: TimeInterval(twoDaysInThePast))

            let cccMOs = knownMachineMOs.filter { $0.machineID == "ccc" }
            XCTAssertEqual(cccMOs.count, 1, "Should have one machine MO for ccc")

            let cccMO = cccMOs.first!
            XCTAssertEqual(cccMO.status, Int64(TPMachineIDStatus.ghostedFromTDL.rawValue), "Status of ccc MO should be 'ghostedFromTDL'")
            XCTAssertFalse(cccMO.allowed, "allowed should no longer be a used field")

            cccMO.modified = Date(timeIntervalSinceNow: TimeInterval(twoDaysInThePast))

            try! container.moc.save()
        }
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: [], accountIsDemo: false), "should be able to set allowed machine IDs")

        try self.assert(container: container, allowedMachineIDs: Set([]), disallowedMachineIDs: Set(["aaa", "bbb", "ccc"]), persistentStore: description, cuttlefish: self.cuttlefish)

        do {
            let (_, _, updateError) = container.updateSync(test: self)
            XCTAssertNil(updateError, "Should not be an error updating the container after establishing")
        }

        assertTrusts(context: container, peerIDs: [peerID!])

        // THe update() will wipe out the deny-listing of bbb and ccc, but we should keep aaa. around.
        try self.assert(container: container, allowedMachineIDs: Set([]), disallowedMachineIDs: Set(["aaa"]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")
    }

    func testAllowSetUpgrade() throws {
        let containerName = ContainerName(container: "test", context: OTDefaultContext)

        let description = tmpStoreDescription(name: "container.db")
        let url = Bundle(for: type(of: self)).url(forResource: "TrustedPeersHelper", withExtension: "momd")!
        let mom = getOrMakeModel(url: url)
        let persistentContainer = NSPersistentContainer(name: "TrustedPeersHelper", managedObjectModel: mom)
        persistentContainer.persistentStoreDescriptions = [description]

        persistentContainer.loadPersistentStores { _, error in
            XCTAssertNil(error, "Should be no error loading persistent stores")
        }

        let moc = persistentContainer.newBackgroundContext()
        moc.performAndWait {
            let containerMO = ContainerMO(context: moc)
            containerMO.name = containerName.asSingleString()
            containerMO.allowedMachineIDs = Set(["aaa", "bbb", "ccc"]) as NSSet

            XCTAssertNoThrow(try! moc.save())
        }

        // Now TPH boots up with a preexisting model
        let container = try Container(name: containerName, persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let (peerID, permanentInfo, permanentInfoSig, _, _, _, error) = container.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")

        XCTAssertNil(error)
        XCTAssertNotNil(peerID)
        XCTAssertNotNil(permanentInfo)
        XCTAssertNotNil(permanentInfoSig)

        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)

        // Setting a new list should work fine
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ddd"], accountIsDemo: false), "should be able to set allowed machine IDs")

        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ddd"]), disallowedMachineIDs: [], ghostedMachineIDs: Set(["ccc"]), persistentStore: description, cuttlefish: self.cuttlefish)

        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            let cccMOs = knownMachineMOs.filter { $0.machineID == "ccc" }
            XCTAssertEqual(cccMOs.count, 1, "Should have one machine MO for ccc")

            let cccMO = cccMOs.first!
            XCTAssertEqual(cccMO.status, Int64(TPMachineIDStatus.ghostedFromTDL.rawValue), "Status of ccc MO should be 'ghostedFromTDL'")
            XCTAssertFalse(cccMO.allowed, "allowed should no longer be a used field")

            cccMO.modified = Date(timeIntervalSinceNow: TimeInterval(twoDaysInThePast))
            try! container.moc.save()
        }

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ddd"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")

        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ddd"]), disallowedMachineIDs: ["ccc"], persistentStore: description, cuttlefish: self.cuttlefish)

        container.moc.performAndWait {
            XCTAssertEqual(container.containerMO.allowedMachineIDs, Set<String>() as NSSet, "Set of allowed machine IDs should now be empty")
        }
    }

    func testAllowStatusUpgrade() throws {
        let containerName = ContainerName(container: "test", context: OTDefaultContext)

        let description = tmpStoreDescription(name: "container.db")
        let url = Bundle(for: type(of: self)).url(forResource: "TrustedPeersHelper", withExtension: "momd")!
        let mom = getOrMakeModel(url: url)
        let persistentContainer = NSPersistentContainer(name: "TrustedPeersHelper", managedObjectModel: mom)
        persistentContainer.persistentStoreDescriptions = [description]

        persistentContainer.loadPersistentStores { _, error in
            XCTAssertNil(error, "Should be no error loading persistent stores")
        }

        let moc = persistentContainer.newBackgroundContext()
        moc.performAndWait {
            let containerMO = ContainerMO(context: moc)
            containerMO.name = containerName.asSingleString()

            let machine = MachineMO(context: moc)
            machine.allowed = true
            machine.modified = Date()
            machine.machineID = "aaa"
            containerMO.addToMachines(machine)

            let machineB = MachineMO(context: moc)
            machineB.allowed = false
            machineB.modified = Date()
            machineB.machineID = "bbb"
            containerMO.addToMachines(machineB)

            try! moc.save()
        }

        // Now TPH boots up with a preexisting model
        let container = try Container(name: containerName, persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        try self.assert(container: container, allowedMachineIDs: Set(["aaa"]), disallowedMachineIDs: ["bbb"], persistentStore: description, cuttlefish: self.cuttlefish)

        // Setting a new list should work fine
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "ddd"], accountIsDemo: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "ddd"]), disallowedMachineIDs: ["bbb"], persistentStore: description, cuttlefish: self.cuttlefish)

        container.moc.performAndWait {
            XCTAssertEqual(container.containerMO.allowedMachineIDs, Set<String>() as NSSet, "Set of allowed machine IDs should now be empty")
        }
    }

    func testMachineIDListSetWithUnknownMachineIDs() throws {
        let description = tmpStoreDescription(name: "container.db")
        let (container, _) = try establish(reload: false, store: description)

        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            knownMachineMOs.forEach {
                container.containerMO.removeFromMachines($0)
            }

            try! container.moc.save()
        }

        // and set the machine ID list to something that doesn't include 'aaa'
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: [], unknownMachineIDs: Set(["aaa"]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // But check that it exists, and set its modification date to a while ago for an upcoming test
        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            let aaaMOs = knownMachineMOs.filter { $0.machineID == "aaa" }
            XCTAssertEqual(aaaMOs.count, 1, "Should have one machine MO for aaa")

            let aaaMO = aaaMOs.first!
            XCTAssertEqual(aaaMO.status, Int64(TPMachineIDStatus.unknown.rawValue), "Status of aaa MO should be 'unknown'")
            XCTAssertFalse(aaaMO.allowed, "allowed should no longer be a used field")

            aaaMO.modified = Date(timeIntervalSinceNow: -60)
            try! container.moc.save()
        }

        // With it 'modified' only 60s ago, we shouldn't want a list fetch
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // Setting it again is fine...
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], accountIsDemo: false, listDifference: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: [], unknownMachineIDs: Set(["aaa"]), persistentStore: description, cuttlefish: self.cuttlefish)

        // And doesn't reset the modified date on the record
        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            let aaaMOs = knownMachineMOs.filter { $0.machineID == "aaa" }
            XCTAssertEqual(aaaMOs.count, 1, "Should have one machine MO for aaa")

            let aaaMO = aaaMOs.first!
            XCTAssertEqual(aaaMO.status, Int64(TPMachineIDStatus.unknown.rawValue), "Status of aaa MO should be 'unknown'")
            XCTAssertFalse(aaaMO.allowed, "allowed should no longer be a used field")

            XCTAssertLessThan(aaaMO.modified!, Date(timeIntervalSinceNow: -5), "Modification date of record should still be its previously on-disk value")
        }

        // And can be promoted to 'allowed'
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")

        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb"]), disallowedMachineIDs: [], ghostedMachineIDs: Set(["ccc"]), persistentStore: description, cuttlefish: self.cuttlefish)

        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            let cccMOs = knownMachineMOs.filter { $0.machineID == "ccc" }
            XCTAssertEqual(cccMOs.count, 1, "Should have one machine MO for ccc")

            let cccMO = cccMOs.first!
            XCTAssertEqual(cccMO.status, Int64(TPMachineIDStatus.ghostedFromTDL.rawValue), "Status of ccc MO should be 'ghostedFromTDL'")
            XCTAssertFalse(cccMO.allowed, "allowed should no longer be a used field")

            cccMO.modified = Date(timeIntervalSinceNow: TimeInterval(twoDaysInThePast))
            try! container.moc.save()
        }

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")

        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb"]), disallowedMachineIDs: ["ccc"], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")
    }

    func testDuplicateVouchersOnload() throws {
        let description = tmpStoreDescription(name: "container.db")

        let store = tmpStoreDescription(name: "container.db")
        let (c, peerID1) = try establish(reload: false, store: store)

        let (c2, peerID2) = try joinByVoucher(sponsor: c,
                                              containerID: "second",
                                              machineID: "bbb",
                                              machineIDs: ["aaa", "bbb", "ccc"], accountIsDemo: false,
                                              store: store)

        let (c3, peerID3) = try joinByVoucher(sponsor: c,
                                              containerID: "third",
                                              machineID: "ccc",
                                              machineIDs: ["aaa", "bbb", "ccc"], accountIsDemo: false,
                                              store: store)

        let (_, _, cUpdateError) = c.updateSync(test: self)
        XCTAssertNil(cUpdateError, "Should be able to update first container")

        let (_, _, cUpdateError2) = c2.updateSync(test: self)
        XCTAssertNil(cUpdateError2, "Should be able to update first container")
        let (_, _, cUpdateError3) = c3.updateSync(test: self)
        XCTAssertNil(cUpdateError3, "Should be able to update first container")
        let (_, _, _) = c.updateSync(test: self)

        assertTrusts(context: c, peerIDs: [peerID1, peerID2, peerID3])

        let iterations = 5
        let expectedVouchers = iterations * 3

        c.moc.performAndWait {
            var vouchers: [VoucherMO] = Array()

            for peer in c.containerMO.peers as! Set<PeerMO> {
                for voucher in peer.vouchers! {
                    let vouch = voucher as! VoucherMO
                    vouchers.append(vouch)
                }
            }

            XCTAssertEqual(vouchers.count, 2, "should have 2 vouchers")

            // Creating a new ContainerMO results in a CoreData object with no data.
            // We could load it by name as in Container.swift.
            // But we can instead use the extant ContainerMO from within `c`.
            let containerMO = c.containerMO

            // The "beneficiary" relationship in the CoreDataModel is:
            //   each Peer many have more than one Voucher, and each Voucher has exactly one Peer
            // Consequently, adding the same VoucherMO to the nth and then n+1st PeerMO results in
            // that VoucherMO being removed from the nth PeerMO.
            // Note that relationship is called "beneficiary" and we should adhere to the invariant
            // that it comports with the semantic beneficiary in the Voucher data -- that the
            // beneficiary field in the Voucher protobuf data matches the PeerID.
            //
            // To test this, we can create new VoucherMO objects which contain the same data+sig,
            // and then violate the invariant that the CoreDataModel "beneficiary" relationship
            // between VoucherMO and PeerMO matches the semantics of the Voucher & Peer.
            for peer in containerMO.peers as? Set<PeerMO> ?? Set() {
                for vouch in peer.vouchers as? Set<VoucherMO> ?? Set() {
                    // We (try to) remove both vouchers from all three peers, but they're actually only
                    // associated with one peer each.
                    peer.removeFromVouchers(vouch)
                }
                for i in 0..<iterations {
                    for vouch in vouchers {
                        let dataDupVoucher = VoucherMO(context: c.moc)
                        dataDupVoucher.voucherInfo = vouch.voucherInfo
                        dataDupVoucher.voucherInfoSig = vouch.voucherInfoSig
                        peer.addToVouchers(dataDupVoucher)
                    }
                    let keyPair = _SFECKeyPair.init(randomKeyPairWith: _SFECKeySpecifier.init(curve: SFEllipticCurve.nistp384))!
                    let orphanTPVoucher = try! TPVoucher(reason: .sameDevice, beneficiaryID: "not the droid you're looking for \(i)", sponsorID: peer.peerID!, signing: keyPair)
                    let orphanVoucherMO = VoucherMO(context: c.moc)
                    orphanVoucherMO.voucherInfo = orphanTPVoucher.data
                    orphanVoucherMO.voucherInfoSig = orphanTPVoucher.sig
                    peer.addToVouchers(orphanVoucherMO)
                }
            }
            // Do this test after removing & adding above, to ensure CoreData is done dealing with
            // the relationships.
            // This is because adding a VoucherMO object to a PeerMO can modify three MO objects:
            // (As of TrustedPeersHelper_4.xcdatamodel as of when this commet was committed.)
            // 1. The VoucherMO: to set the "beneficiary" relationship to the PeerMO to which it is
            //    being added
            // 2. The PeerMO to which it's being added: to add the VoucherMO to this PeerMO's
            //    "vouchers" relationship
            // 3. The PeerMO referenced in the VoucherMO's old "beneficiary" relationship: to remove
            //    the VoucherMO from this old "beneficiary" PeerMO's "vouchers" relationship
            if let containerPeers = containerMO.peers as? Set<PeerMO> {
                for peer in containerPeers {
                    XCTAssertEqual((peer.vouchers as? Set<VoucherMO>)?.count, expectedVouchers, "peer \(peer.peerID!) should have \(expectedVouchers) vouchers")
                }
            } else {
                XCTFail("containerMO.peers should be a Set<PeerMO>")
            }

            XCTAssertNoThrow(try! c.moc.save())

            // double check after saving, to ensure there were no additional CoreData shenanigans
            if let containerPeers = containerMO.peers as? Set<PeerMO> {
                for peer in containerPeers {
                    XCTAssertEqual((peer.vouchers as? Set<VoucherMO>)?.count, expectedVouchers, "peer \(peer.peerID!) should still have \(expectedVouchers) vouchers")
                }
            } else {
                XCTFail("containerMO.peers should be a Set<PeerMO>")
            }
        }

        // reload container
        do {
            let container = try Container(name: c.name, persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
            // Now test that the DB only has vouchers comporting with the invariant that the "beneficiary" CoreDataModel releationship
            // matches the semantic beneficiary in the Voucher data
            container.moc.performAndWait {
                XCTAssertEqual(container.model.voucherCount(), 2, "voucher count should be 2")
                XCTAssertEqual(container.containerMO.peers!.count, 3, "should still have 3 peers")
                if let containerPeers = container.containerMO.peers as? Set<PeerMO> {
                    for peer in containerPeers {
                        let voucherCount = (peer.vouchers as? Set<VoucherMO>)?.count
                        switch peer.peerID! {
                        case peerID1:
                            XCTAssertEqual(voucherCount, 0, "peer \(peerID1) should have 0 vouchers")
                        case peerID2:
                            XCTAssertEqual(voucherCount, 1, "peer \(peerID2) should have 1 voucher")
                        case peerID3:
                            XCTAssertEqual(voucherCount, 1, "peer \(peerID3) should have 1 voucher")
                        default:
                            XCTFail("Encountered unexpected peerID: \(peer.peerID!)")
                        }
                    }
                } else {
                    XCTFail("Couldn't get container.containerMO.peers as Set<PeerMO>")
                }
            }
        } catch {
            XCTFail("Creating container errored: \(error)")
        }
    }

    func testMachineIDListSetDisallowedOldUnknownMachineIDs() throws {
        let description = tmpStoreDescription(name: "container.db")
        var (container, peerID1) = try establish(reload: false, allowedMachineIDs: Set(["aaa"]), store: description)

        // and set the machine ID list to something that doesn't include 'ddd'
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ccc"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: [], unknownMachineIDs: Set(), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        container.moc.performAndWait {
            XCTAssertEqual(container.containerMO.honorIDMSListChanges, "YES", "honorIDMSListChanges should be YES")
        }

        let unknownMachineID = "ddd"
        let (_, peerID3) = try self.joinByVoucher(sponsor: container,
                                                  containerID: "second",
                                                  machineID: unknownMachineID,
                                                  machineIDs: Set([unknownMachineID, "aaa", "bbb", "ccc"]), accountIsDemo: false,
                                                  store: description)

        // And the first container accepts the join...
        let (_, _, cUpdateError) = container.updateSync(test: self)
        XCTAssertNil(cUpdateError, "Should be able to update first container")
        assertTrusts(context: container, peerIDs: [peerID1, peerID3])

        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: [], unknownMachineIDs: Set([unknownMachineID]), persistentStore: description, cuttlefish: self.cuttlefish)

        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")
        container.moc.performAndWait {
            XCTAssertEqual(container.containerMO.honorIDMSListChanges, "YES", "honorIDMSListChanges should be YES")
        }

        // But an entry for "ddd" should exist, as a peer in the model claims it as their MID
        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            let unknownMOs = knownMachineMOs.filter { $0.machineID == unknownMachineID }
            XCTAssertEqual(unknownMOs.count, 1, "Should have one machine MO for ddd")

            let dddMO = unknownMOs.first!
            XCTAssertEqual(dddMO.status, Int64(TPMachineIDStatus.unknown.rawValue), "Status of ddd MO should be 'unknown'")
            XCTAssertFalse(dddMO.allowed, "allowed should no longer be a used field")

            // Pretend that D was added 2 days ago
            dddMO.modified = Date(timeIntervalSinceNow: TimeInterval(twoDaysInThePast))
            try! container.moc.save()
        }

        // reload container
        do {
            container = try Container(name: ContainerName(container: "test", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
            container.moc.performAndWait {
                XCTAssertEqual(container.containerMO.honorIDMSListChanges, "YES", "honorIDMSListChanges should be YES")
            }
        } catch {
            XCTFail("Creating container errored: \(error)")
        }
        XCTAssertTrue(container.fullIDMSListWouldBeHelpful(), "Container should think it could use an IDMS list set: there's machine IDs pending removal")

        // And, setting the list again should disallow ddd, since it is so old
        // Note that this _should_ return a list difference, since D is promoted to disallowed
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ccc"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: [unknownMachineID], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // Setting ths list again has no change
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ccc"], accountIsDemo: false, listDifference: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: [unknownMachineID], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // But D can appear again, no problem.
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ccc", "ddd"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc", "ddd"]), disallowedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")
        container.moc.performAndWait {
            XCTAssertEqual(container.containerMO.honorIDMSListChanges, "YES", "honorIDMSListChanges should be YES")
        }
    }

    func testMachineIDListHandlingWithPeers() throws {
        let description = tmpStoreDescription(name: "container.db")
        let (container, peerID1) = try establish(reload: false, store: description)

        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: [], unknownMachineIDs: Set(), persistentStore: description, cuttlefish: self.cuttlefish)

        let unknownMachineID = "not-on-list"
        let (_, peerID2) = try self.joinByVoucher(sponsor: container,
                                                  containerID: "second",
                                                  machineID: unknownMachineID,
                                                  machineIDs: Set([unknownMachineID, "aaa", "bbb", "ccc"]), accountIsDemo: false,
                                                  store: description)

        // And the first container accepts the join...
        let (_, _, cUpdateError) = container.updateSync(test: self)
        XCTAssertNil(cUpdateError, "Should be able to update first container")
        assertTrusts(context: container, peerIDs: [peerID1, peerID2])

        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: [], unknownMachineIDs: Set([unknownMachineID]), persistentStore: description, cuttlefish: self.cuttlefish)
    }

    func testMachineIDListHandlingInDemoAccounts() throws {
        // Demo accounts have no machine IDs in their lists
        let description = tmpStoreDescription(name: "container.db")
        var (container, peerID1) = try establish(reload: false, allowedMachineIDs: Set(), accountIsDemo: true, store: description)

        // And so we just don't write down any MIDs
        try self.assert(container: container, allowedMachineIDs: Set([]), disallowedMachineIDs: [], unknownMachineIDs: Set([]), persistentStore: description, cuttlefish: self.cuttlefish)

        // Even when joining...
        let unknownMachineID = "not-on-list"
        let (c2, peerID2) = try self.joinByVoucher(sponsor: container,
                                                   containerID: "second",
                                                   machineID: unknownMachineID,
                                                   machineIDs: Set(),
                                                   accountIsDemo: true,
                                                   store: description)
        try self.assert(container: c2, allowedMachineIDs: Set([]), disallowedMachineIDs: [], unknownMachineIDs: Set([]), persistentStore: description, cuttlefish: self.cuttlefish)

        c2.moc.performAndWait {
            c2.containerMO.honorIDMSListChanges = "NO"
        }

        // And the first container accepts the join...
        let (_, _, cUpdateError) = container.updateSync(test: self)
        XCTAssertNil(cUpdateError, "Should be able to update first container")
        assertTrusts(context: container, peerIDs: [peerID1, peerID2])

        // And still has nothing in its list...
        try self.assert(container: container, allowedMachineIDs: Set([]), disallowedMachineIDs: [], unknownMachineIDs: Set([]), persistentStore: description, cuttlefish: self.cuttlefish)

        // reload container
        do {
            container = try Container(name: ContainerName(container: "test", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
            container.moc.performAndWait {
                XCTAssertEqual(container.containerMO.honorIDMSListChanges, "NO", "honorIDMSListChanges should be NO")
            }
        } catch {
            XCTFail("Creating container errored: \(error)")
        }

        // Even after a full list set
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: [], accountIsDemo: true, listDifference: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set([]), disallowedMachineIDs: [], unknownMachineIDs: Set([]), persistentStore: description, cuttlefish: self.cuttlefish)

        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")
        container.moc.performAndWait {
            XCTAssertEqual(container.containerMO.honorIDMSListChanges, "NO", "honorIDMSListChanges should be NO")
        }
    }

    func testContainerAndModelConsistency() throws {
        let preTestContainerName = ContainerName(container: "testToCreatePrepareData", context: "context")
        let description = tmpStoreDescription(name: "container.db")
        let containerTest = try Container(name: preTestContainerName, persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        let (peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, _, error) = containerTest.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        XCTAssertNil(error)
        XCTAssertNotNil(peerID)
        XCTAssertNotNil(permanentInfo)
        XCTAssertNotNil(permanentInfoSig)
        XCTAssertNotNil(stableInfo)
        XCTAssertNotNil(stableInfoSig)

        let containerName = ContainerName(container: "test", context: OTDefaultContext)
        let url = Bundle(for: type(of: self)).url(forResource: "TrustedPeersHelper", withExtension: "momd")!
        let mom = getOrMakeModel(url: url)
        let persistentContainer = NSPersistentContainer(name: "TrustedPeersHelper", managedObjectModel: mom)
        persistentContainer.persistentStoreDescriptions = [description]

        persistentContainer.loadPersistentStores { _, error in
            XCTAssertNil(error, "Should be no error loading persistent stores")
        }

        let moc = persistentContainer.newBackgroundContext()
        moc.performAndWait {
            let containerMO = ContainerMO(context: moc)
            containerMO.name = containerName.asSingleString()
            containerMO.allowedMachineIDs = Set(["aaa"]) as NSSet
            containerMO.egoPeerID = peerID
            containerMO.egoPeerPermanentInfo = permanentInfo
            containerMO.egoPeerPermanentInfoSig = permanentInfoSig
            containerMO.egoPeerStableInfoSig = stableInfoSig
            containerMO.egoPeerStableInfo = stableInfo
            let containerEgoStableInfo = TPPeerStableInfo(data: stableInfo!, sig: stableInfoSig!)
            do {
                let peerKeys: OctagonSelfPeerKeys = try loadEgoKeysSync(peerID: containerMO.egoPeerID!)
                let newClock = containerEgoStableInfo!.clock + 2
                let info3 = try TPPeerStableInfo(clock: newClock,
                                                 frozenPolicyVersion: containerEgoStableInfo!.frozenPolicyVersion,
                                                 flexiblePolicyVersion: containerEgoStableInfo!.flexiblePolicyVersion!,
                                                 policySecrets: containerEgoStableInfo!.policySecrets,
                                                 syncUserControllableViews: containerEgoStableInfo!.syncUserControllableViews,
                                                 secureElementIdentity: containerEgoStableInfo!.secureElementIdentity,
                                                 walrusSetting: containerEgoStableInfo!.walrusSetting,
                                                 webAccess: containerEgoStableInfo!.webAccess,
                                                 deviceName: containerEgoStableInfo!.deviceName,
                                                 serialNumber: containerEgoStableInfo!.serialNumber,
                                                 osVersion: containerEgoStableInfo!.osVersion,
                                                 signing: peerKeys.signingKey,
                                                 recoverySigningPubKey: containerEgoStableInfo!.recoverySigningPublicKey,
                                                 recoveryEncryptionPubKey: containerEgoStableInfo!.recoveryEncryptionPublicKey,
                                                 isInheritedAccount: containerEgoStableInfo!.isInheritedAccount)

                // setting the containerMO's ego stable info to an old clock
                containerMO.egoPeerStableInfo = containerEgoStableInfo!.data
                containerMO.egoPeerStableInfoSig = containerEgoStableInfo!.sig

                // now we are adding the ego stable info with a clock of 3 to the list of peers
                let peer = PeerMO(context: moc)
                peer.peerID = peerID
                peer.permanentInfo = permanentInfo
                peer.permanentInfoSig = permanentInfoSig
                peer.stableInfo = info3.data
                peer.stableInfoSig = info3.sig
                peer.isEgoPeer = true
                peer.container = containerMO

                containerMO.addToPeers(peer)

                // at this point the containerMO's egoStableInfo should have a clock of 1
                // the saved ego peer in the peer list has a clock of 3

            } catch {
                XCTFail("load ego keys failed: \(error)")
            }
            XCTAssertNoThrow(try! moc.save())
        }

        // Now TPH boots up with a preexisting model
        let container = try Container(name: containerName, persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        container.moc.performAndWait {
            do {
                let stableInfoAfterBoot = TPPeerStableInfo(data: try XCTUnwrap(container.containerMO.egoPeerStableInfo, "Should have an ego peer stable info"),
                                                           sig: try XCTUnwrap(container.containerMO.egoPeerStableInfoSig, "Should have an ego peer stable info sig"))
                XCTAssertNotNil(stableInfoAfterBoot)

                // after boot the clock should be updated to the one that was saved in the model
                XCTAssertEqual(stableInfoAfterBoot!.clock, 3, "clock should be updated to 3")
            } catch {
                XCTFail("Should not have thrown this error: \(error)")
            }
        }
    }

    func testRetryableError() throws {
        XCTAssertTrue(RetryingCKCodeService.retryableError(error: NSError(domain: NSURLErrorDomain, code: NSURLErrorTimedOut, userInfo: nil)))
        XCTAssertTrue(RetryingCKCodeService.retryableError(error: NSError(domain: NSURLErrorDomain, code: NSURLErrorNotConnectedToInternet, userInfo: nil)))
        XCTAssertFalse(RetryingCKCodeService.retryableError(error: NSError(domain: NSURLErrorDomain, code: NSURLErrorUnknown, userInfo: nil)))
        XCTAssertTrue(RetryingCKCodeService.retryableError(error: NSError(domain: CKErrorDomain, code: CKError.networkFailure.rawValue, userInfo: nil)))
        XCTAssertFalse(RetryingCKCodeService.retryableError(error: NSError(domain: CKErrorDomain, code: CKError.serverRejectedRequest.rawValue, userInfo: nil)))

        let sub0 = NSError(domain: CKUnderlyingErrorDomain, code: CKUnderlyingError.serverInternalError.rawValue, userInfo: nil)
        let e0 = NSError(domain: CKErrorDomain, code: CKError.serverRejectedRequest.rawValue, userInfo: [NSUnderlyingErrorKey: sub0])
        XCTAssertTrue(RetryingCKCodeService.retryableError(error: e0))

        let sub1 = NSError(domain: CKUnderlyingErrorDomain, code: CKUnderlyingError.genericError.rawValue, userInfo: nil)
        let e1 = NSError(domain: CKErrorDomain, code: CKError.serverRejectedRequest.rawValue, userInfo: [NSUnderlyingErrorKey: sub1])
        XCTAssertFalse(RetryingCKCodeService.retryableError(error: e1))

        let cf2 = NSError(domain: CuttlefishErrorDomain, code: CuttlefishErrorCode.changeTokenExpired.rawValue, userInfo: nil)
        let int2 = NSError(domain: CKUnderlyingErrorDomain, code: CKUnderlyingError.pluginError.rawValue, userInfo: [NSUnderlyingErrorKey: cf2])
        let e2 = NSError(domain: CKErrorDomain, code: CKError.serverRejectedRequest.rawValue, userInfo: [NSUnderlyingErrorKey: int2])
        XCTAssertFalse(RetryingCKCodeService.retryableError(error: e2))

        let cf3 = NSError(domain: CuttlefishErrorDomain, code: CuttlefishErrorCode.retryableServerFailure.rawValue, userInfo: nil)
        let int3 = NSError(domain: CKUnderlyingErrorDomain, code: CKUnderlyingError.pluginError.rawValue, userInfo: [NSUnderlyingErrorKey: cf3])
        let e3 = NSError(domain: CKErrorDomain, code: CKError.serverRejectedRequest.rawValue, userInfo: [NSUnderlyingErrorKey: int3])
        XCTAssertTrue(RetryingCKCodeService.retryableError(error: e3))

        let cf4 = NSError(domain: CuttlefishErrorDomain, code: CuttlefishErrorCode.transactionalFailure.rawValue, userInfo: nil)
        let int4 = NSError(domain: CKUnderlyingErrorDomain, code: CKUnderlyingError.pluginError.rawValue, userInfo: [NSUnderlyingErrorKey: cf4])
        let e4 = NSError(domain: CKErrorDomain, code: CKError.serverRejectedRequest.rawValue, userInfo: [NSUnderlyingErrorKey: int4])
        XCTAssertTrue(RetryingCKCodeService.retryableError(error: e4))

        let int5 = NSError(domain: CKUnderlyingErrorDomain, code: CKUnderlyingError.pluginError.rawValue, userInfo: nil)
        let e5 = NSError(domain: CKErrorDomain, code: CKError.serverRejectedRequest.rawValue, userInfo: [NSUnderlyingErrorKey: int5])
        print("\(String(describing: e5))")
        XCTAssertTrue(RetryingCKCodeService.retryableError(error: e5))
    }

    func testEstablishWithEnforceIDMSListNotSetBehavior() throws {
        let contextID = "OTDefaultContext"
        let description = tmpStoreDescription(name: "container.db")

        var container = try Container(name: ContainerName(container: "test", context: contextID), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        container.moc.performAndWait {
            XCTAssertEqual(container.containerMO.honorIDMSListChanges, "UNKNOWN", "honorIDMSListChanges should be unknown")
        }

        let (peerID, permanentInfo, permanentInfoSig, _, _, _, error) = container.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = container.getStateSync(test: self)
            XCTAssertTrue( state.bottles.contains { $0.peerID == peerID }, "should have a bottle for peer")
            let secret = container.loadSecretSync(test: self, label: peerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNotNil(peerID)
        XCTAssertNotNil(permanentInfo)
        XCTAssertNotNil(permanentInfoSig)
        XCTAssertNil(error)

        _ = container.dumpSync(test: self)

        do {
            container = try Container(name: ContainerName(container: "test", context: contextID), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        } catch {
            XCTFail("Creating container errored: \(error)")
        }

        let (peerID2, _, _, error2) = container.establishSync(test: self, ckksKeys: [self.manateeKeySet], tlkShares: [], preapprovedKeys: [])
        XCTAssertNil(error2)
        XCTAssertNotNil(peerID2)

        _ = container.dumpSync(test: self)
        container.moc.performAndWait {
            XCTAssertEqual(container.containerMO.honorIDMSListChanges, "UNKNOWN", "honorIDMSListChanges should be unknown")
        }
    }

    func testJoinWithEnforceIDMSListNotSetBehavior() throws {
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        let containerC = try Container(name: ContainerName(container: "c", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        containerA.moc.performAndWait {
            XCTAssertEqual(containerA.containerMO.honorIDMSListChanges, "UNKNOWN", "honorIDMSListChanges should be unknown")
        }
        containerB.moc.performAndWait {
            XCTAssertEqual(containerB.containerMO.honorIDMSListChanges, "UNKNOWN", "honorIDMSListChanges should be unknown")
        }
        containerC.moc.performAndWait {
            XCTAssertEqual(containerC.containerMO.honorIDMSListChanges, "UNKNOWN", "honorIDMSListChanges should be unknown")
        }

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, aPolicy, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        XCTAssertNotNil(aPolicy, "Should have a policy coming back from a successful prepare")
        XCTAssertEqual(aPolicy?.version, prevailingPolicyVersion, "Policy coming back from prepare() should be prevailing policy version")

        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertTrue( state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error)
        XCTAssertNotNil(aPeerID)
        XCTAssertNotNil(aPermanentInfo)
        XCTAssertNotNil(aPermanentInfoSig)

        print("establishing A")
        do {
            let (peerID, _, _, error) = containerA.establishSync(test: self, ckksKeys: [], tlkShares: [], preapprovedKeys: [])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
        }

        print("preparing B")
        let (bPeerID, bPermanentInfo, bPermanentInfoSig, bStableInfo, bStableInfoSig, _, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertTrue( state.bottles.contains { $0.peerID == bPeerID }, "should have a bottle for peer")
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error2)
        XCTAssertNotNil(bPeerID)
        XCTAssertNotNil(bPermanentInfo)
        XCTAssertNotNil(bPermanentInfoSig)

        do {
            assertNoTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
            print("A vouches for B, but doesn't provide any TLKShares")
            let (_, _, errorVouchingWithoutTLKs) =
                containerA.vouchSync(test: self,
                                     peerID: bPeerID!,
                                     permanentInfo: bPermanentInfo!,
                                     permanentInfoSig: bPermanentInfoSig!,
                                     stableInfo: bStableInfo!,
                                     stableInfoSig: bStableInfoSig!,
                                     ckksKeys: [],
                                     altDSID: nil,
                                     flowID: nil,
                                     deviceSessionID: nil,
                                     canSendMetrics: false)
            XCTAssertNil(errorVouchingWithoutTLKs, "Should be no error vouching without uploading TLKShares")
            assertNoTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("A vouches for B, but doesn't only has provisional TLKs at the time")
            let provisionalManateeKeySet = try self.makeFakeKeyHierarchy(zoneID: CKRecordZone.ID(zoneName: "Manatee"))
            provisionalManateeKeySet.newUpload = true

            let (_, _, errorVouchingWithProvisionalTLKs) =
                containerA.vouchSync(test: self,
                                     peerID: bPeerID!,
                                     permanentInfo: bPermanentInfo!,
                                     permanentInfoSig: bPermanentInfoSig!,
                                     stableInfo: bStableInfo!,
                                     stableInfoSig: bStableInfoSig!,
                                     ckksKeys: [provisionalManateeKeySet],
                                     altDSID: nil,
                                     flowID: nil,
                                     deviceSessionID: nil,
                                     canSendMetrics: false)
            XCTAssertNil(errorVouchingWithProvisionalTLKs, "Should be no error vouching without uploading TLKShares for a non-existent key")
            assertNoTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("A vouches for B")
            let (voucherData, voucherSig, error3) =
                containerA.vouchSync(test: self,
                                     peerID: bPeerID!,
                                     permanentInfo: bPermanentInfo!,
                                     permanentInfoSig: bPermanentInfoSig!,
                                     stableInfo: bStableInfo!,
                                     stableInfoSig: bStableInfoSig!,
                                     ckksKeys: [self.manateeKeySet],
                                     altDSID: nil,
                                     flowID: nil,
                                     deviceSessionID: nil,
                                     canSendMetrics: false)
            XCTAssertNil(error3)
            XCTAssertNotNil(voucherData)
            XCTAssertNotNil(voucherSig)

            // As part of the vouch, A should have uploaded a tlkshare for B
            assertTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("B joins")
            let (peerID, _, _, error) = containerB.joinSync(test: self,
                                                            voucherData: voucherData!,
                                                            voucherSig: voucherSig!,
                                                            ckksKeys: [],
                                                            tlkShares: [],
                                                            altDSID: nil,
                                                            flowID: nil,
                                                            deviceSessionID: nil,
                                                            canSendMetrics: false)
            XCTAssertNil(error)
            XCTAssertEqual(peerID, bPeerID!)
        }

        _ = containerA.dumpSync(test: self)
        _ = containerB.dumpSync(test: self)
        _ = containerC.dumpSync(test: self)

        print("preparing C")
        let (cPeerID, cPermanentInfo, cPermanentInfoSig, cStableInfo, cStableInfoSig, _, error4) =
            containerC.prepareSync(test: self, epoch: 1, machineID: "ccc", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerC.getStateSync(test: self)
            XCTAssertTrue( state.bottles.contains { $0.peerID == cPeerID }, "should have a bottle for peer")
            let secret = containerC.loadSecretSync(test: self, label: cPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error4)
        XCTAssertNotNil(cPeerID)
        XCTAssertNotNil(cPermanentInfo)
        XCTAssertNotNil(cPermanentInfoSig)

        do {
            // C, when it joins, will create a new CKKS zone. It should also upload TLKShares for A and B.
            let provisionalEngramKeySet = try self.makeFakeKeyHierarchy(zoneID: CKRecordZone.ID(zoneName: "Engram"))
            provisionalEngramKeySet.newUpload = true

            assertNoTLKShareFor(peerID: cPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
            assertNoTLKShareFor(peerID: cPeerID!, keyUUID: provisionalEngramKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Engram"))

            print("B vouches for C")
            let (voucherData, voucherSig, error) =
                containerB.vouchSync(test: self,
                                     peerID: cPeerID!,
                                     permanentInfo: cPermanentInfo!,
                                     permanentInfoSig: cPermanentInfoSig!,
                                     stableInfo: cStableInfo!,
                                     stableInfoSig: cStableInfoSig!,
                                     ckksKeys: [self.manateeKeySet],
                                     altDSID: nil,
                                     flowID: nil,
                                     deviceSessionID: nil,
                                     canSendMetrics: false)
            XCTAssertNil(error)
            XCTAssertNotNil(voucherData)
            XCTAssertNotNil(voucherSig)

            assertTLKShareFor(peerID: cPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("C joins")
            let (peerID, _, _, error2) = containerC.joinSync(test: self,
                                                             voucherData: voucherData!,
                                                             voucherSig: voucherSig!,
                                                             ckksKeys: [self.manateeKeySet, provisionalEngramKeySet],
                                                             tlkShares: [],
                                                             altDSID: nil,
                                                             flowID: nil,
                                                             deviceSessionID: nil,
                                                             canSendMetrics: false)
            XCTAssertNil(error2)
            XCTAssertEqual(peerID, cPeerID!)

            assertTLKShareFor(peerID: cPeerID!, keyUUID: provisionalEngramKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Engram"))
            assertTLKShareFor(peerID: aPeerID!, keyUUID: provisionalEngramKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Engram"))
            assertTLKShareFor(peerID: bPeerID!, keyUUID: provisionalEngramKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Engram"))
        }

        print("A updates")
        do {
            let (_, _, error) = containerA.updateSync(test: self)
            XCTAssertNil(error)
        }

        do {
            let state = containerA.getStateSync(test: self)
            let a = state.peers[aPeerID!]!
            XCTAssertTrue(a.dynamicInfo!.includedPeerIDs.contains(cPeerID!))
        }

        _ = containerA.dumpSync(test: self)
        _ = containerB.dumpSync(test: self)
        _ = containerC.dumpSync(test: self)

        containerA.moc.performAndWait {
            XCTAssertEqual(containerA.containerMO.honorIDMSListChanges, "UNKNOWN", "honorIDMSListChanges should be unknown")
        }
        containerB.moc.performAndWait {
            XCTAssertEqual(containerB.containerMO.honorIDMSListChanges, "UNKNOWN", "honorIDMSListChanges should be unknown")
        }
        containerC.moc.performAndWait {
            XCTAssertEqual(containerC.containerMO.honorIDMSListChanges, "UNKNOWN", "honorIDMSListChanges should be unknown")
        }
    }

    func testPreApprovedJoinWithEnforceIDMSListNotSetBehavior() throws {
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        containerA.moc.performAndWait {
            XCTAssertEqual(containerA.containerMO.honorIDMSListChanges, "UNKNOWN", "honorIDMSListChanges should be unknown")
        }
        containerB.moc.performAndWait {
            XCTAssertEqual(containerB.containerMO.honorIDMSListChanges, "UNKNOWN", "honorIDMSListChanges should be unknown")
        }

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, aPolicy, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        XCTAssertNil(error)
        XCTAssertNotNil(aPeerID)
        XCTAssertNotNil(aPermanentInfo)
        XCTAssertNotNil(aPermanentInfoSig)

        XCTAssertNotNil(aPolicy, "Should have a policy coming back from a successful prepare")
        XCTAssertEqual(aPolicy?.version, prevailingPolicyVersion, "Policy coming back from prepare() should be prevailing policy version")

        print("preparing B")
        let (bPeerID, bPermanentInfo, bPermanentInfoSig, _, _, _, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertTrue( state.bottles.contains { $0.peerID == bPeerID }, "should have a bottle for peer")
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error2)
        XCTAssertNotNil(bPeerID)
        XCTAssertNotNil(bPermanentInfo)
        XCTAssertNotNil(bPermanentInfoSig)

        // Now, A establishes preapproving B
        // Note: secd is responsible for passing in TLKShares to these preapproved keys in sosTLKShares

        let bPermanentInfoParsed = TPPeerPermanentInfo(peerID: bPeerID!, data: bPermanentInfo!, sig: bPermanentInfoSig!, keyFactory: TPECPublicKeyFactory())
        XCTAssertNotNil(bPermanentInfoParsed, "Should have parsed B's permanent info")

        print("establishing A")
        do {
            let (peerID, _, _, error) = containerA.establishSync(test: self, ckksKeys: [self.manateeKeySet], tlkShares: [], preapprovedKeys: [bPermanentInfoParsed!.signingPubKey.spki()])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
        }

        do {
            assertNoTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("B joins by preapproval, and uploads all TLKShares that it has")
            let (bJoinedPeerID, _, policy, bJoinedError) = containerB.preapprovedJoinSync(test: self, ckksKeys: [self.manateeKeySet], tlkShares: [])
            XCTAssertNil(bJoinedError, "Should be no error joining by preapproval")
            XCTAssertNotNil(bJoinedPeerID, "Should have a peer ID out of join")
            XCTAssertNotNil(policy, "Should have a policy back from preapprovedjoin")

            assertTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
        }

        _ = containerA.dumpSync(test: self)
        _ = containerB.dumpSync(test: self)

        containerA.moc.performAndWait {
            XCTAssertEqual(containerA.containerMO.honorIDMSListChanges, "UNKNOWN", "honorIDMSListChanges should be unknown")
        }
        containerB.moc.performAndWait {
            XCTAssertEqual(containerB.containerMO.honorIDMSListChanges, "UNKNOWN", "honorIDMSListChanges should be unknown")
        }
    }

    func testReloadingContainerIDMSListVariable() throws {
        let store = tmpStoreDescription(name: "container.db")
        let contextID = "contextID"
        var container = try Container(name: ContainerName(container: "test", context: contextID), persistentStoreDescription: store, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let (peerID, permanentInfo, permanentInfoSig, _, _, _, error) = container.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = container.getStateSync(test: self)
            XCTAssertTrue( state.bottles.contains { $0.peerID == peerID }, "should have a bottle for peer")
            let secret = container.loadSecretSync(test: self, label: peerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNotNil(peerID)
        XCTAssertNotNil(permanentInfo)
        XCTAssertNotNil(permanentInfoSig)
        XCTAssertNil(error)

        container.moc.performAndWait {
            XCTAssertEqual(container.containerMO.honorIDMSListChanges, "UNKNOWN", "honorIDMSListChanges should be unknown")
        }

        _ = container.dumpSync(test: self)

        do {
            container = try Container(name: ContainerName(container: "test", context: contextID), persistentStoreDescription: store, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
            container.moc.performAndWait {
                XCTAssertEqual(container.containerMO.honorIDMSListChanges, "UNKNOWN", "honorIDMSListChanges should be unknown")
            }
        } catch {
            XCTFail("Creating container errored: \(error)")
        }

        let (peerID2, _, _, error2) = container.establishSync(test: self, ckksKeys: [self.manateeKeySet], tlkShares: [], preapprovedKeys: [])
        XCTAssertNil(error2)
        XCTAssertNotNil(peerID2)

        _ = container.dumpSync(test: self)
    }

    func testCorruptTPHDB() throws {
        let store = tmpStoreDescription(name: "container.db")
        let url = store.url as NSURL?
        var container = try Container(name: ContainerName(container: "test", context: "contextID"), persistentStoreDescription: store, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        XCTAssertNotNil(container, "container should not be nil")

        let (peerID, permanentInfo, permanentInfoSig, _, _, _, error) = container.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = container.getStateSync(test: self)
            XCTAssertTrue( state.bottles.contains { $0.peerID == peerID }, "should have a bottle for peer")
            let secret = container.loadSecretSync(test: self, label: peerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNotNil(peerID)
        XCTAssertNotNil(permanentInfo)
        XCTAssertNotNil(permanentInfoSig)
        XCTAssertNil(error)

        container.moc.performAndWait {
            XCTAssertEqual(container.containerMO.honorIDMSListChanges, "UNKNOWN", "honorIDMSListChanges should be unknown")
        }

        _ = container.dumpSync(test: self)

        // now corrupt the DB
        container.moc.performAndWait {
            let baseFilename = url!.lastPathComponent
            let fileNames = [baseFilename!]

            for filename in fileNames {
                let newURL = url!.deletingLastPathComponent

                let fileURL = newURL!.appendingPathComponent(filename)
                let data: Data = (NSData(contentsOf: fileURL))! as Data
                let tempData: NSMutableData = NSMutableData(data: data)

                let badData = "bad".data(using: String.Encoding.utf8)! as Data

                do {
                    try badData.withUnsafeBytes { (badDataBytes: UnsafeRawBufferPointer) throws -> Void in
                        tempData.replaceBytes(in: NSRange(location: 100, length: badData.count), withBytes: badDataBytes.baseAddress!)
                    }
                } catch {
                    XCTFail("Failed to write to overwrite data")
                }
                do {
                    try tempData.write(to: fileURL, options: [])
                } catch {
                    XCTFail("Failed to write to \(fileURL)")
                }
            }
        }

        // now load the DB again
        container = try Container(name: ContainerName(container: "test", context: "contextID"), persistentStoreDescription: store, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        XCTAssertNotNil(container, "container should not be nil")

        let (peerID2, permanentInfo2, permanentInfoSig2, _, _, _, error2) = container.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "987654321", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = container.getStateSync(test: self)
            XCTAssertTrue( state.bottles.contains { $0.peerID == peerID2 }, "should have a bottle for peer")
            let secret2 = container.loadSecretSync(test: self, label: peerID2!)
            XCTAssertNotNil(secret2, "secret should not be nil")
            XCTAssertNil(error2, "error should be nil")
        }
        XCTAssertNotNil(peerID2)
        XCTAssertNotNil(permanentInfo2)
        XCTAssertNotNil(permanentInfoSig2)
        XCTAssertNil(error2)

        container.moc.performAndWait {
            XCTAssertEqual(container.containerMO.honorIDMSListChanges, "UNKNOWN", "honorIDMSListChanges should be unknown")
        }

        _ = container.dumpSync(test: self)
    }

    func testLockedClassAForHmacKey() throws {
        let store = tmpStoreDescription(name: "container-\(UUID().uuidString).db")
        self.cuttlefish = FakeCuttlefishServer(nil, ckZones: [:], ckksZoneKeys: [:])

        var (container, _) = try establish(reload: false, allowedMachineIDs: Set(["aaa"]), store: store)
        XCTAssertNotNil(container, "container should not be nil")

        _ = container.dumpSync(test: self)

        let beforeHmacSigs: Set<Data> = container.moc.performAndWait {
            guard let peers = container.containerMO.peers as? Set<PeerMO> else {
                XCTFail("peers should not be nil")
                return Set()
            }
            return Set(peers.compactMap { $0.hmacSig })
        }

        XCTAssertNotEqual(beforeHmacSigs.count, 0)

        SecMockAKS.lockClassA()
        defer { SecMockAKS.reset() }

        // now load the DB again
        container = try Container(name: ContainerName(container: "test", context: OTDefaultContext), persistentStoreDescription: store, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        XCTAssertNotNil(container, "container should not be nil")

        _ = container.dumpSync(test: self)

        let afterHmacSigs: Set<Data> = container.moc.performAndWait {
            guard let peers = container.containerMO.peers as? Set<PeerMO> else {
                XCTFail("peers should not be nil")
                return Set()
            }
            return Set(peers.compactMap { $0.hmacSig })
        }

        XCTAssertEqual(beforeHmacSigs.count, afterHmacSigs.count)
        XCTAssertEqual(0, beforeHmacSigs.intersection(afterHmacSigs).count)
    }

    func testMemoryUseWithManyPeers() throws {
        let totalMIDs = 3 // When testing manually, set this to something higher, e.g. 10, to increase memory usage
        let joiningMIDs = (0...totalMIDs).map {
            "mid\($0)"
        }

        let options = XCTMeasureOptions()
        // Note that XCT will always do one more iteration than requested
        // When testing manually, set this to something higher, e.g. 10, to run more iterations to gather more data
        options.iterationCount = 1
        self.measure(metrics: [XCTMemoryMetric()], options: options) {
            do {
                let store = tmpStoreDescription(name: "container-\(UUID().uuidString).db")
                self.cuttlefish = FakeCuttlefishServer(nil, ckZones: [:], ckksZoneKeys: [:])

                let (c, peerID1) = try establish(reload: false,
                                                 allowedMachineIDs: Set(["aaa"] + joiningMIDs),
                                                 store: store)

                let joinedContainers = try joiningMIDs.map { mid in
                    return try joinByVoucher(sponsor: c,
                                             containerID: mid,
                                             machineID: mid,
                                             machineIDs: Set(["aaa"] + joiningMIDs), accountIsDemo: false,
                                             store: store)
                }
                let allPeerIDs = [peerID1] + joinedContainers.map { $1 }

                let (_, _, cUpdateError) = c.updateSync(test: self)
                XCTAssertNil(cUpdateError, "Should be able to update first container")

                self.assertTrusts(context: c, peerIDs: allPeerIDs)

                joinedContainers.forEach { container, _ in
                    let (_, _, updateError) = container.updateSync(test: self)
                    XCTAssertNil(updateError, "Should be able to update second container")

                    self.assertTrusts(context: container, peerIDs: allPeerIDs)
                }
            } catch {
                XCTFail("Expected no failures: \(error)")
            }
        }
    }

    func testMemoryUseLoadingManyPeers() throws {
        // When testing manually, set these to something higher, e.g. 49 & 9, to increase memory usage
        let additionalPeerCount = 5 // not including initial peer
        let remainingPeerCount = 3 // how many of the additional peers will be trusted at the end
        let joiningMIDs = (0..<additionalPeerCount).map {
            "mid\($0)"
        }

        let store = tmpStoreDescription(name: "container-\(UUID().uuidString).db")
        self.cuttlefish = FakeCuttlefishServer(nil, ckZones: [:], ckksZoneKeys: [:])

        let (c, peerID1) = try establish(reload: false,
                                         allowedMachineIDs: Set(["aaa"] + joiningMIDs),
                                         store: store)

        let joinedContainers = try joiningMIDs.map { mid in
            return try joinByVoucher(sponsor: c,
                                     containerID: mid,
                                     machineID: mid,
                                     machineIDs: Set(["aaa"] + joiningMIDs), accountIsDemo: false,
                                     store: store)
        }

        let allPeerIDs: [String] = [peerID1] + (joinedContainers.map { $1 })

        do {
            let (_, _, cUpdateError) = c.updateSync(test: self)
            XCTAssertNil(cUpdateError, "Should be able to update first container")
            self.assertTrusts(context: c, peerIDs: allPeerIDs)
        }

        joinedContainers.forEach { container, _ in
            let (_, _, updateError) = container.updateSync(test: self)
            XCTAssertNil(updateError, "Should be able to update second container")

            self.assertTrusts(context: container, peerIDs: allPeerIDs)
        }

        // Now half of the containers are thrown out
        let newIDList = Set(["aaa"] + joiningMIDs[0..<remainingPeerCount])
        let newTrustedList = [peerID1] + joinedContainers[0..<remainingPeerCount].map { $1 }
        let devicesRemovedList = Set(joiningMIDs).subtracting(newIDList)

        joinedContainers[0..<remainingPeerCount].forEach { joinedContainer, _ in
            XCTAssertNil(joinedContainer.setAllowedMachineIDsSync(test: self,
                                                                  allowedMachineIDs: newIDList,
                                                                  userInitiatedRemovals: devicesRemovedList,
                                                                  accountIsDemo: false), "should be able to set allowed machine IDs")

            let (_, _, joinedContainerUpdateError) = joinedContainer.updateSync(test: self)
            XCTAssertNil(joinedContainerUpdateError, "Should be able to update joined container")
            self.assertTrusts(context: joinedContainer, peerIDs: newTrustedList)
        }

        do {
            let (_, _, cUpdateError) = c.updateSync(test: self)
            XCTAssertNil(cUpdateError, "Should be able to update first container")

            self.assertTrusts(context: c, peerIDs: newTrustedList)
        }

        let options = XCTMeasureOptions()
        // Note that XCT will always do one more iteration than requested
        // When testing manually, set this to something higher, e.g. 25, to run more iterations to gather more data
        options.iterationCount = 1
        options.invocationOptions = .manuallyStop // so we can measure memory before our container goes out of scope
        self.measure(metrics: [XCTCPUMetric(), XCTMemoryMetric()], options: options) {
            let (_, _, updateError) = c.updateSync(test: self)
            XCTAssertNil(updateError, "Should be able to update first container")

            // reload: to simulate memory use when the daemon restarts
            do {
                let container = try Container(name: c.name,
                                              persistentStoreDescription: store,
                                              darwinNotifier: FakeCKKSNotifier.self,
                                              managedConfigurationAdapter: mcAdapterPlaceholder,
                                              cuttlefish: cuttlefish)
                self.assertTrusts(context: container, peerIDs: newTrustedList)
                self.stopMeasuring()
            } catch {
                XCTFail("Expected no failures: \(error)")
            }
        }
    }

    #if SEC_XR
    func testFetchCurrentPolicyWithModelIDOverride() throws {
        let store = tmpStoreDescription(name: "container.db")
        let container = try Container(name: ContainerName(container: "test", context: "contextID"), persistentStoreDescription: store, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        TPSetBecomeiPadOverride(false)
        do {
            let (policy, _, policyError) = container.fetchCurrentPolicySync(test: self, modelIDOverride: "iCycle1,1")
            XCTAssertNil(policy, "Should not have some policy with FF not set")
            XCTAssertNotNil(policyError, "Should have an error fetching policy with FF not set")
        }

        TPSetBecomeiPadOverride(true)
        do {
            let (policy, _, policyError) = container.fetchCurrentPolicySync(test: self, modelIDOverride: "iProd1,1")
            XCTAssertNotNil(policy, "Should have some policy")
            XCTAssertNil(policyError, "Should have no error fetching policy")
        }

        do {
            let (policy, _, policyError) = container.fetchCurrentPolicySync(test: self, modelIDOverride: "iCycle1,1")
            XCTAssertNotNil(policy, "Should have some policy")
            XCTAssertNil(policyError, "Should have no error fetching policy")
        }

        TPSetBecomeiPadOverride(false)
    }
    #endif // SEC_XR

    func testUpdateTLKsEmpty() throws {
        let description = tmpStoreDescription(name: "container.db")
        let (container, _) = try establish(reload: true, store: description)

        container.updateTLKs(ckksKeys: [], tlkShares: []) { ckrecords, error in
            XCTAssertNotNil(ckrecords, "ckrecord should not be nil")
            XCTAssertEqual(ckrecords!.count, 0, "should have no return records")
            XCTAssertNil(error, "should have no error")
        }
    }

    func testUpdateTLKsManatee() throws {
        let description = tmpStoreDescription(name: "container.db")
        let (container, _) = try establish(reload: true, store: description)

        container.updateTLKs(ckksKeys: [self.manateeKeySet], tlkShares: []) { ckrecords, error in
            XCTAssertNotNil(ckrecords, "ckrecord should not be nil")
            XCTAssertEqual(ckrecords!.count, 1, "should have one return record")
            XCTAssertNil(error, "should have no error")
        }
    }

    func testUpdateTLKsTen() throws {
        let description = tmpStoreDescription(name: "container.db")
        let (container, _) = try establish(reload: true, store: description)

        let keySet = try (1...10).map { try self.makeFakeKeyHierarchy(zoneID: CKRecordZone.ID(zoneName: "\($0)")) }
        (1...10).forEach { self.cuttlefish.fakeCKZones[CKRecordZone.ID(zoneName: "\($0)")] = FakeCKZone(zone: CKRecordZone.ID(zoneName: "\($0)")) }
        container.updateTLKs(ckksKeys: keySet, tlkShares: []) { ckrecords, error in
            XCTAssertNotNil(ckrecords, "ckrecord should not be nil")
            XCTAssertEqual(ckrecords!.count, 10, "should have ten return records")
            XCTAssertNil(error, "should have no error")
        }
    }

    func testUpdateTLKsHundred() throws {
        let description = tmpStoreDescription(name: "container.db")
        let (container, _) = try establish(reload: true, store: description)

        let keySet = try (1...100).map { try self.makeFakeKeyHierarchy(zoneID: CKRecordZone.ID(zoneName: "\($0)")) }
        (1...100).forEach { self.cuttlefish.fakeCKZones[CKRecordZone.ID(zoneName: "\($0)")] = FakeCKZone(zone: CKRecordZone.ID(zoneName: "\($0)")) }

        container.tlkSharesBatch = 10

        var n = 0
        let updateTrustExpectation = self.expectation(description: "waiting on updateTrust")
        self.cuttlefish.updateListener = { request in
            XCTAssertEqual(10, request.tlkShares.count, "expect 10 TLKshares")
            n += 1
            if n == 10 {
                self.cuttlefish.updateListener = nil
                updateTrustExpectation.fulfill()
            }
            return nil
        }

        container.updateTLKs(ckksKeys: keySet, tlkShares: []) { ckrecords, error in
            XCTAssertNotNil(ckrecords, "ckrecord should not be nil")
            XCTAssertEqual(ckrecords!.count, 100, "should have hundred return records")
            XCTAssertNil(error, "should have no error")
        }

        self.wait(for: [updateTrustExpectation], timeout: 10)
    }

    func testUpdateTLKsHundredFailed() throws {
        let description = tmpStoreDescription(name: "container.db")
        let (container, _) = try establish(reload: true, store: description)

        let keySet = try (1...100).map { try self.makeFakeKeyHierarchy(zoneID: CKRecordZone.ID(zoneName: "\($0)")) }
        (1...100).forEach { self.cuttlefish.fakeCKZones[CKRecordZone.ID(zoneName: "\($0)")] = FakeCKZone(zone: CKRecordZone.ID(zoneName: "\($0)")) }

        container.tlkSharesBatch = 10

        var n = 0
        let updateTrustExpectation = self.expectation(description: "waiting on updateTrust")
        self.cuttlefish.updateListener = { _ in
            if n == 0 {
                n += 1
                updateTrustExpectation.fulfill()
                return FakeCuttlefishServer.makeCloudKitCuttlefishError(code: .malformedRecord)
            }
            XCTFail("should have no more calls")
            return nil
        }

        container.updateTLKs(ckksKeys: keySet, tlkShares: []) { _, error in
            XCTAssertNotNil(error, "should have an error")
        }
        self.wait(for: [updateTrustExpectation], timeout: 10)
    }

    func testEvictedMachineIDs() throws {
        let description = tmpStoreDescription(name: "container.db")
        let container = try Container(name: ContainerName(container: "test", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let (peerID, permanentInfo, permanentInfoSig, _, _, _, error) = container.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")

        XCTAssertNil(error)
        XCTAssertNotNil(peerID)
        XCTAssertNotNil(permanentInfo)
        XCTAssertNotNil(permanentInfoSig)

        try self.assert(container: container, allowedMachineIDs: [], disallowedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ccc"], accountIsDemo: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // now evict A
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], evictedRemovals: ["aaa"], accountIsDemo: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: [], evictedMachineIDs: ["aaa"], persistentStore: description, cuttlefish: self.cuttlefish)

        // evict A again
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], evictedRemovals: ["aaa"], accountIsDemo: false, listDifference: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: [], evictedMachineIDs: ["aaa"], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            knownMachineMOs.forEach {
                if $0.machineID == "aaa" {
                    $0.modified = Date(timeIntervalSinceNow: TimeInterval(twoDaysInThePast))
                }
            }

            try! container.moc.save()
        }
        XCTAssertTrue(container.fullIDMSListWouldBeHelpful(), "Container should think it could use an IDMS list set")

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], evictedRemovals: ["aaa"], accountIsDemo: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: ["aaa"], evictedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ccc"], evictedRemovals: [], accountIsDemo: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: [], evictedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)

        // Oh no! A is on both allowed and evicted
        // Reverse expectations when IdMS lands changes to not include peers in both lists.
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ccc"], evictedRemovals: ["aaa"], accountIsDemo: false, listDifference: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: [], evictedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)

        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            knownMachineMOs.forEach {
                if $0.machineID == "aaa" {
                    $0.modified = Date(timeIntervalSinceNow: TimeInterval(twoDaysInThePast))
                }
            }

            try! container.moc.save()
        }
        // A is not called out in any list and was last modified 2 days ago.  A should now get tagged as ghosted
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], accountIsDemo: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: [], evictedMachineIDs: [], ghostedMachineIDs: Set(["aaa"]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // A is ghosted and hasn't been modified in 2 days
        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            knownMachineMOs.forEach {
                if $0.machineID == "aaa" {
                    $0.modified = Date(timeIntervalSinceNow: TimeInterval(twoDaysInThePast))
                }
            }

            try! container.moc.save()
        }
        XCTAssertTrue(container.fullIDMSListWouldBeHelpful(), "Container should think it could use an IDMS list set")

        // setting the same TDL should result in aaa being disallowed
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], accountIsDemo: false), "should be able to set allowed machine IDs")

        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: Set(["aaa"]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")
    }

    func testEvictionDispositions() throws {
        var bottleA: ContainerState.Bottle
        var entropy: Data
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa", "bbb"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, _, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            var state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")

            bottleA = state.bottles.removeFirst()

            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNotNil(aPeerID)
        XCTAssertNotNil(aPermanentInfo)
        XCTAssertNotNil(aPermanentInfoSig)

        print("establishing A")
        do {
            let cliqueChangedNotificationExpectation = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: OTCliqueChanged))

            assertNoTLKShareFor(peerID: aPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
            let (peerID, _, _, error) = containerA.establishSync(test: self, ckksKeys: [self.manateeKeySet], tlkShares: [], preapprovedKeys: [])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
            assertTLKShareFor(peerID: aPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            self.wait(for: [cliqueChangedNotificationExpectation], timeout: 1)
        }
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            entropy = secret!
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }

        _ = containerB.updateSync(test: self)

        print("preparing B")
        let (bPeerID, _, _, _, _, _, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == bPeerID }, "should have a bottle for peer")
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error2)

        do {
            print("B prepares to join via bottle")

            let (bottlePeerID, policy, _, errorPreflight) = containerB.preflightVouchWithBottleSync(test: self, bottleID: bottleA.bottleID!)
            XCTAssertNil(errorPreflight, "Should be no error preflighting a vouch with bottle")
            XCTAssertEqual(bottlePeerID, aPeerID, "Bottle should be for peer A")
            XCTAssertNotNil(policy, "Should have a policy")

            let (voucherData, voucherSig, _, _, error3) = containerB.vouchWithBottleSync(test: self, b: bottleA.bottleID!, entropy: entropy, bottleSalt: "123456789", tlkShares: [])

            XCTAssertNil(error3)
            XCTAssertNotNil(voucherData)
            XCTAssertNotNil(voucherSig)

            // Before B joins, there should be no TLKShares for B
            assertNoTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            let cliqueChangedNotificationExpectation = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: OTCliqueChanged))

            print("B joins")
            let (peerID, _, _, error) = containerB.joinSync(test: self, voucherData: voucherData!, voucherSig: voucherSig!, ckksKeys: [self.manateeKeySet], tlkShares: [], altDSID: nil, flowID: nil, deviceSessionID: nil, canSendMetrics: false)
            XCTAssertNil(error)
            XCTAssertEqual(peerID, bPeerID!)

            self.wait(for: [cliqueChangedNotificationExpectation], timeout: 1)

            // But afterward, it has one!
            assertTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            // A is now evicted
            XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb"], evictedRemovals: ["aaa"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")

            _ = containerB.updateSync(test: self)
            _ = containerA.updateSync(test: self)

            containerB.moc.performAndWait {
                let peerMOs = containerB.containerMO.peers as? Set<PeerMO> ?? Set()
                let bbbPeerMOs = peerMOs.filter { $0.peerID == bPeerID }
                let bbbPeerMO = bbbPeerMOs.first!
                let bbbDynamicInfo = TPPeerDynamicInfo(data:  bbbPeerMO.dynamicInfo!, sig:  bbbPeerMO.dynamicInfoSig!)
                XCTAssertNotNil(bbbDynamicInfo, "dynamicInfo should not be nil")
                let bbbDispositions = bbbDynamicInfo?.dispositions
                XCTAssertTrue(bbbDispositions![aPeerID!]!.hasEvictedMachineID, "bbb should have an evicted machineID")
            }

            // A is now disallowed
            XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb"], userInitiatedRemovals:["aaa"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")

            _ = containerB.updateSync(test: self)
            _ = containerA.updateSync(test: self)

            containerB.moc.performAndWait {
                let peerMOs = containerB.containerMO.peers as? Set<PeerMO> ?? Set()
                let bbbPeerMOs = peerMOs.filter { $0.peerID == bPeerID }
                let bbbPeerMO = bbbPeerMOs.first!
                let bbbDynamicInfo = TPPeerDynamicInfo(data:  bbbPeerMO.dynamicInfo!, sig:  bbbPeerMO.dynamicInfoSig!)
                XCTAssertNotNil(bbbDynamicInfo, "dynamicInfo should not be nil")
                let bbbDispositions = bbbDynamicInfo?.dispositions
                XCTAssertTrue(bbbDispositions![aPeerID!]!.hasEvictedMachineID, "bbb should have an evicted machineID")
                XCTAssertTrue(bbbDispositions![aPeerID!]!.hasDisallowedMachineID, "bbb should have a disallowed machineID")
            }
        }
    }

    func testUserInitiatedRemovedMachineIDs() throws {
        let description = tmpStoreDescription(name: "container.db")
        let container = try Container(name: ContainerName(container: "test", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let (peerID, permanentInfo, permanentInfoSig, _, _, _, error) = container.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")

        XCTAssertNil(error)
        XCTAssertNotNil(peerID)
        XCTAssertNotNil(permanentInfo)
        XCTAssertNotNil(permanentInfoSig)

        try self.assert(container: container, allowedMachineIDs: [], disallowedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ccc"], accountIsDemo: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // now simulate user removing A
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], userInitiatedRemovals: ["aaa"], accountIsDemo: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: ["aaa"], persistentStore: description, cuttlefish: self.cuttlefish)

        // remove A again
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], userInitiatedRemovals: ["aaa"], accountIsDemo: false, listDifference: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: ["aaa"], persistentStore: description, cuttlefish: self.cuttlefish)

        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            knownMachineMOs.forEach {
                if $0.machineID == "aaa" {
                    $0.modified = Date(timeIntervalSinceNow: TimeInterval(twoDaysInThePast))
                }
            }

            try! container.moc.save()
        }
        
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], accountIsDemo: false, listDifference: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: ["aaa"], evictedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ccc"], evictedRemovals: [], accountIsDemo: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: [], evictedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)

        // Reverse expectations when IdMS lands changes to not include peers in both lists.  A should be disallowed in this case below
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ccc"], userInitiatedRemovals: ["aaa"], accountIsDemo: false, listDifference: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)

        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            knownMachineMOs.forEach {
                if $0.machineID == "aaa" {
                    $0.modified = Date(timeIntervalSinceNow: TimeInterval(twoDaysInThePast))
                }
            }

            try! container.moc.save()
        }

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], userInitiatedRemovals: ["aaa"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: ["aaa"], evictedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)
    }

    func testUnknownRemovalDispositions() throws {
        var bottleA: ContainerState.Bottle
        var entropy: Data
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa", "bbb"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, _, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            var state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")

            bottleA = state.bottles.removeFirst()

            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNotNil(aPeerID)
        XCTAssertNotNil(aPermanentInfo)
        XCTAssertNotNil(aPermanentInfoSig)

        print("establishing A")
        do {
            let cliqueChangedNotificationExpectation = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: OTCliqueChanged))

            assertNoTLKShareFor(peerID: aPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
            let (peerID, _, _, error) = containerA.establishSync(test: self, ckksKeys: [self.manateeKeySet], tlkShares: [], preapprovedKeys: [])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
            assertTLKShareFor(peerID: aPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            self.wait(for: [cliqueChangedNotificationExpectation], timeout: 1)
        }
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            entropy = secret!
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }

        _ = containerB.updateSync(test: self)

        print("preparing B")
        let (bPeerID, _, _, _, _, _, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == bPeerID }, "should have a bottle for peer")
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error2)

        do {
            print("B prepares to join via bottle")

            let (bottlePeerID, policy, _, errorPreflight) = containerB.preflightVouchWithBottleSync(test: self, bottleID: bottleA.bottleID!)
            XCTAssertNil(errorPreflight, "Should be no error preflighting a vouch with bottle")
            XCTAssertEqual(bottlePeerID, aPeerID, "Bottle should be for peer A")
            XCTAssertNotNil(policy, "Should have a policy")

            let (voucherData, voucherSig, _, _, error3) = containerB.vouchWithBottleSync(test: self, b: bottleA.bottleID!, entropy: entropy, bottleSalt: "123456789", tlkShares: [])

            XCTAssertNil(error3)
            XCTAssertNotNil(voucherData)
            XCTAssertNotNil(voucherSig)

            // Before B joins, there should be no TLKShares for B
            assertNoTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            let cliqueChangedNotificationExpectation = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: OTCliqueChanged))

            print("B joins")
            let (peerID, _, _, error) = containerB.joinSync(test: self, voucherData: voucherData!, voucherSig: voucherSig!, ckksKeys: [self.manateeKeySet], tlkShares: [], altDSID: nil, flowID: nil, deviceSessionID: nil, canSendMetrics: false)
            XCTAssertNil(error)
            XCTAssertEqual(peerID, bPeerID!)

            self.wait(for: [cliqueChangedNotificationExpectation], timeout: 1)

            // But afterward, it has one!
            assertTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            // A is now removed with unknown reason
            XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb"], unknownReasonRemovals: ["aaa"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")

            _ = containerB.updateSync(test: self)
            _ = containerA.updateSync(test: self)

            containerB.moc.performAndWait {
                let peerMOs = containerB.containerMO.peers as? Set<PeerMO> ?? Set()
                let bbbPeerMOs = peerMOs.filter { $0.peerID == bPeerID }
                let bbbPeerMO = bbbPeerMOs.first!
                let bbbDynamicInfo = TPPeerDynamicInfo(data:  bbbPeerMO.dynamicInfo!, sig:  bbbPeerMO.dynamicInfoSig!)
                XCTAssertNotNil(bbbDynamicInfo, "dynamicInfo should not be nil")
                let bbbDispositions = bbbDynamicInfo?.dispositions
                XCTAssertTrue(bbbDispositions![aPeerID!]!.hasUnknownReasonRemovalMachineID, "bbb should have an unknown reason removal machineID")
            }

            // A is now disallowed
            XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb"], userInitiatedRemovals:["aaa"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")

            _ = containerB.updateSync(test: self)
            _ = containerA.updateSync(test: self)

            containerB.moc.performAndWait {
                let peerMOs = containerB.containerMO.peers as? Set<PeerMO> ?? Set()
                let bbbPeerMOs = peerMOs.filter { $0.peerID == bPeerID }
                let bbbPeerMO = bbbPeerMOs.first!
                let bbbDynamicInfo = TPPeerDynamicInfo(data:  bbbPeerMO.dynamicInfo!, sig:  bbbPeerMO.dynamicInfoSig!)
                XCTAssertNotNil(bbbDynamicInfo, "dynamicInfo should not be nil")
                let bbbDispositions = bbbDynamicInfo?.dispositions
                XCTAssertTrue(bbbDispositions![aPeerID!]!.hasUnknownReasonRemovalMachineID, "bbb should have an unknown reason removal machineID")
                XCTAssertTrue(bbbDispositions![aPeerID!]!.hasDisallowedMachineID, "bbb should have a disallowed machineID")
            }
        }
    }

    func testUnknownReasonRemovedMachineIDs() throws {
        let description = tmpStoreDescription(name: "container.db")
        let container = try Container(name: ContainerName(container: "test", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let (peerID, permanentInfo, permanentInfoSig, _, _, _, error) = container.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")

        XCTAssertNil(error)
        XCTAssertNotNil(peerID)
        XCTAssertNotNil(permanentInfo)
        XCTAssertNotNil(permanentInfoSig)

        try self.assert(container: container, allowedMachineIDs: [], disallowedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ccc"], accountIsDemo: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // now A gets moved to the unknown reason for a removal list
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], unknownReasonRemovals: ["aaa"], accountIsDemo: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: Set(), unknownReasonMachineIDs: ["aaa"], persistentStore: description, cuttlefish: self.cuttlefish)

        // assert A is on the unknown reason list again
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], unknownReasonRemovals: ["aaa"], accountIsDemo: false, listDifference: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: [], unknownReasonMachineIDs: ["aaa"], persistentStore: description, cuttlefish: self.cuttlefish)

        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            knownMachineMOs.forEach {
                if $0.machineID == "aaa" {
                    $0.modified = Date(timeIntervalSinceNow: TimeInterval(twoDaysInThePast))
                }
            }

            try! container.moc.save()
        }
        XCTAssertTrue(container.fullIDMSListWouldBeHelpful(), "Container should think it could use an IDMS list set")

        // get an updated TDL from IdMS, A is not on any list
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")
        // A should now be disallowed
        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: ["aaa"], evictedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)

        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // A is back! It should now be trusted again
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ccc"], accountIsDemo: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)

        // oops, A is on the unknownReason list and allowed - A will still be allowed
        // Reverse expectations when IdMS lands changes to not include peers in both lists.  A should be disallowed in this case below
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ccc"], unknownReasonRemovals: ["aaa"], accountIsDemo: false, listDifference: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: [], unknownReasonMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], unknownReasonRemovals: ["aaa"], accountIsDemo: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: [], unknownReasonMachineIDs: ["aaa"], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            knownMachineMOs.forEach {
                if $0.machineID == "aaa" {
                    $0.modified = Date(timeIntervalSinceNow: TimeInterval(twoDaysInThePast))
                }
            }

            try! container.moc.save()
        }
        XCTAssertTrue(container.fullIDMSListWouldBeHelpful(), "Container should think it could use an IDMS list set")

        // Device gets a TDL update from IdMS, A is listed still as unknown but it's Date is 2 days ago. It should move to disallowed
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], unknownReasonRemovals: ["aaa"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")
        // A should now be disallowed
        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: ["aaa"], evictedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")
    }

    func testEvictedUserInitiatedAndUnknownReason () throws {
        let description = tmpStoreDescription(name: "container.db")
        let container = try Container(name: ContainerName(container: "test", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let (peerID, permanentInfo, permanentInfoSig, _, _, _, error) = container.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")

        XCTAssertNil(error)
        XCTAssertNotNil(peerID)
        XCTAssertNotNil(permanentInfo)
        XCTAssertNotNil(permanentInfoSig)

        try self.assert(container: container, allowedMachineIDs: [], disallowedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ccc"], userInitiatedRemovals: ["ddd"], evictedRemovals: ["eee"], unknownReasonRemovals: ["fff"], accountIsDemo: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: ["ddd"], evictedMachineIDs: ["eee"], unknownReasonMachineIDs: ["fff"], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // now A gets moved to the unknown reason for a removal list
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], unknownReasonRemovals: ["aaa"], accountIsDemo: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: Set(["ddd"]), evictedMachineIDs: ["eee"], unknownReasonMachineIDs: ["aaa", "fff"], persistentStore: description, cuttlefish: self.cuttlefish)

        // assert A is on the unknown reason list again
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], unknownReasonRemovals: ["aaa"], accountIsDemo: false, listDifference: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: ["ddd"], evictedMachineIDs: ["eee"], unknownReasonMachineIDs: ["aaa", "fff"], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            knownMachineMOs.forEach {
                if $0.machineID == "aaa" {
                    $0.modified = Date(timeIntervalSinceNow: TimeInterval(twoDaysInThePast))
                }
            }

            try! container.moc.save()
        }
        XCTAssertTrue(container.fullIDMSListWouldBeHelpful(), "Container should think it could use an IDMS list set")

        // get an updated TDL from IdMS, A is not on any list
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], accountIsDemo: false), "should be able to set allowed machine IDs")
        // A should now be disallowed
        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: ["aaa", "ddd"], evictedMachineIDs: ["eee"], unknownReasonMachineIDs: ["fff"], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // A is back! It should now be trusted again
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ccc"], accountIsDemo: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: Set(["ddd"]), evictedMachineIDs: ["eee"], unknownReasonMachineIDs: ["fff"], persistentStore: description, cuttlefish: self.cuttlefish)

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], unknownReasonRemovals: ["aaa"], accountIsDemo: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: Set(["ddd"]), evictedMachineIDs: ["eee"], unknownReasonMachineIDs: ["aaa", "fff"], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            knownMachineMOs.forEach {
                if $0.machineID == "aaa" {
                    $0.modified = Date(timeIntervalSinceNow: TimeInterval(twoDaysInThePast))
                }
            }

            try! container.moc.save()
        }
        // Device gets a TDL update from IdMS, A is listed still as unknown but it's Date is 2 days ago. It should move to disallowed
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], unknownReasonRemovals: ["aaa"], accountIsDemo: false), "should be able to set allowed machine IDs")
        // A should now be disallowed
        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: Set(["aaa", "ddd"]), evictedMachineIDs: ["eee"], unknownReasonMachineIDs: ["fff"], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")
    }

    func testPeerAddedToAllLists() throws {
        let description = tmpStoreDescription(name: "container.db")
        let container = try Container(name: ContainerName(container: "test", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let (peerID, permanentInfo, permanentInfoSig, _, _, _, error) = container.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")

        XCTAssertNil(error)
        XCTAssertNotNil(peerID)
        XCTAssertNotNil(permanentInfo)
        XCTAssertNotNil(permanentInfoSig)

        try self.assert(container: container, allowedMachineIDs: [], disallowedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ccc"], userInitiatedRemovals: ["aaa"], evictedRemovals: ["aaa"], unknownReasonRemovals: ["aaa"], accountIsDemo: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: [], evictedMachineIDs: [], unknownReasonMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")
    }

    func testGhostedMachineIDs() throws {
        let description = tmpStoreDescription(name: "container.db")
        let container = try Container(name: ContainerName(container: "test", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let (peerID, permanentInfo, permanentInfoSig, _, _, _, error) = container.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")

        XCTAssertNil(error)
        XCTAssertNotNil(peerID)
        XCTAssertNotNil(permanentInfo)
        XCTAssertNotNil(permanentInfoSig)

        try self.assert(container: container, allowedMachineIDs: [], disallowedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ccc"], userInitiatedRemovals: [], evictedRemovals: [], unknownReasonRemovals: [], accountIsDemo: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: [], evictedMachineIDs: [], unknownReasonMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")


        // now aaa falls off the list
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], userInitiatedRemovals: [], evictedRemovals: [], unknownReasonRemovals: [], accountIsDemo: false), "should be able to set allowed machine IDs")

        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: [], evictedMachineIDs: [], unknownReasonMachineIDs: [], ghostedMachineIDs: ["aaa"], persistentStore: description, cuttlefish: self.cuttlefish)

        // now 48 hours pass
        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            knownMachineMOs.forEach {
                if $0.machineID == "aaa" {
                    $0.modified = Date(timeIntervalSinceNow: TimeInterval(twoDaysInThePast))
                }
            }

            try! container.moc.save()
        }
        XCTAssertTrue(container.fullIDMSListWouldBeHelpful(), "Container should think it could use an IDMS list set")

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], userInitiatedRemovals: [], evictedRemovals: [], unknownReasonRemovals: [], accountIsDemo: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: ["aaa"], evictedMachineIDs: [], unknownReasonMachineIDs: [], ghostedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.fullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")
    }

    func testGhostDispositions() throws {
        var bottleA: ContainerState.Bottle
        var entropy: Data
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, darwinNotifier: FakeCKKSNotifier.self, managedConfigurationAdapter: mcAdapterPlaceholder, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa", "bbb"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs, accountIsDemo: false))

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, _, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            var state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")

            bottleA = state.bottles.removeFirst()

            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNotNil(aPeerID)
        XCTAssertNotNil(aPermanentInfo)
        XCTAssertNotNil(aPermanentInfoSig)

        print("establishing A")
        do {
            let cliqueChangedNotificationExpectation = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: OTCliqueChanged))

            assertNoTLKShareFor(peerID: aPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
            let (peerID, _, _, error) = containerA.establishSync(test: self, ckksKeys: [self.manateeKeySet], tlkShares: [], preapprovedKeys: [])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
            assertTLKShareFor(peerID: aPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            self.wait(for: [cliqueChangedNotificationExpectation], timeout: 1)
        }
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == aPeerID }, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            entropy = secret!
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }

        _ = containerB.updateSync(test: self)

        print("preparing B")
        let (bPeerID, _, _, _, _, _, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertTrue(state.bottles.contains { $0.peerID == bPeerID }, "should have a bottle for peer")
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error2)

        do {
            print("B prepares to join via bottle")

            let (bottlePeerID, policy, _, errorPreflight) = containerB.preflightVouchWithBottleSync(test: self, bottleID: bottleA.bottleID!)
            XCTAssertNil(errorPreflight, "Should be no error preflighting a vouch with bottle")
            XCTAssertEqual(bottlePeerID, aPeerID, "Bottle should be for peer A")
            XCTAssertNotNil(policy, "Should have a policy")

            let (voucherData, voucherSig, _, _, error3) = containerB.vouchWithBottleSync(test: self, b: bottleA.bottleID!, entropy: entropy, bottleSalt: "123456789", tlkShares: [])

            XCTAssertNil(error3)
            XCTAssertNotNil(voucherData)
            XCTAssertNotNil(voucherSig)

            // Before B joins, there should be no TLKShares for B
            assertNoTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            let cliqueChangedNotificationExpectation = XCTNSNotificationExpectation(name: NSNotification.Name(rawValue: OTCliqueChanged))

            print("B joins")
            let (peerID, _, _, error) = containerB.joinSync(test: self, voucherData: voucherData!, voucherSig: voucherSig!, ckksKeys: [self.manateeKeySet], tlkShares: [], altDSID: nil, flowID: nil, deviceSessionID: nil, canSendMetrics: false)
            XCTAssertNil(error)
            XCTAssertEqual(peerID, bPeerID!)

            self.wait(for: [cliqueChangedNotificationExpectation], timeout: 1)

            // But afterward, it has one!
            assertTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            // A has ghosted the TDL
            XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")

            _ = containerB.updateSync(test: self)
            _ = containerA.updateSync(test: self)

            containerB.moc.performAndWait {
                let peerMOs = containerB.containerMO.peers as? Set<PeerMO> ?? Set()
                let bbbPeerMOs = peerMOs.filter { $0.peerID == bPeerID }
                let bbbPeerMO = bbbPeerMOs.first!
                let bbbDynamicInfo = TPPeerDynamicInfo(data:  bbbPeerMO.dynamicInfo!, sig:  bbbPeerMO.dynamicInfoSig!)
                XCTAssertNotNil(bbbDynamicInfo, "dynamicInfo should not be nil")
                let bbbDispositions = bbbDynamicInfo?.dispositions
                XCTAssertTrue(bbbDispositions![aPeerID!]!.hasGhostedMachineID, "bbb should have a ghost machineID")
            }

            // A is now disallowed
            XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb"], userInitiatedRemovals:["aaa"], accountIsDemo: false, listDifference: true), "should be able to set allowed machine IDs")

            _ = containerB.updateSync(test: self)
            _ = containerA.updateSync(test: self)

            containerB.moc.performAndWait {
                let peerMOs = containerB.containerMO.peers as? Set<PeerMO> ?? Set()
                let bbbPeerMOs = peerMOs.filter { $0.peerID == bPeerID }
                let bbbPeerMO = bbbPeerMOs.first!
                let bbbDynamicInfo = TPPeerDynamicInfo(data:  bbbPeerMO.dynamicInfo!, sig:  bbbPeerMO.dynamicInfoSig!)
                XCTAssertNotNil(bbbDynamicInfo, "dynamicInfo should not be nil")
                let bbbDispositions = bbbDynamicInfo?.dispositions
                XCTAssertTrue(bbbDispositions![aPeerID!]!.hasGhostedMachineID, "bbb should have a ghost machineID")
                XCTAssertTrue(bbbDispositions![aPeerID!]!.hasDisallowedMachineID, "bbb should have a disallowed machineID")
            }
        }
    }
}
