import CoreData
import Foundation

extension Container {
    func preflightVouchWithBottle(bottleID: String,
                                  reply: @escaping (String?, Set<String>?, TPPolicy?, Error?) -> Void) {
        self.semaphore.wait()
        let reply: (String?, Set<String>?, TPPolicy?, Error?) -> Void = {
            os_log("preflightVouchWithBottle complete: %{public}@",
                   log: tplogTrace, type: .info, traceError($3))
            self.semaphore.signal()
            reply($0, $1, $2, $3)
        }

        self.fetchAndPersistChangesIfNeeded { fetchError in
            guard fetchError == nil else {
                os_log("preflightVouchWithBottle unable to fetch current peers: %{public}@", log: tplogDebug, type: .default, (fetchError as CVarArg?) ?? "")
                reply(nil, nil, nil, fetchError)
                return
            }

            // Ensure we have all policy versions claimed by peers, including our sponsor
            let allPolicyVersions = self.model.allPolicyVersions()
            self.fetchPolicyDocumentsWithSemaphore(versions: allPolicyVersions) { _, fetchPolicyDocumentsError in
                guard fetchPolicyDocumentsError == nil else {
                    os_log("preflightVouchWithBottle unable to fetch policy documents: %{public}@", log: tplogDebug, type: .default, (fetchPolicyDocumentsError as CVarArg?) ?? "no error")
                    reply(nil, nil, nil, fetchPolicyDocumentsError)
                    return
                }

                self.moc.performAndWait {
                    guard let egoPeerID = self.containerMO.egoPeerID,
                        let egoPermData = self.containerMO.egoPeerPermanentInfo,
                        let egoPermSig = self.containerMO.egoPeerPermanentInfoSig else {
                            os_log("fetchCurrentPolicy failed to find ego peer information", log: tplogDebug, type: .error)
                            reply(nil, nil, nil, ContainerError.noPreparedIdentity)
                            return
                    }

                    let keyFactory = TPECPublicKeyFactory()
                    guard let egoPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: egoPermData, sig: egoPermSig, keyFactory: keyFactory) else {
                        os_log("fetchCurrentPolicy failed to create TPPeerPermanentInfo", log: tplogDebug, type: .error)
                        reply(nil, nil, nil, ContainerError.invalidPermanentInfoOrSig)
                        return
                    }

                    self.onqueueFindBottle(bottleID: bottleID) { bottleMO, error in
                        guard let bottleMO = bottleMO else {
                            os_log("preflightVouchWithBottle found no bottle: %{public}@", log: tplogDebug, type: .default, (error as CVarArg?) ?? "")
                            reply(nil, nil, nil, error)
                            return
                        }

                        guard let sponsorPeer = self.model.peer(withID: bottleMO.peerID ?? "") else {
                            os_log("preflightVouchWithBottle found no peer to match bottle", log: tplogDebug, type: .default)
                            reply(nil, nil, nil, ContainerError.sponsorNotRegistered(bottleMO.peerID ?? "no peer ID given"))
                            return
                        }

                        guard let sponsorPeerStableInfo = sponsorPeer.stableInfo else {
                            os_log("preflightVouchWithBottle sponsor peer has no stable info", log: tplogDebug, type: .default)
                            reply(nil, nil, nil, ContainerError.sponsorNotRegistered(bottleMO.peerID ?? "no peer ID given"))
                            return
                        }

                        do {
                            // We need to extract the syncing policy that the remote peer would have used (if they were the type of device that we are)
                            // So, figure out their policy version...
                            let (views, policy) = try self.policyAndViewsFor(permanentInfo: egoPermanentInfo, stableInfo: sponsorPeerStableInfo)

                            reply(bottleMO.peerID, views, policy, nil)
                        } catch {
                            os_log("preflightVouchWithBottle failed to fetch policy: %{public}@", log: tplogDebug, type: .default, (error as CVarArg?) ?? "")
                            reply(nil, nil, nil, error)
                        }
                    }
                }
            }
        }
    }
}
