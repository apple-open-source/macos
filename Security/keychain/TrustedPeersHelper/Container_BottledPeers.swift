import CoreData
import Foundation

extension Container {
    func onMOCQueueFindBottle(bottleID: String) throws -> (BottleMO) {
        guard let containerBottles = self.containerMO.bottles as? Set<BottleMO> else {
            throw ContainerError.noBottlesPresent
        }

        let bottles = containerBottles.filter { $0.bottleID == bottleID }

        guard let bottle = bottles.first else {
            throw ContainerError.noBottlesForEscrowRecordID
        }

        return bottle
    }

    func preflightVouchWithBottle(bottleID: String,
                                  reply: @escaping (String?, TPSyncingPolicy?, Bool, Error?) -> Void) {
        self.semaphore.wait()
        let reply: (String?, TPSyncingPolicy?, Bool, Error?) -> Void = {
            os_log("preflightVouchWithBottle complete: %{public}@",
                   log: tplogTrace, type: .info, traceError($3))
            self.semaphore.signal()
            reply($0, $1, $2, $3)
        }

        self.moc.performAndWait {
            do {
                let (_, peerID, syncingPolicy) = try self.onMOCQueuePerformPreflight(bottleID: bottleID)
                reply(peerID, syncingPolicy, false, nil)
            } catch {
                os_log("preflightVouchWithBottle failed; forcing refetch and retrying: %{public}@", log: tplogDebug, type: .default, (error as CVarArg?) ?? "")

                self.fetchAndPersistChanges { fetchError in
                    guard fetchError == nil else {
                        os_log("preflightVouchWithBottle unable to fetch current peers: %{public}@", log: tplogDebug, type: .default, (fetchError as CVarArg?) ?? "")
                        reply(nil, nil, true, fetchError)
                        return
                    }

                    // Ensure we have all policy versions claimed by peers, including our sponsor
                    let allPolicyVersions = self.model.allPolicyVersions()
                    self.fetchPolicyDocumentsWithSemaphore(versions: allPolicyVersions) { _, fetchPolicyDocumentsError in
                        guard fetchPolicyDocumentsError == nil else {
                            os_log("preflightVouchWithBottle unable to fetch policy documents: %{public}@", log: tplogDebug, type: .default, (fetchPolicyDocumentsError as CVarArg?) ?? "no error")
                            reply(nil, nil, true, fetchPolicyDocumentsError)
                            return
                        }

                        self.fetchViableBottlesWithSemaphore { _, _, fetchBottlesError in
                            guard fetchBottlesError == nil else {
                                os_log("preflightVouchWithBottle unable to fetch viable bottles: %{public}@", log: tplogDebug, type: .default, (fetchPolicyDocumentsError as CVarArg?) ?? "no error")
                                reply(nil, nil, true, fetchBottlesError)
                                return
                            }

                            // and try again:
                            self.moc.performAndWait {
                                do {
                                    let (_, peerID, syncingPolicy) = try self.onMOCQueuePerformPreflight(bottleID: bottleID)
                                    reply(peerID, syncingPolicy, true, nil)
                                } catch {
                                    os_log("preflightVouchWithBottle failed after refetches; failing: %{public}@", log: tplogDebug, type: .default, (error as CVarArg?) ?? "")
                                    reply(nil, nil, true, error)
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    func onMOCQueuePerformPreflight(bottleID: String) throws -> (BottleMO, String, TPSyncingPolicy) {
        guard let egoPeerID = self.containerMO.egoPeerID,
              let egoPermData = self.containerMO.egoPeerPermanentInfo,
              let egoPermSig = self.containerMO.egoPeerPermanentInfoSig else {
            os_log("fetchCurrentPolicy failed to find ego peer information", log: tplogDebug, type: .error)
            throw ContainerError.noPreparedIdentity
        }

        let keyFactory = TPECPublicKeyFactory()
        guard let egoPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: egoPermData, sig: egoPermSig, keyFactory: keyFactory) else {
            os_log("fetchCurrentPolicy failed to create TPPeerPermanentInfo", log: tplogDebug, type: .error)
            throw ContainerError.invalidPermanentInfoOrSig
        }

        let bottleMO = try self.onMOCQueueFindBottle(bottleID: bottleID)

        guard let sponsorPeer = self.model.peer(withID: bottleMO.peerID ?? "") else {
            os_log("preflightVouchWithBottle found no peer to match bottle", log: tplogDebug, type: .default)
            throw ContainerError.sponsorNotRegistered(bottleMO.peerID ?? "no peer ID given")
        }

        guard let sponsorPeerStableInfo = sponsorPeer.stableInfo else {
            os_log("preflightVouchWithBottle sponsor peer has no stable info", log: tplogDebug, type: .default)
            throw ContainerError.sponsorNotRegistered(bottleMO.peerID ?? "no peer ID given")
        }

        // We need to extract the syncing policy that the remote peer would have used (if they were the type of device that we are)
        let policy = try self.syncingPolicyFor(modelID: egoPermanentInfo.modelID, stableInfo: sponsorPeerStableInfo)
        return (bottleMO, sponsorPeer.peerID, policy)
    }
}
