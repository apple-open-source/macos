//
//  TrustedPeersHelperUnitTests.swift
//  TrustedPeersHelperUnitTests
//
//  Created by Ben Williamson on 5/1/18.
//

import CoreData
import XCTest

let testDSID = "123456789"

let signingKey_384 = Data(base64Encoded: "BOQbPoiBnzuA0Cgc2QegjKGJqDtpkRenHwAxkYKJH1xELdaoIh8ifSch8sl18tpBYVUpEfdxz2ZSKif+dx7UPfu8WeTtpHkqm3M+9PTjr/KNNJCSR1PQNB5Jh+sRiQ+cpJnoTzm+IZSIukylamAcL3eA0nMUM0Zc2u4TijrbTgVND22WzSirUkwSK3mA/prk9A==")

let encryptionKey_384 = Data(base64Encoded: "BE1RuazBWmSEx0XVGhobbrdSE6fRQOrUrYEQnBkGl4zJq9GCeRoYvbuWNYFcOH0ijCRz9pYILsTn3ajT1OknlvcKmuQ7SeoGWzk9cBZzT5bBEwozn2gZxn80DQoDkmejywlH3D0/cuV6Bxexu5KMAFGqg6eN6th4sQABL5EuI9zKPuxHStM/b9B1LyqcnRKQHA==")

let symmetricKey_384 = Data(base64Encoded: "MfHje3Y/mWV0q+grjwZ4VxuqB7OreYHLxYkeeCiNjjY=")

class TrustedPeersHelperUnitTests: XCTestCase {

    var tmpPath: String!
    var tmpURL: URL!
    var cuttlefish: FakeCuttlefishServer!

    var manateeKeySet: CKKSKeychainBackedKeySet!

    override static func setUp() {
        super.setUp()

        SecTapToRadar.disableTTRsEntirely()

        // Turn on NO_SERVER stuff
        securityd_init_local_spi()

        SecCKKSDisable()
    }

    override func setUp() {
        super.setUp()

        let testName = self.name.components(separatedBy: CharacterSet(charactersIn: " ]"))[1]
        cuttlefish = FakeCuttlefishServer(nil, ckZones: [:], ckksZoneKeys: [:])

        // Make a new fake keychain
        tmpPath = String(format: "/tmp/%@-%X", testName, arc4random())
        tmpURL = URL(fileURLWithPath: tmpPath, isDirectory: true)
        do {
            try FileManager.default.createDirectory(atPath: String(format: "%@/Library/Keychains", tmpPath), withIntermediateDirectories: true, attributes: nil)
            SetCustomHomeURLString(tmpPath as CFString)
            SecKeychainDbReset(nil)
        } catch {
            XCTFail("setUp failed: \(error)")
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
        cuttlefish = nil
        super.tearDown()
    }

    func makeFakeKeyHierarchy(zoneID: CKRecordZone.ID) throws -> CKKSKeychainBackedKeySet {
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
                   store: NSPersistentStoreDescription) throws -> (Container, String) {
        return try self.establish(reload: reload, contextID: OTDefaultContext, store: store)
    }

    func establish(reload: Bool,
                   contextID: String,
                   store: NSPersistentStoreDescription) throws -> (Container, String) {
        var container = try Container(name: ContainerName(container: "test", context: contextID), persistentStoreDescription: store, cuttlefish: cuttlefish)

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ccc"]), "should be able to set allowed machine IDs")

        let (peerID, permanentInfo, permanentInfoSig, _, _, error) = container.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = container.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == peerID } .isEmpty, "should have a bottle for peer")
            let secret = container.loadSecretSync(test: self, label: peerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNotNil(peerID)
        XCTAssertNotNil(permanentInfo)
        XCTAssertNotNil(permanentInfoSig)
        XCTAssertNil(error)

        _ = container.dumpSync(test: self)

        if (reload) {
            do {
                container = try Container(name: ContainerName(container: "test", context: contextID), persistentStoreDescription: store, cuttlefish: cuttlefish)
            } catch {
                XCTFail()
            }
        }

        let (peerID2, _, error2) = container.establishSync(test: self, ckksKeys: [self.manateeKeySet], tlkShares: [], preapprovedKeys: [])
        XCTAssertNil(error2)
        XCTAssertNotNil(peerID2)

        _ = container.dumpSync(test: self)

        return (container, peerID!)
    }

    func testEstablishWithReload() throws {
        let description = tmpStoreDescription(name: "container.db")
        let (_, peerID) = try establish(reload: true, store: description)

        assertTLKShareFor(peerID: peerID, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
    }

    func testEstablishNoReload() throws {
        let description = tmpStoreDescription(name: "container.db")
        _ = try establish(reload: false, store: description)
    }

    func testEstablishNotOnAllowListErrors() throws {
        let description = tmpStoreDescription(name: "container.db")
        let container = try Container(name: ContainerName(container: "test", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)

        let (peerID, permanentInfo, permanentInfoSig, _, _, error) = container.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = container.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == peerID } .isEmpty, "should have a bottle for peer")
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
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa"]), "should be able to set allowed machine IDs")

        let (peerID3, _, error3) = container.establishSync(test: self, ckksKeys: [], tlkShares: [], preapprovedKeys: [])
        XCTAssertNotNil(peerID3, "Should get a peer when you establish a now allow-listed peer")
        XCTAssertNil(error3, "Should not get an error when you establish a now allow-listed peer")
    }

    func joinByVoucher(sponsor: Container,
                       containerID: String,
                       machineID: String,
                       machineIDs: Set<String>,
                       store: NSPersistentStoreDescription) throws -> (Container, String) {
        let c = try Container(name: ContainerName(container: containerID, context: OTDefaultContext),
                              persistentStoreDescription: store,
                              cuttlefish: cuttlefish)

        XCTAssertNil(c.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs), "Should be able to set machine IDs")

        print("preparing \(containerID)")
        let (peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, error) =
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
                                  ckksKeys: [self.manateeKeySet])
            XCTAssertNil(vouchError)
            XCTAssertNotNil(voucherData)
            XCTAssertNotNil(voucherSig)

            // As part of the join, the sponsor should have uploaded a tlk share
            assertTLKShareFor(peerID: peerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("\(containerID) joins")
            let (joinedPeerID, _, joinError) = c.joinSync(test: self,
                                                       voucherData: voucherData!,
                                                       voucherSig: voucherSig!,
                                                       ckksKeys: [],
                                                       tlkShares: [])
            XCTAssertNil(joinError)
            XCTAssertEqual(joinedPeerID, peerID!)
        }

        return (c, peerID!)
    }

    func testJoin() throws {
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)
        let containerC = try Container(name: ContainerName(container: "c", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa", "bbb", "ccc"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs))
        XCTAssertNil(containerC.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs))

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == aPeerID } .isEmpty, "should have a bottle for peer")
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
            let (peerID, _, error) = containerA.establishSync(test: self, ckksKeys: [], tlkShares: [], preapprovedKeys: [])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
        }

        print("preparing B")
        let (bPeerID, bPermanentInfo, bPermanentInfoSig, bStableInfo, bStableInfoSig, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == bPeerID } .isEmpty, "should have a bottle for peer")
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
                                     ckksKeys: [])
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
                                     ckksKeys: [provisionalManateeKeySet])
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
                                     ckksKeys: [self.manateeKeySet])
            XCTAssertNil(error3)
            XCTAssertNotNil(voucherData)
            XCTAssertNotNil(voucherSig)

            // As part of the vouch, A should have uploaded a tlkshare for B
            assertTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("B joins")
            let (peerID, _, error) = containerB.joinSync(test: self,
                                                         voucherData: voucherData!,
                                                         voucherSig: voucherSig!,
                                                         ckksKeys: [],
                                                         tlkShares: [])
            XCTAssertNil(error)
            XCTAssertEqual(peerID, bPeerID!)
        }

        _ = containerA.dumpSync(test: self)
        _ = containerB.dumpSync(test: self)
        _ = containerC.dumpSync(test: self)

        print("preparing C")
        let (cPeerID, cPermanentInfo, cPermanentInfoSig, cStableInfo, cStableInfoSig, error4) =
            containerC.prepareSync(test: self, epoch: 1, machineID: "ccc", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerC.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == cPeerID } .isEmpty, "should have a bottle for peer")
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
                                     ckksKeys: [self.manateeKeySet])
            XCTAssertNil(error)
            XCTAssertNotNil(voucherData)
            XCTAssertNotNil(voucherSig)

            assertTLKShareFor(peerID: cPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("C joins")
            let (peerID, _, error2) = containerC.joinSync(test: self,
                                                          voucherData: voucherData!,
                                                          voucherSig: voucherSig!,
                                                          ckksKeys: [self.manateeKeySet, provisionalEngramKeySet],
                                                          tlkShares: [])
            XCTAssertNil(error2)
            XCTAssertEqual(peerID, cPeerID!)

            assertTLKShareFor(peerID: cPeerID!, keyUUID: provisionalEngramKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Engram"))
            assertTLKShareFor(peerID: aPeerID!, keyUUID: provisionalEngramKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Engram"))
            assertTLKShareFor(peerID: bPeerID!, keyUUID: provisionalEngramKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Engram"))
        }

        print("A updates")
        do {
            let (_, error) = containerA.updateSync(test: self)
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
    }

    func testJoinWithoutAllowListErrors() throws {
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)

        let (peerID, permanentInfo, permanentInfoSig, _, _, error) = containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == peerID } .isEmpty, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: peerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error, "Should not have an error after preparing A")
        XCTAssertNotNil(peerID, "Should have a peer ID after preparing A")
        XCTAssertNotNil(permanentInfo, "Should have a permanent info after preparing A")
        XCTAssertNotNil(permanentInfoSig, "Should have a signature after preparing A")

        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa"]), "should be able to set allowed machine IDs")

        let (peerID2, _, error2) = containerA.establishSync(test: self, ckksKeys: [], tlkShares: [], preapprovedKeys: [])
        XCTAssertNotNil(peerID2, "Should get a peer when you establish a now allow-listed peer")
        XCTAssertNil(error2, "Should not get an error when you establish a now allow-listed peer")

        print("preparing B")
        let (bPeerID, bPermanentInfo, bPermanentInfoSig, bStableInfo, bStableInfoSig, errorPrepareB) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == peerID } .isEmpty, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: peerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(errorPrepareB, "Should not have an error after preparing B")
        XCTAssertNotNil(bPeerID, "Should have a peer ID after preparing B")
        XCTAssertNotNil(bPermanentInfo, "Should have a permanent info after preparing B")
        XCTAssertNotNil(bPermanentInfoSig, "Should have a signature after preparing B")

        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa"]), "should be able to set allowed machine IDs on container B")

        do {
            print("A vouches for B")
            let (voucherData, voucherSig, error3) =
                containerA.vouchSync(test: self,
                                     peerID: bPeerID!,
                                     permanentInfo: bPermanentInfo!,
                                     permanentInfoSig: bPermanentInfoSig!,
                                     stableInfo: bStableInfo!,
                                     stableInfoSig: bStableInfoSig!,
                                     ckksKeys: [])
            XCTAssertNil(error3, "Should be no error vouching for B")
            XCTAssertNotNil(voucherData, "Should have a voucher from A")
            XCTAssertNotNil(voucherSig, "Should have a signature from A")

            print("B joins")
            let (peerID, _, error) = containerB.joinSync(test: self,
                                                         voucherData: voucherData!,
                                                         voucherSig: voucherSig!,
                                                         ckksKeys: [],
                                                         tlkShares: [])
            XCTAssertNotNil(error, "Should have an error joining with an unapproved machine ID")
            XCTAssertNil(peerID, "Should not receive a peer ID joining with an unapproved machine ID")
        }
    }

    func testReset() throws {
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs))

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, error) = containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == aPeerID } .isEmpty, "should have a bottle for peer")
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
            let (peerID, _, error) = containerA.establishSync(test: self, ckksKeys: [self.manateeKeySet], tlkShares: [], preapprovedKeys: [])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
            assertTLKShareFor(peerID: aPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
        }

        print("reset A")
        do {
            let error = containerA.resetSync(test: self)
            XCTAssertNil(error)
        }
        do {
            let (dict, error) = containerA.dumpSync(test: self)
            XCTAssertNil(error)
            XCTAssertNotNil(dict)
            let peers: Array<Any> = dict!["peers"] as! Array<Any>
            XCTAssertEqual(0, peers.count)
        }
    }

    func testResetLocal() throws {
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs))

        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, error) = containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == aPeerID } .isEmpty, "should have a bottle for peer")
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
            let error = containerA.localResetSync(test: self)
            XCTAssertNil(error, "local-reset shouldn't error")
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
        var containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)
        let containerC = try Container(name: ContainerName(container: "c", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)

        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: Set(["aaa", "bbb", "ccc"])))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: Set(["aaa", "bbb", "ccc"])))
        XCTAssertNil(containerC.setAllowedMachineIDsSync(test: self, allowedMachineIDs: Set(["aaa", "bbb", "ccc"])))

        print("preparing")
        let (peerID, _, _, _, _, _) = containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == peerID } .isEmpty, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: peerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
        }
        let (bPeerID, bPermanentInfo, bPermanentInfoSig, bStableInfo, bStableInfoSig, _) = containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == bPeerID } .isEmpty, "should have a bottle for peer")
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
        }
        let (cPeerID, cPermanentInfo, cPermanentInfoSig, cStableInfo, cStableInfoSig, _) = containerC.prepareSync(test: self, epoch: 1, machineID: "ccc", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerC.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == cPeerID } .isEmpty, "should have a bottle for peer")
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
                                                                    ckksKeys: [])

            print("B joins")
            _ = containerB.joinSync(test: self,
                                    voucherData: voucherData!,
                                    voucherSig: voucherSig!,
                                    ckksKeys: [],
                                    tlkShares: [])
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
                                                                    ckksKeys: [])

            print("C joins")
            _ = containerC.joinSync(test: self,
                                    voucherData: voucherData!,
                                    voucherSig: voucherSig!,
                                    ckksKeys: [],
                                    tlkShares: [])
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
        containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)
        do {
            let state = containerA.getStateSync(test: self)
            let b = state.peers[bPeerID!]!
            XCTAssertEqual(lateClock, b.dynamicInfo!.clock)
        }
    }

    // TODO: need a real configurable mock cuttlefish
    func testFetchPolicyDocuments() throws {

        // 1 is known locally via builtin, 3 is not but is known to cuttlefish
        let policies =
            [
                1: ("SHA256:TLXrcQmY4ue3oP5pCX1pwsi9BF8cKfohlJBilCroeBs=",
                    "CAESDgoGaVBob25lEgRmdWxsEgwKBGlQYWQSBGZ1bGwSCwoDTWFjEgRmdWxsEgwKBGlNYWMSBGZ1bGwSDQoHQXBwbGVUVhICdHYS" +
                        "DgoFV2F0Y2gSBXdhdGNoGhEKCVBDU0VzY3JvdxIEZnVsbBoXCgRXaUZpEgRmdWxsEgJ0dhIFd2F0Y2gaGQoRU2FmYXJpQ3JlZGl0" +
                    "Q2FyZHMSBGZ1bGwiDAoEZnVsbBIEZnVsbCIUCgV3YXRjaBIEZnVsbBIFd2F0Y2giDgoCdHYSBGZ1bGwSAnR2"),
                3: ("SHA256:JZzazSuHXrUhiOfSgElsg6vYKpnvvEPVpciR8FewRWg=",
                     "CAMSDgoGaVBob25lEgRmdWxsEgwKBGlQYWQSBGZ1bGwSCwoDTWFjEgRmdWxsEgwKBGlNYWMSBGZ1bGwSDQoHQXBwbGVUVhICdHYSDgoFV2F0Y2gSBXdhdGNoEhcKDkF1ZGlvQWNjZXNzb3J5EgVhdWRpbxocCg1EZXZpY2VQYWlyaW5nEgRmdWxsEgV3YXRjaBoXCghBcHBsZVBheRIEZnVsbBIFd2F0Y2gaJAoVUHJvdGVjdGVkQ2xvdWRTdG9yYWdlEgRmdWxsEgV3YXRjaBoXCghCYWNrc3RvcBIEZnVsbBIFd2F0Y2gaGQoKQXV0b1VubG9jaxIEZnVsbBIFd2F0Y2gaHwoQU2VjdXJlT2JqZWN0U3luYxIEZnVsbBIFd2F0Y2gaIAoRU2FmYXJpQ3JlZGl0Q2FyZHMSBGZ1bGwSBXdhdGNoGhMKBEhvbWUSBGZ1bGwSBXdhdGNoGh4KD1NhZmFyaVBhc3N3b3JkcxIEZnVsbBIFd2F0Y2gaGwoMQXBwbGljYXRpb25zEgRmdWxsEgV3YXRjaBoVCgZFbmdyYW0SBGZ1bGwSBXdhdGNoGi0KE0xpbWl0ZWRQZWVyc0FsbG93ZWQSBGZ1bGwSBXdhdGNoEgJ0dhIFYXVkaW8aFgoHTWFuYXRlZRIEZnVsbBIFd2F0Y2gaHgoEV2lGaRIEZnVsbBIFd2F0Y2gSAnR2EgVhdWRpbxoVCgZIZWFsdGgSBGZ1bGwSBXdhdGNoIhMKBGZ1bGwSBGZ1bGwSBXdhdGNoIhsKBWF1ZGlvEgRmdWxsEgV3YXRjaBIFYXVkaW8iFAoFd2F0Y2gSBGZ1bGwSBXdhdGNoIhUKAnR2EgRmdWxsEgV3YXRjaBICdHYyIgoWAAQiEgIEdndodAoKXkFwcGxlUGF5JBIIQXBwbGVQYXkyJgoYAAQiFAIEdndodAoMXkF1dG9VbmxvY2skEgpBdXRvVW5sb2NrMh4KFAAEIhACBHZ3aHQKCF5FbmdyYW0kEgZFbmdyYW0yHgoUAAQiEAIEdndodAoIXkhlYWx0aCQSBkhlYWx0aDIaChIABCIOAgR2d2h0CgZeSG9tZSQSBEhvbWUyIAoVAAQiEQIEdndodAoJXk1hbmF0ZWUkEgdNYW5hdGVlMjgKIQAEIh0CBHZ3aHQKFV5MaW1pdGVkUGVlcnNBbGxvd2VkJBITTGltaXRlZFBlZXJzQWxsb3dlZDJdClAAAhIeAAQiGgIEdndodAoSXkNvbnRpbnVpdHlVbmxvY2skEhUABCIRAgR2d2h0CgleSG9tZUtpdCQSFQAEIhECBHZ3aHQKCV5BcHBsZVRWJBIJTm90U3luY2VkMisKGwAEIhcCBGFncnAKD15bMC05QS1aXXsxMH1cLhIMQXBwbGljYXRpb25zMsUBCrABAAISNAABChMABCIPAgVjbGFzcwoGXmdlbnAkChsABCIXAgRhZ3JwCg9eY29tLmFwcGxlLnNiZCQSPQABChMABCIPAgVjbGFzcwoGXmtleXMkCiQABCIgAgRhZ3JwChheY29tLmFwcGxlLnNlY3VyaXR5LnNvcyQSGQAEIhUCBHZ3aHQKDV5CYWNrdXBCYWdWMCQSHAAEIhgCBHZ3aHQKEF5pQ2xvdWRJZGVudGl0eSQSEFNlY3VyZU9iamVjdFN5bmMyYwpbAAISEgAEIg4CBHZ3aHQKBl5XaUZpJBJDAAEKEwAEIg8CBWNsYXNzCgZeZ2VucCQKEwAEIg8CBGFncnAKB15hcHBsZSQKFQAEIhECBHN2Y2UKCV5BaXJQb3J0JBIEV2lGaTLbAgrBAgACEhkABCIVAgR2d2h0Cg1eUENTQ2xvdWRLaXQkEhcABCITAgR2d2h0CgteUENTRXNjcm93JBIUAAQiEAIEdndodAoIXlBDU0ZERSQSGQAEIhUCBHZ3aHQKDV5QQ1NGZWxkc3BhciQSGQAEIhUCBHZ3aHQKDV5QQ1NNYWlsRHJvcCQSGgAEIhYCBHZ3aHQKDl5QQ1NNYXN0ZXJLZXkkEhYABCISAgR2d2h0CgpeUENTTm90ZXMkEhcABCITAgR2d2h0CgteUENTUGhvdG9zJBIYAAQiFAIEdndodAoMXlBDU1NoYXJpbmckEh0ABCIZAgR2d2h0ChFeUENTaUNsb3VkQmFja3VwJBIcAAQiGAIEdndodAoQXlBDU2lDbG91ZERyaXZlJBIZAAQiFQIEdndodAoNXlBDU2lNZXNzYWdlJBIVUHJvdGVjdGVkQ2xvdWRTdG9yYWdlMkAKKwAEIicCBGFncnAKH15jb20uYXBwbGUuc2FmYXJpLmNyZWRpdC1jYXJkcyQSEVNhZmFyaUNyZWRpdENhcmRzMjQKIQAEIh0CBGFncnAKFV5jb20uYXBwbGUuY2ZuZXR3b3JrJBIPU2FmYXJpUGFzc3dvcmRzMm0KXAACEh4ABCIaAgR2d2h0ChJeQWNjZXNzb3J5UGFpcmluZyQSGgAEIhYCBHZ3aHQKDl5OYW5vUmVnaXN0cnkkEhwABCIYAgR2d2h0ChBeV2F0Y2hNaWdyYXRpb24kEg1EZXZpY2VQYWlyaW5nMi0KIQAEIh0CBGFncnAKFV5jb20uYXBwbGUuY2ZuZXR3b3JrJBIIQmFja3N0b3A="),
                ]
        let (request1, data1) = policies[1]!
        let (request3, data3) = policies[3]!

        let description = tmpStoreDescription(name: "container.db")
        let container = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)

        // nothing
        let (response1, error1) = container.fetchPolicyDocumentsSync(test: self, keys: [:])
        XCTAssertNil(error1, "No error querying for an empty list")
        XCTAssertEqual(response1, [:], "Received empty dictionary")

        // local stuff
        let (response2, error2) = container.fetchPolicyDocumentsSync(test: self, keys: [1: request1])
        XCTAssertNil(error2, "No error getting locally known policy document")
        XCTAssertEqual(response2?.count, 1, "Got one response for request for one locally known policy")
        XCTAssertEqual(response2?[1]?[0], request1, "retrieved hash matches request hash")
        XCTAssertEqual(response2?[1]?[1], data1, "retrieved data matches known data")

        // fetch remote
        let (response3, error3) = container.fetchPolicyDocumentsSync(test: self, keys: [1: request1, 3: request3])
        XCTAssertNil(error3, "No error fetching local + remote policy")
        XCTAssertEqual(response3?.count, 2, "Got two responses for local+remote policy request")
        XCTAssertEqual(response3?[1]?[0], request1, "retrieved hash matches local request hash")
        XCTAssertEqual(response3?[1]?[1], data1, "retrieved data matches local known data")
        XCTAssertEqual(response3?[3]?[0], request3, "retrieved hash matches remote request hash")
        XCTAssertEqual(response3?[3]?[1], data3, "retrieved data matches remote known data")

        // invalid version
        let (response4, error4) = container.fetchPolicyDocumentsSync(test: self, keys: [9000: "not a hash"])
        XCTAssertNil(response4, "No response for wrong [version: hash] combination")
        XCTAssertNotNil(error4, "Expected error fetching invalid policy version")

        // valid + invalid
        let (response5, error5) = container.fetchPolicyDocumentsSync(test: self, keys: [9000: "not a hash",
                                                                                        1: request1,
                                                                                        3: request3, ])
        XCTAssertNil(response5, "No response for valid + unknown [version: hash] combination")
        XCTAssertNotNil(error5, "Expected error fetching valid + invalid policy version")
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
            let testv1 = try EscrowKeys.generateEscrowKey(keyType: escrowKeyType.kOTEscrowKeySigning, masterSecret: secret!, bottleSalt: testDSID)
            XCTAssertEqual(testv1, signingKey_384, "signing keys should match")

            let testv2 = try EscrowKeys.generateEscrowKey(keyType: escrowKeyType.kOTEscrowKeyEncryption, masterSecret: secret!, bottleSalt: testDSID)
            XCTAssertEqual(testv2, encryptionKey_384, "encryption keys should match")

            let testv3 = try EscrowKeys.generateEscrowKey(keyType: escrowKeyType.kOTEscrowKeySymmetric, masterSecret: secret!, bottleSalt: testDSID)
            XCTAssertEqual(testv3, symmetricKey_384, "symmetric keys should match")

            let newSecretString = "I'm f secretI'm a secretI'm a secretI'm a secretI'm a secretI'm a secret"
            let newSecret = newSecretString.data(using: .utf8)

            let testv4 = try EscrowKeys.generateEscrowKey(keyType: escrowKeyType.kOTEscrowKeySigning, masterSecret: newSecret!, bottleSalt: testDSID)
            XCTAssertNotEqual(testv4, signingKey_384, "signing keys should not match")

            let testv5 = try EscrowKeys.generateEscrowKey(keyType: escrowKeyType.kOTEscrowKeyEncryption, masterSecret: newSecret!, bottleSalt: testDSID)
            XCTAssertNotEqual(testv5, encryptionKey_384, "encryption keys should not match")

            let testv6 = try EscrowKeys.generateEscrowKey(keyType: escrowKeyType.kOTEscrowKeySymmetric, masterSecret: newSecret!, bottleSalt: testDSID)
            XCTAssertNotEqual(testv6, symmetricKey_384, "symmetric keys should not match")
        } catch {
            XCTFail("error testing escrow key test vectors \(error)")
        }
    }

    func testJoiningWithBottle() throws {
        var bottleA: BottleMO
        var entropy: Data
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa", "bbb"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs))

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            var state = containerA.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == aPeerID } .isEmpty, "should have a bottle for peer")

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
            let (peerID, _, error) = containerA.establishSync(test: self, ckksKeys: [self.manateeKeySet], tlkShares: [], preapprovedKeys: [])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
            assertTLKShareFor(peerID: aPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
        }
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == aPeerID } .isEmpty, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            entropy = secret!
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }

        _ = containerB.updateSync(test: self)

        print("preparing B")
        let (bPeerID, _, _, _, _, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == bPeerID } .isEmpty, "should have a bottle for peer")
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error2)

        do {
            print("B prepares to join via bottle")

            let (voucherData, voucherSig, error3) = containerB.vouchWithBottleSync(test: self, b: bottleA.bottleID!, entropy: entropy, bottleSalt: "123456789", tlkShares: [])

            XCTAssertNil(error3)
            XCTAssertNotNil(voucherData)
            XCTAssertNotNil(voucherSig)

            // Before B joins, there should be no TLKShares for B
            assertNoTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("B joins")
            let (peerID, _, error) = containerB.joinSync(test: self, voucherData: voucherData!, voucherSig: voucherSig!, ckksKeys: [self.manateeKeySet], tlkShares: [])
            XCTAssertNil(error)
            XCTAssertEqual(peerID, bPeerID!)

            // But afterward, it has one!
            assertTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
        }
    }

    func testJoiningWithBottleAndEmptyBottleSalt() throws {
        var bottleA: BottleMO
        var entropy: Data
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa", "bbb"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs))

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            var state = containerA.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == aPeerID } .isEmpty, "should have a bottle for peer")

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
            let (peerID, _, error) = containerA.establishSync(test: self, ckksKeys: [self.manateeKeySet], tlkShares: [], preapprovedKeys: [])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
            assertTLKShareFor(peerID: aPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
        }
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == aPeerID } .isEmpty, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            entropy = secret!
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }

        _ = containerB.updateSync(test: self)

        print("preparing B")
        let (bPeerID, _, _, _, _, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == bPeerID } .isEmpty, "should have a bottle for peer")
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error2)

        do {
            print("B prepares to join via bottle")

            let (voucherData, voucherSig, error3) = containerB.vouchWithBottleSync(test: self, b: bottleA.bottleID!, entropy: entropy, bottleSalt: "123456789", tlkShares: [])

            XCTAssertNil(error3)
            XCTAssertNotNil(voucherData)
            XCTAssertNotNil(voucherSig)

            // Before B joins, there should be no TLKShares for B
            assertNoTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("B joins")
            let (peerID, _, error) = containerB.joinSync(test: self, voucherData: voucherData!, voucherSig: voucherSig!, ckksKeys: [self.manateeKeySet], tlkShares: [])
            XCTAssertNil(error)
            XCTAssertEqual(peerID, bPeerID!)

            // But afterward, it has one!
            assertTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
        }
    }

    func testJoiningWithWrongEscrowRecordForBottle() throws {
        var entropy: Data
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa", "bbb"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs))

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == aPeerID } .isEmpty, "should have a bottle for peer")
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
            let (peerID, _, error) = containerA.establishSync(test: self, ckksKeys: [self.manateeKeySet], tlkShares: [], preapprovedKeys: [])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
        }
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == aPeerID } .isEmpty, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            entropy = secret!
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }

        _ = containerB.updateSync(test: self)

        print("preparing B")
        let (bPeerID, _, _, _, _, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == bPeerID } .isEmpty, "should have a bottle for peer")
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error2)

        do {
            print("B joins via bottle")

            let (voucherData, voucherSig, error3) = containerB.vouchWithBottleSync(test: self, b: "wrong escrow record", entropy: entropy, bottleSalt: "123456789", tlkShares: [])

            XCTAssertNotNil(error3)
            XCTAssertNil(voucherData)
            XCTAssertNil(voucherSig)
        }
    }

    func testJoiningWithWrongBottle() throws {
        var bottleB: BottleMO
        var entropy: Data
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa", "bbb"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs))

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == aPeerID } .isEmpty, "should have a bottle for peer")
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
            let (peerID, _, error) = containerA.establishSync(test: self, ckksKeys: [], tlkShares: [], preapprovedKeys: [])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
        }
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == aPeerID } .isEmpty, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            entropy = secret!
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }

        print("preparing B")
        let (bPeerID, _, _, _, _, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            var state = containerB.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == bPeerID } .isEmpty, "should have a bottle for peer")
            bottleB = state.bottles.removeFirst()
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error2)

        do {
            print("B joins via bottle")

            let (voucherData, voucherSig, error3) = containerB.vouchWithBottleSync(test: self, b: bottleB.bottleID!, entropy: entropy, bottleSalt: "123456789", tlkShares: [])

            XCTAssertNotNil(error3)
            XCTAssertNil(voucherData)
            XCTAssertNil(voucherSig)
        }
    }

    func testJoiningWithBottleAndBadSalt() throws {
        var bottleA: BottleMO
        var entropy: Data
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa", "bbb"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs))

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            var state = containerA.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == aPeerID } .isEmpty, "should have a bottle for peer")
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
            let (peerID, _, error) = containerA.establishSync(test: self, ckksKeys: [], tlkShares: [], preapprovedKeys: [])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
        }
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == aPeerID } .isEmpty, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            entropy = secret!
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }

        _ = containerB.updateSync(test: self)

        print("preparing B")
        let (bPeerID, _, _, _, _, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == bPeerID } .isEmpty, "should have a bottle for peer")
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error2)

        do {
            print("B joins via bottle")

            let (voucherData, voucherSig, error3) = containerB.vouchWithBottleSync(test: self, b: bottleA.bottleID!, entropy: entropy, bottleSalt: "987654321", tlkShares: [])

            XCTAssertNotNil(error3)
            XCTAssertNil(voucherData)
            XCTAssertNil(voucherSig)
        }
    }

    func testJoiningWithBottleAndBadSecret() throws {
        var bottleA: BottleMO
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa", "bbb"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs))

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            var state = containerA.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == aPeerID } .isEmpty, "should have a bottle for peer")
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
            let (peerID, _, error) = containerA.establishSync(test: self, ckksKeys: [], tlkShares: [], preapprovedKeys: [])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
        }
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == aPeerID } .isEmpty, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }

        _ = containerB.updateSync(test: self)

        print("preparing B")
        let (bPeerID, _, _, _, _, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == bPeerID } .isEmpty, "should have a bottle for peer")
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error2)

        do {
            print("B joins via bottle")

            let (voucherData, voucherSig, error3) = containerB.vouchWithBottleSync(test: self, b: bottleA.bottleID!, entropy: Data(count: Int(OTMasterSecretLength)), bottleSalt: "123456789", tlkShares: [])

            XCTAssertNotNil(error3)
            XCTAssertNil(voucherData)
            XCTAssertNil(voucherSig)
        }
    }

    func testJoiningWithNoFetchAllBottles() throws {
        var bottleA: BottleMO
        var entropy: Data
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa", "bbb"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs))

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            var state = containerA.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == aPeerID } .isEmpty, "should have a bottle for peer")
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
            let (peerID, _, error) = containerA.establishSync(test: self, ckksKeys: [], tlkShares: [], preapprovedKeys: [])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
        }
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == aPeerID } .isEmpty, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }

        print("preparing B")
        let (bPeerID, _, _, _, _, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == bPeerID } .isEmpty, "should have a bottle for peer")
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error2)

        do {
            print("B joins via bottle")

            // And the first container fetches again, which should succeed
            let cuttlefishError = NSError(domain: CuttlefishErrorDomain, code: CuttlefishErrorCode.changeTokenExpired.rawValue, userInfo: nil)
            let ckError = NSError(domain: CKInternalErrorDomain, code: CKInternalErrorCode.errorInternalPluginError.rawValue, userInfo: [NSUnderlyingErrorKey: cuttlefishError])
            self.cuttlefish.fetchViableBottlesError.append(ckError)

            let (voucherData, voucherSig, error3) = containerB.vouchWithBottleSync(test: self, b: bottleA.bottleID!, entropy: entropy, bottleSalt: "123456789", tlkShares: [])

            XCTAssertNotNil(error3)
            XCTAssertNil(voucherData)
            XCTAssertNil(voucherSig)
        }
    }

    func testJoinByPreapproval() throws {
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa", "bbb"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs))

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        XCTAssertNil(error)
        XCTAssertNotNil(aPeerID)
        XCTAssertNotNil(aPermanentInfo)
        XCTAssertNotNil(aPermanentInfoSig)

        print("preparing B")
        let (bPeerID, bPermanentInfo, bPermanentInfoSig, _, _, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == bPeerID } .isEmpty, "should have a bottle for peer")
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
            let (peerID, _, error) = containerA.establishSync(test: self, ckksKeys: [self.manateeKeySet], tlkShares: [], preapprovedKeys: [bPermanentInfoParsed!.signingPubKey.spki()])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
        }

        do {
            assertNoTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("B joins by preapproval, and uploads all TLKShares that it has")
            let (bJoinedPeerID, _, bJoinedError) = containerB.preapprovedJoinSync(test: self, ckksKeys: [self.manateeKeySet], tlkShares: [])
            XCTAssertNil(bJoinedError, "Should be no error joining by preapproval")
            XCTAssertNotNil(bJoinedPeerID, "Should have a peer ID out of join")

            assertTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
        }

        _ = containerA.dumpSync(test: self)
        _ = containerB.dumpSync(test: self)
    }

    func testDepart() throws {
        let description = tmpStoreDescription(name: "container.db")
        let (container, peerID) = try establish(reload: false, store: description)

        XCTAssertNil(container.departByDistrustingSelfSync(test: self), "Should be no error distrusting self")
        assertDistrusts(context: container, peerIDs: [peerID])
    }

    func testDistrustPeers() throws {
        let store = tmpStoreDescription(name: "container.db")
        let (c, peerID1) = try establish(reload: false, store: store)

        let (c2, peerID2) = try joinByVoucher(sponsor: c,
                                              containerID: "second",
                                              machineID: "bbb",
                                              machineIDs: ["aaa", "bbb", "ccc"],
                                              store: store)

        let (c3, peerID3) = try joinByVoucher(sponsor: c,
                                              containerID: "third",
                                              machineID: "ccc",
                                              machineIDs: ["aaa", "bbb", "ccc"],
                                              store: store)

        let (_, cUpdateError) = c.updateSync(test: self)
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
        let (_, c2UpdateError) = c2.updateSync(test: self)
        XCTAssertNil(c2UpdateError, "Should be able to update second container")
        assertDistrusts(context: c2, peerIDs: [peerID2])

        let (_, c3UpdateError) = c3.updateSync(test: self)
        XCTAssertNil(c3UpdateError, "Should be able to update third container")
        assertDistrusts(context: c3, peerIDs: [peerID3])
    }

    func testFetchWithBadChangeToken() throws {
        let (c, peerID1) = try establish(reload: false, store: tmpStoreDescription(name: "container.db"))

        // But all that goes away, and a new peer establishes
        self.cuttlefish.state = FakeCuttlefishServer.State()
        let (_, peerID2) = try establish(reload: false, contextID: "second", store: tmpStoreDescription(name: "container-peer2.db"))

        // And the first container fetches again, which should succeed
        let cuttlefishError = NSError(domain: CuttlefishErrorDomain, code: CuttlefishErrorCode.changeTokenExpired.rawValue, userInfo: nil)
        let ckError = NSError(domain: CKInternalErrorDomain, code: CKInternalErrorCode.errorInternalPluginError.rawValue, userInfo: [NSUnderlyingErrorKey: cuttlefishError])
        self.cuttlefish.nextFetchErrors.append(ckError)
        _ = c.updateSync(test: self)

        // and c's model should only include peerID2
        c.moc.performAndWait {
            let modelPeers = c.model.allPeerIDs()
            XCTAssertEqual(modelPeers.count, 1, "Model should have one peer")
            XCTAssert(modelPeers.contains(peerID2), "Model should contain peer 2")
            XCTAssertFalse(modelPeers.contains(peerID1), "Model should no longer container peer 1 (ego peer)")
        }
    }

    func testFetchEscrowContents() throws {
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)
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
        }

        let c2 = try Container(name: ContainerName(container: "test", context: "newcomer"), persistentStoreDescription: store, cuttlefish: self.cuttlefish)
        do {
            let state = c2.getStateSync(test: self)
            XCTAssertEqual(state.bottles.count, 0, "before fetch, second container should not have any stored bottles")
        }

        let (c2bottles, _, c2FetchError) = c2.fetchViableBottlesSync(test: self)
        XCTAssertNil(c2FetchError, "should be no error fetching viable bottles")
        XCTAssertNotNil(c2bottles, "should have fetched some bottles")
        XCTAssertEqual(c2bottles!.count, 1, "should have fetched one bottle")

        do {
            let state = c2.getStateSync(test: self)
            XCTAssertEqual(state.bottles.count, 1, "after fetch, second container should have one stored bottles")
        }
    }

    func testTrustStatus() throws {
        let store = tmpStoreDescription(name: "container.db")

        let preC = try Container(name: ContainerName(container: "preC", context: "preCContext"),
                                 persistentStoreDescription: store,
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
                                 cuttlefish: self.cuttlefish)

        let machineIDs = Set(["aaa", "bbb", "ccc"])
        XCTAssertNil(c.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs))

        print("preparing peer A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, error) =
            c.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = c.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == aPeerID } .isEmpty, "should have a bottle for peer")
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
            let (peerID, _, error) = c.establishSync(test: self, ckksKeys: [], tlkShares: [], preapprovedKeys: nil)
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
        }
        let recoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        let (setRecoveryError) = c.setRecoveryKeySync(test: self, recoveryKey: recoveryKey!, recoverySalt: "altDSID", ckksKeys: [])
        XCTAssertNil(setRecoveryError, "error should be nil")
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
                              cuttlefish: self.cuttlefish)

        let machineIDs = Set(["aaa", "bbb", "ccc"])
        XCTAssertNil(c.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs))

        print("preparing peer A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, error) =
            c.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = c.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == aPeerID } .isEmpty, "should have a bottle for peer")
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
            let (peerID, _, error) = c.establishSync(test: self, ckksKeys: [], tlkShares: [], preapprovedKeys: nil)
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
        }
        let (repairAccount, repairEscrow, resetOctagon, healthError) = c.requestHealthCheckSync(requiresEscrowCheck: true, test: self)
        XCTAssertEqual(repairAccount, false, "")
        XCTAssertEqual(repairEscrow, false, "")
        XCTAssertEqual(resetOctagon, false, "")
        XCTAssertNil(healthError)
    }

    func testFetchChangesFailDuringVouchWithBottle() throws {
        var bottleA: BottleMO
        var entropy: Data
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)

        let machineIDs = Set(["aaa", "bbb"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs))

        print("preparing A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, _, _, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            var state = containerA.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == aPeerID } .isEmpty, "should have a bottle for peer")

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
            let (peerID, _, error) = containerA.establishSync(test: self, ckksKeys: [self.manateeKeySet], tlkShares: [], preapprovedKeys: [])
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
            assertTLKShareFor(peerID: aPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))
        }
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == aPeerID } .isEmpty, "should have a bottle for peer")
            let secret = containerA.loadSecretSync(test: self, label: aPeerID!)
            entropy = secret!
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }

        _ = containerB.updateSync(test: self)

        print("preparing B")
        let (bPeerID, _, _, _, _, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == bPeerID } .isEmpty, "should have a bottle for peer")
            let secret = containerB.loadSecretSync(test: self, label: bPeerID!)
            XCTAssertNotNil(secret, "secret should not be nil")
            XCTAssertNil(error, "error should be nil")
        }
        XCTAssertNil(error2)

        do {
            print("B prepares to join via bottle")

            let (voucherData, voucherSig, error3) = containerB.vouchWithBottleSync(test: self, b: bottleA.bottleID!, entropy: entropy, bottleSalt: "123456789", tlkShares: [])

            XCTAssertNil(error3)
            XCTAssertNotNil(voucherData)
            XCTAssertNotNil(voucherSig)

            // And the first container fetches again, which should succeed
            let cuttlefishError = NSError(domain: CuttlefishErrorDomain, code: CuttlefishErrorCode.changeTokenExpired.rawValue, userInfo: nil)
            let ckError = NSError(domain: CKInternalErrorDomain, code: CKInternalErrorCode.errorInternalPluginError.rawValue, userInfo: [NSUnderlyingErrorKey: cuttlefishError])
            self.cuttlefish.nextFetchErrors.append(ckError)

            // Before B joins, there should be no TLKShares for B
            assertNoTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("B joins")
            let (peerID, _, error) = containerB.joinSync(test: self, voucherData: voucherData!, voucherSig: voucherSig!, ckksKeys: [self.manateeKeySet], tlkShares: [])
            XCTAssertNotNil(error)
            XCTAssertNil(peerID)
        }
    }

    func testDistrustedPeerRecoveryKeyNotSet() throws {
        let description = tmpStoreDescription(name: "container.db")
        let containerA = try Container(name: ContainerName(container: "a", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)
        let containerB = try Container(name: ContainerName(container: "b", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)


        let machineIDs = Set(["aaa", "bbb"])
        XCTAssertNil(containerA.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs))
        XCTAssertNil(containerB.setAllowedMachineIDsSync(test: self, allowedMachineIDs: machineIDs))

        print("preparing peer A")
        let (aPeerID, aPermanentInfo, aPermanentInfoSig, aStableInfo, aStableInfoSig, error) =
            containerA.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerA.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == aPeerID } .isEmpty, "should have a bottle for peer")
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
            let (peerID, _, error) = containerA.establishSync(test: self, ckksKeys: [], tlkShares: [], preapprovedKeys: nil)
            XCTAssertNil(error)
            XCTAssertNotNil(peerID)
        }

        print("preparing B")
        let (bPeerID, bPermanentInfo, bPermanentInfoSig, bStableInfo, bStableInfoSig, error2) =
            containerB.prepareSync(test: self, epoch: 1, machineID: "bbb", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
        do {
            let state = containerB.getStateSync(test: self)
            XCTAssertFalse( state.bottles.filter { $0.peerID == bPeerID } .isEmpty, "should have a bottle for peer")
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
                                     ckksKeys: [])
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
                                     ckksKeys: [provisionalManateeKeySet])
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
                                     ckksKeys: [self.manateeKeySet])
            XCTAssertNil(error3)
            XCTAssertNotNil(voucherData)
            XCTAssertNotNil(voucherSig)

            // As part of the vouch, A should have uploaded a tlkshare for B
            assertTLKShareFor(peerID: bPeerID!, keyUUID: self.manateeKeySet.tlk.uuid, zoneID: CKRecordZone.ID(zoneName: "Manatee"))

            print("B joins")
            let (peerID, _, error) = containerB.joinSync(test: self,
                                                         voucherData: voucherData!,
                                                         voucherSig: voucherSig!,
                                                         ckksKeys: [],
                                                         tlkShares: [])
            XCTAssertNil(error)
            XCTAssertEqual(peerID, bPeerID!)
        }

        print("A updates")
        do {
            let (_, error) = containerA.updateSync(test: self)
            XCTAssertNil(error)
        }
        print("B updates")
        do {
            let (_, error) = containerB.updateSync(test: self)
            XCTAssertNil(error)
        }

        // Now, A distrusts B.
        XCTAssertNil(containerA.distrustSync(test: self, peerIDs: Set([bPeerID!])), "Should be no error distrusting peers")
        assertDistrusts(context: containerA, peerIDs: [bPeerID!])


        let recoveryKey = SecRKCreateRecoveryKeyString(nil)
        XCTAssertNotNil(recoveryKey, "recoveryKey should not be nil")

        let (setRecoveryError) = containerB.setRecoveryKeySync(test: self, recoveryKey: recoveryKey!, recoverySalt: "altDSID", ckksKeys: [])
        XCTAssertNil(setRecoveryError, "error should be nil")

        print("A updates")
        do {
            let (_, error) = containerA.updateSync(test: self)
            XCTAssertNil(error)
        }
        print("B updates")
        do {
            let (_, error) = containerB.updateSync(test: self)
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
                persistentStore: NSPersistentStoreDescription,
                cuttlefish: FakeCuttlefishServer) throws {

        let midList = container.onqueueCurrentMIDList()
        XCTAssertEqual(midList.machineIDs(in: .allowed), allowedMachineIDs, "List of allowed machine IDs should match")
        XCTAssertEqual(midList.machineIDs(in: .disallowed), disallowedMachineIDs, "List of disallowed machine IDs should match")
        XCTAssertEqual(midList.machineIDs(in: .unknown), unknownMachineIDs, "List of unknown machine IDs should match")

        // if we reload the container, does it still match?
        let reloadedContainer = try Container(name: ContainerName(container: "test", context: OTDefaultContext), persistentStoreDescription: persistentStore, cuttlefish: cuttlefish)

        let reloadedMidList = reloadedContainer.onqueueCurrentMIDList()
        XCTAssertEqual(reloadedMidList.machineIDs(in: .allowed), allowedMachineIDs, "List of allowed machine IDs on a reloaded container should match")
        XCTAssertEqual(reloadedMidList.machineIDs(in: .disallowed), disallowedMachineIDs, "List of disallowed machine IDs on a reloaded container should match")
        XCTAssertEqual(reloadedMidList.machineIDs(in: .unknown), unknownMachineIDs, "List of unknown machine IDs on a reloaded container should match")
    }

    func testAllowListManipulation() throws {
        let description = tmpStoreDescription(name: "container.db")
        let container = try Container(name: ContainerName(container: "test", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)

        let (peerID, permanentInfo, permanentInfoSig, _, _, error) = container.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")

        XCTAssertNil(error)
        XCTAssertNotNil(peerID)
        XCTAssertNotNil(permanentInfo)
        XCTAssertNotNil(permanentInfoSig)

        try self.assert(container: container, allowedMachineIDs: [], disallowedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ccc"]), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.onqueueFullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb"]), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb"]), disallowedMachineIDs: Set(["ccc"]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.onqueueFullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        XCTAssertNil(container.addAllowedMachineIDsSync(test: self, machineIDs: ["zzz", "kkk"]), "should be able to add allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "zzz", "kkk"]), disallowedMachineIDs: Set(["ccc"]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.onqueueFullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // Receivng a 'remove' push should send the MIDs to the 'unknown' list
        XCTAssertNil(container.removeAllowedMachineIDsSync(test: self, machineIDs: ["bbb", "fff"]), "should be able to remove allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "zzz", "kkk"]), disallowedMachineIDs: Set(["ccc"]), unknownMachineIDs: Set(["bbb", "fff"]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertTrue(container.onqueueFullIDMSListWouldBeHelpful(), "Container should think it could use an IDMS list set: there's machine IDs pending removal")

        // once they're unknown, a full list set will make them disallowed
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "zzz", "kkk"]), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "zzz", "kkk"]), disallowedMachineIDs: Set(["bbb", "ccc", "fff"]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.onqueueFullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // Resetting the list to what it is doesn't change the list
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "zzz", "kkk"], listDifference: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "zzz", "kkk"]), disallowedMachineIDs: Set(["bbb", "ccc", "fff"]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.onqueueFullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // But changing it to something completely new does
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["xxx", "mmm"]), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["xxx", "mmm"]), disallowedMachineIDs: Set(["aaa", "zzz", "kkk", "bbb", "ccc", "fff"]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.onqueueFullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // And, readding a previously disallowed machine ID works too
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["xxx", "mmm", "aaa"]), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["xxx", "mmm", "aaa"]), disallowedMachineIDs: Set(["zzz", "kkk", "bbb", "ccc", "fff"]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.onqueueFullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // A update() before establish() doesn't change the list, since it isn't actually changing anything
        let (_, updateError) = container.updateSync(test: self)
        XCTAssertNil(updateError, "Should not be an error updating the container without first establishing")
        try self.assert(container: container, allowedMachineIDs: Set(["xxx", "mmm", "aaa"]), disallowedMachineIDs: Set(["zzz", "kkk", "bbb", "ccc", "fff"]), persistentStore: description, cuttlefish: self.cuttlefish)

        let (_, _, establishError) = container.establishSync(test: self, ckksKeys: [self.manateeKeySet], tlkShares: [], preapprovedKeys: [])
        XCTAssertNil(establishError, "Should be able to establish() with no error")
        try self.assert(container: container, allowedMachineIDs: Set(["xxx", "mmm", "aaa"]), disallowedMachineIDs: Set(["zzz", "kkk", "bbb", "ccc", "fff"]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.onqueueFullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // But a successful update() does remove all disallowed machine IDs, as they're no longer relevant
        let (_, updateError2) = container.updateSync(test: self)
        XCTAssertNil(updateError2, "Should not be an error updating the container after establishing")
        try self.assert(container: container, allowedMachineIDs: Set(["xxx", "mmm", "aaa"]), disallowedMachineIDs: Set([]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.onqueueFullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")
    }

    func testAllowListManipulationWithAddsAndRemoves() throws {
        let description = tmpStoreDescription(name: "container.db")
        let container = try Container(name: ContainerName(container: "test", context: OTDefaultContext), persistentStoreDescription: description, cuttlefish: cuttlefish)

        try self.assert(container: container, allowedMachineIDs: [], disallowedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)

        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ccc"], listDifference: true), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.onqueueFullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // Now, an 'add' comes in for some peers
        XCTAssertNil(container.addAllowedMachineIDsSync(test: self, machineIDs: ["ddd", "eee"]), "should be able to receive an add push")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc", "ddd", "eee"]), disallowedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.onqueueFullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // But, the next time we ask IDMS, they still haven't made it to the full list, and in fact, C has disappeared.
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb"], listDifference: true), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ddd", "eee"]), disallowedMachineIDs: Set(["ccc"]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.onqueueFullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // And a remove comes in for E. It becomes 'unknown'
        XCTAssertNil(container.removeAllowedMachineIDsSync(test: self, machineIDs: ["eee"]), "should be able to receive an add push")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ddd"]), disallowedMachineIDs: Set(["ccc"]), unknownMachineIDs: Set(["eee"]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertTrue(container.onqueueFullIDMSListWouldBeHelpful(), "Container should think it could use an IDMS list set: there's machine IDs pending removal")

        // and a list set after the remove confirms the removal
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb"], listDifference: true), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ddd"]), disallowedMachineIDs: Set(["ccc", "eee"]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.onqueueFullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // Then a new list set includes D! Hurray IDMS. Note that this is not a "list change", because the list doesn't actually change
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ddd"], listDifference: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ddd"]), disallowedMachineIDs: Set(["ccc", "eee"]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.onqueueFullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // And another list set no longer includes D, so it should now be disallowed
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb"], listDifference: true), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb"]), disallowedMachineIDs: Set(["ccc", "ddd", "eee"]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.onqueueFullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // And just to check the 48 hour boundary...
        XCTAssertNil(container.addAllowedMachineIDsSync(test: self, machineIDs: ["xxx"]), "should be able to receive an add push")
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb"], listDifference: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "xxx"]), disallowedMachineIDs: Set(["ccc", "ddd", "eee"]), persistentStore: description, cuttlefish: self.cuttlefish)

        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            knownMachineMOs.forEach {
                if $0.machineID == "xxx" {
                    $0.modified = Date(timeIntervalSinceNow: -60*60*72)
                }
            }

            try! container.moc.save()
        }

        XCTAssertFalse(container.onqueueFullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // Setting the list again should kick out X, since it was 'added' too long ago
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb"], listDifference: true), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb"]), disallowedMachineIDs: Set(["ccc", "ddd", "eee", "xxx"]), persistentStore: description, cuttlefish: self.cuttlefish)
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
        let container = try Container(name: containerName, persistentStoreDescription: description, cuttlefish: cuttlefish)

        let (peerID, permanentInfo, permanentInfoSig, _, _, error) = container.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")

        XCTAssertNil(error)
        XCTAssertNotNil(peerID)
        XCTAssertNotNil(permanentInfo)
        XCTAssertNotNil(permanentInfoSig)

        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)

        // Setting a new list should work fine
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ddd"]), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ddd"]), disallowedMachineIDs: ["ccc"], persistentStore: description, cuttlefish: self.cuttlefish)

        XCTAssertEqual(container.containerMO.allowedMachineIDs, Set<String>() as NSSet, "Set of allowed machine IDs should now be empty")
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
        let container = try Container(name: containerName, persistentStoreDescription: description, cuttlefish: cuttlefish)
        try self.assert(container: container, allowedMachineIDs: Set(["aaa"]), disallowedMachineIDs: ["bbb"], persistentStore: description, cuttlefish: self.cuttlefish)

        // Setting a new list should work fine
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "ddd"]), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "ddd"]), disallowedMachineIDs: ["bbb"], persistentStore: description, cuttlefish: self.cuttlefish)

        XCTAssertEqual(container.containerMO.allowedMachineIDs, Set<String>() as NSSet, "Set of allowed machine IDs should now be empty")
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
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], listDifference: true), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: [], unknownMachineIDs: Set(["aaa"]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.onqueueFullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // But check that it exists, and set its modification date to a while ago for an upcoming test
        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            let aaaMOs = knownMachineMOs.filter { $0.machineID == "aaa" }
            XCTAssert(aaaMOs.count == 1, "Should have one machine MO for aaa")

            let aaaMO = aaaMOs.first!
            XCTAssertEqual(aaaMO.status, Int64(TPMachineIDStatus.unknown.rawValue), "Status of aaa MO should be 'unknown'")
            XCTAssertFalse(aaaMO.allowed, "allowed should no longer be a used field")

            aaaMO.modified = Date(timeIntervalSinceNow: -60)
            try! container.moc.save()
        }

        // With it 'modified' only 60s ago, we shouldn't want a list fetch
        XCTAssertFalse(container.onqueueFullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // Setting it again is fine...
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], listDifference: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: [], unknownMachineIDs: Set(["aaa"]), persistentStore: description, cuttlefish: self.cuttlefish)

        // And doesn't reset the modified date on the record
        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            let aaaMOs = knownMachineMOs.filter { $0.machineID == "aaa" }
            XCTAssert(aaaMOs.count == 1, "Should have one machine MO for aaa")

            let aaaMO = aaaMOs.first!
            XCTAssertEqual(aaaMO.status, Int64(TPMachineIDStatus.unknown.rawValue), "Status of aaa MO should be 'unknown'")
            XCTAssertFalse(aaaMO.allowed, "allowed should no longer be a used field")

            XCTAssertLessThan(aaaMO.modified!, Date(timeIntervalSinceNow: -5), "Modification date of record should still be its previously on-disk value")
        }

        // And can be promoted to 'allowed'
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb"], listDifference: true), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb"]), disallowedMachineIDs: ["ccc"], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.onqueueFullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")
    }

    func testMachineIDListSetDisallowedOldUnknownMachineIDs() throws {
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
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], listDifference: true), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: [], unknownMachineIDs: Set(["aaa"]), persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.onqueueFullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // But an entry for "aaa" should exist, as a peer in the model claims it as their MID
        container.moc.performAndWait {
            let knownMachineMOs = container.containerMO.machines as? Set<MachineMO> ?? Set()

            let aaaMOs = knownMachineMOs.filter { $0.machineID == "aaa" }
            XCTAssertEqual(aaaMOs.count, 1, "Should have one machine MO for aaa")

            let aaaMO = aaaMOs.first!
            XCTAssertEqual(aaaMO.status, Int64(TPMachineIDStatus.unknown.rawValue), "Status of aaa MO should be 'unknown'")
            XCTAssertFalse(aaaMO.allowed, "allowed should no longer be a used field")

            // Pretend that aaa was added 49 hours ago
            aaaMO.modified = Date(timeIntervalSinceNow: -60*60*49)
            try! container.moc.save()
        }

        XCTAssertTrue(container.onqueueFullIDMSListWouldBeHelpful(), "Container should think it could use an IDMS list set: there's machine IDs pending removal")

        // And, setting the list again should disallow aaa, since it is so old
        // Note that this _should_ return a list difference, since A is promoted to disallowed
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], listDifference: true), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: ["aaa"], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.onqueueFullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // Setting ths list again has no change
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["bbb", "ccc"], listDifference: false), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["bbb", "ccc"]), disallowedMachineIDs: ["aaa"], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.onqueueFullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")

        // But A can appear again, no problem.
        XCTAssertNil(container.setAllowedMachineIDsSync(test: self, allowedMachineIDs: ["aaa", "bbb", "ccc"], listDifference: true), "should be able to set allowed machine IDs")
        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: [], persistentStore: description, cuttlefish: self.cuttlefish)
        XCTAssertFalse(container.onqueueFullIDMSListWouldBeHelpful(), "Container shouldn't think it could use an IDMS list set")
    }

    func testMachineIDListHandlingWithPeers() throws {
        let description = tmpStoreDescription(name: "container.db")
        let (container, peerID1) = try establish(reload: false, store: description)

        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: [], unknownMachineIDs: Set(), persistentStore: description, cuttlefish: self.cuttlefish)

        let unknownMachineID = "not-on-list"
        let (_, peerID2) = try self.joinByVoucher(sponsor: container,
                                                  containerID: "second",
                                                  machineID: unknownMachineID,
                                                  machineIDs: Set([unknownMachineID, "aaa", "bbb", "ccc"]),
                                                  store: description)

        // And the first container accepts the join...
        let (_, cUpdateError) = container.updateSync(test: self)
        XCTAssertNil(cUpdateError, "Should be able to update first container")
        assertTrusts(context: container, peerIDs: [peerID1, peerID2])

        try self.assert(container: container, allowedMachineIDs: Set(["aaa", "bbb", "ccc"]), disallowedMachineIDs: [], unknownMachineIDs: Set([unknownMachineID]), persistentStore: description, cuttlefish: self.cuttlefish)
    }

    func testContainerAndModelConsistency() throws{

        let preTestContainerName = ContainerName(container: "testToCreatePrepareData", context: "context")
        let description = tmpStoreDescription(name: "container.db")
        let containerTest = try Container(name: preTestContainerName, persistentStoreDescription: description, cuttlefish: cuttlefish)
        let (peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, error) = containerTest.prepareSync(test: self, epoch: 1, machineID: "aaa", bottleSalt: "123456789", bottleID: UUID().uuidString, modelID: "iPhone1,1")
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
            do{
                let peerKeys: OctagonSelfPeerKeys = try loadEgoKeysSync(peerID: containerMO.egoPeerID!)
                let info3: TPPeerStableInfo = TPPeerStableInfo(clock: containerEgoStableInfo!.clock + 2,
                                                               policyVersion:containerEgoStableInfo!.policyVersion,
                                                               policyHash:containerEgoStableInfo!.policyHash,
                                                               policySecrets:containerEgoStableInfo!.policySecrets,
                                                               deviceName:containerEgoStableInfo!.deviceName,
                                                               serialNumber:containerEgoStableInfo!.serialNumber,
                                                               osVersion:containerEgoStableInfo!.osVersion,
                                                               signing:peerKeys.signingKey,
                                                               recoverySigningPubKey:containerEgoStableInfo!.recoverySigningPublicKey,
                                                               recoveryEncryptionPubKey:containerEgoStableInfo!.recoveryEncryptionPublicKey,
                                                               error:nil)

                //setting the containerMO's ego stable info to an old clock
                containerMO.egoPeerStableInfo = containerEgoStableInfo!.data
                containerMO.egoPeerStableInfoSig = containerEgoStableInfo!.sig

                //now we are adding the ego stable info with a clock of 3 to the list of peers
                let peer = PeerMO(context: moc)
                peer.peerID = peerID
                peer.permanentInfo = permanentInfo
                peer.permanentInfoSig = permanentInfoSig
                peer.stableInfo = info3.data
                peer.stableInfoSig = info3.sig
                peer.isEgoPeer = true
                peer.container = containerMO

                containerMO.addToPeers(peer)

                //at this point the containerMO's egoStableInfo should have a clock of 1
                //the saved ego peer in the peer list has a clock of 3

            } catch {
                XCTFail("load ego keys failed: \(error)")
            }
            XCTAssertNoThrow(try! moc.save())
        }

        // Now TPH boots up with a preexisting model
        let container = try Container(name: containerName, persistentStoreDescription: description, cuttlefish: cuttlefish)

        let stableInfoAfterBoot = TPPeerStableInfo(data: container.containerMO.egoPeerStableInfo!, sig: container.containerMO.egoPeerStableInfoSig!)
        XCTAssertNotNil(stableInfoAfterBoot)

        //after boot the clock should be updated to the one that was saved in the model
        XCTAssertEqual(stableInfoAfterBoot!.clock, 3, "clock should be updated to 3")
    }
}
