/*
 * Copyright (c) 2018 - 2023 Apple Inc. All Rights Reserved.
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
    case invalidVoucherOrSig
    case invalidStableInfoOrSig
    case sponsorNotRegistered(String)
    case unknownPolicyVersion(UInt64)
    case preparedIdentityNotOnAllowedList(String)
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
    case couldNotLoadAllowedList
    case failedToCreateRecoveryKey(suberror: Error)
    case untrustedRecoveryKeys
    case noBottlesPresent
    case recoveryKeysNotEnrolled
    case bottleCreatingPeerNotFound
    case unknownCloudKitError
    case cloudkitResponseMissing
    case failedToLoadSecret(errorCode: Int)
    case failedToLoadSecretDueToType
    case failedToStoreSecret(errorCode: Int)
    case failedToAssembleBottle
    case invalidPeerID
    case unknownSecurityFoundationError
    case failedToSerializeData(suberror: Error)
    case unknownInternalError
    case unknownSyncUserControllableViewsValue(value: Int32)
    case noPeersPreapprovedBySelf
    case peerRegisteredButNotStored(String)
    case configuredContainerDoesNotMatchSpecifiedUser(TPSpecificUser)
    case noSpecifiedUser
    case noEscrowCache
    case recoveryKeyIsNotCorrect
    case failedToGetPeerViews(suberror: Error)
    case cannotCreateRecoveryKeyPeer(suberror: Error)
    case custodianRecoveryKeyMalformed
    case operationNotImplemented
    case cannotDetermineTrustedPeerCount
    case custodianRecoveryKeyUUIDExists
    case generatingRandomFailed(errorCode: Int)
    case unableToCreateDirectory
    case duplicateMachineID
    case machineIDVanishedFromTDL
    case allowedMIDHashMismatch
    case deletedMIDHashMismatch
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
        case .invalidVoucherOrSig:
            return "invalid voucher or signature"
        case .invalidStableInfoOrSig:
            return "invalid stable info or signature"
        case .sponsorNotRegistered(let s):
            return "sponsor not registered: \(s)"
        case .unknownPolicyVersion(let v):
            return "unknown policy version: \(v)"
        case .preparedIdentityNotOnAllowedList(let id):
            return "prepared identity (\(id)) not on allowed machineID list"
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
        case .couldNotLoadAllowedList:
            return "could not load allowed machineID list"
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
        case .failedToStoreSecret(errorCode: let errorCode):
            return "failed to store the secret in the keychain \(errorCode)"
        case .failedToAssembleBottle:
            return "failed to assemble bottle for peer"
        case .invalidPeerID:
            return "peerID is invalid"
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
        case .configuredContainerDoesNotMatchSpecifiedUser(let user):
            return "Existing container configuration does not match user \(user)"
        case .noSpecifiedUser:
            return "No user specified"
        case .noEscrowCache:
            return "No escrow cache available"
        case .recoveryKeyIsNotCorrect:
            return "Preflighted recovery key is not correct"
        case .failedToGetPeerViews:
            return "failed to get peer views"
        case .cannotCreateRecoveryKeyPeer:
            return "failed to create recovery key peer"
        case .custodianRecoveryKeyMalformed:
            return "Custodian recovery key is malformed"
        case .operationNotImplemented:
            return "Operation not implemented"
        case .cannotDetermineTrustedPeerCount:
            return "peer count could not be determined"
        case .custodianRecoveryKeyUUIDExists:
            return "custodian recovery key UUID already exists"
        case .generatingRandomFailed(errorCode: let errorCode):
            return "generating random failed: \(errorCode)"
        case .unableToCreateDirectory:
            return "unable to create directory"
        case .duplicateMachineID:
            return "duplicateMachineIDDetected"
        case .machineIDVanishedFromTDL:
            return "machineIDVanishedFromTDL"
        case .allowedMIDHashMismatch:
            return "allowedMIDHashMismatch"
        case .deletedMIDHashMismatch:
            return "deletedMIDHashMismatch"
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
        case .configuredContainerDoesNotMatchSpecifiedUser:
            return 49
        case .noSpecifiedUser:
            return 50
        case .noEscrowCache:
            return 51
        case .recoveryKeyIsNotCorrect:
            return 52
        case .failedToGetPeerViews:
            return 53
        case .cannotCreateRecoveryKeyPeer:
            return 54
        case .custodianRecoveryKeyMalformed:
            return 55
        case .operationNotImplemented:
            return 56
        case .cannotDetermineTrustedPeerCount:
            return 57
        case .custodianRecoveryKeyUUIDExists:
            return 58
        case .generatingRandomFailed:
            return 59
        case .unableToCreateDirectory:
            return 60
        case .duplicateMachineID:
            return 61
        case .machineIDVanishedFromTDL:
            return 62
        case .allowedMIDHashMismatch:
            return 63
        case .deletedMIDHashMismatch:
            return 64
        }
    }

    public var underlyingError: NSError? {
        switch self {
        case .failedToCreateRecoveryKey(suberror: let error):
            return error as NSError
        case .failedToSerializeData(suberror: let error):
            return error as NSError
        case .failedToGetPeerViews(suberror: let error):
            return error as NSError
        case .cannotCreateRecoveryKeyPeer(suberror: let error):
            return error as NSError
        case .failedToLoadSecret(errorCode: let errorCode),
             .failedToStoreSecret(errorCode: let errorCode),
             .generatingRandomFailed(errorCode: let errorCode):
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

func loadSecret(label: String) throws -> Data? {
    var secret: Data?

    let query: [CFString: Any] = [
        kSecClass: kSecClassInternetPassword,
        kSecAttrAccessGroup: "com.apple.security.octagon",
        kSecAttrDescription: label,
        kSecReturnAttributes: true,
        kSecReturnData: true,
        kSecAttrSynchronizable: false,
        kSecUseDataProtectionKeychain: true,
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
    do {
        try keychainManager.setIdentity(signingIdentity, forIdentifier: identifier, accessPolicy: accessPolicy)
        resultHandler(true, nil)
    } catch {
        resultHandler(false, error)
    }
}

func loadEgoKeyPair(identifier: String, resultHandler: @escaping (_SFECKeyPair?, Error?) -> Void) {
    let keychainManager = _SFKeychainManager.default()

    // FIXME constrain to egoIdentitiesAccessGroup, <rdar://problem/39597940>
    let result = keychainManager.identity(forIdentifier: identifier)
    switch result?.resultType {
    case .valueAvailable:
        if let retResult = result {
            resultHandler(retResult.value?.keyPair as? _SFECKeyPair, nil)
        }
    case .needsAuthentication:
        resultHandler(nil, ContainerError.needsAuthentication)
    case .error:
        resultHandler(nil, result?.error)
    case .none:
        resultHandler(nil, result?.error)
    @unknown default:
        resultHandler(nil, ContainerError.unknownSecurityFoundationError)
    }
}

func loadEgoKeys(peerID: String, resultHandler: @escaping (OctagonSelfPeerKeys?, Error?) -> Void) {
    loadEgoKeyPair(identifier: signingKeyIdentifier(peerID: peerID)) { signingKey, error in
        guard let signingKey = signingKey else {
            logger.error("Unable to load signing key: \(String(describing: error), privacy: .public)")
            resultHandler(nil, error)
            return
        }

        loadEgoKeyPair(identifier: encryptionKeyIdentifier(peerID: peerID)) { encryptionKey, error in
            guard let encryptionKey = encryptionKey else {
                logger.error("Unable to load encryption key: \(String(describing: error), privacy: .public)")
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
        logger.info("Making TLKShare for \(String(describing: toPeer), privacy: .public) for key \(String(describing: tlk), privacy: .public)")
        // Not being able to convert a TLK to a TLKShare is a failure, but not having a TLK is only half bad
        do {
            return try CKKSTLKShare(tlk, as: asPeer, to: toPeer, epoch: epoch, poisoned: 0)
        } catch {
            let nserror = error as NSError
            if nserror.domain == "securityd" && nserror.code == errSecItemNotFound {
                logger.info("No TLK contents for \(String(describing: tlk), privacy: .public), no TLK share possible")
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
    logger.info("Attempting to recover \(tlkShares.count) shares for peer \(peer.peerID, privacy: .public)")

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

    let egoPeer: TPPeer?
    do {
        egoPeer = try model.peer(withID: sponsorPeerID ?? peer.peerID)
    } catch {
        logger.warning("Error getting ego peer from model: \(String(describing: error), privacy: .public)")
        egoPeer = nil
    }
    if let egoPeer {
        // We should accept TLKShares from any peer that this peer trusts, either via direct link or voucher.
        // Calculate a new dynamic info from the model for that peer (possibly with the wrong signature, but we don't need it to validate)
        do {
            let computedSponsorDynamicInfo = try model.calculateDynamicInfoForPeer(withID: egoPeer.peerID,
                                                                                   addingPeerIDs: nil,
                                                                                   removingPeerIDs: nil,
                                                                                   preapprovedKeys: nil,
                                                                                   signing: peer.signingKey,
                                                                                   currentMachineIDs: nil)
            logger.info("Using computed dynamic info for share recovery: \(computedSponsorDynamicInfo, privacy: .public)")

            computedSponsorDynamicInfo.includedPeerIDs.forEach { trustedPeerID in
                let peer: TPPeer?
                do {
                    peer = try model.peer(withID: trustedPeerID)
                } catch {
                    logger.warning("Error getting included peer (\(trustedPeerID)) from model: \(String(describing: error), privacy: .public)")
                    peer = nil
                }
                if let peer {
                    let peerObj = CKKSActualPeer(peerID: trustedPeerID,
                                                 encryptionPublicKey: (peer.permanentInfo.encryptionPubKey as! _SFECPublicKey),
                                                 signing: (peer.permanentInfo.signingPubKey as! _SFECPublicKey),
                                                 viewList: [])

                    trustedPeers.append(peerObj)
                } else {
                    logger.info("No peer for trusted ID \(trustedPeerID, privacy: .public)")
                }
            }
        } catch {
            logger.error("Unable to create dynamic info for share recovery: \(String(describing: error), privacy: .public)")
        }
    } else {
        logger.info("No ego peer in model; no trusted peers")
    }

    for share in tlkShares {
        guard share.receiverPeerID == peer.peerID else {
            logger.info("Skipping \(String(describing: share), privacy: .public) (wrong peerID)")
            continue
        }

        do {
            let key = try share.recoverTLK(peer,
                                           trustedPeers: Set(trustedPeers),
                                           ckrecord: nil)

            try key.saveMaterialToKeychain()

            tlksRecovered[key.uuid] = key
            sharesRecovered += 1
            logger.info("Recovered \(String(describing: key), privacy: .public) (from \(String(describing: share), privacy: .public)")
        } catch {
            logger.error("Failed to recover share \(String(describing: share), privacy: .public): \(String(describing: error), privacy: .public)")

            if let list = recoveryErrors[share.tlkUUID] {
                recoveryErrors[share.tlkUUID] = list + [error]
            } else {
                recoveryErrors[share.tlkUUID] = [error]
            }
        }
    }

    logger.info("Recovered TLKs: \(String(describing: tlksRecovered), privacy: .public)")

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
    var peerError: Error?
    var vouchers: [TPVoucher] = []
    var voucherError: Error?
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
    let walrusSetting: TPPBPeerStableInfoSetting?
    let webAccess: TPPBPeerStableInfoSetting?
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
class Container: NSObject, ConfiguredCloudKit {
    let name: ContainerName

    private let cuttlefish: ConfiguredCuttlefishAPIAsync

    // Only one request (from Client) is permitted to be in progress at a time.
    // That includes while waiting for network, i.e. one request must complete
    // before the next can begin. Otherwise two requests could in parallel be
    // fetching updates from Cuttlefish, with ensuing changeToken overwrites etc.
    // This applies for mutating requests -- requests that can only read the current
    // state (on the moc queue) do not need to.
    fileprivate let semaphore = DispatchSemaphore(value: 1)

    // For debugging hangs, current operation that has semaphore
    internal var operationWithSemaphore: String?

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
    private var dbAdapter: DBAdapter
    internal var escrowCacheTimeout: TimeInterval

    // Used in tests only. Set when an identity is prepared using a policy version override
    internal var policyVersionOverride: TPPolicyVersion?
    internal var testIgnoreCustodianUpdates: Bool = false
    internal var testDontSetAccountSetting: Bool? = false

    internal let darwinNotifier: CKKSNotifier.Type
    internal let managedConfigurationAdapter: OTManagedConfigurationAdapter

    var tlkSharesBatch = 1000

    // test variables
    var testEgoMachineIDVanished: Bool = false
    var testHashMismatchDetected: Bool = false

    // If you add a new field to the Cuttlefish Changes protocol, such that
    // old devices will ignore it silently, but still persist a change tag
    // beyond the data they ignored, consider increasing this level to cause
    // those devices to refetch all data once they upgrade to this software
    // (which persist the data as its refetched).
    //
    // Refetch level 1: Custodian peers
    internal static let currentRefetchLevel: Int64 = 1

    class SemaphoreWrapper {
        private var parent: Container
        private var function: String
        private var signaled: Bool = false

        init(parent: Container, function: String) {
            self.parent = parent
            self.function = function
            let timeoutInS = 60 * 30
            let timeout = DispatchTimeInterval.seconds(timeoutInS)
            switch parent.semaphore.wait(timeout: DispatchTime.now().advanced(by: timeout)) {
            case .success:
                break
            case .timedOut:
                logger.fault("Timeout after \(String(describing: timeout), privacy: .public) waiting for semaphore (held by \(String(describing: parent.operationWithSemaphore), privacy: .public))")
                SecABC.triggerAutoBugCapture(withType: "TrustedPeersHelper", subType: "hang-timeout", subtypeContext: parent.operationWithSemaphore ?? "", domain: "com.apple.security.keychain", events: nil, payload: nil, detectedProcess: nil)
                _exit(1)
            }
            parent.operationWithSemaphore = function
        }
        deinit {
            guard signaled else {
                logger.fault("Semaphore was not signaled by \(self.function, privacy: .public)")
                SecABC.triggerAutoBugCapture(withType: "TrustedPeersHelper", subType: "hang-semaphore-not-signaled", subtypeContext: self.function, domain: "com.apple.security.keychain", events: nil, payload: nil, detectedProcess: nil)
                _exit(1)
            }
        }
        func release(function: String = #function) {
            guard !signaled else {
                logger.fault("Semaphore double signaled by \(function, privacy: .public)")
                SecABC.triggerAutoBugCapture(withType: "TrustedPeersHelper", subType: "hang-semaphore-double-signaled", subtypeContext: function, domain: "com.apple.security.keychain", events: nil, payload: nil, detectedProcess: nil)
                _exit(1)
            }

            signaled = true
            parent.operationWithSemaphore = nil
            parent.semaphore.signal()
        }
    }

    // So that TPModel can read directly from the DB, rather than keeping a copy of everything internally
    class DBAdapter: TPModelDBAdapterProtocol {
        private let moc: NSManagedObjectContext
        private let containerMO: ContainerMO
        private var hmacKey: Data?

        init(moc: NSManagedObjectContext,
             containerMO: ContainerMO,
             hmacKey: Data?) {
            self.moc = moc
            self.containerMO = containerMO
            self.hmacKey = hmacKey
        }

        func getHmacKey() -> Data? {
            if let hmacKey = self.hmacKey {
                return hmacKey
            }

            let label = "HMACKey"
            do {
                if let ret = try loadSecret(label: label) {
                    self.hmacKey = ret
                    return self.hmacKey
                }
            } catch ContainerError.failedToLoadSecret(errorCode: Int(errSecInteractionNotAllowed)) {
                logger.info("getHmacKey: locked -- cannot return hmac")
                // but fall through and use a new key
            } catch ContainerError.failedToLoadSecret(errorCode: Int(errSecItemNotFound)) {
            } catch {
                logger.notice("getHmacKey: ignoring \(String(describing: error), privacy: .public)")
                // fall through and use a (likely) new key
            }
            var bytes = [UInt8](repeating: 0, count: Int(CC_SHA256_DIGEST_LENGTH))
            let status = SecRandomCopyBytes(kSecRandomDefault, bytes.count, &bytes)
            guard status == errSecSuccess else {
                logger.error("failed generating random bytes: \(status)")
                return nil
            }
            let key = Data(bytes)
            do {
                try saveSecret(key, label: label)
            } catch ContainerError.failedToStoreSecret(errorCode: Int(errSecInteractionNotAllowed)) {
                logger.info("getHmacKey: locked -- cannot save hmac")
                // but keep going
            } catch {
                logger.notice("getHmacKey: saveSecret failed: \(String(describing: error), privacy: .public)")
                // but keep going
            }
            self.hmacKey = key
            return self.hmacKey
        }

        func allPeerIDs() throws -> [String] {
            let fetch = NSFetchRequest<NSFetchRequestResult>(entityName: "Peer")
            fetch.predicate = NSPredicate(format: "container == %@", self.containerMO)
            fetch.propertiesToFetch = ["peerID"]
            do {
                let peers = try self.moc.fetch(fetch)
                let peerIDs = peers.compactMap { ($0 as? PeerMO)?.peerID }
                return peerIDs
            } catch {
                logger.error("Failed to fetch peers: \(String(describing: error), privacy: .public)")
                throw error
            }
        }

        func peerCount(_ errorOut: NSErrorPointer) -> UInt {
            let fetch = NSFetchRequest<NSFetchRequestResult>(entityName: "Peer")
            fetch.predicate = NSPredicate(format: "container == %@", self.containerMO)
            do {
                return UInt(try self.moc.count(for: fetch))
            } catch let error as NSError {
                logger.error("Failed to fetch peer count: \(String(describing: error), privacy: .public)")
                errorOut?.pointee = error
                return 0
            }
        }

        func saveIfNeeded() {
            if moc.hasChanges {
                do {
                    try moc.save()
                } catch {
                    logger.error("Failed to save: \(String(describing: error), privacy: .public)")
                }
            }
        }

        func peer(withID peerID: String, error errorOut: NSErrorPointer) -> TPPeer? {
            do {
                let ret = peerFromMO(peerMO: try fetchPeerMO(peerID: peerID))
                self.saveIfNeeded()
                return ret
            } catch let error as NSError {
                logger.error("Failed to fetch peerID \(peerID, privacy: .public): \(String(describing: error), privacy: .public)")
                errorOut?.pointee = error
                return nil
            }
        }

        func enumeratePeers(_ block: @escaping (TPPeer, UnsafeMutablePointer<ObjCBool>) -> Void) throws {
            let fetch = NSFetchRequest<NSFetchRequestResult>(entityName: "Peer")
            fetch.predicate = NSPredicate(format: "container == %@", self.containerMO)
            let batchSize = 25
            fetch.fetchBatchSize = batchSize
            do {
                var stop = ObjCBool(false)
                var i = 0
                for peer in try self.moc.fetch(fetch) {
                    autoreleasepool {
                        guard let peer = peerFromMO(peerMO: peer as? PeerMO) else {
                            return
                        }
                        block(peer, &stop)
                        i += 1
                        if i == batchSize {
                            self.saveIfNeeded()
                            i = 0
                        }
                    }
                    if stop.boolValue {
                        break
                    }
                }
                self.saveIfNeeded()
            } catch {
                logger.error("Failed to fetch peers for enumeration: \(String(describing: error), privacy: .public)")
                throw error
            }
        }

        internal static func allMachineIDs(containerMO: ContainerMO, moc: NSManagedObjectContext) throws -> Set<String> {
            let fetch = NSFetchRequest<NSFetchRequestResult>(entityName: "Machine")
            fetch.predicate = NSPredicate(format: "container == %@", containerMO)
            fetch.propertiesToFetch = ["machineID"]
            do {
                let machines = try moc.fetch(fetch)
                let machineIDs = Set(machines.compactMap { ($0 as? MachineMO)?.machineID })
                return machineIDs
            } catch {
                logger.error("Failed to fetch machineIDs: \(String(describing: error), privacy: .public)")
                throw error
            }
        }

        internal static func allMachineModifiedDatesFor(containerMO: ContainerMO, moc: NSManagedObjectContext, machineID: String) throws -> Set<MachineMO> {
            let fetch = NSFetchRequest<NSFetchRequestResult>(entityName: "Machine")
            fetch.predicate = NSPredicate(format: "machineID == %@ && container == %@", machineID, containerMO)
            fetch.propertiesToFetch = ["modified"]
            do {
                let machines = try moc.fetch(fetch)
                let modifiedDates = Set(machines.compactMap { ($0 as? MachineMO) })
                return modifiedDates
            } catch {
                logger.error("Failed to fetch modifiedDates: \(String(describing: error), privacy: .public)")
                throw error
            }
        }

        static func fetchPeerMO(moc: NSManagedObjectContext, containerMO: ContainerMO, peerID: String) throws -> PeerMO? {
            let fetch = NSFetchRequest<NSFetchRequestResult>(entityName: "Peer")
            fetch.predicate = NSPredicate(format: "peerID == %@ && container == %@", peerID, containerMO)
            fetch.fetchLimit = 1
            let peers = try moc.fetch(fetch)
            return peers.first as? PeerMO
        }

        func fetchPeerMO(peerID: String) throws -> PeerMO? {
            try DBAdapter.fetchPeerMO(moc: self.moc, containerMO: self.containerMO, peerID: peerID)
        }

        static func stableInfoFromPeerMO(peerMO: PeerMO) -> TPPeerStableInfo? {
            guard let peerID = peerMO.peerID else {
                logger.error("DBAdapter PeerMO has no ID?")
                return nil
            }
            guard let data = peerMO.stableInfo, let sig = peerMO.stableInfoSig else {
                logger.info("DBAdapter peer \(peerID, privacy: .public) has no/incomplete stable info/sig")
                return nil
            }
            guard let stableInfo = TPPeerStableInfo(data: data as Data, sig: sig as Data) else {
                logger.info("DBAdapter peer \(peerID, privacy: .public) has unparseable stable info/sig")
                return nil
            }
            return stableInfo
        }

        static func dynamicInfoFromPeerMO(peerMO: PeerMO) -> TPPeerDynamicInfo? {
            guard let peerID = peerMO.peerID else {
                logger.error("DBAdapter PeerMO has no ID?")
                return nil
            }
            guard let data = peerMO.dynamicInfo, let sig = peerMO.dynamicInfoSig else {
                logger.info("DBAdapter peer \(peerID, privacy: .public) has no/incomplete dynamic info/sig")
                return nil
            }
            guard let dynamicInfo = TPPeerDynamicInfo(data: data as Data, sig: sig as Data) else {
                logger.info("DBAdapter peer \(peerID, privacy: .public) has unparseable dynamic info/sig")
                return nil
            }
            return dynamicInfo
        }

        func peerFromMO(peerMO: PeerMO?) -> TPPeer? {
            guard let peerMO else {
                return nil
            }

            guard let peerID = peerMO.peerID else {
                logger.error("DBAdapter PeerMO has no ID?")
                return nil
            }

            let keyFactory = TPECPublicKeyFactory()

            guard let permanentInfoData = peerMO.permanentInfo, let permanentInfoSig = peerMO.permanentInfoSig else {
                logger.error("DBAdapter peer \(peerID, privacy: .public) has no/incomplete permanent info/sig")
                return nil
            }

            var updateHmac = true
            let hmacKey = self.getHmacKey()
            if let hmacSig = peerMO.hmacSig, let hmacKey {
                if TPPeer.verifyHMAC(withPermanentInfoData: permanentInfoData,
                                     permanentInfoSig: permanentInfoSig,
                                     stableInfoData: peerMO.stableInfo,
                                     stableInfoSig: peerMO.stableInfoSig,
                                     dynamicInfoData: peerMO.dynamicInfo,
                                     dynamicInfoSig: peerMO.dynamicInfoSig,
                                     hmacKey: hmacKey,
                                     hmacSig: hmacSig) {
                    updateHmac = false
                }
            }
            guard let permanentInfo = TPPeerPermanentInfo(peerID: peerID,
                                                          data: permanentInfoData,
                                                          sig: permanentInfoSig,
                                                          keyFactory: keyFactory,
                                                          checkSig: updateHmac) else {
                logger.error("DBAdapter unable to construct permanent info for peerID \(peerID, privacy: .public)")
                return nil
            }

            let stableInfo = DBAdapter.stableInfoFromPeerMO(peerMO: peerMO)
            let dynamicInfo = DBAdapter.dynamicInfoFromPeerMO(peerMO: peerMO)

            let ret: TPPeer
            do {
                ret = try TPPeer(permanentInfo: permanentInfo, stableInfo: stableInfo, dynamicInfo: dynamicInfo, checkSig: updateHmac)
            } catch {
                logger.error("DBAdapter unable to init for peerID \(peerID, privacy: .public)): \(String(describing: error), privacy: .public)")
                return nil
            }

            if updateHmac {
                if let hmacKey {
                    let hmacSig = ret.calculateHmac(withHmacKey: hmacKey)
                    peerMO.hmacSig = hmacSig
                } else {
                    peerMO.hmacSig = nil
                }
            }

            return ret
        }

        func enumerateVouchers(_ block: @escaping (TPVoucher, UnsafeMutablePointer<ObjCBool>) -> Void) throws {
            let fetch = NSFetchRequest<NSFetchRequestResult>(entityName: "Voucher")
            fetch.predicate = NSPredicate(format: "beneficiary.container == %@", self.containerMO)
            fetch.fetchBatchSize = 50
            do {
                var stop = ObjCBool(false)
                for voucher in try self.moc.fetch(fetch) {
                    autoreleasepool {
                        guard let voucher = DBAdapter.voucherFromMO(voucherMO: voucher as? VoucherMO) else {
                            return
                        }
                        block(voucher, &stop)
                    }
                    if stop.boolValue {
                        break
                    }
                }
            } catch {
                logger.error("Failed to fetch vouchers for enumeration: \(String(describing: error), privacy: .public)")
                throw error
            }
        }

        static func voucherFromMO(voucherMO: VoucherMO?) -> TPVoucher? {
            guard let voucherMO = voucherMO, let data = voucherMO.voucherInfo, let sig = voucherMO.voucherInfoSig else {
                return nil
            }

            return TPVoucher(infoWith: data, sig: sig)
        }

        func voucherCount(_ errorOut: NSErrorPointer) -> UInt {
            let fetch = NSFetchRequest<NSFetchRequestResult>(entityName: "Voucher")
            fetch.predicate = NSPredicate(format: "beneficiary.container == %@", self.containerMO)
            do {
                let count = try self.moc.count(for: fetch)
                return UInt(count)
            } catch let error as NSError {
                logger.error("Failed to fetch vouchers for count: \(String(describing: error), privacy: .public)")
                errorOut?.pointee = error
                return 0
            }
        }
    }

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
         managedConfigurationAdapter: OTManagedConfigurationAdapter,
         cuttlefish: ConfiguredCuttlefishAPIAsync) throws {
        var initError: Error?
        var containerMO: ContainerMO!
        var model: TPModel!
        var dbAdapter: DBAdapter!

        self.darwinNotifier = darwinNotifier
        self.managedConfigurationAdapter = managedConfigurationAdapter

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
                containerFetch.fetchLimit = 1
                let fetchedContainers = try moc.fetch(containerFetch)
                if let container = fetchedContainers.first as? ContainerMO {
                    containerMO = container
                } else {
                    containerMO = ContainerMO(context: moc)
                    containerMO.name = name.asSingleString()
                }

                // Perform upgrades as needed
                Container.onqueueUpgradeMachineIDSetToModel(container: containerMO, moc: moc)
                Container.onqueueUpgradeMachineIDSetToUseStatus(container: containerMO, moc: moc)

                // remove duplicate vouchers on all the peers
                Container.onqueueRemoveDuplicateVouchers(container: containerMO, moc: moc)
                
                // remove duplicate machineMOs
                Container.onqueueRemoveDuplicateMachineIDs(containerMO: containerMO, moc: moc)

                (model, dbAdapter) = Container.loadModel(moc: moc, containerMO: containerMO, hmacKey: nil)
                Container.ensureEgoConsistency(from: containerMO, model: model)

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
        self.containerMO = containerMO
        self.cuttlefish = cuttlefish
        self.model = model
        self.dbAdapter = dbAdapter
        self.escrowCacheTimeout = 60.0 * 15.0 // 15 minutes
        super.init()
    }

    internal static func onqueueRemoveDuplicateMachineIDs(containerMO: ContainerMO, moc: NSManagedObjectContext) {
        do {
            let allMachineIDs = try DBAdapter.allMachineIDs(containerMO: containerMO, moc: moc)
            for machineID in allMachineIDs {
                var machines = try DBAdapter.allMachineModifiedDatesFor(containerMO: containerMO, moc: moc, machineID: machineID)
                if var highest = machines.first {
                    for machine in machines {
                        if machine.modifiedDate() > highest.modifiedDate() {
                            highest = machine
                        }
                    }

                    machines.remove(highest)

                    logger.info("onqueueRemoveDuplicateMachineIDs removing: \(machines)")

                    for machine in machines {
                        moc.delete(machine)
                    }
                }
            }
        } catch {
            logger.error("onqueueRemoveDuplicateMachineIDs error removing duplicate machineIDs: \(error, privacy: .public)")
        }
    }

    func getHmacKey() -> Data? {
        return self.dbAdapter.getHmacKey()
    }

    func grabSemaphore(function: String = #function) -> SemaphoreWrapper {
        return SemaphoreWrapper(parent: self, function: function)
    }

    func configuredFor(user: TPSpecificUser) -> Bool {
        return self.cuttlefish.configuredFor(user: user)
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
    internal static func loadModel(moc: NSManagedObjectContext,
                                   containerMO: ContainerMO,
                                   hmacKey: Data?) -> (TPModel, DBAdapter) {
        // Populate model from persistent store
        let dbAdapter = DBAdapter(moc: moc, containerMO: containerMO, hmacKey: hmacKey)
        let model = TPModel(decrypter: Decrypter(), dbAdapter: dbAdapter)
        model.suppressInitialInfoLogging = true
        defer { model.suppressInitialInfoLogging = false }
        let keyFactory = TPECPublicKeyFactory()

        let crks = containerMO.custodianRecoveryKeys as? Set<CustodianRecoveryKeyMO>
        crks?.forEach { crk in
            if let data = crk.crkInfo, let sig = crk.crkInfoSig {
                if let tpcrk = TPCustodianRecoveryKey(data: data, sig: sig, keyFactory: keyFactory) {
                    model.register(tpcrk)
                }
            }
        }

        do {
            let peerCount = try model.peerCount();
            logger.info("loadModel: loaded \(peerCount) peers")
        } catch {
            logger.error("loadModel error getting peerCount: \(error, privacy: .public)")
        }
        do {
            let voucherCount = try model.voucherCount()
            logger.info("loadModel: loaded \(voucherCount) vouchers")
        } catch {
            logger.error("loadModel error getting voucherCount: \(error, privacy: .public)")
        }
        logger.info("loadModel: loaded \(model.allCustodianRecoveryKeys().count) CRKs")

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
                logger.info("loadModel: recovery key not set in model, but is set on ego peer")
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
        let ghostedMachineIDs = Set(knownMachines.filter { $0.status == TPMachineIDStatus.ghostedFromTDL.rawValue }.compactMap { $0.machineID })
        let evictedMachineIDs = Set(knownMachines.filter { $0.status == TPMachineIDStatus.evicted.rawValue }.compactMap { $0.machineID })
        let unknownReasonMachineIDs = Set(knownMachines.filter { $0.status == TPMachineIDStatus.unknownReason.rawValue }.compactMap { $0.machineID })
        let unknownMachineIDs = Set(knownMachines.filter { $0.status == TPMachineIDStatus.unknown.rawValue }.compactMap { $0.machineID })

        logger.info("loadModel: allowedMachineIDs: \(allowedMachineIDs, privacy: .public)")
        logger.info("loadModel: disallowedMachineIDs: \(disallowedMachineIDs, privacy: .public)")
        logger.info("loadModel: ghostedMachineIDs: \(ghostedMachineIDs, privacy: .public)")
        logger.info("loadModel: evictedMachineIDs: \(evictedMachineIDs, privacy: .public)")
        logger.info("loadModel: unknownReasonMachineIDs: \(unknownReasonMachineIDs, privacy: .public)")
        logger.info("loadModel: unknownMachineIDs: \(unknownMachineIDs, privacy: .public)")

        if allowedMachineIDs.isEmpty {
            logger.info("loadModel: no allowedMachineIDs?")
        }

        return (model, dbAdapter)
    }

    func resetContainer() {
        self.moc.delete(self.containerMO)
        self.containerMO = ContainerMO(context: self.moc)
        self.containerMO.name = self.name.asSingleString()
        (self.model, self.dbAdapter) = Container.loadModel(moc: self.moc, containerMO: self.containerMO, hmacKey: self.getHmacKey())
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

        let modelStableInfo: TPPeerStableInfo?
        do {
            modelStableInfo = try model.getStableInfoForPeer(withID: egoPeerID)
        } catch {
            logger.error("ensureEgoConsistency failed to create TPPeerStableInfo from model: \(error, privacy: .public)")
            return
        }
        guard let modelStableInfo else {
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
                if !SecIsInternalRelease() {
                    peerDict["serial_number"] = nil
                    peerDict["device_name"] = nil
                }
            }
            if let dynamicInfo = peer.dynamicInfo {
                peerDict["dynamicInfo"] = dynamicInfo.dictionaryRepresentation()
            }

            return peerDict
        }
    }

    func onQueueDetermineLocalTrustStatus(reply: @escaping (TrustedPeersHelperEgoPeerStatus, Error?) -> Void) {
        func logErrorAndReplyEarly(error: Error, whichFunc: String, viablePeerCountsByModelID: [String:NSNumber]) {
            logger.error("error calling \(whichFunc): \(error, privacy: .public)")
            reply(TrustedPeersHelperEgoPeerStatus(egoPeerID: nil,
                                                  egoPeerMachineID: nil,
                                                  status: TPPeerStatus.unknown,
                                                  viablePeerCountsByModelID: viablePeerCountsByModelID,
                                                  peerCountsByMachineID: [:],
                                                  isExcluded: false,
                                                  isLocked: false),
                  error)
        }

        let viablePeerCountsByModelID: [String:NSNumber]
        do {
            viablePeerCountsByModelID = try self.model.viablePeerCountsByModelID()
        } catch {
            logErrorAndReplyEarly(error: error, whichFunc: "viablePeerCountsByModelID", viablePeerCountsByModelID: [:])
            return
        }

        let peerCountsByMachineID: [String:NSNumber]
        do {
            peerCountsByMachineID = try self.model.peerCountsByMachineID()
        } catch {
            logErrorAndReplyEarly(error: error, whichFunc: "peerCountsByMachineID", viablePeerCountsByModelID: viablePeerCountsByModelID)
            return
        }

        if let egoPeerID = self.containerMO.egoPeerID {
            let egoPeer: TPPeer?
            do {
                egoPeer = try self.model.peer(withID: egoPeerID)
                if egoPeer == nil {
                    logger.warning("Couldn't find ego peer in model")
                }
            } catch {
                logger.warning("Error getting ego peer from model: \(String(describing: error), privacy: .public)")
                egoPeer = nil
            }
            let egoPermanentInfo = egoPeer?.permanentInfo

            var status: TPPeerStatus
            do {
                status = try self.model.statusOfPeer(withID: egoPeerID)
            } catch {
                logger.error("error calling statusOfPeer: \(error, privacy: .public)")
                let egoStatus = TrustedPeersHelperEgoPeerStatus(egoPeerID: egoPeerID,
                                                                egoPeerMachineID: egoPermanentInfo?.machineID,
                                                                status: .unknown,
                                                                viablePeerCountsByModelID: viablePeerCountsByModelID,
                                                                peerCountsByMachineID: peerCountsByMachineID,
                                                                isExcluded: false,
                                                                isLocked: false)
                reply(egoStatus, error)
                return
            }
            var isExcluded: Bool = (status == .excluded)

            loadEgoKeys(peerID: egoPeerID) { egoPeerKeys, loadError in
                var returnError = loadError

                guard returnError == nil else {
                    var isLocked = false
                    if let error = (loadError as NSError?) {
                        logger.error("trust status: Unable to load ego keys: \(String(describing: error), privacy: .public)")
                        if error.code == errSecItemNotFound && error.domain == NSOSStatusErrorDomain {
                            logger.info("trust status: Lost the ego key pair, returning 'excluded' in hopes of fixing up the identity")
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
                    logger.info("trust status: No error but Ego Peer Keys are nil")
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
            do {
                if try self.model.hasAnyPeers() {
                    logger.info("Existing peers in account, but we don't have a peer ID. We are excluded.")
                    let egoStatus = TrustedPeersHelperEgoPeerStatus(egoPeerID: nil,
                                                                    egoPeerMachineID: nil,
                                                                    status: .excluded,
                                                                    viablePeerCountsByModelID: viablePeerCountsByModelID,
                                                                    peerCountsByMachineID: peerCountsByMachineID,
                                                                    isExcluded: true,
                                                                    isLocked: false)
                    reply(egoStatus, nil)
                    return
                } else {
                    logger.info("No existing peers in account")
                    let egoStatus = TrustedPeersHelperEgoPeerStatus(egoPeerID: nil,
                                                                    egoPeerMachineID: nil,
                                                                    status: .unknown,
                                                                    viablePeerCountsByModelID: viablePeerCountsByModelID,
                                                                    peerCountsByMachineID: peerCountsByMachineID,
                                                                    isExcluded: false,
                                                                    isLocked: false)
                    reply(egoStatus, nil)
                    return
                }
            } catch {
                logger.error("error calling hasAnyPeers: \(error, privacy: .public)")
                reply(TrustedPeersHelperEgoPeerStatus(egoPeerID: nil,
                                                      egoPeerMachineID: nil,
                                                      status: TPPeerStatus.unknown,
                                                      viablePeerCountsByModelID: [:],
                                                      peerCountsByMachineID: [:],
                                                      isExcluded: false,
                                                      isLocked: false),
                      error)
                return
            }
        }
    }

    func trustStatus(reply: @escaping (TrustedPeersHelperEgoPeerStatus, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (TrustedPeersHelperEgoPeerStatus, Error?) -> Void = {
            // Suppress logging of successful replies here; it's not that useful
            let logType: OSLogType = $1 == nil ? .debug : .error
            logger.log(level: logType, "trustStatus complete: \(TPPeerStatusToString($0.egoStatus), privacy: .public) \(traceError($1), privacy: .public)")
            sem.release()
            reply($0, $1)
        }
        self.moc.performAndWait {
            // Knowledge of your peer status only exists if you know about other peers. If you haven't fetched, fetch.
            if self.containerMO.changeToken == nil {
                self.onqueueFetchAndPersistChanges { fetchError in
                    guard fetchError == nil else {
                        if let error = fetchError {
                            logger.error("Unable to fetch changes, trust status is unknown: \(String(describing: error), privacy: .public)")
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
            let logType: OSLogType = $2 == nil ? .info : .error
            logger.log(level: logType, "fetch trust state complete: \(String(reflecting: $0), privacy: .public) \(traceError($2), privacy: .public)")
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

                let isPreapproved: Bool
                do {
                    isPreapproved = try self.model.hasPotentiallyTrustedPeerPreapprovingKey(permanentInfo.signingPubKey.spki())
                } catch {
                    logger.error("fetchTrustState: error calling hasPotentiallyTrustedPeerPreapprovingKey \(error, privacy: .public)")
                    reply(nil, nil, error)
                    return
                }
                logger.info("fetchTrustState: ego peer is \(isPreapproved ? "preapproved" : "not yet preapproved", privacy: .public)")

                let egoStableInfo: TPPeerStableInfo?
                do {
                    egoStableInfo = try self.model.getStableInfoForPeer(withID: egoPeerID)
                } catch {
                    logger.error("fetchTrustState: error calling getStableInfoForPeer \(egoPeerID): \(error, privacy: .public)")
                    reply(nil, nil, error)
                    return
                }

                let status: TPPeerStatus
                do {
                    status = try self.model.statusOfPeer(withID: egoPeerID)
                } catch {
                    logger.error("fetchTrustState error calling statusOfPeer: \(error, privacy: .public)")
                    reply(nil, nil, error)
                    return
                }
                let egoPeerStatus = TrustedPeersHelperPeerState(peerID: egoPeerID,
                                                                isPreapproved: isPreapproved,
                                                                status: status,
                                                                memberChanges: false,
                                                                unknownMachineIDs: self.onqueueFullIDMSListWouldBeHelpful(),
                                                                osVersion: egoStableInfo?.osVersion,
                                                                walrus: egoStableInfo?.walrusSetting,
                                                                webAccess: egoStableInfo?.webAccess)

                var tphPeers: [TrustedPeersHelperPeer] = []

                let egoPeer: TPPeer?
                do {
                    egoPeer = try self.model.peer(withID: egoPeerID)
                } catch {
                    logger.warning("Error getting ego peer from model: \(String(describing: error), privacy: .public)")
                    egoPeer = nil
                }
                if let egoPeer {
                    egoPeer.trustedPeerIDs.forEach { trustedPeerID in
                        let peer: TPPeer?
                        do {
                            peer = try self.model.peer(withID: trustedPeerID)
                        } catch {
                            logger.warning("Error getting trusted peer \(trustedPeerID) from model: \(String(describing: error), privacy: .public)")
                            peer = nil
                        }
                        if let peer {
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
                                logger.error("Unable to add CRK as a trusted peer: \(String(describing: error), privacy: .public)")
                            }
                        } else {
                            logger.info("No peer for trusted ID \(trustedPeerID, privacy: .public)")
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
                            logger.error("Unable to add RK as a trusted peer: \(String(describing: error), privacy: .public)")
                        }
                    }
                } else {
                    logger.info("No ego peer in model; no trusted peers")
                }

                logger.info("Returning trust state: \(egoPeerStatus, privacy: .public) \(tphPeers, privacy: .public)")
                reply(egoPeerStatus, tphPeers, nil)
            } else {
                // With no ego peer ID, there are no trusted peers
                logger.info("No peer ID => no trusted peers")
                reply(TrustedPeersHelperPeerState(peerID: nil, isPreapproved: false, status: .unknown, memberChanges: false, unknownMachineIDs: false, osVersion: nil,
                                                  walrus: nil, webAccess: nil), [], nil)
            }
        }
    }

    func dump(reply: @escaping ([AnyHashable: Any]?, Error?) -> Void) {
        let reply: ([AnyHashable: Any]?, Error?) -> Void = {
            let logType: OSLogType = $1 == nil ? .info : .error
            logger.log(level: logType, "dump complete: \(traceError($1), privacy: .public)")
            reply($0, $1)
        }
        self.moc.performAndWait {
            var d: [AnyHashable: Any] = [:]

            if let egoPeerID = self.containerMO.egoPeerID {
                let peer: TPPeer?
                do {
                    peer = try self.model.peer(withID: egoPeerID)
                } catch {
                    logger.warning("Error getting ego peer from model: \(String(describing: error), privacy: .public)")
                    peer = nil
                }
                if let peer {
                    d["self"] = Container.peerdictionaryRepresentation(peer: peer)
                } else {
                    d["self"] = ["peerID": egoPeerID]
                }
            } else {
                d["self"] = [AnyHashable: Any]()
            }

            autoreleasepool {
                var otherPeers: [[String: Any]] = []
                do {
                    try self.model.enumeratePeers { peer, _ in
                        if peer.peerID != self.containerMO.egoPeerID {
                            otherPeers.append(Container.peerdictionaryRepresentation(peer: peer))
                        }
                    }
                } catch {
                    logger.error("Error enumerating peers: \(error, privacy: .public)")
                    d["errorEnumeratingPeers"] = "\(error)"
                }
                d["peers"] = otherPeers

                var vouchers: [[String: Any]] = []
                do {
                    try self.model.enumerateVouchers { voucher, _ in
                        vouchers.append(voucher.dictionaryRepresentation())
                    }
                } catch {
                    logger.error("Error enumerating vouchers: \(error, privacy: .public)")
                    d["errorEnumeratingVouchers"] = "\(error)"
                }
                d["vouchers"] = vouchers

                d["custodian_recovery_keys"] = self.model.allCustodianRecoveryKeys().map { $0.dictionaryRepresentation() }

                if let bottles = self.containerMO.bottles as? Set<BottleMO> {
                    d["bottles"] = bottles.map { Container.dictionaryRepresentation(bottle: $0) }
                } else {
                    d["bottles"] = [Any]()
                }
            }

            let midList = self.onqueueCurrentMIDList()
            d["idmsTrustedDevicesVersion"] = self.containerMO.idmsTrustedDevicesVersion
            d["idmsTrustedDeviceListFetchDate"] = self.containerMO.idmsTrustedDeviceListFetchDate
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
            let logType: OSLogType = $4 == nil ? .info : .error
            logger.log(level: logType, "dumpEgoPeer complete: \(traceError($4), privacy: .public)")
            reply($0, $1, $2, $3, $4)
        }
        self.moc.performAndWait {
            guard let egoPeerID = self.containerMO.egoPeerID else {
                reply(nil, nil, nil, nil, ContainerError.noPreparedIdentity)
                return
            }

            let peer: TPPeer?
            do {
                peer = try self.model.peer(withID: egoPeerID)
            } catch {
                logger.error("Error getting ego peer from model: \(String(describing: error), privacy: .public)")
                reply(egoPeerID, nil, nil, nil, error)
                return
            }
            guard let peer else {
                reply(egoPeerID, nil, nil, nil, nil)
                return
            }

            reply(egoPeerID, peer.permanentInfo, peer.stableInfo, peer.dynamicInfo, nil)
        }
    }

    func reset(resetReason: CuttlefishResetReason, idmsTargetContext: String?, idmsCuttlefishPassword: String?, notifyIdMS: Bool, internalAccount: Bool, demoAccount: Bool, reply: @escaping (Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (Error?) -> Void = {
            let logType: OSLogType = $0 == nil ? .info : .error
            logger.log(level: logType, "reset complete \(traceError($0), privacy: .public)")
            sem.release()
            reply($0)
        }

        self.moc.performAndWait {
            let resetReason = ResetReason.from(cuttlefishResetReason: resetReason)
            let request = ResetRequest.with {
                $0.resetReason = resetReason
                $0.idmsTargetContext = idmsTargetContext ?? ""
                $0.idmsCuttlefishPassword = idmsCuttlefishPassword ?? ""
                $0.testingNotifyIdms = notifyIdMS
                $0.accountInfo = AccountInfo.with {
                    $0.flags = (internalAccount ? UInt32(AccountFlags.internal.rawValue) : 0) | (demoAccount ? UInt32(AccountFlags.demo.rawValue) : 0)
                }
            }
            self.cuttlefish.reset(request) { response in
                switch response {
                case .success(let response):
                    // Erase container's persisted state
                    self.moc.performAndWait {
                        self.resetContainer()
                        self.darwinNotifier.post(OTCliqueChanged)
                        do {
                            try self.onQueuePersist(changes: response.changes)
                            logger.info("reset succeded")
                            reply(nil)
                        } catch {
                            logger.error("reset persist failed: \(String(describing: error), privacy: .public)")
                            reply(error)
                        }
                    }
                case .failure(let error):
                    logger.error("reset failed: \(String(describing: error), privacy: .public)")
                    reply(error)
                    return
                }
            }
        }
    }

    func localReset(reply: @escaping (Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (Error?) -> Void = {
            let logType: OSLogType = $0 == nil ? .info : .error
            logger.log(level: logType, "localReset complete \(traceError($0), privacy: .public)")
            sem.release()
            reply($0)
        }

        self.moc.performAndWait {
            do {
                self.resetContainer()
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
                 setting: OTAccountSettings?,
                 signingPrivateKeyPersistentRef: Data?,
                 encryptionPrivateKeyPersistentRef: Data?,
                 reply: @escaping (String?, Data?, Data?, Data?, Data?, TPSyncingPolicy?, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (String?, Data?, Data?, Data?, Data?, TPSyncingPolicy?, Error?) -> Void = {
            let logType: OSLogType = $6 == nil ? .info : .error
            logger.log(level: logType, "prepare complete peerID: \(String(describing: $0), privacy: .public) \(traceError($6), privacy: .public)")
            sem.release()
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
                                                    creationTime: UInt64(Date().timeIntervalSince1970 * 1000),
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
            logger.error("bottle creation failed: \(String(describing: error), privacy: .public)")
            reply(nil, nil, nil, nil, nil, nil, error)
            return
        }

        saveEgoKeyPair(signingKeyPair, identifier: signingKeyIdentifier(peerID: peerID)) { success, error in
            guard success else {
                logger.error("Unable to save signing key: \(String(describing: error), privacy: .public)")
                reply(nil, nil, nil, nil, nil, nil, error ?? ContainerError.failedToStoreIdentity)
                return
            }
            saveEgoKeyPair(encryptionKeyPair, identifier: encryptionKeyIdentifier(peerID: peerID)) { success, error in
                guard success else {
                    logger.error("Unable to save encryption key: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, nil, nil, nil, nil, error ?? ContainerError.failedToStoreIdentity)
                    return
                }

                let actualPolicyVersion = policyVersion ?? prevailingPolicyVersion
                self.fetchPolicyDocumentWithSemaphore(version: actualPolicyVersion) { policyDoc, policyFetchError in
                    guard let policyDoc = policyDoc, policyFetchError == nil else {
                        logger.info("Unable to fetch policy: \(String(describing: policyFetchError), privacy: .public)")
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

                            var walrusSetting: TPPBPeerStableInfoSetting?
                            var webAccess: TPPBPeerStableInfoSetting?

                            if let accountSetting = setting {
                                if let walrusAccountSetting = accountSetting.walrus {
                                    walrusSetting = TPPBPeerStableInfoSetting()
                                    walrusSetting?.value = walrusAccountSetting.enabled
                                }
                                if let webAccessSetting = accountSetting.webAccess {
                                    webAccess = TPPBPeerStableInfoSetting()
                                    webAccess?.value = webAccessSetting.enabled
                                }
                            }

                            let stableInfo = try TPPeerStableInfo(clock: 1,
                                                                  frozenPolicyVersion: useFrozenPolicyVersion ? frozenPolicyVersion : policyDoc.version,
                                                                  flexiblePolicyVersion: useFrozenPolicyVersion ? policyDoc.version : nil,
                                                                  policySecrets: policySecrets,
                                                                  syncUserControllableViews: syncUserViews,
                                                                  secureElementIdentity: secureElementIdentity,
                                                                  walrusSetting: walrusSetting,
                                                                  webAccess: webAccess,
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
                            logger.error("Unable to save identity: \(String(describing: error), privacy: .public)")
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
        let sem = self.grabSemaphore()
        let reply: (String?, Data?, Data?, Data?, Data?, TPSyncingPolicy?, String?, [CKRecord]?, Error?) -> Void = {
            let logType: OSLogType = $8 == nil ? .info : .error
            logger.log(level: logType, "prepareInheritancePeer complete peerID: \(String(describing: $0), privacy: .public) \(traceError($8), privacy: .public)")
            sem.release()
            reply($0, $1, $2, $3, $4, $5, $6, $7, $8)
        }

        self.fetchAndPersistChangesIfNeeded { error in
            guard error == nil else {
                logger.error("prepareInheritancePeer unable to fetch changes: \(String(describing: error), privacy: .public)")
                reply(nil, nil, nil, nil, nil, nil, nil, nil, error)
                return
            }

            guard let uuid = UUID(uuidString: crk.uuid) else {
                logger.info("Unable to parse uuid: \(crk.uuid, privacy: .public)")
                reply(nil, nil, nil, nil, nil, nil, nil, nil, ContainerError.recoveryKeysNotEnrolled)
                return
            }

            guard let tpcrk = self.model.findCustodianRecoveryKey(with: uuid) else {
                logger.info("Unable to find custodian recovery key \(crk.uuid, privacy: .public) on model")
                reply(nil, nil, nil, nil, nil, nil, nil, nil, ContainerError.recoveryKeysNotEnrolled)
                return
            }

            do {
                guard try self.model.isCustodianRecoveryKeyTrusted(tpcrk) else {
                    logger.info("Custodian Recovery Key is not trusted")
                    reply(nil, nil, nil, nil, nil, nil, nil, nil, ContainerError.untrustedRecoveryKeys)
                    return
                }
            } catch {
                logger.error("Error determining whether Custodian Recovery Key is trusted: \(error, privacy: .public)")
                reply(nil, nil, nil, nil, nil, nil, nil, nil, error)
                return
            }
            guard let recoveryKeyString = crk.recoveryString, let recoverySalt = crk.salt else {
                logger.info("Bad format CRK: recovery string or salt not set")
                reply(nil, nil, nil, nil, nil, nil, nil, nil, ContainerError.custodianRecoveryKeyMalformed)
                return
            }

            // create inheritance recovery key set
            let permanentInfo: TPPeerPermanentInfo
            let signingKeyPair: _SFECKeyPair
            let encryptionKeyPair: _SFECKeyPair
            let recoveryCRK: CustodianRecoveryKey
            do {
                recoveryCRK = try CustodianRecoveryKey(tpCustodian: tpcrk, recoveryKeyString: recoveryKeyString, recoverySalt: recoverySalt)

                signingKeyPair = recoveryCRK.peerKeys.signingKey
                encryptionKeyPair = recoveryCRK.peerKeys.encryptionKey
            } catch {
                logger.error("failed to create custodian recovery keys: \(String(describing: error), privacy: .public)")
                reply(nil, nil, nil, nil, nil, nil, nil, nil, ContainerError.failedToCreateRecoveryKey(suberror: error))
                return
            }

            logger.info("prepareInheritancePeer signingPubKey: \(recoveryCRK.peerKeys.signingKey.publicKey.keyData.base64EncodedString(), privacy: .public)")
            logger.info("prepareInheritancePeer encryptionPubKey: \(recoveryCRK.peerKeys.encryptionKey.publicKey.keyData.base64EncodedString(), privacy: .public)")

            do {
                permanentInfo = try TPPeerPermanentInfo(machineID: machineID,
                                                        modelID: modelID,
                                                        epoch: 1,
                                                        signing: signingKeyPair,
                                                        encryptionKeyPair: encryptionKeyPair,
                                                        creationTime: UInt64(Date().timeIntervalSince1970 * 1000),
                                                        peerIDHashAlgo: TPHashAlgo.SHA256)
            } catch {
                reply(nil, nil, nil, nil, nil, nil, nil, nil, error)
                return
            }

            let peerID = permanentInfo.peerID

            saveEgoKeyPair(signingKeyPair, identifier: signingKeyIdentifier(peerID: peerID)) { success, error in
                guard success else {
                    logger.error("Unable to save signing key: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, nil, nil, nil, nil, nil, nil, error ?? ContainerError.failedToStoreIdentity)
                    return
                }
                saveEgoKeyPair(encryptionKeyPair, identifier: encryptionKeyIdentifier(peerID: peerID)) { success, error in
                    guard success else {
                        logger.error("Unable to save encryption key: \(String(describing: error), privacy: .public)")
                        reply(nil, nil, nil, nil, nil, nil, nil, nil, error ?? ContainerError.failedToStoreIdentity)
                        return
                    }

                    let actualPolicyVersion = policyVersion ?? prevailingPolicyVersion
                    self.fetchPolicyDocumentWithSemaphore(version: actualPolicyVersion) { policyDoc, policyFetchError in
                        guard let policyDoc = policyDoc, policyFetchError == nil else {
                            logger.info("Unable to fetch policy: \(String(describing: policyFetchError), privacy: .public)")
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
                                                                      walrusSetting: nil,
                                                                      webAccess: nil,
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
                                    logger.info("permanentInfo does not exist")
                                    reply(nil, nil, nil, nil, nil, nil, nil, nil, ContainerError.nonMember)
                                    return
                                }
                                guard let permanentInfoSig = self.containerMO.egoPeerPermanentInfoSig else {
                                    logger.info("permanentInfoSig does not exist")
                                    reply(nil, nil, nil, nil, nil, nil, nil, nil, ContainerError.nonMember)
                                    return
                                }

                                guard let stableInfo = self.containerMO.egoPeerStableInfo else {
                                    logger.info("stableInfo does not exist")
                                    reply(nil, nil, nil, nil, nil, nil, nil, nil, ContainerError.nonMember)
                                    return
                                }
                                guard let stableInfoSig = self.containerMO.egoPeerStableInfoSig else {
                                    logger.info("stableInfoSig does not exist")
                                    reply(nil, nil, nil, nil, nil, nil, nil, nil, ContainerError.nonMember)
                                    return
                                }

                                let keyFactory = TPECPublicKeyFactory()
                                guard let selfPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: permanentInfo, sig: permanentInfoSig, keyFactory: keyFactory) else {
                                    reply(nil, nil, nil, nil, nil, nil, nil, nil, ContainerError.invalidPermanentInfoOrSig)
                                    return
                                }
                                guard let selfStableInfo = TPPeerStableInfo(data: stableInfo, sig: stableInfoSig) else {
                                    logger.info("Invalid stableinfo or signature")
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
                                            logger.error("fetchRecoverableTlkshares failed: \(String(describing: error), privacy: .public)")
                                            reply(nil, nil, nil, nil, nil, nil, nil, nil, error)
                                            return

                                        case .success(let response):
                                            let shareCount = response.views.map { $0.tlkShares.count }.reduce(0, +)
                                            logger.info("fetchRecoverableTlkshares succeeded: found \(response.views.count) views and \(shareCount) total TLKShares")
                                            let records = response.views.flatMap { $0.ckrecords() }

                                            reply(selfPermanentInfo.peerID, selfPermanentInfo.data, selfPermanentInfo.sig, selfStableInfo.data, selfStableInfo.sig, syncingPolicy, recoveryCRK.peerID, records, nil)
                                            return
                                        }
                                    }
                                } catch {
                                    logger.error("failed to create syncing policy: \(String(describing: error), privacy: .public)")
                                    reply(nil, nil, nil, nil, nil, nil, nil, nil, error)
                                }
                            } catch {
                                logger.error("Unable to save inheritance identity: \(String(describing: error), privacy: .public)")
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
            let logType: OSLogType = $1 == nil ? .info : .error
            logger.log(level: logType, "getEgoEpoch complete: \($0) \(traceError($1), privacy: .public)")
            reply($0, $1)
        }

        self.moc.performAndWait {
            guard let egoPeerID = self.containerMO.egoPeerID else {
                reply(0, ContainerError.noPreparedIdentity)
                return
            }
            let egoPeer: TPPeer?
            do {
                egoPeer = try self.model.peer(withID: egoPeerID)
            } catch {
                logger.error("Error getting ego peer from model: \(String(describing: error), privacy: .public)")
                reply(0, error)
                return
            }
            guard let egoPeer else {
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
        let sem = self.grabSemaphore()
        let reply: (String?, [CKRecord], TPSyncingPolicy?, Error?) -> Void = {
            let logType: OSLogType = $3 == nil ? .info : .error
            logger.log(level: logType, "establish complete peer: \(String(describing: $0), privacy: .public) \(traceError($3), privacy: .public)")
            sem.release()
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
                logger.error("fetchAfterEstablish failed to reset local data: \(String(describing: error), privacy: .public)")
                reply(nil, [], nil, error)
                return
            }
            self.onqueueFetchAndPersistChanges { error in
                guard error == nil else {
                    logger.error("fetchAfterEstablish failed to fetch changes: \(String(describing: error), privacy: .public)")
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
                            logger.info("fetchAfterEstablish: failed to fetch egoPeerID")
                            reply(nil, [], nil, ContainerError.noPreparedIdentity)
                            return
                    }
                    do {
                        guard try self.model.hasPeer(withID: egoPeerID) else {
                            logger.info("fetchAfterEstablish: did not find peer \(egoPeerID, privacy: .public) in model")
                            reply(nil, [], nil, ContainerError.invalidPeerID)
                            return
                        }
                    } catch {
                        logger.info("fetchAfterEstablish: error finding peer \(egoPeerID, privacy: .public) in model: \(error, privacy: .public)")
                        reply(nil, [], nil, error)
                        return
                    }
                    let keyFactory = TPECPublicKeyFactory()
                    guard let selfPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: egoPermData, sig: egoPermSig, keyFactory: keyFactory) else {
                        reply(nil, [], nil, ContainerError.invalidPermanentInfoOrSig)
                        return
                    }
                    guard let selfStableInfo = TPPeerStableInfo(data: egoStableData, sig: egoStableSig) else {
                        logger.info("cannot create TPPeerStableInfo")
                        reply(nil, [], nil, ContainerError.invalidStableInfoOrSig)
                        return
                    }
                    self.onqueueUpdateTLKs(ckksKeys: ckksKeys, tlkShares: tlkShares) { ckrecords, error in
                        guard error == nil else {
                            logger.error("fetchAfterEstablish failed to update TLKs: \(String(describing: error), privacy: .public)")
                            reply(nil, [], nil, error)
                            return
                        }

                        do {
                            let syncingPolicy = try self.syncingPolicyFor(modelID: selfPermanentInfo.modelID,
                                                                          stableInfo: selfStableInfo)
                            logger.info("fetchAfterEstablish succeeded")
                            reply(egoPeerID, ckrecords ?? [], syncingPolicy, nil)
                        } catch {
                            logger.error("fetchAfterEstablish failed: \(String(describing: error), privacy: .public)")
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
            logger.info("cannot create TPPeerStableInfo")
            reply(nil, [], nil, ContainerError.invalidStableInfoOrSig)
            return
        }
        guard self.onqueueMachineIDAllowedByIDMS(machineID: selfPermanentInfo.machineID) else {
            logger.info("establish: self machineID \(selfPermanentInfo.machineID, privacy: .public) not on list")
            self.onqueueTTRUntrusted()
            reply(nil, [], nil, ContainerError.preparedIdentityNotOnAllowedList(selfPermanentInfo.machineID))
            return
        }

        loadEgoKeys(peerID: egoPeerID) { egoPeerKeys, error in
            guard let egoPeerKeys = egoPeerKeys else {
                logger.error("Don't have my own peer keys; can't establish: \(String(describing: error), privacy: .public)")
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
                    logger.error("Unable to make TLKShares for self: \(String(describing: error), privacy: .public)")
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

                    logger.info("dynamic info: \(newDynamicInfo, privacy: .public)")
                } catch {
                    logger.error("Unable to create peer for joining: \(String(describing: error), privacy: .public)")
                    reply(nil, [], nil, error)
                    return
                }

                guard let newPeerStableInfo = peer.stableInfoAndSig.toStableInfo() else {
                    logger.info("Unable to create new peer stable info for joining")
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
                logger.info("Beginning establish for peer \(egoPeerID, privacy: .public)")
                logger.info("Establish permanentInfo: \(egoPermData.base64EncodedString(), privacy: .public)")
                logger.info("Establish permanentInfoSig: \(egoPermSig.base64EncodedString(), privacy: .public)")
                logger.info("Establish stableInfo: \(egoStableData.base64EncodedString(), privacy: .public)")
                logger.info("Establish stableInfoSig: \(egoStableSig.base64EncodedString(), privacy: .public)")
                logger.info("Establish dynamicInfo: \(peer.dynamicInfoAndSig.peerDynamicInfo.base64EncodedString(), privacy: .public)")
                logger.info("Establish dynamicInfoSig: \(peer.dynamicInfoAndSig.sig.base64EncodedString(), privacy: .public)")

                logger.info("Establish introducing \(viewKeys.count) key sets, \(allTLKShares.count) tlk shares")

                do {
                    let bottleSerialized = try bottle.serializedData().base64EncodedString()
                    logger.info("Establish bottle: \(bottleSerialized, privacy: .public)")
                    let peerSerialized = try peer.serializedData().base64EncodedString()
                    logger.info("Establish peer: \(peerSerialized, privacy: .public)")
                } catch {
                    logger.error("Establish unable to encode bottle/peer: \(String(describing: error), privacy: .public)")
                }

                let request = EstablishRequest.with {
                    $0.peer = peer
                    $0.bottle = bottle
                    $0.viewKeys = viewKeys
                    $0.tlkShares = allTLKShares
                }
                self.cuttlefish.establish(request) { response in
                    logger.info("Establish: viewKeys: \(String(describing: viewKeys), privacy: .public)")

                    switch response {
                    case .success(let response):
                        do {
                            let responseChangesJson = try response.changes.jsonString()
                            logger.info("Establish returned changes: \(responseChangesJson, privacy: .public)")
                        } catch {
                            logger.info("Establish returned changes, but they can't be serialized")
                        }

                        let keyHierarchyRecords = response.zoneKeyHierarchyRecords.compactMap { CKRecord($0) }

                        do {
                            let syncingPolicy = try self.syncingPolicyFor(modelID: selfPermanentInfo.modelID,
                                                                          stableInfo: newPeerStableInfo)

                            try self.persist(changes: response.changes)

                            guard response.changes.more == false else {
                                logger.info("establish succeeded, but more changes need fetching...")

                                self.fetchAndPersistChanges { fetchError in
                                    guard fetchError == nil else {
                                        // This is an odd error condition: we might be able to fetch again and be in a good state...
                                        logger.info("fetch-after-establish failed: \(String(describing: fetchError), privacy: .public)")
                                        reply(nil, keyHierarchyRecords, nil, fetchError)
                                        return
                                    }

                                    logger.info("fetch-after-establish succeeded")
                                    reply(egoPeerID, keyHierarchyRecords, syncingPolicy, nil)
                                }
                                return
                            }

                            logger.info("establish succeeded")
                            reply(egoPeerID, keyHierarchyRecords, syncingPolicy, nil)
                        } catch {
                            logger.error("establish handling failed: \(String(describing: error), privacy: .public)")
                            reply(nil, keyHierarchyRecords, nil, error)
                        }
                    case .failure(let error):
                        switch error {
                        case CuttlefishErrorMatcher(code: CuttlefishErrorCode.establishFailed):
                            logger.info("establish returned failed, trying fetch")
                            self.fetchAfterEstablish(ckksKeys: ckksKeys, tlkShares: tlkShares, reply: reply)
                            return
                        default:
                            logger.error("establish failed: \(String(describing: error), privacy: .public)")
                            reply(nil, [], nil, error)
                            return
                        }
                    }
                }
            }
        }
    }

    func setRecoveryKey(recoveryKey: String, salt: String, ckksKeys: [CKKSKeychainBackedKeySet], reply: @escaping ([CKRecord]?, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: ([CKRecord]?, Error?) -> Void = {
            let logType: OSLogType = $1 == nil ? .info : .error
            logger.log(level: logType, "setRecoveryKey complete: \(traceError($1), privacy: .public)")
            sem.release()
            reply($0, $1)
        }

        logger.info("beginning a setRecoveryKey")

        self.fetchAndPersistChanges { fetchError in
            guard fetchError == nil else {
                reply(nil, fetchError)
                return
            }

            self.moc.performAndWait {
                guard let egoPeerID = self.containerMO.egoPeerID else {
                    logger.info("no prepared identity, cannot set recovery key")
                    reply(nil, ContainerError.noPreparedIdentity)
                    return
                }

                var recoveryKeys: RecoveryKey
                do {
                    recoveryKeys = try RecoveryKey(recoveryKeyString: recoveryKey, recoverySalt: salt)
                    guard try self.model.anyTrustedPeerDistrustsOtherPeer(recoveryKeys.peerKeys.peerID) == false else {
                        logger.error("Recovery key is distrusted!")
                        reply(nil, ContainerError.untrustedRecoveryKeys)
                        return
                    }
                } catch {
                    logger.error("failed to create recovery keys: \(String(describing: error), privacy: .public)")
                    reply(nil, ContainerError.failedToCreateRecoveryKey(suberror: error))
                    return
                }

                let signingPublicKey: Data = recoveryKeys.peerKeys.signingVerificationKey.keyData
                let encryptionPublicKey: Data = recoveryKeys.peerKeys.encryptionVerificationKey.keyData

                logger.info("setRecoveryKey signingPubKey: \(signingPublicKey.base64EncodedString(), privacy: .public)")
                logger.info("setRecoveryKey encryptionPubKey: \(encryptionPublicKey.base64EncodedString(), privacy: .public)")

                guard let stableInfoData = self.containerMO.egoPeerStableInfo else {
                    logger.info("stableInfo does not exist")
                    reply(nil, ContainerError.nonMember)
                    return
                }
                guard let stableInfoSig = self.containerMO.egoPeerStableInfoSig else {
                    logger.info("stableInfoSig does not exist")
                    reply(nil, ContainerError.nonMember)
                    return
                }
                guard let permInfoData = self.containerMO.egoPeerPermanentInfo else {
                    logger.info("permanentInfo does not exist")
                    reply(nil, ContainerError.nonMember)
                    return
                }
                guard let permInfoSig = self.containerMO.egoPeerPermanentInfoSig else {
                    logger.info("permInfoSig does not exist")
                    reply(nil, ContainerError.nonMember)
                    return
                }
                guard let stableInfo = TPPeerStableInfo(data: stableInfoData, sig: stableInfoSig) else {
                    logger.info("cannot create TPPeerStableInfo")
                    reply(nil, ContainerError.invalidStableInfoOrSig)
                    return
                }
                let keyFactory = TPECPublicKeyFactory()
                guard let permanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: permInfoData, sig: permInfoSig, keyFactory: keyFactory) else {
                    logger.info("cannot create TPPeerPermanentInfo")
                    reply(nil, ContainerError.invalidStableInfoOrSig)
                    return
                }

                loadEgoKeyPair(identifier: signingKeyIdentifier(peerID: egoPeerID)) { signingKeyPair, error in
                    guard let signingKeyPair = signingKeyPair else {
                        logger.error("handle: no signing key pair: \(String(describing: error), privacy: .public)")
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
                                                                         walrusSetting: stableInfo.walrusSetting,
                                                                         webAccess: stableInfo.webAccess,
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
                                $0.changeToken = self.changeToken()
                            }

                            self.cuttlefish.setRecoveryKey(request) { response in
                                switch response {
                                case .success(let response):
                                    self.moc.performAndWait {
                                        do {
                                            self.containerMO.egoPeerStableInfo = updatedStableInfo.data
                                            self.containerMO.egoPeerStableInfoSig = updatedStableInfo.sig
                                            try self.onQueuePersist(changes: response.changes)

                                            logger.info("setRecoveryKey succeeded")

                                            let keyHierarchyRecords = response.zoneKeyHierarchyRecords.compactMap { CKRecord($0) }
                                            reply(keyHierarchyRecords, nil)
                                        } catch {
                                            logger.error("setRecoveryKey handling failed: \(String(describing: error), privacy: .public)")
                                            reply(nil, error)
                                        }
                                    }
                                case .failure(let error):
                                    logger.error("setRecoveryKey failed: \(String(describing: error), privacy: .public)")
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
    }

    func createCustodianRecoveryKey(recoveryKey: String,
                                    salt: String,
                                    ckksKeys: [CKKSKeychainBackedKeySet],
                                    uuid: UUID,
                                    kind: TPPBCustodianRecoveryKey_Kind,
                                    reply: @escaping ([CKRecord]?, TrustedPeersHelperCustodianRecoveryKey?, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: ([CKRecord]?, TrustedPeersHelperCustodianRecoveryKey?, Error?) -> Void = {
            let logType: OSLogType = $2 == nil ? .info : .error
            logger.log(level: logType, "createCustodianRecoveryKey complete: \(traceError($2), privacy: .public)")
            sem.release()
            reply($0, $1, $2)
        }

        logger.info("beginning a createCustodianRecoveryKey")

        self.moc.performAndWait {
            guard self.model.findCustodianRecoveryKey(with: uuid) == nil else {
                logger.error("CRK UUID \(uuid) already exists")
                reply(nil, nil, ContainerError.custodianRecoveryKeyUUIDExists)
                return
            }

            guard let egoPeerID = self.containerMO.egoPeerID else {
                logger.info("no prepared identity, cannot create custodian recovery key")
                reply(nil, nil, ContainerError.noPreparedIdentity)
                return
            }

            let crk: CustodianRecoveryKey
            do {
                crk = try CustodianRecoveryKey(uuid: uuid, recoveryKeyString: recoveryKey, recoverySalt: salt, kind: kind)
            } catch {
                logger.error("failed to create custodian recovery keys: \(String(describing: error), privacy: .public)")
                reply(nil, nil, ContainerError.failedToCreateRecoveryKey(suberror: error))
                return
            }

            logger.info("createCustodianRecoveryKey signingPubKey: \(crk.tpCustodian.signingPublicKey.spki().base64EncodedString(), privacy: .public)")
            logger.info("createCustodianRecoveryKey encryptionPubKey: \(crk.tpCustodian.encryptionPublicKey.spki().base64EncodedString(), privacy: .public)")

            guard let permInfoData = self.containerMO.egoPeerPermanentInfo else {
                logger.info("permanentInfo does not exist")
                reply(nil, nil, ContainerError.nonMember)
                return
            }
            guard let permInfoSig = self.containerMO.egoPeerPermanentInfoSig else {
                logger.info("permInfoSig does not exist")
                reply(nil, nil, ContainerError.nonMember)
                return
            }
            let keyFactory = TPECPublicKeyFactory()
            guard let permanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: permInfoData, sig: permInfoSig, keyFactory: keyFactory) else {
                logger.info("cannot create TPPeerPermanentInfo")
                reply(nil, nil, ContainerError.invalidPermanentInfoOrSig)
                return
            }
            guard let stableInfoData = self.containerMO.egoPeerStableInfo else {
                logger.info("stableInfo does not exist")
                reply(nil, nil, ContainerError.invalidStableInfoOrSig)
                return
            }
            guard let stableInfoSig = self.containerMO.egoPeerStableInfoSig else {
                logger.info("stableInfoSig does not exist")
                reply(nil, nil, ContainerError.invalidStableInfoOrSig)
                return
            }
            guard let stableInfo = TPPeerStableInfo(data: stableInfoData, sig: stableInfoSig) else {
                logger.info("cannot create TPPeerStableInfo")
                reply(nil, nil, ContainerError.invalidStableInfoOrSig)
                return
            }
            loadEgoKeyPair(identifier: signingKeyIdentifier(peerID: egoPeerID)) { signingKeyPair, error in
                guard let signingKeyPair = signingKeyPair else {
                    logger.error("handle: no signing key pair: \(String(describing: error), privacy: .public)")
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
                            logger.error("Error preparing dynamic info: \(String(describing: error), privacy: .public)")
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
                            $0.changeToken = self.changeToken()
                        }

                        self.cuttlefish.addCustodianRecoveryKey(request) { response in
                            switch response {
                            case .success(let response):
                                self.moc.performAndWait {
                                    do {
                                        try self.onQueuePersist(changes: response.changes)

                                        logger.info("CreateCustodianRecoveryKey succeeded")

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
                                        logger.error("CreateCustodianRecoveryKey handling failed: \(String(describing: error), privacy: .public)")
                                        reply(nil, nil, error)
                                        return
                                    }
                                }
                            case .failure(let error):
                                logger.error("CreateCustodianRecoveryKey failed: \(String(describing: error), privacy: .public)")
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
        let sem = self.grabSemaphore()
        let reply: (Error?) -> Void = {
            let logType: OSLogType = $0 == nil ? .info : .error
            logger.log(level: logType, "removeCustodianRecoveryKey complete: \(traceError($0), privacy: .public)")
            sem.release()
            reply($0)
        }

        self.moc.performAndWait {
            guard let egoPeerID = self.containerMO.egoPeerID else {
                logger.info("No dynamic info for self?")
                reply(ContainerError.noPreparedIdentity)
                return
            }

            guard let tpcrk = self.model.findCustodianRecoveryKey(with: uuid) else {
                logger.info("Unable to find custodian recovery key \(uuid, privacy: .public) on model")
                reply(ContainerError.recoveryKeysNotEnrolled)
                return
            }

            let crkPeerID = tpcrk.peerID

            // This would be a strange case (where we confuse the ego peerID with the CRK peerID), but it's clearly an error.
            guard crkPeerID != egoPeerID else {
                logger.info("Self-distrust via peerID not allowed")
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

    func findCustodianRecoveryKey(uuid: UUID,
                                  reply: @escaping (TrustedPeersHelperCustodianRecoveryKey?, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (TrustedPeersHelperCustodianRecoveryKey?, Error?) -> Void = {
            let logType: OSLogType = $1 == nil ? .info : .error
            logger.log(level: logType, "findCustodianRecoveryKey complete: \(traceError($1), privacy: .public)")
            sem.release()
            reply($0, $1)
        }

        self.moc.performAndWait {
            let tpcrk = self.model.findCustodianRecoveryKey(with: uuid)
            guard let tpcrk else {
                reply(nil, nil)
                return
            }

            do {
                guard try self.model.isCustodianRecoveryKeyTrusted(tpcrk) else {
                    logger.debug("CRK \(tpcrk.peerID) is not trusted")
                    reply(nil, ContainerError.untrustedRecoveryKeys)
                    return
                }
            } catch {
                logger.error("error determine whether CRK is trusted: \(error, privacy: .public)")
                reply(nil, error)
                return
            }

            let tphcrk = TrustedPeersHelperCustodianRecoveryKey(uuid: uuid.uuidString,
                                                                encryptionKey: tpcrk.encryptionPublicKey.spki(),
                                                                signingKey: tpcrk.signingPublicKey.spki(),
                                                                recoveryString: nil,
                                                                salt: nil,
                                                                kind: tpcrk.kind)
            reply(tphcrk, nil)
        }
    }

    func vouchWithBottle(bottleID: String,
                         entropy: Data,
                         bottleSalt: String,
                         tlkShares: [CKKSTLKShare],
                         reply: @escaping (Data?, Data?, [CKKSTLKShare]?, TrustedPeersHelperTLKRecoveryResult?, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (Data?, Data?, [CKKSTLKShare]?, TrustedPeersHelperTLKRecoveryResult?, Error?) -> Void = {
            let logType: OSLogType = $4 == nil ? .info : .error
            logger.log(level: logType, "vouchWithBottle complete: \(traceError($4), privacy: .public)")
            sem.release()
            reply($0, $1, $2, $3, $4)
        }

        // A preflight should have been successful before calling this function. So, we can assume that all required data is stored locally.

        self.moc.performAndWait {
            let bmo: BottleMO

            do {
                (bmo, _, _) = try self.onMOCQueuePerformPreflight(bottleID: bottleID)
            } catch {
                logger.error("vouchWithBottle failed preflight: \(String(describing: error), privacy: .public)")
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
                guard let sponsorPeer = try self.model.peer(withID: sponsorPeerID) else {
                    logger.info("vouchWithBottle: Unable to find peer that created the bottle")
                    reply(nil, nil, nil, nil, ContainerError.bottleCreatingPeerNotFound)
                    return
                }
                guard let signingKey: _SFECPublicKey = sponsorPeer.permanentInfo.signingPubKey as? _SFECPublicKey else {
                    logger.info("vouchWithBottle: Unable to create a sponsor public key")
                    reply(nil, nil, nil, nil, ContainerError.signatureVerificationFailed)
                    return
                }

                _ = try BottledPeer.verifyBottleSignature(data: bottledContents, signature: signatureUsingPeerKey, pubKey: signingKey)
            } catch {
                logger.error("vouchWithBottle: Verification of bottled signature failed: \(String(describing: error), privacy: .public)")
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
                logger.info("Creation of Bottled Peer failed with bottle salt: \(bottleSalt, privacy: .public)\nAttempting with empty bottle salt")

                do {
                    bottledPeer = try BottledPeer(contents: bottledContents,
                                                  secret: entropy,
                                                  bottleSalt: "",
                                                  signatureUsingEscrow: signatureUsingEscrowKey,
                                                  signatureUsingPeerKey: signatureUsingPeerKey)
                } catch {
                    logger.error("Creation of Bottled Peer failed: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, nil, nil, ContainerError.failedToCreateBottledPeer)
                    return
                }
            }

            logger.info("Have a bottle for peer \(bottledPeer.peerID, privacy: .public)")

            // Extract any TLKs we have been given
            let (recoveredTLKs, recoveryResult) = extract(tlkShares: tlkShares, peer: bottledPeer.peerKeys, model: self.model)

            self.moc.performAndWait {
                // I must have an ego identity in order to vouch using bottle
                guard let egoPeerID = self.containerMO.egoPeerID else {
                    logger.info("As a nonmember, can't vouch for someone else")
                    reply(nil, nil, nil, nil, ContainerError.nonMember)
                    return
                }
                guard let permanentInfo = self.containerMO.egoPeerPermanentInfo else {
                    logger.info("permanentInfo does not exist")
                    reply(nil, nil, nil, nil, ContainerError.nonMember)
                    return
                }
                guard let permanentInfoSig = self.containerMO.egoPeerPermanentInfoSig else {
                    logger.info("permanentInfoSig does not exist")
                    reply(nil, nil, nil, nil, ContainerError.nonMember)
                    return
                }
                guard let stableInfo = self.containerMO.egoPeerStableInfo else {
                    logger.info("stableInfo does not exist")
                    reply(nil, nil, nil, nil, ContainerError.nonMember)
                    return
                }
                guard let stableInfoSig = self.containerMO.egoPeerStableInfoSig else {
                    logger.info("stableInfoSig does not exist")
                    reply(nil, nil, nil, nil, ContainerError.nonMember)
                    return
                }
                let keyFactory = TPECPublicKeyFactory()
                guard let beneficiaryPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: permanentInfo, sig: permanentInfoSig, keyFactory: keyFactory) else {
                    logger.info("Invalid permenent info or signature; can't vouch for them")
                    reply(nil, nil, nil, nil, ContainerError.invalidPermanentInfoOrSig)
                    return
                }
                guard let beneficiaryStableInfo = TPPeerStableInfo(data: stableInfo, sig: stableInfoSig) else {
                    logger.info("Invalid stableinfo or signature; van't vouch for them")
                    reply(nil, nil, nil, nil, ContainerError.invalidStableInfoOrSig)
                    return
                }
                loadEgoKeys(peerID: egoPeerID) { egoPeerKeys, error in
                    guard let egoPeerKeys = egoPeerKeys else {
                        logger.error("Error loading ego peer keys: \(String(describing: error), privacy: .public)")
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
                        logger.error("Error creating voucher with bottle: \(String(describing: error), privacy: .public)")
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
        let sem = self.grabSemaphore()
        let reply: (Data?, Data?, [CKKSTLKShare]?, TrustedPeersHelperTLKRecoveryResult?, Error?) -> Void = {
            let logType: OSLogType = $4 == nil ? .info : .error
            logger.log(level: logType, "vouchWithRecoveryKey complete: \(traceError($4), privacy: .public)")
            sem.release()
            reply($0, $1, $2, $3, $4)
        }

        self.moc.performAndWait {
            logger.info("beginning a vouchWithRecoveryKey")

            // I must have an ego identity in order to vouch using recovery key
            guard let egoPeerID = self.containerMO.egoPeerID else {
                logger.info("As a nonmember, can't vouch for someone else")
                reply(nil, nil, nil, nil, ContainerError.nonMember)
                return
            }
            guard let permanentInfo = self.containerMO.egoPeerPermanentInfo else {
                logger.info("permanentInfo does not exist")
                reply(nil, nil, nil, nil, ContainerError.nonMember)
                return
            }
            guard let permanentInfoSig = self.containerMO.egoPeerPermanentInfoSig else {
                logger.info("permanentInfoSig does not exist")
                reply(nil, nil, nil, nil, ContainerError.nonMember)
                return
            }
            guard let stableInfo = self.containerMO.egoPeerStableInfo else {
                logger.info("stableInfo does not exist")
                reply(nil, nil, nil, nil, ContainerError.nonMember)
                return
            }
            guard let stableInfoSig = self.containerMO.egoPeerStableInfoSig else {
                logger.info("stableInfoSig does not exist")
                reply(nil, nil, nil, nil, ContainerError.nonMember)
                return
            }
            let keyFactory = TPECPublicKeyFactory()
            guard let beneficiaryPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: permanentInfo, sig: permanentInfoSig, keyFactory: keyFactory) else {
                logger.info("Invalid permenent info or signature; can't vouch for them")
                reply(nil, nil, nil, nil, ContainerError.invalidPermanentInfoOrSig)
                return
            }
            guard let beneficiaryStableInfo = TPPeerStableInfo(data: stableInfo, sig: stableInfoSig) else {
                logger.info("Invalid stableinfo or signature; van't vouch for them")
                reply(nil, nil, nil, nil, ContainerError.invalidStableInfoOrSig)
                return
            }

            loadEgoKeys(peerID: egoPeerID) { egoPeerKeys, error in
                guard let egoPeerKeys = egoPeerKeys else {
                    logger.error("Don't have my own peer keys; can't vouch with recovery key: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, nil, nil, error)
                    return
                }

                // create recovery key set
                var recoveryKeys: RecoveryKey
                do {
                    recoveryKeys = try RecoveryKey(recoveryKeyString: recoveryKey, recoverySalt: salt)
                } catch {
                    logger.error("failed to create recovery keys: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, nil, nil, ContainerError.failedToCreateRecoveryKey(suberror: error))
                    return
                }

                let signingPublicKey: Data = recoveryKeys.peerKeys.signingKey.publicKey.keyData
                let encryptionPublicKey: Data = recoveryKeys.peerKeys.encryptionKey.publicKey.keyData

                logger.info("vouchWithRecoveryKey signingPubKey: \(signingPublicKey.base64EncodedString(), privacy: .public)")
                logger.info("vouchWithRecoveryKey encryptionPubKey: \(encryptionPublicKey.base64EncodedString(), privacy: .public)")

                do {
                    guard try self.model.isRecoveryKeyEnrolled() else {
                        logger.info("Recovery Key is not enrolled")
                        reply(nil, nil, nil, nil, ContainerError.recoveryKeysNotEnrolled)
                        return
                    }
                } catch {
                    logger.warning("Error determining whether Recovery Key is enrolled: \(error, privacy: .public)")
                    reply(nil, nil, nil, nil, error)
                    return
                }

                // find matching peer containing recovery keys
                let sponsorPeerID: String?
                do {
                    sponsorPeerID = try self.model.peerIDThatTrustsRecoveryKeys(TPRecoveryKeyPair(signingKeyData: signingPublicKey,
                                                                                                    encryptionKeyData: encryptionPublicKey),
                                                                                  canIntroducePeer: beneficiaryPermanentInfo,
                                                                                  stableInfo: beneficiaryStableInfo)
                } catch {
                    logger.error("Failed to get peer that trusts RK: \(error, privacy: .public)")
                    reply(nil, nil, nil, nil, error)
                    return
                }
                guard let sponsorPeerID else {
                    logger.info("Untrusted recovery key set")
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
                    logger.error("Error creating voucher using recovery key set: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, nil, nil, error)
                    return
                }
            }
        }
    }

    func recoverTLKSharesForInheritor(crk: TrustedPeersHelperCustodianRecoveryKey,
                                      tlkShares: [CKKSTLKShare],
                                      reply: @escaping ([CKKSTLKShare]?, TrustedPeersHelperTLKRecoveryResult?, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: ([CKKSTLKShare]?, TrustedPeersHelperTLKRecoveryResult?, Error?) -> Void = {
            let logType: OSLogType = $2 == nil ? .info : .error
            logger.log(level: logType, "recoverTLKSharesForInheritor complete: \(traceError($2), privacy: .public)")
            sem.release()
            reply($0, $1, $2)
        }

        self.moc.performAndWait {
            logger.info("beginning a recoverTLKSharesForInheritor")

                // I must have an ego identity in order to vouch using bottle
            guard let egoPeerID = self.containerMO.egoPeerID else {
                logger.info("As a nonmember, can't vouch for someone else")
                reply(nil, nil, ContainerError.nonMember)
                return
            }
            guard let permanentInfo = self.containerMO.egoPeerPermanentInfo else {
                logger.info("permanentInfo does not exist")
                reply(nil, nil, ContainerError.nonMember)
                return
            }
            guard let permanentInfoSig = self.containerMO.egoPeerPermanentInfoSig else {
                logger.info("permanentInfoSig does not exist")
                reply(nil, nil, ContainerError.nonMember)
                return
            }

            let keyFactory = TPECPublicKeyFactory()
            guard let beneficiaryPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: permanentInfo, sig: permanentInfoSig, keyFactory: keyFactory) else {
                logger.info("Invalid permenent info or signature; can't vouch for them")
                reply(nil, nil, ContainerError.invalidPermanentInfoOrSig)
                return
            }

            loadEgoKeys(peerID: egoPeerID) { egoPeerKeys, error in
                guard let egoPeerKeys = egoPeerKeys else {
                    logger.error("Don't have my own peer keys; can't establish: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, error)
                    return
                }

                guard let uuid = UUID(uuidString: crk.uuid) else {
                    logger.info("Unable to parse uuid: \(crk.uuid, privacy: .public)")
                    reply(nil, nil, ContainerError.recoveryKeysNotEnrolled)
                    return
                }

                guard let tpcrk = self.model.findCustodianRecoveryKey(with: uuid) else {
                    logger.info("Unable to find custodian recovery key \(crk.uuid, privacy: .public) on model")
                    reply(nil, nil, ContainerError.recoveryKeysNotEnrolled)
                    return
                }

                do {
                    guard try self.model.isCustodianRecoveryKeyTrusted(tpcrk) else {
                        logger.info("Custodian Recovery Key is not trusted")
                        reply(nil, nil, ContainerError.untrustedRecoveryKeys)
                        return
                    }
                } catch {
                    logger.info("error determining whether Custodian Recovery Key is trusted: \(error, privacy: .public)")
                    reply(nil, nil, error)
                    return
                }

                guard let recoveryKeyString = crk.recoveryString, let recoverySalt = crk.salt else {
                    logger.info("Bad format CRK: recovery string or salt not set")
                    reply(nil, nil, ContainerError.custodianRecoveryKeyMalformed)
                    return
                }

                // create recovery key set
                let recoveryCRK: CustodianRecoveryKey
                do {
                    recoveryCRK = try CustodianRecoveryKey(tpCustodian: tpcrk, recoveryKeyString: recoveryKeyString, recoverySalt: recoverySalt)
                } catch {
                    logger.error("failed to create custodian recovery keys: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, ContainerError.failedToCreateRecoveryKey(suberror: error))
                    return
                }

                logger.info("recoverTLKSharesForInheritor signingPubKey: \(recoveryCRK.peerKeys.signingKey.publicKey.keyData.base64EncodedString(), privacy: .public)")
                logger.info("recoverTLKSharesForInheritor encryptionPubKey: \(recoveryCRK.peerKeys.encryptionKey.publicKey.keyData.base64EncodedString(), privacy: .public)")

                // find matching peer trusting custodian recovery keys
                let sponsorPeerID: String?
                do {
                    sponsorPeerID = try self.model.peerIDThatTrustsCustodianRecoveryKeys(tpcrk,
                                                                                         canIntroducePeer: beneficiaryPermanentInfo,
                                                                                         stableInfo: nil)
                } catch {
                    logger.error("Error getting peer that trusts CRK: \(error, privacy: .public)")
                    reply(nil, nil, error)
                    return
                }
                guard let sponsorPeerID else {
                    logger.info("Untrusted custodian recovery key set")
                    reply(nil, nil, ContainerError.untrustedRecoveryKeys)
                    return
                }

                let (recoveredTLKs, tlkRecoveryResult) = extract(tlkShares: tlkShares, peer: recoveryCRK.peerKeys, sponsorPeerID: sponsorPeerID, model: self.model)

                do {
                    let newSelfTLKShares = try makeCKKSTLKShares(ckksTLKs: recoveredTLKs, asPeer: egoPeerKeys, toPeer: egoPeerKeys, epoch: Int(beneficiaryPermanentInfo.epoch))
                    reply(newSelfTLKShares, tlkRecoveryResult, nil)
                    return
                } catch {
                    logger.error("Error making CKKSTLKShares for inheritor: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, error)
                    return
                }
            }
        }
    }

    func vouchWithCustodianRecoveryKey(crk: TrustedPeersHelperCustodianRecoveryKey,
                                       tlkShares: [CKKSTLKShare],
                                       reply: @escaping (Data?, Data?, [CKKSTLKShare]?, TrustedPeersHelperTLKRecoveryResult?, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (Data?, Data?, [CKKSTLKShare]?, TrustedPeersHelperTLKRecoveryResult?, Error?) -> Void = {
            let logType: OSLogType = $4 == nil ? .info : .error
            logger.log(level: logType, "vouchWithCustodianRecoveryKey complete: \(traceError($4), privacy: .public)")
            sem.release()
            reply($0, $1, $2, $3, $4)
        }

        self.moc.performAndWait {
            logger.info("beginning a vouchWithCustodianRecoveryKey")

            // I must have an ego identity in order to vouch using bottle
            guard let egoPeerID = self.containerMO.egoPeerID else {
                logger.info("As a nonmember, can't vouch for someone else")
                reply(nil, nil, nil, nil, ContainerError.nonMember)
                return
            }
            guard let permanentInfo = self.containerMO.egoPeerPermanentInfo else {
                logger.info("permanentInfo does not exist")
                reply(nil, nil, nil, nil, ContainerError.nonMember)
                return
            }
            guard let permanentInfoSig = self.containerMO.egoPeerPermanentInfoSig else {
                logger.info("permanentInfoSig does not exist")
                reply(nil, nil, nil, nil, ContainerError.nonMember)
                return
            }
            guard let stableInfo = self.containerMO.egoPeerStableInfo else {
                logger.info("stableInfo does not exist")
                reply(nil, nil, nil, nil, ContainerError.nonMember)
                return
            }
            guard let stableInfoSig = self.containerMO.egoPeerStableInfoSig else {
                logger.info("stableInfoSig does not exist")
                reply(nil, nil, nil, nil, ContainerError.nonMember)
                return
            }
            let keyFactory = TPECPublicKeyFactory()
            guard let beneficiaryPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: permanentInfo, sig: permanentInfoSig, keyFactory: keyFactory) else {
                logger.info("Invalid permenent info or signature; can't vouch for them")
                reply(nil, nil, nil, nil, ContainerError.invalidPermanentInfoOrSig)
                return
            }
            guard let beneficiaryStableInfo = TPPeerStableInfo(data: stableInfo, sig: stableInfoSig) else {
                logger.info("Invalid stableinfo or signature; van't vouch for them")
                reply(nil, nil, nil, nil, ContainerError.invalidStableInfoOrSig)
                return
            }

            loadEgoKeys(peerID: egoPeerID) { egoPeerKeys, error in
                guard let egoPeerKeys = egoPeerKeys else {
                    logger.error("Don't have my own peer keys; can't establish: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, nil, nil, error)
                    return
                }

                guard let uuid = UUID(uuidString: crk.uuid) else {
                    logger.info("Unable to parse uuid \(crk.uuid, privacy: .public)")
                    reply(nil, nil, nil, nil, ContainerError.recoveryKeysNotEnrolled)
                    return
                }

                guard let tpcrk = self.model.findCustodianRecoveryKey(with: uuid) else {
                    logger.info("Unable to find custodian recovery key \(crk.uuid, privacy: .public) on model")
                    reply(nil, nil, nil, nil, ContainerError.recoveryKeysNotEnrolled)
                    return
                }

                do {
                    guard try self.model.isCustodianRecoveryKeyTrusted(tpcrk) else {
                        logger.info("Custodian Recovery Key is not trusted")
                        reply(nil, nil, nil, nil, ContainerError.untrustedRecoveryKeys)
                        return
                    }
                } catch {
                    logger.info("Error determining whether Custodian Recovery Key is trusted: \(error, privacy: .public)")
                    reply(nil, nil, nil, nil, error)
                    return
                }

                guard let recoveryKeyString = crk.recoveryString, let recoverySalt = crk.salt else {
                    logger.info("Bad format CRK: recovery string or salt not set")
                    reply(nil, nil, nil, nil, ContainerError.custodianRecoveryKeyMalformed)
                    return
                }

                // create recovery key set
                let recoveryCRK: CustodianRecoveryKey
                do {
                    recoveryCRK = try CustodianRecoveryKey(tpCustodian: tpcrk, recoveryKeyString: recoveryKeyString, recoverySalt: recoverySalt)
                } catch {
                    logger.error("failed to create custodian recovery keys: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, nil, nil, ContainerError.failedToCreateRecoveryKey(suberror: error))
                    return
                }

                logger.info("vouchWithCustodianRecoveryKey signingPubKey: \(recoveryCRK.peerKeys.signingKey.publicKey.keyData.base64EncodedString(), privacy: .public)")
                logger.info("vouchWithCustodianRecoveryKey encryptionPubKey: \(recoveryCRK.peerKeys.encryptionKey.publicKey.keyData.base64EncodedString(), privacy: .public)")

                // find matching peer trusting custodian recovery keys
                let sponsorPeerID: String?
                do {
                    sponsorPeerID = try self.model.peerIDThatTrustsCustodianRecoveryKeys(tpcrk,
                                                                                         canIntroducePeer: beneficiaryPermanentInfo,
                                                                                         stableInfo: beneficiaryStableInfo)
                } catch {
                    logger.info("Error getting peer that trusts CRK: \(error, privacy: .public)")
                    reply(nil, nil, nil, nil, error)
                    return
                }
                guard let sponsorPeerID else {
                    logger.info("Untrusted custodian recovery key set")
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
                    logger.error("Error creating voucher using custodian recovery key set: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, nil, nil, error)
                    return
                }
            }
        }
    }

    func vouchWithReroll(oldPeerID: String,
                         tlkShares: [CKKSTLKShare],
                         reply: @escaping (Data?, Data?, [CKKSTLKShare]?, TrustedPeersHelperTLKRecoveryResult?, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (Data?, Data?, [CKKSTLKShare]?, TrustedPeersHelperTLKRecoveryResult?, Error?) -> Void = {
            let logType: OSLogType = $4 == nil ? .info : .error
            logger.log(level: logType, "vouchWithReroll complete: \(traceError($4), privacy: .public)")
            sem.release()
            reply($0, $1, $2, $3, $4)
        }

        self.moc.performAndWait {
            logger.info("beginning a vouchWithReroll")

            guard let egoPeerID = self.containerMO.egoPeerID else {
                logger.info("As a nonmember, can't vouch for someone else")
                reply(nil, nil, nil, nil, ContainerError.nonMember)
                return
            }
            guard let permanentInfo = self.containerMO.egoPeerPermanentInfo else {
                logger.info("permanentInfo does not exist")
                reply(nil, nil, nil, nil, ContainerError.nonMember)
                return
            }
            guard let permanentInfoSig = self.containerMO.egoPeerPermanentInfoSig else {
                logger.info("permanentInfoSig does not exist")
                reply(nil, nil, nil, nil, ContainerError.nonMember)
                return
            }
            guard let stableInfo = self.containerMO.egoPeerStableInfo else {
                logger.info("stableInfo does not exist")
                reply(nil, nil, nil, nil, ContainerError.nonMember)
                return
            }
            guard let stableInfoSig = self.containerMO.egoPeerStableInfoSig else {
                logger.info("stableInfoSig does not exist")
                reply(nil, nil, nil, nil, ContainerError.nonMember)
                return
            }
            let keyFactory = TPECPublicKeyFactory()
            guard let beneficiaryPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: permanentInfo, sig: permanentInfoSig, keyFactory: keyFactory) else {
                logger.info("Invalid permenent info or signature; can't vouch for them")
                reply(nil, nil, nil, nil, ContainerError.invalidPermanentInfoOrSig)
                return
            }
            guard let beneficiaryStableInfo = TPPeerStableInfo(data: stableInfo, sig: stableInfoSig) else {
                logger.info("Invalid stableinfo or signature; van't vouch for them")
                reply(nil, nil, nil, nil, ContainerError.invalidStableInfoOrSig)
                return
            }

            loadEgoKeys(peerID: egoPeerID) { egoPeerKeys, error in
                guard let egoPeerKeys = egoPeerKeys else {
                    logger.error("Don't have my own peer keys; can't establish: \(String(describing: error), privacy: .public)")
                    reply(nil, nil, nil, nil, error)
                    return
                }

                loadEgoKeys(peerID: oldPeerID) { oldPeerKeys, error in
                    guard let oldPeerKeys else {
                        logger.error("Don't have my own peer keys; can't establish: \(String(describing: error), privacy: .public)")
                        reply(nil, nil, nil, nil, error)
                        return
                    }

                    let (recoveredTLKs, tlkRecoveryResult) = extract(tlkShares: tlkShares, peer: oldPeerKeys, sponsorPeerID: oldPeerID, model: self.model)

                    do {
                        let voucher = try self.model.createVoucher(forCandidate: beneficiaryPermanentInfo,
                                                                   stableInfo: beneficiaryStableInfo,
                                                                   withSponsorID: oldPeerID,
                                                                   reason: TPVoucherReason.sameDevice,
                                                                   signing: oldPeerKeys.signingKey)

                        let newSelfTLKShares = try makeCKKSTLKShares(ckksTLKs: recoveredTLKs, asPeer: egoPeerKeys, toPeer: egoPeerKeys, epoch: Int(beneficiaryPermanentInfo.epoch))
                        reply(voucher.data, voucher.sig, newSelfTLKShares, tlkRecoveryResult, nil)
                        return
                    } catch {
                        logger.error("Error creating voucher using reroll: \(String(describing: error), privacy: .public)")
                        reply(nil, nil, nil, nil, error)
                        return
                    }
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
               altDSID: String?,
               flowID: String?,
               deviceSessionID: String?,
               canSendMetrics: Bool,
               reply: @escaping (Data?, Data?, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (Data?, Data?, Error?) -> Void = {
            let logType: OSLogType = $2 == nil ? .info : .error
            logger.log(level: logType, "vouch complete: \(traceError($2), privacy: .public)")
            sem.release()
            reply($0, $1, $2)
        }

        self.moc.performAndWait {
            // I must have an ego identity in order to vouch for someone else.
            guard let egoPeerID = self.containerMO.egoPeerID,
                let egoPermData = self.containerMO.egoPeerPermanentInfo,
                let egoPermSig = self.containerMO.egoPeerPermanentInfoSig else {
                    logger.info("As a nonmember, can't vouch for someone else")
                    reply(nil, nil, ContainerError.nonMember)
                    return
            }

            let keyFactory = TPECPublicKeyFactory()

            guard let selfPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: egoPermData, sig: egoPermSig, keyFactory: keyFactory) else {
                reply(nil, nil, ContainerError.invalidPermanentInfoOrSig)
                return
            }

            guard let beneficiaryPermanentInfo = TPPeerPermanentInfo(peerID: peerID, data: permanentInfo, sig: permanentInfoSig, keyFactory: keyFactory) else {
                logger.info("Invalid permenent info or signature; can't vouch for them")
                reply(nil, nil, ContainerError.invalidPermanentInfoOrSig)
                return
            }

            guard let beneficiaryStableInfo = TPPeerStableInfo(data: stableInfo, sig: stableInfoSig) else {
                logger.info("Invalid stableinfo or signature; van't vouch for them")
                reply(nil, nil, ContainerError.invalidStableInfoOrSig)
                return
            }

            loadEgoKeys(peerID: egoPeerID) { egoPeerKeys, error in
                guard let egoPeerKeys = egoPeerKeys else {
                    logger.error("Don't have my own keys: can't vouch for \(peerID, privacy: .public)(\(beneficiaryPermanentInfo, privacy: .public)): \(String(describing: error), privacy: .public)")
                    reply(nil, nil, error)
                    return
                }

                let eventS = AAFAnalyticsEventSecurity(keychainCircleMetrics: nil,
                                                       altDSID: altDSID,
                                                       flowID: flowID,
                                                       deviceSessionID: deviceSessionID,
                                                       eventName: kSecurityRTCEventNameFetchPolicyDocument,
                                                       testsAreEnabled: soft_MetricsOverrideTestsAreEnabled(),
                                                       canSendMetrics: canSendMetrics,
                                                       category: kSecurityRTCEventCategoryAccountDataAccessRecovery)

                self.fetchPolicyDocumentsWithSemaphore(versions: Set([beneficiaryStableInfo.bestPolicyVersion()])) { _, policyFetchError in
                    guard policyFetchError == nil else {
                        logger.error("Unknown policy for beneficiary: \(String(describing: error), privacy: .public)")
                        SecurityAnalyticsReporterRTC.sendMetric(withEvent: eventS, success: false, error: policyFetchError)
                        reply(nil, nil, policyFetchError)
                        return
                    }

                    SecurityAnalyticsReporterRTC.sendMetric(withEvent: eventS, success: true, error: nil)

                    self.moc.performAndWait {
                        let voucher: TPVoucher
                        do {
                            voucher = try self.model.createVoucher(forCandidate: beneficiaryPermanentInfo,
                                                                   stableInfo: beneficiaryStableInfo,
                                                                   withSponsorID: egoPeerID,
                                                                   reason: TPVoucherReason.secureChannel,
                                                                   signing: egoPeerKeys.signingKey)
                        } catch {
                            logger.error("Error creating voucher: \(String(describing: error), privacy: .public)")
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
                            logger.error("Unable to make TLKShares for beneficiary \(peerID, privacy: .public)(\(beneficiaryPermanentInfo, privacy: .public)): \(String(describing: error), privacy: .public)")
                            reply(nil, nil, error)
                            return
                        }

                        guard !tlkShares.isEmpty else {
                            logger.info("No TLKShares to upload for new peer, returning voucher")
                            reply(voucher.data, voucher.sig, nil)
                            return
                        }

                        let metrics = Metrics.with {
                            $0.flowID = flowID ?? String()
                            $0.deviceSessionID = deviceSessionID ?? String()
                        }

                        self.cuttlefish.updateTrust(changeToken: self.changeToken(),
                                                    peerID: egoPeerID,
                                                    stableInfoAndSig: nil,
                                                    dynamicInfoAndSig: nil,
                                                    tlkShares: tlkShares,
                                                    viewKeys: [],
                                                    metrics: metrics) { response in
                            switch response {
                            case .success(let response):
                                let newKeyRecords = response.zoneKeyHierarchyRecords.map(CKRecord.init)
                                logger.info("Uploaded new tlkshares: \(newKeyRecords, privacy: .public)")
                                // We don't need to save these; CKKS will refetch them as needed

                                reply(voucher.data, voucher.sig, nil)
                            case .failure(let error):
                                logger.error("Unable to upload new tlkshares: \(String(describing: error), privacy: .public)")
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
        let sem = self.grabSemaphore()
        let reply: (Error?) -> Void = {
            let logType: OSLogType = $0 == nil ? .info : .error
            logger.log(level: logType, "departByDistrustingSelf complete: \(traceError($0), privacy: .public)")
            sem.release()
            reply($0)
        }

        self.moc.performAndWait {
            guard let egoPeerID = self.containerMO.egoPeerID else {
                logger.info("No dynamic info for self?")
                reply(ContainerError.noPreparedIdentity)
                return
            }

            self.onqueueDistrust(peerIDs: [egoPeerID], reply: reply)
        }
    }

    func distrust(peerIDs: Set<String>,
                  reply: @escaping (Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (Error?) -> Void = {
            let logType: OSLogType = $0 == nil ? .info : .error
            logger.log(level: logType, "distrust complete: \(traceError($0), privacy: .public)")
            sem.release()
            reply($0)
        }

        self.moc.performAndWait {
            guard let egoPeerID = self.containerMO.egoPeerID else {
                logger.info("No dynamic info for self?")
                reply(ContainerError.noPreparedIdentity)
                return
            }

            guard !peerIDs.contains(egoPeerID) else {
                logger.info("Self-distrust via peerID not allowed")
                reply(ContainerError.invalidPeerID)
                return
            }

            self.onqueueDistrust(peerIDs: peerIDs, reply: reply)
        }
    }

    func idmsTrustedDevicesVersion() -> IdmsTrustedDevicesVersion {
        IdmsTrustedDevicesVersion.with {
            $0.idmsTrustedDevicesVersionString = self.containerMO.idmsTrustedDevicesVersion ?? "unknown"
            $0.timestamp = Google_Protobuf_Timestamp(date: self.containerMO.idmsTrustedDeviceListFetchDate ?? Date(timeIntervalSince1970: 0))
        }
    }

    func changeToken() -> String {
        self.containerMO.changeToken ?? ""
    }

    func onqueueDistrust(peerIDs: Set<String>,
                         reply: @escaping (Error?) -> Void) {
        guard let egoPeerID = self.containerMO.egoPeerID else {
            logger.info("No dynamic info for self?")
            reply(ContainerError.noPreparedIdentity)
            return
        }

        loadEgoKeyPair(identifier: signingKeyIdentifier(peerID: egoPeerID)) { signingKeyPair, error in
            guard let signingKeyPair = signingKeyPair else {
                logger.error("No longer have signing key pair; can't sign distrust: \(String(describing: error), privacy: .public)")
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
                    logger.error("Error preparing dynamic info: \(String(describing: error), privacy: .public)")
                    reply(error)
                    return
                }

                let signedDynamicInfo = SignedPeerDynamicInfo(dynamicInfo)
                logger.info("attempting distrust for \(peerIDs, privacy: .public) with: \(dynamicInfo, privacy: .public)")

                let request = UpdateTrustRequest.with {
                    $0.changeToken = self.changeToken()
                    $0.peerID = egoPeerID
                    $0.dynamicInfoAndSig = signedDynamicInfo
                    $0.trustedDevicesVersion = self.idmsTrustedDevicesVersion()
                }
                self.cuttlefish.updateTrust(request) { response in
                    switch response {
                    case .success(let response):
                        do {
                            try self.persist(changes: response.changes)
                            logger.info("distrust succeeded")
                            reply(nil)
                        } catch {
                            logger.error("distrust handling failed: \(String(describing: error), privacy: .public)")
                            reply(error)
                        }
                    case .failure(let error):
                        logger.error("updateTrust failed: \(String(describing: error), privacy: .public)")
                        reply(error)
                        return
                    }
                }
            }
        }
    }

    func drop(peerIDs: Set<String>,
              reply: @escaping (Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (Error?) -> Void = {
            let logType: OSLogType = $0 == nil ? .default : .error
            logger.log(level: logType, "drop complete: \(traceError($0), privacy: .public)")
            sem.release()
            reply($0)
        }

        self.moc.performAndWait {
            defer { self.moc.rollback() } // in case we return early w/o saving

            guard let egoPeerID = self.containerMO.egoPeerID else {
                logger.error("No dynamic info for self?")
                reply(ContainerError.noPreparedIdentity)
                return
            }

            guard !peerIDs.contains(egoPeerID) else {
                logger.error("Self-drop not allowed")
                reply(ContainerError.invalidPeerID)
                return
            }

            for peerID in peerIDs {
                do {
                    try autoreleasepool {
                        if let peerMO = try self.fetchPeerMO(peerID: peerID) {
                            logger.log("Dropping MO for \(peerID, privacy: .private)")
                            self.moc.delete(peerMO)
                        } else {
                            logger.log("MO for peer not found, but that's ok: \(peerID, privacy: .private)")
                        }
                    }
                } catch {
                    logger.error("Failed to fetch peerMO to be dropped: \(peerID, privacy: .private): \(String(describing: error), privacy: .private)")
                    reply(error)
                    return
                }
            }

            // save all the drops together, atomically
            do {
                try self.moc.save()
                logger.log("Saved MOC to drop peer MOs")
            } catch {
                logger.error("Failed to save MOC to drop peers: \(String(describing: error), privacy: .private)")
                reply(error)
                return
            }

            reply(nil)
        }
    }

    func fetchEscrowContents(reply: @escaping (Data?, String?, Data?, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (Data?, String?, Data?, Error?) -> Void = {
            let logType: OSLogType = $3 == nil ? .info : .error
            logger.log(level: logType, "fetchEscrowContents complete: \(traceError($3), privacy: .public)")
            sem.release()
            reply($0, $1, $2, $3)
        }
        logger.info("beginning a fetchEscrowContents")

        self.moc.performAndWait {
            guard let egoPeerID = self.containerMO.egoPeerID else {
                logger.info("fetchEscrowContents failed")
                reply(nil, nil, nil, ContainerError.noPreparedIdentity)
                return
            }

            guard let bottles = self.containerMO.bottles as? Set<BottleMO> else {
                logger.info("fetchEscrowContents failed")
                reply(nil, nil, nil, ContainerError.noBottleForPeer)
                return
            }

            guard let bmo = (bottles.first { $0.peerID == egoPeerID }) else {
                logger.info("fetchEscrowContents no bottle matches peerID")
                reply(nil, nil, nil, ContainerError.noBottleForPeer)
                return
            }

            let bottleID = bmo.bottleID
            var entropy: Data

            do {
                guard let loaded = try loadSecret(label: egoPeerID) else {
                    logger.info("fetchEscrowContents failed to load entropy")
                    reply(nil, nil, nil, ContainerError.failedToFetchEscrowContents)
                    return
                }
                entropy = loaded
            } catch {
                logger.error("fetchEscrowContents failed to load entropy: \(String(describing: error), privacy: .public)")
                reply(nil, nil, nil, error)
                return
            }

            guard let signingPublicKey = bmo.escrowedSigningSPKI else {
                logger.info("fetchEscrowContents no escrow signing spki")
                reply(nil, nil, nil, ContainerError.bottleDoesNotContainerEscrowKeySPKI)
                return
            }
            reply(entropy, bottleID, signingPublicKey, nil)
        }
    }

    func fetchViableBottles(from source: OTEscrowRecordFetchSource, reply: @escaping ([String]?, [String]?, Error?) -> Void) {
        let sem = self.grabSemaphore()
        self.fetchViableBottlesWithSemaphore(from: source) { result in
            sem.release()

            switch result {
            case let .success((viableBottles, partialBottles)):
                logger.info("fetchViableBottles succeeded with \(viableBottles.count, privacy: .public) viable bottles and \(partialBottles.count, privacy: .public) partial bottles")
                reply(viableBottles, partialBottles, nil)
            case let .failure(error):
                logger.error("fetchViableBottles failed with \(traceError(error), privacy: .public)")
                reply(nil, nil, error)
            }
        }
    }

    func handleFetchViableBottlesResponseWithSemaphore(response: FetchViableBottlesResponse?) {
        guard let escrowPairs = response?.viableBottles else {
            logger.info("fetchViableBottles returned no viable bottles")
            return
        }

        var partialPairs: [EscrowPair] = []
        if let partial = response?.partialBottles {
            partialPairs = partial
        } else {
            logger.info("fetchViableBottles returned no partially viable bottles, but that's ok")
        }

        var legacyEscrowInformations: [EscrowInformation] = []
        if let legacy = response?.legacyRecords {
            legacyEscrowInformations = legacy
        } else {
            logger.info("fetchViableBottles returned no legacy escrow records")
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
                        logger.info("fetchViableBottles already knows about record, re-adding entry, label = \(record.label, privacy: .public)")
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
                    logger.info("fetchViableBottles already knows about bottle: \(bottle.bottleID, privacy: .public)")
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

            logger.info("fetchViableBottles saving new bottle: \(bmo, privacy: .public)")
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
                        logger.info("fetchViableBottles already knows about record, re-adding entry: \(record.label, privacy: .public)")
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
                    logger.info("fetchViableBottles already knows about bottle: \(bottle.bottleID, privacy: .public)")
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

            logger.info("fetchViableBottles saving new bottle: \(bmo, privacy: .public)")
            self.containerMO.addToBottles(bmo)
        }
        legacyEscrowInformations.forEach { record in
            // Save this escrow record only if we don't already have it
            if let existingRecords = self.containerMO.legacyEscrowRecords as? Set<EscrowRecordMO> {
                let matchingRecords: Set<EscrowRecordMO> = existingRecords.filter { existing in existing.label == record.label }
                if !matchingRecords.isEmpty {
                    logger.info("fetchViableBottles already knows about legacy record \(record.label, privacy: .public), re-adding entry")
                    self.containerMO.removeFromLegacyEscrowRecords(matchingRecords as NSSet)
                }
                if record.label.hasSuffix(".double") {
                    logger.info("ignoring double enrollment record \(record.label, privacy: .public)")
                } else if record.label.hasPrefix("com.apple.icdp.record.") {
                    self.setEscrowRecord(record: record, viability: Viability.none)
                } else {
                    logger.info("ignoring non-iCDP record: \(record.label, privacy: .public)")
                }
            }
        }
    }

    func fetchViableBottlesWithSemaphore(from source: OTEscrowRecordFetchSource, reply: @escaping ([String]?, [String]?, Error?) -> Void) {
        self.fetchViableBottlesWithSemaphore(from: source) { result in
            switch result {
            case let .success((viableBottles, partialBottles)):
                reply(viableBottles, partialBottles, nil)
            case let .failure(error):
                reply(nil, nil, error)
            }
        }
    }

    func removeEscrowCache(reply: @escaping (Error?) -> Void) {
        logger.info("beginning a removeEscrowCache")

        let sem = self.grabSemaphore()
        let reply: (Error?) -> Void = {
            let logType: OSLogType = $0 == nil ? .info : .error
            logger.log(level: logType, "removeEscrowCache complete \(traceError($0), privacy: .public)")
            sem.release()
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

    func fetchViableBottlesWithSemaphore(from source: OTEscrowRecordFetchSource, reply: @escaping (Result<([String], [String]), Error>) -> Void) {
        logger.info("beginning a fetchViableBottles from source \(source.rawValue)")

        switch source {
        case .cache:
            self.fetchViableBottlesFromCacheWithSemaphore(checkingTimeout: false, reply: reply)
        case .cuttlefish:
            self.fetchViableBottlesFromCuttlefishWithSemaphore(reply: reply)
        case .default:
            fallthrough
        @unknown default:
            self.fetchViableBottlesFromCacheWithSemaphore(checkingTimeout: true) { result in
                switch result {
                case let .success((viableBottles, partialBottles)):
                    if viableBottles.isEmpty, partialBottles.isEmpty {
                        fallthrough
                    } else {
                        logger.info("fetchViableBottlesFromCache returned bottles")
                        reply(.success((viableBottles, partialBottles)))
                    }
                case .failure:
                    logger.info("fetchViableBottlesFromCache did not return any bottles, checking cuttlefish")
                    self.fetchViableBottlesFromCuttlefishWithSemaphore(reply: reply)
                }
            }
        }
    }

    func fetchEscrowRecordsWithSemaphore(from source: OTEscrowRecordFetchSource, reply: @escaping (Result<[Data], Error>) -> Void) {
        logger.info("starting fetchEscrowRecordsWithSemaphore from source \(source.rawValue)")

        switch source {
        case .cache:
            self.fetchEscrowRecordsFromCacheWithSemaphore(checkingTimeout: false, reply: reply)
        case .cuttlefish:
            self.fetchEscrowRecordsFromCuttlefishWithSemaphore(reply: reply)
        case .default:
            fallthrough
        @unknown default:
            self.fetchEscrowRecordsFromCacheWithSemaphore(checkingTimeout: true) { result in
                switch result {
                case let .success(records):
                    if records.isEmpty {
                        fallthrough
                    } else {
                        logger.info("fetchEscrowRecordsFromCache returned records")
                        reply(.success(records))
                    }
                case .failure:
                    logger.info("fetchEscrowRecordsFromCache did not return any records, checking cuttlefish")
                    self.fetchEscrowRecordsFromCuttlefishWithSemaphore(reply: reply)
                }
            }
        }
    }

    func fetchViableBottlesFromCacheWithSemaphore(checkingTimeout: Bool, reply: @escaping (Result<([String], [String]), Error>) -> Void) {
        logger.info("starting fetchViableBottlesFromCacheWithSemaphore and will check timeout: \(checkingTimeout)")
        self.moc.performAndWait {
            self.fetchFromEscrowCacheWithSemaphore(checkingTimeout: checkingTimeout, cacheFetch: {
                let bottles = self.onqueueCachedBottlesFromEscrowRecords()
                return (
                    collection: (bottles.viableBottles, bottles.partialBottles),
                    isEmpty: bottles.viableBottles.isEmpty && bottles.partialBottles.isEmpty
                )
            }, reply: reply)
        }
    }

    func fetchEscrowRecordsFromCacheWithSemaphore(checkingTimeout: Bool, reply: @escaping (Result<[Data], Error>) -> Void) {
        logger.info("starting fetchEscrowRecordsFromCacheWithSemaphore and will check timeout: \(checkingTimeout)")
        self.moc.performAndWait {
            self.fetchFromEscrowCacheWithSemaphore(checkingTimeout: checkingTimeout, cacheFetch: {
                let records = self.onqueueCachedEscrowRecords().compactMap { $0.data }
                return (
                    collection: records,
                    isEmpty: records.isEmpty
                )
            }, reply: reply)
        }
    }

    func fetchFromEscrowCacheWithSemaphore<T>(
        checkingTimeout: Bool,
        cacheFetch: () -> (collection: T, isEmpty: Bool),
        reply: @escaping (Result<T, Error>) -> Void
    ) {
        logger.info("starting fetchFromEscrowCacheWithSemaphore and will check timeout: \(checkingTimeout)")
        reply(self.moc.performAndWait {
            let (records, isEmpty) = cacheFetch()
            guard !isEmpty || self.containerMO.escrowFetchDate != nil else {
                logger.info("no cached records were found, no saved escrowFetchDate either, returning no cache error")
                return .failure(ContainerError.noEscrowCache)
            }

            guard checkingTimeout else {
                logger.info("skipping timeout check and directly returning cached records")
                return .success(records)
            }

            if let lastDate = self.containerMO.escrowFetchDate, Date() < lastDate.addingTimeInterval(self.escrowCacheTimeout) {
                logger.info("escrow cache still valid")
                return .success(records)
            } else {
                logger.info("escrow cache no longer valid")
                return .failure(ContainerError.failedToFetchEscrowContents)
            }
        })
    }

    func fetchViableBottlesFromCuttlefishWithSemaphore(reply: @escaping (Result<([String], [String]), Error>) -> Void) {
        logger.info("starting fetchViableBottlesWithSemaphoreFromCuttlefish")

        let request = FetchViableBottlesRequest.with { request in
            if OctagonPlatformSupportsSOS() && !SOSCompatibilityModeEnabled() {
                request.filterRequest = .unknown
            } else {
                logger.info("Requesting Cuttlefish to filter records by Octagon Only")
                request.filterRequest = .byOctagonOnly
            }
        }

        self.cuttlefish.fetchViableBottles(request) { result in
            switch result {
            case let .success(response):
                reply(self.moc.performAndWait {
                    do {
                        // Purge escrow cache before updating with a fresh response.
                        self.onQueueRemoveEscrowCache()
                        self.handleFetchViableBottlesResponseWithSemaphore(response: response)
                        self.containerMO.escrowFetchDate = Date()

                        try self.moc.save()
                        logger.info("fetchViableBottles saved bottles and records")

                        let viableBottleIDs = response.viableBottles.map { $0.bottle.bottleID }
                        let partialBottleIDs = response.partialBottles.map { $0.bottle.bottleID }

                        logger.info("fetchViableBottles returned viable bottles: \(viableBottleIDs, privacy: .public)")
                        logger.info("fetchViableBottles returned partial bottles: \(partialBottleIDs, privacy: .public)")

                        return .success((viableBottleIDs, partialBottleIDs))
                    } catch {
                        logger.error("fetchViableBottles unable to save bottles and records with \(traceError(error), privacy: .public)")
                        return .failure(error)
                    }
                })
            case let .failure(error):
                logger.error("fetchViableBottles failed with \(traceError(error), privacy: .public)")
                reply(.failure(error))
            }
        }
    }

    func fetchEscrowRecordsFromCuttlefishWithSemaphore(reply: @escaping (Result<[Data], Error>) -> Void) {
        self.fetchViableBottlesFromCuttlefishWithSemaphore { result in
            reply(self.moc.performAndWait { result.map { _ in
                self.onqueueCachedEscrowRecords().compactMap { $0.data }
            }})
        }
    }

    func fetchCurrentPolicy(modelIDOverride: String?, isInheritedAccount: Bool, reply: @escaping (TPSyncingPolicy?, TPPBPeerStableInfoUserControllableViewStatus, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (TPSyncingPolicy?, TPPBPeerStableInfoUserControllableViewStatus, Error?) -> Void = {
            let logType: OSLogType = $2 == nil ? .info : .error
            logger.log(level: logType, "fetchCurrentPolicy complete: \(traceError($2), privacy: .public)")
            sem.release()
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

                guard let modelIDOverride = modelIDOverride else {
                    logger.info("no model ID override; returning error")
                    reply(nil, .UNKNOWN, ContainerError.noPreparedIdentity)
                    return
                }

                let modelID = TPPeerPermanentInfo.mungeModelID(modelIDOverride)

                guard let policyDocument = self.model.policy(withVersion: prevailingPolicyVersion.versionNumber) else {
                    logger.info("prevailing policy is missing?")
                    reply(nil, .UNKNOWN, ContainerError.noPreparedIdentity)
                    return
                }

                do {
                    let prevailingPolicy = try policyDocument.policy(withSecrets: [:], decrypter: Decrypter())
                    let syncingPolicy = try prevailingPolicy.syncingPolicy(forModel: modelID, syncUserControllableViews: .UNKNOWN, isInheritedAccount: isInheritedAccount)

                    logger.info("returning a policy for model ID \(modelID, privacy: .public)")
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
            let allPolicyVersions: Set<TPPolicyVersion>
            do {
                allPolicyVersions = try self.model.allPolicyVersions()
            } catch {
                logger.error("Error fetching all policy versions: \(error, privacy: .public)")
                reply(nil, .UNKNOWN, error)
                return
            }
            self.fetchPolicyDocumentsWithSemaphore(versions: allPolicyVersions) { _, policyFetchError in
                if let error = policyFetchError {
                    logger.info("join: error fetching all requested policies (continuing anyway): \(String(describing: error), privacy: .public)")
                }

                do {
                    let syncingPolicy = try self.syncingPolicyFor(modelID: modelIDOverride ?? permanentInfo.modelID, stableInfo: stableInfo)

                    guard let peer = try self.model.peer(withID: permanentInfo.peerID), let dynamicInfo = peer.dynamicInfo else {
                        logger.error("fetchCurrentPolicy with no dynamic info")
                        reply(syncingPolicy, .UNKNOWN, nil)
                        return
                    }

                    // Note: we specifically do not want to sanitize this value for the platform: returning FOLLOWING here isn't that helpful
                    let peersUserViewSyncability = self.model.userViewSyncabilityConsensusAmongTrustedPeers(dynamicInfo)
                    reply(syncingPolicy, peersUserViewSyncability, nil)
                    return
                } catch {
                    logger.error("Fetching the syncing policy failed: \(String(describing: error), privacy: .public)")
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
            logger.info("Ignoring policy version from pre-CKKS4All peer")
        } else {
            bestPolicyVersion = peerPolicyVersion
        }

        guard let policyDocument = self.model.policy(withVersion: bestPolicyVersion.versionNumber) else {
            logger.info("best policy(\(bestPolicyVersion, privacy: .public)) is missing?")
            throw ContainerError.unknownPolicyVersion(bestPolicyVersion.versionNumber)
        }

        let policy = try policyDocument.policy(withSecrets: stableInfo.policySecrets ?? [:], decrypter: Decrypter())
        return try policy.syncingPolicy(forModel: modelID, syncUserControllableViews: stableInfo.syncUserControllableViews, isInheritedAccount: stableInfo.isInheritedAccount)
    }

    // All-or-nothing: return an error in case full list cannot be returned.
    // Completion handler data format: [version : [hash, data]]
    func fetchPolicyDocuments(versions: Set<TPPolicyVersion>,
                              reply: @escaping ([TPPolicyVersion: Data]?, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: ([TPPolicyVersion: Data]?, Error?) -> Void = {
            let logType: OSLogType = $1 == nil ? .info : .error
            logger.log(level: logType, "fetchPolicyDocuments complete: \(traceError($1), privacy: .public)")
            sem.release()
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
                logger.info("fetchPolicyDocument: didn't return policy of version: \(String(describing: versions), privacy: .public)")
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
                PolicyDocumentKey.with { $0.version = version.versionNumber; $0.hash = version.policyHash } }
        }

        self.cuttlefish.fetchPolicyDocuments(request) { response in
            switch response {
            case .success(let response):
                self.moc.performAndWait {
                    for mapEntry in response.entries {
                        // TODO: validate the policy's signature

                        guard let doc = TPPolicyDocument.policyDoc(withHash: mapEntry.key.hash, data: mapEntry.value) else {
                            logger.info("Can't make policy document with hash \(mapEntry.key.hash, privacy: .public) and data \(mapEntry.value.base64EncodedString(), privacy: .public)")
                            reply(nil, ContainerError.policyDocumentDoesNotValidate)
                            return
                        }

                        guard let expectedVersion = (remaining.first { $0.versionNumber == doc.version.versionNumber }) else {
                            logger.info("Received a policy version we didn't request: \(doc.version.versionNumber)")
                            reply(nil, ContainerError.policyDocumentDoesNotValidate)
                            return
                        }

                        guard expectedVersion.policyHash == doc.version.policyHash else {
                            logger.info("Requested hash \(expectedVersion.policyHash, privacy: .public) does not match fetched hash \(doc.version.policyHash, privacy: .public)")
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
                logger.error("FetchPolicyDocuments failed: \(String(describing: error), privacy: .public)")
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
                logger.error("fetchRecoverableTlkshares failed: \(String(describing: error), privacy: .public)")
                reply(nil, error)
                return

            case .success(let response):
                let shareCount = response.views.map { $0.tlkShares.count }.reduce(0, +)
                logger.info("fetchRecoverableTlkshares succeeded: found \(response.views.count) views and \(shareCount) total TLKShares")
                let records = response.views.flatMap { $0.ckrecords() }
                reply(records, nil)
                return
            }
        }
    }

    // Must be on moc queue to call this.
    // Caller is responsible for saving the moc afterwards.
    private func registerPeerMO(peer: TPPeer,
                                vouchers: [TPVoucher]? = nil,
                                isEgoPeer: Bool = false) throws {
        let peerID = peer.permanentInfo.peerID

        let peerMO = PeerMO(context: self.moc)
        peerMO.peerID = peerID
        peerMO.permanentInfo = peer.permanentInfo.data
        peerMO.permanentInfoSig = peer.permanentInfo.sig
        peerMO.stableInfo = peer.stableInfo?.data
        peerMO.stableInfoSig = peer.stableInfo?.sig
        peerMO.dynamicInfo = peer.dynamicInfo?.data
        peerMO.dynamicInfoSig = peer.dynamicInfo?.sig
        peerMO.isEgoPeer = isEgoPeer
        if let hmacKey = self.getHmacKey() {
            peerMO.hmacSig = peer.calculateHmac(withHmacKey: hmacKey)
        } else {
            peerMO.hmacSig = nil
        }
        self.containerMO.addToPeers(peerMO)

        vouchers?.forEach { voucher in
            let peerVouchers = peerMO.vouchers as? Set<VoucherMO> ?? Set()
            if !(peerVouchers.contains { $0.voucherInfo == voucher.data && $0.voucherInfoSig == voucher.sig }) {
                let voucherMO = VoucherMO(context: self.moc)
                voucherMO.voucherInfo = voucher.data
                voucherMO.voucherInfoSig = voucher.sig
                peerMO.addToVouchers(voucherMO)
            }
        }
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
                logger.info("Planning to share \(String(describing: keyset.tlk), privacy: .public) with peers \(peerIDsWithAccess, privacy: .public)")

                let peers = try peerIDsWithAccess.compactMap { try self.model.peer(withID: $0) }
                let viewPeerShares = try peers.map { receivingPeer in
                    TLKShare.convert(ckksTLKShare: try CKKSTLKShare(keyset.tlk,
                                                                    as: egoPeerKeys,
                                                                    to: receivingPeer.permanentInfo,
                                                                    epoch: epoch,
                                                                    poisoned: 0))
                }

                peerShares += viewPeerShares
            } catch {
                logger.error("Unable to create TLKShares for keyset \(keyset, privacy: .public): \(String(describing: error), privacy: .public)")
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
                let consensusUserViewSyncability = self.model.userViewSyncabilityConsensusAmongTrustedPeers(dynamicInfo)

                if consensusUserViewSyncability == .ENABLED && !self.managedConfigurationAdapter.isCloudKeychainSyncAllowed() {
                    logger.info("user-controllable views disabled by profile")
                    newUserViewSyncability = .DISABLED
                } else {
                    newUserViewSyncability = consensusUserViewSyncability
                }
            }
            logger.info("join: setting 'user view sync' control as: \(TPPBPeerStableInfoUserControllableViewStatusAsString(newUserViewSyncability), privacy: .public)")
            userViewSyncability = newUserViewSyncability
        }

        let newStableInfo = try self.createNewStableInfoIfNeeded(stableChanges: StableChanges.change(viewStatus: userViewSyncability),
                                                                 permanentInfo: peerPermanentInfo,
                                                                 existingStableInfo: stableInfo,
                                                                 dynamicInfo: dynamicInfo,
                                                                 signingKeyPair: egoPeerKeys.signingKey,
                                                                 vouchers: vouchers)

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
              altDSID: String?,
              flowID: String?,
              deviceSessionID: String?,
              canSendMetrics: Bool,
              reply: @escaping (String?, [CKRecord], TPSyncingPolicy?, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (String?, [CKRecord], TPSyncingPolicy?, Error?) -> Void = {
            let logType: OSLogType = $3 == nil ? .info : .error
            logger.log(level: logType, "join complete: \(traceError($3), privacy: .public)")
            sem.release()
            reply($0, $1, $2, $3)
        }

        let eventFetchAndPersistChanges = AAFAnalyticsEventSecurity(keychainCircleMetrics: nil,
                                                                    altDSID: altDSID,
                                                                    flowID: flowID,
                                                                    deviceSessionID: deviceSessionID,
                                                                    eventName: kSecurityRTCEventNameFetchAndPersistChanges,
                                                                    testsAreEnabled: soft_MetricsOverrideTestsAreEnabled(),
                                                                    canSendMetrics: canSendMetrics,
                                                                    category: kSecurityRTCEventCategoryAccountDataAccessRecovery)
        self.fetchAndPersistChanges { error in
            guard error == nil else {
                reply(nil, [], nil, error)
                SecurityAnalyticsReporterRTC.sendMetric(withEvent: eventFetchAndPersistChanges, success: false, error: error)
                return
            }

            SecurityAnalyticsReporterRTC.sendMetric(withEvent: eventFetchAndPersistChanges, success: true, error: nil)

            let eventFetchPolicyDocument = AAFAnalyticsEventSecurity(keychainCircleMetrics: nil,
                                                                     altDSID: altDSID,
                                                                     flowID: flowID,
                                                                     deviceSessionID: deviceSessionID,
                                                                     eventName: kSecurityRTCEventNameFetchPolicyDocument,
                                                                     testsAreEnabled: soft_MetricsOverrideTestsAreEnabled(),
                                                                     canSendMetrics: canSendMetrics,
                                                                     category: kSecurityRTCEventCategoryAccountDataAccessRecovery)
            // To join, you must know all policies that exist
            let allPolicyVersions: Set<TPPolicyVersion>? = self.moc.performAndWait {
                do {
                    return try self.model.allPolicyVersions()
                } catch {
                    logger.error("Error fetching all policy versions: \(error, privacy: .public)")
                    reply(nil, [], nil, error)
                    return nil
                }
            }
            guard let allPolicyVersions else {
                return
            }
            self.fetchPolicyDocumentsWithSemaphore(versions: allPolicyVersions) { _, policyFetchError in
                if let error = policyFetchError {
                    logger.info("join: error fetching all requested policies (continuing anyway): \(String(describing: error), privacy: .public)")
                    SecurityAnalyticsReporterRTC.sendMetric(withEvent: eventFetchPolicyDocument, success: false, error: policyFetchError)
                } else {
                    SecurityAnalyticsReporterRTC.sendMetric(withEvent: eventFetchPolicyDocument, success: true, error: nil)
                }

                self.moc.performAndWait {
                    guard let voucher = TPVoucher(infoWith: voucherData, sig: voucherSig) else {
                        reply(nil, [], nil, ContainerError.invalidVoucherOrSig)
                        return
                    }
                    let sponsor: TPPeer?
                    do {
                        sponsor = try self.model.peer(withID: voucher.sponsorID)
                    } catch {
                        logger.error("Error getting sponsor (\(voucher.sponsorID)): \(String(describing: error), privacy: .public)")
                        reply(nil, [], nil, error)
                        return
                    }
                    guard let sponsor else {
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
                        logger.info("join: self machineID \(selfPermanentInfo.machineID, privacy: .public) not on list")
                        self.onqueueTTRUntrusted()
                        reply(nil, [], nil, ContainerError.preparedIdentityNotOnAllowedList(selfPermanentInfo.machineID))
                        return
                    }

                    loadEgoKeys(peerID: egoPeerID) { egoPeerKeys, error in
                        guard let egoPeerKeys = egoPeerKeys else {
                            logger.error("Don't have my own peer keys; can't join: \(String(describing: error), privacy: .public)")
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
                                logger.error("Unable to create peer for joining: \(String(describing: error), privacy: .public)")
                                reply(nil, [], nil, error)
                                return
                            }

                            guard let peerStableInfo = peer.stableInfoAndSig.toStableInfo() else {
                                logger.info("Unable to create new peer stable info for joining")
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
                                logger.error("Unable to process keys before joining: \(String(describing: error), privacy: .public)")
                                reply(nil, [], nil, error)
                                return
                            }

                            do {
                                try self.model.checkIntroduction(forCandidate: selfPermanentInfo,
                                                                 stableInfo: peer.stableInfoAndSig.toStableInfo(),
                                                                 withSponsorID: sponsor.peerID)
                            } catch {
                                logger.error("Error checking introduction: \(String(describing: error), privacy: .public)")
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

                            logger.info("Beginning join for peer \(egoPeerID, privacy: .public)")
                            logger.info("Join permanentInfo: \(egoPermData.base64EncodedString(), privacy: .public)")
                            logger.info("Join permanentInfoSig: \(egoPermSig.base64EncodedString(), privacy: .public)")
                            logger.info("Join stableInfo: \(peer.stableInfoAndSig.peerStableInfo.base64EncodedString(), privacy: .public)")
                            logger.info("Join stableInfoSig: \(peer.stableInfoAndSig.sig.base64EncodedString(), privacy: .public)")
                            logger.info("Join dynamicInfo: \(peer.dynamicInfoAndSig.peerDynamicInfo.base64EncodedString(), privacy: .public)")
                            logger.info("Join dynamicInfoSig: \(peer.dynamicInfoAndSig.sig.base64EncodedString(), privacy: .public)")

                            logger.info("Join vouchers: \(peer.vouchers.map { $0.voucher.base64EncodedString() }, privacy: .public)")
                            logger.info("Join voucher signatures: \(peer.vouchers.map { $0.sig.base64EncodedString() }, privacy: .public)")

                            logger.info("Uploading \(allTLKShares.count) tlk shares")

                            do {
                                let peerBase64 = try peer.serializedData().base64EncodedString()
                                logger.info("Join peer: \(peerBase64, privacy: .public)")
                            } catch {
                                logger.info("Join unable to encode peer: \(String(describing: error), privacy: .public)")
                            }

                            let metrics = Metrics.with {
                                $0.flowID = flowID ?? String()
                                $0.deviceSessionID = deviceSessionID ?? String()
                            }

                            let changeToken = self.changeToken()
                            let request = JoinWithVoucherRequest.with {
                                $0.changeToken = changeToken
                                $0.peer = peer
                                $0.bottle = bottle
                                $0.tlkShares = allTLKShares
                                $0.viewKeys = viewKeys
                                $0.trustedDevicesVersion = self.idmsTrustedDevicesVersion()
                                $0.metrics = metrics
                            }

                            self.cuttlefish.joinWithVoucher(request) { response in
                                switch response {
                                case .success(let response):
                                    self.moc.performAndWait {
                                        do {
                                            let event = AAFAnalyticsEventSecurity(keychainCircleMetrics: [kSecurityRTCFieldNumberOfTrustedPeers: newDynamicInfo.includedPeerIDs.count as NSNumber],
                                                                                  altDSID: altDSID,
                                                                                  flowID: flowID,
                                                                                  deviceSessionID: nil,
                                                                                  eventName: kSecurityRTCEventNameNumberOfTrustedOctagonPeers,
                                                                                  testsAreEnabled: soft_MetricsOverrideTestsAreEnabled(),
                                                                                  canSendMetrics: canSendMetrics,
                                                                                  category: kSecurityRTCEventCategoryAccountDataAccessRecovery)
                                            SecurityAnalyticsReporterRTC.sendMetric(withEvent: event, success: true, error: nil)

                                            self.containerMO.egoPeerStableInfo = peer.stableInfoAndSig.peerStableInfo
                                            self.containerMO.egoPeerStableInfoSig = peer.stableInfoAndSig.sig

                                            let syncingPolicy = try self.syncingPolicyFor(modelID: selfPermanentInfo.modelID,
                                                                                          stableInfo: peerStableInfo)

                                            try self.onQueuePersist(changes: response.changes)
                                            logger.info("JoinWithVoucher succeeded")

                                            let keyHierarchyRecords = response.zoneKeyHierarchyRecords.compactMap { CKRecord($0) }
                                            reply(egoPeerID, keyHierarchyRecords, syncingPolicy, nil)
                                        } catch {
                                            logger.error("JoinWithVoucher failed: \(String(describing: error), privacy: .public)")
                                            reply(nil, [], nil, error)
                                        }
                                    }
                                case .failure(let error):
                                    logger.error("joinWithVoucher failed: \(String(describing: error), privacy: .public)")
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

    func requestHealthCheck(requiresEscrowCheck: Bool, repair: Bool, knownFederations: [String], reply: @escaping (TrustedPeersHelperHealthCheckResult?, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (TrustedPeersHelperHealthCheckResult?, Error?) -> Void = {
            let logType: OSLogType = $1 == nil ? .info : .error
            logger.log(level: logType, "health check complete: \(traceError($1), privacy: .public)")
            sem.release()
            reply($0, $1)
        }

        logger.info("requestHealthCheck requiring escrow check: \(requiresEscrowCheck), \(repair), knownFederations: \(knownFederations, privacy: .public)")

        self.moc.performAndWait {
            guard let egoPeerID = self.containerMO.egoPeerID else {
                // No identity, nothing to do
                logger.info("requestHealthCheck: No identity.")
                reply(nil, ContainerError.noPreparedIdentity)
                return
            }
            let request = GetRepairActionRequest.with {
                $0.peerID = egoPeerID
                $0.requiresEscrowCheck = requiresEscrowCheck
                $0.knownFederations = knownFederations
                $0.performCleanup = repair
            }

            self.cuttlefish.getRepairAction(request) { response in
                switch response {
                case .success(let response):
                    let action = response.repairAction

                    var postRepairAccount: Bool = false
                    var postRepairEscrow: Bool = false
                    var resetOctagon: Bool = false
                    var leaveTrust: Bool = false
                    var reroll: Bool = false

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
                    case .reroll:
                        reroll = true
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

                    let response = TrustedPeersHelperHealthCheckResult(postRepairCFU: postRepairAccount,
                                                                       postEscrowCFU: postRepairEscrow,
                                                                        resetOctagon: resetOctagon,
                                                                          leaveTrust: leaveTrust,
                                                                              reroll: reroll,
                                                                         moveRequest: moveRequest,
                                                                  totalEscrowRecords: response.totalEscrowRecords,
                                                            collectableEscrowRecords: response.collectableEscrowRecords,
                                                              collectedEscrowRecords: response.collectedEscrowRecords,
                                                escrowRecordGarbageCollectionEnabled: response.escrowRecordGarbageCollectionEnabled,
                                                                      totalTlkShares: response.totalTlkShares,
                                                                collectableTlkShares: response.collectableTlkShares,
                                                                  collectedTlkShares: response.collectedTlkShares,
                                                    tlkShareGarbageCollectionEnabled: response.tlkShareGarbageCollectionEnabled,
                                                                          totalPeers: response.totalPeers,
                                                                        trustedPeers: response.trustedPeers,
                                                                    superfluousPeers: response.superfluousPeers,
                                                                      peersCleanedup: response.peersCleanedup,
                                                      superfluousPeersCleanupEnabled: response.superfluousPeersCleanupEnabled
                                                                         )
                    reply(response, nil)
                    return
                case .failure(let error):
                    reply(nil, error)
                    return
                }
            }
        }
    }

    func getSupportAppInfo(reply: @escaping (Data?, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (Data?, Error?) -> Void = {
            let logType: OSLogType = $1 == nil ? .info : .error
            logger.log(level: logType, "getSupportAppInfo complete: \(traceError($1), privacy: .public)")
            sem.release()
            reply($0, $1)
        }

        self.cuttlefish.getSupportAppInfo { response in
            switch response {
            case .success(let response):
                let data: Data
                do {
                    data = try response.serializedData()
                } catch {
                    reply(nil, ContainerError.failedToSerializeData(suberror: error))
                    return
                }

                reply(data, nil)
            case .failure(let error):
                logger.error("getSupportAppInfo failed: \(String(describing: error), privacy: .public)")
                reply(nil, error)
                return
            }
        }
    }

    func fetchTrustedPeersCount(reply: @escaping (NSNumber?, Error?) -> Void) {
        let reply: (NSNumber?, Error?) -> Void = {
            let logType: OSLogType = $1 == nil ? .info : .error
            logger.log(level: logType, "fetch trusted peer count complete: \(String(reflecting: $0), privacy: .public) \(traceError($1), privacy: .public)")
            reply($0, $1)
        }
        logger.info("beginning a fetchTrustedPeersCount")

        self.fetchAndPersistChangesIfNeeded { fetchError in
            guard fetchError == nil else {
                reply(nil, fetchError)
                return
            }

            self.moc.performAndWait {
                do {
                    reply(NSNumber(value: try self.model.trustedPeerCount()), nil)
                } catch {
                    reply(nil, error)
                }
                return
            }
        }
    }

    func octagonContainsDistrustedRecoveryKeys(reply: @escaping (Bool, Error?) -> Void) {
        let reply: (Bool, Error?) -> Void = {
            let logType: OSLogType = $1 == nil ? .info : .error
            logger.log(level: logType, "octagon contains distrusted recovery keys complete: \(String(reflecting: $0), privacy: .public) \(traceError($1), privacy: .public)")
            reply($0, $1)
        }
        logger.info("beginning a octagonContainsDistrustedRecoveryKeys")

        self.fetchAndPersistChangesIfNeeded { fetchError in
            guard fetchError == nil else {
                reply(false, fetchError)
                return
            }

            self.moc.performAndWait {
                let containsDistrusted: Bool
                do {
                    containsDistrusted = try self.model.doesOctagonContainDistrustedRecoveryKeys()
                } catch {
                    logger.error("error determining whether octagon contains distrusted RKs: \(error, privacy: .public)")
                    reply(false, error)
                    return
                }
                logger.info("distrusted recovery keys exist: \(containsDistrusted)")
                reply(containsDistrusted, nil)
                return
            }
        }
    }

    func resetCDPAccountData(idmsTargetContext: String?, idmsCuttlefishPassword: String?, notifyIdMS: Bool, internalAccount: Bool, demoAccount: Bool, reply: @escaping (Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (Error?) -> Void = {
            let logType: OSLogType = $0 == nil ? .info : .error
            logger.log(level: logType, "resetCDPAccountData complete: \(traceError($0), privacy: .public)")
            sem.release()
            reply($0)
        }

        let request = ResetAccountCDPContentsRequest.with {
            $0.resetReason = .testGenerated
            $0.idmsTargetContext = idmsTargetContext ?? ""
            $0.idmsCuttlefishPassword = idmsCuttlefishPassword ?? ""
	    $0.testingNotifyIdms = notifyIdMS
            $0.accountInfo = AccountInfo.with {
                $0.flags = (internalAccount ? UInt32(AccountFlags.internal.rawValue) : 0) | (demoAccount ? UInt32(AccountFlags.demo.rawValue) : 0)
            }
        }
        self.cuttlefish.resetAccountCdpcontents(request) { response in
            switch response {
            case .success:
                self.moc.performAndWait {
                    self.onQueueRemoveEscrowCache()
                }
                reply(nil)
            case .failure(let error):
                logger.error("resetCDPAccountData failed: \(String(describing: error), privacy: .public)")
                reply(error)
                return
            }
        }
    }

    func fetchAccountSettings(forceFetch: Bool, reply: @escaping([String: TPPBPeerStableInfoSetting]?, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: ([String: TPPBPeerStableInfoSetting]?, Error?) -> Void = {
            let logType: OSLogType = $1 == nil ? .info : .error
            logger.log(level: logType, "fetchAccountSettings complete: \(traceError($1), privacy: .public)")
            sem.release()
            reply($0, $1)
        }
        let block: (Error?) -> Void = { error in
            guard error == nil else {
                logger.error("fetchAccountSettings unable to fetch changes: \(String(describing: error), privacy: .public)")
                reply(nil, error)
                return
            }
            self.moc.performAndWait {
                let bestWalrus: TPPBPeerStableInfoSetting?
                do {
                    bestWalrus = try self.model.bestWalrusAcrossTrustedPeers()
                } catch {
                    logger.error("fetchAccountSettings unable to find best ADP: \(String(describing: error), privacy: .public)")
                    reply(nil, error)
                    return
                }
                let bestWebAccess: TPPBPeerStableInfoSetting?
                do {
                    bestWebAccess = try self.model.bestWebAccessAcrossTrustedPeers()
                } catch {
                    logger.error("fetchAccountSettings unable to find best web access: \(String(describing: error), privacy: .public)")
                    reply(nil, error)
                    return
                }

                var settings: [String: TPPBPeerStableInfoSetting] = [:]
                if let walrus = bestWalrus {
                    settings["walrus"] = walrus
                }
                if let webAccess = bestWebAccess {
                    settings["webAccess"] = webAccess
                }
                reply(settings, nil)
            }
        }
        if forceFetch {
            self.fetchAndPersistChanges(reply: block)
        } else {
            self.fetchAndPersistChangesIfNeeded(reply: block)
        }
    }

    func preflightPreapprovedJoin(preapprovedKeys: [Data]?,
                                  reply: @escaping (Bool, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (Bool, Error?) -> Void = {
            let logType: OSLogType = $1 == nil ? .info : .error
            logger.log(level: logType, "preflightPreapprovedJoin complete: \(traceError($1), privacy: .public)")
            sem.release()
            reply($0, $1)
        }

        self.fetchAndPersistChanges { error in
            guard error == nil else {
                logger.error("preflightPreapprovedJoin unable to fetch changes: \(String(describing: error), privacy: .public)")
                reply(false, error)
                return
            }

            // We need to try to have all policy versions that our peers claim to behave
            let allPolicyVersions: Set<TPPolicyVersion>? = self.moc.performAndWait {
                do {
                    return try self.model.allPolicyVersions()
                } catch {
                    logger.error("Error fetching all policy versions: \(error, privacy: .public)")
                    reply(false, error)
                    return nil
                }
            }
            guard let allPolicyVersions else {
                return
            }

            self.fetchPolicyDocumentsWithSemaphore(versions: allPolicyVersions) { _, policyFetchError in
                if let error = policyFetchError {
                    logger.info("preflightPreapprovedJoin: error fetching all requested policies (continuing anyway): \(String(describing: error), privacy: .public)")
                }

                // We explicitly ignore the machine ID list here; we're only interested in the peer states: do they preapprove us?

                self.moc.performAndWait {
                    do {
                        guard try self.model.hasAnyPeers() else {
                            // If, after fetch and handle changes, there's no peers, then we can likely establish.
                            reply(true, nil)
                            return
                        }
                    } catch {
                        logger.error("error calling hasAnyPeers: \(error, privacy: .public)")
                        reply(false, error)
                        return
                    }

                    guard let egoPeerID = self.containerMO.egoPeerID,
                          let egoPermData = self.containerMO.egoPeerPermanentInfo,
                          let egoPermSig = self.containerMO.egoPeerPermanentInfoSig
                    else {
                        logger.info("preflightPreapprovedJoin: no prepared identity")
                        reply(false, ContainerError.noPreparedIdentity)
                        return
                    }

                    let keyFactory = TPECPublicKeyFactory()
                    guard let egoPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: egoPermData, sig: egoPermSig, keyFactory: keyFactory) else {
                        logger.info("preflightPreapprovedJoin: invalid permanent info")
                        reply(false, ContainerError.invalidPermanentInfoOrSig)
                        return
                    }

                    do {
                        guard try self.model.hasPotentiallyTrustedPeerPreapprovingKey(egoPermanentInfo.signingPubKey.spki()) else {
                            logger.info("preflightPreapprovedJoin: no peers preapprove our key")
                            reply(false, ContainerError.noPeersPreapprovePreparedIdentity)
                            return
                        }
                    } catch {
                        logger.info("preflightPreapprovedJoin: error calling hasPotentiallyTrustedPeerPreapprovingKey \(error, privacy: .public)")
                        reply(false, error)
                        return
                    }

                    let keysApprovingPeers: [Data]?
                    do {
                        keysApprovingPeers = try preapprovedKeys?.filter { key in
                            try self.model.hasPotentiallyTrustedPeer(withSigningKey: key)
                        }
                    } catch {
                        logger.error("preflightPreapprovedJoin: error calling hasPotentiallyTrustedPeerWithSigningKey \(error, privacy: .public)")
                        reply(false, error)
                        return
                    }

                    guard (keysApprovingPeers?.count ?? 0) > 0 else {
                        logger.info("preflightPreapprovedJoin: no reciprocal trust for existing peers")
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
        let sem = self.grabSemaphore()
        let reply: (String?, [CKRecord], TPSyncingPolicy?, Error?) -> Void = {
            let logType: OSLogType = $3 == nil ? .info : .error
            logger.log(level: logType, "preapprovedJoin complete: \(traceError($3), privacy: .public)")
            sem.release()
            reply($0, $1, $2, $3)
        }

        self.fetchAndPersistChangesIfNeeded { error in
            guard error == nil else {
                logger.error("preapprovedJoin unable to fetch changes: \(String(describing: error), privacy: .public)")
                reply(nil, [], nil, error)
                return
            }
            self.moc.performAndWait {
                // If, after fetch and handle changes, there's no peers, then fire off an establish
                // Note that if the establish fails, retrying this call might work.
                // That's up to the caller.
                do {
                    if try self.model.peerCount() == 0 {
                        logger.info("preapprovedJoin but no existing peers, attempting establish")

                        self.onqueueEstablish(ckksKeys: ckksKeys,
                                              tlkShares: tlkShares,
                                              preapprovedKeys: preapprovedKeys,
                                              reply: reply)
                        return
                    }
                } catch {
                    logger.error("preapprovedJoin: error getting peerCount: \(error, privacy: .public)")
                    reply(nil, [], nil, error)
                    return
                }

                // Fetch ego peer identity from local storage.
                guard let egoPeerID = self.containerMO.egoPeerID,
                    let egoPermData = self.containerMO.egoPeerPermanentInfo,
                    let egoPermSig = self.containerMO.egoPeerPermanentInfoSig,
                    let egoStableData = self.containerMO.egoPeerStableInfo,
                    let egoStableSig = self.containerMO.egoPeerStableInfoSig
                    else {
                        logger.info("preapprovedJoin: no prepared identity")
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
                    logger.info("preapprovedJoin: self machineID \(selfPermanentInfo.machineID, privacy: .public) (me) not on list")
                    self.onqueueTTRUntrusted()
                    reply(nil, [], nil, ContainerError.preparedIdentityNotOnAllowedList(selfPermanentInfo.machineID))
                    return
                }
                loadEgoKeys(peerID: egoPeerID) { egoPeerKeys, error in
                    guard let egoPeerKeys = egoPeerKeys else {
                        logger.error("preapprovedJoin: Don't have my own keys: can't join: \(String(describing: error), privacy: .public)")
                        reply(nil, [], nil, error)
                        return
                    }

                    do {
                        guard try self.model.hasPotentiallyTrustedPeerPreapprovingKey(egoPeerKeys.signingKey.publicKey().spki()) else {
                            logger.info("preapprovedJoin: no peers preapprove our key")
                            reply(nil, [], nil, ContainerError.noPeersPreapprovePreparedIdentity)
                            return
                        }
                    } catch {
                        logger.info("preapprovedJoin: error calling hasPotentiallyTrustedPeerPreapprovingKey \(error, privacy: .public)")
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
                                                                                           sponsorID: nil,
                                                                                           preapprovedKeys: preapprovedKeys,
                                                                                           vouchers: [],
                                                                                           egoPeerKeys: egoPeerKeys)
                        } catch {
                            logger.error("Unable to create peer for joining: \(String(describing: error), privacy: .public)")
                            reply(nil, [], nil, error)
                            return
                        }

                        guard let peerStableInfo = peer.stableInfoAndSig.toStableInfo() else {
                            logger.info("Unable to create new peer stable info for joining")
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
                            logger.error("Unable to process keys before joining: \(String(describing: error), privacy: .public)")
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

                        logger.info("Beginning preapprovedJoin for peer \(egoPeerID, privacy: .public)")
                        logger.info("preapprovedJoin permanentInfo: \(egoPermData.base64EncodedString(), privacy: .public)")
                        logger.info("preapprovedJoin permanentInfoSig: \(egoPermSig.base64EncodedString(), privacy: .public)")
                        logger.info("preapprovedJoin stableInfo: \(egoStableData.base64EncodedString(), privacy: .public)")
                        logger.info("preapprovedJoin stableInfoSig: \(egoStableSig.base64EncodedString(), privacy: .public)")
                        logger.info("preapprovedJoin dynamicInfo: \(peer.dynamicInfoAndSig.peerDynamicInfo.base64EncodedString(), privacy: .public)")
                        logger.info("preapprovedJoin dynamicInfoSig: \(peer.dynamicInfoAndSig.sig.base64EncodedString(), privacy: .public)")

                        logger.info("preapprovedJoin vouchers: \(peer.vouchers.map { $0.voucher.base64EncodedString() }, privacy: .public)")
                        logger.info("preapprovedJoin voucher signatures: \(peer.vouchers.map { $0.sig.base64EncodedString() }, privacy: .public)")

                        logger.info("preapprovedJoin: uploading \(allTLKShares.count) tlk shares")

                        do {
                            let peerBase64 = try peer.serializedData().base64EncodedString()
                            logger.info("preapprovedJoin peer: \(peerBase64, privacy: .public)")
                        } catch {
                            logger.info("preapprovedJoin unable to encode peer: \(String(describing: error), privacy: .public)")
                        }

                        let changeToken = self.changeToken()
                        let request = JoinWithVoucherRequest.with {
                            $0.changeToken = changeToken
                            $0.peer = peer
                            $0.bottle = bottle
                            $0.tlkShares = allTLKShares
                            $0.viewKeys = viewKeys
                            $0.trustedDevicesVersion = self.idmsTrustedDevicesVersion()
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
                                        logger.info("preapprovedJoin succeeded")

                                        let keyHierarchyRecords = response.zoneKeyHierarchyRecords.compactMap { CKRecord($0) }
                                        reply(egoPeerID, keyHierarchyRecords, syncingPolicy, nil)
                                    } catch {
                                        logger.error("preapprovedJoin failed: \(String(describing: error), privacy: .public)")
                                        reply(nil, [], nil, error)
                                    }
                                }
                            case .failure(let error):
                                logger.error("preapprovedJoin failed: \(String(describing: error), privacy: .public)")
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
                walrusSetting: TPPBPeerStableInfoSetting?,
                webAccess: TPPBPeerStableInfoSetting?,
                reply: @escaping (TrustedPeersHelperPeerState?, TPSyncingPolicy?, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (TrustedPeersHelperPeerState?, TPSyncingPolicy?, Error?) -> Void = {
            let logType: OSLogType = $2 == nil ? .info : .error
            logger.log(level: logType, "update complete: \(traceError($2), privacy: .public)")
            sem.release()
            reply($0, $1, $2)
        }

        // Get (and save) the latest from cuttlefish
        let stableChanges = StableChanges(deviceName: deviceName,
                                          serialNumber: serialNumber,
                                          osVersion: osVersion,
                                          policyVersion: policyVersion,
                                          policySecrets: policySecrets,
                                          setSyncUserControllableViews: syncUserControllableViews,
                                          secureElementIdentity: secureElementIdentity,
                                          walrusSetting: walrusSetting,
                                          webAccess: webAccess
        )
        self.fetchChangesAndUpdateTrustIfNeeded(forceRefetch: forceRefetch, stableChanges: stableChanges, reply: reply)
    }

    func set(preapprovedKeys: [Data],
             reply: @escaping (TrustedPeersHelperPeerState?, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (TrustedPeersHelperPeerState?, Error?) -> Void = {
            let logType: OSLogType = $1 == nil ? .info : .error
            logger.log(level: logType, "setPreapprovedKeys complete: \(traceError($1), privacy: .public)")
            sem.release()
            reply($0, $1)
        }

        self.moc.performAndWait {
            logger.info("setPreapprovedKeys: \(preapprovedKeys, privacy: .public)")

            guard let egoPeerID = self.containerMO.egoPeerID else {
                // No identity, nothing to do
                logger.info("setPreapprovedKeys: No identity.")
                reply(nil, ContainerError.noPreparedIdentity)
                return
            }
            loadEgoKeyPair(identifier: signingKeyIdentifier(peerID: egoPeerID)) { signingKeyPair, error in
                guard let signingKeyPair = signingKeyPair else {
                    logger.error("setPreapprovedKeys: no signing key pair: \(String(describing: error), privacy: .public)")
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
                        logger.error("setPreapprovedKeys: couldn't calculate dynamic info: \(String(describing: error), privacy: .public)")
                        reply(nil, error)
                        return
                    }

                    logger.info("setPreapprovedKeys: produced a dynamicInfo: \(dynamicInfo, privacy: .public)")

                    let egoPeer: TPPeer?
                    do {
                        egoPeer = try self.model.peer(withID: egoPeerID)
                    } catch {
                        logger.warning("setPreapprovedKeys: error getting ego peer from model: \(String(describing: error), privacy: .public)")
                        egoPeer = nil
                    }
                    if dynamicInfo == egoPeer?.dynamicInfo {
                        logger.info("setPreapprovedKeys: no change; nothing to do.")

                        // Calling this will fill in the peer status
                        self.updateTrustIfNeeded { status, _, error in
                            reply(status, error)
                        }
                        return
                    }

                    logger.info("setPreapprovedKeys: attempting updateTrust for \(egoPeerID, privacy: .public) with: \(dynamicInfo, privacy: .public)")
                    let request = UpdateTrustRequest.with {
                        $0.changeToken = self.changeToken()
                        $0.peerID = egoPeerID
                        $0.dynamicInfoAndSig = SignedPeerDynamicInfo(dynamicInfo)
                        $0.trustedDevicesVersion = self.idmsTrustedDevicesVersion()
                    }

                    self.perform(updateTrust: request) { state, _, error in
                        guard error == nil else {
                            logger.error("setPreapprovedKeys: failed: \(String(describing: error), privacy: .public)")
                            reply(state, error)
                            return
                        }

                        logger.info("setPreapprovedKeys: updateTrust succeeded")
                        reply(state, nil)
                    }
                }
            }
        }
    }

    func updateTLKs(ckksKeys: [CKKSKeychainBackedKeySet],
                    tlkShares: [CKKSTLKShare],
                    reply: @escaping ([CKRecord]?, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: ([CKRecord]?, Error?) -> Void = {
            let logType: OSLogType = $1 == nil ? .info : .error
            logger.log(level: logType, "updateTLKs complete: \(traceError($1), privacy: .public)")
            sem.release()
            reply($0, $1)
        }

        logger.info("Uploading some new TLKs: \(ckksKeys, privacy: .public)")

        self.moc.performAndWait {
            self.onqueueUpdateTLKs(ckksKeys: ckksKeys, tlkShares: tlkShares, reply: reply)
        }
    }

    func _onqueueUpdateTrustTLKs(egoPeerID: String,
                                 tlkShares: [TLKShare],
                                 viewKeys: [ViewKeys],
                                 stride: any IteratorProtocol<Int>,
                                 result: [CKRecord],
                                 reply: @escaping (Result<[CKRecord], Error>) -> Void) {
        var stride = stride
        guard let it = stride.next() else {
            reply(.success(result))
            return
        }

        let request = UpdateTrustRequest.with {
            let startIndex = it
            let end = min(startIndex + tlkSharesBatch, tlkShares.count)
            $0.changeToken = self.changeToken()
            $0.peerID = egoPeerID
            $0.tlkShares = Array(tlkShares[startIndex..<end])
            $0.viewKeys = viewKeys
            $0.trustedDevicesVersion = self.idmsTrustedDevicesVersion()
        }

        self.cuttlefish.updateTrust(request) { response in
            switch response {
            case .success(let response):
                self.moc.performAndWait {
                    do {
                        try self.onQueuePersist(changes: response.changes)
                    } catch {
                        logger.error("persisting changes failed: \(String(describing: error), privacy: .public)")
                        reply(.failure(error))
                        return
                    }
                    let result2 = result + response.zoneKeyHierarchyRecords.compactMap { CKRecord($0) }
                    self._onqueueUpdateTrustTLKs(egoPeerID: egoPeerID,
                                                 tlkShares: tlkShares,
                                                 viewKeys: viewKeys,
                                                 stride: stride,
                                                 result: result2,
                                                 reply: reply)
                }
            case .failure(let error):
                reply(.failure(error))
            }
        }
    }

    func onqueueUpdateTrustTLKs(egoPeerID: String,
                                tlkShares: [TLKShare],
                                viewKeys: [ViewKeys],
                                reply: @escaping (Result<[CKRecord], Error>) -> Void) {
        self._onqueueUpdateTrustTLKs(egoPeerID: egoPeerID,
                                     tlkShares: tlkShares,
                                     viewKeys: viewKeys,
                                     stride: stride(from: 0, to: tlkShares.count, by: tlkSharesBatch).makeIterator(),
                                     result: [CKRecord](),
                                     reply: reply)
    }

    func onqueueUpdateTLKs(ckksKeys: [CKKSKeychainBackedKeySet],
                           tlkShares: [CKKSTLKShare],
                           reply: @escaping ([CKRecord]?, Error?) -> Void) {
        guard let egoPeerID = self.containerMO.egoPeerID,
            let egoPermData = self.containerMO.egoPeerPermanentInfo,
            let egoPermSig = self.containerMO.egoPeerPermanentInfoSig
            else {
                logger.info("Have no self identity, can't make tlk shares")
                reply(nil, ContainerError.noPreparedIdentity)
                return
        }

        guard let selfPermanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: egoPermData, sig: egoPermSig, keyFactory: TPECPublicKeyFactory()) else {
            logger.info("Couldn't parse self identity: \(String(describing: ckksKeys), privacy: .public)")
            reply(nil, ContainerError.invalidPermanentInfoOrSig)
            return
        }

        loadEgoKeys(peerID: egoPeerID) { egoPeerKeys, error in
            guard let egoPeerKeys = egoPeerKeys else {
                logger.error("Don't have my own peer keys; can't upload new TLKs: \(String(describing: error), privacy: .public)")
                reply(nil, error)
                return
            }
            self.moc.performAndWait {
                let egoPeerDynamicInfo: TPPeerDynamicInfo?
                do {
                    egoPeerDynamicInfo = try self.model.getDynamicInfoForPeer(withID: egoPeerID)
                } catch {
                    logger.info("Unable to fetch dynamic info for self: \(error, privacy: .public)")
                    reply(nil, error)
                    return
                }
                guard let egoPeerDynamicInfo else {
                    logger.info("Unable to fetch dynamic info for self")
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
                    logger.error("Unable to process keys before uploading: \(String(describing: error), privacy: .public)")
                    reply(nil, error)
                    return
                }

                self.onqueueUpdateTrustTLKs(egoPeerID: egoPeerID,
                                            tlkShares: allTLKShares,
                                            viewKeys: viewKeys) { result in
                    switch result {
                    case .success(let keyHierarchyRecords):
                        logger.info("Received \(keyHierarchyRecords.count) CKRecords back")
                        reply(keyHierarchyRecords, nil)
                    case .failure(let error):
                        reply(nil, error)
                    }
                }
            }
        }
    }

    func getState(reply: @escaping (ContainerState) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (ContainerState) -> Void = {
            logger.info("getState complete: \(String(describing: $0.egoPeerID), privacy: .public)")
            sem.release()
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

            do {
                try self.model.enumeratePeers { peer, _ in
                    state.peers[peer.peerID] = peer
                }
            } catch {
                logger.error("getState: error enumerating peers: \(error, privacy: .public)")
                state.peerError = error
            }

            do {
                try self.model.enumerateVouchers { voucher, _ in
                    state.vouchers.append(voucher)
                }
            } catch {
                logger.error("getState: error enumerating vouchers: \(error, privacy: .public)")
                state.voucherError = error
            }

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
                logger.info("Forcing a full refetch: by request")
                causeRefetch = true
            } else {
                let modelRefetch: Bool
                do {
                    modelRefetch = try self.model.currentStatePossiblyMissingData()
                } catch {
                    logger.error("currentStatePossiblyMissingData error: \(error, privacy: .public)")
                    reply(error)
                    return
                }

                if modelRefetch && (self.containerMO.refetchLevel < Container.currentRefetchLevel) {
                    logger.info("Forcing a full refetch due to model: last refetch level: \(self.containerMO.refetchLevel)")
                    causeRefetch = true
                } else if modelRefetch {
                    logger.info("Model would like a full refetch, but we've done one at this refetch level: \(self.containerMO.refetchLevel)")
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
                $0.changeToken = self.changeToken()
            }
        }
        logger.info("Fetching with change token: \(!request.changeToken.isEmpty ? request.changeToken : "empty", privacy: .public)")

        self.cuttlefish.fetchChanges(request) { response in
            switch response {
            case .success(let response):
                do {
                    try self.persist(changes: response.changes, refetchForced: forceRefetch)
                } catch {
                    logger.error("Could not persist changes: \(String(describing: error), privacy: .public)")
                    reply(error)
                    return
                }

                if response.changes.more {
                    logger.info("persist: More changes indicated. Fetching...")
                    self.fetchAndPersistChanges(reply: reply)
                    return
                } else {
                    logger.info("persist: no more changes!")
                    reply(nil)
                }
            case .failure(let error):
                switch error {
                case CuttlefishErrorMatcher(code: CuttlefishErrorCode.changeTokenExpired):
                    logger.info("change token is expired; resetting local CK storage")

                    self.moc.performAndWait {
                        do {
                            try self.deleteLocalCloudKitData()
                        } catch {
                            logger.error("Failed to reset local data: \(String(describing: error), privacy: .public)")
                            reply(error)
                            return
                        }

                        self.fetchAndPersistChanges(reply: reply)
                    }

                    return
                default:
                    logger.info("Fetch error is an unknown error: \(String(describing: error), privacy: .public)")
                }

                logger.error("Could not fetch changes: \(String(describing: error), privacy: .public)")
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
                logger.error("fetchChangesAndUpdateTrustIfNeeded: fetching failed: \(String(describing: error), privacy: .public)")
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
                logger.info("updateTrustIfNeeded: No identity.")
                reply(TrustedPeersHelperPeerState(peerID: nil, isPreapproved: false, status: .unknown, memberChanges: peerChanges, unknownMachineIDs: false, osVersion: nil, walrus: nil, webAccess: nil),
                      nil,
                      nil)
                return
            }
            loadEgoKeyPair(identifier: signingKeyIdentifier(peerID: egoPeerID)) { signingKeyPair, error in
                guard let signingKeyPair = signingKeyPair else {
                    logger.error("updateTrustIfNeeded: no signing key pair: \(String(describing: error), privacy: .public)")
                    reply(TrustedPeersHelperPeerState(peerID: nil, isPreapproved: false, status: .unknown, memberChanges: peerChanges, unknownMachineIDs: false, osVersion: nil, walrus: nil, webAccess: nil),
                          nil,
                          error)
                    return
                }
                let currentSelfInModel: TPPeer?
                do {
                    currentSelfInModel = try self.model.peer(withID: egoPeerID)
                } catch {
                    logger.warning("Failed to get (current self) ego peer from model: \(String(describing: error), privacy: .public)")
                    currentSelfInModel = nil
                }
                guard let currentSelfInModel else {
                    // Not in circle, nothing to do
                    do {
                        let isPreapproved = try self.model.hasPotentiallyTrustedPeerPreapprovingKey(signingKeyPair.publicKey().spki())
                        logger.info("updateTrustIfNeeded: ego peer is not in model, is \(isPreapproved ? "preapproved" : "not yet preapproved", privacy: .public)")
                        reply(TrustedPeersHelperPeerState(peerID: egoPeerID,
                                                          isPreapproved: isPreapproved,
                                                          status: .unknown,
                                                          memberChanges: peerChanges,
                                                          unknownMachineIDs: false,
                                                          osVersion: nil,
                                                          walrus: nil,
                                                          webAccess: nil),
                              nil,
                              nil)
                    } catch {
                        logger.error("updateTrustIfNeeded: error calling hasPotentiallyTrustedPeerPreapprovingKey \(error, privacy: .public)")
                        reply(nil, nil, error)
                    }
                    return
                }
                let oldDynamicInfo = currentSelfInModel.dynamicInfo

                // We need to try to have all policy versions that our peers claim to behave
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
                self.fetchPolicyDocumentsWithSemaphore(versions: allPolicyVersions) { _, policyFetchError in
                    if let error = policyFetchError {
                        logger.info("updateTrustIfNeeded: error fetching all requested policies (continuing anyway): \(String(describing: error), privacy: .public)")
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
                                                                              signingKeyPair: signingKeyPair,
                                                                              vouchers: nil)
                        } catch {
                            logger.info("updateTrustIfNeeded: couldn't calculate dynamic info: \(String(describing: error), privacy: .public)")
                            let status: TPPeerStatus
                            do {
                                status = try self.model.statusOfPeer(withID: egoPeerID)
                            } catch {
                                logger.warning("updateTrustIfNeeded: ignoring additional error calling statusOfPeer: \(error, privacy: .public)")
                                status = .unknown
                            }
                            reply(TrustedPeersHelperPeerState(peerID: egoPeerID,
                                                              isPreapproved: false,
                                                              status: status,
                                                              memberChanges: peerChanges,
                                                              unknownMachineIDs: false,
                                                              osVersion: nil,
                                                              walrus: nil,
                                                              webAccess: nil),
                                  nil,
                                  error)
                            return
                        }

                        logger.info("updateTrustIfNeeded: produced a stableInfo: \(String(describing: stableInfo), privacy: .public)")
                        logger.info("updateTrustIfNeeded: produced a dynamicInfo: \(String(describing: dynamicInfo), privacy: .public)")

                        let peer: TPPeer?
                        do {
                            peer = try self.model.peer(withID: egoPeerID)
                        } catch {
                            logger.warning("Error getting ego peer from model: \(String(describing: error), privacy: .public)")
                            peer = nil
                        }
                        if (stableInfo == nil || stableInfo == peer?.stableInfo) &&
                            dynamicInfo == peer?.dynamicInfo {
                            logger.info("updateTrustIfNeeded: complete.")
                            // No change to the dynamicInfo: update the MID list now that we've reached a steady state
                            do {
                                self.onqueueUpdateMachineIDListFromModel(dynamicInfo: dynamicInfo)
                                try self.moc.save()
                            } catch {
                                logger.error("updateTrustIfNeeded: unable to remove untrusted MachineIDs: \(String(describing: error), privacy: .public)")
                            }

                            let syncingPolicy: TPSyncingPolicy?
                            do {
                                if let peer, let stableInfo = peer.stableInfo {
                                    syncingPolicy = try self.syncingPolicyFor(modelID: peer.permanentInfo.modelID, stableInfo: stableInfo)
                                } else {
                                    syncingPolicy = nil
                                }
                            } catch {
                                logger.error("updateTrustIfNeeded: unable to compute a new syncing policy: \(String(describing: error), privacy: .public)")
                                syncingPolicy = nil
                            }

                            let status: TPPeerStatus
                            do {
                                status = try self.model.statusOfPeer(withID: egoPeerID)
                            } catch {
                                logger.warning("updateTrustIfNeeded: ignoring additional error calling statusOfPeer: \(error, privacy: .public)")
                                status = .unknown
                            }

                            reply(TrustedPeersHelperPeerState(peerID: egoPeerID,
                                                              isPreapproved: false,
                                                              status: status,
                                                              memberChanges: peerChanges,
                                                              unknownMachineIDs: self.onqueueFullIDMSListWouldBeHelpful(),
                                                              osVersion: peer?.stableInfo?.osVersion,
                                                              walrus: peer?.stableInfo?.walrusSetting,
                                                              webAccess: peer?.stableInfo?.webAccess),
                                  syncingPolicy,
                                  nil)
                            return
                        }

                        let oldExcluded = Set(oldDynamicInfo?.excludedPeerIDs ?? [])
                        let newExcluded = Set(dynamicInfo.excludedPeerIDs)
                        let crkPeerIDs = self.model.allCustodianRecoveryKeys().map { $0.peerID }
                        let additions = newExcluded.subtracting(oldExcluded)
                        let newDistrustedCRKs = additions.intersection(crkPeerIDs)
                        if !newDistrustedCRKs.isEmpty {
                            logger.warning("Found CRKs that are being distrusted: \(newDistrustedCRKs)")
                            let ttr = SecTapToRadar(tapToRadar: "Recovery Contact or Legacy Contact just removed from another device",
                                                    description: "Please TTR unless you just removed a recovery or legacy contact",
                                                    radar: "115183035")
                            ttr.trigger()
                        }

                        // Check if we change that should trigger a notification that should trigger TLKShare updates
                        let havePeerChanges = peerChanges || self.haveTrustMemberChanges(newDynamicInfo: dynamicInfo, oldDynamicInfo: peer?.dynamicInfo)

                        let signedDynamicInfo = SignedPeerDynamicInfo(dynamicInfo)
                        logger.info("updateTrustIfNeeded: attempting updateTrust for \(egoPeerID, privacy: .public) with: \(dynamicInfo, privacy: .public)")
                        var request = UpdateTrustRequest.with {
                            $0.changeToken = self.changeToken()
                            $0.peerID = egoPeerID
                            $0.dynamicInfoAndSig = signedDynamicInfo
                            $0.trustedDevicesVersion = self.idmsTrustedDevicesVersion()
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
                    logger.error("UpdateTrust failed: \(String(describing: error), privacy: .public)")
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
                logger.error("UpdateTrust failed: \(String(describing: error), privacy: .public)")
                reply(nil, nil, error)
                return
            }
        }
    }

    private func persist(changes: Changes, refetchForced: Bool = false) throws {
        try self.moc.performAndWait {
            logger.info("persist: Received \(changes.differences.count) peer differences, more: \(changes.more)")
            logger.info("persist: New change token: \(changes.changeToken, privacy: .public)")
            try self.onQueuePersist(changes: changes, refetchForced: refetchForced)
        }
    }

    // For tests
    internal func removePeer(peerID: String) {
        do {
            try self.moc.performAndWait {
                try onQueueRemovePeer(peerID: peerID)
                try self.moc.save()
            }
        } catch {
            logger.error("removePeer unable to remove peerID \(peerID, privacy: .public)): \(String(describing: error), privacy: .public)")
        }
    }

    // Must be on moc queue to call this.
    private func onQueueRemovePeer(peerID: String) throws {
        if let peerMO = try self.fetchPeerMO(peerID: peerID) {
            self.moc.delete(peerMO)
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
            logger.info("escrow cache and viable bottles are no longer valid")
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
                    try onQueueRemovePeer(peerID: peer.peerID)
                }
            }
        }

        let signingKey = changes.recoverySigningPubKey
        let encryptionKey = changes.recoveryEncryptionPubKey

        if !signingKey.isEmpty && !encryptionKey.isEmpty {
            self.addOrUpdate(signingKey: signingKey, encryptionKey: encryptionKey)
        }

        try self.moc.save()

        // Don't print this if there's more changes coming; the logs will be very confusing
        if !changes.more {
            do {
                let peerCount = try self.model.peerCount()
                logger.info("Currently know about \(peerCount) peers")
            } catch {
                logger.error("Error getting peerCount: \(error, privacy: .public)")
            }
        }
    }

    // Must be on moc queue to call this
    // Deletes all things that should come back from CloudKit. Keeps the egoPeer data, though.
    private func deleteLocalCloudKitData() throws {
        logger.info("Deleting all CloudKit data")

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

            (self.model, self.dbAdapter) = Container.loadModel(moc: self.moc, containerMO: self.containerMO, hmacKey: self.getHmacKey())
            try self.moc.save()
        } catch {
            logger.error("Local delete failed: \(String(describing: error), privacy: .public)")
            throw error
        }

        do {
            let peerCount = try self.model.peerCount()
            logger.info("Saved model with \(peerCount) peers")
        } catch {
            logger.error("error getting peerCount: \(error, privacy: .public)")
            throw error
        }
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
            logger.info("failed to parse custodian recovery key")
            return
        }
        logger.info("Register CRK with peerID \(peer.peerID, privacy: .public)")
        self.model.register(crk)
        let crkMO = CustodianRecoveryKeyMO(context: self.moc)
        crkMO.crkInfo = crk.data
        crkMO.crkInfoSig = crk.sig
        self.containerMO.addToCustodianRecoveryKeys(crkMO)
    }

    // Must be on moc queue to call this.
    private func addOrUpdate(peer: Peer) throws {
        let peerID = peer.peerID
        if try !self.model.hasPeer(withID: peerID) {
            // Add:
            guard let permanentInfo = peer.permanentInfoAndSig.toPermanentInfo(peerID: peerID) else {
                if peer.hasCustodianRecoveryKeyAndSig {
                    if self.testIgnoreCustodianUpdates {
                        logger.info("Ignoring Custodian update due to test request: \(String(describing: peer), privacy: .public)")
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
            let isEgoPeer = peerID == self.containerMO.egoPeerID
            let tppeer = try TPPeer(permanentInfo: permanentInfo, stableInfo: stableInfo, dynamicInfo: dynamicInfo)
            try self.registerPeerMO(peer: tppeer,
                                    vouchers: vouchers,
                                    isEgoPeer: isEgoPeer)
        } else {
            // Update:
            guard let peerMO = try self.fetchPeerMO(peerID: peerID) else {
                throw ContainerError.peerRegisteredButNotStored(peerID)
            }

            let stableInfo = peer.stableInfoAndSig.toStableInfo()
            if let stableInfo {
                // Ask the model to return a new peer with updated stable info, and persist that.
                // The model checks signatures and clocks to prevent replay attacks.
                let modelPeer = try self.model.copyPeer(withNewStableInfo: stableInfo, forPeerWithID: peerID)
                peerMO.stableInfo = modelPeer.stableInfo?.data
                peerMO.stableInfoSig = modelPeer.stableInfo?.sig
            }
            let dynamicInfo = peer.dynamicInfoAndSig.toDynamicInfo()
            if let dynamicInfo {
                // Ask the model to return a new peer with updated dynamic info, and persist that.
                // The model checks signatures and clocks to prevent replay attacks.
                let modelPeer = try self.model.copyPeer(withNewDynamicInfo: dynamicInfo, forPeerWithID: peerID)
                peerMO.dynamicInfo = modelPeer.dynamicInfo?.data
                peerMO.dynamicInfoSig = modelPeer.dynamicInfo?.sig
            }
            peer.vouchers.forEach {
                guard let voucher = TPVoucher(infoWith: $0.voucher, sig: $0.sig) else {
                    return
                }
                let peerMOVouchers = peerMO.vouchers as? Set<VoucherMO> ?? Set()
                if !(peerMOVouchers.contains { $0.voucherInfo == voucher.data && $0.voucherInfoSig == voucher.sig }) {
                    let voucherMO = VoucherMO(context: self.moc)
                    voucherMO.voucherInfo = voucher.data
                    voucherMO.voucherInfoSig = voucher.sig
                    peerMO.addToVouchers(voucherMO)
                }
            }
            guard let permanentInfoData = peerMO.permanentInfo, let permanentInfoSig = peerMO.permanentInfoSig else {
                logger.error("addOrUpdate peer \(peerID, privacy: .public) has no/incomplete permanent info/sig")
                return
            }
            let keyFactory = TPECPublicKeyFactory()
            guard let permanentInfo = TPPeerPermanentInfo(peerID: peerID,
                                                          data: permanentInfoData,
                                                          sig: permanentInfoSig,
                                                          keyFactory: keyFactory) else {
                logger.error("Couldn't parse peer identity: \(String(describing: peerID), privacy: .public)")
                return
            }
            do {
                let tppeer = try TPPeer(permanentInfo: permanentInfo, stableInfo: stableInfo, dynamicInfo: dynamicInfo)
                if let hmacKey = self.getHmacKey() {
                    peerMO.hmacSig = tppeer.calculateHmac(withHmacKey: hmacKey)
                } else {
                    peerMO.hmacSig = nil
                }
            } catch {
                logger.error("failed to construct peer for \(String(describing: peerID), privacy: .public): \(error)")
            }
        }
    }

    // Must be on moc queue to call this.
    internal func fetchPeerMO(peerID: String) throws -> PeerMO? {
        return try self.dbAdapter.fetchPeerMO(peerID: peerID)
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
                                             signingKeyPair: _SFECKeyPair,
                                             vouchers: [SignedVoucher]?) throws -> TPPeerStableInfo? {
        func noChange<T: Equatable>(_ change: T?, _ existing: T?) -> Bool {
            return (nil == change) || change == existing
        }

        func rkNoChange<T: Equatable>(_ change: T?, _ existing: T?) -> Bool {
            return change == existing
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
        let tpVouchers = vouchers?.compactMap { TPVoucher(infoWith: $0.voucher, sig: $0.sig) }.filter { $0.beneficiaryID == permanentInfo.peerID }

        let optimalRecoveryKey = try self.model.bestRecoveryKey(for: existingStableInfo, vouchers: tpVouchers)

        // Determine which walrus setting we'd like to be using, given our current idea of who to trust
        var optimalWalrusSetting: TPPBPeerStableInfoSetting?
        if self.testDontSetAccountSetting == false {
            optimalWalrusSetting = try self.model.bestWalrus(for: existingStableInfo, walrusStableChanges: stableChanges?.walrusSetting)
        }

        // Determine which web access setting we'd like to be using, given our current idea of who to trust
        var optimalWebAccessSetting: TPPBPeerStableInfoSetting?
        if self.testDontSetAccountSetting == false {
            optimalWebAccessSetting = try self.model.bestWebAccess(for: existingStableInfo, webAccessStableChanges: stableChanges?.webAccess)
        }

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
            rkNoChange(optimalRecoveryKey?.signingKeyData, existingStableInfo?.recoverySigningPublicKey) &&
            rkNoChange(optimalRecoveryKey?.encryptionKeyData, existingStableInfo?.recoveryEncryptionPublicKey) &&
            noChange(intendedSyncUserControllableViews, existingStableInfo?.syncUserControllableViews)
        noChanges = noChanges && noChange(optimalWalrusSetting, existingStableInfo?.walrusSetting)
        noChanges = noChanges && noChange(optimalWebAccessSetting, existingStableInfo?.webAccess)
        noChanges = noChanges && (intendedSecureElementIdentity == existingStableInfo?.secureElementIdentity)

        if noChanges {
            return nil
        }

        // If a test has asked a policy version before we froze this policy, then don't set a flexible version--it's trying to build a peer from before the policy was frozen
        let optimalPolicyVersion = try self.getPolicyDoc(optimalPolicyVersionNumber).version
        let useFrozenPolicyVersion = optimalPolicyVersion.versionNumber >= frozenPolicyVersion.versionNumber

        if let intendedSyncUserControllableViews = intendedSyncUserControllableViews {
            logger.info("Intending to set user-controllable views to \(TPPBPeerStableInfoUserControllableViewStatusAsString(intendedSyncUserControllableViews), privacy: .public)")
        }

        return try self.model.createStableInfo(withFrozenPolicyVersion: useFrozenPolicyVersion ? frozenPolicyVersion : optimalPolicyVersion,
                                               flexiblePolicyVersion: useFrozenPolicyVersion ? optimalPolicyVersion : nil,
                                               policySecrets: stableChanges?.policySecrets ?? existingStableInfo?.policySecrets,
                                               syncUserControllableViews: intendedSyncUserControllableViews ?? existingStableInfo?.syncUserControllableViews ?? .UNKNOWN,
                                               secureElementIdentity: intendedSecureElementIdentity,
                                               walrusSetting: optimalWalrusSetting,
                                               webAccess: optimalWebAccessSetting,
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

    func isRecoveryKeySet(reply: @escaping(Bool, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (Bool, Error?) -> Void = {
            let logType: OSLogType = $1 == nil ? .info : .error
            logger.log(level: logType, "isRecoveryKeySet complete \(traceError($1), privacy: .public)")
            sem.release()
            reply($0, $1)
        }

        self.fetchAndPersistChanges { fetchError in
            guard fetchError == nil else {
                reply(false, fetchError)
                return
            }
            self.moc.performAndWait {
                let isSet: Bool
                do {
                    isSet = try self.model.isRecoveryKeyEnrolled()
                } catch {
                    logger.error("Error determining whether Recovery Key is enrolled: \(error, privacy: .public)")

                    reply(false, error)
                    return
                }

                logger.info("recoveryKey is enrolled \(isSet, privacy: .public)")

                reply(isSet, nil)
                return
            }
        }
    }

    func removeRecoveryKey(reply: @escaping (Bool, Error?) -> Void) {
        let sem = self.grabSemaphore()
        let reply: (Bool, Error?) -> Void = {
            let logType: OSLogType = $1 == nil ? .info : .error
            logger.log(level: logType, "removeRecoveryKey complete: \(traceError($1), privacy: .public)")
            sem.release()
            reply($0, $1)
        }

        logger.info("beginning a removeRecoveryKey")

        self.fetchAndPersistChanges { fetchError in
            guard fetchError == nil else {
                reply(false, fetchError)
                return
            }

            self.moc.performAndWait {
                guard let egoPeerID = self.containerMO.egoPeerID else {
                    logger.info("no prepared identity, cannot remove recovery key")
                    reply(false, ContainerError.noPreparedIdentity)
                    return
                }
                guard let stableInfoData = self.containerMO.egoPeerStableInfo else {
                    logger.info("stableInfo does not exist")
                    reply(false, ContainerError.nonMember)
                    return
                }
                guard let stableInfoSig = self.containerMO.egoPeerStableInfoSig else {
                    logger.info("stableInfoSig does not exist")
                    reply(false, ContainerError.nonMember)
                    return
                }
                guard let stableInfo = TPPeerStableInfo(data: stableInfoData, sig: stableInfoSig) else {
                    logger.info("cannot create TPPeerStableInfo")
                    reply(false, ContainerError.invalidStableInfoOrSig)
                    return
                }
                guard let permInfoData = self.containerMO.egoPeerPermanentInfo else {
                    logger.info("permanentInfo does not exist")
                    reply(false, ContainerError.nonMember)
                    return
                }
                guard let permInfoSig = self.containerMO.egoPeerPermanentInfoSig else {
                    logger.info("permInfoSig does not exist")
                    reply(false, ContainerError.nonMember)
                    return
                }
                let keyFactory = TPECPublicKeyFactory()
                guard let permanentInfo = TPPeerPermanentInfo(peerID: egoPeerID, data: permInfoData, sig: permInfoSig, keyFactory: keyFactory) else {
                    logger.info("cannot create TPPeerPermanentInfo")
                    reply(false, ContainerError.invalidStableInfoOrSig)
                    return
                }

                let peerViews: Set<String>
                do {
                    peerViews = try self.model.getViewsForPeer(permanentInfo, stableInfo: stableInfo)
                } catch {
                    logger.error("cannot create peerViews: \(error))")
                    reply(false, ContainerError.failedToGetPeerViews(suberror: error))
                    return
                }

                do {
                    if try self.model.isRecoveryKeyEnrolled() == false {
                        logger.info("recovery key is not registered, nothing to remove.")
                        reply(true, nil)
                        return
                    }
                } catch {
                    logger.error("error determining whether Recovery Key is enrolled: \(error, privacy: .public)")
                    reply(false, error)
                    return
                }

                let currentRecoveryKeyPeer: TrustedPeersHelperPeer
                do {
                    currentRecoveryKeyPeer = try RecoveryKey.asPeer(recoveryKeys: TPRecoveryKeyPair(stableInfo: stableInfo), viewList: peerViews)
                } catch {
                    logger.error("cannot create recovery key peer: \(error)")
                    reply(false, ContainerError.cannotCreateRecoveryKeyPeer(suberror: error))
                    return
                }

                loadEgoKeyPair(identifier: signingKeyIdentifier(peerID: egoPeerID)) { signingKeyPair, error in
                    guard let signingKeyPair = signingKeyPair else {
                        logger.error("handle: no signing key pair: \(String(describing: error), privacy: .public)")
                        reply(false, error)
                        return
                    }
                    self.moc.performAndWait {
                        do {
                            let policyVersion = stableInfo.bestPolicyVersion()
                            let policyDoc = try self.getPolicyDoc(policyVersion.versionNumber)

                            let updatedStableInfo = try TPPeerStableInfo(clock: stableInfo.clock + 1,
                                                                         frozenPolicyVersion: frozenPolicyVersion,
                                                                         flexiblePolicyVersion: policyDoc.version,
                                                                         policySecrets: stableInfo.policySecrets,
                                                                         syncUserControllableViews: stableInfo.syncUserControllableViews,
                                                                         secureElementIdentity: stableInfo.secureElementIdentity,
                                                                         walrusSetting: stableInfo.walrusSetting,
                                                                         webAccess: stableInfo.webAccess,
                                                                         deviceName: stableInfo.deviceName,
                                                                         serialNumber: stableInfo.serialNumber,
                                                                         osVersion: stableInfo.osVersion,
                                                                         signing: signingKeyPair,
                                                                         recoverySigningPubKey: nil,
                                                                         recoveryEncryptionPubKey: nil,
                                                                         isInheritedAccount: stableInfo.isInheritedAccount)
                            let signedStableInfo = SignedPeerStableInfo(updatedStableInfo)

                            guard let rkPeerID = currentRecoveryKeyPeer.peerID else {
                                logger.error("Error creating recovery key peerid: \(String(describing: error), privacy: .public)")
                                reply(false, error)
                                return
                            }

                            let dynamicInfo: TPPeerDynamicInfo
                            do {
                                dynamicInfo = try self.model.calculateDynamicInfoForPeer(withID: egoPeerID,
                                                                                         addingPeerIDs: nil,
                                                                                         removingPeerIDs: [rkPeerID],
                                                                                         preapprovedKeys: nil,
                                                                                         signing: signingKeyPair,
                                                                                         currentMachineIDs: self.onqueueCurrentMIDList())
                            } catch {
                                logger.error("Error preparing dynamic info: \(String(describing: error), privacy: .public)")
                                reply(false, error)
                                return
                            }

                            let signedDynamicInfo = SignedPeerDynamicInfo(dynamicInfo)

                            let request = RemoveRecoveryKeyRequest.with {
                                $0.peerID = egoPeerID
                                $0.stableInfoAndSig = signedStableInfo
                                $0.dynamicInfoAndSig = signedDynamicInfo
                                $0.changeToken = self.changeToken()
                            }

                            self.cuttlefish.removeRecoveryKey(request) { response in
                                switch response {
                                case .success(let response):
                                    self.moc.performAndWait {
                                        do {
                                            self.containerMO.egoPeerStableInfo = updatedStableInfo.data
                                            self.containerMO.egoPeerStableInfoSig = updatedStableInfo.sig

                                            try self.onQueuePersist(changes: response.changes)

                                            self.model.removeRecoveryKey()
                                            self.containerMO.recoveryKeySigningSPKI = nil
                                            self.containerMO.recoveryKeyEncryptionSPKI = nil

                                            logger.info("removeRecoveryKey succeeded")

                                            try self.moc.save()

                                            reply(true, nil)
                                        } catch {
                                            logger.error("removeRecoveryKey handling failed: \(String(describing: error), privacy: .public)")
                                            reply(false, error)
                                        }
                                    }
                                case .failure(let error):
                                    logger.error("removeRecoveryKey failed: \(String(describing: error), privacy: .public)")
                                    reply(false, error)
                                    return
                                }
                            }
                        } catch {
                            reply(false, error)
                        }
                    }
                }
            }
        }
    }

    func performATOPRVActions(reply: @escaping (Error?) -> Void) {
        self.cuttlefish.performAtoprvactions { response in
            switch response {
            case .success:
                reply(nil)
            case .failure(let error):
                logger.error("performATOPRVActions failed: \(String(describing: error), privacy: .public)")
                reply(error)
            }
        }
    }

    func testSemaphore(arg: String, reply: @escaping (Error?) -> Void) {
        guard SecIsInternalRelease() else {
            reply(ContainerError.operationNotImplemented)
            return
        }

        let sem = self.grabSemaphore()
        let reply: (Error?) -> Void = {
            let logType: OSLogType = $0 == nil ? .debug : .error
            logger.log(level: logType, "testSemaphore complete: \(traceError($0), privacy: .public)")
            sem.release()
            reply($0)
        }
        switch arg {
        case "noreply", "n":
            return
        case "dispatch", "d":
            let oneSecond = DispatchTimeInterval.seconds(1)
            DispatchQueue.global(qos: .userInitiated).asyncAfter(deadline: DispatchTime.now().advanced(by: oneSecond)) {
                reply(nil)
            }
        case "dispatch-noreply", "+", "dn", "d-n":
            let oneSecond = DispatchTimeInterval.seconds(1)
            DispatchQueue.global(qos: .userInitiated).asyncAfter(deadline: DispatchTime.now().advanced(by: oneSecond)) {
                _ = reply
            }
        case "double-release":
            reply(nil)
            reply(nil)
        default:
            reply(nil)
        }
    }
}
