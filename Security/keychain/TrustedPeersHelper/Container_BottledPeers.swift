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
        let sem = self.grabSemaphore()
        let reply: (String?, TPSyncingPolicy?, Bool, Error?) -> Void = {
            logger.info("preflightVouchWithBottle complete: \(traceError($3), privacy: .public)")
            sem.release()
            reply($0, $1, $2, $3)
        }

        self.moc.performAndWait {
            do {
                let (_, peerID, syncingPolicy) = try self.onMOCQueuePerformPreflight(bottleID: bottleID)
                reply(peerID, syncingPolicy, false, nil)
            } catch {
                logger.info("preflightVouchWithBottle failed; forcing refetch and retrying: \(String(describing: error), privacy: .public)")

                self.fetchAndPersistChanges { fetchError in
                    guard fetchError == nil else {
                        logger.info("preflightVouchWithBottle unable to fetch current peers: \(String(describing: fetchError), privacy: .public)")
                        reply(nil, nil, true, fetchError)
                        return
                    }

                    // Ensure we have all policy versions claimed by peers, including our sponsor
                    let allPolicyVersions: Set<TPPolicyVersion>? = self.moc.performAndWait {
                        do {
                            return try self.model.allPolicyVersions()
                        } catch {
                            logger.error("Error fetching all policy versions: \(error, privacy: .public)")
                            reply(nil, nil, true, error)
                            return nil
                        }
                    }
                    guard let allPolicyVersions else {
                        return
                    }
                    self.fetchPolicyDocumentsWithSemaphore(versions: allPolicyVersions) { _, fetchPolicyDocumentsError in
                        guard fetchPolicyDocumentsError == nil else {
                            logger.info("preflightVouchWithBottle unable to fetch policy documents: \(String(describing: fetchPolicyDocumentsError), privacy: .public)")
                            reply(nil, nil, true, fetchPolicyDocumentsError)
                            return
                        }

                        self.fetchViableBottlesWithSemaphore(from: .default, flowID: nil, deviceSessionID: nil) { _, _, fetchBottlesError in
                            guard fetchBottlesError == nil else {
                                logger.info("preflightVouchWithBottle unable to fetch viable bottles: \(String(describing: fetchPolicyDocumentsError), privacy: .public)")
                                reply(nil, nil, true, fetchBottlesError)
                                return
                            }

                            // and try again:
                            self.moc.performAndWait {
                                do {
                                    let (_, peerID, syncingPolicy) = try self.onMOCQueuePerformPreflight(bottleID: bottleID)
                                    reply(peerID, syncingPolicy, true, nil)
                                } catch {
                                    logger.error("preflightVouchWithBottle failed after refetches: \(String(describing: error), privacy: .public)")
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

        let sponsorPeer: TPPeer?
        do {
            sponsorPeer = try self.model.peer(withID: bottleMO.peerID ?? "")
        } catch {
            logger.warning("preflightVouchWithBottle Error finding peer with ID \(bottleMO.peerID ?? "no peer ID given", privacy: .public): \(String(describing: error), privacy: .public)")
            throw error
        }

        guard let sponsorPeer else {
            logger.info("preflightVouchWithBottle found no peer to match bottle with ID \(bottleMO.peerID ?? "no peer ID given", privacy: .public)")
            throw ContainerError.sponsorNotRegistered(bottleMO.peerID ?? "no peer ID given")
        }

        guard let sponsorPeerStableInfo = sponsorPeer.stableInfo else {
            logger.info("preflightVouchWithBottle sponsor peer has no stable info")
            throw ContainerError.sponsorNotRegistered(bottleMO.peerID ?? "no peer ID given")
        }

        let policy = try self.syncingPolicyFor(modelID: egoPermanentInfo.modelID, stableInfo: sponsorPeerStableInfo)
        return (bottleMO, sponsorPeer.peerID, policy)
    }
}
