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

private let logger = Logger(subsystem: "com.apple.security.trustedpeers", category: "bottledpeer")

class BottledPeer: NSObject {
    var escrowKeys: EscrowKeys
    var secret: Data
    var peerID: String
    var bottleID: String
    var peerKeys: OctagonSelfPeerKeys

    var signatureUsingEscrowKey: Data
    var signatureUsingPeerKey: Data
    var escrowSigningPublicKey: Data
    var escrowSigningSPKI: Data
    var peersigningSPKI: Data

    var contents: Data

    class func encryptionOperation() -> (_SFAuthenticatedEncryptionOperation) {
        let keySpecifier = _SFAESKeySpecifier.init(bitSize: TPHObjectiveC.aes256BitSize())
        return _SFAuthenticatedEncryptionOperation.init(keySpecifier: keySpecifier)
    }

    // Given a peer's details including private key material, and
    // the keys generated from the escrow secret, encrypt the peer private keys,
    // make a bottled peer object and serialize it into data.
    init (peerID: String, bottleID: String, peerSigningKey: _SFECKeyPair, peerEncryptionKey: _SFECKeyPair, bottleSalt: String) throws {
        let secret = try BottledPeer.makeMeSomeEntropy(requiredLength: Int(OTMasterSecretLength))

        self.secret = secret
        self.escrowKeys = try EscrowKeys(secret: secret, bottleSalt: bottleSalt)

        // Serialize the peer private keys into "contents"
        guard let contentsObj = OTBottleContents() else {
            throw Error.bottleCreation
        }
        guard let signingPK = OTPrivateKey() else {
            throw Error.privateKeyCreation
        }
        signingPK.keyType = OTPrivateKey_KeyType.EC_NIST_CURVES
        signingPK.keyData = peerSigningKey.keyData

        guard let encryptionPK = OTPrivateKey() else {
            throw Error.privateKeyCreation
        }
        encryptionPK.keyType = OTPrivateKey_KeyType.EC_NIST_CURVES
        encryptionPK.keyData = peerEncryptionKey.keyData

        contentsObj.peerSigningPrivKey = signingPK
        contentsObj.peerEncryptionPrivKey = encryptionPK
        guard let clearContentsData = contentsObj.data else {
            throw Error.bottleCreation
        }

        // Encrypt the contents
        let op = BottledPeer.encryptionOperation()
        let cipher = try op.encrypt(clearContentsData, with: escrowKeys.symmetricKey)
        guard let cipher = cipher as? _SFAuthenticatedCiphertext else {
            throw Error.authCipherTextCreation
        }

        let escrowSigningECPubKey: _SFECPublicKey = (escrowKeys.signingKey.publicKey as! _SFECPublicKey)
        let escrowEncryptionECPubKey: _SFECPublicKey = escrowKeys.encryptionKey.publicKey as! _SFECPublicKey

        let peerSigningECPublicKey: _SFECPublicKey = peerSigningKey.publicKey as! _SFECPublicKey
        let peerEncryptionECPublicKey: _SFECPublicKey = peerEncryptionKey.publicKey as! _SFECPublicKey

        // Serialize the whole thing
        guard let obj = OTBottle() else {
            throw Error.bottleCreation
        }
        obj.peerID = peerID
        obj.bottleID = bottleID
        obj.escrowedSigningSPKI = escrowSigningECPubKey.encodeSubjectPublicKeyInfo()
        obj.escrowedEncryptionSPKI = escrowEncryptionECPubKey.encodeSubjectPublicKeyInfo()
        obj.peerSigningSPKI = peerSigningECPublicKey.encodeSubjectPublicKeyInfo()
        obj.peerEncryptionSPKI = peerEncryptionECPublicKey.encodeSubjectPublicKeyInfo()

        guard let authObj = OTAuthenticatedCiphertext() else {
            throw Error.authCipherTextCreation
        }
        authObj.ciphertext = cipher.ciphertext
        authObj.authenticationCode = cipher.authenticationCode
        authObj.initializationVector = cipher.initializationVector

        obj.contents = authObj

        self.peerID = peerID
        self.bottleID = bottleID

        try self.peerKeys = OctagonSelfPeerKeys(peerID: peerID, signingKey: peerSigningKey, encryptionKey: peerEncryptionKey)
        self.contents = obj.data

        let escrowedSigningECPublicKey = escrowKeys.signingKey.publicKey as! _SFECPublicKey

        self.escrowSigningPublicKey = escrowedSigningECPublicKey.keyData
        self.escrowSigningSPKI = escrowedSigningECPublicKey.encodeSubjectPublicKeyInfo()
        self.peersigningSPKI = peerSigningECPublicKey.encodeSubjectPublicKeyInfo()

        let xso = BottledPeer.signingOperation()

        let signedByEscrowKey = try xso.sign(self.contents, with: escrowKeys.signingKey)
        self.signatureUsingEscrowKey = signedByEscrowKey.signature

        let signedByPeerKey = try xso.sign(self.contents, with: peerSigningKey)
        self.signatureUsingPeerKey = signedByPeerKey.signature
    }

    // Deserialize a bottle (data) and decrypt the contents (peer keys)
    // using the keys generated from the escrow secret, and signatures from signing keys
    init (contents: Data, secret: Data, bottleSalt: String, signatureUsingEscrow: Data, signatureUsingPeerKey: Data) throws {
        self.secret = secret
        self.escrowKeys = try EscrowKeys(secret: self.secret, bottleSalt: bottleSalt)

        guard let escrowSigningECKey: _SFECPublicKey = escrowKeys.signingKey.publicKey() as? _SFECPublicKey else {
            logger.info("escrow key not an SFECPublicKey?")
            throw Error.bottleCreation
        }
        self.escrowSigningSPKI = escrowSigningECKey.encodeSubjectPublicKeyInfo()

        // Deserialize the whole thing
        guard let obj = OTBottle(data: contents) else {
            logger.info("Unable to deserialize bottle")
            throw Error.deserializationFailure
        }

        // First, the easy check: did the entropy create the keys that are supposed to be in the bottle?
        guard obj.escrowedSigningSPKI == self.escrowSigningSPKI else {
            logger.info("Bottled SPKI does not match re-created SPKI")
            throw Error.entropyKeyMismatch
        }

        // Second, does the signature verify on the given data?
        let xso = BottledPeer.signingOperation()

        let escrowSigned = _SFSignedData.init(data: contents, signature: signatureUsingEscrow)
        try xso.verify(escrowSigned, with: escrowSigningECKey)

        // Now, decrypt contents
        let op = BottledPeer.encryptionOperation()
        let ac: OTAuthenticatedCiphertext = obj.contents as OTAuthenticatedCiphertext

        let ciphertext = _SFAuthenticatedCiphertext.init(ciphertext: ac.ciphertext, authenticationCode: ac.authenticationCode, initializationVector: ac.initializationVector)

        let clearContentsData = try op.decrypt(ciphertext, with: escrowKeys.symmetricKey)
        if clearContentsData.isEmpty {
            throw Error.decryptionFailure
        }

        // Deserialize contents into private peer keys
        guard let contentsObj = OTBottleContents(data: clearContentsData) else {
            throw Error.deserializationFailure
        }

        self.peerID = obj.peerID
        self.bottleID = obj.bottleID

        try self.peerKeys = OctagonSelfPeerKeys(peerID: peerID,
                                                signingKey: try contentsObj.peerSigningPrivKey.asECKeyPair(),
                                                encryptionKey: try contentsObj.peerEncryptionPrivKey.asECKeyPair())
        self.contents = contents

        self.peersigningSPKI = obj.peerSigningSPKI

        let peerSigningPubKey = _SFECPublicKey(subjectPublicKeyInfo: obj.peerSigningSPKI)
        let peerEncryptionPubKey = _SFECPublicKey(subjectPublicKeyInfo: obj.peerEncryptionSPKI)

        // Check the private keys match the public keys
        if self.peerKeys.signingKey.publicKey != peerSigningPubKey {
            throw Error.keyMismatch
        }
        if self.peerKeys.encryptionKey.publicKey != peerEncryptionPubKey {
            throw Error.keyMismatch
        }

        self.escrowSigningSPKI = escrowSigningECKey.encodeSubjectPublicKeyInfo()

        self.signatureUsingPeerKey = signatureUsingPeerKey
        self.signatureUsingEscrowKey = signatureUsingEscrow

        self.escrowSigningPublicKey = escrowSigningECKey.keyData

        let peerSigned = _SFSignedData.init(data: self.contents, signature: signatureUsingPeerKey)
        guard let peerPublicKey = self.peerKeys.publicSigningKey else {
            throw Error.keyMismatch
        }
        try xso.verify(peerSigned, with: peerPublicKey)
    }

    func escrowSigningPublicKeyHash() -> String {
        return TPHObjectiveC.digest(usingSha384: self.escrowSigningPublicKey)
    }

    class func signingOperation() -> (_SFEC_X962SigningOperation) {
        let keySpecifier = _SFECKeySpecifier.init(curve: SFEllipticCurve.nistp384)
        let digestOperation = _SFSHA384DigestOperation.init()
        return _SFEC_X962SigningOperation.init(keySpecifier: keySpecifier, digestOperation: digestOperation)
    }

    class func verifyBottleSignature(data: Data, signature: Data, pubKey: _SFECPublicKey) throws -> (Bool) {
        let xso = BottledPeer.signingOperation()
        let peerSigned = _SFSignedData.init(data: data, signature: signature)
        try xso.verify(peerSigned, with: pubKey)
        return true
    }

    class func makeMeSomeEntropy(requiredLength: Int) throws -> Data {
        var bytes = Data(count: requiredLength)
        try bytes.withUnsafeMutableBytes { (bufferPointer: UnsafeMutableRawBufferPointer) throws -> Void in
            guard let bytes = bufferPointer.baseAddress else {
                throw Error.entropyCreation
            }
            if SecRandomCopyBytes(kSecRandomDefault, bufferPointer.count, bytes) != 0 {
                throw Error.entropyCreation
            }
        }
        return bytes
    }
}

extension BottledPeer {
    enum Error: Swift.Error {
        case deserializationFailure
        case decryptionFailure
        case keyMismatch
        case bottleCreation
        case authCipherTextCreation
        case privateKeyCreation
        case escrowKeyCreation
        case entropyCreation
        case entropyKeyMismatch
    }
}

extension BottledPeer.Error: CustomNSError {
    public static var errorDomain: String {
        return "TrustedPeersHelper.BottledPeer.Error"
    }

    public var errorCode: Int {
        switch self {
        case .deserializationFailure:
            return 0
        case .decryptionFailure:
            return 1
        // reserved 2
        case .keyMismatch:
            return 3
        case .bottleCreation:
            return 4
        case .authCipherTextCreation:
            return 5
        case .privateKeyCreation:
            return 6
        case .escrowKeyCreation:
            return 7
        case .entropyCreation:
            return 8
        case .entropyKeyMismatch:
            return 9
        }
    }

    public var errorDescription: String? {
        switch self {
        case .deserializationFailure:
            return "Failed to deserialize bottle peer"
        case .decryptionFailure:
            return "could not decrypt bottle contents"
        case .keyMismatch:
            return "public and private peer signing keys do not match"
        case .bottleCreation:
            return "failed to create bottle"
        case .authCipherTextCreation:
            return "failed to create authenticated ciphertext"
        case .privateKeyCreation:
            return "failed to create private key"
        case .escrowKeyCreation:
            return "failed to create escrow keys"
        case .entropyCreation:
            return "failed to create entropy"
        case .entropyKeyMismatch:
            return "keys generated by the entropy+salt do not match the bottle contents"
        }
    }

    public var errorUserInfo: [String: Any] {
        var ret = [String: Any]()
        if let desc = self.errorDescription {
            ret[NSLocalizedDescriptionKey] = desc
        }
        return ret
    }
}
