/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

import Foundation

private let logger = Logger(subsystem: "com.apple.security.trustedpeers", category: "client")

extension Error {
    func sanitizeForClientXPC() -> Error {
        let nserror = self as NSError

        // CoreData errors might have extra things in them that need removal.
        if nserror.domain == NSCocoaErrorDomain {
            return nserror.cleanAllButDescription()
        }

        // The docs for CKXPCSuitableError say it only returns nil if you pass it nil, but swift can't read those.
        guard let ckCleanedError = CKXPCSuitableError(self) else {
            return ContainerError.unknownCloudKitError
        }
        return ckCleanedError
    }
}

extension NSError {
    func cleanAllButDescription() -> NSError {
        let userInfo: [String: AnyHashable]?
        if let description = self.userInfo[NSLocalizedDescriptionKey] as? AnyHashable {
            userInfo = [NSLocalizedDescriptionKey: description]
        } else {
            userInfo = nil
        }

        return NSError(domain: self.domain,
                       code: self.code,
                       userInfo: userInfo)
    }
}

class Client: TrustedPeersHelperProtocol {

    let endpoint: NSXPCListenerEndpoint?
    let containerMap: ContainerMap

    init(endpoint: NSXPCListenerEndpoint?, containerMap: ContainerMap) {
        self.endpoint = endpoint
        self.containerMap = containerMap
    }

    func ping(reply: @escaping (() -> Void)) {
        reply()
    }

    func logComplete(function: String, container: ContainerName, error: Error?) {
        if let error = error {
            logger.info("\(function, privacy: .public) errored for \(container.description, privacy: .public): \(String(describing: error), privacy: .public)")
        } else {
            logger.info("\(function, privacy: .public) finished for \(container.description, privacy: .public)")
        }
    }

    internal func getContainer(with user: TPSpecificUser?) throws -> Container {
        return try self.containerMap.findOrCreate(user: user)
    }

    func dump(with user: TPSpecificUser?,
              reply: @escaping ([AnyHashable: Any]?, Error?) -> Void) {
        do {
            logger.info("Dumping for \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.dump { result, error in
                self.logComplete(function: "Dumping", container: container.name, error: error)
                reply(result, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("Dumping failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, error.sanitizeForClientXPC())
        }
    }

    func dumpEgoPeer(with user: TPSpecificUser?,
                     reply: @escaping (String?, TPPeerPermanentInfo?, TPPeerStableInfo?, TPPeerDynamicInfo?, Error?) -> Void) {
        do {
            logger.info("Dumping peer for \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.dumpEgoPeer { peerID, perm, stable, dyn, error in
                self.logComplete(function: "Dumping peer", container: container.name, error: error)
                reply(peerID, perm, stable, dyn, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("Dumping peer failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, nil, nil, error.sanitizeForClientXPC())
        }
    }

    func trustStatus(with user: TPSpecificUser?,
                     reply: @escaping (TrustedPeersHelperEgoPeerStatus, Error?) -> Void) {
        do {
            let container = try self.containerMap.findOrCreate(user: user)
            container.trustStatus { egoPeerStatus, error in
                reply(egoPeerStatus, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("Trust status failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(TrustedPeersHelperEgoPeerStatus(egoPeerID: nil,
                                                  egoPeerMachineID: nil,
                                                  status: TPPeerStatus.unknown,
                                                  viablePeerCountsByModelID: [:],
                                                  peerCountsByMachineID: [:],
                                                  isExcluded: false,
                                                  isLocked: false),
                  error.sanitizeForClientXPC())
        }
    }

    func fetchTrustState(with user: TPSpecificUser?, reply: @escaping (TrustedPeersHelperPeerState?, [TrustedPeersHelperPeer]?, Error?) -> Void) {
        do {
            logger.info("Fetch Trust State for \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.fetchTrustState { peerState, peerList, error in
                reply(peerState, peerList, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("Fetch Trust State failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, error.sanitizeForClientXPC())
        }
    }

    func reset(with user: TPSpecificUser?, resetReason: CuttlefishResetReason, reply: @escaping (Error?) -> Void) {
        do {
            logger.info("Resetting for \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.reset(resetReason: resetReason) { error in
                self.logComplete(function: "Resetting", container: container.name, error: error)
                reply(error?.sanitizeForClientXPC()) }
        } catch {
            logger.info("Resetting failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(error.sanitizeForClientXPC())
        }
    }

    func localReset(with user: TPSpecificUser?, reply: @escaping (Error?) -> Void) {
        do {
            logger.info("Performing local reset for \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.localReset { error in
                self.logComplete(function: "Local reset", container: container.name, error: error)
                reply(error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("Local reset failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(error.sanitizeForClientXPC())
        }
    }

    func setAllowedMachineIDsWith(_ user: TPSpecificUser?,
                                  allowedMachineIDs: Set<String>,
                                  honorIDMSListChanges: Bool,
                                  reply: @escaping (Bool, Error?) -> Void) {
        do {
            logger.info("Setting allowed machineIDs for \(String(describing: user), privacy: .public) to \(allowedMachineIDs, privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.setAllowedMachineIDs(allowedMachineIDs, honorIDMSListChanges: honorIDMSListChanges) { differences, error in
                self.logComplete(function: "Setting allowed machineIDs", container: container.name, error: error)
                reply(differences, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("Setting allowed machineIDs failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(false, error.sanitizeForClientXPC())
        }
    }

    func addAllowedMachineIDs(with user: TPSpecificUser?,
                              machineIDs: [String],
                              reply: @escaping (Error?) -> Void) {
        do {
            logger.info("Adding allowed machineIDs for \(String(describing: user), privacy: .public): \(machineIDs, privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.addAllow(machineIDs) { error in
                self.logComplete(function: "Adding allowed machineIDs", container: container.name, error: error)
                reply(error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("Adding allowed machineID failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(error.sanitizeForClientXPC())
        }
    }

    func removeAllowedMachineIDs(with user: TPSpecificUser?,
                                 machineIDs: [String],
                                 reply: @escaping (Error?) -> Void) {
        do {
            logger.info("Removing allowed machineIDs for \(String(describing: user), privacy: .public): \(machineIDs, privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.removeAllow(machineIDs) { error in
                self.logComplete(function: "Removing allowed machineIDs", container: container.name, error: error)
                reply(error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("Removing allowed machineID failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(error.sanitizeForClientXPC())
        }
    }

    func fetchAllowedMachineIDs(with user: TPSpecificUser?, reply: @escaping (Set<String>?, Error?) -> Void) {
        do {
            logger.info("Fetching allowed machineIDs for \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.fetchAllowedMachineIDs { mids, error in
                self.logComplete(function: "Fetched allowed machineIDs", container: container.name, error: error)
                reply(mids, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("Fetching allowed machineIDs failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, error.sanitizeForClientXPC())
        }
    }

    func fetchEgoEpoch(with user: TPSpecificUser?, reply: @escaping (UInt64, Error?) -> Void) {
        do {
            logger.info("retrieving epoch for \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.getEgoEpoch { epoch, error in
                reply(epoch, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("Epoch retrieval failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(0, error.sanitizeForClientXPC())
        }
    }

    func prepare(with user: TPSpecificUser?,
                 epoch: UInt64,
                 machineID: String,
                 bottleSalt: String,
                 bottleID: String,
                 modelID: String,
                 deviceName: String?,
                 serialNumber: String?,
                 osVersion: String,
                 policyVersion: TPPolicyVersion?,
                 policySecrets: [String: Data]?,
                 syncUserControllableViews: TPPBPeerStableInfoUserControllableViewStatus,
                 secureElementIdentity: TPPBSecureElementIdentity?,
                 setting: OTAccountSettingsX?,
                 signingPrivKeyPersistentRef: Data?,
                 encPrivKeyPersistentRef: Data?,
                 reply: @escaping (String?, Data?, Data?, Data?, Data?, TPSyncingPolicy?, Error?) -> Void) {
        do {
            logger.info("Preparing new identity for \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.prepare(epoch: epoch,
                              machineID: machineID,
                              bottleSalt: bottleSalt,
                              bottleID: bottleID,
                              modelID: modelID,
                              deviceName: deviceName,
                              serialNumber: serialNumber,
                              osVersion: osVersion,
                              policyVersion: policyVersion,
                              policySecrets: policySecrets,
                              syncUserControllableViews: syncUserControllableViews,
                              secureElementIdentity: secureElementIdentity,
                              setting: setting,
                              signingPrivateKeyPersistentRef: signingPrivKeyPersistentRef,
                              encryptionPrivateKeyPersistentRef: encPrivKeyPersistentRef) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, policy, error in
                self.logComplete(function: "Prepare", container: container.name, error: error)
                reply(peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, policy, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("Prepare failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, nil, nil, nil, nil, error.sanitizeForClientXPC())
        }
    }

    func prepareInheritancePeer(with user: TPSpecificUser?,
                                epoch: UInt64,
                                machineID: String,
                                bottleSalt: String,
                                bottleID: String,
                                modelID: String,
                                deviceName: String?,
                                serialNumber: String?,
                                osVersion: String,
                                policyVersion: TPPolicyVersion?,
                                policySecrets: [String: Data]?,
                                syncUserControllableViews: TPPBPeerStableInfoUserControllableViewStatus,
                                secureElementIdentity: TPPBSecureElementIdentity?,
                                signingPrivKeyPersistentRef: Data?,
                                encPrivKeyPersistentRef: Data?,
                                crk: TrustedPeersHelperCustodianRecoveryKey,
                                reply: @escaping (String?, Data?, Data?, Data?, Data?, TPSyncingPolicy?, String?, [CKRecord]?, Error?) -> Void) {
        do {
            logger.info("Preparing new identity for inheritance peer \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.prepareInheritancePeer(epoch: epoch,
                                             machineID: machineID,
                                             bottleSalt: bottleSalt,
                                             bottleID: bottleID,
                                             modelID: modelID,
                                             deviceName: deviceName,
                                             serialNumber: serialNumber,
                                             osVersion: osVersion,
                                             policyVersion: policyVersion,
                                             policySecrets: policySecrets,
                                             syncUserControllableViews: syncUserControllableViews,
                                             secureElementIdentity: secureElementIdentity,
                                             signingPrivateKeyPersistentRef: signingPrivKeyPersistentRef,
                                             encryptionPrivateKeyPersistentRef: encPrivKeyPersistentRef,
                                             crk: crk) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, policy, recoveryKeyID, tlkShares, error in
                self.logComplete(function: "prepareInheritancePeer", container: container.name, error: error)
                reply(peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, policy, recoveryKeyID, tlkShares, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("prepareInheritancePeer failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, nil, nil, nil, nil, nil, nil, error.sanitizeForClientXPC())
        }
    }

    func establish(with user: TPSpecificUser?,
                   ckksKeys: [CKKSKeychainBackedKeySet],
                   tlkShares: [CKKSTLKShare],
                   preapprovedKeys: [Data]?,
                   reply: @escaping (String?, [CKRecord]?, TPSyncingPolicy?, Error?) -> Void) {
        do {
            logger.info("Establishing \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.establish(ckksKeys: ckksKeys,
                                tlkShares: tlkShares,
                                preapprovedKeys: preapprovedKeys) { peerID, keyHierarchyRecords, policy, error in
                                    self.logComplete(function: "Establishing", container: container.name, error: error)
                                    reply(peerID, keyHierarchyRecords, policy, error?.sanitizeForClientXPC()) }
        } catch {
            logger.info("Establishing failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, nil, error.sanitizeForClientXPC())
        }
    }

    func vouch(with user: TPSpecificUser?,
               peerID: String,
               permanentInfo: Data,
               permanentInfoSig: Data,
               stableInfo: Data,
               stableInfoSig: Data,
               ckksKeys: [CKKSKeychainBackedKeySet],
               reply: @escaping (Data?, Data?, Error?) -> Void) {
        do {
            logger.info("Vouching \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.vouch(peerID: peerID,
                            permanentInfo: permanentInfo,
                            permanentInfoSig: permanentInfoSig,
                            stableInfo: stableInfo,
                            stableInfoSig: stableInfoSig,
                            ckksKeys: ckksKeys) { voucher, voucherSig, error in
                                self.logComplete(function: "Vouching", container: container.name, error: error)
                                reply(voucher, voucherSig, error?.sanitizeForClientXPC()) }
        } catch {
            logger.info("Vouching failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, error.sanitizeForClientXPC())
        }
    }

    func preflightVouchWithBottle(with user: TPSpecificUser?,
                                  bottleID: String,
                                  reply: @escaping (String?, TPSyncingPolicy?, Bool, Error?) -> Void) {
        do {
            logger.info("Preflight Vouch With Bottle \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.preflightVouchWithBottle(bottleID: bottleID) { peerID, policy, refetched, error in
                self.logComplete(function: "Preflight Vouch With Bottle", container: container.name, error: error)
                reply(peerID, policy, refetched, error?.sanitizeForClientXPC()) }
        } catch {
            logger.info("Preflighting Vouch With Bottle failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, false, error.sanitizeForClientXPC())
        }
    }

    func vouchWithBottle(with user: TPSpecificUser?,
                         bottleID: String,
                         entropy: Data,
                         bottleSalt: String,
                         tlkShares: [CKKSTLKShare],
                         reply: @escaping (Data?, Data?, [CKKSTLKShare]?, TrustedPeersHelperTLKRecoveryResult?, Error?) -> Void) {
        do {
            logger.info("Vouching With Bottle \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.vouchWithBottle(bottleID: bottleID, entropy: entropy, bottleSalt: bottleSalt, tlkShares: tlkShares) { voucher, voucherSig, newTLKShares, recoveryResult, error in
                self.logComplete(function: "Vouching With Bottle", container: container.name, error: error)
                reply(voucher, voucherSig, newTLKShares, recoveryResult, error?.sanitizeForClientXPC()) }
        } catch {
            logger.info("Vouching with Bottle failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, nil, nil, error.sanitizeForClientXPC())
        }
    }

    func preflightVouchWithRecoveryKey(with user: TPSpecificUser?,
                                       recoveryKey: String,
                                       salt: String,
                                       reply: @escaping (String?, TPSyncingPolicy?, Error?) -> Void) {
        do {
            logger.info("Preflight Vouch With RecoveryKey \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.preflightVouchWithRecoveryKey(recoveryKey: recoveryKey, salt: salt) { rkID, policy, error in
                self.logComplete(function: "Preflight Vouch With RecoveryKey", container: container.name, error: error)
                reply(rkID, policy, error?.sanitizeForClientXPC()) }
        } catch {
            logger.info("Preflighting Vouch With RecoveryKey failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, error.sanitizeForClientXPC())
        }
    }

    func preflightVouchWithCustodianRecoveryKey(with user: TPSpecificUser?,
                                                crk: TrustedPeersHelperCustodianRecoveryKey,
                                                reply: @escaping (String?, TPSyncingPolicy?, Error?) -> Void) {
        do {
            logger.info("Preflight Vouch With CustodianRecoveryKey \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.preflightVouchWithCustodianRecoveryKey(crk: crk) { peerID, policy, error in
                self.logComplete(function: "Preflight Vouch With CustodianRecoveryKey", container: container.name, error: error)
                reply(peerID, policy, error?.sanitizeForClientXPC()) }
        } catch {
            logger.info("Preflighting Vouch With CustodianRecoveryKey failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, error.sanitizeForClientXPC())
        }
    }

    func vouchWithRecoveryKey(with user: TPSpecificUser?,
                              recoveryKey: String,
                              salt: String,
                              tlkShares: [CKKSTLKShare],
                              reply: @escaping (Data?, Data?, [CKKSTLKShare]?, TrustedPeersHelperTLKRecoveryResult?, Error?) -> Void) {
        do {
            logger.info("Vouching With Recovery Key \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.vouchWithRecoveryKey(recoveryKey: recoveryKey, salt: salt, tlkShares: tlkShares) { voucher, voucherSig, newTLKShares, recoveryResult, error in
                self.logComplete(function: "Vouching With Recovery Key", container: container.name, error: error)
                reply(voucher, voucherSig, newTLKShares, recoveryResult, error?.sanitizeForClientXPC()) }
        } catch {
            logger.info("Vouching with Recovery Key failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, nil, nil, error.sanitizeForClientXPC())
        }
    }

    func recoverTLKSharesForInheritor(with user: TPSpecificUser?,
                                      crk: TrustedPeersHelperCustodianRecoveryKey,
                                      tlkShares: [CKKSTLKShare],
                                      reply: @escaping ([CKKSTLKShare]?, TrustedPeersHelperTLKRecoveryResult?, Error?) -> Void) {
        do {
            logger.info("Recovering TLKShares for Inheritor \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.recoverTLKSharesForInheritor(crk: crk, tlkShares: tlkShares) { newTLKShares, recoveryResult, error in
                self.logComplete(function: "Recovering TLKShares for Inheritor", container: container.name, error: error)
                reply(newTLKShares, recoveryResult, error?.sanitizeForClientXPC()) }
        } catch {
            logger.info("Recovering TLKShares for Inheritor failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, error.sanitizeForClientXPC())
        }
    }

    func vouchWithCustodianRecoveryKey(with user: TPSpecificUser?,
                                       crk: TrustedPeersHelperCustodianRecoveryKey,
                                       tlkShares: [CKKSTLKShare],
                                       reply: @escaping (Data?, Data?, [CKKSTLKShare]?, TrustedPeersHelperTLKRecoveryResult?, Error?) -> Void) {
        do {
            logger.info("Vouching With Custodian Recovery Key \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.vouchWithCustodianRecoveryKey(crk: crk, tlkShares: tlkShares) { voucher, voucherSig, newTLKShares, recoveryResult, error in
                self.logComplete(function: "Vouching With Custodian Recovery Key", container: container.name, error: error)
                reply(voucher, voucherSig, newTLKShares, recoveryResult, error?.sanitizeForClientXPC()) }
        } catch {
            logger.info("Vouching with Custodian Recovery Key failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, nil, nil, error.sanitizeForClientXPC())
        }
    }

    func join(with user: TPSpecificUser?,
              voucherData: Data,
              voucherSig: Data,
              ckksKeys: [CKKSKeychainBackedKeySet],
              tlkShares: [CKKSTLKShare],
              preapprovedKeys: [Data]?,
              reply: @escaping (String?, [CKRecord]?, TPSyncingPolicy?, Error?) -> Void) {
        do {
            logger.info("Joining \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.join(voucherData: voucherData,
                           voucherSig: voucherSig,
                           ckksKeys: ckksKeys,
                           tlkShares: tlkShares,
                           preapprovedKeys: preapprovedKeys) { peerID, keyHierarchyRecords, policy, error in
                            reply(peerID, keyHierarchyRecords, policy, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("Joining failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, nil, error.sanitizeForClientXPC())
        }
    }

    func preflightPreapprovedJoin(with user: TPSpecificUser?,
                                  preapprovedKeys: [Data]?,
                                  reply: @escaping (Bool, Error?) -> Void) {
        do {
            logger.info("Attempting to preflight a preapproved join for \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.preflightPreapprovedJoin(preapprovedKeys: preapprovedKeys) { success, error in reply(success, error?.sanitizeForClientXPC()) }
        } catch {
            logger.info("preflightPreapprovedJoin failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(false, error.sanitizeForClientXPC())
        }
    }

    func attemptPreapprovedJoin(with user: TPSpecificUser?,
                                ckksKeys: [CKKSKeychainBackedKeySet],
                                tlkShares: [CKKSTLKShare],
                                preapprovedKeys: [Data]?,
                                reply: @escaping (String?, [CKRecord]?, TPSyncingPolicy?, Error?) -> Void) {
        do {
            logger.info("Attempting a preapproved join for \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.preapprovedJoin(ckksKeys: ckksKeys,
                                      tlkShares: tlkShares,
                                      preapprovedKeys: preapprovedKeys) { peerID, keyHierarchyRecords, policy, error in
                                        reply(peerID, keyHierarchyRecords, policy, error?.sanitizeForClientXPC()) }
        } catch {
            logger.info("attemptPreapprovedJoin failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, nil, error.sanitizeForClientXPC())
        }
    }

    func update(with user: TPSpecificUser?,
                forceRefetch: Bool,
                deviceName: String?,
                serialNumber: String?,
                osVersion: String?,
                policyVersion: NSNumber?,
                policySecrets: [String: Data]?,
                syncUserControllableViews: NSNumber?,
                secureElementIdentity: TrustedPeersHelperIntendedTPPBSecureElementIdentity?,
                reply: @escaping (TrustedPeersHelperPeerState?, TPSyncingPolicy?, Error?) -> Void) {
        do {
            logger.info("Updating \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)

            let syncUserControllableSetting: TPPBPeerStableInfoUserControllableViewStatus?
            if let value = syncUserControllableViews?.int32Value {
                switch value {
                case TPPBPeerStableInfoUserControllableViewStatus.ENABLED.rawValue:
                    syncUserControllableSetting = .ENABLED
                case TPPBPeerStableInfoUserControllableViewStatus.DISABLED.rawValue:
                    syncUserControllableSetting = .DISABLED
                case TPPBPeerStableInfoUserControllableViewStatus.FOLLOWING.rawValue:
                    syncUserControllableSetting = .FOLLOWING
                default:
                    throw ContainerError.unknownSyncUserControllableViewsValue(value: value)
                }
            } else {
                syncUserControllableSetting = nil
            }

            container.update(forceRefetch: forceRefetch,
                             deviceName: deviceName,
                             serialNumber: serialNumber,
                             osVersion: osVersion,
                             policyVersion: policyVersion?.uint64Value,
                             policySecrets: policySecrets,
                             syncUserControllableViews: syncUserControllableSetting,
                             secureElementIdentity: secureElementIdentity
                             ) { state, policy, error in reply(state, policy, error?.sanitizeForClientXPC()) }
        } catch {
            logger.info("update failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, error.sanitizeForClientXPC())
        }
    }

    func setPreapprovedKeysWith(_ user: TPSpecificUser?,
                                preapprovedKeys: [Data],
                                reply: @escaping (TrustedPeersHelperPeerState?, Error?) -> Void) {
        do {
            logger.info("setPreapprovedKeys \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.set(preapprovedKeys: preapprovedKeys) { state, error in reply(state, error?.sanitizeForClientXPC()) }
        } catch {
            logger.info("setPreapprovedKeys failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, error.sanitizeForClientXPC())
        }
    }

    func updateTLKs(with user: TPSpecificUser?,
                    ckksKeys: [CKKSKeychainBackedKeySet],
                    tlkShares: [CKKSTLKShare],
                    reply: @escaping ([CKRecord]?, Error?) -> Void) {
        do {
            logger.info("Updating TLKs for \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.updateTLKs(ckksKeys: ckksKeys,
                                 tlkShares: tlkShares) { records, error in
                reply(records, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("updateTLKs failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, error.sanitizeForClientXPC())
        }
    }

    func departByDistrustingSelf(with user: TPSpecificUser?,
                                 reply: @escaping (Error?) -> Void) {
        do {
            logger.info("Departing \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.departByDistrustingSelf { error in
                reply(error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("departByDistrustingSelf failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(error.sanitizeForClientXPC())
        }
    }

    func distrustPeerIDs(with user: TPSpecificUser?,
                         peerIDs: Set<String>,
                         reply: @escaping (Error?) -> Void) {
        do {
            logger.info("Distrusting \(peerIDs, privacy: .public) in \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.distrust(peerIDs: peerIDs) { error in
                reply(error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("distrustPeerIDs failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(error.sanitizeForClientXPC())
        }
    }

    func fetchViableBottles(with user: TPSpecificUser?, reply: @escaping ([String]?, [String]?, Error?) -> Void) {
        do {
            logger.info("fetchViableBottles in \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.fetchViableBottles { sortedBottleIDs, partialBottleIDs, error in
                reply(sortedBottleIDs, partialBottleIDs, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("fetchViableBottles failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, error.sanitizeForClientXPC())
        }
    }

    func fetchViableEscrowRecords(with user: TPSpecificUser?, forceFetch: Bool, reply: @escaping ([Data]?, Error?) -> Void) {
        do {
            logger.info("fetchViableEscrowRecords in \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.fetchEscrowRecords(forceFetch: forceFetch) { recordDatas, error in
                reply(recordDatas, error?.sanitizeForClientXPC())
            }
        } catch {
            reply(nil, error.sanitizeForClientXPC())
        }
    }

    func fetchEscrowContents(with user: TPSpecificUser?, reply: @escaping (Data?, String?, Data?, Error?) -> Void) {
        do {
            logger.info("fetchEscrowContents in \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.fetchEscrowContents { entropy, bottleID, signingPublicKey, error in
                reply(entropy, bottleID, signingPublicKey, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("fetchEscrowContents failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, nil, error.sanitizeForClientXPC())
        }
    }

    func fetchCurrentPolicy(with user: TPSpecificUser?,
                            modelIDOverride: String?,
                            isInheritedAccount: Bool,
                            reply: @escaping (TPSyncingPolicy?, TPPBPeerStableInfoUserControllableViewStatus, Error?) -> Void) {
        do {
            logger.info("Fetching policy+views for \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.fetchCurrentPolicy(modelIDOverride: modelIDOverride, isInheritedAccount: isInheritedAccount) { policy, peersOpinion, error in
                reply(policy, peersOpinion, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("fetchCurrentPolicy failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, .UNKNOWN, error.sanitizeForClientXPC())
        }
    }

    func fetchPolicyDocuments(with user: TPSpecificUser?,
                              versions: Set<TPPolicyVersion>,
                              reply: @escaping ([TPPolicyVersion: Data]?, Error?) -> Void) {
        do {
            logger.info("Fetching policy documents \(String(describing: user), privacy: .public) with versions: \(versions, privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.fetchPolicyDocuments(versions: versions) { entries, error in
                reply(entries, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("fetchPolicyDocuments failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, error.sanitizeForClientXPC())
        }
    }

    func fetchRecoverableTLKShares(with user: TPSpecificUser?,
                                   peerID: String?,
                                   reply: @escaping ([CKRecord]?, Error?) -> Void) {
        do {
            logger.info("Fetching recoverable TLKShares \(String(describing: user), privacy: .public) with peerID filter: \(String(describing: peerID), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)

            container.fetchRecoverableTLKShares(peerID: peerID) { records, error in
                reply(records, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("fetchRecoverableTLKShares failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, error.sanitizeForClientXPC())
        }
    }

    func validatePeers(with user: TPSpecificUser?, reply: @escaping ([AnyHashable: Any]?, Error?) -> Void) {
        do {
            logger.info("ValidatePeers for \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            let request = ValidatePeersRequest()
            container.validatePeers(request: request) { result, error in
                self.logComplete(function: "validatePeers", container: container.name, error: error)
                reply(result, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("ValidatePeers failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, error.sanitizeForClientXPC())
        }
    }

    func setRecoveryKeyWith(_ user: TPSpecificUser?,
                            recoveryKey: String,
                            salt: String,
                            ckksKeys: [CKKSKeychainBackedKeySet],
                            reply: @escaping ([CKRecord]?, Error?) -> Void) {
        do {
            logger.info("SetRecoveryKey for \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.setRecoveryKey(recoveryKey: recoveryKey, salt: salt, ckksKeys: ckksKeys) { records, error in
                self.logComplete(function: "setRecoveryKey", container: container.name, error: error)
                reply(records, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("SetRecoveryKey failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, error.sanitizeForClientXPC())
        }
    }

    func createCustodianRecoveryKey(with user: TPSpecificUser?,
                                    recoveryKey: String,
                                    salt: String,
                                    ckksKeys: [CKKSKeychainBackedKeySet],
                                    uuid: UUID,
                                    kind: TPPBCustodianRecoveryKey_Kind,
                                    reply: @escaping ([CKRecord]?, TrustedPeersHelperCustodianRecoveryKey?, Error?) -> Void) {
        do {
            logger.info("CreateCustodianRecoveryKey for \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.createCustodianRecoveryKey(recoveryKey: recoveryKey, salt: salt, ckksKeys: ckksKeys, uuid: uuid, kind: kind) { records, custodianRecoveryKey, error in
                self.logComplete(function: "createCustodianRecoveryKey", container: container.name, error: error)
                reply(records, custodianRecoveryKey, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("CreateCustodianRecoveryKey failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, error.sanitizeForClientXPC())
        }
    }

    func removeCustodianRecoveryKey(with user: TPSpecificUser?,
                                    uuid: UUID,
                                    reply: @escaping (Error?) -> Void) {
        do {
            logger.info("RemoveCustodianRecoveryKey for \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.removeCustodianRecoveryKey(uuid: uuid) { error in
                self.logComplete(function: "removeCustodianRecoveryKey", container: container.name, error: error)
                reply(error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("RemoveCustodianRecoveryKey failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(error.sanitizeForClientXPC())
        }
    }

    func reportHealth(with user: TPSpecificUser?, stateMachineState: String, trustState: String, reply: @escaping (Error?) -> Void) {
        do {
            logger.info("ReportHealth for \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            let request = ReportHealthRequest.with {
                $0.stateMachineState = stateMachineState
            }
            container.reportHealth(request: request) { error in
                self.logComplete(function: "reportHealth", container: container.name, error: error)
                reply(error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("ReportHealth failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(error.sanitizeForClientXPC())
        }
    }

    func pushHealthInquiry(with user: TPSpecificUser?, reply: @escaping (Error?) -> Void) {
        do {
            logger.info("PushHealthInquiry for \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.pushHealthInquiry { error in
                self.logComplete(function: "pushHealthInquiry", container: container.name, error: error)
                reply(error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("PushHealthInquiry failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(error.sanitizeForClientXPC())
        }
    }

    func requestHealthCheck(with user: TPSpecificUser?, requiresEscrowCheck: Bool, knownFederations: [String], reply: @escaping (Bool, Bool, Bool, Bool, OTEscrowMoveRequestContext?, Error?) -> Void) {
        do {
            logger.info("Health Check! requiring escrow check? \(requiresEscrowCheck) for \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.requestHealthCheck(requiresEscrowCheck: requiresEscrowCheck, knownFederations: knownFederations) { postRepair, postEscrow, postReset, leaveTrust, moveRequest, error in
                reply(postRepair, postEscrow, postReset, leaveTrust, moveRequest, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("Health Check! failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(false, false, false, false, nil, error.sanitizeForClientXPC())
        }
    }

    func getSupportAppInfo(with user: TPSpecificUser?, reply: @escaping (Data?, Error?) -> Void) {
        do {
            logger.info("getSupportAppInfo for \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.getSupportAppInfo { info, error in
                reply(info, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("getSupportInfo failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, error.sanitizeForClientXPC())
        }
    }
    func removeEscrowCache(with user: TPSpecificUser?, reply: @escaping (Error?) -> Void) {
        do {
            logger.info("removeEscrowCache for \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.removeEscrowCache { error in
                reply(error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("removeEscrowCache failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(error.sanitizeForClientXPC())
        }
    }
    func resetAccountCDPContents(with user: TPSpecificUser?, reply: @escaping (Error?) -> Void) {
        do {
            logger.info("resetAccountCDPContents for \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.resetCDPAccountData { error in
                reply(error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("resetAccountCDPContents failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(error.sanitizeForClientXPC())
        }
    }
    func fetchAccountSettings(with user: TPSpecificUser?, reply: @escaping ([String: TPPBPeerStableInfoSetting]?, Error?) -> Void) {
        do {
            logger.info("fetchAccountSettings for \(String(describing: user), privacy: .public)")
            let container = try self.containerMap.findOrCreate(user: user)
            container.fetchAccountSettings { settings, error in
                reply(settings, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.info("fetchAccountSettings failed for \(String(describing: user), privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, error.sanitizeForClientXPC())
        }
    }
}
