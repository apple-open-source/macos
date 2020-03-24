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
                     policyVersion: TPPolicyVersion? = nil,
                     policySecrets: [String: Data]? = nil,
                     signingPrivateKeyPersistentRef: Data? = nil,
                     encryptionPrivateKeyPersistentRef: Data? = nil
        ) -> (String?, Data?, Data?, Data?, Data?, Set<String>?, TPPolicy?, Error?) {
        let expectation = XCTestExpectation(description: "prepare replied")
        var reta: String?, retb: Data?, retc: Data?, retd: Data?, rete: Data?, reterr: Error?
        var retviews: Set<String>?
        var retpolicy: TPPolicy?
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
        ) { a, b, c, d, e, f, g, err in
            reta = a
            retb = b
            retc = c
            retd = d
            rete = e
            retviews = f
            retpolicy = g
            reterr = err
            expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return (reta, retb, retc, retd, rete, retviews, retpolicy, reterr)
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

    func preflightVouchWithBottleSync(test: XCTestCase, bottleID: String) -> (String?, Set<String>?, TPPolicy?, Error?) {
        let expectation = XCTestExpectation(description: "preflightVouchWithBottle replied")
        var reta: String?, reterr: Error?
        var retviews: Set<String>?, retpolicy: TPPolicy?
        self.preflightVouchWithBottle(bottleID: bottleID) { a, views, policy, err in
            reta = a
            retviews = views
            retpolicy = policy
            reterr = err
            expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return (reta, retviews, retpolicy, reterr)
    }

    func vouchWithBottleSync(test: XCTestCase, b: String, entropy: Data, bottleSalt: String, tlkShares: [CKKSTLKShare]) -> (Data?, Data?, Int64, Int64, Error?) {
        let expectation = XCTestExpectation(description: "vouchWithBottle replied")
        var reta: Data?, retb: Data?, retc: Int64 = 0, retd: Int64 = 0, reterr: Error?
        self.vouchWithBottle(bottleID: b, entropy: entropy, bottleSalt: bottleSalt, tlkShares: tlkShares) { a, b, c, d, err in
            reta = a
            retb = b
            retc = c
            retd = d
            reterr = err
            expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return (reta, retb, retc, retd, reterr)
    }

    func joinSync(test: XCTestCase,
                  voucherData: Data,
                  voucherSig: Data,
                  ckksKeys: [CKKSKeychainBackedKeySet],
                  tlkShares: [CKKSTLKShare],
                  preapprovedKeys: [Data]? = nil) -> (String?, [CKRecord]?, Set<String>?, TPPolicy?, Error?) {
        let expectation = XCTestExpectation(description: "join replied")
        var reta: String?, retkhr: [CKRecord]?, reterr: Error?
        var retviews: Set<String>?, retpolicy: TPPolicy?
        self.join(voucherData: voucherData,
                  voucherSig: voucherSig,
                  ckksKeys: ckksKeys,
                  tlkShares: tlkShares,
                  preapprovedKeys: preapprovedKeys) { a, khr, views, policy, err in
                    reta = a
                    retkhr = khr
                    retviews = views
                    retpolicy = policy
                    reterr = err
                    expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return (reta, retkhr, retviews, retpolicy, reterr)
    }

    func preapprovedJoinSync(test: XCTestCase,
                             ckksKeys: [CKKSKeychainBackedKeySet],
                             tlkShares: [CKKSTLKShare],
                             preapprovedKeys: [Data]? = nil) -> (String?, [CKRecord]?, Set<String>?, TPPolicy?, Error?) {
        let expectation = XCTestExpectation(description: "preapprovedjoin replied")
        var reta: String?
        var retkhr: [CKRecord]?
        var retviews: Set<String>?
        var retpolicy: TPPolicy?
        var reterr: Error?
        self.preapprovedJoin(ckksKeys: ckksKeys,
                             tlkShares: tlkShares,
                             preapprovedKeys: preapprovedKeys) { a, khr, views, policy, err in
                                reta = a
                                retkhr = khr
                                retviews = views
                                retpolicy = policy
                                reterr = err
                                expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return (reta, retkhr, retviews, retpolicy, reterr)
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

    func setAllowedMachineIDsSync(test: XCTestCase, allowedMachineIDs: Set<String>, accountIsDemo: Bool, listDifference: Bool = true) -> (Error?) {
        let expectation = XCTestExpectation(description: "setAllowedMachineIDs replied")
        var reterr: Error?
        let honorIDMSListChanges = accountIsDemo ? false : true
        self.setAllowedMachineIDs(allowedMachineIDs, honorIDMSListChanges: honorIDMSListChanges) { differences, err in
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

    func fetchAllowedMachineIDsSync(test: XCTestCase) -> (Set<String>?, Error?) {
        let expectation = XCTestExpectation(description: "fetchMIDList replied")
        var retlist: Set<String>?
        var reterr: Error?
        self.fetchAllowedMachineIDs { list, err in
            retlist = list
            reterr = err
            expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return (retlist, reterr)
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
        var retEgoStatus = TrustedPeersHelperEgoPeerStatus(egoPeerID: nil,
                                                           status: .unknown,
                                                           viablePeerCountsByModelID: [:],
                                                           peerCountsByMachineID: [:],
                                                           isExcluded: false,
                                                           isLocked: false)
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
                                  versions: Set<TPPolicyVersion>) -> ([TPPolicyVersion: Data]?, Error?) {
        let expectation = XCTestExpectation(description: "fetchPolicyDocuments replied")
        var reta: [TPPolicyVersion: Data]?, reterr: Error?
        self.fetchPolicyDocuments(versions: versions) { a, err in
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

    func requestHealthCheckSync(requiresEscrowCheck: Bool, test: XCTestCase) -> (Bool, Bool, Bool, Bool, Error?) {
        let expectation = XCTestExpectation(description: "requestHealthCheck replied")
        var retrepairaccount: Bool = false
        var retrepairescrow: Bool = false
        var retresetoctagon: Bool = false
        var retleavetrust: Bool = false
        var reterror: Error?

        self.requestHealthCheck(requiresEscrowCheck: requiresEscrowCheck) { repairAccount, repairEscrow, resetOctagon, leaveTrust, error in
            retrepairaccount = repairAccount
            retrepairescrow = repairEscrow
            retresetoctagon = resetOctagon
            retleavetrust = leaveTrust
            reterror = error

            expectation.fulfill()
        }
        test.wait(for: [expectation], timeout: 10.0)
        return (retrepairaccount, retrepairescrow, retresetoctagon, retleavetrust, reterror)
    }
}
