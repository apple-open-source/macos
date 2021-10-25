import CoreData
import Foundation

extension Container {
    func preflightVouchWithRecoveryKey(recoveryKey: String,
                                       salt: String,
                                       reply: @escaping (String?, TPSyncingPolicy?, Error?) -> Void) {
        self.semaphore.wait()
        let reply: (String?, TPSyncingPolicy?, Error?) -> Void = {
            os_log("preflightRecoveryKey complete: %{public}@",
                   log: tplogTrace, type: .info, traceError($2))
            self.semaphore.signal()
            reply($0, $1, $2)
        }

        self.fetchAndPersistChangesIfNeeded { fetchError in
            guard fetchError == nil else {
                os_log("preflightRecoveryKey unable to fetch current peers: %{public}@", log: tplogDebug, type: .default, (fetchError as CVarArg?) ?? "")
                reply(nil, nil, fetchError)
                return
            }

            // Ensure we have all policy versions claimed by peers, including our sponsor
            self.fetchPolicyDocumentsWithSemaphore(versions: self.model.allPolicyVersions()) { _, fetchPolicyDocumentsError in
                guard fetchPolicyDocumentsError == nil else {
                    os_log("preflightRecoveryKey unable to fetch policy documents: %{public}@", log: tplogDebug, type: .default, (fetchPolicyDocumentsError as CVarArg?) ?? "no error")
                    reply(nil, nil, fetchPolicyDocumentsError)
                    return
                }

                self.moc.performAndWait {
                    guard let egoPeerID = self.containerMO.egoPeerID,
                        let egoPermData = self.containerMO.egoPeerPermanentInfo,
                        let egoPermSig = self.containerMO.egoPeerPermanentInfoSig,
                        let egoStableData = self.containerMO.egoPeerStableInfo,
                        let egoStableSig = self.containerMO.egoPeerStableInfoSig else {
                            os_log("preflightRecoveryKey: no ego peer ID", log: tplogDebug, type: .default)
                            reply(nil, nil, ContainerError.noPreparedIdentity)
                            return
                    }

                    let keyFactory = TPECPublicKeyFactory()
                    guard let selfPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: egoPermData, sig: egoPermSig, keyFactory: keyFactory) else {
                        reply(nil, nil, ContainerError.invalidPermanentInfoOrSig)
                        return
                    }

                    guard let selfStableInfo = TPPeerStableInfo(data: egoStableData, sig: egoStableSig) else {
                        os_log("cannot create TPPeerStableInfo", log: tplogDebug, type: .default)
                        reply(nil, nil, ContainerError.invalidStableInfoOrSig)
                        return
                    }

                    var recoveryKeys: RecoveryKey
                    do {
                        recoveryKeys = try RecoveryKey(recoveryKeyString: recoveryKey, recoverySalt: salt)
                    } catch {
                        os_log("preflightRecoveryKey: failed to create recovery keys: %{public}@", log: tplogDebug, type: .default, error as CVarArg)
                        reply(nil, nil, ContainerError.failedToCreateRecoveryKey)
                        return
                    }

                    // Dear model: if i were to use this recovery key, what peers would I end up using?
                    guard self.model.isRecoveryKeyEnrolled() else {
                        os_log("preflightRecoveryKey: recovery Key is not enrolled", log: tplogDebug, type: .default)
                        reply(nil, nil, ContainerError.recoveryKeysNotEnrolled)
                        return
                    }

                    guard let sponsorPeerID = self.model.peerIDThatTrustsRecoveryKeys(TPRecoveryKeyPair(signingKeyData: recoveryKeys.peerKeys.signingKey.publicKey.keyData,
                                                                                                        encryptionKeyData: recoveryKeys.peerKeys.encryptionKey.publicKey.keyData),
                                                                                      canIntroducePeer: selfPermanentInfo,
                                                                                      stableInfo: selfStableInfo) else {
                        os_log("preflightRecoveryKey Untrusted recovery key set", log: tplogDebug, type: .default)
                        reply(nil, nil, ContainerError.untrustedRecoveryKeys)
                        return
                    }

                    guard let sponsor = self.model.peer(withID: sponsorPeerID) else {
                        os_log("preflightRecoveryKey Failed to find peer with ID", log: tplogDebug, type: .default)
                        reply(nil, nil, ContainerError.sponsorNotRegistered(sponsorPeerID))
                        return
                    }

                    do {
                        let bestPolicy = try self.model.policy(forPeerIDs: sponsor.dynamicInfo?.includedPeerIDs ?? [sponsor.peerID],
                                                               candidatePeerID: egoPeerID,
                                                               candidateStableInfo: sponsor.stableInfo)
                        let syncingPolicy = try bestPolicy.syncingPolicy(forModel: selfPermanentInfo.modelID,
                                                                         syncUserControllableViews: sponsor.stableInfo?.syncUserControllableViews ?? .UNKNOWN, isInheritedAccount: selfStableInfo.isInheritedAccount)

                        reply(recoveryKeys.peerKeys.peerID, syncingPolicy, nil)
                    } catch {
                        os_log("preflightRecoveryKey: error fetching policy: %{public}@", log: tplogDebug, type: .default, error as CVarArg)
                        reply(nil, nil, error)
                        return
                    }
                }
            }
        }
    }

    func preflightVouchWithCustodianRecoveryKey(crk: TrustedPeersHelperCustodianRecoveryKey,
                                                reply: @escaping (String?, TPSyncingPolicy?, Error?) -> Void) {
        self.semaphore.wait()
        let reply: (String?, TPSyncingPolicy?, Error?) -> Void = {
            os_log("preflightCustodianRecoveryKey complete: %{public}@",
                   log: tplogTrace, type: .info, traceError($2))
            self.semaphore.signal()
            reply($0, $1, $2)
        }

        self.fetchAndPersistChangesIfNeeded { fetchError in
            guard fetchError == nil else {
                os_log("preflightCustodianRecoveryKey unable to fetch current peers: %{public}@", log: tplogDebug, type: .default, (fetchError as CVarArg?) ?? "")
                reply(nil, nil, fetchError)
                return
            }

            // Ensure we have all policy versions claimed by peers, including our sponsor
            self.fetchPolicyDocumentsWithSemaphore(versions: self.model.allPolicyVersions()) { _, fetchPolicyDocumentsError in
                guard fetchPolicyDocumentsError == nil else {
                    os_log("preflightCustodianRecoveryKey unable to fetch policy documents: %{public}@", log: tplogDebug, type: .default, (fetchPolicyDocumentsError as CVarArg?) ?? "no error")
                    reply(nil, nil, fetchPolicyDocumentsError)
                    return
                }

                self.moc.performAndWait {
                    guard let egoPeerID = self.containerMO.egoPeerID,
                        let egoPermData = self.containerMO.egoPeerPermanentInfo,
                        let egoPermSig = self.containerMO.egoPeerPermanentInfoSig,
                        let egoStableData = self.containerMO.egoPeerStableInfo,
                        let egoStableSig = self.containerMO.egoPeerStableInfoSig else {
                            os_log("preflightCustodianRecoveryKey: no ego peer ID", log: tplogDebug, type: .default)
                            reply(nil, nil, ContainerError.noPreparedIdentity)
                            return
                    }

                    let keyFactory = TPECPublicKeyFactory()
                    guard let selfPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: egoPermData, sig: egoPermSig, keyFactory: keyFactory) else {
                        reply(nil, nil, ContainerError.invalidPermanentInfoOrSig)
                        return
                    }

                    guard let selfStableInfo = TPPeerStableInfo(data: egoStableData, sig: egoStableSig) else {
                        reply(nil, nil, ContainerError.invalidStableInfoOrSig)
                        return
                    }

                    guard let uuid = UUID(uuidString: crk.uuid) else {
                        os_log("Unable to parse uuid %{public}@", log: tplogDebug, type: .default, crk.uuid)
                        reply(nil, nil, ContainerError.recoveryKeysNotEnrolled)
                        return
                    }

                    guard let tpcrk = self.model.findCustodianRecoveryKey(with: uuid) else {
                        os_log("Unable to find custodian recovery key %{public}@ on model", log: tplogDebug, type: .default, crk.uuid)
                        reply(nil, nil, ContainerError.recoveryKeysNotEnrolled)
                        return
                    }

                    let crkRecoveryKey: CustodianRecoveryKey
                    do {
                        crkRecoveryKey = try CustodianRecoveryKey(tpCustodian: tpcrk, recoveryKeyString: crk.recoveryString, recoverySalt: crk.salt)
                    } catch {
                        os_log("preflightCustodianRecoveryKey: failed to create custodian recovery keys: %{public}@", log: tplogDebug, type: .default, error as CVarArg)
                        reply(nil, nil, ContainerError.failedToCreateRecoveryKey)
                        return
                    }

                    // Dear model: if I were to use this custodian recovery key, what peers would I end up using?
                    guard self.model.isCustodianRecoveryKeyEnrolled(tpcrk.peerID) else {
                        os_log("preflightCustodianRecoveryKey: custodian recovery Key is not enrolled", log: tplogDebug, type: .default)
                        reply(nil, nil, ContainerError.recoveryKeysNotEnrolled)
                        return
                    }

                    guard let sponsorPeerID = self.model.peerIDThatTrustsCustodianRecoveryKeys(tpcrk,
                                                                                               canIntroducePeer: selfPermanentInfo,
                                                                                               stableInfo: selfStableInfo) else {
                        os_log("preflightCustodianRecoveryKey Untrusted custodian recovery key", log: tplogDebug, type: .default)
                        reply(nil, nil, ContainerError.untrustedRecoveryKeys)
                        return
                    }

                    guard let sponsor = self.model.peer(withID: sponsorPeerID) else {
                        os_log("preflightCustodianRecoveryKey Failed to find peer with ID", log: tplogDebug, type: .default)
                        reply(nil, nil, ContainerError.sponsorNotRegistered(sponsorPeerID))
                        return
                    }

                    do {
                        let bestPolicy = try self.model.policy(forPeerIDs: sponsor.dynamicInfo?.includedPeerIDs ?? [sponsor.peerID],
                                                               candidatePeerID: egoPeerID,
                                                               candidateStableInfo: sponsor.stableInfo)
                        let syncingPolicy = try bestPolicy.syncingPolicy(forModel: selfPermanentInfo.modelID,
                                                                         syncUserControllableViews: sponsor.stableInfo?.syncUserControllableViews ?? .UNKNOWN, isInheritedAccount: selfStableInfo.isInheritedAccount)

                        reply(crkRecoveryKey.peerKeys.peerID, syncingPolicy, nil)
                    } catch {
                        os_log("preflightCustodianRecoveryKey: error fetching policy: %{public}@", log: tplogDebug, type: .default, error as CVarArg)
                        reply(nil, nil, error)
                        return
                    }
                }
            }
        }
    }
}
