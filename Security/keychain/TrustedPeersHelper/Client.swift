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
            os_log("%@ errored for %@: %@", log: tplogDebug, type: .default, function, container.description, error as CVarArg)
        } else {
            os_log("%@ finished for %@", log: tplogDebug, type: .default, function, container.description)
        }
    }

    internal func getContainer(withContainer container: String, context: String) throws -> Container {
        let containerName = ContainerName(container: container, context: context)
        return try self.containerMap.findOrCreate(name: containerName)
    }

    func dump(withContainer container: String, context: String, reply: @escaping ([AnyHashable: Any]?, Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("Dumping for %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.dump { result, error in
                self.logComplete(function: "Dumping", container: container.name, error: error)
                reply(result, CKXPCSuitableError(error))
            }
        } catch {
            os_log("Dumping failed for (%@, %@): %@", log: tplogDebug, type: .default, container, context, error as CVarArg)
            reply(nil, CKXPCSuitableError(error))
        }
    }

    func dumpEgoPeer(withContainer container: String,
                     context: String,
                     reply: @escaping (String?, TPPeerPermanentInfo?, TPPeerStableInfo?, TPPeerDynamicInfo?, Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("Dumping peer for %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.dumpEgoPeer { peerID, perm, stable, dyn, error in
                self.logComplete(function: "Dumping peer", container: container.name, error: error)
                reply(peerID, perm, stable, dyn, CKXPCSuitableError(error))
            }
        } catch {
            os_log("Dumping peer failed for (%@, %@): %@", log: tplogDebug, type: .default, container, context, error as CVarArg)
            reply(nil, nil, nil, nil, CKXPCSuitableError(error))
        }
    }

    func trustStatus(withContainer container: String, context: String, reply: @escaping (TrustedPeersHelperEgoPeerStatus, Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.trustStatus(reply: reply)
        } catch {
            os_log("Trust status failed for (%@, %@): %@", log: tplogDebug, type: .default, container, context, error as CVarArg)
            reply(TrustedPeersHelperEgoPeerStatus(egoPeerID: nil,
                                                  status: TPPeerStatus.unknown,
                                                  viablePeerCountsByModelID: [:],
                                                  peerCountsByMachineID: [:],
                                                  isExcluded: false,
                                                  isLocked: false),
                  CKXPCSuitableError(error))
        }
    }

    func fetchTrustState(withContainer container: String, context: String, reply: @escaping (TrustedPeersHelperPeerState?, [TrustedPeersHelperPeer]?, Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("Fetch Trust State for %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.fetchTrustState(reply: reply)
        } catch {
            os_log("Fetch Trust State failed for (%@, %@): %@", log: tplogDebug, type: .default, container, context, error as CVarArg)
            reply(nil, nil, CKXPCSuitableError(error))
        }
    }

    func reset(withContainer container: String, context: String, resetReason: CuttlefishResetReason, reply: @escaping (Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("Resetting for %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.reset(resetReason: resetReason) { error in
                self.logComplete(function: "Resetting", container: container.name, error: error)
                reply(CKXPCSuitableError(error)) }
        } catch {
            os_log("Resetting failed for (%@, %@): %@", log: tplogDebug, type: .default, container, context, error as CVarArg)
            reply(CKXPCSuitableError(error))
        }
    }

    func localReset(withContainer container: String, context: String, reply: @escaping (Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("Performing local reset for %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.localReset { error in
                self.logComplete(function: "Local reset", container: container.name, error: error)
                reply(CKXPCSuitableError(error))
            }
        } catch {
            os_log("Local reset failed for (%@, %@): %@", log: tplogDebug, type: .default, container, context, error as CVarArg)
            reply(CKXPCSuitableError(error))
        }
    }

    func setAllowedMachineIDsWithContainer(_ container: String,
                                           context: String,
                                           allowedMachineIDs: Set<String>,
                                           reply: @escaping (Bool, Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("Setting allowed machineIDs for %@ to %@", log: tplogDebug, type: .default, containerName.description, allowedMachineIDs)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.setAllowedMachineIDs(allowedMachineIDs) { differences, error in
                self.logComplete(function: "Setting allowed machineIDs", container: container.name, error: error)
                reply(differences, CKXPCSuitableError(error))
            }
        } catch {
            os_log("Setting allowed machineIDs failed for (%@, %@): %@", log: tplogDebug, type: .default, container, context, error as CVarArg)
            reply(false, CKXPCSuitableError(error))
        }
    }

    func addAllowedMachineIDs(withContainer container: String,
                              context: String,
                              machineIDs: [String],
                              reply: @escaping (Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("Adding allowed machineIDs for %@: %@", log: tplogDebug, type: .default, containerName.description, machineIDs)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.addAllow(machineIDs) { error in
                self.logComplete(function: "Adding allowed machineIDs", container: container.name, error: error)
                reply(CKXPCSuitableError(error))
            }
        } catch {
            os_log("Adding allowed machineID failed for (%@, %@): %@", log: tplogDebug, type: .default, container, context, error as CVarArg)
            reply(CKXPCSuitableError(error))
        }
    }

    func removeAllowedMachineIDs(withContainer container: String,
                                 context: String,
                                 machineIDs: [String],
                                 reply: @escaping (Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("Removing allowed machineIDs for %@: %@", log: tplogDebug, type: .default, containerName.description, machineIDs)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.removeAllow(machineIDs) { error in
                self.logComplete(function: "Removing allowed machineIDs", container: container.name, error: error)
                reply(CKXPCSuitableError(error))
            }
        } catch {
            os_log("Removing allowed machineID failed for (%@, %@): %@", log: tplogDebug, type: .default, container, context, error as CVarArg)
            reply(CKXPCSuitableError(error))
        }
    }

    func fetchAllowedMachineIDs(withContainer container: String, context: String, reply: @escaping (Set<String>?, Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("Fetching allowed machineIDs for %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.fetchAllowedMachineIDs() { mids, error in
                self.logComplete(function: "Fetched allowed machineIDs", container: container.name, error: error)
                reply(mids, CKXPCSuitableError(error))
            }
        } catch {
            os_log("Fetching allowed machineIDs failed for (%@, %@): %@", log: tplogDebug, type: .default, container, context, error as CVarArg)
            reply(nil, CKXPCSuitableError(error))
        }
    }

    func fetchEgoEpoch(withContainer container: String, context: String, reply: @escaping (UInt64, Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("retrieving epoch for %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.getEgoEpoch { epoch, error in
                reply(epoch, CKXPCSuitableError(error))
            }
        } catch {
            os_log("Epoch retrieval failed for (%@, %@): %@", log: tplogDebug, type: .default, container, context, error as CVarArg)
            reply(0, CKXPCSuitableError(error))
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
                 serialNumber: String,
                 osVersion: String,
                 policyVersion: NSNumber?,
                 policySecrets: [String: Data]?,
                 signingPrivKeyPersistentRef: Data?,
                 encPrivKeyPersistentRef: Data?,
                 reply: @escaping (String?, Data?, Data?, Data?, Data?, Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("Preparing new identity for %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.prepare(epoch: epoch,
                              machineID: machineID,
                              bottleSalt: bottleSalt,
                              bottleID: bottleID,
                              modelID: modelID,
                              deviceName: deviceName,
                              serialNumber: serialNumber,
                              osVersion: osVersion,
                              policyVersion: policyVersion?.uint64Value,
                              policySecrets: policySecrets,
                              signingPrivateKeyPersistentRef: signingPrivKeyPersistentRef,
                              encryptionPrivateKeyPersistentRef: encPrivKeyPersistentRef) { peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, error in
                                self.logComplete(function: "Prepare", container: container.name, error: error)
                                reply(peerID, permanentInfo, permanentInfoSig, stableInfo, stableInfoSig, CKXPCSuitableError(error))
            }
        } catch {
            os_log("Prepare failed for (%@, %@): %@", log: tplogDebug, type: .default, container, context, error as CVarArg)
            reply(nil, nil, nil, nil, nil, CKXPCSuitableError(error))
        }
    }

    func establish(withContainer container: String,
                   context: String,
                   ckksKeys: [CKKSKeychainBackedKeySet],
                   tlkShares: [CKKSTLKShare],
                   preapprovedKeys: [Data]?,
                   reply: @escaping (String?, [CKRecord]?, Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("Establishing %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.establish(ckksKeys: ckksKeys,
                                tlkShares: tlkShares,
                                preapprovedKeys: preapprovedKeys) { peerID, keyHierarchyRecords, error in
                                    self.logComplete(function: "Establishing", container: container.name, error: error)
                                    reply(peerID, keyHierarchyRecords, CKXPCSuitableError(error)) }
        } catch {
            os_log("Establishing failed for (%@, %@): %@", log: tplogDebug, type: .default, container, context, error as CVarArg)
            reply(nil, nil, CKXPCSuitableError(error))
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
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("Vouching %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.vouch(peerID: peerID,
                            permanentInfo: permanentInfo,
                            permanentInfoSig: permanentInfoSig,
                            stableInfo: stableInfo,
                            stableInfoSig: stableInfoSig,
                            ckksKeys: ckksKeys) { voucher, voucherSig, error in
                                self.logComplete(function: "Vouching", container: container.name, error: error)
                                reply(voucher, voucherSig, CKXPCSuitableError(error)) }
        } catch {
            os_log("Vouching failed for (%@, %@): %@", log: tplogDebug, type: .default, container, context, error as CVarArg)
            reply(nil, nil, CKXPCSuitableError(error))
        }
    }

    func preflightVouchWithBottle(withContainer container: String,
                                  context: String,
                                  bottleID: String,
                                  reply: @escaping (String?, Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("Preflight Vouch With Bottle %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.preflightVouchWithBottle(bottleID: bottleID) { peerID, error in
                self.logComplete(function: "Preflight Vouch With Bottle", container: container.name, error: error)
                reply(peerID, CKXPCSuitableError(error)) }
        } catch {
            os_log("Preflighting Vouch With Bottle failed for (%@, %@): %@", log: tplogDebug, type: .default, container, context, error as CVarArg)
            reply(nil, CKXPCSuitableError(error))
        }
    }

    func vouchWithBottle(withContainer container: String,
                         context: String,
                         bottleID: String,
                         entropy: Data,
                         bottleSalt: String,
                         tlkShares: [CKKSTLKShare],
                         reply: @escaping (Data?, Data?, Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("Vouching With Bottle %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.vouchWithBottle(bottleID: bottleID, entropy: entropy, bottleSalt: bottleSalt, tlkShares: tlkShares) { voucher, voucherSig, error in
                self.logComplete(function: "Vouching With Bottle", container: container.name, error: error)
                reply(voucher, voucherSig, CKXPCSuitableError(error)) }
        } catch {
            os_log("Vouching with Bottle failed for (%@, %@): %@", log: tplogDebug, type: .default, container, context, error as CVarArg)
            reply(nil, nil, CKXPCSuitableError(error))
        }
    }

    func vouchWithRecoveryKey(withContainer container: String,
                              context: String,
                              recoveryKey: String,
                              salt: String,
                              tlkShares: [CKKSTLKShare],
                              reply: @escaping (Data?, Data?, Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("Vouching With Recovery Key %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.vouchWithRecoveryKey(recoveryKey: recoveryKey, salt: salt, tlkShares: tlkShares) { voucher, voucherSig, error in
                self.logComplete(function: "Vouching With Recovery Key", container: container.name, error: error)
                reply(voucher, voucherSig, CKXPCSuitableError(error)) }
        } catch {
            os_log("Vouching with Recovery Key failed for (%@, %@): %@", log: tplogDebug, type: .default, container, context, error as CVarArg)
            reply(nil, nil, CKXPCSuitableError(error))
        }
    }

    func join(withContainer container: String,
              context: String,
              voucherData: Data,
              voucherSig: Data,
              ckksKeys: [CKKSKeychainBackedKeySet],
              tlkShares: [CKKSTLKShare],
              preapprovedKeys: [Data],
              reply: @escaping (String?, [CKRecord]?, Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("Joining %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.join(voucherData: voucherData,
                           voucherSig: voucherSig,
                           ckksKeys: ckksKeys,
                           tlkShares: tlkShares,
                           preapprovedKeys: preapprovedKeys) { peerID, keyHierarchyRecords, error in reply(peerID, keyHierarchyRecords, CKXPCSuitableError(error)) }
        } catch {
            reply(nil, nil, CKXPCSuitableError(error))
        }
    }

    func preflightPreapprovedJoin(withContainer container: String,
                                  context: String,
                                  reply: @escaping (Bool, Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("Attempting to preflight a preapproved join for %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.preflightPreapprovedJoin { success, error in reply(success, CKXPCSuitableError(error)) }
        } catch {
            reply(false, CKXPCSuitableError(error))
        }
    }

    func attemptPreapprovedJoin(withContainer container: String,
                                context: String,
                                ckksKeys: [CKKSKeychainBackedKeySet],
                                tlkShares: [CKKSTLKShare],
                                preapprovedKeys: [Data],
                                reply: @escaping (String?, [CKRecord]?, Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("Attempting a preapproved join for %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.preapprovedJoin(ckksKeys: ckksKeys,
                                      tlkShares: tlkShares,
                                      preapprovedKeys: preapprovedKeys) { peerID, keyHierarchyRecords, error in reply(peerID, keyHierarchyRecords, CKXPCSuitableError(error)) }
        } catch {
            reply(nil, nil, CKXPCSuitableError(error))
        }
    }

    func update(withContainer container: String,
                context: String,
                deviceName: String?,
                serialNumber: String?,
                osVersion: String?,
                policyVersion: NSNumber?,
                policySecrets: [String: Data]?,
                reply: @escaping (TrustedPeersHelperPeerState?, Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("Updating %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.update(deviceName: deviceName,
                             serialNumber: serialNumber,
                             osVersion: osVersion,
                             policyVersion: policyVersion?.uint64Value,
                             policySecrets: policySecrets) { state, error in reply(state, CKXPCSuitableError(error)) }
        } catch {
            reply(nil, CKXPCSuitableError(error))
        }
    }

    func setPreapprovedKeysWithContainer(_ container: String,
                               context: String,
                               preapprovedKeys: [Data],
                               reply: @escaping (Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("Updating %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.set(preapprovedKeys: preapprovedKeys) { error in reply(CKXPCSuitableError(error)) }
        } catch {
            reply(CKXPCSuitableError(error))
        }
    }

    func updateTLKs(withContainer container: String,
                    context: String,
                    ckksKeys: [CKKSKeychainBackedKeySet],
                    tlkShares: [CKKSTLKShare],
                    reply: @escaping ([CKRecord]?, Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("Updating TLKs for %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.updateTLKs(ckksKeys: ckksKeys,
                                 tlkShares: tlkShares,
                                 reply: reply)
        } catch {
            reply(nil, CKXPCSuitableError(error))
        }
    }

    func departByDistrustingSelf(withContainer container: String,
                                 context: String,
                                 reply: @escaping (Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("Departing %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.departByDistrustingSelf { error in
                reply(CKXPCSuitableError(error))
            }
        } catch {
            reply(CKXPCSuitableError(error))
        }
    }

    func distrustPeerIDs(withContainer container: String,
                         context: String,
                         peerIDs: Set<String>,
                         reply: @escaping (Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("Distrusting %@ in %@", log: tplogDebug, type: .default, peerIDs, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.distrust(peerIDs: peerIDs) { error in
                reply(CKXPCSuitableError(error))
            }
        } catch {
            reply(CKXPCSuitableError(error))
        }
    }

    func fetchViableBottles(withContainer container: String, context: String, reply: @escaping ([String]?, [String]?, Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("fetchViableBottles in %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.fetchViableBottles { sortedBottleIDs, partialBottleIDs, error in
                reply(sortedBottleIDs, partialBottleIDs, CKXPCSuitableError(error))
            }
        } catch {
            reply(nil, nil, CKXPCSuitableError(error))
        }
    }

    func fetchEscrowContents(withContainer container: String, context: String, reply: @escaping (Data?, String?, Data?, Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("fetchEscrowContents in %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.fetchEscrowContents { entropy, bottleID, signingPublicKey, error in
                reply(entropy, bottleID, signingPublicKey, CKXPCSuitableError(error))
            }
        } catch {
            reply(nil, nil, nil, CKXPCSuitableError(error))
        }
    }

    func fetchPolicy(withContainer container: String,
                     context: String,
                     reply: @escaping (TPPolicy?, Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("Fetching policy for %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.fetchPolicy { policy, error in
                reply(policy, CKXPCSuitableError(error))
            }
        } catch {
            reply(nil, CKXPCSuitableError(error))
        }
    }

    func fetchPolicyDocuments(withContainer container: String,
                              context: String,
                              keys: [NSNumber: String],
                              reply: @escaping ([NSNumber: [String]]?, Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("Fetching policy documents %@ with keys: %@", log: tplogDebug, type: .default, containerName.description, keys)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.fetchPolicyDocuments(keys: keys) { entries, error in
                reply(entries, CKXPCSuitableError(error))
            }
        } catch {
            reply(nil, CKXPCSuitableError(error))
        }
    }

    func validatePeers(withContainer container: String, context: String, reply: @escaping ([AnyHashable: Any]?, Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("ValidatePeers for %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            let request = ValidatePeersRequest()
            container.validatePeers(request: request) { result, error in
                self.logComplete(function: "validatePeers", container: container.name, error: error)
                reply(result, CKXPCSuitableError(error))
            }
        } catch {
            os_log("ValidatePeers failed for (%@, %@): %@", log: tplogDebug, type: .default, container, context, error as CVarArg)
            reply(nil, CKXPCSuitableError(error))
        }
    }

    func setRecoveryKeyWithContainer(_ container: String, context: String, recoveryKey: String, salt: String, ckksKeys: [CKKSKeychainBackedKeySet], reply: @escaping (Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("SetRecoveryKey for %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.setRecoveryKey(recoveryKey: recoveryKey, salt: salt, ckksKeys: ckksKeys) { error in
                self.logComplete(function: "setRecoveryKey", container: container.name, error: error)
                reply(CKXPCSuitableError(error))
            }
        } catch {
            os_log("SetRecoveryKey failed for (%@, %@): %@", log: tplogDebug, type: .default, container, context, error as CVarArg)
            reply(CKXPCSuitableError(error))
        }
    }

    func reportHealth(withContainer container: String, context: String, stateMachineState: String, trustState: String, reply: @escaping (Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("ReportHealth for %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            let request = ReportHealthRequest.with {
                $0.stateMachineState = stateMachineState
            }
            container.reportHealth(request: request) { error in
                self.logComplete(function: "reportHealth", container: container.name, error: error)
                reply(CKXPCSuitableError(error))
            }
        } catch {
            os_log("ReportHealth failed for (%@, %@): %@", log: tplogDebug, type: .default, container, context, error as CVarArg)
            reply(CKXPCSuitableError(error))
        }
    }

    func pushHealthInquiry(withContainer container: String, context: String, reply: @escaping (Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("PushHealthInquiry for %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.pushHealthInquiry { error in
                self.logComplete(function: "pushHealthInquiry", container: container.name, error: error)
                reply(CKXPCSuitableError(error))
            }
        } catch {
            os_log("PushHealthInquiry failed for (%@, %@): %@", log: tplogDebug, type: .default, container, context, error as CVarArg)
            reply(CKXPCSuitableError(error))
        }
    }

    func getViewsWithContainer(_ container: String, context: String, inViews: [String], reply: @escaping ([String]?, Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("GetViews (%@) for %@", log: tplogDebug, type: .default, inViews, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.getViews(inViews: inViews) { outViews, error in
                reply(outViews, CKXPCSuitableError(error))
            }
        } catch {
            os_log("GetViews failed for (%@, %@): %@", log: tplogDebug, type: .default, container, context, error as CVarArg)
            reply(nil, CKXPCSuitableError(error))
        }
    }

    func requestHealthCheck(withContainer container: String, context: String, requiresEscrowCheck: Bool, reply: @escaping (Bool, Bool, Bool, Error?) -> Void) {
        do {
            let containerName = ContainerName(container: container, context: context)
            os_log("Health Check! requiring escrow check? %d for %@", log: tplogDebug, type: .default, requiresEscrowCheck, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.requestHealthCheck(requiresEscrowCheck: requiresEscrowCheck) { postRepair, postEscrow, postReset, error in
                reply(postRepair, postEscrow, postReset, CKXPCSuitableError(error))
            }
        } catch {
            os_log("Health Check! failed for (%@, %@): %@", log: tplogDebug, type: .default, container, context, error as CVarArg)
            reply(false, false, false, CKXPCSuitableError(error))
        }
    }

    func getSupportAppInfo(withContainer container: String, context: String, reply: @escaping (Data?, Error?) -> Void) {
                do {
            let containerName = ContainerName(container: container, context: context)
            os_log("getSupportInfo %d for %@", log: tplogDebug, type: .default, containerName.description)
            let container = try self.containerMap.findOrCreate(name: containerName)
            container.getSupportAppInfo { info, error in
                reply(info, CKXPCSuitableError(error))
            }
        } catch {
            os_log("getSupportInfo failed for (%@, %@): %@", log: tplogDebug, type: .default, container, context, error as CVarArg)
            reply(nil, CKXPCSuitableError(error))
        }

    }
}
