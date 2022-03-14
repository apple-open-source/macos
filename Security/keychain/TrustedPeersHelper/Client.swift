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
            logger.debug("\(function, privacy: .public) errored for \(container.description, privacy: .public): \(String(describing: error), privacy: .public)")
        } else {
            logger.debug("\(function, privacy: .public) finished for \(container.description, privacy: .public)")
        }
    }

    internal func getContainer(withContainer container: String, context: String) throws -> Container {
        let containerName = ContainerName(container: container, context: context)
        return try self.containerMap.findOrCreate(name: containerName)
    }

    func dump(withContainer container: String, context: String, reply: @escaping ([AnyHashable: Any]?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Dumping for \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.dump { result, error in
                self.logComplete(function: "Dumping", container: container.name, error: error)
                reply(result, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("Dumping failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, error.sanitizeForClientXPC())
        }
    }

    func dumpEgoPeer(withContainer container: String,
                     context: String,
                     reply: @escaping (String?, TPPeerPermanentInfo?, TPPeerStableInfo?, TPPeerDynamicInfo?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Dumping peer for \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.dumpEgoPeer { peerID, perm, stable, dyn, error in
                self.logComplete(function: "Dumping peer", container: container.name, error: error)
                reply(peerID, perm, stable, dyn, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("Dumping peer failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, nil, nil, error.sanitizeForClientXPC())
        }
    }

    func trustStatus(withContainer container: String, context: String, reply: @escaping (TrustedPeersHelperEgoPeerStatus, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.trustStatus { egoPeerStatus, error in
                reply(egoPeerStatus, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("Trust status failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
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

    func fetchTrustState(withContainer container: String, context: String, reply: @escaping (TrustedPeersHelperPeerState?, [TrustedPeersHelperPeer]?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Fetch Trust State for \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.fetchTrustState { peerState, peerList, error in
                reply(peerState, peerList, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("Fetch Trust State failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, error.sanitizeForClientXPC())
        }
    }

    func reset(withContainer container: String, context: String, resetReason: CuttlefishResetReason, reply: @escaping (Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Resetting for \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.reset(resetReason: resetReason) { error in
                self.logComplete(function: "Resetting", container: container.name, error: error)
                reply(error?.sanitizeForClientXPC()) }
        } catch {
            logger.debug("Resetting failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(error.sanitizeForClientXPC())
        }
    }

    func localReset(withContainer container: String, context: String, reply: @escaping (Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Performing local reset for \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.localReset { error in
                self.logComplete(function: "Local reset", container: container.name, error: error)
                reply(error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("Local reset failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(error.sanitizeForClientXPC())
        }
    }

    func setAllowedMachineIDsWithContainer(_ container: String,
                                           context: String,
                                           allowedMachineIDs: Set<String>,
                                           honorIDMSListChanges: Bool,
                                           reply: @escaping (Bool, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Setting allowed machineIDs for \(containerName.description, privacy: .public) to \(allowedMachineIDs, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.setAllowedMachineIDs(allowedMachineIDs, honorIDMSListChanges: honorIDMSListChanges) { differences, error in
                self.logComplete(function: "Setting allowed machineIDs", container: container.name, error: error)
                reply(differences, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("Setting allowed machineIDs failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(false, error.sanitizeForClientXPC())
        }
    }

    func addAllowedMachineIDs(withContainer container: String,
                              context: String,
                              machineIDs: [String],
                              reply: @escaping (Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Adding allowed machineIDs for \(containerName.description, privacy: .public): \(machineIDs, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.addAllow(machineIDs) { error in
                self.logComplete(function: "Adding allowed machineIDs", container: container.name, error: error)
                reply(error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("Adding allowed machineID failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(error.sanitizeForClientXPC())
        }
    }

    func removeAllowedMachineIDs(withContainer container: String,
                                 context: String,
                                 machineIDs: [String],
                                 reply: @escaping (Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Removing allowed machineIDs for \(containerName.description, privacy: .public): \(machineIDs, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.removeAllow(machineIDs) { error in
                self.logComplete(function: "Removing allowed machineIDs", container: container.name, error: error)
                reply(error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("Removing allowed machineID failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(error.sanitizeForClientXPC())
        }
    }

    func fetchAllowedMachineIDs(withContainer container: String, context: String, reply: @escaping (Set<String>?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Fetching allowed machineIDs for \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.fetchAllowedMachineIDs { mids, error in
                self.logComplete(function: "Fetched allowed machineIDs", container: container.name, error: error)
                reply(mids, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("Fetching allowed machineIDs failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, error.sanitizeForClientXPC())
        }
    }

    func fetchEgoEpoch(withContainer container: String, context: String, reply: @escaping (UInt64, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("retrieving epoch for \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.getEgoEpoch { epoch, error in
                reply(epoch, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("Epoch retrieval failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(0, error.sanitizeForClientXPC())
        }
    }

    func prepare(withContainer container: String,
                 context: String,
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
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Preparing new identity for \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
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
            logger.debug("Prepare failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, nil, nil, nil, nil, error.sanitizeForClientXPC())
        }
    }

    func prepareInheritancePeer(withContainer container: String,
                                context: String,
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
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Preparing new identity for inheritance peer \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
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
            logger.debug("prepareInheritancePeer failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, nil, nil, nil, nil, nil, nil, error.sanitizeForClientXPC())
        }
    }

    func establish(withContainer container: String,
                   context: String,
                   ckksKeys: [CKKSKeychainBackedKeySet],
                   tlkShares: [CKKSTLKShare],
                   preapprovedKeys: [Data]?,
                   reply: @escaping (String?, [CKRecord]?, TPSyncingPolicy?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Establishing \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.establish(ckksKeys: ckksKeys,
                                tlkShares: tlkShares,
                                preapprovedKeys: preapprovedKeys) { peerID, keyHierarchyRecords, policy, error in
                                    self.logComplete(function: "Establishing", container: container.name, error: error)
                                    reply(peerID, keyHierarchyRecords, policy, error?.sanitizeForClientXPC()) }
        } catch {
            logger.debug("Establishing failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, nil, error.sanitizeForClientXPC())
        }
    }

    func vouch(withContainer container: String,
               context: String,
               peerID: String,
               permanentInfo: Data,
               permanentInfoSig: Data,
               stableInfo: Data,
               stableInfoSig: Data,
               ckksKeys: [CKKSKeychainBackedKeySet],
               reply: @escaping (Data?, Data?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Vouching \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.vouch(peerID: peerID,
                            permanentInfo: permanentInfo,
                            permanentInfoSig: permanentInfoSig,
                            stableInfo: stableInfo,
                            stableInfoSig: stableInfoSig,
                            ckksKeys: ckksKeys) { voucher, voucherSig, error in
                                self.logComplete(function: "Vouching", container: container.name, error: error)
                                reply(voucher, voucherSig, error?.sanitizeForClientXPC()) }
        } catch {
            logger.debug("Vouching failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, error.sanitizeForClientXPC())
        }
    }

    func preflightVouchWithBottle(withContainer container: String,
                                  context: String,
                                  bottleID: String,
                                  reply: @escaping (String?, TPSyncingPolicy?, Bool, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Preflight Vouch With Bottle \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.preflightVouchWithBottle(bottleID: bottleID) { peerID, policy, refetched, error in
                self.logComplete(function: "Preflight Vouch With Bottle", container: container.name, error: error)
                reply(peerID, policy, refetched, error?.sanitizeForClientXPC()) }
        } catch {
            logger.debug("Preflighting Vouch With Bottle failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, false, error.sanitizeForClientXPC())
        }
    }

    func vouchWithBottle(withContainer container: String,
                         context: String,
                         bottleID: String,
                         entropy: Data,
                         bottleSalt: String,
                         tlkShares: [CKKSTLKShare],
                         reply: @escaping (Data?, Data?, [CKKSTLKShare]?, TrustedPeersHelperTLKRecoveryResult?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Vouching With Bottle \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.vouchWithBottle(bottleID: bottleID, entropy: entropy, bottleSalt: bottleSalt, tlkShares: tlkShares) { voucher, voucherSig, newTLKShares, recoveryResult, error in
                self.logComplete(function: "Vouching With Bottle", container: container.name, error: error)
                reply(voucher, voucherSig, newTLKShares, recoveryResult, error?.sanitizeForClientXPC()) }
        } catch {
            logger.debug("Vouching with Bottle failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, nil, nil, error.sanitizeForClientXPC())
        }
    }

    func preflightVouchWithRecoveryKey(withContainer container: String,
                                       context: String,
                                       recoveryKey: String,
                                       salt: String,
                                       reply: @escaping (String?, TPSyncingPolicy?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Preflight Vouch With RecoveryKey \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.preflightVouchWithRecoveryKey(recoveryKey: recoveryKey, salt: salt) { rkID, policy, error in
                self.logComplete(function: "Preflight Vouch With RecoveryKey", container: container.name, error: error)
                reply(rkID, policy, error?.sanitizeForClientXPC()) }
        } catch {
            logger.debug("Preflighting Vouch With RecoveryKey failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, error.sanitizeForClientXPC())
        }
    }

    func preflightVouchWithCustodianRecoveryKey(withContainer container: String,
                                                context: String,
                                                crk: TrustedPeersHelperCustodianRecoveryKey,
                                                reply: @escaping (String?, TPSyncingPolicy?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Preflight Vouch With CustodianRecoveryKey \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.preflightVouchWithCustodianRecoveryKey(crk: crk) { peerID, policy, error in
                self.logComplete(function: "Preflight Vouch With CustodianRecoveryKey", container: container.name, error: error)
                reply(peerID, policy, error?.sanitizeForClientXPC()) }
        } catch {
            logger.debug("Preflighting Vouch With CustodianRecoveryKey failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, error.sanitizeForClientXPC())
        }
    }

    func vouchWithRecoveryKey(withContainer container: String,
                              context: String,
                              recoveryKey: String,
                              salt: String,
                              tlkShares: [CKKSTLKShare],
                              reply: @escaping (Data?, Data?, [CKKSTLKShare]?, TrustedPeersHelperTLKRecoveryResult?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Vouching With Recovery Key \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.vouchWithRecoveryKey(recoveryKey: recoveryKey, salt: salt, tlkShares: tlkShares) { voucher, voucherSig, newTLKShares, recoveryResult, error in
                self.logComplete(function: "Vouching With Recovery Key", container: container.name, error: error)
                reply(voucher, voucherSig, newTLKShares, recoveryResult, error?.sanitizeForClientXPC()) }
        } catch {
            logger.debug("Vouching with Recovery Key failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, nil, nil, error.sanitizeForClientXPC())
        }
    }

    func recoverTLKSharesForInheritor(withContainer container: String,
                                      context: String,
                                      crk: TrustedPeersHelperCustodianRecoveryKey,
                                      tlkShares: [CKKSTLKShare],
                                      reply: @escaping ([CKKSTLKShare]?, TrustedPeersHelperTLKRecoveryResult?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Recovering TLKShares for Inheritor \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.recoverTLKSharesForInheritor(crk: crk, tlkShares: tlkShares) { newTLKShares, recoveryResult, error in
                self.logComplete(function: "Recovering TLKShares for Inheritor", container: container.name, error: error)
                reply(newTLKShares, recoveryResult, error?.sanitizeForClientXPC()) }
        } catch {
            logger.debug("Recovering TLKShares for Inheritor failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, error.sanitizeForClientXPC())
        }
    }

    func vouchWithCustodianRecoveryKey(withContainer container: String,
                                       context: String,
                                       crk: TrustedPeersHelperCustodianRecoveryKey,
                                       tlkShares: [CKKSTLKShare],
                                       reply: @escaping (Data?, Data?, [CKKSTLKShare]?, TrustedPeersHelperTLKRecoveryResult?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Vouching With Custodian Recovery Key \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.vouchWithCustodianRecoveryKey(crk: crk, tlkShares: tlkShares) { voucher, voucherSig, newTLKShares, recoveryResult, error in
                self.logComplete(function: "Vouching With Custodian Recovery Key", container: container.name, error: error)
                reply(voucher, voucherSig, newTLKShares, recoveryResult, error?.sanitizeForClientXPC()) }
        } catch {
            logger.debug("Vouching with Custodian Recovery Key failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, nil, nil, error.sanitizeForClientXPC())
        }
    }

    func join(withContainer container: String,
              context: String,
              voucherData: Data,
              voucherSig: Data,
              ckksKeys: [CKKSKeychainBackedKeySet],
              tlkShares: [CKKSTLKShare],
              preapprovedKeys: [Data]?,
              reply: @escaping (String?, [CKRecord]?, TPSyncingPolicy?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Joining \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.join(voucherData: voucherData,
                           voucherSig: voucherSig,
                           ckksKeys: ckksKeys,
                           tlkShares: tlkShares,
                           preapprovedKeys: preapprovedKeys) { peerID, keyHierarchyRecords, policy, error in
                            reply(peerID, keyHierarchyRecords, policy, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("Joining failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, nil, error.sanitizeForClientXPC())
        }
    }

    func preflightPreapprovedJoin(withContainer container: String,
                                  context: String,
                                  preapprovedKeys: [Data]?,
                                  reply: @escaping (Bool, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Attempting to preflight a preapproved join for \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.preflightPreapprovedJoin(preapprovedKeys: preapprovedKeys) { success, error in reply(success, error?.sanitizeForClientXPC()) }
        } catch {
            logger.debug("preflightPreapprovedJoin failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(false, error.sanitizeForClientXPC())
        }
    }

    func attemptPreapprovedJoin(withContainer container: String,
                                context: String,
                                ckksKeys: [CKKSKeychainBackedKeySet],
                                tlkShares: [CKKSTLKShare],
                                preapprovedKeys: [Data]?,
                                reply: @escaping (String?, [CKRecord]?, TPSyncingPolicy?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Attempting a preapproved join for \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.preapprovedJoin(ckksKeys: ckksKeys,
                                      tlkShares: tlkShares,
                                      preapprovedKeys: preapprovedKeys) { peerID, keyHierarchyRecords, policy, error in
                                        reply(peerID, keyHierarchyRecords, policy, error?.sanitizeForClientXPC()) }
        } catch {
            logger.debug("attemptPreapprovedJoin failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, nil, error.sanitizeForClientXPC())
        }
    }

    func update(withContainer container: String,
                context: String,
                forceRefetch: Bool,
                deviceName: String?,
                serialNumber: String?,
                osVersion: String?,
                policyVersion: NSNumber?,
                policySecrets: [String: Data]?,
                syncUserControllableViews: NSNumber?,
                secureElementIdentity: TrustedPeersHelperIntendedTPPBSecureElementIdentity?,
                reply: @escaping (TrustedPeersHelperPeerState?, TPSyncingPolicy?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Updating \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)

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
            logger.debug("update failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, error.sanitizeForClientXPC())
        }
    }

    func setPreapprovedKeysWithContainer(_ container: String,
                                         context: String,
                                         preapprovedKeys: [Data],
                                         reply: @escaping (TrustedPeersHelperPeerState?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("setPreapprovedKeysWithContainer \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.set(preapprovedKeys: preapprovedKeys) { state, error in reply(state, error?.sanitizeForClientXPC()) }
        } catch {
            logger.debug("setPreapprovedKeysWithContainer failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, error.sanitizeForClientXPC())
        }
    }

    func updateTLKs(withContainer container: String,
                    context: String,
                    ckksKeys: [CKKSKeychainBackedKeySet],
                    tlkShares: [CKKSTLKShare],
                    reply: @escaping ([CKRecord]?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Updating TLKs for \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.updateTLKs(ckksKeys: ckksKeys,
                                 tlkShares: tlkShares) { records, error in
                reply(records, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("updateTLKs failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, error.sanitizeForClientXPC())
        }
    }

    func departByDistrustingSelf(withContainer container: String,
                                 context: String,
                                 reply: @escaping (Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Departing \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.departByDistrustingSelf { error in
                reply(error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("departByDistrustingSelf failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(error.sanitizeForClientXPC())
        }
    }

    func distrustPeerIDs(withContainer container: String,
                         context: String,
                         peerIDs: Set<String>,
                         reply: @escaping (Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Distrusting \(peerIDs, privacy: .public) in \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.distrust(peerIDs: peerIDs) { error in
                reply(error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("distrustPeerIDs failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(error.sanitizeForClientXPC())
        }
    }

    func fetchViableBottles(withContainer container: String, context: String, reply: @escaping ([String]?, [String]?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("fetchViableBottles in \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.fetchViableBottles { sortedBottleIDs, partialBottleIDs, error in
                reply(sortedBottleIDs, partialBottleIDs, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("fetchViableBottles failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, error.sanitizeForClientXPC())
        }
    }

    func fetchViableEscrowRecords(withContainer container: String, context: String, forceFetch: Bool, reply: @escaping ([Data]?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("fetchViableEscrowRecords in \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.fetchEscrowRecords(forceFetch: forceFetch) { recordDatas, error in
                reply(recordDatas, error?.sanitizeForClientXPC())
            }
        } catch {
            reply(nil, error.sanitizeForClientXPC())
        }
    }

    func fetchEscrowContents(withContainer container: String, context: String, reply: @escaping (Data?, String?, Data?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("fetchEscrowContents in \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.fetchEscrowContents { entropy, bottleID, signingPublicKey, error in
                reply(entropy, bottleID, signingPublicKey, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("fetchEscrowContents failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, nil, error.sanitizeForClientXPC())
        }
    }

    func fetchCurrentPolicy(withContainer container: String,
                            context: String,
                            modelIDOverride: String?,
                            isInheritedAccount: Bool,
                            reply: @escaping (TPSyncingPolicy?, TPPBPeerStableInfoUserControllableViewStatus, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Fetching policy+views for \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.fetchCurrentPolicy(modelIDOverride: modelIDOverride, isInheritedAccount: isInheritedAccount) { policy, peersOpinion, error in
                reply(policy, peersOpinion, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("fetchCurrentPolicy failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, .UNKNOWN, error.sanitizeForClientXPC())
        }
    }

    func fetchPolicyDocuments(withContainer container: String,
                              context: String,
                              versions: Set<TPPolicyVersion>,
                              reply: @escaping ([TPPolicyVersion: Data]?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Fetching policy documents \(containerName.description, privacy: .public) with versions: \(versions, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.fetchPolicyDocuments(versions: versions) { entries, error in
                reply(entries, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("fetchPolicyDocuments failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, error.sanitizeForClientXPC())
        }
    }

    func fetchRecoverableTLKShares(withContainer container: String,
                                   context: String,
                                   peerID: String?,
                                   reply: @escaping ([CKRecord]?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Fetching recoverable TLKShares \(containerName.description, privacy: .public) with peerID filter: \(String(describing: peerID), privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)

            container.fetchRecoverableTLKShares(peerID: peerID) { records, error in
                reply(records, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("fetchRecoverableTLKShares failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, error.sanitizeForClientXPC())
        }
    }

    func validatePeers(withContainer container: String, context: String, reply: @escaping ([AnyHashable: Any]?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("ValidatePeers for \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            let request = ValidatePeersRequest()
            container.validatePeers(request: request) { result, error in
                self.logComplete(function: "validatePeers", container: container.name, error: error)
                reply(result, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("ValidatePeers failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, error.sanitizeForClientXPC())
        }
    }

    func setRecoveryKeyWithContainer(_ container: String,
                                     context: String,
                                     recoveryKey: String,
                                     salt: String,
                                     ckksKeys: [CKKSKeychainBackedKeySet],
                                     reply: @escaping ([CKRecord]?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("SetRecoveryKey for \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.setRecoveryKey(recoveryKey: recoveryKey, salt: salt, ckksKeys: ckksKeys) { records, error in
                self.logComplete(function: "setRecoveryKey", container: container.name, error: error)
                reply(records, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("SetRecoveryKey failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, error.sanitizeForClientXPC())
        }
    }

    func createCustodianRecoveryKey(withContainer container: String,
                                    context: String,
                                    recoveryKey: String,
                                    salt: String,
                                    ckksKeys: [CKKSKeychainBackedKeySet],
                                    uuid: UUID,
                                    kind: TPPBCustodianRecoveryKey_Kind,
                                    reply: @escaping ([CKRecord]?, TrustedPeersHelperCustodianRecoveryKey?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("CreateCustodianRecoveryKey for \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.createCustodianRecoveryKey(recoveryKey: recoveryKey, salt: salt, ckksKeys: ckksKeys, uuid: uuid, kind: kind) { records, custodianRecoveryKey, error in
                self.logComplete(function: "createCustodianRecoveryKey", container: container.name, error: error)
                reply(records, custodianRecoveryKey, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("CreateCustodianRecoveryKey failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, nil, error.sanitizeForClientXPC())
        }
    }

    func removeCustodianRecoveryKey(withContainer container: String,
                                    context: String,
                                    uuid: UUID,
                                    reply: @escaping (Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("RemoveCustodianRecoveryKey for \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.removeCustodianRecoveryKey(uuid: uuid) { error in
                self.logComplete(function: "removeCustodianRecoveryKey", container: container.name, error: error)
                reply(error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("RemoveCustodianRecoveryKey failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(error.sanitizeForClientXPC())
        }
    }

    func reportHealth(withContainer container: String, context: String, stateMachineState: String, trustState: String, reply: @escaping (Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("ReportHealth for \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            let request = ReportHealthRequest.with {
                $0.stateMachineState = stateMachineState
            }
            container.reportHealth(request: request) { error in
                self.logComplete(function: "reportHealth", container: container.name, error: error)
                reply(error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("ReportHealth failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(error.sanitizeForClientXPC())
        }
    }

    func pushHealthInquiry(withContainer container: String, context: String, reply: @escaping (Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("PushHealthInquiry for \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.pushHealthInquiry { error in
                self.logComplete(function: "pushHealthInquiry", container: container.name, error: error)
                reply(error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("PushHealthInquiry failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(error.sanitizeForClientXPC())
        }
    }

    func requestHealthCheck(withContainer container: String, context: String, requiresEscrowCheck: Bool, knownFederations: [String], reply: @escaping (Bool, Bool, Bool, Bool, OTEscrowMoveRequestContext?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("Health Check! requiring escrow check? \(requiresEscrowCheck) for \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.requestHealthCheck(requiresEscrowCheck: requiresEscrowCheck, knownFederations: knownFederations) { postRepair, postEscrow, postReset, leaveTrust, moveRequest, error in
                reply(postRepair, postEscrow, postReset, leaveTrust, moveRequest, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("Health Check! failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(false, false, false, false, nil, error.sanitizeForClientXPC())
        }
    }

    func getSupportAppInfo(withContainer container: String, context: String, reply: @escaping (Data?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("getSupportAppInfo for \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.getSupportAppInfo { info, error in
                reply(info, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("getSupportInfo failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, error.sanitizeForClientXPC())
        }
    }
    func removeEscrowCache(withContainer container: String, context: String, reply: @escaping (Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("removeEscrowCache for \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.removeEscrowCache { error in
                reply(error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("removeEscrowCache failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(error.sanitizeForClientXPC())
        }
    }
    func resetAccountCDPContents(withContainer container: String, context: String, reply: @escaping (Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("resetAccountCDPContents for \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.resetCDPAccountData { error in
                reply(error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("resetAccountCDPContents failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(error.sanitizeForClientXPC())
        }
    }
    func fetchAccountSettings(withContainer container: String, context: String, reply: @escaping ([String: TPPBPeerStableInfoSetting]?, Error?) -> Void) {
        let containerName = ContainerName(container: container, context: context)
        do {
            logger.debug("fetchAccountSettings for \(containerName.description, privacy: .public)")
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.fetchAccountSettings { settings, error in
                reply(settings, error?.sanitizeForClientXPC())
            }
        } catch {
            logger.debug("fetchAccountSettings failed for \(containerName.description, privacy: .public): \(String(describing: error), privacy: .public)")
            reply(nil, error.sanitizeForClientXPC())
        }
    }
}
