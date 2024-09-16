import Foundation

extension ViewKey {
    static func convert(ckksKey: CKKSKeychainBackedKey) -> ViewKey {
        let kc: ViewKeyClass
        switch ckksKey.keyclass {
        case SecCKKSKeyClassTLK:
            kc = .tlk
        case SecCKKSKeyClassA:
            kc = .classA
        case SecCKKSKeyClassC:
            kc = .classC
        default:
            kc = .tlk
        }

        return ViewKey.with {
            $0.uuid = ckksKey.uuid
            $0.parentkeyUuid = ckksKey.parentKeyUUID
            $0.keyclass = kc
            $0.wrappedkeyBase64 = ckksKey.wrappedkey.base64WrappedKey()
            $0.uploadOsVersion = SecCKKSHostOSVersion()
        }
    }
}

// TODO: We need to support key rolling as well...
extension ViewKeys {
    static func convert(ckksKeySet: CKKSKeychainBackedKeySet) -> ViewKeys {
        return ViewKeys.with {
            $0.view = ckksKeySet.tlk.zoneID.zoneName
            $0.newTlk = ViewKey.convert(ckksKey: ckksKeySet.tlk)
            $0.newClassA = ViewKey.convert(ckksKey: ckksKeySet.classA)
            $0.newClassC = ViewKey.convert(ckksKey: ckksKeySet.classC)
        }
    }
}

extension TLKShare {
    static func convert(ckksTLKShare: CKKSTLKShare) -> TLKShare {
        return TLKShare.with {
            $0.view = ckksTLKShare.zoneID.zoneName
            $0.curve = Int64(ckksTLKShare.curve.rawValue)
            $0.epoch = Int64(ckksTLKShare.epoch)
            $0.keyUuid = ckksTLKShare.tlkUUID
            $0.poisoned = Int64(ckksTLKShare.poisoned)
            $0.receiver = ckksTLKShare.receiverPeerID
            $0.receiverPublicEncryptionKey = ckksTLKShare.receiverPublicEncryptionKeySPKI.base64EncodedString()
            $0.sender = ckksTLKShare.senderPeerID
            $0.signature = ckksTLKShare.signature?.base64EncodedString() ?? ""
            $0.version = Int64(ckksTLKShare.version.rawValue)
            $0.wrappedkey = ckksTLKShare.wrappedTLK?.base64EncodedString() ?? ""
        }
    }
}

extension FetchRecoverableTLKSharesResponse.View {
    func ckrecords() -> [CKRecord] {
        let records = [CKRecord(self.keys.tlk), CKRecord(self.keys.classA), CKRecord(self.keys.classC)] + self.tlkShares.map { CKRecord($0) }
        return records.compactMap { $0 }
    }
}

extension CurrentCKKSItem {
    func convert() -> CuttlefishCurrentItem {
        return CuttlefishCurrentItem(self.itemSpecifier.convert(), item: CKRecord(self.item)!)
    }
}

extension CurrentCKKSItemSpecifier {
    func convert() -> CuttlefishCurrentItemSpecifier {
        return CuttlefishCurrentItemSpecifier(self.itemPointerName, zoneID: self.zone)
    }
}

extension PCSService {
    func convert() -> CuttlefishPCSServiceIdentifier {
        return CuttlefishPCSServiceIdentifier(self.serviceIdentifier as NSNumber, pcsPublicKey: self.publicKey, zoneID: self.zone)
    }
}

extension DirectPCSIdentity {
    func convert() -> CuttlefishPCSIdentity {
        return CuttlefishPCSIdentity(self.pcsService.convert(), item: CKRecord(self.item)!)
    }
}
