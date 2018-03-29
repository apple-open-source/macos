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

#import "KeychainXCTest.h"
#import "SecDbKeychainItem.h"
#import "SecdTestKeychainUtilities.h"
#import "CKKS.h"
#import "SecDbKeychainItemV7.h"
#import "SecItemPriv.h"
#import "SecItemServer.h"
#import "spi.h"
#import "SecDbKeychainSerializedItemV7.h"
#import "SecDbKeychainSerializedMetadata.h"
#import "SecDbKeychainSerializedSecretData.h"
#import "SecDbKeychainSerializedAKSWrappedKey.h"
#import <utilities/SecCFWrappers.h>
#import <SecurityFoundation/SFEncryptionOperation.h>
#import <SecurityFoundation/SFCryptoServicesErrors.h>
#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>

#if USE_KEYSTORE

@interface SecDbKeychainItemV7 ()

+ (SFAESKeySpecifier*)keySpecifier;

@end

@implementation KeychainXCTest

+ (void)setUp
{
    [super setUp];
    
    SecCKKSDisable();
    securityd_init(NULL);
}

- (void)setUp
{
    [super setUp];
    
    self.lockState = LockStateUnlocked;
    self.allowDecryption = true;
    self.didAKSDecrypt = NO;
    self.simulateRolledAKSKey = NO;
    

    self.keyclassUsedForAKSDecryption = 0;
    
    self.keySpecifier = [[SFAESKeySpecifier alloc] initWithBitSize:SFAESKeyBitSize256];
    [self setNewFakeAKSKey:[NSData dataWithBytes:"1234567890123456789012" length:32]];
    
    [SecDbKeychainMetadataKeyStore resetSharedStore];
    
    self.mockSecDbKeychainItemV7 = OCMClassMock([SecDbKeychainItemV7 class]);
    [[[[self.mockSecDbKeychainItemV7 stub] andCall:@selector(fakeAKSEncryptWithKeybag:keyclass:keyData:outKeyclass:wrappedKey:error:) onObject:self] ignoringNonObjectArgs] aksEncryptWithKeybag:0 keyclass:0 keyData:[OCMArg any] outKeyclass:NULL wrappedKey:[OCMArg any] error:NULL];
    [[[[self.mockSecDbKeychainItemV7 stub] andCall:@selector(fakeAKSDecryptWithKeybag:keyclass:wrappedKeyData:outKeyclass:unwrappedKey:error:) onObject:self] ignoringNonObjectArgs] aksDecryptWithKeybag:0 keyclass:0 wrappedKeyData:[OCMArg any] outKeyclass:NULL unwrappedKey:[OCMArg any] error:NULL];
    [[[self.mockSecDbKeychainItemV7 stub] andCall:@selector(decryptionOperation) onObject:self] decryptionOperation];
    [[[self.mockSecDbKeychainItemV7 stub] andCall:@selector(isKeychainUnlocked) onObject:self] isKeychainUnlocked];
    
    NSArray* partsOfName = [self.name componentsSeparatedByCharactersInSet:[NSCharacterSet characterSetWithCharactersInString:@" ]"]];
    secd_test_setup_temp_keychain([partsOfName[1] UTF8String], NULL);
}

- (void)tearDown
{
    [self.mockSecDbKeychainItemV7 stopMocking];
    [super tearDown];
}

- (bool)isKeychainUnlocked
{
    return self.lockState == LockStateUnlocked;
}

- (id)decryptionOperation
{
    return self.allowDecryption ? [[SFAuthenticatedEncryptionOperation alloc] initWithKeySpecifier:[SecDbKeychainItemV7 keySpecifier]] : nil;
}

- (bool)setNewFakeAKSKey:(NSData*)newKeyData
{
    NSError* error = nil;
    self.fakeAKSKey = [[SFAESKey alloc] initWithData:newKeyData specifier:self.keySpecifier error:&error];
    XCTAssertNil(error, "Should be no error making a fake AKS key");
    XCTAssertNotNil(self.fakeAKSKey, "Should have received a fake AKS key");
    return true;
}

- (bool)fakeAKSEncryptWithKeybag:(keybag_handle_t)keybag
                        keyclass:(keyclass_t)keyclass
                         keyData:(NSData*)keyData
                     outKeyclass:(keyclass_t*)outKeyclass
                      wrappedKey:(NSMutableData*)wrappedKey
                           error:(NSError**)error
{
    if (self.lockState == LockStateLockedAndDisallowAKS) {
        if (error) {
            *error = [NSError errorWithDomain:(__bridge NSString *)kSecErrorDomain code:errSecInteractionNotAllowed userInfo:NULL];
        }
        return false;
    }
    
    uint32_t keyLength = (uint32_t)keyData.length;
    const uint8_t* keyBytes = keyData.bytes;
    
    NSData* dataToEncrypt = [NSData dataWithBytes:keyBytes length:keyLength];
    NSError* localError = nil;
    
    SFAuthenticatedEncryptionOperation* encryptionOperation = [[SFAuthenticatedEncryptionOperation alloc] initWithKeySpecifier:self.keySpecifier];
    encryptionOperation.authenticationCodeLength = 8;
    SFAuthenticatedCiphertext* ciphertext = [encryptionOperation encrypt:dataToEncrypt withKey:self.fakeAKSKey error:&localError];
    
    if (error) {
        *error = localError;
    }
    
    if (ciphertext) {
        void* wrappedKeyMutableBytes = wrappedKey.mutableBytes;
        memcpy(wrappedKeyMutableBytes, ciphertext.ciphertext.bytes, 32);
        memcpy(wrappedKeyMutableBytes + 32, ciphertext.initializationVector.bytes, 32);
        memcpy(wrappedKeyMutableBytes + 64, ciphertext.authenticationCode.bytes, 8);
        
        if (self.simulateRolledAKSKey && outKeyclass) {
            *outKeyclass = keyclass | (key_class_last + 1);
        }
        
        return true;
    }
    else {
        return false;
    }
}

- (bool)fakeAKSDecryptWithKeybag:(keybag_handle_t)keybag
                        keyclass:(keyclass_t)keyclass
                  wrappedKeyData:(NSData*)wrappedKeyData
                     outKeyclass:(keyclass_t*)outKeyclass
                    unwrappedKey:(NSMutableData*)unwrappedKey
                           error:(NSError**)error
{
    if (self.lockState == LockStateLockedAndDisallowAKS) {
        if (error) {
            *error = [NSError errorWithDomain:(__bridge NSString *)kSecErrorDomain code:errSecInteractionNotAllowed userInfo:NULL];
        }
        return false;
    }
    
    if (self.simulateRolledAKSKey && keyclass < key_class_last) {
        // let's make decryption fail like it would if this were an old metadata key entry made with a generational AKS key, but we didn't store that info in the database
        return false;
    }
    
    const uint8_t* wrappedKeyBytes = wrappedKeyData.bytes;
    
    NSData* ciphertextData = [NSData dataWithBytes:wrappedKeyBytes length:32];
    NSData* ivData = [NSData dataWithBytes:wrappedKeyBytes + 32 length:32];
    NSData* authCodeData = [NSData dataWithBytes:wrappedKeyBytes + 64 length:8];
    SFAuthenticatedCiphertext* ciphertext = [[SFAuthenticatedCiphertext alloc] initWithCiphertext:ciphertextData authenticationCode:authCodeData initializationVector:ivData];
    
    NSError* localError = nil;
    
    SFAuthenticatedEncryptionOperation* encryptionOperation = [[SFAuthenticatedEncryptionOperation alloc] initWithKeySpecifier:self.keySpecifier];
    encryptionOperation.authenticationCodeLength = 8;
    NSData* decryptedData = [encryptionOperation decrypt:ciphertext withKey:self.fakeAKSKey error:&localError];

    // in real securityd, we go through AKS rather than SFCryptoServices
    // we need to translate the error for proper handling
    if ([localError.domain isEqualToString:SFCryptoServicesErrorDomain] && localError.code == SFCryptoServicesErrorDecryptionFailed) {
        if (!self.simulateRolledAKSKey && keyclass > key_class_last) {
            // for this case we want to simulate what happens when we try decrypting with a rolled keyclass on a device which has never been rolled, which is it ends up with a NotPermitted error from AKS which the security layer translates as locked keybag
            localError = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecInteractionNotAllowed userInfo:nil];
        }
        else {
            localError = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecDecode userInfo:nil];
        }
    }
    
    if (error) {
        *error = localError;
    }
    
    self.keyclassUsedForAKSDecryption = keyclass;
    if (decryptedData && decryptedData.length <= unwrappedKey.length) {
        memcpy(unwrappedKey.mutableBytes, decryptedData.bytes, decryptedData.length);
        unwrappedKey.length = decryptedData.length;
        self.didAKSDecrypt = YES;
        return true;
    }
    else {
        return false;
    }
}

@end

#endif
