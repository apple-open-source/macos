/*
 * Copyright (c) 2018 - 2020 Apple Inc. All Rights Reserved.
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

import CloudKitCode
import CoreData
import Foundation
import InternalSwiftProtobuf
import os
import Security
import SecurityFoundation

private let logger = Logger(subsystem: "com.apple.security.trustedpeers", category: "container")

private let egoIdentitiesAccessGroup = "com.apple.security.egoIdentities"

enum Viability {
    case full
    case partial
    case none
}

extension ResetReason {
    static func from(cuttlefishResetReason: CuttlefishResetReason) -> ResetReason {
        switch cuttlefishResetReason {
        case .unknown:
            return ResetReason.unknown
        case .userInitiatedReset:
            return  ResetReason.userInitiatedReset
        case .healthCheck:
            return ResetReason.healthCheck
        case .noBottleDuringEscrowRecovery:
            return ResetReason.noBottleDuringEscrowRecovery
        case .legacyJoinCircle:
            return ResetReason.legacyJoinCircle
        case .recoveryKey:
            return ResetReason.recoveryKey
        case .testGenerated:
            return ResetReason.testGenerated
        @unknown default:
            fatalError("unknown reset reason: \(cuttlefishResetReason)")
        }
    }
}

public enum ContainerError: Error {
    case unableToCreateKeyPair
    case noPreparedIdentity
    case failedToStoreIdentity
    case needsAuthentication
    case missingStableInfo
    case missingDynamicInfo
    case nonMember
    case invalidPermanentInfoOrSig
    case invalidStableInfoOrSig
    case invalidVoucherOrSig
    case sponsorNotRegistered(String)
    case unknownPolicyVersion(UInt64)
    case preparedIdentityNotOnAllowedList(String)
    case couldNotLoadAllowedList
    case noPeersPreapprovePreparedIdentity
    case policyDocumentDoesNotValidate
    case tooManyBottlesForPeer
    case noBottleForPeer
    case restoreBottleFailed
    case noBottlesForEscrowRecordID
    case bottleDoesNotContainContents
    case bottleDoesNotContainEscrowKeySignature
    case bottleDoesNotContainerPeerKeySignature
    case bottleDoesNotContainPeerID
    case failedToCreateBottledPeer
    case signatureVerificationFailed
    case bottleDoesNotContainerEscrowKeySPKI
    case failedToFetchEscrowContents
    case failedToCreateRecoveryKey
    case untrustedRecoveryKeys
    case noBottlesPresent
    case recoveryKeysNotEnrolled
    case bottleCreatingPeerNotFound
    case unknownCloudKitError
    case cloudkitResponseMissing
    case failedToLoadSecret(errorCode: Int)
    case failedToLoadSecretDueToType
    case failedToAssembleBottle
    case invalidPeerID
    case failedToStoreSecret(errorCode: Int)
    case unknownSecurityFoundationError
    case failedToSerializeData
    case unknownInternalError
    case unknownSyncUserControllableViewsValue(value: Int32)
    case noPeersPreapprovedBySelf
    case peerRegisteredButNotStored(String)
}

extension ContainerError: LocalizedError {
    public var errorDescription: String? {
        switch self {
        case .unableToCreateKeyPair:
            return "unable to create key pair"
        case .noPreparedIdentity:
            return "no prepared identity"
        case .failedToStoreIdentity:
            return "failed to stored identity"
        case .needsAuthentication:
            return "needs authentication"
        case .missingStableInfo:
            return "missing stable info"
        case .missingDynamicInfo:
            return "missing dynamic info"
        case .nonMember:
            return "non member"
        case .invalidPermanentInfoOrSig:
            return "invalid permanent info or signature"
        case .invalidStableInfoOrSig:
            return "invalid stable info or signature"
        case .invalidVoucherOrSig:
            return "invalid voucher or signature"
        case .sponsorNotRegistered(let s):
            return "sponsor not registered: \(s)"
        case .unknownPolicyVersion(let v):
            return "unknown policy version: \(v)"
        case .preparedIdentityNotOnAllowedList(let id):
            return "prepared identity (\(id)) not on allowed machineID list"
        case .couldNotLoadAllowedList:
            return "could not load allowed machineID list"
        case .noPeersPreapprovePreparedIdentity:
            return "no peers preapprove prepared identity"
        case .policyDocumentDoesNotValidate:
            return "policy document from server doesn't validate"
        case .tooManyBottlesForPeer:
            return "too many bottles exist for peer"
        case .noBottleForPeer:
            return "no bottle exists for peer"
        case .restoreBottleFailed:
            return "failed to restore bottle"
        case .noBottlesForEscrowRecordID:
            return "0 bottles exist for escrow record id"
        case .bottleDoesNotContainContents:
            return "bottle does not contain encrypted contents"
        case .bottleDoesNotContainEscrowKeySignature:
            return "bottle does not contain escrow signature"
        case .bottleDoesNotContainerPeerKeySignature:
            return "bottle does not contain peer signature"
        case .bottleDoesNotContainPeerID:
            return "bottle does not contain peer id"
        case .failedToCreateBottledPeer:
            return "failed to create a bottled peer"
        case .signatureVerificationFailed:
            return "failed to verify signature"
        case .bottleDoesNotContainerEscrowKeySPKI:
            return "bottle does not contain escrowed key spki"
        case .failedToFetchEscrowContents:
            return "failed to fetch escrow contents"
        case .failedToCreateRecoveryKey:
            return "failed to create recovery keys"
        case .untrustedRecoveryKeys:
            return "untrusted recovery keys"
        case .noBottlesPresent:
            return "no bottle present"
        case .recoveryKeysNotEnrolled:
            return "recovery key is not enrolled with octagon"
        case .bottleCreatingPeerNotFound:
            return "The peer that created the bottle was not found"
        case .unknownCloudKitError:
            return "unknown error from cloudkit"
        case .cloudkitResponseMissing:
            return "Response missing from CloudKit"
        case .failedToLoadSecret(errorCode: let errorCode):
            return "failed to load secret: \(errorCode)"
        case .failedToLoadSecretDueToType:
            return "Failed to load secret due to type mismatch (value was not dictionary)"
        case .failedToAssembleBottle:
            return "failed to assemble bottle for peer"
        case .invalidPeerID:
            return "peerID is invalid"
        case .failedToStoreSecret(errorCode: let errorCode):
            return "failed to store the secret in the keychain \(errorCode)"
        case .unknownSecurityFoundationError:
            return "SecurityFoundation returned an unknown type"
        case .failedToSerializeData:
            return "Failed to encode protobuf data"
        case .unknownInternalError:
            return "Internal code failed, but didn't return error"
        case .unknownSyncUserControllableViewsValue(value: let value):
            return "Unknown syncUserControllableViews number: \(value)"
        case .noPeersPreapprovedBySelf:
            return "No peers preapproved by the local peer"
        case .peerRegisteredButNotStored(let s):
            return "Peer \(s) not found in database"
        }
    }
}

extension ContainerError: CustomNSError {
    public static var errorDomain: String {
        return "com.apple.security.trustedpeers.container"
    }

    public var errorCode: Int {
        switch self {
        case .unableToCreateKeyPair:
            return 0
        case .noPreparedIdentity:
            return 1
        case .failedToStoreIdentity:
            return 2
        case .needsAuthentication:
            return 3
        case .missingStableInfo:
            return 4
        case .missingDynamicInfo:
            return 5
        case .nonMember:
            return 6
        case .invalidPermanentInfoOrSig:
            return 7
        case .invalidVoucherOrSig:
            return 8
        case .invalidStableInfoOrSig:
            return 9
        case .sponsorNotRegistered:
            return 11
        case .unknownPolicyVersion:
            return 12
        case .preparedIdentityNotOnAllowedList:
            return 13
        case .noPeersPreapprovePreparedIdentity:
            return 14
        case .policyDocumentDoesNotValidate:
            return 15
        // 16 was invalidPeerID and failedToAssembleBottle
        case .tooManyBottlesForPeer:
            return 17
        case .noBottleForPeer:
            return 18
        case .restoreBottleFailed:
            return 19
        case .noBottlesForEscrowRecordID:
            return 20
        case .bottleDoesNotContainContents:
            return 21
        case .bottleDoesNotContainEscrowKeySignature:
            return 22
        case .bottleDoesNotContainerPeerKeySignature:
            return 23
        case .bottleDoesNotContainPeerID:
            return 24
        case .failedToCreateBottledPeer:
            return 25
        case .signatureVerificationFailed:
            return 26
        // 27 was failedToLoadSecret*
        case .bottleDoesNotContainerEscrowKeySPKI:
            return 28
        case .failedToFetchEscrowContents:
            return 29
        case .couldNotLoadAllowedList:
            return 30
        case .failedToCreateRecoveryKey:
            return 31
        case .untrustedRecoveryKeys:
            return 32
        case .noBottlesPresent:
            return 33
        case .recoveryKeysNotEnrolled:
            return 34
        case .bottleCreatingPeerNotFound:
            return 35
        case .unknownCloudKitError:
            return 36
        case .cloudkitResponseMissing:
            return 37
        case .failedToLoadSecret:
            return 38
        case .failedToLoadSecretDueToType:
            return 39
        case .failedToStoreSecret:
            return 40
        case .failedToAssembleBottle:
            return 41
        case .invalidPeerID:
            return 42
        case .unknownSecurityFoundationError:
            return 43
        case .failedToSerializeData:
            return 44
        case .unknownInternalError:
            return 45
        case .unknownSyncUserControllableViewsValue:
            return 46
        case .noPeersPreapprovedBySelf:
            return 47
        case .peerRegisteredButNotStored:
            return 48
        }
    }

    public var underlyingError: NSError? {
        switch self {
        case .failedToLoadSecret(errorCode: let errorCode):
            return NSError(domain: "securityd", code: errorCode)
        case .failedToStoreSecret(errorCode: let errorCode):
            return NSError(domain: "securityd", code: errorCode)
        default:
            return nil
        }
    }

    public var errorUserInfo: [String: Any] {
        var ret = [String: Any]()
        if let desc = self.errorDescription {
            ret[NSLocalizedDescriptionKey] = desc
        }
        if let underlyingError = self.underlyingError {
            ret[NSUnderlyingErrorKey] = underlyingError
        }
        return ret
    }
}

extension NSError {
    func evaluateErrorForCorruption() -> Bool {
        if let underlyingInitError = self.userInfo[NSUnderlyingErrorKey] as? Error {
            let underlyingInitNSError = underlyingInitError as NSError

            if self.code == NSMigrationError && self.domain == NSCocoaErrorDomain && underlyingInitNSError.code == SQLITE_CORRUPT && underlyingInitNSError.domain == NSSQLiteErrorDomain {
                return true
            } else {
                return false
            }
        } else if self.code == SQLITE_CORRUPT && self.domain == NSSQLiteErrorDomain {
            return true
        }

        return false
    }
}

internal func traceError(_ error: Error?) -> String {
    if let error = error {
        return "error: \(String(describing: error))"
    } else {
        return "success"
    }
}

func saveSecret(_ secret: Data, label: String) throws {
    let query: [CFString: Any] = [
        kSecClass: kSecClassInternetPassword,
        kSecAttrAccessible: kSecAttrAccessibleWhenUnlocked,
        kSecUseDataProtectionKeychain: true,
        kSecAttrAccessGroup: "com.apple.security.octagon",
        kSecAttrSynchronizable: false,
        kSecAttrDescription: label,
        kSecAttrPath: label,
        kSecValueData: secret,
        ]

    var results: CFTypeRef?
    var status = SecItemAdd(query as CFDictionary, &results)

    if status == errSecSuccess {
        return
    }

    if status == errSecDuplicateItem {
        // Add every primary key attribute to this find dictionary
        var findQuery: [CFString: Any] = [:]
        findQuery[kSecClass]              = query[kSecClass]
        findQuery[kSecAttrSynchronizable] = query[kSecAttrSynchronizable]
        findQuery[kSecAttrAccessGroup]    = query[kSecAttrAccessGroup]
        findQuery[kSecAttrServer]         = query[kSecAttrDescription]
        findQuery[kSecAttrPath]           = query[kSecAttrPath]
        findQuery[kSecUseDataProtectionKeychain] = query[kSecUseDataProtectionKeychain]

        var updateQuery: [CFString: Any] = query
        updateQuery[kSecClass] = nil

        status = SecItemUpdate(findQuery as CFDictionary, updateQuery as CFDictionary)

        if status != errSecSuccess {
            throw ContainerError.failedToStoreSecret(errorCode: Int(status))
        }
    } else {
        throw ContainerError.failedToStoreSecret(errorCode: Int(status))
    }
}

func loadSecret(label: String) throws -> (Data?) {
    var secret: Data?

    let query: [CFString: Any] = [
        kSecClass: kSecClassInternetPassword,
        kSecAttrAccessGroup: "com.apple.security.octagon",
        kSecAttrDescription: label,
        kSecReturnAttributes: true,
        kSecReturnData: true,
        kSecAttrSynchronizable: false,
        kSecMatchLimit: kSecMatchLimitOne,
        ]

    var result: CFTypeRef?
    let status = SecItemCopyMatching(query as CFDictionary, &result)

    if status != errSecSuccess || result == nil {
        throw ContainerError.failedToLoadSecret(errorCode: Int(status))
    }

    if result != nil {
        if let dictionary = result as? [CFString: Any] {
            secret = dictionary[kSecValueData] as? Data
        } else {
            throw ContainerError.failedToLoadSecretDueToType
        }
    }
    return secret
}

func saveEgoKeyPair(_ keyPair: _SFECKeyPair, identifier: String, resultHandler: @escaping (Bool, Error?) -> Void) {
    let keychainManager = _SFKeychainManager.default()
    let signingIdentity = _SFIdentity(keyPair: keyPair)
    let accessibility = SFAccessibilityMakeWithMode(SFAccessibilityMode.accessibleWhenUnlocked)  // class A
    let accessPolicy = _SFAccessPolicy(accessibility: accessibility,
                                       sharingPolicy: SFSharingPolicy.thisDeviceOnly)
    accessPolicy.accessGroup = egoIdentitiesAccessGroup
    keychainManager.setIdentity(signingIdentity,
                                forIdentifier: identifier,
                                accessPolicy: accessPolicy,
                                resultHandler: resultHandler)
}

func loadEgoKeyPair(identifier: String, resultHandler: @escaping (_SFECKeyPair?, Error?) -> Void) {
    let keychainManager = _SFKeychainManager.default()

    // FIXME constrain to egoIdentitiesAccessGroup, <rdar://problem/39597940>
    keychainManager.identity(forIdentifier: identifier) { result in
        switch result.resultType {
        case .valueAvailable:
            resultHandler(result.value?.keyPair as? _SFECKeyPair, nil)
        case .needsAuthentication:
            resultHandler(nil, ContainerError.needsAuthentication)
        case .error:
            resultHandler(nil, result.error)
        @unknown default:
            resultHandler(nil, ContainerError.unknownSecurityFoundationError)
        }
    }
}

func loadEgoKeys(peerID: String, resultHandler: @escaping (OctagonSelfPeerKeys?, Error?) -> Void) {
    loadEgoKeyPair(identifier: signingKeyIdentifier(peerID: peerID)) { signingKey, error in
        guard let signingKey = signingKey else {
            logger.debug("Unable to load signing key: \(String(describing: error), privacy: .public)")
            resultHandler(nil, error)
            return
        }

        loadEgoKeyPair(identifier: encryptionKeyIdentifier(peerID: peerID)) { encryptionKey, error in
            guard let encryptionKey = encryptionKey else {
                logger.debug("Unable to load encryption key: \(String(describing: error), privacy: .public)")
                resultHandler(nil, error)
                return
            }

            do {
                resultHandler(try OctagonSelfPeerKeys(peerID: peerID, signingKey: signingKey, encryptionKey: encryptionKey), nil)
            } catch {
                resultHandler(nil, error)
            }
        }
    }
}

func removeEgoKeysSync(peerID: String) throws -> Bool {
    var resultSema = DispatchSemaphore(value: 0)

    let keychainManager = _SFKeychainManager.default()

    var retresultForSigningDeletion: Bool = false
    var reterrorForSigningDeletion: Error?

    // remove signing keys
    keychainManager.removeItem(withIdentifier: signingKeyIdentifier(peerID: peerID)) { result, error in
        retresultForSigningDeletion = result
        reterrorForSigningDeletion = error
        resultSema.signal()
    }

    resultSema.wait()

    if let error = reterrorForSigningDeletion {
        throw error
    }

    if retresultForSigningDeletion == false {
        return retresultForSigningDeletion
    }

    // now let's do the same thing with the encryption keys
    resultSema = DispatchSemaphore(value: 0)
    var retresultForEncryptionDeletion: Bool = false
    var reterrorForEncryptionDeletion: Error?

    keychainManager.removeItem(withIdentifier: encryptionKeyIdentifier(peerID: peerID)) { result, error in
        retresultForEncryptionDeletion = result
        reterrorForEncryptionDeletion = error
        resultSema.signal()
    }
    resultSema.wait()

    if let error = reterrorForEncryptionDeletion {
        throw error
    }

    return retresultForEncryptionDeletion && retresultForSigningDeletion
}

func loadEgoKeysSync(peerID: String) throws -> OctagonSelfPeerKeys {
    // Gotta promote to synchronous; 'antipattern' ahoy
    let resultSema = DispatchSemaphore(value: 0)

    var result: OctagonSelfPeerKeys?
    var reserror: Error?

    loadEgoKeys(peerID: peerID) { keys, error in
        result = keys
        reserror = error
        resultSema.signal()
    }
    resultSema.wait()

    if let error = reserror {
        throw error
    }

    if let result = result {
        return result
    }

    abort()
}

func signingKeyIdentifier(peerID: String) -> String {
    return "signing-key " + peerID
}

func encryptionKeyIdentifier(peerID: String) -> String {
    return "encryption-key " + peerID
}

func makeCKKSTLKShares(ckksTLKs: [CKKSKeychainBackedKey]?, asPeer: CKKSSelfPeer, toPeer: CKKSPeer, epoch: Int) throws -> [CKKSTLKShare] {
    return try (ckksTLKs ?? []).map { tlk in
        logger.debug("Making TLKShare for \(String(describing: toPeer), privacy: .public) for key \(String(describing: tlk), privacy: .public)")
        // Not being able to convert a TLK to a TLKShare is a failure, but not having a TLK is only half bad
        do {
            return try CKKSTLKShare(tlk, as: asPeer, to: toPeer, epoch: epoch, poisoned: 0)
        } catch {
            let nserror = error as NSError
            if nserror.domain == "securityd" && nserror.code == errSecItemNotFound {
                logger.debug("No TLK contents for \(String(describing: tlk), privacy: .public), no TLK share possible")
                return nil
            } else {
                throw error
            }
        }
    }
    .compactMap { $0 }
}

func makeTLKShares(ckksTLKs: [CKKSKeychainBackedKey]?, asPeer: CKKSSelfPeer, toPeer: CKKSPeer, epoch: Int) throws -> [TLKShare] {
    return try makeCKKSTLKShares(ckksTLKs: ckksTLKs, asPeer: asPeer, toPeer: toPeer, epoch: epoch).compactMap { TLKShare.convert(ckksTLKShare: $0) }
}

@discardableResult
func extract(tlkShares: [CKKSTLKShare], peer: OctagonSelfPeerKeys, model: TPModel) -> ([CKKSKeychainBackedKey], TrustedPeersHelperTLKRecoveryResult) {
    logger.debug("Attempting to recover \(tlkShares.count) shares for peer \(peer.peerID, privacy: .public)")

    return extract(tlkShares: tlkShares, peer: peer, sponsorPeerID: nil, model: model)
}

@discardableResult
func extract(tlkShares: [CKKSTLKShare],
             peer: OctagonSelfPeerKeys,
             sponsorPeerID: String?,
             model: TPModel) -> ([CKKSKeychainBackedKey], TrustedPeersHelperTLKRecoveryResult) {
    var tlksRecovered: [String: CKKSKeychainBackedKey] = [:]
    var sharesRecovered: Int64 = 0

    var trustedPeers: [AnyHashable] = [peer]

    var recoveryErrors: [String: [Error]] = [:]

    if let egoPeer = model.peer(withID: sponsorPeerID ?? peer.peerID) {
        egoPeer.trustedPeerIDs.forEach { trustedPeerID in
            if let peer = model.peer(withID: trustedPeerID) {
                let peerObj = CKKSActualPeer(peerID: trustedPeerID,
                                             encryptionPublicKey: (peer.permanentInfo.encryptionPubKey as! _SFECPublicKey),
                                             signing: (peer.permanentInfo.signingPubKey as! _SFECPublicKey),
                                             viewList: [])

                trustedPeers.append(peerObj)
            } else {
                logger.debug("No peer for trusted ID \(trustedPeerID, privacy: .public)")
            }
        }
    } else {
        logger.debug("No ego peer in model; no trusted peers")
    }

    for share in tlkShares {
        guard share.receiverPeerID == peer.peerID else {
            logger.debug("Skipping \(String(describing: share), privacy: .public) (wrong peerID)")
            continue
        }

        do {
            let key = try share.recoverTLK(peer,
                                           trustedPeers: Set(trustedPeers),
                                           ckrecord: nil)

            try key.saveMaterialToKeychain()

            tlksRecovered[key.uuid] = key
            sharesRecovered += 1
            logger.debug("Recovered \(String(describing: key), privacy: .public) (from \(String(describing: share), privacy: .public)")
        } catch {
            logger.debug("Failed to recover share \(String(describing: share), privacy: .public): \(String(describing: error), privacy: .public)")

            if let list = recoveryErrors[share.tlkUUID] {
                recoveryErrors[share.tlkUUID] = list + [error]
            } else {
                recoveryErrors[share.tlkUUID] = [error]
            }
        }
    }

    logger.debug("Recovered TLKs: \(String(describing: tlksRecovered), privacy: .public)")

    return (Array(tlksRecovered.values), TrustedPeersHelperTLKRecoveryResult(successfulKeyUUIDs: Set(tlksRecovered.keys),
                                                                             totalTLKSharesRecovered: sharesRecovered,
                                                                             tlkRecoveryErrors: recoveryErrors))
}

struct ContainerState {
    struct Bottle: Hashable {
        let bottleID: String?
        let contents: Data?
        let escrowedSigningSPKI: Data?
        let peerID: String?
        let signatureUsingEscrowKey: Data?
        let signatureUsingPeerKey: Data?

        init(bottleMO: BottleMO) {
            self.bottleID = bottleMO.bottleID
            self.contents = bottleMO.contents
            self.escrowedSigningSPKI = bottleMO.escrowedSigningSPKI
            self.peerID = bottleMO.peerID
            self.signatureUsingEscrowKey = bottleMO.signatureUsingEscrowKey
            self.signatureUsingPeerKey = bottleMO.signatureUsingPeerKey
        }
    }
    struct EscrowRecord: Hashable {
        let creationDate: Date?
        let label: String?
        let recordStatus: Int64
        let remainingAttempts: Int64
        let silentAttemptAllowed: Int64
        let sosViability: Int64
        let federationID: String?
        let expectedFederationID: String?

        init(escrowRecordMO: EscrowRecordMO) {
            self.creationDate = escrowRecordMO.creationDate
            self.label = escrowRecordMO.label
            self.recordStatus = escrowRecordMO.recordStatus
            self.remainingAttempts = escrowRecordMO.remainingAttempts
            self.silentAttemptAllowed = escrowRecordMO.silentAttemptAllowed
            self.sosViability = escrowRecordMO.sosViability
            self.federationID = escrowRecordMO.federationID
            self.expectedFederationID = escrowRecordMO.expectedFederationID
        }
    }

    var egoPeerID: String?
    var peers: [String: TPPeer] = [:]
    var vouchers: [TPVoucher] = []
    var bottles = Set<Bottle>()
    var escrowRecords = Set<EscrowRecord>()
    var recoverySigningKey: Data?
    var recoveryEncryptionKey: Data?
}

internal struct StableChanges {
    let deviceName: String?
    let serialNumber: String?
    let osVersion: String?
    let policyVersion: UInt64?
    let policySecrets: [String: Data]?
    let setSyncUserControllableViews: TPPBPeerStableInfoUserControllableViewStatus?
    let secureElementIdentity: TrustedPeersHelperIntendedTPPBSecureElementIdentity?
}

// CoreData doesn't handle creating an identical model from an identical URL. Help it out.
private var nsObjectModels: [URL: NSManagedObjectModel] = [:]
private let nsObjectModelsQueue = DispatchQueue(label: "com.apple.security.TrustedPeersHelper.nsObjectModels")

func getOrMakeModel(url: URL) -> NSManagedObjectModel {
    return nsObjectModelsQueue.sync {
        if let model = nsObjectModels[url] {
            return model
        }
        let newModel = NSManagedObjectModel(contentsOf: url)!
        nsObjectModels[url] = newModel
        return newModel
    }
}

struct ContainerName: Hashable, CustomStringConvertible {
    let container: String
    let context: String

    func asSingleString() -> String {
        // Just to be nice, hide the fact that multiple contexts exist on most machines
        if self.context == OTDefaultContext {
            return self.container
        } else {
            return self.container + "-" + self.context
        }
    }

    var description: String {
        return "Container(\(self.container),\(self.context))"
    }
}

extension ContainerMO {
    func egoStableInfo() -> TPPeerStableInfo? {
        guard let egoStableData = self.egoPeerStableInfo,
            let egoStableSig = self.egoPeerStableInfoSig else {
                return nil
        }

        return TPPeerStableInfo(data: egoStableData, sig: egoStableSig)
    }
}

/// This maps to a Cuttlefish service backed by a CloudKit container,
/// and a corresponding local Core Data persistent container.
///
/// Methods may be invoked concurrently.
class Container: NSObject {
    let name: ContainerName

    private let cuttlefish: CuttlefishAPIAsync

    // Only one request (from Client) is permitted to be in progress at a time.
    // That includes while waiting for network, i.e. one request must complete
    // before the next can begin. Otherwise two requests could in parallel be
    // fetching updates from Cuttlefish, with ensuing changeToken overwrites etc.
    // This applies for mutating requests -- requests that can only read the current
    // state (on the moc queue) do not need to.
    internal let semaphore = DispatchSemaphore(value: 1)

    // All Core Data access happens through moc: NSManagedObjectContext. The
    // moc insists on having its own queue, and all operations must happen on
    // that queue.
    internal let moc: NSManagedObjectContext

    // To facilitate CoreData tear down, we need to keep the PersistentContainer around.
    internal let persistentContainer: NSPersistentContainer

    // Rather than Container having its own dispatch queue, we use moc's queue
    // to synchronise access to our own state as well. So the following instance
    // variables must only be accessed within blocks executed by calling
    // moc.perform() or moc.performAndWait().
    internal var containerMO: ContainerMO
    internal var model: TPModel
    internal var escrowCacheTimeout: TimeInterval

    // Used in tests only. Set when an identity is prepared using a policy version override
    internal var policyVersionOverride: TPPolicyVersion?
    internal var testIgnoreCustodianUpdates: Bool = false
    internal var testDontSetAccountSetting: Bool? = false

    internal let darwinNotifier: CKKSNotifier.Type

    // If you add a new field to the Cuttlefish Changes protocol, such that
    // old devices will ignore it silently, but still persist a change tag
    // beyond the data they ignored, consider increasing this level to cause
    // those devices to refetch all data once they upgrade to this software
    // (which persist the data as its refetched).
    //
    // Refetch level 1: Custodian peers
    internal static let currentRefetchLevel: Int64 = 1

    /**
     Construct a Container.

     - Parameter name: The name the CloudKit container to which requests will be routed.

     The "real" container that drives CKKS etc should be named `"com.apple.security.keychain"`.
     Use other names for test containers such as for rawfish (rawhide-style testing for cuttlefish)

     - Parameter persistentStoreURL: The location the local Core Data database for this container will be stored.

     - Parameter cuttlefish: Interface to cuttlefish.
     */
    init(name: ContainerName,
         persistentStoreDescription: NSPersistentStoreDescription,
         darwinNotifier: CKKSNotifier.Type,
         cuttlefish: CuttlefishAPIAsync) throws {
        var initError: Error?
        var containerMO: ContainerMO?
        var model: TPModel?

        self.darwinNotifier = darwinNotifier

        // Set up Core Data stack
        let url = Bundle(for: type(of: self)).url(forResource: "TrustedPeersHelper", withExtension: "momd")!
        let mom = getOrMakeModel(url: url)
        self.persistentContainer = NSPersistentContainer(name: "TrustedPeersHelper", managedObjectModel: mom)
        self.persistentContainer.persistentStoreDescriptions = [persistentStoreDescription]

        let moc = self.persistentContainer.newBackgroundContext()

        self.persistentContainer.loadPersistentStores { _, error in
            initError = error
        }
        if let initErr = initError {
            let initNSError = initErr as NSError

            if initNSError.evaluateErrorForCorruption() {
                var destroyError: Error?

                moc.performAndWait {
                    if let url = persistentStoreDescription.url {
                        do {
                            try moc.persistentStoreCoordinator?.destroyPersistentStore(at: url,
                                                                                       ofType: persistentStoreDescription.type,
                                                                                       options: [:])
                            initError = nil
                        } catch {
                            destroyError = error
                        }
                    }
                }

                if let destroyError = destroyError {
                    throw destroyError
                }

                // now try loading again
                var initRetryError: Error?
                self.persistentContainer.loadPersistentStores { _, retryError in
                    initRetryError = retryError
                }

                if let initRetryError = initRetryError {
                    throw initRetryError
                }
            } else {
                throw initErr
            }
        }

        moc.mergePolicy = NSMergePolicy.mergeByPropertyStoreTrump

        moc.performAndWait {
            // Fetch an existing ContainerMO record if it exists, or create and save one
            do {
                let containerFetch = NSFetchRequest<NSFetchRequestResult>(entityName: "Container")
                containerFetch.predicate = NSPredicate(format: "name == %@", name.asSingleString())
                let fetchedContainers = try moc.fetch(containerFetch)
                if let container = fetchedContainers.first as? ContainerMO {
                    containerMO = container
                } else {
                    containerMO = ContainerMO(context: moc)
                    containerMO!.name = name.asSingleString()
                }

                // Perform upgrades as needed
                Container.onqueueUpgradeMachineIDSetToModel(container: containerMO!, moc: moc)
                Container.onqueueUpgradeMachineIDSetToUseStatus(container: containerMO!, moc: moc)

                // remove duplicate vouchers on all the peers
                Container.onqueueRemoveDuplicateVouchersPerPeer(container: containerMO!, moc: moc)

                model = Container.loadModel(from: containerMO!)
                Container.ensureEgoConsistency(from: containerMO!, model: model!)
                try moc.save()
            } catch {
                initError = error
                return
            }
        }
        if let initError = initError {
            throw initError
        }

        self.name = name
        self.moc = moc
        self.containerMO = containerMO!
        self.cuttlefish = cuttlefish
        self.model = model!
        self.escrowCacheTimeout = 60.0 * 15.0 // 15 minutes
        super.init()
    }

    func deletePersistentStore() throws {
        // Call this to entirely destroy the persistent store.
        // This container should not be used after this event.

        try self.persistentContainer.persistentStoreDescriptions.forEach { storeDescription in
            if let url = storeDescription.url {
                try self.moc.persistentStoreCoordinator?.destroyPersistentStore(at: url,
                                                                                ofType: storeDescription.type,
                                                                                options: [:])
            }
        }
    }

    // Must be on containerMO's moc queue to call this
    internal static func loadModel(from containerMO: ContainerMO) -> TPModel {
        // Populate model from persistent store
        let model = TPModel(decrypter: Decrypter())
        let keyFactory = TPECPublicKeyFactory()
        let peers = containerMO.peers as? Set<PeerMO>
        peers?.forEach { peer in
            guard let permanentInfo = TPPeerPermanentInfo(peerID: peer.peerID!,
                                                          data: peer.permanentInfo! as Data,
                                                          sig: peer.permanentInfoSig! as Data,
                                                          keyFactory: keyFactory) else {
                                                            return
            }
            model.registerPeer(with: permanentInfo)
            if let data = peer.stableInfo, let sig = peer.stableInfoSig {
                if let stableInfo = TPPeerStableInfo(data: data as Data, sig: sig as Data) {
                    do {
                        try model.update(stableInfo, forPeerWithID: permanentInfo.peerID)
                    } catch {
                        logger.debug("loadModel unable to update stable info for peer(\(peer, privacy: .public)): \(String(describing: error), privacy: .public)")
                    }
                } else {
                    logger.debug("loadModel: peer \(permanentInfo.peerID, privacy: .public) has unparseable stable info")
                }
            } else {
                logger.debug("loadModel: peer \(permanentInfo.peerID, privacy: .public) has no stable info")
            }
            if let data = peer.dynamicInfo, let sig = peer.dynamicInfoSig {
                if let dynamicInfo = TPPeerDynamicInfo(data: data as Data, sig: sig as Data) {
                    do {
                        try model.update(dynamicInfo, forPeerWithID: permanentInfo.peerID)
                    } catch {
                        logger.debug("loadModel unable to update dynamic info for peer(\(peer, privacy: .public)): \(String(describing: error), privacy: .public)")
                    }
                } else {
                    logger.debug("loadModel: peer \(permanentInfo.peerID, privacy: .public) has unparseable dynamic info")
                }
            } else {
                logger.debug("loadModel: peer \(permanentInfo.peerID, privacy: .public) has no dynamic info")
            }
            peer.vouchers?.forEach {
                let v = $0 as! VoucherMO
                if let data = v.voucherInfo, let sig = v.voucherInfoSig {
                    if let voucher = TPVoucher(infoWith: data, sig: sig) {
                        model.register(voucher)
                    }
                }
            }
        }

        let crks = containerMO.custodianRecoveryKeys as? Set<CustodianRecoveryKeyMO>
        crks?.forEach { crk in
            if let data = crk.crkInfo, let sig = crk.crkInfoSig {
                if let tpcrk = TPCustodianRecoveryKey(data: data, sig: sig, keyFactory: keyFactory) {
                    model.register(tpcrk)
                }
            }
        }

        logger.debug("loadModel: loaded \(model.allPeerIDs().count) peers")
        logger.debug("loadModel: loaded \(model.allVouchers().count) vouchers")
        logger.debug("loadModel: loaded \(model.allCustodianRecoveryKeys().count) CRKs")

        // Note: the containerMO objects are misnamed; they are key data, and not SPKI.
        if let recoveryKeySigningKeyData = containerMO.recoveryKeySigningSPKI,
            let recoveryKeyEncyryptionKeyData = containerMO.recoveryKeyEncryptionSPKI {
            model.setRecoveryKeys(TPRecoveryKeyPair(signingKeyData: recoveryKeySigningKeyData, encryptionKeyData: recoveryKeyEncyryptionKeyData))
        } else {
            // If the ego peer has an RK set, tell the model to use that one
            // This is a hack to work around TPH databases which don't have the RK set on the container due to previously running old software
            if let egoStableInfo = containerMO.egoStableInfo(),
               let recoverySigningPublicKey = egoStableInfo.recoverySigningPublicKey,
               let recoveryEncryptionPublicKey = egoStableInfo.recoveryEncryptionPublicKey,
                !recoverySigningPublicKey.isEmpty,
                !recoveryEncryptionPublicKey.isEmpty {
                logger.debug("loadModel: recovery key not set in model, but is set on ego peer")
                model.setRecoveryKeys(TPRecoveryKeyPair(signingKeyData: recoverySigningPublicKey, encryptionKeyData: recoveryEncryptionPublicKey))
            }
        }

        // Register persisted policies (cached from cuttlefish)
        let policies = containerMO.policies as? Set<PolicyMO>
        policies?.forEach { policyMO in
            if let policyHash = policyMO.policyHash,
                let policyData = policyMO.policyData {
                if let policyDoc = TPPolicyDocument.policyDoc(withHash: policyHash, data: policyData) {
                    model.register(policyDoc)
                }
            }
        }

        // Register built-in policies
        builtInPolicyDocuments.forEach { policyDoc in
            model.register(policyDoc)
        }

        let knownMachines = containerMO.machines as? Set<MachineMO> ?? Set()
        let allowedMachineIDs = Set(knownMachines.filter { $0.status == TPMachineIDStatus.allowed.rawValue }.compactMap { $0.machineID })
        let disallowedMachineIDs = Set(knownMachines.filter { $0.status == TPMachineIDStatus.disallowed.rawValue }.compactMap { $0.machineID })

        logger.debug("loadModel: allowedMachineIDs: \(allowedMachineIDs, privacy: .public)")
        logger.debug("loadModel: disallowedMachineIDs: \(disallowedMachineIDs, privacy: .public)")

        if allowedMachineIDs.isEmpty {
            logger.debug("loadModel: no allowedMachineIDs?")
        }

        return model
    }

    // Must be on containerMO's moc queue to call this
    internal static func ensureEgoConsistency(from containerMO: ContainerMO, model: TPModel) {
        guard let egoPeerID = containerMO.egoPeerID,
            let egoStableData = containerMO.egoPeerStableInfo,
            let egoStableSig = containerMO.egoPeerStableInfoSig
            else {
                logger.error("ensureEgoConsistency failed to find ego peer information")
                return
        }

        guard let containerEgoStableInfo = TPPeerStableInfo(data: egoStableData, sig: egoStableSig) else {
            logger.error("ensureEgoConsistency failed to create TPPeerStableInfo from container")
            return
        }

        guard let modelStableInfo = model.getStableInfoForPeer(withID: egoPeerID) else {
            logger.error("ensureEgoConsistency failed to create TPPeerStableInfo from model")
            return
        }

        if modelStableInfo.clock > containerEgoStableInfo.clock {
            containerMO.egoPeerStableInfo = modelStableInfo.data
            containerMO.egoPeerStableInfoSig = modelStableInfo.sig
        }
    }

    static func dictionaryRepresentation(bottle: BottleMO) -> [String: Any] {
        var dict: [String: String] = [:]

        dict["bottleID"] = bottle.bottleID
        dict["peerID"] = bottle.peerID
        dict["signingSPKI"] = bottle.escrowedSigningSPKI?.base64EncodedString()
        dict["signatureUsingPeerKey"] = bottle.signatureUsingPeerKey?.base64EncodedString()
        dict["signatureUsingSPKI"] = bottle.signatureUsingEscrowKey?.base64EncodedString()
        // ignore the bottle contents; they're mostly unreadable

        return dict
    }

    static func peerdictionaryRepresentation(peer: TPPeer) -> [String: Any] {
        autoreleasepool {
            var peerDict: [String: Any] = [
                "permanentInfo": peer.permanentInfo.dictionaryRepresentation(),
                "peerID": peer.peerID,
            ]
            if let stableInfo = peer.stableInfo {
                peerDict["stableInfo"] = stableInfo.dictionaryRepresentation()
            }
            if let dynamicInfo = peer.dynamicInfo {
                peerDict["dynamicInfo"] = dynamicInfo.dictionaryRepresentation()
            }

            return peerDict
        }
    }

    func onQueueDetermineLocalTrustStatus(reply: @escaping (TrustedPeersHelperEgoPeerStatus, Error?) -> Void) {
        let viablePeerCountsByModelID = self.model.viablePeerCountsByModelID()
        let peerCountsByMachineID = self.model.peerCountsByMachineID()
        if let egoPeerID = self.containerMO.egoPeerID {
            let egoPermanentInfo = self.model.peer(withID: egoPeerID)?.permanentInfo

            var status = self.model.statusOfPeer(withID: egoPeerID)
            var isExcluded: Bool = (status == .excluded)

            loadEgoKeys(peerID: egoPeerID) { egoPeerKeys, loadError in
                var returnError = loadError

                guard returnError == nil else {
                    var isLocked = false
                    if let error = (loadError as NSError?) {
                        logger.debug("trust status: Unable to load ego keys: \(String(describing: error), privacy: .public)")
                        if error.code == errSecItemNotFound && error.domain == NSOSStatusErrorDomain {
                            logger.debug("trust status: Lost the ego key pair, returning 'excluded' in hopes of fixing up the identity")
                            isExcluded = true
                            status = .excluded
                        } else if error.code == errSecInteractionNotAllowed && error.domain == NSOSStatusErrorDomain {
                            // we have an item, but can't read it, suppressing error
                            isLocked = true
                            returnError = nil
                        }
                    }

                    let egoStatus = TrustedPeersHelperEgoPeerStatus(egoPeerID: egoPeerID,
                                                                    egoPeerMachineID: egoPermanentInfo?.machineID,
                                                                    status: status,
                                                                    viablePeerCountsByModelID: viablePeerCountsByModelID,
                                                                    peerCountsByMachineID: peerCountsByMachineID,
                                                                    isExcluded: isExcluded,
                                                                    isLocked: isLocked)
                    reply(egoStatus, returnError)
                    return
                }

                // ensure egoPeerKeys are populated
                guard egoPeerKeys != nil else {
                    logger.debug("trust status: No error but Ego Peer Keys are nil")
                    let egoStatus = TrustedPeersHelperEgoPeerStatus(egoPeerID: egoPeerID,
                                                                    egoPeerMachineID: egoPermanentInfo?.machineID,
                                                                    status: .excluded,
                                                                    viablePeerCountsByModelID: viablePeerCountsByModelID,
                                                                    peerCountsByMachineID: peerCountsByMachineID,
                                                                    isExcluded: true,
                                                                    isLocked: false)

                    reply(egoStatus, loadError)
                    return
                }

                let egoStatus = TrustedPeersHelperEgoPeerStatus(egoPeerID: egoPeerID,
                                                                egoPeerMachineID: egoPermanentInfo?.machineID,
                                                                status: status,
                                                                viablePeerCountsByModelID: viablePeerCountsByModelID,
                                                                peerCountsByMachineID: peerCountsByMachineID,
                                                                isExcluded: isExcluded,
                                                                isLocked: false)
                reply(egoStatus, nil)
                return
            }
        } else {
            // With no ego peer ID, either return 'excluded' if there are extant peers, or 'unknown' to signal no peers at all
            if self.model.allPeerIDs().isEmpty {
                logger.debug("No existing peers in account")
                let egoStatus = TrustedPeersHelperEgoPeerStatus(egoPeerID: nil,
                                                                egoPeerMachineID: nil,
                                                                status: .unknown,
                                                                viablePeerCountsByModelID: viablePeerCountsByModelID,
                                                                peerCountsByMachineID: peerCountsByMachineID,
                                                                isExcluded: false,
                                                                isLocked: false)
                reply(egoStatus, nil)
                return
            } else {
                logger.debug("Existing peers in account, but we don't have a peer ID. We are excluded.")
                let egoStatus = TrustedPeersHelperEgoPeerStatus(egoPeerID: nil,
                                                                egoPeerMachineID: nil,
                                                                status: .excluded,
                                                                viablePeerCountsByModelID: viablePeerCountsByModelID,
                                                                peerCountsByMachineID: peerCountsByMachineID,
                                                                isExcluded: true,
                                                                isLocked: false)
                reply(egoStatus, nil)
                return
            }
        }
    }

    func trustStatus(reply: @escaping (TrustedPeersHelperEgoPeerStatus, Error?) -> Void) {
        self.semaphore.wait()
        let reply: (TrustedPeersHelperEgoPeerStatus, Error?) -> Void = {
            // Suppress logging of successful replies here; it's not that useful
            let logType: OSLogType = $1 == nil ? .debug : .info
            logger.log(level: logType, "trustStatus complete: \(TPPeerStatusToString($0.egoStatus), privacy: .public) \(traceError($1), privacy: .public)")
            self.semaphore.signal()
            reply($0, $1)
        }
        self.moc.performAndWait {
            // Knowledge of your peer status only exists if you know about other peers. If you haven't fetched, fetch.
            if self.containerMO.changeToken == nil {
                self.onqueueFetchAndPersistChanges { fetchError in
                    guard fetchError == nil else {
                        if let error = fetchError {
                            logger.debug("Unable to fetch changes, trust status is unknown: \(String(describing: error), privacy: .public)")
                        }

                        let egoStatus = TrustedPeersHelperEgoPeerStatus(egoPeerID: nil,
                                                                        egoPeerMachineID: nil,
                                                                        status: .unknown,
                                                                        viablePeerCountsByModelID: [:],
                                                                        peerCountsByMachineID: [:],
                                                                        isExcluded: false,
                                                                        isLocked: false)
                        reply(egoStatus, fetchError)
                        return
                    }

                    self.moc.performAndWait {
                        self.onQueueDetermineLocalTrustStatus(reply: reply)
                    }
                }
            } else {
                self.onQueueDetermineLocalTrustStatus(reply: reply)
            }
        }
    }

    func fetchTrustState(reply: @escaping (TrustedPeersHelperPeerState?, [TrustedPeersHelperPeer]?, Error?) -> Void) {
        let reply: (TrustedPeersHelperPeerState?, [TrustedPeersHelperPeer]?, Error?) -> Void = {
            logger.info("fetch trust state complete: \(String(reflecting: $0), privacy: .public) \(traceError($2), privacy: .public)")
            reply($0, $1, $2)
        }

        self.moc.performAndWait {
            if let egoPeerID = self.containerMO.egoPeerID,
                let egoPermData = self.containerMO.egoPeerPermanentInfo,
                let egoPermSig = self.containerMO.egoPeerPermanentInfoSig {
                let keyFactory = TPECPublicKeyFactory()
                guard let permanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: egoPermData, sig: egoPermSig, keyFactory: keyFactory) else {
                    logger.error("fetchTrustState failed to create TPPeerPermanentInfo")
                    reply(nil, nil, ContainerError.invalidPermanentInfoOrSig)
                    return
                }

                let isPreapproved = self.model.hasPotentiallyTrustedPeerPreapprovingKey(permanentInfo.signingPubKey.spki())
                logger.debug("fetchTrustState: ego peer is \(isPreapproved ? "preapproved" : "not yet preapproved", privacy: .public)")

                let egoStableInfo = self.model.getStableInfoForPeer(withID: egoPeerID)

                let egoPeerStatus = TrustedPeersHelperPeerState(peerID: egoPeerID,
                                                                isPreapproved: isPreapproved,
                                                                status: self.model.statusOfPeer(withID: egoPeerID),
                                                                memberChanges: false,
                                                                unknownMachineIDs: self.onqueueFullIDMSListWouldBeHelpful(),
                                                                osVersion: egoStableInfo?.osVersion)

                var tphPeers: [TrustedPeersHelperPeer] = []

                if let egoPeer = self.model.peer(withID: egoPeerID) {
                    egoPeer.trustedPeerIDs.forEach { trustedPeerID in
                        if let peer = self.model.peer(withID: trustedPeerID) {
                            let peerViews = try? self.model.getViewsForPeer(peer.permanentInfo,
                                                                            stableInfo: peer.stableInfo)

                            tphPeers.append(TrustedPeersHelperPeer(peerID: trustedPeerID,
                                                                   signingSPKI: peer.permanentInfo.signingPubKey.spki(),
                                                                   encryptionSPKI: peer.permanentInfo.encryptionPubKey.spki(),
                                                                   secureElementIdentity: peer.stableInfo?.secureElementIdentity,
                                                                   viewList: peerViews ?? Set()))
                        } else if let crk = self.model.custodianPeer(withID: trustedPeerID) {
                            do {
                                let crkViews = try? self.model.getViewsForCRK(crk,
                                                                              donorPermanentInfo: egoPeer.permanentInfo,
                                                                              donorStableInfo: egoPeer.stableInfo)

                                tphPeers.append(try crk.asCustodianPeer(viewList: crkViews ?? Set()))
                            } catch {
                                logger.debug("Unable to add CRK as a trusted peer: \(String(describing: error), privacy: .public)")
                            }
                        } else {
                            logger.debug("No peer for trusted ID \(trustedPeerID, privacy: .public)")
                        }
                    }

                    if let stableInfo = egoPeer.stableInfo,
                       let recoveryEncryptionPublicKey = stableInfo.recoveryEncryptionPublicKey,
                       let recoverySigningPublicKey = stableInfo.recoverySigningPublicKey,
                        !recoveryEncryptionPublicKey.isEmpty,
                        !recoverySigningPublicKey.isEmpty {
                        let recoveryKeyPair = TPRecoveryKeyPair(stableInfo: stableInfo)

                        do {
                            // The RK should have all views. So, claim that it should have all views that this peer has.
                            let rkViews = try self.model.getViewsForPeer(egoPeer.permanentInfo,
                                                                         stableInfo: egoPeer.stableInfo)

                            tphPeers.append(try RecoveryKey.asPeer(recoveryKeys: recoveryKeyPair,
                                                                   viewList: rkViews))
                        } catch {
                            logger.debug("Unable to add RK as a trusted peer: \(String(describing: error), privacy: .public)")
                        }
                    }
                } else {
                    logger.debug("No ego peer in model; no trusted peers")
                }

                logger.debug("Returning trust state: \(egoPeerStatus, privacy: .public) \(tphPeers, privacy: .public)")
                reply(egoPeerStatus, tphPeers, nil)
            } else {
                // With no ego peer ID, there are no trusted peers
                logger.debug("No peer ID => no trusted peers")
                reply(TrustedPeersHelperPeerState(peerID: nil, isPreapproved: false, status: .unknown, memberChanges: false, unknownMachineIDs: false, osVersion: nil), [], nil)
            }
        }
    }

    func dump(reply: @escaping ([AnyHashable: Any]?, Error?) -> Void) {
        let reply: ([AnyHashable: Any]?, Error?) -> Void = {
            logger.info("dump complete: \(traceError($1), privacy: .public)")
            reply($0, $1)
        }
        self.moc.performAndWait {
            var d: [AnyHashable: Any] = [:]

            if let egoPeerID = self.containerMO.egoPeerID {
                if let peer = self.model.peer(withID: egoPeerID) {
                    d["self"] = Container.peerdictionaryRepresentation(peer: peer)
                } else {
                    d["self"] = ["peerID": egoPeerID]
                }
            } else {
                d["self"] = [:]
            }

            autoreleasepool {
                d["peers"] = self.model.allPeers().filter { $0.peerID != self.containerMO.egoPeerID }.map { peer in
                    Container.peerdictionaryRepresentation(peer: peer)
                }

                d["vouchers"] = self.model.allVouchers().map { $0.dictionaryRepresentation() }

                d["custodian_recovery_keys"] = self.model.allCustodianRecoveryKeys().map { $0.dictionaryRepresentation() }

                if let bottles = self.containerMO.bottles as? Set<BottleMO> {
                    d["bottles"] = bottles.map { Container.dictionaryRepresentation(bottle: $0) }
                } else {
                    d["bottles"] = []
                }
            }

            let midList = self.onqueueCurrentMIDList()
            d["machineIDsAllowed"] = midList.machineIDs(in: .allowed).sorted()
            d["machineIDsDisallowed"] = midList.machineIDs(in: .disallowed).sorted()
            d["modelRecoverySigningPublicKey"] = self.model.recoverySigningPublicKey()
            d["modelRecoveryEncryptionPublicKey"] = self.model.recoveryEncryptionPublicKey()
            d["registeredPolicyVersions"] = self.model.allRegisteredPolicyVersions().sorted().map { policyVersion in "\(policyVersion.versionNumber), \(policyVersion.policyHash)" }

            reply(d, nil)
        }
    }

    func dumpEgoPeer(reply: @escaping (String?, TPPeerPermanentInfo?, TPPeerStableInfo?, TPPeerDynamicInfo?, Error?) -> Void) {
        let reply: (String?, TPPeerPermanentInfo?, TPPeerStableInfo?, TPPeerDynamicInfo?, Error?) -> Void = {
            logger.info("dumpEgoPeer complete: \(traceError($4), privacy: .public)")
            reply($0, $1, $2, $3, $4)
        }
        self.moc.performAndWait {
            guard let egoPeerID = self.containerMO.egoPeerID else {
                reply(nil, nil, nil, nil, ContainerError.noPreparedIdentity)
                return
            }

            guard let peer = self.model.peer(withID: egoPeerID) else {
                reply(egoPeerID, nil, nil, nil, nil)
                return
            }

            reply(egoPeerID, peer.permanentInfo, peer.stableInfo, peer.dynamicInfo, nil)
        }
    }

    func validatePeers(request: ValidatePeersRequest, reply: @escaping ([AnyHashable: Any]?, Error?) -> Void) {
        self.semaphore.wait()
        let reply: ([AnyHashable: Any]?, Error?) -> Void = {
            logger.info("validatePeers complete \(traceError($1), privacy: .public)")
            self.semaphore.signal()
            reply($0, $1)
        }

        self.cuttlefish.validatePeers(request) { response in
            switch response {
            case .success(let response):
                var info: [AnyHashable: Any] = [:]
                info["health"] = response.validatorsHealth as AnyObject
                info["results"] = try? JSONSerialization.jsonObject(with: response.jsonUTF8Data())

                reply(info, nil)
            case .failure(let error):
                logger.debug("validatePeers failed: \(String(describing: error), privacy: .public)")
                reply(nil, error)
                return
            }
        }
    }

    func reset(resetReason: CuttlefishResetReason, reply: @escaping (Error?) -> Void) {
        self.semaphore.wait()
        let reply: (Error?) -> Void = {
            logger.info("reset complete \(traceError($0), privacy: .public)")
            self.semaphore.signal()
            reply($0)
        }

        self.moc.performAndWait {
            let resetReason = ResetReason.from(cuttlefishResetReason: resetReason)
            let request = ResetRequest.with {
                $0.resetReason = resetReason
            }
            self.cuttlefish.reset(request) { response in
                switch response {
                case .success(let response):
                    // Erase container's persisted state
                    self.moc.performAndWait {
                        self.moc.delete(self.containerMO)
                        self.containerMO = ContainerMO(context: self.moc)
                        self.containerMO.name = self.name.asSingleString()
                        self.model = Container.loadModel(from: self.containerMO)
                        self.darwinNotifier.post(OTCliqueChanged)
                        do {
                            try self.onQueuePersist(changes: response.changes)
                            logger.debug("reset succeded")
                            reply(nil)
                        } catch {
                            logger.debug("reset persist failed: \(String(describing: error), privacy: .public)")
                            reply(error)
                        }
                    }
                case .failure(let error):
                    logger.debug("reset failed: \(String(describing: error), privacy: .public)")
                    reply(error)
                    return
                }
            }
        }
    }

    func localReset(reply: @escaping (Error?) -> Void) {
        self.semaphore.wait()
        let reply: (Error?) -> Void = {
            logger.info("localReset complete \(traceError($0), privacy: .public)")
            self.semaphore.signal()
            reply($0)
        }

        self.moc.performAndWait {
            do {
                // Erase container's persisted state
                self.moc.delete(self.containerMO)
                self.containerMO = ContainerMO(context: self.moc)
                self.containerMO.name = self.name.asSingleString()
                self.model = Container.loadModel(from: self.containerMO)
                try self.moc.save()
            } catch {
                reply(error)
                return
            }
            reply(nil)
        }
    }

    func loadOrCreateKeyPair(privateKeyPersistentRef: Data?) throws -> _SFECKeyPair {
        if let privateKeyPersistentRef = privateKeyPersistentRef {
            return try TPHObjectiveC.fetchKeyPair(withPrivateKeyPersistentRef: privateKeyPersistentRef)
        } else {
            let keySpecifier = _SFECKeySpecifier(curve: SFEllipticCurve.nistp384)
            guard let keyPair = _SFECKeyPair(randomKeyPairWith: keySpecifier) else {
                throw ContainerError.unableToCreateKeyPair
            }
            return keyPair
        }
    }

    // policyVersion should only be non-nil for testing, to override prevailingPolicyVersion.versionNumber
    func prepare(epoch: UInt64,
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
                 signingPrivateKeyPersistentRef: Data?,
                 encryptionPrivateKeyPersistentRef: Data?,
                 reply: @escaping (String?, Data?, Data?, Data?, Data?, TPSyncingPolicy?, Error?) -> Void) {
        self.semaphore.wait()
        let reply: (String?, Data?, Data?, Data?, Data?, TPSyncingPolicy?, Error?) -> Void = {
            logger.info("prepare complete peerID: \(String(describing: $0), privacy: .public) \(traceError($6), privacy: .public)")
            self.semaphore.signal()
            reply($0, $1, $2, $3, $4, $5, $6)
        }

        // Create a new peer identity with random keys, and store the keys in keychain
        let permanentInfo: TPPeerPermanentInfo
        let signingKeyPair: _SFECKeyPair
        let encryptionKeyPair: _SFECKeyPair
        do {
            signingKeyPair = try self.loadOrCreateKeyPair(privateKeyPersistentRef: signingPrivateKeyPersistentRef)
            encryptionKeyPair = try self.loadOrCreateKeyPair(privateKeyPersistentRef: encryptionPrivateKeyPersistentRef)

            // <rdar://problem/56270219> Octagon: use epoch transmitted across pairing channel
            permanentInfo = try TPPeerPermanentInfo(machineID: machineID,
                                                    modelID: modelID,
                                                    epoch: 1,
                                                    signing: signingKeyPair,
                                                    encryptionKeyPair: encryptionKeyPair,
                                                    peerIDHashAlgo: TPHashAlgo.SHA256)
        } catch {
            reply(nil, nil, nil, nil, nil, nil, error)
            return
        }

        let peerID = permanentInfo.peerID

        let bottle: BottledPeer
        do {
            bottle = try BottledPeer(peerID: peerID,
                                     bottleID: bottleID,
                                     peerSigningKey: signingKeyPair,
                                     peerEncryptionKey: encryptionKeyPair,
                                     bottleSalt: bottleSalt)

            _ = try saveSecret(bottle.secret, label: peerID)
        } catch {
            logger.debug("bottle creation failed: \(String(describing: error), privacy: .public)")
            reply(nil, nil, nil, nil, nil, nil, error)
            return
        }

        saveEgoKeyPair(signingKeyPair, identifier: signingKeyIdentifier(peerID: peerID)) { success, error in
            guard success else {
                logger.debug("Unable to save signing key: \(String(describing: error), privacy: .public)")
                reply(nil, nil, nil, nil, nil, nil, error ?? ContainerError.failedToStoreIdentity)
                return
            }
            saveEgoKeyPair(encryptionKeyPair, identifier: encryptionKeyIdentifier(peerID: peerID)) { success, error in
                guard success else {
                    logger.debug("Unable to save encryption key: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, nil, nil, nil, nil, error ?? ContainerError.failedToStoreIdentity)
                    return
                }

                let actualPolicyVersion = policyVersion ?? prevailingPolicyVersion
                self.fetchPolicyDocumentWithSemaphore(version: actualPolicyVersion) { policyDoc, policyFetchError in
                    guard let policyDoc = policyDoc, policyFetchError == nil else {
                        logger.debug("Unable to fetch policy: \(String(describing: policyFetchError), privacy: .public)")
                        reply(nil, nil, nil, nil, nil, nil, error ?? ContainerError.unknownInternalError)
                        return
                    }

                    if policyVersion != nil {
                        self.policyVersionOverride = policyDoc.version
                    }

                    // Save the prepared identity as containerMO.egoPeer* and its bottle
                    self.moc.performAndWait {
                        do {
                            // Note: the client chooses for syncUserControllableViews here.
                            // if they pass in UNKNOWN, we'll fix it later at join time, following the peers we trust.
                            let syncUserViews = syncUserControllableViews.sanitizeForPlatform(permanentInfo: permanentInfo)

                            let useFrozenPolicyVersion = policyDoc.version.versionNumber >= frozenPolicyVersion.versionNumber

                            let stableInfo = try TPPeerStableInfo(clock: 1,
                                                                  frozenPolicyVersion: useFrozenPolicyVersion ? frozenPolicyVersion : policyDoc.version,
                                                                  flexiblePolicyVersion: useFrozenPolicyVersion ? policyDoc.version : nil,
                                                                  policySecrets: policySecrets,
                                                                  syncUserControllableViews: syncUserViews,
                                                                  secureElementIdentity: secureElementIdentity,
                                                                  deviceName: deviceName,
                                                                  serialNumber: serialNumber,
                                                                  osVersion: osVersion,
                                                                  signing: signingKeyPair,
                                                                  recoverySigningPubKey: nil,
                                                                  recoveryEncryptionPubKey: nil,
                                                                  isInheritedAccount: false)
                            self.containerMO.egoPeerID = permanentInfo.peerID
                            self.containerMO.egoPeerPermanentInfo = permanentInfo.data
                            self.containerMO.egoPeerPermanentInfoSig = permanentInfo.sig
                            self.containerMO.egoPeerStableInfo = stableInfo.data
                            self.containerMO.egoPeerStableInfoSig = stableInfo.sig

                            let bottleMO = BottleMO(context: self.moc)
                            bottleMO.peerID = bottle.peerID
                            bottleMO.bottleID = bottle.bottleID
                            bottleMO.escrowedSigningSPKI = bottle.escrowSigningSPKI
                            bottleMO.signatureUsingEscrowKey = bottle.signatureUsingEscrowKey
                            bottleMO.signatureUsingPeerKey = bottle.signatureUsingPeerKey
                            bottleMO.contents = bottle.contents

                            self.containerMO.addToBottles(bottleMO)

                            let syncingPolicy = try self.syncingPolicyFor(modelID: permanentInfo.modelID, stableInfo: stableInfo)

                            try self.moc.save()

                            reply(permanentInfo.peerID, permanentInfo.data, permanentInfo.sig, stableInfo.data, stableInfo.sig, syncingPolicy, nil)
                        } catch {
                            logger.debug("Unable to save identity: \(String(describing: error), privacy: .public)")
                            reply(nil, nil, nil, nil, nil, nil, error)
                        }
                    }
                }
            }
        }
    }

    // policyVersion should only be non-nil for testing, to override prevailingPolicyVersion.versionNumber
    func prepareInheritancePeer(epoch: UInt64,
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
                                signingPrivateKeyPersistentRef: Data?,
                                encryptionPrivateKeyPersistentRef: Data?,
                                crk: TrustedPeersHelperCustodianRecoveryKey,
                                reply: @escaping (String?, Data?, Data?, Data?, Data?, TPSyncingPolicy?, String?, [CKRecord]?, Error?) -> Void) {
        self.semaphore.wait()
        let reply: (String?, Data?, Data?, Data?, Data?, TPSyncingPolicy?, String?, [CKRecord]?, Error?) -> Void = {
            logger.info("prepareInheritancePeer complete peerID: \(String(describing: $0), privacy: .public) \(traceError($8), privacy: .public)")
            self.semaphore.signal()
            reply($0, $1, $2, $3, $4, $5, $6, $7, $8)
        }

        self.fetchAndPersistChangesIfNeeded { error in
            guard error == nil else {
                logger.debug("prepareInheritancePeer unable to fetch changes: \(String(describing: error), privacy: .public)")
                reply(nil, nil, nil, nil, nil, nil, nil, nil, error)
                return
            }

            guard let uuid = UUID(uuidString: crk.uuid) else {
                logger.debug("Unable to parse uuid: \(crk.uuid, privacy: .public)")
                reply(nil, nil, nil, nil, nil, nil, nil, nil, ContainerError.recoveryKeysNotEnrolled)
                return
            }

            guard let tpcrk = self.model.findCustodianRecoveryKey(with: uuid) else {
                logger.debug("Unable to find custodian recovery key \(crk.uuid, privacy: .public) on model")
                reply(nil, nil, nil, nil, nil, nil, nil, nil, ContainerError.recoveryKeysNotEnrolled)
                return
            }

            guard self.model.isCustodianRecoveryKeyEnrolled(tpcrk.peerID) else {
                logger.debug("Custodian Recovery Key is not enrolled")
                reply(nil, nil, nil, nil, nil, nil, nil, nil, ContainerError.recoveryKeysNotEnrolled)
                return
            }

            // create inheritance recovery key set
            let permanentInfo: TPPeerPermanentInfo
            let signingKeyPair: _SFECKeyPair
            let encryptionKeyPair: _SFECKeyPair
            let recoveryCRK: CustodianRecoveryKey
            do {
                recoveryCRK = try CustodianRecoveryKey(tpCustodian: tpcrk, recoveryKeyString: crk.recoveryString, recoverySalt: crk.salt)

                signingKeyPair = recoveryCRK.peerKeys.signingKey
                encryptionKeyPair = recoveryCRK.peerKeys.encryptionKey
            } catch {
                logger.debug("failed to create custodian recovery keys: \(String(describing: error), privacy: .public)")
                reply(nil, nil, nil, nil, nil, nil, nil, nil, ContainerError.failedToCreateRecoveryKey)
                return
            }

            logger.debug("prepareInheritancePeer signingPubKey: \(recoveryCRK.peerKeys.signingKey.publicKey.keyData.base64EncodedString(), privacy: .public)")
            logger.debug("prepareInheritancePeer encryptionPubKey: \(recoveryCRK.peerKeys.encryptionKey.publicKey.keyData.base64EncodedString(), privacy: .public)")

            do {
                permanentInfo = try TPPeerPermanentInfo(machineID: machineID,
                                                        modelID: modelID,
                                                        epoch: 1,
                                                        signing: signingKeyPair,
                                                        encryptionKeyPair: encryptionKeyPair,
                                                        peerIDHashAlgo: TPHashAlgo.SHA256)
            } catch {
                reply(nil, nil, nil, nil, nil, nil, nil, nil, error)
                return
            }

            let peerID = permanentInfo.peerID

            saveEgoKeyPair(signingKeyPair, identifier: signingKeyIdentifier(peerID: peerID)) { success, error in
                guard success else {
                    logger.debug("Unable to save signing key: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, nil, nil, nil, nil, nil, nil, error ?? ContainerError.failedToStoreIdentity)
                    return
                }
                saveEgoKeyPair(encryptionKeyPair, identifier: encryptionKeyIdentifier(peerID: peerID)) { success, error in
                    guard success else {
                        logger.debug("Unable to save encryption key: \(String(describing: error), privacy: .public)")
                        reply(nil, nil, nil, nil, nil, nil, nil, nil, error ?? ContainerError.failedToStoreIdentity)
                        return
                    }

                    let actualPolicyVersion = policyVersion ?? prevailingPolicyVersion
                    self.fetchPolicyDocumentWithSemaphore(version: actualPolicyVersion) { policyDoc, policyFetchError in
                        guard let policyDoc = policyDoc, policyFetchError == nil else {
                            logger.debug("Unable to fetch policy: \(String(describing: policyFetchError), privacy: .public)")
                            reply(nil, nil, nil, nil, nil, nil, nil, nil, error ?? ContainerError.unknownInternalError)
                            return
                        }

                        if policyVersion != nil {
                            self.policyVersionOverride = policyDoc.version
                        }

                        // Save the prepared identity as containerMO.egoPeer*
                        self.moc.performAndWait {
                            do {
                                // Note: the client chooses for syncUserControllableViews here.
                                // if they pass in UNKNOWN, we'll fix it later at join time, following the peers we trust.
                                let syncUserViews = syncUserControllableViews.sanitizeForPlatform(permanentInfo: permanentInfo)

                                let useFrozenPolicyVersion = policyDoc.version.versionNumber >= frozenPolicyVersion.versionNumber

                                let stableInfo = try TPPeerStableInfo(clock: 1,
                                                                      frozenPolicyVersion: useFrozenPolicyVersion ? frozenPolicyVersion : policyDoc.version,
                                                                      flexiblePolicyVersion: useFrozenPolicyVersion ? policyDoc.version : nil,
                                                                      policySecrets: policySecrets,
                                                                      syncUserControllableViews: syncUserViews,
                                                                      secureElementIdentity: secureElementIdentity,
                                                                      deviceName: deviceName,
                                                                      serialNumber: serialNumber,
                                                                      osVersion: osVersion,
                                                                      signing: signingKeyPair,
                                                                      recoverySigningPubKey: nil,
                                                                      recoveryEncryptionPubKey: nil,
                                                                      isInheritedAccount: true)
                                self.containerMO.egoPeerID = permanentInfo.peerID
                                self.containerMO.egoPeerPermanentInfo = permanentInfo.data
                                self.containerMO.egoPeerPermanentInfoSig = permanentInfo.sig
                                self.containerMO.egoPeerStableInfo = stableInfo.data
                                self.containerMO.egoPeerStableInfoSig = stableInfo.sig

                                try self.moc.save()

                                guard let egoPeerID = self.containerMO.egoPeerID else {
                                    reply(nil, nil, nil, nil, nil, nil, nil, nil, ContainerError.noPreparedIdentity)
                                    return
                                }
                                guard let permanentInfo = self.containerMO.egoPeerPermanentInfo else {
                                    logger.debug("permanentInfo does not exist")
                                    reply(nil, nil, nil, nil, nil, nil, nil, nil, ContainerError.nonMember)
                                    return
                                }
                                guard let permanentInfoSig = self.containerMO.egoPeerPermanentInfoSig else {
                                    logger.debug("permanentInfoSig does not exist")
                                    reply(nil, nil, nil, nil, nil, nil, nil, nil, ContainerError.nonMember)
                                    return
                                }

                                guard let stableInfo = self.containerMO.egoPeerStableInfo else {
                                    logger.debug("stableInfo does not exist")
                                    reply(nil, nil, nil, nil, nil, nil, nil, nil, ContainerError.nonMember)
                                    return
                                }
                                guard let stableInfoSig = self.containerMO.egoPeerStableInfoSig else {
                                    logger.debug("stableInfoSig does not exist")
                                    reply(nil, nil, nil, nil, nil, nil, nil, nil, ContainerError.nonMember)
                                    return
                                }

                                let keyFactory = TPECPublicKeyFactory()
                                guard let selfPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: permanentInfo, sig: permanentInfoSig, keyFactory: keyFactory) else {
                                    reply(nil, nil, nil, nil, nil, nil, nil, nil, ContainerError.invalidPermanentInfoOrSig)
                                    return
                                }
                                guard let selfStableInfo = TPPeerStableInfo(data: stableInfo, sig: stableInfoSig) else {
                                    logger.debug("Invalid stableinfo or signature")
                                    reply(nil, nil, nil, nil, nil, nil, nil, nil, ContainerError.invalidStableInfoOrSig)
                                    return
                                }

                                do {
                                    let syncingPolicy = try self.syncingPolicyFor(modelID: selfPermanentInfo.modelID, stableInfo: selfStableInfo)

                                    let request = FetchRecoverableTLKSharesRequest.with {
                                        $0.peerID = recoveryCRK.peerID
                                    }

                                    self.cuttlefish.fetchRecoverableTlkshares(request) { response in
                                        switch response {
                                        case .failure(let error):
                                            logger.debug("fetchRecoverableTlkshares failed: \(String(describing: error), privacy: .public)")
                                            reply(nil, nil, nil, nil, nil, nil, nil, nil, error)
                                            return

                                        case .success(let response):
                                            let shareCount = response.views.map { $0.tlkShares.count }.reduce(0, +)
                                            logger.debug("fetchRecoverableTlkshares succeeded: found \(response.views.count) views and \(shareCount) total TLKShares")
                                            let records = response.views.flatMap { $0.ckrecords() }

                                            reply(selfPermanentInfo.peerID, selfPermanentInfo.data, selfPermanentInfo.sig, selfStableInfo.data, selfStableInfo.sig, syncingPolicy, recoveryCRK.peerID, records, nil)
                                            return
                                        }
                                    }
                                } catch {
                                    logger.debug("failed to create syncing policy: \(String(describing: error), privacy: .public)")
                                    reply(nil, nil, nil, nil, nil, nil, nil, nil, error)
                                }
                            } catch {
                                logger.debug("Unable to save inheritance identity: \(String(describing: error), privacy: .public)")
                                reply(nil, nil, nil, nil, nil, nil, nil, nil, error)
                            }
                        }
                    }
                }
            }
        }
    }

    func getEgoEpoch(reply: @escaping (UInt64, Error?) -> Void) {
        let reply: (UInt64, Error?) -> Void = {
            logger.info("getEgoEpoch complete: \($0) \(traceError($1), privacy: .public)")
            reply($0, $1)
        }

        self.moc.performAndWait {
            guard let egoPeerID = self.containerMO.egoPeerID else {
                reply(0, ContainerError.noPreparedIdentity)
                return
            }
            guard let egoPeer = self.model.peer(withID: egoPeerID) else {
                reply(0, ContainerError.noPreparedIdentity)
                return
            }

            reply(egoPeer.permanentInfo.epoch, nil)
        }
    }
    func establish(ckksKeys: [CKKSKeychainBackedKeySet],
                   tlkShares: [CKKSTLKShare],
                   preapprovedKeys: [Data]?,
                   reply: @escaping (String?, [CKRecord], TPSyncingPolicy?, Error?) -> Void) {
        self.semaphore.wait()
        let reply: (String?, [CKRecord], TPSyncingPolicy?, Error?) -> Void = {
            logger.info("establish complete peer: \(String(describing: $0), privacy: .public) \(traceError($3), privacy: .public)")
            self.semaphore.signal()
            reply($0, $1, $2, $3)
        }

        self.moc.performAndWait {
            self.onqueueEstablish(ckksKeys: ckksKeys,
                                  tlkShares: tlkShares,
                                  preapprovedKeys: preapprovedKeys) { peerID, ckrecords, syncingPolicy, error in
                                    reply(peerID, ckrecords, syncingPolicy, error)
            }
        }
    }

    func onqueueTTRUntrusted() {
        let ttr = SecTapToRadar(tapToRadar: "Device not IDMS trusted",
                                description: "Device not IDMS trusted",
                                radar: "52874119")
        ttr.trigger()
    }

    func fetchAfterEstablish(ckksKeys: [CKKSKeychainBackedKeySet],
                             tlkShares: [CKKSTLKShare],
                             reply: @escaping (String?, [CKRecord], TPSyncingPolicy?, Error?) -> Void) {
        self.moc.performAndWait {
            do {
                try self.deleteLocalCloudKitData()
            } catch {
                logger.debug("fetchAfterEstablish failed to reset local data: \(String(describing: error), privacy: .public)")
                reply(nil, [], nil, error)
                return
            }
            self.onqueueFetchAndPersistChanges { error in
                guard error == nil else {
                    logger.debug("fetchAfterEstablish failed to fetch changes: \(String(describing: error), privacy: .public)")
                    reply(nil, [], nil, error)
                    return
                }

                self.moc.performAndWait {
                    guard let egoPeerID = self.containerMO.egoPeerID,
                        let egoPermData = self.containerMO.egoPeerPermanentInfo,
                        let egoPermSig = self.containerMO.egoPeerPermanentInfoSig,
                        let egoStableData = self.containerMO.egoPeerStableInfo,
                        let egoStableSig = self.containerMO.egoPeerStableInfoSig
                        else {
                            logger.debug("fetchAfterEstablish: failed to fetch egoPeerID")
                            reply(nil, [], nil, ContainerError.noPreparedIdentity)
                            return
                    }
                    guard self.model.hasPeer(withID: egoPeerID) else {
                        logger.debug("fetchAfterEstablish: did not find peer \(egoPeerID, privacy: .public) in model")
                        reply(nil, [], nil, ContainerError.invalidPeerID)
                        return
                    }
                    let keyFactory = TPECPublicKeyFactory()
                    guard let selfPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: egoPermData, sig: egoPermSig, keyFactory: keyFactory) else {
                        reply(nil, [], nil, ContainerError.invalidPermanentInfoOrSig)
                        return
                    }
                    guard let selfStableInfo = TPPeerStableInfo(data: egoStableData, sig: egoStableSig) else {
                        logger.debug("cannot create TPPeerStableInfo")
                        reply(nil, [], nil, ContainerError.invalidStableInfoOrSig)
                        return
                    }
                    self.onqueueUpdateTLKs(ckksKeys: ckksKeys, tlkShares: tlkShares) { ckrecords, error in
                        guard error == nil else {
                            logger.debug("fetchAfterEstablish failed to update TLKs: \(String(describing: error), privacy: .public)")
                            reply(nil, [], nil, error)
                            return
                        }

                        do {
                            let syncingPolicy = try self.syncingPolicyFor(modelID: selfPermanentInfo.modelID,
                                                                          stableInfo: selfStableInfo)
                            logger.debug("fetchAfterEstablish succeeded")
                            reply(egoPeerID, ckrecords ?? [], syncingPolicy, nil)
                        } catch {
                            logger.debug("fetchAfterEstablish failed: \(String(describing: error), privacy: .public)")
                            reply(nil, [], nil, error)
                        }
                    }
                }
            }
        }
    }

    func onqueueEstablish(ckksKeys: [CKKSKeychainBackedKeySet],
                          tlkShares: [CKKSTLKShare],
                          preapprovedKeys: [Data]?,
                          reply: @escaping (String?, [CKRecord], TPSyncingPolicy?, Error?) -> Void) {
        // Fetch ego peer identity from local storage.
        guard let egoPeerID = self.containerMO.egoPeerID,
            let egoPermData = self.containerMO.egoPeerPermanentInfo,
            let egoPermSig = self.containerMO.egoPeerPermanentInfoSig,
            let egoStableData = self.containerMO.egoPeerStableInfo,
            let egoStableSig = self.containerMO.egoPeerStableInfoSig
            else {
                reply(nil, [], nil, ContainerError.noPreparedIdentity)
                return
        }

        let keyFactory = TPECPublicKeyFactory()
        guard let selfPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: egoPermData, sig: egoPermSig, keyFactory: keyFactory) else {
            reply(nil, [], nil, ContainerError.invalidPermanentInfoOrSig)
            return
        }
        guard let selfStableInfo = TPPeerStableInfo(data: egoStableData, sig: egoStableSig) else {
            logger.debug("cannot create TPPeerStableInfo")
            reply(nil, [], nil, ContainerError.invalidStableInfoOrSig)
            return
        }
        guard self.onqueueMachineIDAllowedByIDMS(machineID: selfPermanentInfo.machineID) else {
            logger.debug("establish: self machineID \(selfPermanentInfo.machineID, privacy: .public) not on list")
            self.onqueueTTRUntrusted()
            reply(nil, [], nil, ContainerError.preparedIdentityNotOnAllowedList(selfPermanentInfo.machineID))
            return
        }

        loadEgoKeys(peerID: egoPeerID) { egoPeerKeys, error in
            guard let egoPeerKeys = egoPeerKeys else {
                logger.debug("Don't have my own peer keys; can't establish: \(String(describing: error), privacy: .public)")
                reply(nil, [], nil, error)
                return
            }
            self.moc.performAndWait {
                let viewKeys: [ViewKeys] = ckksKeys.map(ViewKeys.convert)
                let allTLKShares: [TLKShare]
                do {
                    let octagonShares = try makeTLKShares(ckksTLKs: ckksKeys.map { $0.tlk }, asPeer: egoPeerKeys, toPeer: egoPeerKeys, epoch: Int(selfPermanentInfo.epoch))
                    let sosShares = tlkShares.map { TLKShare.convert(ckksTLKShare: $0) }

                    allTLKShares = octagonShares + sosShares
                } catch {
                    logger.debug("Unable to make TLKShares for self: \(String(describing: error), privacy: .public)")
                    reply(nil, [], nil, error)
                    return
                }

                let peer: Peer
                let newDynamicInfo: TPPeerDynamicInfo
                do {
                    (peer, newDynamicInfo) = try self.onqueuePreparePeerForJoining(egoPeerID: egoPeerID,
                                                                                   peerPermanentInfo: selfPermanentInfo,
                                                                                   stableInfo: selfStableInfo,
                                                                                   sponsorID: nil,
                                                                                   preapprovedKeys: preapprovedKeys,
                                                                                   vouchers: [],
                                                                                   egoPeerKeys: egoPeerKeys)

                    logger.debug("dynamic info: \(newDynamicInfo, privacy: .public)")
                } catch {
                    logger.debug("Unable to create peer for joining: \(String(describing: error), privacy: .public)")
                    reply(nil, [], nil, error)
                    return
                }

                guard let newPeerStableInfo = peer.stableInfoAndSig.toStableInfo() else {
                    logger.debug("Unable to create new peer stable info for joining")
                    reply(nil, [], nil, ContainerError.invalidStableInfoOrSig)
                    return
                }

                let bottle: Bottle
                do {
                    bottle = try self.assembleBottle(egoPeerID: egoPeerID)
                } catch {
                    reply(nil, [], nil, error)
                    return
                }
                logger.debug("Beginning establish for peer \(egoPeerID, privacy: .public)")
                logger.debug("Establish permanentInfo: \(egoPermData.base64EncodedString(), privacy: .public)")
                logger.debug("Establish permanentInfoSig: \(egoPermSig.base64EncodedString(), privacy: .public)")
                logger.debug("Establish stableInfo: \(egoStableData.base64EncodedString(), privacy: .public)")
                logger.debug("Establish stableInfoSig: \(egoStableSig.base64EncodedString(), privacy: .public)")
                logger.debug("Establish dynamicInfo: \(peer.dynamicInfoAndSig.peerDynamicInfo.base64EncodedString(), privacy: .public)")
                logger.debug("Establish dynamicInfoSig: \(peer.dynamicInfoAndSig.sig.base64EncodedString(), privacy: .public)")

                logger.debug("Establish introducing \(viewKeys.count) key sets, \(allTLKShares.count) tlk shares")

                do {
                    let bottleSerialized = try bottle.serializedData().base64EncodedString()
                    logger.debug("Establish bottle: \(bottleSerialized, privacy: .public)")
                    let peerSerialized = try peer.serializedData().base64EncodedString()
                    logger.debug("Establish peer: \(peerSerialized, privacy: .public)")
                } catch {
                    logger.debug("Establish unable to encode bottle/peer: \(String(describing: error), privacy: .public)")
                }

                let request = EstablishRequest.with {
                    $0.peer = peer
                    $0.bottle = bottle
                    $0.viewKeys = viewKeys
                    $0.tlkShares = allTLKShares
                }
                self.cuttlefish.establish(request) { response in
                    logger.debug("Establish: viewKeys: \(String(describing: viewKeys), privacy: .public)")

                    switch response {
                    case .success(let response):
                        do {
                            let responseChangesJson = try response.changes.jsonString()
                            logger.debug("Establish returned changes: \(responseChangesJson, privacy: .public)")
                        } catch {
                            logger.debug("Establish returned changes, but they can't be serialized")
                        }

                        let keyHierarchyRecords = response.zoneKeyHierarchyRecords.compactMap { CKRecord($0) }

                        do {
                            let syncingPolicy = try self.syncingPolicyFor(modelID: selfPermanentInfo.modelID,
                                                                          stableInfo: newPeerStableInfo)

                            try self.persist(changes: response.changes)

                            guard response.changes.more == false else {
                                logger.debug("establish succeeded, but more changes need fetching...")

                                self.fetchAndPersistChanges { fetchError in
                                    guard fetchError == nil else {
                                        // This is an odd error condition: we might be able to fetch again and be in a good state...
                                        logger.debug("fetch-after-establish failed: \(String(describing: fetchError), privacy: .public)")
                                        reply(nil, keyHierarchyRecords, nil, fetchError)
                                        return
                                    }

                                    logger.debug("fetch-after-establish succeeded")
                                    reply(egoPeerID, keyHierarchyRecords, syncingPolicy, nil)
                                }
                                return
                            }

                            logger.debug("establish succeeded")
                            reply(egoPeerID, keyHierarchyRecords, syncingPolicy, nil)
                        } catch {
                            logger.debug("establish handling failed: \(String(describing: error), privacy: .public)")
                            reply(nil, keyHierarchyRecords, nil, error)
                        }
                    case .failure(let error):
                        switch error {
                        case CuttlefishErrorMatcher(code: CuttlefishErrorCode.establishFailed):
                            logger.debug("establish returned failed, trying fetch")
                            self.fetchAfterEstablish(ckksKeys: ckksKeys, tlkShares: tlkShares, reply: reply)
                            return
                        default:
                            logger.debug("establish failed: \(String(describing: error), privacy: .public)")
                            reply(nil, [], nil, error)
                            return
                        }
                    }
                }
            }
        }
    }

    func setRecoveryKey(recoveryKey: String, salt: String, ckksKeys: [CKKSKeychainBackedKeySet], reply: @escaping ([CKRecord]?, Error?) -> Void) {
        self.semaphore.wait()
        let reply: ([CKRecord]?, Error?) -> Void = {
            logger.info("setRecoveryKey complete: \(traceError($1), privacy: .public)")
            self.semaphore.signal()
            reply($0, $1)
        }

        logger.debug("beginning a setRecoveryKey")

        self.moc.performAndWait {
            guard let egoPeerID = self.containerMO.egoPeerID else {
                logger.debug("no prepared identity, cannot set recovery key")
                reply(nil, ContainerError.noPreparedIdentity)
                return
            }

            var recoveryKeys: RecoveryKey
            do {
                recoveryKeys = try RecoveryKey(recoveryKeyString: recoveryKey, recoverySalt: salt)
            } catch {
                logger.debug("failed to create recovery keys: \(String(describing: error), privacy: .public)")
                reply(nil, ContainerError.failedToCreateRecoveryKey)
                return
            }

            let signingPublicKey: Data = recoveryKeys.peerKeys.signingVerificationKey.keyData
            let encryptionPublicKey: Data = recoveryKeys.peerKeys.encryptionVerificationKey.keyData

            logger.debug("setRecoveryKey signingPubKey: \(signingPublicKey.base64EncodedString(), privacy: .public)")
            logger.debug("setRecoveryKey encryptionPubKey: \(encryptionPublicKey.base64EncodedString(), privacy: .public)")

            guard let stableInfoData = self.containerMO.egoPeerStableInfo else {
                logger.debug("stableInfo does not exist")
                reply(nil, ContainerError.nonMember)
                return
            }
            guard let stableInfoSig = self.containerMO.egoPeerStableInfoSig else {
                logger.debug("stableInfoSig does not exist")
                reply(nil, ContainerError.nonMember)
                return
            }
            guard let permInfoData = self.containerMO.egoPeerPermanentInfo else {
                logger.debug("permanentInfo does not exist")
                reply(nil, ContainerError.nonMember)
                return
            }
            guard let permInfoSig = self.containerMO.egoPeerPermanentInfoSig else {
                logger.debug("permInfoSig does not exist")
                reply(nil, ContainerError.nonMember)
                return
            }
            guard let stableInfo = TPPeerStableInfo(data: stableInfoData, sig: stableInfoSig) else {
                logger.debug("cannot create TPPeerStableInfo")
                reply(nil, ContainerError.invalidStableInfoOrSig)
                return
            }
            let keyFactory = TPECPublicKeyFactory()
            guard let permanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: permInfoData, sig: permInfoSig, keyFactory: keyFactory) else {
                logger.debug("cannot create TPPeerPermanentInfo")
                reply(nil, ContainerError.invalidStableInfoOrSig)
                return
            }

            loadEgoKeyPair(identifier: signingKeyIdentifier(peerID: egoPeerID)) { signingKeyPair, error in
                guard let signingKeyPair = signingKeyPair else {
                    logger.debug("handle: no signing key pair: \(String(describing: error), privacy: .public)")
                    reply(nil, error)
                    return
                }
                self.moc.performAndWait {
                    do {
                        let tlkShares = try makeTLKShares(ckksTLKs: ckksKeys.map { $0.tlk },
                                                          asPeer: recoveryKeys.peerKeys,
                                                          toPeer: recoveryKeys.peerKeys,
                                                          epoch: Int(permanentInfo.epoch))

                        let policyVersion = stableInfo.bestPolicyVersion()
                        let policyDoc = try self.getPolicyDoc(policyVersion.versionNumber)

                        let updatedStableInfo = try TPPeerStableInfo(clock: stableInfo.clock + 1,
                                                                     frozenPolicyVersion: frozenPolicyVersion,
                                                                     flexiblePolicyVersion: policyDoc.version,
                                                                     policySecrets: stableInfo.policySecrets,
                                                                     syncUserControllableViews: stableInfo.syncUserControllableViews,
                                                                     secureElementIdentity: stableInfo.secureElementIdentity,
                                                                     deviceName: stableInfo.deviceName,
                                                                     serialNumber: stableInfo.serialNumber,
                                                                     osVersion: stableInfo.osVersion,
                                                                     signing: signingKeyPair,
                                                                     recoverySigningPubKey: signingPublicKey,
                                                                     recoveryEncryptionPubKey: encryptionPublicKey,
                                                                     isInheritedAccount: stableInfo.isInheritedAccount)
                        let signedStableInfo = SignedPeerStableInfo(updatedStableInfo)

                        let request = SetRecoveryKeyRequest.with {
                            $0.peerID = egoPeerID
                            $0.recoverySigningPubKey = signingPublicKey
                            $0.recoveryEncryptionPubKey = encryptionPublicKey
                            $0.stableInfoAndSig = signedStableInfo
                            $0.tlkShares = tlkShares
                            $0.changeToken = self.containerMO.changeToken ?? ""
                        }

                        self.cuttlefish.setRecoveryKey(request) { response in
                            switch response {
                            case .success(let response):
                                self.moc.performAndWait {
                                    do {
                                        self.containerMO.egoPeerStableInfo = updatedStableInfo.data
                                        self.containerMO.egoPeerStableInfoSig = updatedStableInfo.sig
                                        try self.onQueuePersist(changes: response.changes)

                                        logger.debug("setRecoveryKey succeeded")

                                        let keyHierarchyRecords = response.zoneKeyHierarchyRecords.compactMap { CKRecord($0) }
                                        reply(keyHierarchyRecords, nil)
                                    } catch {
                                        logger.debug("setRecoveryKey handling failed: \(String(describing: error), privacy: .public)")
                                        reply(nil, error)
                                    }
                                }
                            case .failure(let error):
                                logger.debug("setRecoveryKey failed: \(String(describing: error), privacy: .public)")
                                reply(nil, error)
                                return
                            }
                        }
                    } catch {
                        reply(nil, error)
                    }
                }
            }
        }
    }

    func createCustodianRecoveryKey(recoveryKey: String,
                                    salt: String,
                                    ckksKeys: [CKKSKeychainBackedKeySet],
                                    uuid: UUID,
                                    kind: TPPBCustodianRecoveryKey_Kind,
                                    reply: @escaping ([CKRecord]?, TrustedPeersHelperCustodianRecoveryKey?, Error?) -> Void) {
        self.semaphore.wait()
        let reply: ([CKRecord]?, TrustedPeersHelperCustodianRecoveryKey?, Error?) -> Void = {
            logger.info("createCustodianRecoveryKey complete: \(traceError($2), privacy: .public)")
            self.semaphore.signal()
            reply($0, $1, $2)
        }

        logger.debug("beginning a createCustodianRecoveryKey")

        self.moc.performAndWait {
            guard let egoPeerID = self.containerMO.egoPeerID else {
                logger.debug("no prepared identity, cannot create custodian recovery key")
                reply(nil, nil, ContainerError.noPreparedIdentity)
                return
            }

            let crk: CustodianRecoveryKey
            do {
                crk = try CustodianRecoveryKey(uuid: uuid, recoveryKeyString: recoveryKey, recoverySalt: salt, kind: kind)
            } catch {
                logger.debug("failed to create custodian recovery keys: \(String(describing: error), privacy: .public)")
                reply(nil, nil, ContainerError.failedToCreateRecoveryKey)
                return
            }

            logger.debug("createCustodianRecoveryKey signingPubKey: \(crk.tpCustodian.signingPublicKey.spki().base64EncodedString(), privacy: .public)")
            logger.debug("createCustodianRecoveryKey encryptionPubKey: \(crk.tpCustodian.encryptionPublicKey.spki().base64EncodedString(), privacy: .public)")

            guard let permInfoData = self.containerMO.egoPeerPermanentInfo else {
                logger.debug("permanentInfo does not exist")
                reply(nil, nil, ContainerError.nonMember)
                return
            }
            guard let permInfoSig = self.containerMO.egoPeerPermanentInfoSig else {
                logger.debug("permInfoSig does not exist")
                reply(nil, nil, ContainerError.nonMember)
                return
            }
            let keyFactory = TPECPublicKeyFactory()
            guard let permanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: permInfoData, sig: permInfoSig, keyFactory: keyFactory) else {
                logger.debug("cannot create TPPeerPermanentInfo")
                reply(nil, nil, ContainerError.invalidPermanentInfoOrSig)
                return
            }
            guard let stableInfoData = self.containerMO.egoPeerStableInfo else {
                logger.debug("stableInfo does not exist")
                reply(nil, nil, ContainerError.invalidStableInfoOrSig)
                return
            }
            guard let stableInfoSig = self.containerMO.egoPeerStableInfoSig else {
                logger.debug("stableInfoSig does not exist")
                reply(nil, nil, ContainerError.invalidStableInfoOrSig)
                return
            }
            guard let stableInfo = TPPeerStableInfo(data: stableInfoData, sig: stableInfoSig) else {
                logger.debug("cannot create TPPeerStableInfo")
                reply(nil, nil, ContainerError.invalidStableInfoOrSig)
                return
            }
            loadEgoKeyPair(identifier: signingKeyIdentifier(peerID: egoPeerID)) { signingKeyPair, error in
                guard let signingKeyPair = signingKeyPair else {
                    logger.debug("handle: no signing key pair: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, error)
                    return
                }
                self.moc.performAndWait {
                    do {
                        let crkViews = try self.model.getViewsForCRK(crk.tpCustodian, donorPermanentInfo: permanentInfo, donorStableInfo: stableInfo)
                        let ckksTLKs = ckksKeys.filter { crkViews.contains($0.tlk.zoneID.zoneName) }.map { $0.tlk }
                        let tlkShares = try makeTLKShares(ckksTLKs: ckksTLKs,
                                                          asPeer: crk.peerKeys,
                                                          toPeer: crk.peerKeys,
                                                          epoch: Int(permanentInfo.epoch))

                        let signedCrk = SignedCustodianRecoveryKey(crk)
                        let crkPeerID = crk.tpCustodian.peerID

                        let dynamicInfo: TPPeerDynamicInfo
                        do {
                            dynamicInfo = try self.model.calculateDynamicInfoForPeer(withID: egoPeerID,
                                                                                     addingPeerIDs: [crkPeerID],
                                                                                     removingPeerIDs: nil,
                                                                                     preapprovedKeys: nil,
                                                                                     signing: signingKeyPair,
                                                                                     currentMachineIDs: self.onqueueCurrentMIDList())
                        } catch {
                            logger.debug("Error preparing dynamic info: \(String(describing: error), privacy: .public)")
                            reply(nil, nil, error)
                            return
                        }

                        let signedDynamicInfo = SignedPeerDynamicInfo(dynamicInfo)

                        let request = AddCustodianRecoveryKeyRequest.with {
                            $0.peerID = egoPeerID
                            $0.peer = Peer.with {
                                $0.peerID = crkPeerID
                                $0.custodianRecoveryKeyAndSig = signedCrk
                            }
                            $0.tlkShares = tlkShares
                            $0.dynamicInfoAndSig = signedDynamicInfo
                            $0.changeToken = self.containerMO.changeToken ?? ""
                        }

                        self.cuttlefish.addCustodianRecoveryKey(request) { response in
                            switch response {
                            case .success(let response):
                                self.moc.performAndWait {
                                    do {
                                        try self.onQueuePersist(changes: response.changes)

                                        logger.debug("CreateCustodianRecoveryKey succeeded")

                                        let keyHierarchyRecords = response.zoneKeyHierarchyRecords.compactMap { CKRecord($0) }
                                        let replyCrk = TrustedPeersHelperCustodianRecoveryKey(uuid: uuid.uuidString,
                                                                                              encryptionKey: crk.tpCustodian.encryptionPublicKey.spki(),
                                                                                              signingKey: crk.tpCustodian.signingPublicKey.spki(),
                                                                                              recoveryString: recoveryKey,
                                                                                              salt: salt,
                                                                                              kind: kind)
                                        reply(keyHierarchyRecords, replyCrk, nil)
                                        return
                                    } catch {
                                        logger.debug("CreateCustodianRecoveryKey handling failed: \(String(describing: error), privacy: .public)")
                                        reply(nil, nil, error)
                                        return
                                    }
                                }
                            case .failure(let error):
                                logger.debug("CreateCustodianRecoveryKey failed: \(String(describing: error), privacy: .public)")
                                reply(nil, nil, error)
                                return
                            }
                        }
                    } catch {
                        reply(nil, nil, error)
                    }
                }
            }
        }
    }

    func removeCustodianRecoveryKey(uuid: UUID,
                                    reply: @escaping (Error?) -> Void) {
        self.semaphore.wait()
        let reply: (Error?) -> Void = {
            logger.info("removeCustodianRecoveryKey complete: \(traceError($0), privacy: .public)")
            self.semaphore.signal()
            reply($0)
        }

        self.moc.performAndWait {
            guard let egoPeerID = self.containerMO.egoPeerID else {
                logger.debug("No dynamic info for self?")
                reply(ContainerError.noPreparedIdentity)
                return
            }

            guard let tpcrk = self.model.findCustodianRecoveryKey(with: uuid) else {
                logger.debug("Unable to find custodian recovery key \(uuid, privacy: .public) on model")
                reply(ContainerError.recoveryKeysNotEnrolled)
                return
            }

            let crkPeerID = tpcrk.peerID

            // This would be a strange case (where we confuse the ego peerID with the CRK peerID), but it's clearly an error.
            guard crkPeerID != egoPeerID else {
                logger.debug("Self-distrust via peerID not allowed")
                reply(ContainerError.invalidPeerID)
                return
            }

            self.onqueueDistrust(peerIDs: [crkPeerID]) { error in
                if error == nil {
                    self.moc.performAndWait {
                        self.model.removeCustodianRecoveryKey(crkPeerID)
                        let crkMO = CustodianRecoveryKeyMO(context: self.moc)
                        crkMO.crkInfo = tpcrk.data
                        crkMO.crkInfoSig = tpcrk.sig
                        self.containerMO.removeFromCustodianRecoveryKeys([crkMO] as NSSet)
                    }
                }
                reply(error)
            }
        }
    }

    func vouchWithBottle(bottleID: String,
                         entropy: Data,
                         bottleSalt: String,
                         tlkShares: [CKKSTLKShare],
                         reply: @escaping (Data?, Data?, [CKKSTLKShare]?, TrustedPeersHelperTLKRecoveryResult?, Error?) -> Void) {
        self.semaphore.wait()
        let reply: (Data?, Data?, [CKKSTLKShare]?, TrustedPeersHelperTLKRecoveryResult?, Error?) -> Void = {
            logger.info("vouchWithBottle complete: \(traceError($4), privacy: .public)")
            self.semaphore.signal()
            reply($0, $1, $2, $3, $4)
        }

        // A preflight should have been successful before calling this function. So, we can assume that all required data is stored locally.

        self.moc.performAndWait {
            let bmo: BottleMO

            do {
                (bmo, _, _) = try self.onMOCQueuePerformPreflight(bottleID: bottleID)
            } catch {
                logger.debug("vouchWithBottle failed preflight: \(String(describing: error), privacy: .public)")
                reply(nil, nil, nil, nil, error)
                return
            }

            guard let bottledContents = bmo.contents else {
                reply(nil, nil, nil, nil, ContainerError.bottleDoesNotContainContents)
                return
            }
            guard let signatureUsingEscrowKey = bmo.signatureUsingEscrowKey else {
                reply(nil, nil, nil, nil, ContainerError.bottleDoesNotContainEscrowKeySignature)
                return
            }

            guard let signatureUsingPeerKey = bmo.signatureUsingPeerKey else {
                reply(nil, nil, nil, nil, ContainerError.bottleDoesNotContainerPeerKeySignature)
                return
            }
            guard let sponsorPeerID = bmo.peerID else {
                reply(nil, nil, nil, nil, ContainerError.bottleDoesNotContainPeerID)
                return
            }

            // verify bottle signature using peer
            do {
                guard let sponsorPeer = self.model.peer(withID: sponsorPeerID) else {
                    logger.debug("vouchWithBottle: Unable to find peer that created the bottle")
                    reply(nil, nil, nil, nil, ContainerError.bottleCreatingPeerNotFound)
                    return
                }
                guard let signingKey: _SFECPublicKey = sponsorPeer.permanentInfo.signingPubKey as? _SFECPublicKey else {
                    logger.debug("vouchWithBottle: Unable to create a sponsor public key")
                    reply(nil, nil, nil, nil, ContainerError.signatureVerificationFailed)
                    return
                }

                _ = try BottledPeer.verifyBottleSignature(data: bottledContents, signature: signatureUsingPeerKey, pubKey: signingKey)
            } catch {
                logger.debug("vouchWithBottle: Verification of bottled signature failed: \(String(describing: error), privacy: .public)")
                reply(nil, nil, nil, nil, ContainerError.failedToCreateBottledPeer)
                return
            }

            // create bottled peer
            let bottledPeer: BottledPeer
            do {
                bottledPeer = try BottledPeer(contents: bottledContents,
                                              secret: entropy,
                                              bottleSalt: bottleSalt,
                                              signatureUsingEscrow: signatureUsingEscrowKey,
                                              signatureUsingPeerKey: signatureUsingPeerKey)
            } catch {
                logger.debug("Creation of Bottled Peer failed with bottle salt: \(bottleSalt, privacy: .public)\nAttempting with empty bottle salt")

                do {
                    bottledPeer = try BottledPeer(contents: bottledContents,
                                                  secret: entropy,
                                                  bottleSalt: "",
                                                  signatureUsingEscrow: signatureUsingEscrowKey,
                                                  signatureUsingPeerKey: signatureUsingPeerKey)
                } catch {
                    logger.debug("Creation of Bottled Peer failed: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, nil, nil, ContainerError.failedToCreateBottledPeer)
                    return
                }
            }

            logger.debug("Have a bottle for peer \(bottledPeer.peerID, privacy: .public)")

            // Extract any TLKs we have been given
            let (recoveredTLKs, recoveryResult) = extract(tlkShares: tlkShares, peer: bottledPeer.peerKeys, model: self.model)

            self.moc.performAndWait {
                // I must have an ego identity in order to vouch using bottle
                guard let egoPeerID = self.containerMO.egoPeerID else {
                    logger.debug("As a nonmember, can't vouch for someone else")
                    reply(nil, nil, nil, nil, ContainerError.nonMember)
                    return
                }
                guard let permanentInfo = self.containerMO.egoPeerPermanentInfo else {
                    logger.debug("permanentInfo does not exist")
                    reply(nil, nil, nil, nil, ContainerError.nonMember)
                    return
                }
                guard let permanentInfoSig = self.containerMO.egoPeerPermanentInfoSig else {
                    logger.debug("permanentInfoSig does not exist")
                    reply(nil, nil, nil, nil, ContainerError.nonMember)
                    return
                }
                guard let stableInfo = self.containerMO.egoPeerStableInfo else {
                    logger.debug("stableInfo does not exist")
                    reply(nil, nil, nil, nil, ContainerError.nonMember)
                    return
                }
                guard let stableInfoSig = self.containerMO.egoPeerStableInfoSig else {
                    logger.debug("stableInfoSig does not exist")
                    reply(nil, nil, nil, nil, ContainerError.nonMember)
                    return
                }
                let keyFactory = TPECPublicKeyFactory()
                guard let beneficiaryPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: permanentInfo, sig: permanentInfoSig, keyFactory: keyFactory) else {
                    logger.debug("Invalid permenent info or signature; can't vouch for them")
                    reply(nil, nil, nil, nil, ContainerError.invalidPermanentInfoOrSig)
                    return
                }
                guard let beneficiaryStableInfo = TPPeerStableInfo(data: stableInfo, sig: stableInfoSig) else {
                    logger.debug("Invalid stableinfo or signature; van't vouch for them")
                    reply(nil, nil, nil, nil, ContainerError.invalidStableInfoOrSig)
                    return
                }
                loadEgoKeys(peerID: egoPeerID) { egoPeerKeys, error in
                    guard let egoPeerKeys = egoPeerKeys else {
                        logger.debug("Error loading ego peer keys: \(String(describing: error), privacy: .public)")
                        reply(nil, nil, nil, nil, error)
                        return
                    }

                    do {
                        let voucher = try self.model.createVoucher(forCandidate: beneficiaryPermanentInfo,
                                                                   stableInfo: beneficiaryStableInfo,
                                                                   withSponsorID: sponsorPeerID,
                                                                   reason: TPVoucherReason.restore,
                                                                   signing: bottledPeer.peerKeys.signingKey)

                        let newSelfTLKShares = try makeCKKSTLKShares(ckksTLKs: recoveredTLKs, asPeer: egoPeerKeys, toPeer: egoPeerKeys, epoch: Int(beneficiaryPermanentInfo.epoch))

                        reply(voucher.data, voucher.sig, newSelfTLKShares, recoveryResult, nil)
                        return
                    } catch {
                        logger.debug("Error creating voucher with bottle: \(String(describing: error), privacy: .public)")
                        reply(nil, nil, nil, nil, error)
                        return
                    }
                }
            }
        }
    }

    func vouchWithRecoveryKey(recoveryKey: String,
                              salt: String,
                              tlkShares: [CKKSTLKShare],
                              reply: @escaping (Data?, Data?, [CKKSTLKShare]?, TrustedPeersHelperTLKRecoveryResult?, Error?) -> Void) {
        self.semaphore.wait()
        let reply: (Data?, Data?, [CKKSTLKShare]?, TrustedPeersHelperTLKRecoveryResult?, Error?) -> Void = {
            logger.info("vouchWithRecoveryKey complete: \(traceError($4), privacy: .public)")
            self.semaphore.signal()
            reply($0, $1, $2, $3, $4)
        }

        self.moc.performAndWait {
            logger.debug("beginning a vouchWithRecoveryKey")

            // I must have an ego identity in order to vouch using bottle
            guard let egoPeerID = self.containerMO.egoPeerID else {
                logger.debug("As a nonmember, can't vouch for someone else")
                reply(nil, nil, nil, nil, ContainerError.nonMember)
                return
            }
            guard let permanentInfo = self.containerMO.egoPeerPermanentInfo else {
                logger.debug("permanentInfo does not exist")
                reply(nil, nil, nil, nil, ContainerError.nonMember)
                return
            }
            guard let permanentInfoSig = self.containerMO.egoPeerPermanentInfoSig else {
                logger.debug("permanentInfoSig does not exist")
                reply(nil, nil, nil, nil, ContainerError.nonMember)
                return
            }
            guard let stableInfo = self.containerMO.egoPeerStableInfo else {
                logger.debug("stableInfo does not exist")
                reply(nil, nil, nil, nil, ContainerError.nonMember)
                return
            }
            guard let stableInfoSig = self.containerMO.egoPeerStableInfoSig else {
                logger.debug("stableInfoSig does not exist")
                reply(nil, nil, nil, nil, ContainerError.nonMember)
                return
            }
            let keyFactory = TPECPublicKeyFactory()
            guard let beneficiaryPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: permanentInfo, sig: permanentInfoSig, keyFactory: keyFactory) else {
                logger.debug("Invalid permenent info or signature; can't vouch for them")
                reply(nil, nil, nil, nil, ContainerError.invalidPermanentInfoOrSig)
                return
            }
            guard let beneficiaryStableInfo = TPPeerStableInfo(data: stableInfo, sig: stableInfoSig) else {
                logger.debug("Invalid stableinfo or signature; van't vouch for them")
                reply(nil, nil, nil, nil, ContainerError.invalidStableInfoOrSig)
                return
            }

            loadEgoKeys(peerID: egoPeerID) { egoPeerKeys, error in
                guard let egoPeerKeys = egoPeerKeys else {
                    logger.debug("Don't have my own peer keys; can't establish: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, nil, nil, error)
                    return
                }

                // create recovery key set
                var recoveryKeys: RecoveryKey
                do {
                    recoveryKeys = try RecoveryKey(recoveryKeyString: recoveryKey, recoverySalt: salt)
                } catch {
                    logger.debug("failed to create recovery keys: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, nil, nil, ContainerError.failedToCreateRecoveryKey)
                    return
                }

                let signingPublicKey: Data = recoveryKeys.peerKeys.signingKey.publicKey.keyData
                let encryptionPublicKey: Data = recoveryKeys.peerKeys.encryptionKey.publicKey.keyData

                logger.debug("vouchWithRecoveryKey signingPubKey: \(signingPublicKey.base64EncodedString(), privacy: .public)")
                logger.debug("vouchWithRecoveryKey encryptionPubKey: \(encryptionPublicKey.base64EncodedString(), privacy: .public)")

                guard self.model.isRecoveryKeyEnrolled() else {
                    logger.debug("Recovery Key is not enrolled")
                    reply(nil, nil, nil, nil, ContainerError.recoveryKeysNotEnrolled)
                    return
                }

                // find matching peer containing recovery keys
                guard let sponsorPeerID = self.model.peerIDThatTrustsRecoveryKeys(TPRecoveryKeyPair(signingKeyData: signingPublicKey,
                                                                                                    encryptionKeyData: encryptionPublicKey),
                                                                                  canIntroducePeer: beneficiaryPermanentInfo,
                                                                                  stableInfo: beneficiaryStableInfo) else {
                    logger.debug("Untrusted recovery key set")
                    reply(nil, nil, nil, nil, ContainerError.untrustedRecoveryKeys)
                    return
                }

                // We're going to end up trusting every peer that the sponsor peer trusts.
                // We might as well trust all TLKShares from those peers at this point.
                let (recoveredTLKs, tlkRecoverResult) = extract(tlkShares: tlkShares, peer: recoveryKeys.peerKeys, sponsorPeerID: sponsorPeerID, model: self.model)

                do {
                    let voucher = try self.model.createVoucher(forCandidate: beneficiaryPermanentInfo,
                                                               stableInfo: beneficiaryStableInfo,
                                                               withSponsorID: sponsorPeerID,
                                                               reason: TPVoucherReason.recoveryKey,
                                                               signing: recoveryKeys.peerKeys.signingKey)

                    let newSelfTLKShares = try makeCKKSTLKShares(ckksTLKs: recoveredTLKs, asPeer: egoPeerKeys, toPeer: egoPeerKeys, epoch: Int(beneficiaryPermanentInfo.epoch))
                    reply(voucher.data, voucher.sig, newSelfTLKShares, tlkRecoverResult, nil)
                    return
                } catch {
                    logger.debug("Error creating voucher using recovery key set: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, nil, nil, error)
                    return
                }
            }
        }
    }

    func recoverTLKSharesForInheritor(crk: TrustedPeersHelperCustodianRecoveryKey,
                                      tlkShares: [CKKSTLKShare],
                                      reply: @escaping ([CKKSTLKShare]?, TrustedPeersHelperTLKRecoveryResult?, Error?) -> Void) {
        self.semaphore.wait()
        let reply: ([CKKSTLKShare]?, TrustedPeersHelperTLKRecoveryResult?, Error?) -> Void = {
            logger.info("recoverTLKSharesForInheritor complete: \(traceError($2), privacy: .public)")
            self.semaphore.signal()
            reply($0, $1, $2)
        }

        self.moc.performAndWait {
            logger.debug("beginning a recoverTLKSharesForInheritor")

                // I must have an ego identity in order to vouch using bottle
            guard let egoPeerID = self.containerMO.egoPeerID else {
                logger.debug("As a nonmember, can't vouch for someone else")
                reply(nil, nil, ContainerError.nonMember)
                return
            }
            guard let permanentInfo = self.containerMO.egoPeerPermanentInfo else {
                logger.debug("permanentInfo does not exist")
                reply(nil, nil, ContainerError.nonMember)
                return
            }
            guard let permanentInfoSig = self.containerMO.egoPeerPermanentInfoSig else {
                logger.debug("permanentInfoSig does not exist")
                reply(nil, nil, ContainerError.nonMember)
                return
            }

            let keyFactory = TPECPublicKeyFactory()
            guard let beneficiaryPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: permanentInfo, sig: permanentInfoSig, keyFactory: keyFactory) else {
                logger.debug("Invalid permenent info or signature; can't vouch for them")
                reply(nil, nil, ContainerError.invalidPermanentInfoOrSig)
                return
            }

            loadEgoKeys(peerID: egoPeerID) { egoPeerKeys, error in
                guard let egoPeerKeys = egoPeerKeys else {
                    logger.debug("Don't have my own peer keys; can't establish: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, error)
                    return
                }

                guard let uuid = UUID(uuidString: crk.uuid) else {
                    logger.debug("Unable to parse uuid: \(crk.uuid, privacy: .public)")
                    reply(nil, nil, ContainerError.recoveryKeysNotEnrolled)
                    return
                }

                guard let tpcrk = self.model.findCustodianRecoveryKey(with: uuid) else {
                    logger.debug("Unable to find custodian recovery key \(crk.uuid, privacy: .public) on model")
                    reply(nil, nil, ContainerError.recoveryKeysNotEnrolled)
                    return
                }

                guard self.model.isCustodianRecoveryKeyEnrolled(tpcrk.peerID) else {
                    logger.debug("Custodian Recovery Key is not enrolled")
                    reply(nil, nil, ContainerError.recoveryKeysNotEnrolled)
                    return
                }

                // create recovery key set
                let recoveryCRK: CustodianRecoveryKey
                do {
                    recoveryCRK = try CustodianRecoveryKey(tpCustodian: tpcrk, recoveryKeyString: crk.recoveryString, recoverySalt: crk.salt)
                } catch {
                    logger.debug("failed to create custodian recovery keys: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, ContainerError.failedToCreateRecoveryKey)
                    return
                }

                logger.debug("recoverTLKSharesForInheritor signingPubKey: \(recoveryCRK.peerKeys.signingKey.publicKey.keyData.base64EncodedString(), privacy: .public)")
                logger.debug("recoverTLKSharesForInheritor encryptionPubKey: \(recoveryCRK.peerKeys.encryptionKey.publicKey.keyData.base64EncodedString(), privacy: .public)")

                // find matching peer trusting custodian recovery keys
                guard let sponsorPeerID = self.model.peerIDThatTrustsCustodianRecoveryKeys(tpcrk,
                                                                                           canIntroducePeer: beneficiaryPermanentInfo,
                                                                                           stableInfo: nil) else {
                    logger.debug("Untrusted custodian recovery key set")
                    reply(nil, nil, ContainerError.untrustedRecoveryKeys)
                    return
                }

                let (recoveredTLKs, tlkRecoveryResult) = extract(tlkShares: tlkShares, peer: recoveryCRK.peerKeys, sponsorPeerID: sponsorPeerID, model: self.model)

                do {
                    let newSelfTLKShares = try makeCKKSTLKShares(ckksTLKs: recoveredTLKs, asPeer: egoPeerKeys, toPeer: egoPeerKeys, epoch: Int(beneficiaryPermanentInfo.epoch))
                    reply(newSelfTLKShares, tlkRecoveryResult, nil)
                    return
                } catch {
                    logger.debug("Error making CKKSTLKShares for inheritor: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, error)
                    return
                }
            }
        }
    }

    func vouchWithCustodianRecoveryKey(crk: TrustedPeersHelperCustodianRecoveryKey,
                                       tlkShares: [CKKSTLKShare],
                                       reply: @escaping (Data?, Data?, [CKKSTLKShare]?, TrustedPeersHelperTLKRecoveryResult?, Error?) -> Void) {
        self.semaphore.wait()
        let reply: (Data?, Data?, [CKKSTLKShare]?, TrustedPeersHelperTLKRecoveryResult?, Error?) -> Void = {
            logger.info("vouchWithCustodianRecoveryKey complete: \(traceError($4), privacy: .public)")
            self.semaphore.signal()
            reply($0, $1, $2, $3, $4)
        }

        self.moc.performAndWait {
            logger.debug("beginning a vouchWithCustodianRecoveryKey")

            // I must have an ego identity in order to vouch using bottle
            guard let egoPeerID = self.containerMO.egoPeerID else {
                logger.debug("As a nonmember, can't vouch for someone else")
                reply(nil, nil, nil, nil, ContainerError.nonMember)
                return
            }
            guard let permanentInfo = self.containerMO.egoPeerPermanentInfo else {
                logger.debug("permanentInfo does not exist")
                reply(nil, nil, nil, nil, ContainerError.nonMember)
                return
            }
            guard let permanentInfoSig = self.containerMO.egoPeerPermanentInfoSig else {
                logger.debug("permanentInfoSig does not exist")
                reply(nil, nil, nil, nil, ContainerError.nonMember)
                return
            }
            guard let stableInfo = self.containerMO.egoPeerStableInfo else {
                logger.debug("stableInfo does not exist")
                reply(nil, nil, nil, nil, ContainerError.nonMember)
                return
            }
            guard let stableInfoSig = self.containerMO.egoPeerStableInfoSig else {
                logger.debug("stableInfoSig does not exist")
                reply(nil, nil, nil, nil, ContainerError.nonMember)
                return
            }
            let keyFactory = TPECPublicKeyFactory()
            guard let beneficiaryPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: permanentInfo, sig: permanentInfoSig, keyFactory: keyFactory) else {
                logger.debug("Invalid permenent info or signature; can't vouch for them")
                reply(nil, nil, nil, nil, ContainerError.invalidPermanentInfoOrSig)
                return
            }
            guard let beneficiaryStableInfo = TPPeerStableInfo(data: stableInfo, sig: stableInfoSig) else {
                logger.debug("Invalid stableinfo or signature; van't vouch for them")
                reply(nil, nil, nil, nil, ContainerError.invalidStableInfoOrSig)
                return
            }

            loadEgoKeys(peerID: egoPeerID) { egoPeerKeys, error in
                guard let egoPeerKeys = egoPeerKeys else {
                    logger.debug("Don't have my own peer keys; can't establish: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, nil, nil, error)
                    return
                }

                guard let uuid = UUID(uuidString: crk.uuid) else {
                    logger.debug("Unable to parse uuid \(crk.uuid, privacy: .public)")
                    reply(nil, nil, nil, nil, ContainerError.recoveryKeysNotEnrolled)
                    return
                }

                guard let tpcrk = self.model.findCustodianRecoveryKey(with: uuid) else {
                    logger.debug("Unable to find custodian recovery key \(crk.uuid, privacy: .public) on model")
                    reply(nil, nil, nil, nil, ContainerError.recoveryKeysNotEnrolled)
                    return
                }

                guard self.model.isCustodianRecoveryKeyEnrolled(tpcrk.peerID) else {
                    logger.debug("Custodian Recovery Key is not enrolled")
                    reply(nil, nil, nil, nil, ContainerError.recoveryKeysNotEnrolled)
                    return
                }

                // create recovery key set
                let recoveryCRK: CustodianRecoveryKey
                do {
                    recoveryCRK = try CustodianRecoveryKey(tpCustodian: tpcrk, recoveryKeyString: crk.recoveryString, recoverySalt: crk.salt)
                } catch {
                    logger.debug("failed to create custodian recovery keys: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, nil, nil, ContainerError.failedToCreateRecoveryKey)
                    return
                }

                logger.debug("vouchWithCustodianRecoveryKey signingPubKey: \(recoveryCRK.peerKeys.signingKey.publicKey.keyData.base64EncodedString(), privacy: .public)")
                logger.debug("vouchWithCustodianRecoveryKey encryptionPubKey: \(recoveryCRK.peerKeys.encryptionKey.publicKey.keyData.base64EncodedString(), privacy: .public)")

                // find matching peer trusting custodian recovery keys
                guard let sponsorPeerID = self.model.peerIDThatTrustsCustodianRecoveryKeys(tpcrk,
                                                                                           canIntroducePeer: beneficiaryPermanentInfo,
                                                                                           stableInfo: beneficiaryStableInfo) else {
                    logger.debug("Untrusted custodian recovery key set")
                    reply(nil, nil, nil, nil, ContainerError.untrustedRecoveryKeys)
                    return
                }

                // We're going to end up trusting every peer that the sponsor peer trusts.
                // We might as well trust all TLKShares from those peers at this point.
                let (recoveredTLKs, tlkRecoveryResult) = extract(tlkShares: tlkShares, peer: recoveryCRK.peerKeys, sponsorPeerID: sponsorPeerID, model: self.model)

                do {
                    let voucher = try self.model.createVoucher(forCandidate: beneficiaryPermanentInfo,
                                                               stableInfo: beneficiaryStableInfo,
                                                               withSponsorID: sponsorPeerID,
                                                               reason: TPVoucherReason.recoveryKey,
                                                               signing: recoveryCRK.peerKeys.signingKey)

                    let newSelfTLKShares = try makeCKKSTLKShares(ckksTLKs: recoveredTLKs, asPeer: egoPeerKeys, toPeer: egoPeerKeys, epoch: Int(beneficiaryPermanentInfo.epoch))
                    reply(voucher.data, voucher.sig, newSelfTLKShares, tlkRecoveryResult, nil)
                    return
                } catch {
                    logger.debug("Error creating voucher using custodian recovery key set: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, nil, nil, error)
                    return
                }
            }
        }
    }

    func vouch(peerID: String,
               permanentInfo: Data,
               permanentInfoSig: Data,
               stableInfo: Data,
               stableInfoSig: Data,
               ckksKeys: [CKKSKeychainBackedKeySet],
               reply: @escaping (Data?, Data?, Error?) -> Void) {
        self.semaphore.wait()
        let reply: (Data?, Data?, Error?) -> Void = {
            logger.info("vouch complete: \(traceError($2), privacy: .public)")
            self.semaphore.signal()
            reply($0, $1, $2)
        }

        self.moc.performAndWait {
            // I must have an ego identity in order to vouch for someone else.
            guard let egoPeerID = self.containerMO.egoPeerID,
                let egoPermData = self.containerMO.egoPeerPermanentInfo,
                let egoPermSig = self.containerMO.egoPeerPermanentInfoSig else {
                    logger.debug("As a nonmember, can't vouch for someone else")
                    reply(nil, nil, ContainerError.nonMember)
                    return
            }

            let keyFactory = TPECPublicKeyFactory()

            guard let selfPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: egoPermData, sig: egoPermSig, keyFactory: keyFactory) else {
                reply(nil, nil, ContainerError.invalidPermanentInfoOrSig)
                return
            }

            guard let beneficiaryPermanentInfo = TPPeerPermanentInfo(peerID: peerID, data: permanentInfo, sig: permanentInfoSig, keyFactory: keyFactory) else {
                logger.debug("Invalid permenent info or signature; can't vouch for them")
                reply(nil, nil, ContainerError.invalidPermanentInfoOrSig)
                return
            }

            guard let beneficiaryStableInfo = TPPeerStableInfo(data: stableInfo, sig: stableInfoSig) else {
                logger.debug("Invalid stableinfo or signature; van't vouch for them")
                reply(nil, nil, ContainerError.invalidStableInfoOrSig)
                return
            }

            loadEgoKeys(peerID: egoPeerID) { egoPeerKeys, error in
                guard let egoPeerKeys = egoPeerKeys else {
                    logger.debug("Don't have my own keys: can't vouch for \(peerID, privacy: .public)(\(beneficiaryPermanentInfo, privacy: .public)): \(String(describing: error), privacy: .public)")
                    reply(nil, nil, error)
                    return
                }

                self.fetchPolicyDocumentsWithSemaphore(versions: Set([beneficiaryStableInfo.bestPolicyVersion()])) { _, policyFetchError in
                    guard policyFetchError == nil else {
                        logger.debug("Unknown policy for beneficiary: \(String(describing: error), privacy: .public)")
                        reply(nil, nil, policyFetchError)
                        return
                    }

                    self.moc.performAndWait {
                        let voucher: TPVoucher
                        do {
                            voucher = try self.model.createVoucher(forCandidate: beneficiaryPermanentInfo,
                                                                   stableInfo: beneficiaryStableInfo,
                                                                   withSponsorID: egoPeerID,
                                                                   reason: TPVoucherReason.secureChannel,
                                                                   signing: egoPeerKeys.signingKey)
                        } catch {
                            logger.debug("Error creating voucher: \(String(describing: error), privacy: .public)")
                            reply(nil, nil, error)
                            return
                        }

                        // And generate and upload any tlkShares
                        let tlkShares: [TLKShare]
                        do {
                            // Note that this might not be the whole list, so filter some of them out
                            let peerViews = try? self.model.getViewsForPeer(beneficiaryPermanentInfo,
                                                                            stableInfo: beneficiaryStableInfo)

                            // Note: we only want to send up TLKs for uploaded ckks zones
                            let ckksTLKs = ckksKeys
                                .filter { !$0.newUpload }
                                .filter { peerViews?.contains($0.tlk.zoneID.zoneName) ?? false }
                                .map { $0.tlk }

                            tlkShares = try makeTLKShares(ckksTLKs: ckksTLKs,
                                                          asPeer: egoPeerKeys,
                                                          toPeer: beneficiaryPermanentInfo,
                                                          epoch: Int(selfPermanentInfo.epoch))
                        } catch {
                            logger.debug("Unable to make TLKShares for beneficiary \(peerID, privacy: .public)(\(beneficiaryPermanentInfo, privacy: .public)): \(String(describing: error), privacy: .public)")
                            reply(nil, nil, error)
                            return
                        }

                        guard !tlkShares.isEmpty else {
                            logger.debug("No TLKShares to upload for new peer, returning voucher")
                            reply(voucher.data, voucher.sig, nil)
                            return
                        }

                        self.cuttlefish.updateTrust(changeToken: self.containerMO.changeToken ?? "",
                                                    peerID: egoPeerID,
                                                    stableInfoAndSig: nil,
                                                    dynamicInfoAndSig: nil,
                                                    tlkShares: tlkShares,
                                                    viewKeys: []) { response in
                            switch response {
                            case .success(let response):
                                let newKeyRecords = response.zoneKeyHierarchyRecords.map(CKRecord.init)
                                logger.debug("Uploaded new tlkshares: \(newKeyRecords, privacy: .public)")
                                // We don't need to save these; CKKS will refetch them as needed

                                reply(voucher.data, voucher.sig, nil)
                            case .failure(let error):
                                logger.debug("Unable to upload new tlkshares: \(String(describing: error), privacy: .public)")
                                reply(voucher.data, voucher.sig, error)
                                return
                            }
                        }
                    }
                }
            }
        }
    }

    func departByDistrustingSelf(reply: @escaping (Error?) -> Void) {
        self.semaphore.wait()
        let reply: (Error?) -> Void = {
            logger.info("departByDistrustingSelf complete: \(traceError($0), privacy: .public)")
            self.semaphore.signal()
            reply($0)
        }

        self.moc.performAndWait {
            guard let egoPeerID = self.containerMO.egoPeerID else {
                logger.debug("No dynamic info for self?")
                reply(ContainerError.noPreparedIdentity)
                return
            }

            self.onqueueDistrust(peerIDs: [egoPeerID], reply: reply)
        }
    }

    func distrust(peerIDs: Set<String>,
                  reply: @escaping (Error?) -> Void) {
        self.semaphore.wait()
        let reply: (Error?) -> Void = {
            logger.info("distrust complete: \(traceError($0), privacy: .public)")
            self.semaphore.signal()
            reply($0)
        }

        self.moc.performAndWait {
            guard let egoPeerID = self.containerMO.egoPeerID else {
                logger.debug("No dynamic info for self?")
                reply(ContainerError.noPreparedIdentity)
                return
            }

            guard !peerIDs.contains(egoPeerID) else {
                logger.debug("Self-distrust via peerID not allowed")
                reply(ContainerError.invalidPeerID)
                return
            }

            self.onqueueDistrust(peerIDs: peerIDs, reply: reply)
        }
    }

    func onqueueDistrust(peerIDs: Set<String>,
                         reply: @escaping (Error?) -> Void) {
        guard let egoPeerID = self.containerMO.egoPeerID else {
            logger.debug("No dynamic info for self?")
            reply(ContainerError.noPreparedIdentity)
            return
        }

        loadEgoKeyPair(identifier: signingKeyIdentifier(peerID: egoPeerID)) { signingKeyPair, error in
            guard let signingKeyPair = signingKeyPair else {
                logger.debug("No longer have signing key pair; can't sign distrust: \(String(describing: error), privacy: .public)")
                reply(error)
                return
            }

            self.moc.performAndWait {
                let dynamicInfo: TPPeerDynamicInfo
                do {
                    dynamicInfo = try self.model.calculateDynamicInfoForPeer(withID: egoPeerID,
                                                                             addingPeerIDs: nil,
                                                                             removingPeerIDs: Array(peerIDs),
                                                                             preapprovedKeys: nil,
                                                                             signing: signingKeyPair,
                                                                             currentMachineIDs: self.onqueueCurrentMIDList())
                } catch {
                    logger.debug("Error preparing dynamic info: \(String(describing: error), privacy: .public)")
                    reply(error)
                    return
                }

                let signedDynamicInfo = SignedPeerDynamicInfo(dynamicInfo)
                logger.debug("attempting distrust for \(peerIDs, privacy: .public) with: \(dynamicInfo, privacy: .public)")

                let request = UpdateTrustRequest.with {
                    $0.changeToken = self.containerMO.changeToken ?? ""
                    $0.peerID = egoPeerID
                    $0.dynamicInfoAndSig = signedDynamicInfo
                }
                self.cuttlefish.updateTrust(request) { response in
                    switch response {
                    case .success(let response):
                        do {
                            try self.persist(changes: response.changes)
                            logger.debug("distrust succeeded")
                            reply(nil)
                        } catch {
                            logger.debug("distrust handling failed: \(String(describing: error), privacy: .public)")
                            reply(error)
                        }
                    case .failure(let error):
                        logger.debug("updateTrust failed: \(String(describing: error), privacy: .public)")
                        reply(error)
                        return
                    }
                }
            }
        }
    }

    func fetchEscrowContents(reply: @escaping (Data?, String?, Data?, Error?) -> Void) {
        self.semaphore.wait()
        let reply: (Data?, String?, Data?, Error?) -> Void = {
            logger.info("fetchEscrowContents complete: \(traceError($3), privacy: .public)")
            self.semaphore.signal()
            reply($0, $1, $2, $3)
        }
        logger.debug("beginning a fetchEscrowContents")

        self.moc.performAndWait {
            guard let egoPeerID = self.containerMO.egoPeerID else {
                logger.debug("fetchEscrowContents failed")
                reply(nil, nil, nil, ContainerError.noPreparedIdentity)
                return
            }

            guard let bottles = self.containerMO.bottles as? Set<BottleMO> else {
                logger.debug("fetchEscrowContents failed")
                reply(nil, nil, nil, ContainerError.noBottleForPeer)
                return
            }

            guard let bmo = (bottles.first { $0.peerID == egoPeerID }) else {
                logger.debug("fetchEscrowContents no bottle matches peerID")
                reply(nil, nil, nil, ContainerError.noBottleForPeer)
                return
            }

            let bottleID = bmo.bottleID
            var entropy: Data

            do {
                guard let loaded = try loadSecret(label: egoPeerID) else {
                    logger.debug("fetchEscrowContents failed to load entropy")
                    reply(nil, nil, nil, ContainerError.failedToFetchEscrowContents)
                    return
                }
                entropy = loaded
            } catch {
                logger.debug("fetchEscrowContents failed to load entropy: \(String(describing: error), privacy: .public)")
                reply(nil, nil, nil, error)
                return
            }

            guard let signingPublicKey = bmo.escrowedSigningSPKI else {
                logger.debug("fetchEscrowContents no escrow signing spki")
                reply(nil, nil, nil, ContainerError.bottleDoesNotContainerEscrowKeySPKI)
                return
            }
            reply(entropy, bottleID, signingPublicKey, nil)
        }
    }

    func fetchViableBottles(reply: @escaping ([String]?, [String]?, Error?) -> Void) {
        self.semaphore.wait()
        let reply: ([String]?, [String]?, Error?) -> Void = {
            logger.info("fetchViableBottles complete: \(traceError($2), privacy: .public)")
            self.semaphore.signal()
            reply($0, $1, $2)
        }

        self.fetchViableBottlesWithSemaphore(reply: reply)
    }

    func handleFetchViableBottlesResponseWithSemaphore(response: FetchViableBottlesResponse?) {
        guard let escrowPairs = response?.viableBottles else {
            logger.debug("fetchViableBottles returned no viable bottles")
            return
        }

        var partialPairs: [EscrowPair] = []
        if let partial = response?.partialBottles {
            partialPairs = partial
        } else {
            logger.debug("fetchViableBottles returned no partially viable bottles, but that's ok")
        }

        var legacyEscrowInformations: [EscrowInformation] = []
        if let legacy = response?.legacyRecords {
            legacyEscrowInformations = legacy
        } else {
            logger.debug("fetchViableBottles returned no legacy escrow records")
        }

        escrowPairs.forEach { pair in
            let bottle = pair.bottle
            let record = pair.record
            if pair.hasRecord {
                // Save this escrow record only if we don't already have it
                if let existingRecords = self.containerMO.fullyViableEscrowRecords as? Set<EscrowRecordMO> {
                    let matchingRecords: Set<EscrowRecordMO> = existingRecords.filter { existing in existing.label == record.label
                        && existing.escrowMetadata?.bottleID == record.escrowInformationMetadata.bottleID }
                    if !matchingRecords.isEmpty {
                        logger.debug("fetchViableBottles already knows about record, re-adding entry, label = \(record.label, privacy: .public)")
                        self.containerMO.removeFromFullyViableEscrowRecords(matchingRecords as NSSet)
                    }
                    self.setEscrowRecord(record: record, viability: .full)
                }
            }
            // Save this bottle only if we don't already have it
            if let existingBottles = self.containerMO.bottles as? Set<BottleMO> {
                let matchingBottles: Set<BottleMO> = existingBottles.filter { existing in
                    existing.peerID == bottle.peerID &&
                        existing.bottleID == bottle.bottleID &&
                        existing.escrowedSigningSPKI == bottle.escrowedSigningSpki &&
                        existing.signatureUsingEscrowKey == bottle.signatureUsingEscrowKey &&
                        existing.signatureUsingPeerKey == bottle.signatureUsingPeerKey &&
                        existing.contents == bottle.contents
                }
                if !matchingBottles.isEmpty {
                    logger.debug("fetchViableBottles already knows about bottle: \(bottle.bottleID, privacy: .public)")
                    return
                }
            }

            let bmo = BottleMO(context: self.moc)
            bmo.peerID = bottle.peerID
            bmo.bottleID = bottle.bottleID
            bmo.escrowedSigningSPKI = bottle.escrowedSigningSpki
            bmo.signatureUsingEscrowKey = bottle.signatureUsingEscrowKey
            bmo.signatureUsingPeerKey = bottle.signatureUsingPeerKey
            bmo.contents = bottle.contents

            logger.debug("fetchViableBottles saving new bottle: \(bmo, privacy: .public)")
            self.containerMO.addToBottles(bmo)
        }

        partialPairs.forEach { pair in
            let bottle = pair.bottle

            let record = pair.record
            // Save this escrow record only if we don't already have it
            if pair.hasRecord {
                if let existingRecords = self.containerMO.partiallyViableEscrowRecords as? Set<EscrowRecordMO> {
                    let matchingRecords: Set<EscrowRecordMO> = existingRecords.filter { existing in existing.label == record.label
                        && existing.escrowMetadata?.bottleID == record.escrowInformationMetadata.bottleID }
                    if !matchingRecords.isEmpty {
                        logger.debug("fetchViableBottles already knows about record, re-adding entry: \(record.label, privacy: .public)")
                        self.containerMO.removeFromPartiallyViableEscrowRecords(matchingRecords as NSSet)
                    }
                    self.setEscrowRecord(record: record, viability: Viability.partial)
                }
            }

            // Save this bottle only if we don't already have it
            if let existingBottles = self.containerMO.bottles as? Set<BottleMO> {
                let matchingBottles: Set<BottleMO> = existingBottles.filter { existing in
                    existing.peerID == bottle.peerID &&
                        existing.bottleID == bottle.bottleID &&
                        existing.escrowedSigningSPKI == bottle.escrowedSigningSpki &&
                        existing.signatureUsingEscrowKey == bottle.signatureUsingEscrowKey &&
                        existing.signatureUsingPeerKey == bottle.signatureUsingPeerKey &&
                        existing.contents == bottle.contents
                }
                if !matchingBottles.isEmpty {
                    logger.debug("fetchViableBottles already knows about bottle: \(bottle.bottleID, privacy: .public)")
                    return
                }
            }

            let bmo = BottleMO(context: self.moc)
            bmo.peerID = bottle.peerID
            bmo.bottleID = bottle.bottleID
            bmo.escrowedSigningSPKI = bottle.escrowedSigningSpki
            bmo.signatureUsingEscrowKey = bottle.signatureUsingEscrowKey
            bmo.signatureUsingPeerKey = bottle.signatureUsingPeerKey
            bmo.contents = bottle.contents

            logger.debug("fetchViableBottles saving new bottle: \(bmo, privacy: .public)")
            self.containerMO.addToBottles(bmo)
        }
        legacyEscrowInformations.forEach { record in
            // Save this escrow record only if we don't already have it
            if let existingRecords = self.containerMO.legacyEscrowRecords as? Set<EscrowRecordMO> {
                let matchingRecords: Set<EscrowRecordMO> = existingRecords.filter { existing in existing.label == record.label }
                if !matchingRecords.isEmpty {
                    logger.debug("fetchViableBottles already knows about legacy record \(record.label, privacy: .public), re-adding entry")
                    self.containerMO.removeFromLegacyEscrowRecords(matchingRecords as NSSet)
                }
                if record.label.hasSuffix(".double") {
                    logger.debug("ignoring double enrollment record \(record.label, privacy: .public)")
                } else {
                    self.setEscrowRecord(record: record, viability: Viability.none)
                }
            }
        }
    }

    func fetchViableBottlesWithSemaphore(reply: @escaping ([String]?, [String]?, Error?) -> Void) {
        logger.debug("beginning a fetchViableBottles")

        self.moc.performAndWait {
            var cachedBottles = TPCachedViableBottles(viableBottles: [], partialBottles: [])

            if let lastDate = self.containerMO.escrowFetchDate {
                if Date() < lastDate.addingTimeInterval(escrowCacheTimeout) {
                    logger.debug("escrow cache still valid")
                    cachedBottles = onqueueCachedBottlesFromEscrowRecords()
                } else {
                    logger.debug("escrow cache no longer valid")
                    if let records = self.containerMO.fullyViableEscrowRecords {
                        self.containerMO.removeFromFullyViableEscrowRecords(records)
                    }
                    if let records = self.containerMO.partiallyViableEscrowRecords {
                        self.containerMO.removeFromPartiallyViableEscrowRecords(records)
                   }
                    self.containerMO.escrowFetchDate = nil
                }
            }

            if !cachedBottles.viableBottles.isEmpty || !cachedBottles.partialBottles.isEmpty {
                logger.debug("returning from fetchViableBottles, using cached bottles")
                reply(cachedBottles.viableBottles, cachedBottles.partialBottles, nil)
                return
            }

            let request = FetchViableBottlesRequest.with {
                $0.filterRequest = OctagonPlatformSupportsSOS() ? .unknown : .byOctagonOnly
            }
            if request.filterRequest == .byOctagonOnly {
                logger.debug("Requesting Cuttlefish sort records by Octagon Only")
            }

            self.cuttlefish.fetchViableBottles(request) { response in
                switch response {
                case .success(let response):
                    self.moc.performAndWait {
                        let escrowPairs = response.viableBottles
                        let  partialPairs = response.partialBottles

                        let viableBottleIDs = escrowPairs.compactMap { $0.bottle.bottleID }
                        logger.debug("fetchViableBottles returned viable bottles: \(viableBottleIDs, privacy: .public)")

                        let partialBottleIDs = partialPairs.compactMap { $0.bottle.bottleID }
                        logger.debug("fetchViableBottles returned partial bottles: \(partialBottleIDs, privacy: .public)")

                        self.handleFetchViableBottlesResponseWithSemaphore(response: response)

                        do {
                            try self.moc.save()
                            logger.debug("fetchViableBottles saved bottles")
                            self.containerMO.escrowFetchDate = Date()
                            reply(viableBottleIDs, partialBottleIDs, nil)
                        } catch {
                            logger.debug("fetchViableBottles unable to save bottles: \(String(describing: error), privacy: .public)")
                            reply(nil, nil, error)
                        }
                    }
                case .failure(let error):
                    logger.debug("fetchViableBottles failed: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, error)
                    return
                }
            }
        }
    }

    func removeEscrowCache(reply: @escaping (Error?) -> Void) {
        logger.debug("beginning a removeEscrowCache")

        self.semaphore.wait()
        let reply: (Error?) -> Void = {
            logger.info("removeEscrowCache complete \(traceError($0), privacy: .public)")
            self.semaphore.signal()
            reply($0)
        }

        self.moc.performAndWait {
            self.onQueueRemoveEscrowCache()
            reply(nil)
        }
    }

    private func onQueueRemoveEscrowCache() {
        if let records = self.containerMO.fullyViableEscrowRecords {
            self.containerMO.removeFromFullyViableEscrowRecords(records)
        }
        if let records = self.containerMO.partiallyViableEscrowRecords {
            self.containerMO.removeFromPartiallyViableEscrowRecords(records)
        }
        if let records = self.containerMO.legacyEscrowRecords {
            self.containerMO.removeFromLegacyEscrowRecords(records)
        }
        self.containerMO.escrowFetchDate = nil
    }

    func fetchEscrowRecordsWithSemaphore(forceFetch: Bool, reply: @escaping ([Data]?, Error?) -> Void) {
        logger.debug("beginning a fetchEscrowRecords")

        self.moc.performAndWait {
            var cachedRecords: [OTEscrowRecord] = []

            if forceFetch == false {
                logger.debug("fetchEscrowRecords: force fetch flag is off")
                if let lastDate = self.containerMO.escrowFetchDate {
                    if Date() < lastDate.addingTimeInterval(escrowCacheTimeout) {
                        logger.debug("escrow cache still valid")
                        cachedRecords = onqueueCachedEscrowRecords()
                    } else {
                        logger.debug("escrow cache no longer valid")
                        self.onQueueRemoveEscrowCache()
                    }
                }
            } else {
                logger.debug("fetchEscrowRecords: force fetch flag is on, removing escrow cache")
                self.onQueueRemoveEscrowCache()
            }

            if !cachedRecords.isEmpty {
                logger.debug("returning from fetchEscrowRecords, using cached escrow records")
                let recordData: [Data] = cachedRecords.map { $0.data }
                reply(recordData, nil)
                return
            }

            let request = FetchViableBottlesRequest.with {
                $0.filterRequest = OctagonPlatformSupportsSOS() ? .unknown : .byOctagonOnly
            }
            if request.filterRequest == .byOctagonOnly {
                logger.debug("Requesting Cuttlefish sort records by Octagon Only")
            }

            self.cuttlefish.fetchViableBottles(request) { response in
                switch response {
                case .success(let response):
                    self.moc.performAndWait {
                        do {
                            self.handleFetchViableBottlesResponseWithSemaphore(response: response)

                            try self.moc.save()
                            logger.debug("fetchViableBottles saved bottles and records")
                            self.containerMO.escrowFetchDate = Date()

                            var allEscrowRecordData: [Data] = []
                            if let fullyViableRecords = self.containerMO.fullyViableEscrowRecords as? Set<EscrowRecordMO> {
                                for record in fullyViableRecords {
                                    if let r = self.escrowRecordMOToEscrowRecords(record: record, viability: .full) {
                                        allEscrowRecordData.append(r.data)
                                    }
                                }
                            }
                            if let partiallyViableRecords = self.containerMO.partiallyViableEscrowRecords as? Set<EscrowRecordMO> {
                                for record in partiallyViableRecords {
                                    if let r = self.escrowRecordMOToEscrowRecords(record: record, viability: .partial) {
                                        allEscrowRecordData.append(r.data)
                                    }
                                }
                            }
                            if let legacyRecords = self.containerMO.legacyEscrowRecords as? Set<EscrowRecordMO> {
                                for record in legacyRecords {
                                    if let r = self.escrowRecordMOToEscrowRecords(record: record, viability: .none) {
                                        allEscrowRecordData.append(r.data)
                                    }
                                }
                            }
                            reply(allEscrowRecordData, nil)
                        } catch {
                            logger.debug("fetchViableBottles unable to save bottles and records: \(String(describing: error), privacy: .public)")
                            reply(nil, error)
                        }
                    }
                case .failure(let error):
                    logger.debug("fetchViableBottles failed: \(String(describing: error), privacy: .public)")
                    reply(nil, error)
                    return
                }
            }
        }
    }

    func fetchCurrentPolicy(modelIDOverride: String?, isInheritedAccount: Bool, reply: @escaping (TPSyncingPolicy?, TPPBPeerStableInfoUserControllableViewStatus, Error?) -> Void) {
        self.semaphore.wait()
        let reply: (TPSyncingPolicy?, TPPBPeerStableInfoUserControllableViewStatus, Error?) -> Void = {
            logger.info("fetchCurrentPolicy complete: \(traceError($2), privacy: .public)")
            self.semaphore.signal()
            reply($0, $1, $2)
        }

        self.moc.performAndWait {
            guard let egoPeerID = self.containerMO.egoPeerID,
                let egoPermData = self.containerMO.egoPeerPermanentInfo,
                let egoPermSig = self.containerMO.egoPeerPermanentInfoSig,
                let stableInfoData = self.containerMO.egoPeerStableInfo,
                let stableInfoSig = self.containerMO.egoPeerStableInfoSig else {
                logger.error("fetchCurrentPolicy failed to find ego peer information")
                // This is technically an error, but we also need to know the prevailing syncing policy at CloudKit signin time, not just after we've started to join

                guard let modelID = modelIDOverride else {
                    logger.debug("no model ID override; returning error")
                    reply(nil, .UNKNOWN, ContainerError.noPreparedIdentity)
                    return
                }

                guard let policyDocument = self.model.policy(withVersion: prevailingPolicyVersion.versionNumber) else {
                    logger.debug("prevailing policy is missing?")
                    reply(nil, .UNKNOWN, ContainerError.noPreparedIdentity)
                    return
                }

                do {
                    let prevailingPolicy = try policyDocument.policy(withSecrets: [:], decrypter: Decrypter())
                    let syncingPolicy = try prevailingPolicy.syncingPolicy(forModel: modelID, syncUserControllableViews: .UNKNOWN, isInheritedAccount: isInheritedAccount)

                    logger.debug("returning a policy for model ID \(modelID, privacy: .public)")
                    reply(syncingPolicy, .UNKNOWN, nil)
                    return
                } catch {
                    logger.error("fetchCurrentPolicy failed to prevailing policy: \(String(describing: error), privacy: .public)")
                    reply(nil, .UNKNOWN, ContainerError.noPreparedIdentity)
                    return
                }
            }

            let keyFactory = TPECPublicKeyFactory()
            guard let permanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: egoPermData, sig: egoPermSig, keyFactory: keyFactory) else {
                logger.error("fetchCurrentPolicy failed to create TPPeerPermanentInfo")
                reply(nil, .UNKNOWN, ContainerError.invalidPermanentInfoOrSig)
                return
            }
            guard let stableInfo = TPPeerStableInfo(data: stableInfoData, sig: stableInfoSig) else {
                logger.error("fetchCurrentPolicy failed to create TPPeerStableInfo")
                reply(nil, .UNKNOWN, ContainerError.invalidStableInfoOrSig)
                return
            }

            // We should know about all policy versions that peers might use before trying to find the best one
            let allPolicyVersions = self.model.allPolicyVersions()
            self.fetchPolicyDocumentsWithSemaphore(versions: allPolicyVersions) { _, policyFetchError in
                if let error = policyFetchError {
                    logger.debug("join: error fetching all requested policies (continuing anyway): \(String(describing: error), privacy: .public)")
                }

                do {
                    let syncingPolicy = try self.syncingPolicyFor(modelID: modelIDOverride ?? permanentInfo.modelID, stableInfo: stableInfo)

                    guard let peer = self.model.peer(withID: permanentInfo.peerID), let dynamicInfo = peer.dynamicInfo else {
                        logger.error("fetchCurrentPolicy with no dynamic info")
                        reply(syncingPolicy, .UNKNOWN, nil)
                        return
                    }

                    // Note: we specifically do not want to sanitize this value for the platform: returning FOLLOWING here isn't that helpful
                    let peersUserViewSyncability = self.model.userViewSyncabilityConsensusAmongTrustedPeers(dynamicInfo)
                    reply(syncingPolicy, peersUserViewSyncability, nil)
                    return
                } catch {
                    logger.debug("Fetching the syncing policy failed: \(String(describing: error), privacy: .public)")
                    reply(nil, .UNKNOWN, error)
                    return
                }
            }
        }
    }

    func syncingPolicyFor(modelID: String, stableInfo: TPPeerStableInfo) throws -> TPSyncingPolicy {
        let bestPolicyVersion: TPPolicyVersion

        let peerPolicyVersion = stableInfo.bestPolicyVersion()
        if peerPolicyVersion.versionNumber < frozenPolicyVersion.versionNumber {
            // This peer was from before CKKS4All, and we shouldn't listen to them when it comes to Syncing Policies
            bestPolicyVersion = prevailingPolicyVersion
            logger.debug("Ignoring policy version from pre-CKKS4All peer")
        } else {
            bestPolicyVersion = peerPolicyVersion
        }

        guard let policyDocument = self.model.policy(withVersion: bestPolicyVersion.versionNumber) else {
            logger.debug("best policy(\(bestPolicyVersion, privacy: .public)) is missing?")
            throw ContainerError.unknownPolicyVersion(bestPolicyVersion.versionNumber)
        }

        let policy = try policyDocument.policy(withSecrets: stableInfo.policySecrets ?? [:], decrypter: Decrypter())
        return try policy.syncingPolicy(forModel: modelID, syncUserControllableViews: stableInfo.syncUserControllableViews, isInheritedAccount: stableInfo.isInheritedAccount)
    }

    // All-or-nothing: return an error in case full list cannot be returned.
    // Completion handler data format: [version : [hash, data]]
    func fetchPolicyDocuments(versions: Set<TPPolicyVersion>,
                              reply: @escaping ([TPPolicyVersion: Data]?, Error?) -> Void) {
        self.semaphore.wait()
        let reply: ([TPPolicyVersion: Data]?, Error?) -> Void = {
            logger.info("fetchPolicyDocuments complete: \(traceError($1), privacy: .public)")
            self.semaphore.signal()
            reply($0, $1)
        }

        self.fetchPolicyDocumentsWithSemaphore(versions: versions) { policyDocuments, fetchError in
            reply(policyDocuments.flatMap { $0.mapValues { policyDoc in policyDoc.protobuf } }, fetchError)
        }
    }

    func fetchPolicyDocumentWithSemaphore(version: TPPolicyVersion,
                                          reply: @escaping (TPPolicyDocument?, Error?) -> Void) {
        self.fetchPolicyDocumentsWithSemaphore(versions: Set([version])) { versions, fetchError in
            guard fetchError == nil else {
                reply(nil, fetchError)
                return
            }

            guard let doc = versions?[version] else {
                logger.debug("fetchPolicyDocument: didn't return policy of version: \(String(describing: versions), privacy: .public)")
                reply(nil, ContainerError.unknownPolicyVersion(version.versionNumber))
                return
            }

            reply(doc, nil)
        }
    }

    func fetchPolicyDocumentsWithSemaphore(versions: Set<TPPolicyVersion>,
                                           reply: @escaping ([TPPolicyVersion: TPPolicyDocument]?, Error?) -> Void) {
        var remaining = versions
        var docs: [TPPolicyVersion: TPPolicyDocument] = [:]

        self.moc.performAndWait {
            for version in remaining {
                if let policydoc = try? self.getPolicyDoc(version.versionNumber), policydoc.version.policyHash == version.policyHash {
                    docs[policydoc.version] = policydoc
                    remaining.remove(version)
                }
            }
        }

        guard !remaining.isEmpty else {
            reply(docs, nil)
            return
        }

        let request = FetchPolicyDocumentsRequest.with {
            $0.keys = remaining.map { version in
                PolicyDocumentKey.with { $0.version = version.versionNumber; $0.hash = version.policyHash }}
        }

        self.cuttlefish.fetchPolicyDocuments(request) { response in
            switch response {
            case .success(let response):
                self.moc.performAndWait {
                    for mapEntry in response.entries {
                        // TODO: validate the policy's signature

                        guard let doc = TPPolicyDocument.policyDoc(withHash: mapEntry.key.hash, data: mapEntry.value) else {
                            logger.debug("Can't make policy document with hash \(mapEntry.key.hash, privacy: .public) and data \(mapEntry.value.base64EncodedString(), privacy: .public)")
                            reply(nil, ContainerError.policyDocumentDoesNotValidate)
                            return
                        }

                        guard let expectedVersion = (remaining.first { $0.versionNumber == doc.version.versionNumber }) else {
                            logger.debug("Received a policy version we didn't request: \(doc.version.versionNumber)")
                            reply(nil, ContainerError.policyDocumentDoesNotValidate)
                            return
                        }

                        guard expectedVersion.policyHash == doc.version.policyHash else {
                            logger.debug("Requested hash \(expectedVersion.policyHash, privacy: .public) does not match fetched hash \(doc.version.policyHash, privacy: .public)")
                            reply(nil, ContainerError.policyDocumentDoesNotValidate)
                            return
                        }

                        remaining.remove(expectedVersion) // Server responses should be unique, let's enforce

                        docs[doc.version] = doc
                        self.model.register(doc)

                        let policyMO = PolicyMO(context: self.moc)
                        policyMO.version = Int64(doc.version.versionNumber)
                        policyMO.policyHash = doc.version.policyHash
                        policyMO.policyData = mapEntry.value

                        self.containerMO.addToPolicies(policyMO)
                    }

                    do {
                        try self.moc.save()     // if this fails callers might make bad data assumptions
                    } catch {
                        reply(nil, error)
                        return
                    }

                    // Determine if there's anything left to fetch
                    guard let unfetchedVersion = remaining.first else {
                        // Nothing remaining? Success!
                        reply(docs, nil)
                        return
                    }

                    reply(nil, ContainerError.unknownPolicyVersion(unfetchedVersion.versionNumber))
                }
            case .failure(let error):
                logger.debug("FetchPolicyDocuments failed: \(String(describing: error), privacy: .public)")
                reply(nil, error)
                return
            }
        }
    }

    func fetchRecoverableTLKShares(peerID: String?,
                                   reply: @escaping ([CKRecord]?, Error?) -> Void) {
        let request = FetchRecoverableTLKSharesRequest.with {
            $0.peerID = peerID ?? ""
        }

        self.cuttlefish.fetchRecoverableTlkshares(request) { response in
            switch response {
            case .failure(let error):
                logger.debug("fetchRecoverableTlkshares failed: \(String(describing: error), privacy: .public)")
                reply(nil, error)
                return

            case .success(let response):
                let shareCount = response.views.map { $0.tlkShares.count }.reduce(0, +)
                logger.debug("fetchRecoverableTlkshares succeeded: found \(response.views.count) views and \(shareCount) total TLKShares")
                let records = response.views.flatMap { $0.ckrecords() }
                reply(records, nil)
                return
            }
        }
    }

    // Must be on moc queue to call this.
    // Caller is responsible for saving the moc afterwards.
    @discardableResult
    private func registerPeerMO(permanentInfo: TPPeerPermanentInfo,
                                stableInfo: TPPeerStableInfo? = nil,
                                dynamicInfo: TPPeerDynamicInfo? = nil,
                                vouchers: [TPVoucher]? = nil,
                                isEgoPeer: Bool = false) throws -> PeerMO {
        let peerID = permanentInfo.peerID

        let peer = PeerMO(context: self.moc)
        peer.peerID = peerID
        peer.permanentInfo = permanentInfo.data
        peer.permanentInfoSig = permanentInfo.sig
        peer.stableInfo = stableInfo?.data
        peer.stableInfoSig = stableInfo?.sig
        peer.dynamicInfo = dynamicInfo?.data
        peer.dynamicInfoSig = dynamicInfo?.sig
        peer.isEgoPeer = isEgoPeer
        self.containerMO.addToPeers(peer)

        self.model.registerPeer(with: permanentInfo)
        if let stableInfo = stableInfo {
            try self.model.update(stableInfo, forPeerWithID: peerID)
        }
        if let dynamicInfo = dynamicInfo {
            try self.model.update(dynamicInfo, forPeerWithID: peerID)
        }
        vouchers?.forEach { voucher in
            self.model.register(voucher)

            if !((peer.vouchers as? Set<TPVoucher> ?? Set()).contains { $0.data == voucher.data && $0.sig == voucher.sig }) {
                let voucherMO = VoucherMO(context: self.moc)
                voucherMO.voucherInfo = voucher.data
                voucherMO.voucherInfoSig = voucher.sig
                peer.addToVouchers(voucherMO)
            }
        }
        return peer
    }

    /* Returns any new CKKS keys that need uploading, as well as any TLKShares necessary for those keys */
    func makeSharesForNewKeySets(ckksKeys: [CKKSKeychainBackedKeySet],
                                 tlkShares: [CKKSTLKShare],
                                 egoPeerKeys: OctagonSelfPeerKeys,
                                 egoPeerDynamicInfo: TPPeerDynamicInfo,
                                 epoch: Int) throws -> ([ViewKeys], [TLKShare]) {
        let newCKKSKeys = ckksKeys.filter { $0.newUpload }
        let newViewKeys: [ViewKeys] = newCKKSKeys.map(ViewKeys.convert)

        let octagonSelfShares = try makeTLKShares(ckksTLKs: ckksKeys.map { $0.tlk },
                                                  asPeer: egoPeerKeys,
                                                  toPeer: egoPeerKeys,
                                                  epoch: epoch)
        let extraShares = tlkShares.map { TLKShare.convert(ckksTLKShare: $0) }

        var peerShares: [TLKShare] = []

        for keyset in newCKKSKeys {
            do {
                let peerIDsWithAccess = try self.model.getPeerIDsTrustedByPeer(with: egoPeerDynamicInfo,
                                                                               toAccessView: keyset.tlk.zoneID.zoneName)
                logger.debug("Planning to share \(String(describing: keyset.tlk), privacy: .public) with peers \(peerIDsWithAccess, privacy: .public)")

                let peers = peerIDsWithAccess.compactMap { self.model.peer(withID: $0) }
                let viewPeerShares = try peers.map { receivingPeer in
                    TLKShare.convert(ckksTLKShare: try CKKSTLKShare(keyset.tlk,
                                                                    as: egoPeerKeys,
                                                                    to: receivingPeer.permanentInfo,
                                                                    epoch: epoch,
                                                                    poisoned: 0))
                }

                peerShares += viewPeerShares
            } catch {
                logger.debug("Unable to create TLKShares for keyset \(keyset, privacy: .public): \(String(describing: error), privacy: .public)")
            }
        }

        return (newViewKeys, octagonSelfShares + peerShares + extraShares)
    }

    func onqueuePreparePeerForJoining(egoPeerID: String,
                                      peerPermanentInfo: TPPeerPermanentInfo,
                                      stableInfo: TPPeerStableInfo,
                                      sponsorID: String?,
                                      preapprovedKeys: [Data]?,
                                      vouchers: [SignedVoucher],
                                      egoPeerKeys: OctagonSelfPeerKeys) throws -> (Peer, TPPeerDynamicInfo) {
        let dynamicInfo = try self.model.dynamicInfo(forJoiningPeerID: egoPeerID,
                                                     peerPermanentInfo: peerPermanentInfo,
                                                     peerStableInfo: stableInfo,
                                                     sponsorID: sponsorID,
                                                     preapprovedKeys: preapprovedKeys,
                                                     signing: egoPeerKeys.signingKey,
                                                     currentMachineIDs: self.onqueueCurrentMIDList())

        let userViewSyncability: TPPBPeerStableInfoUserControllableViewStatus?
        if [.ENABLED, .DISABLED].contains(stableInfo.syncUserControllableViews) {
            // No change!
            userViewSyncability = nil
        } else {
            let newUserViewSyncability: TPPBPeerStableInfoUserControllableViewStatus

            if peerPermanentInfo.modelID.hasPrefix("AppleTV") ||
                peerPermanentInfo.modelID.hasPrefix("AudioAccessory") ||
                peerPermanentInfo.modelID.hasPrefix("Watch") {
                // Watches, TVs, and AudioAccessories always join as FOLLOWING.
                newUserViewSyncability = .FOLLOWING
            } else {
                // All other platforms select what the other devices say to do
                newUserViewSyncability = self.model.userViewSyncabilityConsensusAmongTrustedPeers(dynamicInfo)
            }

            logger.debug("join: setting 'user view sync' control as: \(TPPBPeerStableInfoUserControllableViewStatusAsString(newUserViewSyncability), privacy: .public)")
            userViewSyncability = newUserViewSyncability
        }

        let newStableInfo = try self.createNewStableInfoIfNeeded(stableChanges: StableChanges.change(viewStatus: userViewSyncability),
                                                                 permanentInfo: peerPermanentInfo,
                                                                 existingStableInfo: stableInfo,
                                                                 dynamicInfo: dynamicInfo,
                                                                 signingKeyPair: egoPeerKeys.signingKey)

        let peer = Peer.with {
            $0.peerID = egoPeerID
            $0.permanentInfoAndSig = SignedPeerPermanentInfo(peerPermanentInfo)
            $0.stableInfoAndSig = SignedPeerStableInfo(newStableInfo ?? stableInfo)
            $0.dynamicInfoAndSig = SignedPeerDynamicInfo(dynamicInfo)
            $0.vouchers = vouchers
        }

        return (peer, dynamicInfo)
    }

    func join(voucherData: Data,
              voucherSig: Data,
              ckksKeys: [CKKSKeychainBackedKeySet],
              tlkShares: [CKKSTLKShare],
              preapprovedKeys: [Data]?,
              reply: @escaping (String?, [CKRecord], TPSyncingPolicy?, Error?) -> Void) {
        self.semaphore.wait()
        let reply: (String?, [CKRecord], TPSyncingPolicy?, Error?) -> Void = {
            logger.info("join complete: \(traceError($3), privacy: .public)")
            self.semaphore.signal()
            reply($0, $1, $2, $3)
        }

        self.fetchAndPersistChanges { error in
            guard error == nil else {
                reply(nil, [], nil, error)
                return
            }

            // To join, you must know all policies that exist
            let allPolicyVersions = self.model.allPolicyVersions()
            self.fetchPolicyDocumentsWithSemaphore(versions: allPolicyVersions) { _, policyFetchError in
                if let error = policyFetchError {
                    logger.debug("join: error fetching all requested policies (continuing anyway): \(String(describing: error), privacy: .public)")
                }

                self.moc.performAndWait {
                    guard let voucher = TPVoucher(infoWith: voucherData, sig: voucherSig) else {
                        reply(nil, [], nil, ContainerError.invalidVoucherOrSig)
                        return
                    }
                    guard let sponsor = self.model.peer(withID: voucher.sponsorID) else {
                        reply(nil, [], nil, ContainerError.sponsorNotRegistered(voucher.sponsorID))
                        return
                    }

                    // Fetch ego peer identity from local storage.
                    guard let egoPeerID = self.containerMO.egoPeerID,
                        let egoPermData = self.containerMO.egoPeerPermanentInfo,
                        let egoPermSig = self.containerMO.egoPeerPermanentInfoSig,
                        let egoStableData = self.containerMO.egoPeerStableInfo,
                        let egoStableSig = self.containerMO.egoPeerStableInfoSig
                        else {
                            reply(nil, [], nil, ContainerError.noPreparedIdentity)
                            return
                    }

                    let keyFactory = TPECPublicKeyFactory()
                    guard let selfPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: egoPermData, sig: egoPermSig, keyFactory: keyFactory) else {
                        reply(nil, [], nil, ContainerError.invalidPermanentInfoOrSig)
                        return
                    }
                    guard let selfStableInfo = TPPeerStableInfo(data: egoStableData, sig: egoStableSig) else {
                        reply(nil, [], nil, ContainerError.invalidStableInfoOrSig)
                        return
                    }
                    guard self.onqueueMachineIDAllowedByIDMS(machineID: selfPermanentInfo.machineID) else {
                        logger.debug("join: self machineID \(selfPermanentInfo.machineID, privacy: .public) not on list")
                        self.onqueueTTRUntrusted()
                        reply(nil, [], nil, ContainerError.preparedIdentityNotOnAllowedList(selfPermanentInfo.machineID))
                        return
                    }

                    loadEgoKeys(peerID: egoPeerID) { egoPeerKeys, error in
                        guard let egoPeerKeys = egoPeerKeys else {
                            logger.debug("Don't have my own peer keys; can't join: \(String(describing: error), privacy: .public)")
                            reply(nil, [], nil, error)
                            return
                        }
                        self.moc.performAndWait {
                            let peer: Peer
                            let newDynamicInfo: TPPeerDynamicInfo
                            do {
                                (peer, newDynamicInfo) = try self.onqueuePreparePeerForJoining(egoPeerID: egoPeerID,
                                                                                               peerPermanentInfo: selfPermanentInfo,
                                                                                               stableInfo: selfStableInfo,
                                                                                               sponsorID: sponsor.peerID,
                                                                                               preapprovedKeys: preapprovedKeys,
                                                                                               vouchers: [SignedVoucher.with {
                                                                                                $0.voucher = voucher.data
                                                                                                $0.sig = voucher.sig
                                                                                                }, ],
                                                                                               egoPeerKeys: egoPeerKeys)
                            } catch {
                                logger.debug("Unable to create peer for joining: \(String(describing: error), privacy: .public)")
                                reply(nil, [], nil, error)
                                return
                            }

                            guard let peerStableInfo = peer.stableInfoAndSig.toStableInfo() else {
                                logger.debug("Unable to create new peer stable info for joining")
                                reply(nil, [], nil, ContainerError.invalidStableInfoOrSig)
                                return
                            }

                            let allTLKShares: [TLKShare]
                            let viewKeys: [ViewKeys]
                            do {
                                (viewKeys, allTLKShares) = try self.makeSharesForNewKeySets(ckksKeys: ckksKeys,
                                                                                            tlkShares: tlkShares,
                                                                                            egoPeerKeys: egoPeerKeys,
                                                                                            egoPeerDynamicInfo: newDynamicInfo,
                                                                                            epoch: Int(selfPermanentInfo.epoch))
                            } catch {
                                logger.debug("Unable to process keys before joining: \(String(describing: error), privacy: .public)")
                                reply(nil, [], nil, error)
                                return
                            }

                            do {
                                try self.model.checkIntroduction(forCandidate: selfPermanentInfo,
                                                                 stableInfo: peer.stableInfoAndSig.toStableInfo(),
                                                                 withSponsorID: sponsor.peerID)
                            } catch {
                                logger.debug("Error checking introduction: \(String(describing: error), privacy: .public)")
                                reply(nil, [], nil, error)
                                return
                            }

                            var bottle: Bottle
                            do {
                                bottle = try self.assembleBottle(egoPeerID: egoPeerID)
                            } catch {
                                reply(nil, [], nil, error)
                                return
                            }

                            logger.debug("Beginning join for peer \(egoPeerID, privacy: .public)")
                            logger.debug("Join permanentInfo: \(egoPermData.base64EncodedString(), privacy: .public)")
                            logger.debug("Join permanentInfoSig: \(egoPermSig.base64EncodedString(), privacy: .public)")
                            logger.debug("Join stableInfo: \(peer.stableInfoAndSig.peerStableInfo.base64EncodedString(), privacy: .public)")
                            logger.debug("Join stableInfoSig: \(peer.stableInfoAndSig.sig.base64EncodedString(), privacy: .public)")
                            logger.debug("Join dynamicInfo: \(peer.dynamicInfoAndSig.peerDynamicInfo.base64EncodedString(), privacy: .public)")
                            logger.debug("Join dynamicInfoSig: \(peer.dynamicInfoAndSig.sig.base64EncodedString(), privacy: .public)")

                            logger.debug("Join vouchers: \(peer.vouchers.map { $0.voucher.base64EncodedString() }, privacy: .public)")
                            logger.debug("Join voucher signatures: \(peer.vouchers.map { $0.sig.base64EncodedString() }, privacy: .public)")

                            logger.debug("Uploading \(allTLKShares.count) tlk shares")

                            do {
                                let peerBase64 = try peer.serializedData().base64EncodedString()
                                logger.debug("Join peer: \(peerBase64, privacy: .public)")
                            } catch {
                                logger.debug("Join unable to encode peer: \(String(describing: error), privacy: .public)")
                            }

                            let changeToken = self.containerMO.changeToken ?? ""
                            let request = JoinWithVoucherRequest.with {
                                $0.changeToken = changeToken
                                $0.peer = peer
                                $0.bottle = bottle
                                $0.tlkShares = allTLKShares
                                $0.viewKeys = viewKeys
                            }
                            self.cuttlefish.joinWithVoucher(request) { response in
                                switch response {
                                case .success(let response):
                                    self.moc.performAndWait {
                                        do {
                                            self.containerMO.egoPeerStableInfo = peer.stableInfoAndSig.peerStableInfo
                                            self.containerMO.egoPeerStableInfoSig = peer.stableInfoAndSig.sig

                                            let syncingPolicy = try self.syncingPolicyFor(modelID: selfPermanentInfo.modelID,
                                                                                          stableInfo: peerStableInfo)

                                            try self.onQueuePersist(changes: response.changes)
                                            logger.debug("JoinWithVoucher succeeded")

                                            let keyHierarchyRecords = response.zoneKeyHierarchyRecords.compactMap { CKRecord($0) }
                                            reply(egoPeerID, keyHierarchyRecords, syncingPolicy, nil)
                                        } catch {
                                            logger.debug("JoinWithVoucher failed: \(String(describing: error), privacy: .public)")
                                            reply(nil, [], nil, error)
                                        }
                                    }
                                case .failure(let error):
                                    logger.debug("joinWithVoucher failed: \(String(describing: error), privacy: .public)")
                                    reply(nil, [], nil, error)
                                    return
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    func requestHealthCheck(requiresEscrowCheck: Bool, knownFederations: [String], reply: @escaping (Bool, Bool, Bool, Bool, OTEscrowMoveRequestContext?, Error?) -> Void) {
        self.semaphore.wait()
        let reply: (Bool, Bool, Bool, Bool, OTEscrowMoveRequestContext?, Error?) -> Void = {
            logger.info("health check complete: \(traceError($5), privacy: .public)")
            self.semaphore.signal()
            reply($0, $1, $2, $3, $4, $5)
        }

        logger.debug("requestHealthCheck requiring escrow check: \(requiresEscrowCheck), knownFederations: \(knownFederations, privacy: .public)")

        self.moc.performAndWait {
            guard let egoPeerID = self.containerMO.egoPeerID else {
                // No identity, nothing to do
                logger.debug("requestHealthCheck: No identity.")
                reply(false, false, false, false, nil, ContainerError.noPreparedIdentity)
                return
            }
            let request = GetRepairActionRequest.with {
                $0.peerID = egoPeerID
                $0.requiresEscrowCheck = requiresEscrowCheck
                $0.knownFederations = knownFederations
            }

            self.cuttlefish.getRepairAction(request) { response in
                switch response {
                case .success(let response):
                    let action = response.repairAction

                    var postRepairAccount: Bool = false
                    var postRepairEscrow: Bool = false
                    var resetOctagon: Bool = false
                    var leaveTrust: Bool = false

                    switch action {
                    case .noAction:
                        break
                    case .postRepairAccount:
                        postRepairAccount = true
                    case .postRepairEscrow:
                        postRepairEscrow = true
                    case .resetOctagon:
                        resetOctagon = true
                    case .leaveTrust:
                        leaveTrust = true
                    case .UNRECOGNIZED:
                        break
                    }

                    var moveRequest: OTEscrowMoveRequestContext?
                    if response.hasEscrowRecordMoveRequest {
                        moveRequest = OTEscrowMoveRequestContext()
                        if let m = moveRequest {
                            m.escrowRecordLabel = response.escrowRecordMoveRequest.escrowRecordLabel
                            m.currentFederation = response.escrowRecordMoveRequest.currentFederation
                            m.intendedFederation = response.escrowRecordMoveRequest.intendedFederation
                        }
                    }

                    reply(postRepairAccount, postRepairEscrow, resetOctagon, leaveTrust, moveRequest, nil)
                case .failure(let error):
                    reply(false, false, false, false, nil, error)
                    return
                }
            }
        }
    }

    func getSupportAppInfo(reply: @escaping (Data?, Error?) -> Void) {
        self.semaphore.wait()
        let reply: (Data?, Error?) -> Void = {
            logger.info("getSupportAppInfo complete: \(traceError($1), privacy: .public)")
            self.semaphore.signal()
            reply($0, $1)
        }

        self.cuttlefish.getSupportAppInfo { response in
            switch response {
            case .success(let response):
                guard let data = try? response.serializedData() else {
                    reply(nil, ContainerError.failedToSerializeData)
                    return
                }

                reply(data, nil)
            case .failure(let error):
                logger.debug("getSupportAppInfo failed: \(String(describing: error), privacy: .public)")
                reply(nil, error)
                return
            }
        }
    }

    func resetCDPAccountData(reply: @escaping (Error?) -> Void) {
        self.semaphore.wait()
        let reply: (Error?) -> Void = {
            logger.info("resetCDPAccountData complete: \(traceError($0), privacy: .public)")
            self.semaphore.signal()
            reply($0)
        }

        let request = ResetAccountCDPContentsRequest.with {
            $0.resetReason = .testGenerated
        }
        self.cuttlefish.resetAccountCdpcontents(request) { response in
            switch response {
            case .success:
                self.moc.performAndWait {
                    self.onQueueRemoveEscrowCache()
                }
                reply(nil)
            case .failure(let error):
                logger.debug("resetCDPAccountData failed: \(String(describing: error), privacy: .public)")
                reply(error)
                return
            }
        }
    }


    func preflightPreapprovedJoin(preapprovedKeys: [Data]?,
                                  reply: @escaping (Bool, Error?) -> Void) {
        self.semaphore.wait()
        let reply: (Bool, Error?) -> Void = {
            logger.info("preflightPreapprovedJoin complete: \(traceError($1), privacy: .public)")
            self.semaphore.signal()
            reply($0, $1)
        }

        self.fetchAndPersistChanges { error in
            guard error == nil else {
                logger.debug("preflightPreapprovedJoin unable to fetch changes: \(String(describing: error), privacy: .public)")
                reply(false, error)
                return
            }

            // We need to try to have all policy versions that our peers claim to behave
            let allPolicyVersions = self.model.allPolicyVersions()
            self.fetchPolicyDocumentsWithSemaphore(versions: allPolicyVersions) { _, policyFetchError in
                if let error = policyFetchError {
                    logger.debug("preflightPreapprovedJoin: error fetching all requested policies (continuing anyway): \(String(describing: error), privacy: .public)")
                }

                // We explicitly ignore the machine ID list here; we're only interested in the peer states: do they preapprove us?

                guard !self.model.allPeerIDs().isEmpty else {
                    // If, after fetch and handle changes, there's no peers, then we can likely establish.
                    reply(true, nil)
                    return
                }

                self.moc.performAndWait {
                    guard let egoPeerID = self.containerMO.egoPeerID,
                          let egoPermData = self.containerMO.egoPeerPermanentInfo,
                          let egoPermSig = self.containerMO.egoPeerPermanentInfoSig
                    else {
                        logger.debug("preflightPreapprovedJoin: no prepared identity")
                        reply(false, ContainerError.noPreparedIdentity)
                        return
                    }

                    let keyFactory = TPECPublicKeyFactory()
                    guard let egoPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: egoPermData, sig: egoPermSig, keyFactory: keyFactory) else {
                        logger.debug("preflightPreapprovedJoin: invalid permanent info")
                        reply(false, ContainerError.invalidPermanentInfoOrSig)
                        return
                    }

                    guard self.model.hasPotentiallyTrustedPeerPreapprovingKey(egoPermanentInfo.signingPubKey.spki()) else {
                        logger.debug("preflightPreapprovedJoin: no peers preapprove our key")
                        reply(false, ContainerError.noPeersPreapprovePreparedIdentity)
                        return
                    }

                    let keysApprovingPeers = preapprovedKeys?.filter { key in
                        self.model.hasPotentiallyTrustedPeer(withSigningKey: key)
                    }

                    guard (keysApprovingPeers?.count ?? 0) > 0 else {
                        logger.debug("preflightPreapprovedJoin: no reciprocal trust for existing peers")
                        reply(false, ContainerError.noPeersPreapprovedBySelf)
                        return
                    }

                    reply(true, nil)
                }
            }
        }
    }

    func preapprovedJoin(ckksKeys: [CKKSKeychainBackedKeySet],
                         tlkShares: [CKKSTLKShare],
                         preapprovedKeys: [Data]?,
                         reply: @escaping (String?, [CKRecord], TPSyncingPolicy?, Error?) -> Void) {
        self.semaphore.wait()
        let reply: (String?, [CKRecord], TPSyncingPolicy?, Error?) -> Void = {
            logger.info("preapprovedJoin complete: \(traceError($3), privacy: .public)")
            self.semaphore.signal()
            reply($0, $1, $2, $3)
        }

        self.fetchAndPersistChangesIfNeeded { error in
            guard error == nil else {
                logger.debug("preapprovedJoin unable to fetch changes: \(String(describing: error), privacy: .public)")
                reply(nil, [], nil, error)
                return
            }
            self.moc.performAndWait {
                // If, after fetch and handle changes, there's no peers, then fire off an establish
                // Note that if the establish fails, retrying this call might work.
                // That's up to the caller.
                if self.model.allPeerIDs().isEmpty {
                    logger.debug("preapprovedJoin but no existing peers, attempting establish")

                    self.onqueueEstablish(ckksKeys: ckksKeys,
                                          tlkShares: tlkShares,
                                          preapprovedKeys: preapprovedKeys,
                                          reply: reply)
                    return
                }

                // Fetch ego peer identity from local storage.
                guard let egoPeerID = self.containerMO.egoPeerID,
                    let egoPermData = self.containerMO.egoPeerPermanentInfo,
                    let egoPermSig = self.containerMO.egoPeerPermanentInfoSig,
                    let egoStableData = self.containerMO.egoPeerStableInfo,
                    let egoStableSig = self.containerMO.egoPeerStableInfoSig
                    else {
                        logger.debug("preapprovedJoin: no prepared identity")
                        reply(nil, [], nil, ContainerError.noPreparedIdentity)
                        return
                }

                let keyFactory = TPECPublicKeyFactory()
                guard let selfPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: egoPermData, sig: egoPermSig, keyFactory: keyFactory) else {
                    reply(nil, [], nil, ContainerError.invalidPermanentInfoOrSig)
                    return
                }
                guard let selfStableInfo = TPPeerStableInfo(data: egoStableData, sig: egoStableSig) else {
                    reply(nil, [], nil, ContainerError.invalidStableInfoOrSig)
                    return
                }

                guard self.onqueueMachineIDAllowedByIDMS(machineID: selfPermanentInfo.machineID) else {
                    logger.debug("preapprovedJoin: self machineID \(selfPermanentInfo.machineID, privacy: .public) (me) not on list")
                    self.onqueueTTRUntrusted()
                    reply(nil, [], nil, ContainerError.preparedIdentityNotOnAllowedList(selfPermanentInfo.machineID))
                    return
                }
                loadEgoKeys(peerID: egoPeerID) { egoPeerKeys, error in
                    guard let egoPeerKeys = egoPeerKeys else {
                        logger.debug("preapprovedJoin: Don't have my own keys: can't join: \(String(describing: error), privacy: .public)")
                        reply(nil, [], nil, error)
                        return
                    }

                    guard self.model.hasPotentiallyTrustedPeerPreapprovingKey(egoPeerKeys.signingKey.publicKey().spki()) else {
                        logger.debug("preapprovedJoin: no peers preapprove our key")
                        reply(nil, [], nil, ContainerError.noPeersPreapprovePreparedIdentity)
                        return
                    }

                    self.moc.performAndWait {
                        let peer: Peer
                        let newDynamicInfo: TPPeerDynamicInfo
                        do {
                            (peer, newDynamicInfo) = try self.onqueuePreparePeerForJoining(egoPeerID: egoPeerID,
                                                                                           peerPermanentInfo: selfPermanentInfo,
                                                                                           stableInfo: selfStableInfo,
                                                                                           sponsorID: nil,
                                                                                           preapprovedKeys: preapprovedKeys,
                                                                                           vouchers: [],
                                                                                           egoPeerKeys: egoPeerKeys)
                        } catch {
                            logger.debug("Unable to create peer for joining: \(String(describing: error), privacy: .public)")
                            reply(nil, [], nil, error)
                            return
                        }

                        guard let peerStableInfo = peer.stableInfoAndSig.toStableInfo() else {
                            logger.debug("Unable to create new peer stable info for joining")
                            reply(nil, [], nil, ContainerError.invalidStableInfoOrSig)
                            return
                        }

                        let allTLKShares: [TLKShare]
                        let viewKeys: [ViewKeys]
                        do {
                            (viewKeys, allTLKShares) = try self.makeSharesForNewKeySets(ckksKeys: ckksKeys,
                                                                                        tlkShares: tlkShares,
                                                                                        egoPeerKeys: egoPeerKeys,
                                                                                        egoPeerDynamicInfo: newDynamicInfo,
                                                                                        epoch: Int(selfPermanentInfo.epoch))
                        } catch {
                            logger.debug("Unable to process keys before joining: \(String(describing: error), privacy: .public)")
                            reply(nil, [], nil, error)
                            return
                        }

                        var bottle: Bottle
                        do {
                            bottle = try self.assembleBottle(egoPeerID: egoPeerID)
                        } catch {
                            reply(nil, [], nil, error)
                            return
                        }

                        logger.debug("Beginning preapprovedJoin for peer \(egoPeerID, privacy: .public)")
                        logger.debug("preapprovedJoin permanentInfo: \(egoPermData.base64EncodedString(), privacy: .public)")
                        logger.debug("preapprovedJoin permanentInfoSig: \(egoPermSig.base64EncodedString(), privacy: .public)")
                        logger.debug("preapprovedJoin stableInfo: \(egoStableData.base64EncodedString(), privacy: .public)")
                        logger.debug("preapprovedJoin stableInfoSig: \(egoStableSig.base64EncodedString(), privacy: .public)")
                        logger.debug("preapprovedJoin dynamicInfo: \(peer.dynamicInfoAndSig.peerDynamicInfo.base64EncodedString(), privacy: .public)")
                        logger.debug("preapprovedJoin dynamicInfoSig: \(peer.dynamicInfoAndSig.sig.base64EncodedString(), privacy: .public)")

                        logger.debug("preapprovedJoin vouchers: \(peer.vouchers.map { $0.voucher.base64EncodedString() }, privacy: .public)")
                        logger.debug("preapprovedJoin voucher signatures: \(peer.vouchers.map { $0.sig.base64EncodedString() }, privacy: .public)")

                        logger.debug("preapprovedJoin: uploading \(allTLKShares.count) tlk shares")

                        do {
                            let peerBase64 = try peer.serializedData().base64EncodedString()
                            logger.debug("preapprovedJoin peer: \(peerBase64, privacy: .public)")
                        } catch {
                            logger.debug("preapprovedJoin unable to encode peer: \(String(describing: error), privacy: .public)")
                        }

                        let changeToken = self.containerMO.changeToken ?? ""
                        let request = JoinWithVoucherRequest.with {
                            $0.changeToken = changeToken
                            $0.peer = peer
                            $0.bottle = bottle
                            $0.tlkShares = allTLKShares
                            $0.viewKeys = viewKeys
                        }
                        self.cuttlefish.joinWithVoucher(request) { response in
                            switch response {
                            case .success(let response):
                                self.moc.performAndWait {
                                    do {
                                        self.containerMO.egoPeerStableInfo = peer.stableInfoAndSig.peerStableInfo
                                        self.containerMO.egoPeerStableInfoSig = peer.stableInfoAndSig.sig

                                        let syncingPolicy = try self.syncingPolicyFor(modelID: selfPermanentInfo.modelID,
                                                                                      stableInfo: peerStableInfo)

                                        try self.onQueuePersist(changes: response.changes)
                                        logger.debug("preapprovedJoin succeeded")

                                        let keyHierarchyRecords = response.zoneKeyHierarchyRecords.compactMap { CKRecord($0) }
                                        reply(egoPeerID, keyHierarchyRecords, syncingPolicy, nil)
                                    } catch {
                                        logger.debug("preapprovedJoin failed: \(String(describing: error), privacy: .public)")
                                        reply(nil, [], nil, error)
                                    }
                                }
                            case .failure(let error):
                                logger.debug("preapprovedJoin failed: \(String(describing: error), privacy: .public)")
                                reply(nil, [], nil, error)
                                return
                            }
                        }
                    }
                }
            }
        }
    }

    func update(forceRefetch: Bool,
                deviceName: String?,
                serialNumber: String?,
                osVersion: String?,
                policyVersion: UInt64?,
                policySecrets: [String: Data]?,
                syncUserControllableViews: TPPBPeerStableInfoUserControllableViewStatus?,
                secureElementIdentity: TrustedPeersHelperIntendedTPPBSecureElementIdentity?,
                reply: @escaping (TrustedPeersHelperPeerState?, TPSyncingPolicy?, Error?) -> Void) {
        self.semaphore.wait()
        let reply: (TrustedPeersHelperPeerState?, TPSyncingPolicy?, Error?) -> Void = {
            logger.info("update complete: \(traceError($2), privacy: .public)")
            self.semaphore.signal()
            reply($0, $1, $2)
        }

        // Get (and save) the latest from cuttlefish
        let stableChanges = StableChanges(deviceName: deviceName,
                                          serialNumber: serialNumber,
                                          osVersion: osVersion,
                                          policyVersion: policyVersion,
                                          policySecrets: policySecrets,
                                          setSyncUserControllableViews: syncUserControllableViews,
                                          secureElementIdentity: secureElementIdentity)
        self.fetchChangesAndUpdateTrustIfNeeded(forceRefetch: forceRefetch, stableChanges: stableChanges, reply: reply)
    }

    func set(preapprovedKeys: [Data],
             reply: @escaping (TrustedPeersHelperPeerState?, Error?) -> Void) {
        self.semaphore.wait()
        let reply: (TrustedPeersHelperPeerState?, Error?) -> Void = {
            logger.info("setPreapprovedKeys complete: \(traceError($1), privacy: .public)")
            self.semaphore.signal()
            reply($0, $1)
        }

        self.moc.performAndWait {
            logger.debug("setPreapprovedKeys: \(preapprovedKeys, privacy: .public)")

            guard let egoPeerID = self.containerMO.egoPeerID else {
                // No identity, nothing to do
                logger.debug("setPreapprovedKeys: No identity.")
                reply(nil, ContainerError.noPreparedIdentity)
                return
            }
            loadEgoKeyPair(identifier: signingKeyIdentifier(peerID: egoPeerID)) { signingKeyPair, error in
                guard let signingKeyPair = signingKeyPair else {
                    logger.debug("setPreapprovedKeys: no signing key pair: \(String(describing: error), privacy: .public)")
                    reply(nil, error ?? ContainerError.unableToCreateKeyPair)
                    return
                }

                self.moc.performAndWait {
                    let dynamicInfo: TPPeerDynamicInfo
                    do {
                        dynamicInfo = try self.model.calculateDynamicInfoForPeer(withID: egoPeerID,
                                                                                 addingPeerIDs: nil,
                                                                                 removingPeerIDs: nil,
                                                                                 preapprovedKeys: preapprovedKeys,
                                                                                 signing: signingKeyPair,
                                                                                 currentMachineIDs: self.onqueueCurrentMIDList())
                    } catch {
                        logger.debug("setPreapprovedKeys: couldn't calculate dynamic info: \(String(describing: error), privacy: .public)")
                        reply(nil, error)
                        return
                    }

                    logger.debug("setPreapprovedKeys: produced a dynamicInfo: \(dynamicInfo, privacy: .public)")

                    if dynamicInfo == self.model.peer(withID: egoPeerID)?.dynamicInfo {
                        logger.debug("setPreapprovedKeys: no change; nothing to do.")

                        // Calling this will fill in the peer status
                        self.updateTrustIfNeeded { status, _, error in
                            reply(status, error)
                        }
                        return
                    }

                    logger.debug("setPreapprovedKeys: attempting updateTrust for \(egoPeerID, privacy: .public) with: \(dynamicInfo, privacy: .public)")
                    let request = UpdateTrustRequest.with {
                        $0.changeToken = self.containerMO.changeToken ?? ""
                        $0.peerID = egoPeerID
                        $0.dynamicInfoAndSig = SignedPeerDynamicInfo(dynamicInfo)
                    }

                    self.perform(updateTrust: request) { state, _, error in
                        guard error == nil else {
                            logger.debug("setPreapprovedKeys: failed: \(String(describing: error), privacy: .public)")
                            reply(state, error)
                            return
                        }

                        logger.debug("setPreapprovedKeys: updateTrust succeeded")
                        reply(state, nil)
                    }
                }
            }
        }
    }

    func updateTLKs(ckksKeys: [CKKSKeychainBackedKeySet],
                    tlkShares: [CKKSTLKShare],
                    reply: @escaping ([CKRecord]?, Error?) -> Void) {
        self.semaphore.wait()
        let reply: ([CKRecord]?, Error?) -> Void = {
            logger.info("updateTLKs complete: \(traceError($1), privacy: .public)")
            self.semaphore.signal()
            reply($0, $1)
        }

        logger.debug("Uploading some new TLKs: \(ckksKeys, privacy: .public)")

        self.moc.performAndWait {
            self.onqueueUpdateTLKs(ckksKeys: ckksKeys, tlkShares: tlkShares, reply: reply)
        }
    }

    func onqueueUpdateTLKs(ckksKeys: [CKKSKeychainBackedKeySet],
                           tlkShares: [CKKSTLKShare],
                           reply: @escaping ([CKRecord]?, Error?) -> Void) {
        guard let egoPeerID = self.containerMO.egoPeerID,
            let egoPermData = self.containerMO.egoPeerPermanentInfo,
            let egoPermSig = self.containerMO.egoPeerPermanentInfoSig
            else {
                logger.debug("Have no self identity, can't make tlk shares")
                reply(nil, ContainerError.noPreparedIdentity)
                return
        }

        guard let selfPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: egoPermData, sig: egoPermSig, keyFactory: TPECPublicKeyFactory()) else {
            logger.debug("Couldn't parse self identity: \(String(describing: ckksKeys), privacy: .public)")
            reply(nil, ContainerError.invalidPermanentInfoOrSig)
            return
        }

        loadEgoKeys(peerID: egoPeerID) { egoPeerKeys, error in
            guard let egoPeerKeys = egoPeerKeys else {
                logger.debug("Don't have my own peer keys; can't upload new TLKs: \(String(describing: error), privacy: .public)")
                reply(nil, error)
                return
            }
            self.moc.performAndWait {
                guard let egoPeerDynamicInfo = self.model.getDynamicInfoForPeer(withID: egoPeerID) else {
                    logger.debug("Unable to fetch dynamic info for self")
                    reply(nil, ContainerError.missingDynamicInfo)
                    return
                }

                let allTLKShares: [TLKShare]
                let viewKeys: [ViewKeys]
                do {
                    (viewKeys, allTLKShares) = try self.makeSharesForNewKeySets(ckksKeys: ckksKeys,
                                                                                tlkShares: tlkShares,
                                                                                egoPeerKeys: egoPeerKeys,
                                                                                egoPeerDynamicInfo: egoPeerDynamicInfo,
                                                                                epoch: Int(selfPermanentInfo.epoch))
                } catch {
                    logger.debug("Unable to process keys before uploading: \(String(describing: error), privacy: .public)")
                    reply(nil, error)
                    return
                }

                let request = UpdateTrustRequest.with {
                    $0.changeToken = self.containerMO.changeToken ?? ""
                    $0.peerID = egoPeerID
                    $0.tlkShares = allTLKShares
                    $0.viewKeys = viewKeys
                }

                self.cuttlefish.updateTrust(request) { response in
                    switch response {
                    case .success(let response):
                        let keyHierarchyRecords = response.zoneKeyHierarchyRecords.compactMap { CKRecord($0) }
                        logger.debug("Received \(keyHierarchyRecords.count) CKRecords back")
                        reply(keyHierarchyRecords, nil)
                    case .failure(let error):
                        reply(nil, error)
                        return
                    }
                }
            }
        }
    }

    func getState(reply: @escaping (ContainerState) -> Void) {
        self.semaphore.wait()
        let reply: (ContainerState) -> Void = {
            logger.info("getState complete: \(String(describing: $0.egoPeerID), privacy: .public)")
            self.semaphore.signal()
            reply($0)
        }

        self.moc.performAndWait {
            var state = ContainerState()
            state.egoPeerID = self.containerMO.egoPeerID

            if self.containerMO.bottles != nil {
                self.containerMO.bottles!.forEach { bottle in
                    state.bottles.insert(ContainerState.Bottle(bottleMO: bottle as! BottleMO))
                }
            }

            if self.containerMO.fullyViableEscrowRecords != nil {
                self.containerMO.fullyViableEscrowRecords!.forEach { record in
                    state.escrowRecords.insert(ContainerState.EscrowRecord(escrowRecordMO: record as! EscrowRecordMO))
                }
            }

            if self.containerMO.partiallyViableEscrowRecords != nil {
                self.containerMO.partiallyViableEscrowRecords!.forEach { record in
                    state.escrowRecords.insert(ContainerState.EscrowRecord(escrowRecordMO: record as! EscrowRecordMO))
                }
            }

            self.model.allPeers().forEach { peer in
                state.peers[peer.peerID] = peer
            }
            state.vouchers = Array(self.model.allVouchers())
            reply(state)
        }
    }

    // This will only fetch changes if no changes have ever been fetched before
    func fetchAndPersistChangesIfNeeded(reply: @escaping (Error?) -> Void) {
        self.moc.performAndWait {
            if self.containerMO.changeToken == nil {
                self.onqueueFetchAndPersistChanges(forceRefetch: false, reply: reply)
            } else {
                reply(nil)
            }
        }
    }

    func fetchAndPersistChanges(forceRefetch: Bool = false, reply: @escaping (Error?) -> Void) {
        self.moc.performAndWait {

            let causeRefetch: Bool
            if forceRefetch {
                logger.debug("Forcing a full refetch: by request")
                causeRefetch = true
            } else {
                let modelRefetch = self.model.currentStatePossiblyMissingData()

                if modelRefetch && (self.containerMO.refetchLevel < Container.currentRefetchLevel) {
                    logger.debug("Forcing a full refetch due to model: last refetch level: \(self.containerMO.refetchLevel)")
                    causeRefetch = true
                } else if modelRefetch {
                    logger.debug("Model would like a full refetch, but we've done one at this refetch level: \(self.containerMO.refetchLevel)")
                    causeRefetch = false
                } else {
                    causeRefetch = false
                }
            }

            self.onqueueFetchAndPersistChanges(forceRefetch: causeRefetch, reply: reply)
        }
    }

    private func onqueueFetchAndPersistChanges(forceRefetch: Bool = false, reply: @escaping (Error?) -> Void) {
        let request = FetchChangesRequest.with {
            if !forceRefetch {
                $0.changeToken = self.containerMO.changeToken ?? ""
            }
        }
        logger.debug("Fetching with change token: \(!request.changeToken.isEmpty ? request.changeToken : "empty", privacy: .public)")

        self.cuttlefish.fetchChanges(request) { response in
            switch response {
            case .success(let response):
                do {
                    try self.persist(changes: response.changes, refetchForced: forceRefetch)
                } catch {
                    logger.debug("Could not persist changes: \(String(describing: error), privacy: .public)")
                    reply(error)
                    return
                }

                if response.changes.more {
                    logger.debug("persist: More changes indicated. Fetching...")
                    self.fetchAndPersistChanges(reply: reply)
                    return
                } else {
                    logger.debug("persist: no more changes!")
                    reply(nil)
                }
            case .failure(let error):
                switch error {
                case CuttlefishErrorMatcher(code: CuttlefishErrorCode.changeTokenExpired):
                    logger.debug("change token is expired; resetting local CK storage")

                    self.moc.performAndWait {
                        do {
                            try self.deleteLocalCloudKitData()
                        } catch {
                            logger.debug("Failed to reset local data: \(String(describing: error), privacy: .public)")
                            reply(error)
                            return
                        }

                        self.fetchAndPersistChanges(reply: reply)
                    }

                    return
                default:
                    logger.debug("Fetch error is an unknown error: \(String(describing: error), privacy: .public)")
                }

                logger.debug("Could not fetch changes: \(String(describing: error), privacy: .public)")
                reply(error)
                return
            }
        }
    }

    //
    // Check for delta update in trust lists, that should lead to update of TLKShares
    //
    private func haveTrustMemberChanges(newDynamicInfo: TPPeerDynamicInfo, oldDynamicInfo: TPPeerDynamicInfo?) -> Bool {
        guard let oldDynamicInfo = oldDynamicInfo else {
            return true
        }
        if newDynamicInfo.includedPeerIDs != oldDynamicInfo.includedPeerIDs {
            return true
        }
        if newDynamicInfo.excludedPeerIDs != oldDynamicInfo.excludedPeerIDs {
            return true
        }
        if newDynamicInfo.preapprovals != oldDynamicInfo.preapprovals {
            return true
        }
        return false
    }

    // Fetch and persist changes from Cuttlefish. If this results
    // in us calculating a new dynamicInfo then give the new dynamicInfo
    // to Cuttlefish. If Cuttlefish returns more changes then persist
    // them locally, update dynamicInfo if needed, and keep doing that
    // until dynamicInfo is unchanged and there are no more changes to fetch.
    //
    // Must be holding the semaphore to call this, and it remains
    // the caller's responsibility to release it after it completes
    // (i.e. after reply is invoked).
    internal func fetchChangesAndUpdateTrustIfNeeded(forceRefetch: Bool = false,
                                                     stableChanges: StableChanges? = nil,
                                                     peerChanges: Bool = false,
                                                     reply: @escaping (TrustedPeersHelperPeerState?, TPSyncingPolicy?, Error?) -> Void) {
        self.fetchAndPersistChanges(forceRefetch: forceRefetch) { error in
            if let error = error {
                logger.debug("fetchChangesAndUpdateTrustIfNeeded: fetching failed: \(String(describing: error), privacy: .public)")
                reply(nil, nil, error)
                return
            }

            self.updateTrustIfNeeded(stableChanges: stableChanges, peerChanges: peerChanges, reply: reply)
        }
    }

    // If this results in us calculating a new dynamicInfo then,
    // upload the new dynamicInfo
    // to Cuttlefish. If Cuttlefish returns more changes then persist
    // them locally, update dynamicInfo if needed, and keep doing that
    // until dynamicInfo is unchanged and there are no more changes to fetch.
    //
    // Must be holding the semaphore to call this, and it remains
    // the caller's responsibility to release it after it completes
    // (i.e. after reply is invoked).
    private func updateTrustIfNeeded(stableChanges: StableChanges? = nil,
                                     peerChanges: Bool = false,
                                     reply: @escaping (TrustedPeersHelperPeerState?, TPSyncingPolicy?, Error?) -> Void) {
        self.moc.performAndWait {
            guard let egoPeerID = self.containerMO.egoPeerID else {
                // No identity, nothing to do
                logger.debug("updateTrustIfNeeded: No identity.")
                reply(TrustedPeersHelperPeerState(peerID: nil, isPreapproved: false, status: .unknown, memberChanges: peerChanges, unknownMachineIDs: false, osVersion: nil),
                      nil,
                      nil)
                return
            }
            loadEgoKeyPair(identifier: signingKeyIdentifier(peerID: egoPeerID)) { signingKeyPair, error in
                guard let signingKeyPair = signingKeyPair else {
                    logger.debug("updateTrustIfNeeded: no signing key pair: \(String(describing: error), privacy: .public)")
                    reply(TrustedPeersHelperPeerState(peerID: nil, isPreapproved: false, status: .unknown, memberChanges: peerChanges, unknownMachineIDs: false, osVersion: nil),
                          nil,
                          error)
                    return
                }
                guard let currentSelfInModel = self.model.peer(withID: egoPeerID) else {
                    // Not in circle, nothing to do
                    let isPreapproved = self.model.hasPotentiallyTrustedPeerPreapprovingKey(signingKeyPair.publicKey().spki())
                    logger.debug("updateTrustIfNeeded: ego peer is not in model, is \(isPreapproved ? "preapproved" : "not yet preapproved", privacy: .public)")
                    reply(TrustedPeersHelperPeerState(peerID: egoPeerID,
                                                      isPreapproved: isPreapproved,
                                                      status: .unknown,
                                                      memberChanges: peerChanges,
                                                      unknownMachineIDs: false,
                                                      osVersion: nil),
                          nil,
                          nil)
                    return
                }

                // We need to try to have all policy versions that our peers claim to behave
                let allPolicyVersions = self.model.allPolicyVersions()
                self.fetchPolicyDocumentsWithSemaphore(versions: allPolicyVersions) { _, policyFetchError in
                    if let error = policyFetchError {
                        logger.debug("updateTrustIfNeeded: error fetching all requested policies (continuing anyway): \(String(describing: error), privacy: .public)")
                    }

                    self.moc.performAndWait {
                        let dynamicInfo: TPPeerDynamicInfo
                        var stableInfo: TPPeerStableInfo?
                        do {
                            // FIXME We should be able to calculate the contents of dynamicInfo without the signingKeyPair,
                            // and then only load the key if it has changed and we need to sign a new one. This would also
                            // help make our detection of change immune from non-canonical serialization of dynamicInfo.
                            dynamicInfo = try self.model.calculateDynamicInfoForPeer(withID: egoPeerID,
                                                                                     addingPeerIDs: nil,
                                                                                     removingPeerIDs: nil,
                                                                                     preapprovedKeys: nil,
                                                                                     signing: signingKeyPair,
                                                                                     currentMachineIDs: self.onqueueCurrentMIDList())

                            stableInfo = try self.createNewStableInfoIfNeeded(stableChanges: stableChanges,
                                                                              permanentInfo: currentSelfInModel.permanentInfo,
                                                                              existingStableInfo: currentSelfInModel.stableInfo,
                                                                              dynamicInfo: dynamicInfo,
                                                                              signingKeyPair: signingKeyPair)
                        } catch {
                            logger.debug("updateTrustIfNeeded: couldn't calculate dynamic info: \(String(describing: error), privacy: .public)")
                            reply(TrustedPeersHelperPeerState(peerID: egoPeerID,
                                                              isPreapproved: false,
                                                              status: self.model.statusOfPeer(withID: egoPeerID),
                                                              memberChanges: peerChanges,
                                                              unknownMachineIDs: false,
                                                              osVersion: nil),
                                  nil,
                                  error)
                            return
                        }

                        logger.debug("updateTrustIfNeeded: produced a stableInfo: \(String(describing: stableInfo), privacy: .public)")
                        logger.debug("updateTrustIfNeeded: produced a dynamicInfo: \(String(describing: dynamicInfo), privacy: .public)")

                        let peer = self.model.peer(withID: egoPeerID)
                        if (stableInfo == nil || stableInfo == peer?.stableInfo) &&
                            dynamicInfo == peer?.dynamicInfo {
                            logger.debug("updateTrustIfNeeded: complete.")
                            // No change to the dynamicInfo: update the MID list now that we've reached a steady state
                            do {
                                self.onqueueUpdateMachineIDListFromModel(dynamicInfo: dynamicInfo)
                                try self.moc.save()
                            } catch {
                                logger.debug("updateTrustIfNeeded: unable to remove untrusted MachineIDs: \(String(describing: error), privacy: .public)")
                            }

                            let syncingPolicy: TPSyncingPolicy?
                            do {
                                if let peer = self.model.peer(withID: egoPeerID), let stableInfo = peer.stableInfo {
                                    syncingPolicy = try self.syncingPolicyFor(modelID: peer.permanentInfo.modelID, stableInfo: stableInfo)
                                } else {
                                    syncingPolicy = nil
                                }
                            } catch {
                                logger.debug("updateTrustIfNeeded: unable to compute a new syncing policy: \(String(describing: error), privacy: .public)")
                                syncingPolicy = nil
                            }

                            reply(TrustedPeersHelperPeerState(peerID: egoPeerID,
                                                              isPreapproved: false,
                                                              status: self.model.statusOfPeer(withID: egoPeerID),
                                                              memberChanges: peerChanges,
                                                              unknownMachineIDs: self.onqueueFullIDMSListWouldBeHelpful(),
                                                              osVersion: peer?.stableInfo?.osVersion),
                                  syncingPolicy,
                                  nil)
                            return
                            }

                        // Check if we change that should trigger a notification that should trigger TLKShare updates
                        let havePeerChanges = peerChanges || self.haveTrustMemberChanges(newDynamicInfo: dynamicInfo, oldDynamicInfo: peer?.dynamicInfo)

                        let signedDynamicInfo = SignedPeerDynamicInfo(dynamicInfo)
                        logger.debug("updateTrustIfNeeded: attempting updateTrust for \(egoPeerID, privacy: .public) with: \(dynamicInfo, privacy: .public)")
                        var request = UpdateTrustRequest.with {
                            $0.changeToken = self.containerMO.changeToken ?? ""
                            $0.peerID = egoPeerID
                            $0.dynamicInfoAndSig = signedDynamicInfo
                        }
                        if let stableInfo = stableInfo {
                            request.stableInfoAndSig = SignedPeerStableInfo(stableInfo)
                        }

                        self.perform(updateTrust: request, stableChanges: stableChanges, peerChanges: havePeerChanges, reply: reply)
                    }
                }
            }
        }
    }

    private func perform(updateTrust request: UpdateTrustRequest,
                         stableChanges: StableChanges? = nil,
                         peerChanges: Bool = false,
                         reply: @escaping (TrustedPeersHelperPeerState?, TPSyncingPolicy?, Error?) -> Void) {
        self.cuttlefish.updateTrust(request) { response in
            switch response {
            case .success(let response):
                do {
                    try self.persist(changes: response.changes)
                } catch {
                    logger.debug("UpdateTrust failed: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, error)
                    return
                }

                if response.changes.more {
                    self.fetchChangesAndUpdateTrustIfNeeded(stableChanges: stableChanges,
                                                            peerChanges: peerChanges,
                                                            reply: reply)
                } else {
                    self.updateTrustIfNeeded(stableChanges: stableChanges,
                                             peerChanges: peerChanges,
                                             reply: reply)
                }
            case .failure(let error):
                logger.debug("UpdateTrust failed: \(String(describing: error), privacy: .public)")
                reply(nil, nil, error)
                return
            }
        }
    }

    private func persist(changes: Changes, refetchForced: Bool = false) throws {
        // This is some nonsense: I can't figure out how to tell swift to throw an exception across performAndWait.
        // So, do it ourself
        var outsideBlockError: Error?

        self.moc.performAndWait {
            logger.debug("persist: Received \(changes.differences.count) peer differences, more: \(changes.more)")
            logger.debug("persist: New change token: \(changes.changeToken, privacy: .public)")

            do {
                try self.onQueuePersist(changes: changes, refetchForced: refetchForced)
            } catch {
                outsideBlockError = error
            }
        }

        if let outsideBlockError = outsideBlockError {
            throw outsideBlockError
        }
    }

    // Must be on moc queue to call this.
    // Changes are registered in the model and stored in moc.
    private func onQueuePersist(changes: Changes, refetchForced: Bool = false) throws {
        self.containerMO.changeToken = changes.changeToken
        self.containerMO.moreChanges = changes.more

        if refetchForced {
            self.containerMO.refetchLevel = Container.currentRefetchLevel
        }

        if !changes.differences.isEmpty {
            logger.debug("escrow cache and viable bottles are no longer valid")
            self.onQueueRemoveEscrowCache()

            self.darwinNotifier.post(OTCliqueChanged)
        }

        try changes.differences.forEach { peerDifference in
            if let operation = peerDifference.operation {
                switch operation {
                case .add(let peer), .update(let peer):
                    try self.addOrUpdate(peer: peer)
                    // Update containerMO ego data if it has changed.
                    if peer.peerID == self.containerMO.egoPeerID {
                        guard let stableInfoAndSig: TPPeerStableInfo = peer.stableInfoAndSig.toStableInfo() else {
                            break
                        }
                        self.containerMO.egoPeerStableInfo = stableInfoAndSig.data
                        self.containerMO.egoPeerStableInfoSig = stableInfoAndSig.sig
                    }

                case .remove(let peer):
                    self.model.deletePeer(withID: peer.peerID)
                    if let peerMO = try self.fetchPeerMO(peerID: peer.peerID) {
                        self.moc.delete(peerMO)
                    }
                }
            }
        }

        let signingKey = changes.recoverySigningPubKey
        let encryptionKey = changes.recoveryEncryptionPubKey

        if !signingKey.isEmpty && !encryptionKey.isEmpty {
            self.addOrUpdate(signingKey: signingKey, encryptionKey: encryptionKey)
        }

        // Don't print this if there's more changes coming; the logs will be very confusing
        if !changes.more {
            logger.debug("Currently know about \(self.model.allPeerIDs().count) peers")
        }

        try self.moc.save()
    }

    // Must be on moc queue to call this
    // Deletes all things that should come back from CloudKit. Keeps the egoPeer data, though.
    private func deleteLocalCloudKitData() throws {
        logger.debug("Deleting all CloudKit data")

        do {
            let peerRequest = NSFetchRequest<NSFetchRequestResult>(entityName: "Peer")
            peerRequest.predicate = NSPredicate(format: "container == %@", self.containerMO)
            try self.moc.execute(NSBatchDeleteRequest(fetchRequest: peerRequest))

            // If we have an ego peer ID, keep the bottle associated with it
            if let peerID = self.containerMO.egoPeerID, let bottles = self.containerMO.bottles {
                let nonPeerBottles = NSSet(array: bottles.filter {
                    switch $0 {
                    case let bottleMO as BottleMO:
                        return bottleMO.peerID != peerID
                    default:
                        return false
                    }
                })
                self.containerMO.removeFromBottles(nonPeerBottles as NSSet)
            } else {
                let bottleRequest = NSFetchRequest<NSFetchRequestResult>(entityName: "Bottle")
                bottleRequest.predicate = NSPredicate(format: "container == %@", self.containerMO)
                try self.moc.execute(NSBatchDeleteRequest(fetchRequest: bottleRequest))
                self.containerMO.bottles = nil
            }

            self.containerMO.peers = nil
            self.containerMO.changeToken = nil
            self.containerMO.moreChanges = false

            self.model = Container.loadModel(from: self.containerMO)
            try self.moc.save()
        } catch {
            logger.debug("Local delete failed: \(String(describing: error), privacy: .public)")
            throw error
        }

        logger.debug("Saved model with \(self.model.allPeerIDs().count) peers")
    }

    // Must be on moc queue to call this.
    private func addOrUpdate(signingKey: Data, encryptionKey: Data) {
        self.model.setRecoveryKeys(
            TPRecoveryKeyPair(signingKeyData: signingKey, encryptionKeyData: encryptionKey))

        self.containerMO.recoveryKeySigningSPKI = signingKey
        self.containerMO.recoveryKeyEncryptionSPKI = encryptionKey
    }

    // Must be on moc queue to call this.
    private func addOrUpdateCRK(peer: Peer) throws {
        guard let crk = peer.custodianRecoveryKeyAndSig.toCustodianRecoveryKey() else {
            logger.debug("failed to parse custodian recovery key")
            return
        }
        logger.debug("Register CRK with peerID \(peer.peerID, privacy: .public)")
        self.model.register(crk)
        let crkMO = CustodianRecoveryKeyMO(context: self.moc)
        crkMO.crkInfo = crk.data
        crkMO.crkInfoSig = crk.sig
        self.containerMO.addToCustodianRecoveryKeys(crkMO)
    }

    // Must be on moc queue to call this.
    private func addOrUpdate(peer: Peer) throws {
        if !self.model.hasPeer(withID: peer.peerID) {
            // Add:
            guard let permanentInfo = peer.permanentInfoAndSig.toPermanentInfo(peerID: peer.peerID) else {
                if peer.hasCustodianRecoveryKeyAndSig {
                    if self.testIgnoreCustodianUpdates {
                        logger.debug("Ignoring Custodian update due to test request: \(String(describing: peer), privacy: .public)")
                    } else {
                        try self.addOrUpdateCRK(peer: peer)
                    }
                }
                // Ignoring bad peer
                return
            }
            let stableInfo = peer.stableInfoAndSig.toStableInfo()
            let dynamicInfo = peer.dynamicInfoAndSig.toDynamicInfo()
            let vouchers = peer.vouchers.compactMap {
                TPVoucher(infoWith: $0.voucher, sig: $0.sig)
            }
            let isEgoPeer = peer.peerID == self.containerMO.egoPeerID
            try self.registerPeerMO(permanentInfo: permanentInfo,
                                    stableInfo: stableInfo,
                                    dynamicInfo: dynamicInfo,
                                    vouchers: vouchers,
                                    isEgoPeer: isEgoPeer)
        } else {
            // Update:
            // The assertion here is that every peer registered in model is also present in containerMO
            guard let peerMO = try self.fetchPeerMO(peerID: peer.peerID) else {
                throw ContainerError.peerRegisteredButNotStored(peer.peerID)
            }

            if let stableInfo = peer.stableInfoAndSig.toStableInfo() {
                try self.model.update(stableInfo, forPeerWithID: peer.peerID)
                // Pull the stableInfo back out of the model, and persist that.
                // The model checks signatures and clocks to prevent replay attacks.
                let modelPeer = self.model.peer(withID: peer.peerID)
                peerMO.stableInfo = modelPeer?.stableInfo?.data
                peerMO.stableInfoSig = modelPeer?.stableInfo?.sig
            }
            if let dynamicInfo = peer.dynamicInfoAndSig.toDynamicInfo() {
                try self.model.update(dynamicInfo, forPeerWithID: peer.peerID)
                // Pull the dynamicInfo back out of the model, and persist that.
                // The model checks signatures and clocks to prevent replay attacks.
                let modelPeer = self.model.peer(withID: peer.peerID)
                peerMO.dynamicInfo = modelPeer?.dynamicInfo?.data
                peerMO.dynamicInfoSig = modelPeer?.dynamicInfo?.sig
            }
            peer.vouchers.forEach {
                if let voucher = TPVoucher(infoWith: $0.voucher, sig: $0.sig) {
                    self.model.register(voucher)
                    if !(peer.vouchers.contains { $0.voucher == voucher.data && $0.sig == voucher.sig }) {
                        let voucherMO = VoucherMO(context: self.moc)
                        voucherMO.voucherInfo = voucher.data
                        voucherMO.voucherInfoSig = voucher.sig
                        peerMO.addToVouchers(voucherMO)
                    }
                }
            }
        }
    }

    // Must be on moc queue to call this.
    private func fetchPeerMO(peerID: String) throws -> PeerMO? {
        let fetch = NSFetchRequest<NSFetchRequestResult>(entityName: "Peer")
        fetch.predicate = NSPredicate(format: "peerID == %@ && container == %@", peerID, self.containerMO)
        let peers = try self.moc.fetch(fetch)
        return peers.first as? PeerMO
    }

    // Must be on moc queue to call this.
    private func getPolicyDoc(_ policyVersion: UInt64) throws -> TPPolicyDocument {
        guard let policyDoc = self.model.policy(withVersion: policyVersion) else {
            throw ContainerError.unknownPolicyVersion(policyVersion)
        }
        assert(policyVersion == policyDoc.version.versionNumber)
        if policyVersion == prevailingPolicyVersion.versionNumber {
            assert(policyDoc.version.policyHash == prevailingPolicyVersion.policyHash)
        }
        return policyDoc
    }

    // Must be on moc queue to call this.
    private func createNewStableInfoIfNeeded(stableChanges: StableChanges?,
                                             permanentInfo: TPPeerPermanentInfo,
                                             existingStableInfo: TPPeerStableInfo?,
                                             dynamicInfo: TPPeerDynamicInfo,
                                             signingKeyPair: _SFECKeyPair) throws -> TPPeerStableInfo? {
        func noChange<T: Equatable>(_ change: T?, _ existing: T?) -> Bool {
            return (nil == change) || change == existing
        }

        let policyOfPeers = try? self.model.policy(forPeerIDs: dynamicInfo.includedPeerIDs,
                                                   candidatePeerID: permanentInfo.peerID,
                                                   candidateStableInfo: existingStableInfo)

        // Pick the best version of:
        //   1. The policy version asked for by the client
        //   2. The policy override set on this object (tests only)
        //   3. The max of our existing policyVersion, the highest policy used by our trusted peers, and the compile-time prevailing policy version
        let optimalPolicyVersionNumber = stableChanges?.policyVersion ??
                self.policyVersionOverride?.versionNumber ??
                max(existingStableInfo?.bestPolicyVersion().versionNumber ?? prevailingPolicyVersion.versionNumber,
                    policyOfPeers?.version.versionNumber ?? prevailingPolicyVersion.versionNumber,
                    prevailingPolicyVersion.versionNumber)

        // Determine which recovery key we'd like to be using, given our current idea of who to trust
        let optimalRecoveryKey = self.model.bestRecoveryKey(for: existingStableInfo, dynamicInfo: dynamicInfo)


        let intendedSyncUserControllableViews = stableChanges?.setSyncUserControllableViews?.sanitizeForPlatform(permanentInfo: permanentInfo)

        let intendedSecureElementIdentity: TPPBSecureElementIdentity?
        if let identitySet = stableChanges?.secureElementIdentity {
            // note: this might be nil. If so, it represents an affirmative request to delete the secureElementIdentity.
            intendedSecureElementIdentity = identitySet.secureElementIdentity
        } else {
            // the stable changes does not have an opinion; just use what's there
            intendedSecureElementIdentity = existingStableInfo?.secureElementIdentity
        }

        var noChanges: Bool
        noChanges = noChange(stableChanges?.deviceName, existingStableInfo?.deviceName) &&
            noChange(stableChanges?.serialNumber, existingStableInfo?.serialNumber) &&
            noChange(stableChanges?.osVersion, existingStableInfo?.osVersion) &&
            noChange(optimalPolicyVersionNumber, existingStableInfo?.bestPolicyVersion().versionNumber) &&
            noChange(stableChanges?.policySecrets, existingStableInfo?.policySecrets) &&
            noChange(optimalRecoveryKey?.signingKeyData, existingStableInfo?.recoverySigningPublicKey) &&
            noChange(optimalRecoveryKey?.encryptionKeyData, existingStableInfo?.recoveryEncryptionPublicKey) &&
            noChange(intendedSyncUserControllableViews, existingStableInfo?.syncUserControllableViews)
        noChanges = noChanges && (intendedSecureElementIdentity == existingStableInfo?.secureElementIdentity)
        if noChanges {
            return nil
        }

        // If a test has asked a policy version before we froze this policy, then don't set a flexible version--it's trying to build a peer from before the policy was frozen
        let optimalPolicyVersion = try self.getPolicyDoc(optimalPolicyVersionNumber).version
        let useFrozenPolicyVersion = optimalPolicyVersion.versionNumber >= frozenPolicyVersion.versionNumber

        if let intendedSyncUserControllableViews = intendedSyncUserControllableViews {
            logger.debug("Intending to set user-controllable views to \(TPPBPeerStableInfoUserControllableViewStatusAsString(intendedSyncUserControllableViews), privacy: .public)")
        }

        return try self.model.createStableInfo(withFrozenPolicyVersion: useFrozenPolicyVersion ? frozenPolicyVersion : optimalPolicyVersion,
                                               flexiblePolicyVersion: useFrozenPolicyVersion ? optimalPolicyVersion : nil,
                                               policySecrets: stableChanges?.policySecrets ?? existingStableInfo?.policySecrets,
                                               syncUserControllableViews: intendedSyncUserControllableViews ?? existingStableInfo?.syncUserControllableViews ?? .UNKNOWN,
                                               secureElementIdentity: intendedSecureElementIdentity,
                                               deviceName: stableChanges?.deviceName ?? existingStableInfo?.deviceName ?? "",
                                               serialNumber: stableChanges?.serialNumber ?? existingStableInfo?.serialNumber ?? "",
                                               osVersion: stableChanges?.osVersion ?? existingStableInfo?.osVersion ?? "",
                                               signing: signingKeyPair,
                                               recoverySigningPubKey: optimalRecoveryKey?.signingKeyData,
                                               recoveryEncryptionPubKey: optimalRecoveryKey?.encryptionKeyData,
                                               isInheritedAccount: existingStableInfo?.isInheritedAccount ?? false)
    }

    private func assembleBottle(egoPeerID: String) throws -> Bottle {
        let bottleMoSet = containerMO.bottles as? Set<BottleMO>

        var bottleMOs = bottleMoSet?.filter {
            $0.peerID == egoPeerID
        }

        if let count = bottleMOs?.count {
            if count > 1 {
                throw ContainerError.tooManyBottlesForPeer
                // swiftlint:disable empty_count
            } else if count == 0 {
                // swiftlint:enable empty_count
                throw ContainerError.noBottleForPeer
            }
        } else {
            throw ContainerError.failedToAssembleBottle
        }

        let BM: BottleMO? = (bottleMOs?.removeFirst())

        let bottle = try Bottle.with {
            $0.peerID = egoPeerID

            if let bottledPeerData = BM?.contents {
                $0.contents = bottledPeerData
            } else {
                throw ContainerError.failedToAssembleBottle
            }

            if let bottledPeerEscrowSigningSPKI = BM?.escrowedSigningSPKI {
                $0.escrowedSigningSpki = bottledPeerEscrowSigningSPKI
            } else {
                throw ContainerError.failedToAssembleBottle
            }

            if let bottledPeerSignatureUsingEscrowKey = BM?.signatureUsingEscrowKey {
                $0.signatureUsingEscrowKey = bottledPeerSignatureUsingEscrowKey
            } else {
                throw ContainerError.failedToAssembleBottle
            }

            if let bottledPeerSignatureUsingPeerKey = BM?.signatureUsingPeerKey {
                $0.signatureUsingPeerKey = bottledPeerSignatureUsingPeerKey
            } else {
                throw ContainerError.failedToAssembleBottle
            }

            if let bID = BM?.bottleID {
                $0.bottleID = bID
            } else {
                throw ContainerError.failedToAssembleBottle
            }
        }

        return bottle
    }

    func reportHealth(request: ReportHealthRequest, reply: @escaping (Error?) -> Void) {
        self.semaphore.wait()
        let reply: (Error?) -> Void = {
            logger.info("reportHealth complete \(traceError($0), privacy: .public)")
            self.semaphore.signal()
            reply($0)
        }

        var updatedRequest = request

        if let egoPeerID = self.containerMO.egoPeerID {
            loadEgoKeys(peerID: egoPeerID) { egoPeerKeys, _ in
                updatedRequest.octagonEgoIdentity = (egoPeerKeys != nil)
            }
        }

        self.moc.performAndWait {
            self.cuttlefish.reportHealth(updatedRequest) { response in
                switch response {
                case .success:
                    reply(nil)
                case .failure(let error):
                    reply(error)
                    return
                }
            }
        }
    }

    func pushHealthInquiry(reply: @escaping (Error?) -> Void) {
        self.semaphore.wait()
        let reply: (Error?) -> Void = {
            logger.info("reportHealth complete \(traceError($0), privacy: .public)")
            self.semaphore.signal()
            reply($0)
        }

        self.moc.performAndWait {
            self.cuttlefish.pushHealthInquiry(PushHealthInquiryRequest()) { response in
                switch response {
                case .success:
                    reply(nil)
                case .failure(let error):
                    reply(error)
                    return
                }
            }
        }
    }
}
