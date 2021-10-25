/*
 * Copyright (c) 2019 Apple Inc. All Rights Reserved.
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
import SecurityFoundation

class RecoveryKey: NSObject {
    internal var peerKeys: OctagonSelfPeerKeys

    internal init(recoveryKeySet: RecoveryKeySet) throws {
        let peerID = RecoveryKey.PeerID(signingPublicKeyData: recoveryKeySet.signingKey.publicKey.keyData)

        try self.peerKeys = OctagonSelfPeerKeys(peerID: peerID, signingKey: recoveryKeySet.signingKey, encryptionKey: recoveryKeySet.encryptionKey)
    }

    internal convenience init(recoveryKeyString: String, recoverySalt: String) throws {
        let secret = Data(bytes: Array(recoveryKeyString.utf8), count: recoveryKeyString.utf8.count)
        let recoveryKeys = try RecoveryKeySet(secret: secret, recoverySalt: recoverySalt)
        try self.init(recoveryKeySet: recoveryKeys)
    }

    static func PeerID(signingPublicKeyData: Data) -> String {
        let hash = RecoveryKeySet.hashRecoveryedSigningPublicKey(keyData: signingPublicKeyData)
        let peerID = "RK-" + hash

        return peerID
    }

    static func spki(publicKeyData: Data) throws -> Data {
        let key = try _SFECPublicKey(data: publicKeyData, specifier: _SFECKeySpecifier(curve: SFEllipticCurve.nistp384))
        return key.encodeSubjectPublicKeyInfo()
    }

    static func asPeer(recoveryKeys: TPRecoveryKeyPair, viewList: Set<String>) throws -> TrustedPeersHelperPeer {
        return TrustedPeersHelperPeer(peerID: self.PeerID(signingPublicKeyData: recoveryKeys.signingKeyData),
                                      signingSPKI: try self.spki(publicKeyData: recoveryKeys.signingKeyData),
                                      encryptionSPKI: try self.spki(publicKeyData: recoveryKeys.encryptionKeyData),
                                      secureElementIdentity: nil,
                                      viewList: viewList)
    }
}

class CustodianRecoveryKey {
    internal var peerKeys: OctagonSelfPeerKeys
    internal var tpCustodian: TPCustodianRecoveryKey

    var peerID: String {
        return self.tpCustodian.peerID
    }

    internal init(uuid: UUID, recoveryKeySet: RecoveryKeySet, kind: TPPBCustodianRecoveryKey_Kind = .UNKNOWN) throws {
        self.tpCustodian = try TPCustodianRecoveryKey(uuid: uuid,
                                                      signing: recoveryKeySet.signingKey.publicKey(),
                                                      encryptionPublicKey: recoveryKeySet.encryptionKey.publicKey(),
                                                      signing: recoveryKeySet.signingKey,
                                                      kind: kind)

        self.peerKeys = try OctagonSelfPeerKeys(peerID: self.tpCustodian.peerID, signingKey: recoveryKeySet.signingKey, encryptionKey: recoveryKeySet.encryptionKey)
    }

    internal convenience init(uuid: UUID, recoveryKeyString: String, recoverySalt: String, kind: TPPBCustodianRecoveryKey_Kind = .UNKNOWN) throws {
        let secret = Data(bytes: Array(recoveryKeyString.utf8), count: recoveryKeyString.utf8.count)
        let recoveryKeys = try RecoveryKeySet(secret: secret, recoverySalt: recoverySalt)
        try self.init(uuid: uuid, recoveryKeySet: recoveryKeys, kind: kind)
    }

    internal init(tpCustodian: TPCustodianRecoveryKey, recoveryKeyString: String, recoverySalt: String) throws {
        let secret = Data(bytes: Array(recoveryKeyString.utf8), count: recoveryKeyString.utf8.count)
        let recoveryKeys = try RecoveryKeySet(secret: secret, recoverySalt: recoverySalt)

        guard recoveryKeys.signingKey.publicKey().spki() == tpCustodian.signingPublicKey.spki() &&
          recoveryKeys.encryptionKey.publicKey().spki() == tpCustodian.encryptionPublicKey.spki() else {
            throw RecoveryKey.Error.OTErrorEntropyKeyMismatch
        }

        self.tpCustodian = tpCustodian
        self.peerKeys = try OctagonSelfPeerKeys(peerID: self.tpCustodian.peerID, signingKey: recoveryKeys.signingKey, encryptionKey: recoveryKeys.encryptionKey)
    }
}

class InheritanceKey {
    internal var peerKeys: OctagonSelfPeerKeys
    internal var tpInheritance: TPCustodianRecoveryKey

    var peerID: String {
        return self.tpInheritance.peerID
    }

    internal init(uuid: UUID, recoveryKeySet: RecoveryKeySet) throws {
        self.tpInheritance = try TPCustodianRecoveryKey(uuid: uuid,
                                                        signing: recoveryKeySet.signingKey.publicKey(),
                                                        encryptionPublicKey: recoveryKeySet.encryptionKey.publicKey(),
                                                        signing: recoveryKeySet.signingKey,
                                                        kind: .INHERITANCE_KEY)

        self.peerKeys = try OctagonSelfPeerKeys(peerID: self.tpInheritance.peerID, signingKey: recoveryKeySet.signingKey, encryptionKey: recoveryKeySet.encryptionKey)
    }

    internal convenience init(uuid: UUID, recoveryKeyData: Data, recoverySalt: String) throws {
        let recoveryKeyString = recoveryKeyData.base64EncodedString()
        let secret = Data(bytes: Array(recoveryKeyString.utf8), count: recoveryKeyString.utf8.count)
        let recoveryKeys = try RecoveryKeySet(secret: secret, recoverySalt: recoverySalt)
        try self.init(uuid: uuid, recoveryKeySet: recoveryKeys)
    }

    internal init(tpInheritance: TPCustodianRecoveryKey, recoveryKeyData: Data, recoverySalt: String) throws {
        let recoveryKeyString = recoveryKeyData.base64EncodedString()
        let secret = Data(bytes: Array(recoveryKeyString.utf8), count: recoveryKeyString.utf8.count)
        let recoveryKeys = try RecoveryKeySet(secret: secret, recoverySalt: recoverySalt)

        guard recoveryKeys.signingKey.publicKey().spki() == tpInheritance.signingPublicKey.spki() &&
        recoveryKeys.encryptionKey.publicKey().spki() == tpInheritance.encryptionPublicKey.spki() else {
            throw RecoveryKey.Error.OTErrorEntropyKeyMismatch
        }

        self.tpInheritance = tpInheritance
        self.peerKeys = try OctagonSelfPeerKeys(peerID: self.tpInheritance.peerID, signingKey: recoveryKeys.signingKey, encryptionKey: recoveryKeys.encryptionKey)
    }
}

extension TPCustodianRecoveryKey {
    func asCustodianPeer(viewList: Set<String>) throws -> TrustedPeersHelperPeer {
        return TrustedPeersHelperPeer(peerID: self.peerID,
                                      signingSPKI: self.signingPublicKey.spki(),
                                      encryptionSPKI: self.encryptionPublicKey.spki(),
                                      secureElementIdentity: nil,
                                      viewList: viewList)
    }
}

extension RecoveryKey {
    enum Error: Swift.Error {
        case OTErrorDeserializationFailure
        case OTErrorDecryptionFailure
        case OTErrorKeyInstantiation
        case OTErrorKeyMismatch
        case OTErrorRecoveryCreation
        case OTErrorAuthCipherTextCreation
        case OTErrorPrivateKeyCreation
        case OTErrorRecoveryKeyCreation
        case OTErrorEntropyCreation
        case OTErrorEntropyKeyMismatch
    }
}

extension RecoveryKey.Error: LocalizedError {
    public var errorDescription: String? {
        switch self {
        case .OTErrorDeserializationFailure:
            return "Failed to deserialize Recovery peer"
        case .OTErrorDecryptionFailure:
            return "could not decrypt Recovery contents"
        case .OTErrorKeyInstantiation:
            return "Failed to instantiate octagon peer keys"
        case .OTErrorKeyMismatch:
            return "public and private peer signing keys do not match"
        case .OTErrorRecoveryCreation:
            return "failed to create Recovery"
        case .OTErrorAuthCipherTextCreation:
            return "failed to create authenticated ciphertext"
        case .OTErrorPrivateKeyCreation:
            return "failed to create private key"
        case .OTErrorRecoveryKeyCreation:
            return "failed to create recovery keys"
        case .OTErrorEntropyCreation:
            return "failed to create entropy"
        case .OTErrorEntropyKeyMismatch:
            return "keys generated by the entropy+salt do not match the Recovery contents"
        }
    }
}
