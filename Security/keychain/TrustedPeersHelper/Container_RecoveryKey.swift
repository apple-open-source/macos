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
                        let egoPermSig = self.containerMO.egoPeerPermanentInfoSig else {
                            os_log("preflightRecoveryKey: no ego peer ID", log: tplogDebug, type: .default)
                            reply(nil, nil, ContainerError.noPreparedIdentity)
                            return
                    }

                    let keyFactory = TPECPublicKeyFactory()
                    guard let selfPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: egoPermData, sig: egoPermSig, keyFactory: keyFactory) else {
                        reply(nil, nil, ContainerError.invalidPermanentInfoOrSig)
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
                                                                                                        encryptionKeyData: recoveryKeys.peerKeys.encryptionKey.publicKey.keyData)) else {
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
                                                                         syncUserControllableViews: sponsor.stableInfo?.syncUserControllableViews ?? .UNKNOWN)

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
}
