import CoreData
import Foundation

private let logger = Logger(subsystem: "com.apple.security.trustedpeers", category: "bottledpeers")

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
            logger.info("preflightVouchWithBottle complete: \(traceError($3), privacy: .public)")
            self.semaphore.signal()
            reply($0, $1, $2, $3)
        }

        self.moc.performAndWait {
            do {
                let (_, peerID, syncingPolicy) = try self.onMOCQueuePerformPreflight(bottleID: bottleID)
                reply(peerID, syncingPolicy, false, nil)
            } catch {
                logger.debug("preflightVouchWithBottle failed; forcing refetch and retrying: \(String(describing: error), privacy: .public)")

                self.fetchAndPersistChanges { fetchError in
                    guard fetchError == nil else {
                        logger.debug("preflightVouchWithBottle unable to fetch current peers: \(String(describing: fetchError), privacy: .public)")
                        reply(nil, nil, true, fetchError)
                        return
                    }

                    // Ensure we have all policy versions claimed by peers, including our sponsor
                    let allPolicyVersions = self.model.allPolicyVersions()
                    self.fetchPolicyDocumentsWithSemaphore(versions: allPolicyVersions) { _, fetchPolicyDocumentsError in
                        guard fetchPolicyDocumentsError == nil else {
                            logger.debug("preflightVouchWithBottle unable to fetch policy documents: \(String(describing: fetchPolicyDocumentsError), privacy: .public)")
                            reply(nil, nil, true, fetchPolicyDocumentsError)
                            return
                        }

                        self.fetchViableBottlesWithSemaphore { _, _, fetchBottlesError in
                            guard fetchBottlesError == nil else {
                                logger.debug("preflightVouchWithBottle unable to fetch viable bottles: \(String(describing: fetchPolicyDocumentsError), privacy: .public)")
                                reply(nil, nil, true, fetchBottlesError)
                                return
                            }

                            // and try again:
                            self.moc.performAndWait {
                                do {
                                    let (_, peerID, syncingPolicy) = try self.onMOCQueuePerformPreflight(bottleID: bottleID)
                                    reply(peerID, syncingPolicy, true, nil)
                                } catch {
                                    logger.debug("preflightVouchWithBottle failed after refetches: \(String(describing: error), privacy: .public)")
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
            logger.error("fetchCurrentPolicy failed to find ego peer information")
            throw ContainerError.noPreparedIdentity
        }

        let keyFactory = TPECPublicKeyFactory()
        guard let egoPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: egoPermData, sig: egoPermSig, keyFactory: keyFactory) else {
            logger.error("fetchCurrentPolicy failed to create TPPeerPermanentInfo")
            throw ContainerError.invalidPermanentInfoOrSig
        }

        let bottleMO = try self.onMOCQueueFindBottle(bottleID: bottleID)

        guard let sponsorPeer = self.model.peer(withID: bottleMO.peerID ?? "") else {
            logger.debug("preflightVouchWithBottle found no peer to match bottle")
            throw ContainerError.sponsorNotRegistered(bottleMO.peerID ?? "no peer ID given")
        }

        guard let sponsorPeerStableInfo = sponsorPeer.stableInfo else {
            logger.debug("preflightVouchWithBottle sponsor peer has no stable info")
            throw ContainerError.sponsorNotRegistered(bottleMO.peerID ?? "no peer ID given")
        }

        // We need to extract the syncing policy that the remote peer would have used (if they were the type of device that we are)
        let policy = try self.syncingPolicyFor(modelID: egoPermanentInfo.modelID, stableInfo: sponsorPeerStableInfo)
        return (bottleMO, sponsorPeer.peerID, policy)
    }
}
