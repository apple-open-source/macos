import CoreData
import Foundation

private let logger = Logger(subsystem: "com.apple.security.trustedpeers", category: "recoverykey")

extension Container {
    func preflightVouchWithRecoveryKey(recoveryKey: String,
                                       salt: String,
                                       reply: @escaping (String?, TPSyncingPolicy?, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (String?, TPSyncingPolicy?, Error?) -> Void = {
            logger.info("preflightRecoveryKey complete: \(traceError($2), privacy: .public)")
            sem.release()
            reply($0, $1, $2)
        }

        self.fetchAndPersistChanges { fetchError in
            guard fetchError == nil else {
                logger.info("preflightRecoveryKey unable to fetch current peers: \(String(describing: fetchError), privacy: .public)")
                reply(nil, nil, fetchError)
                return
            }

            // Ensure we have all policy versions claimed by peers, including our sponsor
            let allPolicyVersions: Set<TPPolicyVersion>? = self.moc.performAndWait {
                do {
                    return try self.model.allPolicyVersions()
                } catch {
                    logger.error("Error fetching all policy versions: \(error, privacy: .public)")
                    reply(nil, nil, error)
                    return nil
                }
            }
            guard let allPolicyVersions else {
                return
            }
            self.fetchPolicyDocumentsWithSemaphore(versions: allPolicyVersions) { _, fetchPolicyDocumentsError in
                guard fetchPolicyDocumentsError == nil else {
                    logger.info("preflightRecoveryKey unable to fetch policy documents: \(String(describing: fetchPolicyDocumentsError), privacy: .public)")
                    reply(nil, nil, fetchPolicyDocumentsError)
                    return
                }

                self.moc.performAndWait {
                    guard let egoPeerID = self.containerMO.egoPeerID,
                        let egoPermData = self.containerMO.egoPeerPermanentInfo,
                        let egoPermSig = self.containerMO.egoPeerPermanentInfoSig,
                        let egoStableData = self.containerMO.egoPeerStableInfo,
                        let egoStableSig = self.containerMO.egoPeerStableInfoSig else {
                            logger.info("preflightRecoveryKey: no ego peer ID")
                            reply(nil, nil, ContainerError.noPreparedIdentity)
                            return
                    }

                    let keyFactory = TPECPublicKeyFactory()
                    guard let selfPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: egoPermData, sig: egoPermSig, keyFactory: keyFactory) else {
                        reply(nil, nil, ContainerError.invalidPermanentInfoOrSig)
                        return
                    }

                    guard let selfStableInfo = TPPeerStableInfo(data: egoStableData, sig: egoStableSig) else {
                        logger.info("cannot create TPPeerStableInfo")
                        reply(nil, nil, ContainerError.invalidStableInfoOrSig)
                        return
                    }

                    var recoveryKeys: RecoveryKey
                    do {
                        recoveryKeys = try RecoveryKey(recoveryKeyString: recoveryKey, recoverySalt: salt)

                        let signingPublicKey: Data = recoveryKeys.peerKeys.signingVerificationKey.keyData
                        let encryptionPublicKey: Data = recoveryKeys.peerKeys.encryptionVerificationKey.keyData

                        logger.info("preflightVouchWithRecoveryKey signingPubKey: \(signingPublicKey.base64EncodedString(), privacy: .public)")
                        logger.info("preflightVouchWithRecoveryKey encryptionPubKey: \(encryptionPublicKey.base64EncodedString(), privacy: .public)")
                    } catch {
                        logger.error("preflightRecoveryKey: failed to create recovery keys: \(String(describing: error), privacy: .public)")
                        reply(nil, nil, ContainerError.failedToCreateRecoveryKey(suberror: error))
                        return
                    }

                    // Dear model: if i were to use this recovery key, what peers would I end up using?
                    do {
                        guard try self.model.isRecoveryKeyEnrolled() else {
                            logger.info("preflightRecoveryKey: recovery Key is not enrolled")
                            reply(nil, nil, ContainerError.recoveryKeysNotEnrolled)
                            return
                        }
                    } catch {
                        logger.error("preflightRecoveryKey: error determine whether Recovery Key is enrolled: \(error, privacy: .public)")
                        reply(nil, nil, error)
                        return
                    }

                    let sponsorPeerID: String?
                    do {
                        sponsorPeerID = try self.model.peerIDThatTrustsRecoveryKeys(TPRecoveryKeyPair(signingKeyData: recoveryKeys.peerKeys.signingKey.publicKey.keyData,
                                                                                                      encryptionKeyData: recoveryKeys.peerKeys.encryptionKey.publicKey.keyData),
                                                                                    canIntroducePeer: selfPermanentInfo,
                                                                                    stableInfo: selfStableInfo)
                    } catch {
                        logger.info("preflightRecoveryKey failed to get peer that trusts RK: \(error, privacy: .public)")
                        reply(nil, nil, error)
                        return
                    }
                    guard let sponsorPeerID else {
                        logger.info("preflightRecoveryKey Untrusted recovery key set")
                        reply(nil, nil, ContainerError.untrustedRecoveryKeys)
                        return
                    }

                    let sponsor: TPPeer?
                    do {
                        sponsor = try self.model.peer(withID: sponsorPeerID)
                    } catch {
                        logger.warning("preflightRecoveryKey Error finding peer with ID \(sponsorPeerID, privacy: .public): \(String(describing:error), privacy: .public)")
                        reply(nil, nil, error)
                        return
                    }

                    guard let sponsor else {
                        logger.info("preflightRecoveryKey Failed to find peer with ID \(sponsorPeerID, privacy: .public)")
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
                        logger.error("preflightRecoveryKey: error fetching policy: \(String(describing: error), privacy: .public)")
                        reply(nil, nil, error)
                        return
                    }
                }
            }
        }
    }

    func preflightVouchWithCustodianRecoveryKey(crk: TrustedPeersHelperCustodianRecoveryKey,
                                                reply: @escaping (String?, TPSyncingPolicy?, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (String?, TPSyncingPolicy?, Error?) -> Void = {
            logger.info("preflightCustodianRecoveryKey complete: \(traceError($2), privacy: .public)")
            sem.release()
            reply($0, $1, $2)
        }

        self.fetchAndPersistChangesIfNeeded { fetchError in
            guard fetchError == nil else {
                logger.info("preflightCustodianRecoveryKey unable to fetch current peers: \(String(describing: fetchError), privacy: .public)")
                reply(nil, nil, fetchError)
                return
            }

            // Ensure we have all policy versions claimed by peers, including our sponsor
            let allPolicyVersions: Set<TPPolicyVersion>? = self.moc.performAndWait {
                do {
                    return try self.model.allPolicyVersions()
                } catch {
                    logger.error("Error fetching all policy versions: \(error, privacy: .public)")
                    reply(nil, nil, error)
                    return nil
                }
            }
            guard let allPolicyVersions else {
                return
            }
            self.fetchPolicyDocumentsWithSemaphore(versions: allPolicyVersions) { _, fetchPolicyDocumentsError in
                guard fetchPolicyDocumentsError == nil else {
                    logger.info("preflightCustodianRecoveryKey unable to fetch policy documents: \(String(describing: fetchPolicyDocumentsError), privacy: .public)")
                    reply(nil, nil, fetchPolicyDocumentsError)
                    return
                }

                self.moc.performAndWait {
                    guard let egoPeerID = self.containerMO.egoPeerID,
                        let egoPermData = self.containerMO.egoPeerPermanentInfo,
                        let egoPermSig = self.containerMO.egoPeerPermanentInfoSig,
                        let egoStableData = self.containerMO.egoPeerStableInfo,
                        let egoStableSig = self.containerMO.egoPeerStableInfoSig else {
                            logger.info("preflightCustodianRecoveryKey: no ego peer ID")
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
                        logger.info("Unable to parse uuid \(crk.uuid, privacy: .public)")
                        reply(nil, nil, ContainerError.recoveryKeysNotEnrolled)
                        return
                    }

                    guard let tpcrk = self.model.findCustodianRecoveryKey(with: uuid) else {
                        logger.info("Unable to find custodian recovery key \(crk.uuid, privacy: .public) on model")
                        reply(nil, nil, ContainerError.recoveryKeysNotEnrolled)
                        return
                    }

                    guard let recoveryKeyString = crk.recoveryString, let recoverySalt = crk.salt else {
                        logger.info("Bad format CRK: recovery string or salt not set")
                        reply(nil, nil, ContainerError.custodianRecoveryKeyMalformed)
                        return
                    }

                    let crkRecoveryKey: CustodianRecoveryKey
                    do {
                        crkRecoveryKey = try CustodianRecoveryKey(tpCustodian: tpcrk, recoveryKeyString: recoveryKeyString, recoverySalt: recoverySalt)
                    } catch {
                        logger.error("preflightCustodianRecoveryKey: failed to create custodian recovery keys: \(String(describing: error), privacy: .public)")
                        reply(nil, nil, ContainerError.failedToCreateRecoveryKey(suberror: error))
                        return
                    }

                    // Dear model: if I were to use this custodian recovery key, what peers would I end up using?
                    do {
                        guard try self.model.isCustodianRecoveryKeyTrusted(tpcrk) else {
                            logger.info("preflightCustodianRecoveryKey: custodian recovery key is not trusted")
                            reply(nil, nil, ContainerError.untrustedRecoveryKeys)
                            return
                        }
                    } catch {
                        logger.error("preflightCustodianRecoveryKey: error determining whether custodian recovery key is trusted: \(error, privacy: .public)")
                        reply(nil, nil, error)
                        return
                    }

                    let sponsorPeerID: String?
                    do {
                        sponsorPeerID = try self.model.peerIDThatTrustsCustodianRecoveryKeys(tpcrk,
                                                                                           canIntroducePeer: selfPermanentInfo,
                                                                                           stableInfo: selfStableInfo)
                    } catch {
                        logger.error("preflightCustodianRecoveryKey error getting peer that trusts CRK: \(error, privacy: .public)")
                        reply(nil, nil, ContainerError.untrustedRecoveryKeys)
                        return
                    }
                    guard let sponsorPeerID  else {
                        logger.info("preflightCustodianRecoveryKey Untrusted custodian recovery key")
                        reply(nil, nil, ContainerError.untrustedRecoveryKeys)
                        return
                    }

                    let sponsor: TPPeer?
                    do {
                        sponsor = try self.model.peer(withID: sponsorPeerID)
                    } catch {
                        logger.warning("preflightCustodianRecoveryKey Error finding peer with ID \(sponsorPeerID, privacy: .public): \(String(describing:error), privacy: .public)")
                        reply(nil, nil, error)
                        return
                    }

                    guard let sponsor else {
                        logger.info("preflightCustodianRecoveryKey Failed to find peer with ID \(sponsorPeerID, privacy: .public)")
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
                        logger.error("preflightCustodianRecoveryKey: error fetching policy: \(String(describing: error), privacy: .public)")
                        reply(nil, nil, error)
                        return
                    }
                }
            }
        }
    }

    func preflightRecoverOctagonWithRecoveryKey(recoveryKey: String,
                                                salt: String,
                                                reply: @escaping (Bool, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (Bool, Error?) -> Void = {
            logger.info("preflightRecoverOctagonWithRecoveryKey complete: \(traceError($1), privacy: .public)")
            sem.release()
            reply($0, $1)
        }

        self.fetchAndPersistChanges { error in
            guard error == nil else {
                logger.error("preflightRecoverOctagonWithRecoveryKey unable to fetch changes: \(String(describing: error), privacy: .public)")
                reply(false, error)
                return
            }

            self.moc.performAndWait {

                // inflate this RK
                var recoveryKeys: RecoveryKey
                do {
                    recoveryKeys = try RecoveryKey(recoveryKeyString: recoveryKey, recoverySalt: salt)

                    let signingPublicKey: Data = recoveryKeys.peerKeys.signingVerificationKey.keyData
                    let encryptionPublicKey: Data = recoveryKeys.peerKeys.encryptionVerificationKey.keyData

                    logger.info("preflightRecoverOctagonWithRecoveryKey signingPubKey: \(signingPublicKey.base64EncodedString(), privacy: .public)")
                    logger.info("preflightRecoverOctagonWithRecoveryKey encryptionPubKey: \(encryptionPublicKey.base64EncodedString(), privacy: .public)")
                } catch {
                    logger.error("preflightRecoverOctagonWithRecoveryKey: failed to create recovery keys: \(String(describing: error), privacy: .public)")
                    reply(false, ContainerError.failedToCreateRecoveryKey(suberror: error))
                    return
                }

                // is a RK enrolled and trusted?
                do {
                    guard try self.model.isRecoveryKeyEnrolled() else {
                        logger.info("preflightRecoverOctagonWithRecoveryKey: recovery Key is not enrolled")
                        reply(false, ContainerError.recoveryKeysNotEnrolled)
                        return
                    }
                } catch {
                    logger.error("preflightRecoverOctagonWithRecoveryKey: error determining whether Recovery Key is enrolled: \(error, privacy: .public)")
                    reply(false, ContainerError.recoveryKeysNotEnrolled)
                    return
                }

                // does this passed in recovery key match the enrolled one?
                guard self.model.recoverySigningPublicKey() == recoveryKeys.peerKeys.publicSigningKey?.keyData && self.model.recoveryEncryptionPublicKey() == recoveryKeys.peerKeys.publicEncryptionKey?.keyData else {
                    logger.info("preflightRecoverOctagonWithRecoveryKey: recovery Key is incorrect")
                    reply(false, ContainerError.recoveryKeyIsNotCorrect)
                    return
                }

                reply(true, nil)
            }
        }
    }
}
