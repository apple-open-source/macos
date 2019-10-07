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

let OT_RECOVERY_SIGNING_HKDF_SIZE = 56
let OT_RECOVERY_ENCRYPTION_HKDF_SIZE = 56

enum recoveryKeyType: Int {
    case kOTRecoveryKeySigning = 1
    case kOTRecoveryKeyEncryption = 2
}

class RecoveryKeySet: NSObject {
    public var encryptionKey: _SFECKeyPair
    public var signingKey: _SFECKeyPair

    public var secret: Data
    public var recoverySalt: String

    public init (secret: Data, recoverySalt: String) throws {
        self.secret = secret
        self.recoverySalt = recoverySalt

        let encryptionKeyData = try RecoveryKeySet.generateRecoveryKey(keyType: recoveryKeyType.kOTRecoveryKeyEncryption, masterSecret: secret, recoverySalt: recoverySalt)
        self.encryptionKey = _SFECKeyPair.init(secKey: try RecoveryKeySet.createSecKey(keyData: encryptionKeyData))

        let signingKeyData = try RecoveryKeySet.generateRecoveryKey(keyType: recoveryKeyType.kOTRecoveryKeySigning, masterSecret: secret, recoverySalt: recoverySalt)
        self.signingKey = _SFECKeyPair.init(secKey: try RecoveryKeySet.createSecKey(keyData: signingKeyData))

        let RecoverySigningPubKeyHash = try RecoveryKeySet.hashRecoveryedSigningPublicKey(keyData: self.signingKey.publicKey().spki())
        _ = try RecoveryKeySet.storeRecoveryedSigningKeyPair(keyData: self.signingKey.keyData, label: RecoverySigningPubKeyHash)
        _ = try RecoveryKeySet.storeRecoveryedEncryptionKeyPair(keyData: self.encryptionKey.keyData, label: RecoverySigningPubKeyHash)
    }

    class func generateMasterKeyString() -> (String?) {
        return  SecRKCreateRecoveryKeyString(nil) as String
    }

    class func generateRecoveryKey(keyType: recoveryKeyType, masterSecret: Data, recoverySalt: String) throws -> (Data) {
        var keyLength: Int
        var info: Data
        var infoLength: Int
        var derivedKey: Data
        var finalKey = Data()

        switch keyType {
        case recoveryKeyType.kOTRecoveryKeyEncryption:
            keyLength = OT_RECOVERY_ENCRYPTION_HKDF_SIZE

            let infoString = Array("Recovery Encryption Private Key".utf8)
            info = Data(bytes: infoString, count: infoString.count)
            infoLength = info.count

            break
        case recoveryKeyType.kOTRecoveryKeySigning:
            keyLength = OT_RECOVERY_SIGNING_HKDF_SIZE

            let infoString = Array("Recovery Signing Private Key".utf8)
            info = Data(bytes: infoString, count: infoString.count)
            infoLength = info.count

            break
        }

        guard let cp = ccec_cp_384() else {
            throw RecoveryKeySetError.keyGeneration
        }
        var status: Int32 = 0

        let fullKey = TPHObjectiveC.ccec384Context()
        defer { TPHObjectiveC.contextFree(fullKey) }

        derivedKey = Data(count: keyLength)

        var masterSecretMutable = masterSecret
        let masterSecretLength = masterSecret.count
        let derivedKeySize = derivedKey.count

        let bottleSaltData = Data(bytes: Array(recoverySalt.utf8), count: recoverySalt.utf8.count)

        try derivedKey.withUnsafeMutableBytes { (derivedKeyBytes: UnsafeMutablePointer<UInt8>) throws ->Void in
            try masterSecretMutable.withUnsafeMutableBytes { (masterSecretBytes: UnsafeMutablePointer<UInt8>) throws ->Void in
                try bottleSaltData.withUnsafeBytes { (bottleSaltBytes: UnsafePointer<UInt8>) throws -> Void in
                    try info.withUnsafeBytes { (infoBytes: UnsafePointer<UInt8>) throws -> Void in
                        status = cchkdf(ccsha384_di(),
                                        masterSecretLength, masterSecretBytes,
                                        bottleSaltData.count, bottleSaltBytes,
                                        infoLength, infoBytes,
                                        keyLength, derivedKeyBytes)
                        if status != 0 {
                            throw RecoveryKeySetError.corecryptoKeyGeneration(corecryptoError: status)
                        }

                        if(keyType == recoveryKeyType.kOTRecoveryKeyEncryption || keyType == recoveryKeyType.kOTRecoveryKeySigning) {
                            status = ccec_generate_key_deterministic(cp,
                                                                     derivedKeySize, derivedKeyBytes,
                                                                     ccDRBGGetRngState(),
                                                                     UInt32(CCEC_GENKEY_DETERMINISTIC_FIPS),
                                                                     fullKey)

                            guard status == 0 else {
                                throw RecoveryKeySetError.corecryptoKeyGeneration(corecryptoError: status)
                            }

                            let space = ccec_x963_export_size(1, ccec_ctx_pub(fullKey))
                            var key = Data(count: space)
                            key.withUnsafeMutableBytes { (bytes: UnsafeMutablePointer<UInt8>) -> Void in
                                ccec_x963_export(1, bytes, fullKey)
                            }
                            finalKey = Data(key)
                        }
                    }
                }
            }
        }
        return finalKey
    }

    class func createSecKey(keyData: Data) throws -> (SecKey) {
        let keyAttributes = [kSecAttrKeyClass: kSecAttrKeyClassPrivate, kSecAttrKeyType: kSecAttrKeyTypeEC]

        guard let key = SecKeyCreateWithData(keyData as CFData, keyAttributes as CFDictionary, nil) else {
            throw RecoveryKeySetError.keyGeneration
        }

        return key
    }

    class func setKeyMaterialInKeychain(query: Dictionary<CFString, Any>) throws -> (Bool) {
        var result = false

        var results: CFTypeRef?
        var status = SecItemAdd(query as CFDictionary, &results)

        if status == errSecSuccess {
            result = true
        } else if status == errSecDuplicateItem {
            var updateQuery: Dictionary<CFString, Any> = query
            updateQuery[kSecClass] = nil

            status = SecItemUpdate(query as CFDictionary, updateQuery as CFDictionary)

            if status != errSecSuccess {
                throw RecoveryKeySetError.failedToSaveToKeychain(errorCode: status)
            } else {
                result = true
            }
        } else {
            throw RecoveryKeySetError.failedToSaveToKeychain(errorCode: status)
        }

        return result
    }

    class func hashRecoveryedSigningPublicKey(keyData: Data) throws -> (String) {
        let di = ccsha384_di()
        var result = Data(count: TPHObjectiveC.ccsha384_diSize())

        let derivedKeySize = keyData.count
        var keyDataMutable = keyData
        result.withUnsafeMutableBytes {(resultBytes: UnsafeMutablePointer<UInt8>) -> Void in
            keyDataMutable.withUnsafeMutableBytes {(keyDataBytes: UnsafeMutablePointer<UInt8>) -> Void in
                ccdigest(di, derivedKeySize, keyDataBytes, resultBytes)
            }
        }
        let hash = result.base64EncodedString(options: [])

        return hash
    }

    class func storeRecoveryedEncryptionKeyPair(keyData: Data, label: String) throws -> (Bool) {

        let query: [CFString: Any] = [
            kSecClass: kSecClassKey,
            kSecAttrAccessible: kSecAttrAccessibleWhenUnlocked,
            kSecUseDataProtectionKeychain: true,
            kSecAttrAccessGroup: "com.apple.security.octagon",
            kSecAttrSynchronizable: kCFBooleanFalse,
            kSecAttrLabel: label,
            kSecAttrApplicationLabel: String(format: "Recoveryed Encryption Key-%@", NSUUID().uuidString),
            kSecValueData: keyData,
            ]
        return try RecoveryKeySet.setKeyMaterialInKeychain(query: query)
    }

    class func storeRecoveryedSigningKeyPair(keyData: Data, label: String) throws -> (Bool) {
        let query: [CFString: Any] = [
            kSecClass: kSecClassKey,
            kSecAttrAccessible: kSecAttrAccessibleWhenUnlocked,
            kSecUseDataProtectionKeychain: true,
            kSecAttrAccessGroup: "com.apple.security.octagon",
            kSecAttrSynchronizable: kCFBooleanFalse,
            kSecAttrApplicationLabel: String(format: "Recoveryed Signing Key-%@", NSUUID().uuidString),
            kSecAttrLabel: label,
            kSecValueData: keyData,
            ]
        return try RecoveryKeySet.setKeyMaterialInKeychain(query: query)
    }

    class func retrieveRecoveryKeysFromKeychain(label: String) throws -> [Dictionary <CFString, Any>]? {
        var keySet: [Dictionary<CFString, Any>]?

        let query: [CFString: Any] = [
            kSecClass: kSecClassKey,
            kSecAttrAccessGroup: "com.apple.security.octagon",
            kSecAttrLabel: label,
            kSecReturnAttributes: true,
            kSecReturnData: true,
            kSecAttrSynchronizable: kCFBooleanFalse,
            kSecMatchLimit: kSecMatchLimitAll,
            ]

        var result: CFTypeRef?
        let status = SecItemCopyMatching(query as CFDictionary, &result)

        if status != errSecSuccess || result == nil {
            throw RecoveryKeySetError.itemDoesNotExist
        }

        if result != nil {
            if let dictionaryArray = result as? [Dictionary<CFString, Any>] {
                keySet = dictionaryArray
            } else {
                if let dictionary = result as? Dictionary<CFString, Any> {
                    keySet = [dictionary]
                } else {
                    keySet = nil
                }
            }
        }
        return keySet
    }

    class func findRecoveryKeysForLabel(label: String) throws -> (_SFECKeyPair?, _SFECKeyPair?) {
        var signingKey: _SFECKeyPair?
        var encryptionKey: _SFECKeyPair?

        let keySet = try retrieveRecoveryKeysFromKeychain(label: label)
        if keySet == nil {
            throw RecoveryKeySetError.itemDoesNotExist
        }
        for item in keySet! {
            let keyTypeData = item[kSecAttrApplicationLabel as CFString] as! Data
            let keyType = String(data: keyTypeData, encoding: .utf8)!

            if keyType.range(of: "Encryption") != nil {
                let keyData = item[kSecValueData as CFString] as! Data
                let encryptionSecKey = try RecoveryKeySet.createSecKey(keyData: keyData)
                encryptionKey = _SFECKeyPair.init(secKey: encryptionSecKey)
            } else if keyType.range(of: "Signing") != nil {
                let keyData = item[kSecValueData as CFString] as! Data
                let signingSecKey = try RecoveryKeySet.createSecKey(keyData: keyData)
                signingKey = _SFECKeyPair.init(secKey: signingSecKey)
            } else {
                throw RecoveryKeySetError.unsupportedKeyType(keyType: keyType)
            }
        }

        return (signingKey, encryptionKey)
    }
}

enum RecoveryKeySetError: Error {
    case keyGeneration
    case itemDoesNotExist
    case failedToSaveToKeychain(errorCode: OSStatus)
    case unsupportedKeyType(keyType: String)
    case corecryptoKeyGeneration(corecryptoError: Int32)
}

extension RecoveryKeySetError: LocalizedError {
    public var errorDescription: String? {
        switch self {
        case .keyGeneration:
            return "Key generation failed"
        case .itemDoesNotExist:
            return "Item does not exist"
        case .failedToSaveToKeychain(errorCode: let osError):
            return "Failed to save item to keychain: \(osError)"
        case .unsupportedKeyType(keyType: let keyType):
            return "Unsupported Key Type \(keyType)"
        case .corecryptoKeyGeneration(corecryptoError: let corecryptoError):
            return "Key generation crypto failed \(corecryptoError)"
        }
    }
}

extension RecoveryKeySetError: CustomNSError {

    public static var errorDomain: String {
        return "com.apple.security.trustedpeers.RecoveryKeySetError"
    }

    public var errorCode: Int {
        switch self {
        case .keyGeneration:
            return 1
        case .itemDoesNotExist:
            return 2
        case .failedToSaveToKeychain:
            return 3
        case .unsupportedKeyType:
            return 4
        case .corecryptoKeyGeneration:
            return 5
        }
    }

    public var errorUserInfo: [String: Any] {
        var userInfo: [String: Any] = [:]
        if let desc = self.errorDescription {
            userInfo[NSLocalizedDescriptionKey] = desc
        }
        switch self {
        case .failedToSaveToKeychain(errorCode: let osError):
            userInfo[NSUnderlyingErrorKey] = NSError.init(domain: NSOSStatusErrorDomain, code: Int(osError), userInfo: nil)
        case .corecryptoKeyGeneration(corecryptoError: let corecryptoError):
            userInfo[NSUnderlyingErrorKey] = NSError.init(domain: "corecrypto", code: Int(corecryptoError), userInfo: nil)
        default:
            break
        }
        return userInfo
    }
}
