//
//  SessionSync.swift
//  Security_ios
//
//  Created by Ben Williamson on 6/8/18.
//

import XCTest

extension Container {

    func dumpSync(test: XCTestCase) -> ([AnyHashable: Any]?, Error?) {
        let expectation = XCTestExpectation(description: "dump replied")
        var reta: [AnyHashable: Any]?, reterr: Error?
        self.dump { a, err in
            reta = a
            reterr = err
            expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return (reta, reterr)
    }

    func resetSync(resetReason: CuttlefishResetReason, test: XCTestCase) -> Error? {
        let expectation = XCTestExpectation(description: "reset replied")
        var reterr: Error?
        self.reset(resetReason: resetReason) { error in
            reterr = error
            expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return reterr
    }

    func localResetSync(test: XCTestCase) -> Error? {
        let expectation = XCTestExpectation(description: "reset replied")
        var reterr: Error?
        self.localReset { error in
            reterr = error
            expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return reterr
    }

    func prepareSync(test: XCTestCase,
                     epoch: UInt64,
                     machineID: String,
                     bottleSalt: String,
                     bottleID: String,
                     modelID: String,
                     deviceName: String = "test device name",
                     serialNumber: String = "456",
                     osVersion: String = "123",
                     policyVersion: UInt64? = nil,
                     policySecrets: [String: Data]? = nil,
                     signingPrivateKeyPersistentRef: Data? = nil,
                     encryptionPrivateKeyPersistentRef: Data? = nil
        ) -> (String?, Data?, Data?, Data?, Data?, Error?) {
        let expectation = XCTestExpectation(description: "prepare replied")
        var reta: String?, retb: Data?, retc: Data?, retd: Data?, rete: Data?, reterr: Error?
        self.prepare(epoch: epoch,
                     machineID: machineID,
                     bottleSalt: bottleSalt,
                     bottleID: bottleID,
                     modelID: modelID,
                     deviceName: deviceName,
                     serialNumber: serialNumber,
                     osVersion: osVersion,
                     policyVersion: policyVersion,
                     policySecrets: policySecrets,
                     signingPrivateKeyPersistentRef: signingPrivateKeyPersistentRef,
                     encryptionPrivateKeyPersistentRef: encryptionPrivateKeyPersistentRef
        ) { a, b, c, d, e, err in
            reta = a
            retb = b
            retc = c
            retd = d
            rete = e
            reterr = err
            expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return (reta, retb, retc, retd, rete, reterr)
    }

    func establishSync(test: XCTestCase,
                       ckksKeys: [CKKSKeychainBackedKeySet],
                       tlkShares: [CKKSTLKShare],
                       preapprovedKeys: [Data]?) -> (String?, [CKRecord], Error?) {
        let expectation = XCTestExpectation(description: "prepare replied")
        var reta: String?, retkhr: [CKRecord]?, reterr: Error?
        self.establish(ckksKeys: ckksKeys,
                       tlkShares: tlkShares,
                       preapprovedKeys: preapprovedKeys) { a, khr, err in
                        reta = a
                        retkhr = khr
                        reterr = err
                        expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return (reta, retkhr!, reterr)
    }

    func vouchSync(test: XCTestCase,
                   peerID: String,
                   permanentInfo: Data,
                   permanentInfoSig: Data,
                   stableInfo: Data,
                   stableInfoSig: Data,
                   ckksKeys: [CKKSKeychainBackedKeySet]) -> (Data?, Data?, Error?) {
        let expectation = XCTestExpectation(description: "vouch replied")
        var reta: Data?, retb: Data?, reterr: Error?
        self.vouch(peerID: peerID,
                   permanentInfo: permanentInfo,
                   permanentInfoSig: permanentInfoSig,
                   stableInfo: stableInfo,
                   stableInfoSig: stableInfoSig,
                   ckksKeys: ckksKeys) { a, b, err in
                    reta = a
                    retb = b
                    reterr = err
                    expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return (reta, retb, reterr)
    }

    func preflightVouchWithBottleSync(test: XCTestCase, bottleID: String) -> (String?, Error?) {
        let expectation = XCTestExpectation(description: "preflightVouchWithBottle replied")
        var reta: String?, reterr: Error?
        self.preflightVouchWithBottle(bottleID: bottleID) { a, err in
            reta = a
            reterr = err
            expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return (reta, reterr)
    }

    func vouchWithBottleSync(test: XCTestCase, b: String, entropy: Data, bottleSalt: String, tlkShares: [CKKSTLKShare]) -> (Data?, Data?, Error?) {
        let expectation = XCTestExpectation(description: "vouchWithBottle replied")
        var reta: Data?, retb: Data?, reterr: Error?
        self.vouchWithBottle(bottleID: b, entropy: entropy, bottleSalt: bottleSalt, tlkShares: tlkShares) { a, b, err in
            reta = a
            retb = b
            reterr = err
            expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return (reta, retb, reterr)
    }

    func joinSync(test: XCTestCase,
                  voucherData: Data,
                  voucherSig: Data,
                  ckksKeys: [CKKSKeychainBackedKeySet],
                  tlkShares: [CKKSTLKShare],
                  preapprovedKeys: [Data]? = nil) -> (String?, [CKRecord]?, Error?) {
        let expectation = XCTestExpectation(description: "join replied")
        var reta: String?, retkhr: [CKRecord]?, reterr: Error?
        self.join(voucherData: voucherData,
                  voucherSig: voucherSig,
                  ckksKeys: ckksKeys,
                  tlkShares: tlkShares,
                  preapprovedKeys: preapprovedKeys) { a, khr, err in
                    reta = a
                    retkhr = khr
                    reterr = err
                    expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return (reta, retkhr, reterr)
    }

    func preapprovedJoinSync(test: XCTestCase,
                             ckksKeys: [CKKSKeychainBackedKeySet],
                             tlkShares: [CKKSTLKShare],
                             preapprovedKeys: [Data]? = nil) -> (String?, [CKRecord]?, Error?) {
        let expectation = XCTestExpectation(description: "preapprovedjoin replied")
        var reta: String?
        var retkhr: [CKRecord]?
        var reterr: Error?
        self.preapprovedJoin(ckksKeys: ckksKeys,
                             tlkShares: tlkShares,
                             preapprovedKeys: preapprovedKeys) { a, khr, err in
                                reta = a
                                retkhr = khr
                                reterr = err
                                expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return (reta, retkhr, reterr)
    }

    func updateSync(test: XCTestCase,
                    deviceName: String? = nil,
                    serialNumner: String? = nil,
                    osVersion: String? = nil,
                    policyVersion: UInt64? = nil,
                    policySecrets: [String: Data]? = nil) -> (TrustedPeersHelperPeerState?, Error?) {
        let expectation = XCTestExpectation(description: "update replied")
        var reterr: Error?
        var retstate: TrustedPeersHelperPeerState?
        self.update(deviceName: deviceName,
                    serialNumber: serialNumner,
                    osVersion: osVersion,
                    policyVersion: policyVersion,
                    policySecrets: policySecrets) { state, err in
                        retstate = state
                        reterr = err
                        expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return (retstate, reterr)
    }

    func setAllowedMachineIDsSync(test: XCTestCase, allowedMachineIDs: Set<String>, listDifference: Bool = true) -> (Error?) {
        let expectation = XCTestExpectation(description: "setAllowedMachineIDs replied")
        var reterr: Error?
        self.setAllowedMachineIDs(allowedMachineIDs) { differences, err in
            XCTAssertEqual(differences, listDifference, "Reported list difference should match expectation")
            reterr = err
            expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return reterr
    }

    func addAllowedMachineIDsSync(test: XCTestCase, machineIDs: [String]) -> Error? {
        let expectation = XCTestExpectation(description: "addAllow replied")
        var reterr: Error?
        self.addAllow(machineIDs) { err in
            reterr = err
            expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return reterr
    }

    func removeAllowedMachineIDsSync(test: XCTestCase, machineIDs: [String]) -> Error? {
        let expectation = XCTestExpectation(description: "removeAllow replied")
        var reterr: Error?
        self.removeAllow(machineIDs) { err in
            reterr = err
            expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return reterr
    }

    func departByDistrustingSelfSync(test: XCTestCase) -> Error? {
        let expectation = XCTestExpectation(description: "departByDistrustingSelf replied")
        var reterr: Error?
        self.departByDistrustingSelf { error in
            reterr = error
            expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return reterr
    }

    func distrustSync(test: XCTestCase, peerIDs: Set<String>) -> Error? {
        let expectation = XCTestExpectation(description: "distrustSync replied")
        var reterr: Error?
        self.distrust(peerIDs: peerIDs) { error in
            reterr = error
            expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return reterr
    }

    func getStateSync(test: XCTestCase) -> ContainerState {
        let expectation = XCTestExpectation(description: "getState replied")
        var retstate: ContainerState?
        self.getState { state in
            retstate = state
            expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return retstate!
    }

    func loadSecretSync(test: XCTestCase,
                        label: String) -> (Data?) {
        var secret: Data?
        do {
            secret = try loadSecret(label: label)
        } catch {

        }
        return secret
    }

    func setRecoveryKeySync(test: XCTestCase, recoveryKey: String, recoverySalt: String, ckksKeys: [CKKSKeychainBackedKeySet]) -> (Error?) {
        let expectation = XCTestExpectation(description: "setRecoveryKey replied")
        var reterr: Error?

        self.setRecoveryKey(recoveryKey: recoveryKey, salt: recoverySalt, ckksKeys: ckksKeys) { error in
            reterr = error
            expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return (reterr)
    }

    func fetchViableBottlesSync(test: XCTestCase) -> ([String]?, [String]?, Error?) {
        let expectation = XCTestExpectation(description: "fetchViableBottles replied")
        var retescrowRecordIDs: [String]?
        var retpartialEscrowRecordIDs: [String]?
        var reterror: Error?
        self.fetchViableBottles { escrowRecordIDs, partialEscrowRecordIDs, error in
            retescrowRecordIDs = escrowRecordIDs
            retpartialEscrowRecordIDs = partialEscrowRecordIDs
            reterror = error
            expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return (retescrowRecordIDs, retpartialEscrowRecordIDs, reterror)
    }

    func trustStatusSync(test: XCTestCase) -> (TrustedPeersHelperEgoPeerStatus, Error?) {
        let expectation = XCTestExpectation(description: "trustStatus replied")
        var retEgoStatus = TrustedPeersHelperEgoPeerStatus(egoPeerID: nil, status: .unknown, viablePeerCountsByModelID: [:], isExcluded: false, isLocked: false)
        var reterror: Error?
        self.trustStatus { egoStatus, error in
            retEgoStatus = egoStatus
            reterror = error
            expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return (retEgoStatus, reterror)
    }

    func fetchPolicyDocumentsSync(test: XCTestCase,
                                  keys: [NSNumber: String]) -> ([NSNumber: [String]]?, Error?) {
        let expectation = XCTestExpectation(description: "fetchPolicyDocuments replied")
        var reta: [NSNumber: [String]]?, reterr: Error?
        self.fetchPolicyDocuments(keys: keys) { a, err in
            reta = a
            reterr = err
            expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return (reta, reterr)
    }

    func fetchEscrowContentsSync(test: XCTestCase) -> (Data?, String?, Data?, Error?) {
        let expectation = XCTestExpectation(description: "fetchEscrowContents replied")
        var retentropy: Data?
        var retbottleID: String?
        var retspki: Data?
        var reterror: Error?

        self.fetchEscrowContents { entropy, bottleID, spki, error in
            retentropy = entropy
            retbottleID = bottleID
            retspki = spki
            reterror = error

            expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return (retentropy, retbottleID, retspki, reterror)
    }

    func requestHealthCheckSync(requiresEscrowCheck: Bool, test: XCTestCase) -> (Bool, Bool, Bool, Error?) {
        let expectation = XCTestExpectation(description: "requestHealthCheck replied")
        var retrepairaccount: Bool = false
        var retrepairescrow: Bool = false
        var retresetoctagon: Bool = false
        var reterror: Error?

        self.requestHealthCheck(requiresEscrowCheck: requiresEscrowCheck) { repairAccount, repairEscrow, resetOctagon, error in
            retrepairaccount = repairAccount
            retrepairescrow = repairEscrow
            retresetoctagon = resetOctagon
            reterror = error

            expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return (retrepairaccount, retrepairescrow, retresetoctagon, reterror)
    }
}
