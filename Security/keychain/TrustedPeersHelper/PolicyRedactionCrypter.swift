/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

class PolicyRedactionCrypter: NSObject, TPDecrypter, TPEncrypter {
    func decryptData(_ ciphertext: TPPBPolicyRedactionAuthenticatedCiphertext, withKey key: Data) throws -> Data {
        let operation = _SFAuthenticatedEncryptionOperation(keySpecifier: _SFAESKeySpecifier(bitSize: TPHObjectiveC.aes256BitSize()))
        let symmetricKey = try _SFAESKey(data: key, specifier: _SFAESKeySpecifier(bitSize: TPHObjectiveC.aes256BitSize()))

        let realCiphertext = _SFAuthenticatedCiphertext(ciphertext: ciphertext.ciphertext,
                                                        authenticationCode: ciphertext.authenticationCode,
                                                        initializationVector: ciphertext.initializationVector)

        let plaintext = try operation.decrypt(realCiphertext, with: symmetricKey)
        return plaintext
    }

    func encryptData(_ plaintext: Data, withKey key: Data) throws -> TPPBPolicyRedactionAuthenticatedCiphertext {
        let operation = _SFAuthenticatedEncryptionOperation(keySpecifier: _SFAESKeySpecifier(bitSize: TPHObjectiveC.aes256BitSize()))
        let symmetricKey = try _SFAESKey(data: key, specifier: _SFAESKeySpecifier(bitSize: TPHObjectiveC.aes256BitSize()))

        let ciphertext = try operation.encrypt(plaintext, with: symmetricKey, additionalAuthenticatedData: nil)

        let wrappedCiphertext = TPPBPolicyRedactionAuthenticatedCiphertext()!
        wrappedCiphertext.ciphertext = ciphertext.ciphertext
        wrappedCiphertext.authenticationCode = ciphertext.authenticationCode
        wrappedCiphertext.initializationVector = ciphertext.initializationVector

        return wrappedCiphertext
    }

    func randomKey() -> Data {
        var bytes = [Int8](repeating: 0, count: 256 / 8)
        guard errSecSuccess == SecRandomCopyBytes(kSecRandomDefault, 256 / 8, &bytes) else {
            abort()
        }

        return Data(bytes: bytes, count: 256 / 8)
    }
}
