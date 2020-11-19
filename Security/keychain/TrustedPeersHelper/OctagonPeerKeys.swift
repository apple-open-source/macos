import Foundation

enum OctagonSelfPeerKeysError: Error {
    case noPublicKeys
}

class OctagonSelfPeerKeys: NSObject, CKKSSelfPeer {
    var encryptionKey: _SFECKeyPair
    var signingKey: _SFECKeyPair
    var peerID: String

    // Here for conformance with CKKSPeer
    var publicEncryptionKey: _SFECPublicKey?
    var publicSigningKey: _SFECPublicKey?

    var encryptionVerificationKey: _SFECPublicKey
    var signingVerificationKey: _SFECPublicKey

    func matchesPeer(_ peer: CKKSPeer) -> Bool {
        return false
    }

    init(peerID: String, signingKey: _SFECKeyPair, encryptionKey: _SFECKeyPair) throws {
        self.peerID = peerID
        self.signingKey = signingKey
        self.encryptionKey = encryptionKey

        self.publicEncryptionKey = encryptionKey.publicKey as? _SFECPublicKey
        self.publicSigningKey = signingKey.publicKey as? _SFECPublicKey

        guard let encryptionVerificationKey = self.publicEncryptionKey,
            let signingVerificationKey = self.publicSigningKey else {
                throw OctagonSelfPeerKeysError.noPublicKeys
        }

        self.encryptionVerificationKey = encryptionVerificationKey
        self.signingVerificationKey = signingVerificationKey
    }

    override var description: String {
        return "<OctagonSelfPeerKeys: \(self.peerID)>"
    }
}

extension TPPeerPermanentInfo: CKKSPeer {
    public var publicEncryptionKey: _SFECPublicKey? {
        return self.encryptionPubKey as? _SFECPublicKey
    }

    public var publicSigningKey: _SFECPublicKey? {
        return self.signingPubKey as? _SFECPublicKey
    }

    public func matchesPeer(_ peer: CKKSPeer) -> Bool {
        return self.peerID == peer.peerID
    }
}
