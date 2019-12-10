/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#import "SecDbKeychainItemV7.h"
#import "SecKeybagSupport.h"
#import "SecItemServer.h"
#import "SecAccessControl.h"
#import "SecDbKeychainSerializedItemV7.h"
#import "SecDbKeychainSerializedAKSWrappedKey.h"
#import "SecDbKeychainSerializedMetadata.h"
#import "SecDbKeychainSerializedSecretData.h"
#import <dispatch/dispatch.h>
#import <utilities/SecAKSWrappers.h>
#import "SecAKSObjCWrappers.h"
#import <utilities/der_plist.h>
#import <SecurityFoundation/SFEncryptionOperation.h>
#import <SecurityFoundation/SFKey_Private.h>
#import <SecurityFoundation/SFCryptoServicesErrors.h>

#import <Foundation/NSKeyedArchiver_Private.h>

#if USE_KEYSTORE && __has_include(<Kernel/IOKit/crypto/AppleKeyStoreDefs.h>)
#import <Kernel/IOKit/crypto/AppleKeyStoreDefs.h>
#endif

#import "SecDbKeychainMetadataKeyStore.h"
#import "SecDbBackupManager.h"

#define KEYCHAIN_ITEM_PADDING_MODULUS 20

// See corresponding "reasonable size" client-side limit(s) in SecItem.

// Generally the secret data dictionary contains a single key
// with the client's password/key NSData therein, so 4k feels extremely luxurious
#define REASONABLE_SECRET_DATA_SIZE 4096

// This feels similarly generous, but let's find out
#define REASONABLE_METADATA_SIZE 2048

NSString* const SecDbKeychainErrorDomain = @"SecDbKeychainErrorDomain";
const NSInteger SecDbKeychainErrorDeserializationFailed = 1;

static NSString* const SecDBTamperCheck = @"TamperCheck";

static NSDictionary* dictionaryFromDERData(NSData* data)
{
    NSDictionary* dict = (__bridge_transfer NSDictionary*)CFPropertyListCreateWithDERData(NULL, (__bridge CFDataRef)data, 0, NULL, NULL);
    return [dict isKindOfClass:[NSDictionary class]] ? dict : nil;
}

typedef NS_ENUM(uint32_t, SecDbKeychainAKSWrappedKeyType) {
    SecDbKeychainAKSWrappedKeyTypeRegular,
    SecDbKeychainAKSWrappedKeyTypeRefKey
};

@interface SecDbKeychainAKSWrappedKey : NSObject

@property (readonly) NSData* wrappedKey;
@property (readonly) NSData* refKeyBlob;
@property (readonly) SecDbKeychainAKSWrappedKeyType type;

@property (readonly) NSData* serializedRepresentation;

- (instancetype)initWithData:(NSData*)data;
- (instancetype)initRegularWrappedKeyWithData:(NSData*)wrappedKey;
- (instancetype)initRefKeyWrappedKeyWithData:(NSData*)wrappedKey refKeyBlob:(NSData*)refKeyBlob;

@end

@interface SecDbKeychainMetadata : NSObject

@property (readonly) SFAuthenticatedCiphertext* ciphertext;
@property (readonly) SFAuthenticatedCiphertext* wrappedKey;
@property (readonly) NSString* tamperCheck;

@property (readonly) NSData* serializedRepresentation;

- (instancetype)initWithData:(NSData*)data;
- (instancetype)initWithCiphertext:(SFAuthenticatedCiphertext*)ciphertext wrappedKey:(SFAuthenticatedCiphertext*)wrappedKey tamperCheck:(NSString*)tamperCheck error:(NSError**)error;

@end

@interface SecDbKeychainSecretData : NSObject

@property (readonly) SFAuthenticatedCiphertext* ciphertext;
@property (readonly) SecDbKeychainAKSWrappedKey* wrappedKey;
@property (readonly) NSString* tamperCheck;

@property (readonly) NSData* serializedRepresentation;

- (instancetype)initWithData:(NSData*)data;
- (instancetype)initWithCiphertext:(SFAuthenticatedCiphertext*)ciphertext wrappedKey:(SecDbKeychainAKSWrappedKey*)wrappedKey tamperCheck:(NSString*)tamperCheck backupWrappedKey:(SecDbBackupWrappedItemKey*)backupWrappedKey error:(NSError**)error;

@end

@implementation SecDbKeychainAKSWrappedKey {
    SecDbKeychainSerializedAKSWrappedKey* _serializedHolder;
}

- (instancetype)initRegularWrappedKeyWithData:(NSData*)wrappedKey
{
    if (self = [super init]) {
        _serializedHolder = [[SecDbKeychainSerializedAKSWrappedKey alloc] init];
        _serializedHolder.wrappedKey = wrappedKey;
        _serializedHolder.type = SecDbKeychainAKSWrappedKeyTypeRegular;
    }

    return self;
}

- (instancetype)initRefKeyWrappedKeyWithData:(NSData*)wrappedKey refKeyBlob:(NSData*)refKeyBlob
{
    if (self = [super init]) {
        _serializedHolder = [[SecDbKeychainSerializedAKSWrappedKey alloc] init];
        _serializedHolder.wrappedKey = wrappedKey;
        _serializedHolder.refKeyBlob = refKeyBlob;
        _serializedHolder.type = SecDbKeychainAKSWrappedKeyTypeRefKey;
    }

    return self;
}

- (instancetype)initWithData:(NSData*)data
{
    if (self = [super init]) {
        _serializedHolder = [[SecDbKeychainSerializedAKSWrappedKey alloc] initWithData:data];
        if (!_serializedHolder.wrappedKey || (_serializedHolder.type == SecDbKeychainAKSWrappedKeyTypeRefKey && !_serializedHolder.refKeyBlob)) {
            self = nil;
        }
    }

    return self;
}

- (NSData*)serializedRepresentation
{
    return _serializedHolder.data;
}

- (NSData*)wrappedKey
{
    return _serializedHolder.wrappedKey;
}

- (NSData*)refKeyBlob
{
    return _serializedHolder.refKeyBlob;
}

- (SecDbKeychainAKSWrappedKeyType)type
{
    return _serializedHolder.type;
}

@end

@implementation SecDbKeychainMetadata {
    SecDbKeychainSerializedMetadata* _serializedHolder;
}

- (instancetype)initWithCiphertext:(SFAuthenticatedCiphertext*)ciphertext
                        wrappedKey:(SFAuthenticatedCiphertext*)wrappedKey
                       tamperCheck:(NSString*)tamperCheck
                             error:(NSError**)error
{
    if (self = [super init]) {
        _serializedHolder = [[SecDbKeychainSerializedMetadata alloc] init];
        _serializedHolder.ciphertext = [NSKeyedArchiver archivedDataWithRootObject:ciphertext requiringSecureCoding:YES error:error];
        _serializedHolder.wrappedKey = [NSKeyedArchiver archivedDataWithRootObject:wrappedKey requiringSecureCoding:YES error:error];
        _serializedHolder.tamperCheck = tamperCheck;
        if (!_serializedHolder.ciphertext || !_serializedHolder.wrappedKey || !_serializedHolder.tamperCheck) {
            self = nil;
        }
    }

    return self;
}

- (instancetype)initWithData:(NSData*)data
{
    if (self = [super init]) {
        _serializedHolder = [[SecDbKeychainSerializedMetadata alloc] initWithData:data];
        if (!_serializedHolder.ciphertext || !_serializedHolder.wrappedKey || !_serializedHolder.tamperCheck) {
            self = nil;
        }
    }

    return self;
}

- (NSData*)serializedRepresentation
{
    return _serializedHolder.data;
}

- (SFAuthenticatedCiphertext*)ciphertext
{
    NSError* error = nil;
    SFAuthenticatedCiphertext* ciphertext =  [NSKeyedUnarchiver unarchivedObjectOfClass:[SFAuthenticatedCiphertext class] fromData:_serializedHolder.ciphertext error:&error];
    if (!ciphertext) {
        secerror("SecDbKeychainItemV7: error deserializing ciphertext from metadata: %@", error);
    }

    return ciphertext;
}

- (SFAuthenticatedCiphertext*)wrappedKey
{
    NSError* error = nil;
    SFAuthenticatedCiphertext* wrappedKey =  [NSKeyedUnarchiver unarchivedObjectOfClass:[SFAuthenticatedCiphertext class] fromData:_serializedHolder.wrappedKey error:&error];
    if (!wrappedKey) {
        secerror("SecDbKeychainItemV7: error deserializing wrappedKey from metadata: %@", error);
    }

    return wrappedKey;
}

- (NSString*)tamperCheck
{
    return _serializedHolder.tamperCheck;
}

@end

@implementation SecDbKeychainSecretData {
    SecDbKeychainSerializedSecretData* _serializedHolder;
}

- (instancetype)initWithCiphertext:(SFAuthenticatedCiphertext*)ciphertext
                        wrappedKey:(SecDbKeychainAKSWrappedKey*)wrappedKey
                       tamperCheck:(NSString*)tamperCheck
                  backupWrappedKey:(SecDbBackupWrappedItemKey*)backupWrappedKey
                             error:(NSError**)error
{
    if (self = [super init]) {
        _serializedHolder = [[SecDbKeychainSerializedSecretData alloc] init];
        _serializedHolder.ciphertext = [NSKeyedArchiver archivedDataWithRootObject:ciphertext requiringSecureCoding:YES error:error];
        _serializedHolder.wrappedKey = wrappedKey.serializedRepresentation;
        _serializedHolder.tamperCheck = tamperCheck;
        _serializedHolder.secDbBackupWrappedItemKey = backupWrappedKey ? [NSKeyedArchiver archivedDataWithRootObject:backupWrappedKey requiringSecureCoding:YES error:error] : nil;
        if (!_serializedHolder.ciphertext || !_serializedHolder.wrappedKey || !_serializedHolder.tamperCheck) {
            self = nil;
        }
    }

    return self;
}

- (instancetype)initWithData:(NSData*)data
{
    if (self = [super init]) {
        _serializedHolder = [[SecDbKeychainSerializedSecretData alloc] initWithData:data];
        if (!_serializedHolder.ciphertext || !_serializedHolder.wrappedKey || !_serializedHolder.tamperCheck) {
            self = nil;
        }
    }

    return self;
}

- (NSData*)serializedRepresentation
{
    return _serializedHolder.data;
}

- (SFAuthenticatedCiphertext*)ciphertext
{
    NSError* error = nil;
    SFAuthenticatedCiphertext* ciphertext =  [NSKeyedUnarchiver unarchivedObjectOfClass:[SFAuthenticatedCiphertext class] fromData:_serializedHolder.ciphertext error:&error];
    if (!ciphertext) {
        secerror("SecDbKeychainItemV7: error deserializing ciphertext from secret data: %@", error);
    }

    return ciphertext;
}

- (SecDbKeychainAKSWrappedKey*)wrappedKey
{
    return [[SecDbKeychainAKSWrappedKey alloc] initWithData:_serializedHolder.wrappedKey];
}

- (NSString*)tamperCheck
{
    return _serializedHolder.tamperCheck;
}

@end

@interface SecDbKeychainItemV7 ()
@property (nonatomic) NSData* backupUUID;
@end;

@implementation SecDbKeychainItemV7 {
    SecDbKeychainSecretData* _encryptedSecretData;
    SecDbKeychainMetadata* _encryptedMetadata;
    NSDictionary* _secretAttributes;
    NSDictionary* _metadataAttributes;
    NSString* _tamperCheck;
    keyclass_t _keyclass;
    keybag_handle_t _keybag;
}

@synthesize keyclass = _keyclass;

// bring back with <rdar://problem/37523001>
#if 0
+ (bool)isKeychainUnlocked
{
    return kc_is_unlocked();
}
#endif

- (instancetype)initWithData:(NSData*)data decryptionKeybag:(keybag_handle_t)decryptionKeybag error:(NSError**)error
{
    if (self = [super init]) {
        SecDbKeychainSerializedItemV7* serializedItem = [[SecDbKeychainSerializedItemV7 alloc] initWithData:data];
        if (serializedItem) {

            // Add 10% for serializing overhead. We're trying to catch blatant overstuffing, not enforce hard limits
            if (data.length > ((REASONABLE_SECRET_DATA_SIZE + REASONABLE_METADATA_SIZE) * 1.1)) {
                secwarning("SecDbKeychainItemV7: serialized item exceeds reasonable size (%lu bytes)", (unsigned long)data.length);
            }

            _keybag = decryptionKeybag;
            _encryptedSecretData = [[SecDbKeychainSecretData alloc] initWithData:serializedItem.encryptedSecretData];
            _encryptedMetadata = [[SecDbKeychainMetadata alloc] initWithData:serializedItem.encryptedMetadata];
            _keyclass = serializedItem.keyclass;
            if (![_encryptedSecretData.tamperCheck isEqualToString:_encryptedMetadata.tamperCheck]) {
                self = nil;
            }
        }
        else {
            self = nil;
        }
    }

    if (!self && error) {
        *error = [NSError errorWithDomain:(id)kCFErrorDomainOSStatus code:errSecDecode userInfo:@{NSLocalizedDescriptionKey : @"failed to deserialize keychain item blob"}];
    }

    return self;
}

- (instancetype)initWithSecretAttributes:(NSDictionary*)secretAttributes metadataAttributes:(NSDictionary*)metadataAttributes tamperCheck:(NSString*)tamperCheck keyclass:(keyclass_t)keyclass
{
    NSParameterAssert(tamperCheck);

    if (self = [super init]) {
        _secretAttributes = secretAttributes ? secretAttributes.copy : [NSDictionary dictionary];
        _metadataAttributes = metadataAttributes ? metadataAttributes.copy : [NSDictionary dictionary];
        _tamperCheck = tamperCheck.copy;
        _keyclass = keyclass;
    }

    return self;
}

+ (SFAESKeySpecifier*)keySpecifier
{
    static SFAESKeySpecifier* keySpecifier = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        keySpecifier = [[SFAESKeySpecifier alloc] initWithBitSize:SFAESKeyBitSize256];
    });

    return keySpecifier;
}

+ (SFAuthenticatedEncryptionOperation*)encryptionOperation
{
    static SFAuthenticatedEncryptionOperation* encryptionOperation = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        encryptionOperation = [[SFAuthenticatedEncryptionOperation alloc] initWithKeySpecifier:[self keySpecifier]];
    });

    return encryptionOperation;
}

+ (SFAuthenticatedEncryptionOperation*)decryptionOperation
{
    static SFAuthenticatedEncryptionOperation* decryptionOperation = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        decryptionOperation = [[SFAuthenticatedEncryptionOperation alloc] initWithKeySpecifier:[self keySpecifier]];
    });

    return decryptionOperation;
}

- (NSDictionary*)metadataAttributesWithError:(NSError**)error
{
    if (!_metadataAttributes) {
        SFAESKey* metadataClassKey = [self metadataClassKeyWithKeybag:_keybag
                                                   createKeyIfMissing:false
                                                  overwriteCorruptKey:false
                                                                error:error];
        if (metadataClassKey) {
            NSError* localError = nil;
            NSData* keyData = [[self.class decryptionOperation] decrypt:_encryptedMetadata.wrappedKey withKey:metadataClassKey error:&localError];
            if (!keyData) {
                secerror("SecDbKeychainItemV7: error unwrapping item metadata key (class %d, bag %d): %@", (int)self.keyclass, _keybag, localError);
                // TODO: track this in LocalKeychainAnalytics
                if (error) {
                    CFErrorRef secError = (CFErrorRef)CFBridgingRetain(localError); // this makes localError become the underlying error
                    SecError(errSecDecode, &secError, CFSTR("failed to unwrap item metadata key"));
                    *error = CFBridgingRelease(secError);
                }
                return nil;
            }
            SFAESKey* key = [[SFAESKey alloc] initWithData:keyData specifier:[self.class keySpecifier] error:error];
            if (!key) {
                return nil;
            }

            NSData* metadata = [[self.class decryptionOperation] decrypt:_encryptedMetadata.ciphertext withKey:key error:&localError];
            if (!metadata) {
                secerror("SecDbKeychainItemV7: error decrypting metadata content: %@", localError);
                if (error) {
                    CFErrorRef secError = (CFErrorRef)CFBridgingRetain(localError); // this makes localError become the underlying error
                    SecError(errSecDecode, &secError, CFSTR("failed to decrypt item metadata contents"));
                    *error = CFBridgingRelease(secError);
                }
                return nil;
            }
            NSMutableDictionary* decryptedAttributes = dictionaryFromDERData(metadata).mutableCopy;
            NSString* tamperCheck = decryptedAttributes[SecDBTamperCheck];
            if ([tamperCheck isEqualToString:_encryptedMetadata.tamperCheck]) {
                [decryptedAttributes removeObjectForKey:SecDBTamperCheck];
                _metadataAttributes = decryptedAttributes;
            }
            else {
                secerror("SecDbKeychainItemV7: tamper check failed for metadata decryption, expected %@ found %@", tamperCheck, _encryptedMetadata.tamperCheck);
                if (error) {
                    CFErrorRef secError = NULL;
                    SecError(errSecDecode, &secError, CFSTR("tamper check failed for metadata decryption"));
                    *error = CFBridgingRelease(secError);
                }
            }
        }
    }

    return _metadataAttributes;
}

- (NSDictionary*)secretAttributesWithAcmContext:(NSData*)acmContext accessControl:(SecAccessControlRef)accessControl callerAccessGroups:(NSArray*)callerAccessGroups error:(NSError**)error
{
    if (!_secretAttributes) {
        SFAESKey* key = [self unwrapFromAKS:_encryptedSecretData.wrappedKey accessControl:accessControl acmContext:acmContext callerAccessGroups:callerAccessGroups delete:NO error:error];
        if (key) {
            NSError* localError = nil;
            NSData* secretDataWithPadding = [[self.class decryptionOperation] decrypt:_encryptedSecretData.ciphertext withKey:key error:&localError];
            if (!secretDataWithPadding) {
                secerror("SecDbKeychainItemV7: error decrypting item secret data contents: %@", localError);
                if (error) {
                    CFErrorRef secError = (CFErrorRef)CFBridgingRetain(localError); // this makes localError become the underlying error
                    SecError(errSecDecode, &secError, CFSTR("error decrypting item secret data contents"));
                    *error = CFBridgingRelease(secError);
                }
                return nil;
            }
            int8_t paddingLength = *((int8_t*)secretDataWithPadding.bytes + secretDataWithPadding.length - 1);
            NSData* secretDataWithoutPadding = [secretDataWithPadding subdataWithRange:NSMakeRange(0, secretDataWithPadding.length - paddingLength)];

            NSMutableDictionary* decryptedAttributes = dictionaryFromDERData(secretDataWithoutPadding).mutableCopy;
            NSString* tamperCheck = decryptedAttributes[SecDBTamperCheck];
            if ([tamperCheck isEqualToString:_encryptedSecretData.tamperCheck]) {
                [decryptedAttributes removeObjectForKey:SecDBTamperCheck];
                _secretAttributes = decryptedAttributes;
            }
            else {
                secerror("SecDbKeychainItemV7: tamper check failed for secret data decryption, expected %@ found %@", tamperCheck, _encryptedMetadata.tamperCheck);
            }
        }
    }

    return _secretAttributes;
}

- (BOOL)deleteWithAcmContext:(NSData*)acmContext accessControl:(SecAccessControlRef)accessControl callerAccessGroups:(NSArray*)callerAccessGroups error:(NSError**)error
{
    NSError* localError = nil;
    (void)[self unwrapFromAKS:_encryptedSecretData.wrappedKey accessControl:accessControl acmContext:acmContext callerAccessGroups:callerAccessGroups delete:YES error:&localError];
    if (localError) {
        secerror("SecDbKeychainItemV7: failed to delete item secret key from aks");
        if (error) {
            *error = localError;
        }

        return NO;
    }

    return YES;
}

- (NSData*)encryptedBlobWithKeybag:(keybag_handle_t)keybag accessControl:(SecAccessControlRef)accessControl acmContext:(NSData*)acmContext error:(NSError**)error
{
    NSError* localError = nil;
    BOOL success = [self encryptMetadataWithKeybag:keybag error:&localError];
    if (!success || !_encryptedMetadata || localError) {
        if (error) {
            *error = localError;
        }
        return nil;
    }

    success = [self encryptSecretDataWithKeybag:keybag accessControl:accessControl acmContext:acmContext error:&localError];
    if (!success || !_encryptedSecretData || localError) {
        if (error) {
            *error = localError;
        }
        return nil;
    }

    SecDbKeychainSerializedItemV7* serializedItem = [[SecDbKeychainSerializedItemV7 alloc] init];
    serializedItem.encryptedMetadata = self.encryptedMetadataBlob;
    serializedItem.encryptedSecretData = self.encryptedSecretDataBlob;
    serializedItem.keyclass = _keyclass;
    return serializedItem.data;
}

- (NSData*)encryptedMetadataBlob
{
    return _encryptedMetadata.serializedRepresentation;
}

- (NSData*)encryptedSecretDataBlob
{
    return _encryptedSecretData.serializedRepresentation;
}

- (BOOL)encryptMetadataWithKeybag:(keybag_handle_t)keybag error:(NSError**)error
{
    SFAESKey* key = [[SFAESKey alloc] initRandomKeyWithSpecifier:[self.class keySpecifier] error:error];
    if (!key) {
        return NO;
    }
    SFAuthenticatedEncryptionOperation* encryptionOperation = [self.class encryptionOperation];

    NSMutableDictionary* attributesToEncrypt = _metadataAttributes.mutableCopy;
    attributesToEncrypt[SecDBTamperCheck] = _tamperCheck;
    NSData* metadata = (__bridge_transfer NSData*)CFPropertyListCreateDERData(NULL, (__bridge CFDictionaryRef)attributesToEncrypt, NULL);

    if (metadata.length > REASONABLE_METADATA_SIZE) {
        NSString *agrp = _metadataAttributes[(__bridge NSString *)kSecAttrAccessGroup];
        secwarning("SecDbKeychainItemV7: item's metadata exceeds reasonable size (%lu bytes) (%@)", (unsigned long)metadata.length, agrp);
    }

    SFAuthenticatedCiphertext* ciphertext = [encryptionOperation encrypt:metadata withKey:key error:error];

    SFAESKey* metadataClassKey = [self metadataClassKeyWithKeybag:keybag
                                               createKeyIfMissing:true
                                              overwriteCorruptKey:true
                                                            error:error];
    if (metadataClassKey) {
        SFAuthenticatedCiphertext* wrappedKey = [encryptionOperation encrypt:key.keyData withKey:metadataClassKey error:error];
        _encryptedMetadata = [[SecDbKeychainMetadata alloc] initWithCiphertext:ciphertext wrappedKey:wrappedKey tamperCheck:_tamperCheck error:error];
    }

    return _encryptedMetadata != nil;
}

- (BOOL)encryptSecretDataWithKeybag:(keybag_handle_t)keybag accessControl:(SecAccessControlRef)accessControl acmContext:(NSData*)acmContext error:(NSError**)error
{
    SFAESKey* key = [[SFAESKey alloc] initRandomKeyWithSpecifier:[self.class keySpecifier] error:error];
    if (!key) {
        return NO;
    }
    SFAuthenticatedEncryptionOperation* encryptionOperation = [self.class encryptionOperation];

    NSMutableDictionary* attributesToEncrypt = _secretAttributes.mutableCopy;
    attributesToEncrypt[SecDBTamperCheck] = _tamperCheck;
    NSMutableData* secretData = [(__bridge_transfer NSData*)CFPropertyListCreateDERData(NULL, (__bridge CFDictionaryRef)attributesToEncrypt, NULL) mutableCopy];

    if (secretData.length > REASONABLE_SECRET_DATA_SIZE) {
        NSString *agrp = _metadataAttributes[(__bridge NSString *)kSecAttrAccessGroup];
        secwarning("SecDbKeychainItemV7: item's secret data exceeds reasonable size (%lu bytes) (%@)", (unsigned long)secretData.length, agrp);
    }

    int8_t paddingLength = KEYCHAIN_ITEM_PADDING_MODULUS - (secretData.length % KEYCHAIN_ITEM_PADDING_MODULUS);
    int8_t paddingBytes[KEYCHAIN_ITEM_PADDING_MODULUS];
    for (int i = 0; i < KEYCHAIN_ITEM_PADDING_MODULUS; i++) {
        paddingBytes[i] = paddingLength;
    }
    [secretData appendBytes:paddingBytes length:paddingLength];

    SFAuthenticatedCiphertext* ciphertext = [encryptionOperation encrypt:secretData withKey:key error:error];
    SecDbKeychainAKSWrappedKey* wrappedKey = [self wrapToAKS:key withKeybag:keybag accessControl:accessControl acmContext:acmContext error:error];

    SecDbBackupWrappedItemKey* backupWrappedKey;
    if (checkV12DevEnabled()) {
        backupWrappedKey = [[SecDbBackupManager manager] wrapItemKey:key forKeyclass:_keyclass error:error];
        if (backupWrappedKey) {
            _backupUUID = backupWrappedKey.baguuid;
        } else {
            secwarning("SecDbKeychainItemV7: backup manager didn't return wrapped key: %@", error ? *error : nil);
            if (error) {
                *error = nil;
            }
        }
    }

    _encryptedSecretData = [[SecDbKeychainSecretData alloc] initWithCiphertext:ciphertext
                                                                    wrappedKey:wrappedKey
                                                                   tamperCheck:_tamperCheck
                                                              backupWrappedKey:backupWrappedKey
                                                                         error:error];
    return _encryptedSecretData != nil;
}

- (SFAESKey*)metadataClassKeyWithKeybag:(keybag_handle_t)keybag
                     createKeyIfMissing:(bool)createIfMissing
                    overwriteCorruptKey:(bool)force
                                  error:(NSError**)error
{
    return [[SecDbKeychainMetadataKeyStore sharedStore] keyForKeyclass:_keyclass
                                                                keybag:keybag
                                                          keySpecifier:[self.class keySpecifier]
                                                    createKeyIfMissing:(bool)createIfMissing
                                                   overwriteCorruptKey:force
                                                                 error:error];
}

- (SecDbKeychainAKSWrappedKey*)wrapToAKS:(SFAESKey*)key withKeybag:(keybag_handle_t)keybag accessControl:(SecAccessControlRef)accessControl acmContext:(NSData*)acmContext error:(NSError**)error
{
    NSData* keyData = key.keyData;

#if USE_KEYSTORE
    NSDictionary* constraints = (__bridge NSDictionary*)SecAccessControlGetConstraints(accessControl);
    if (constraints) {
        aks_ref_key_t refKey = NULL;
        CFErrorRef cfError = NULL;
        NSData* authData = (__bridge_transfer NSData*)CFPropertyListCreateDERData(NULL, (__bridge CFDictionaryRef)@{(id)kAKSKeyAcl : constraints}, &cfError);

        if (!acmContext || !SecAccessControlIsBound(accessControl)) {
            secerror("SecDbKeychainItemV7: access control error");
            if (error) {
                CFDataRef accessControlData = SecAccessControlCopyData(accessControl);
                ks_access_control_needed_error(&cfError, accessControlData, SecAccessControlIsBound(accessControl) ? kAKSKeyOpEncrypt : CFSTR(""));
                CFReleaseNull(accessControlData);
            }

            BridgeCFErrorToNSErrorOut(error, cfError);
            return nil;
        }

        void* aksParams = NULL;
        size_t aksParamsLength = 0;
        aks_operation_optional_params(0, 0, authData.bytes, authData.length, acmContext.bytes, (int)acmContext.length, &aksParams, &aksParamsLength);

        int aksResult = aks_ref_key_create(keybag, _keyclass, key_type_sym, aksParams, aksParamsLength, &refKey);
        if (aksResult != 0) {
            CFDataRef accessControlData = SecAccessControlCopyData(accessControl);
            create_cferror_from_aks(aksResult, kAKSKeyOpEncrypt, keybag, _keyclass, accessControlData, (__bridge CFDataRef)acmContext, &cfError);
            CFReleaseNull(accessControlData);
            free(aksParams);
            BridgeCFErrorToNSErrorOut(error, cfError);
            return nil;
        }

        size_t wrappedKeySize = 0;
        void* wrappedKeyBytes = NULL;
        aksResult = aks_ref_key_encrypt(refKey, aksParams, aksParamsLength, keyData.bytes, keyData.length, &wrappedKeyBytes, &wrappedKeySize);
        if (aksResult != 0) {
            CFDataRef accessControlData = SecAccessControlCopyData(accessControl);
            create_cferror_from_aks(aksResult, kAKSKeyOpEncrypt, keybag, _keyclass, accessControlData, (__bridge CFDataRef)acmContext, &cfError);
            CFReleaseNull(accessControlData);
            free(aksParams);
            aks_ref_key_free(&refKey);
            BridgeCFErrorToNSErrorOut(error, cfError);
            return nil;
        }
        free(aksParams);

        BridgeCFErrorToNSErrorOut(error, cfError);

        NSData* wrappedKey = [[NSData alloc] initWithBytesNoCopy:wrappedKeyBytes length:wrappedKeySize];

        size_t refKeyBlobLength = 0;
        const void* refKeyBlobBytes = aks_ref_key_get_blob(refKey, &refKeyBlobLength);
        NSData* refKeyBlob = [[NSData alloc] initWithBytesNoCopy:(void*)refKeyBlobBytes length:refKeyBlobLength];
        aks_ref_key_free(&refKey);
        return [[SecDbKeychainAKSWrappedKey alloc] initRefKeyWrappedKeyWithData:wrappedKey refKeyBlob:refKeyBlob];
    }
    else {
        NSMutableData* wrappedKey = [[NSMutableData alloc] initWithLength:(size_t)keyData.length + 40];
        bool success = [SecAKSObjCWrappers aksEncryptWithKeybag:keybag keyclass:_keyclass plaintext:keyData outKeyclass:&_keyclass ciphertext:wrappedKey error:error];
        return success ? [[SecDbKeychainAKSWrappedKey alloc] initRegularWrappedKeyWithData:wrappedKey] : nil;
    }
#else
    NSMutableData* wrappedKey = [[NSMutableData alloc] initWithLength:(size_t)keyData.length + 40];
    bool success = [SecAKSObjCWrappers aksEncryptWithKeybag:keybag keyclass:_keyclass plaintext:keyData outKeyclass:&_keyclass ciphertext:wrappedKey error:error];
    return success ? [[SecDbKeychainAKSWrappedKey alloc] initRegularWrappedKeyWithData:wrappedKey] : nil;
#endif
}

- (SFAESKey*)unwrapFromAKS:(SecDbKeychainAKSWrappedKey*)wrappedKey accessControl:(SecAccessControlRef)accessControl acmContext:(NSData*)acmContext callerAccessGroups:(NSArray*)callerAccessGroups delete:(BOOL)delete error:(NSError**)error
{
    NSData* wrappedKeyData = wrappedKey.wrappedKey;

    if (wrappedKey.type == SecDbKeychainAKSWrappedKeyTypeRegular) {
        NSMutableData* unwrappedKey = [NSMutableData dataWithCapacity:wrappedKeyData.length + 40];
        unwrappedKey.length = wrappedKeyData.length + 40;
        bool result = [SecAKSObjCWrappers aksDecryptWithKeybag:_keybag keyclass:_keyclass ciphertext:wrappedKeyData outKeyclass:&_keyclass plaintext:unwrappedKey error:error];
        if (result) {
            return [[SFAESKey alloc] initWithData:unwrappedKey specifier:[self.class keySpecifier] error:error];
        }
        else {
            return nil;
        }
    }
#if USE_KEYSTORE
    else if (wrappedKey.type == SecDbKeychainAKSWrappedKeyTypeRefKey) {
        aks_ref_key_t refKey = NULL;
        aks_ref_key_create_with_blob(_keybag, wrappedKey.refKeyBlob.bytes, wrappedKey.refKeyBlob.length, &refKey);

        CFErrorRef cfError = NULL;
        size_t refKeyExternalDataLength = 0;
        const uint8_t* refKeyExternalDataBytes = aks_ref_key_get_external_data(refKey, &refKeyExternalDataLength);
        if (!refKeyExternalDataBytes) {
            aks_ref_key_free(&refKey);
            return nil;
        }
        NSDictionary* aclDict = nil;
        der_decode_plist(NULL, kCFPropertyListImmutable, (CFPropertyListRef*)(void*)&aclDict, &cfError, refKeyExternalDataBytes, refKeyExternalDataBytes + refKeyExternalDataLength);
        if (!aclDict) {
            SecError(errSecDecode, &cfError, CFSTR("SecDbKeychainItemV7: failed to decode acl dict"));
        }
        SecAccessControlSetConstraints(accessControl, (__bridge CFDictionaryRef)aclDict);
        if (!SecAccessControlGetConstraint(accessControl, kAKSKeyOpEncrypt)) {
            SecAccessControlAddConstraintForOperation(accessControl, kAKSKeyOpEncrypt, kCFBooleanTrue, &cfError);
        }

        size_t derPlistLength = der_sizeof_plist((__bridge CFPropertyListRef)callerAccessGroups, &cfError);
        NSMutableData* accessGroupDERData = [[NSMutableData alloc] initWithLength:derPlistLength];
        der_encode_plist((__bridge CFPropertyListRef)callerAccessGroups, &cfError, accessGroupDERData.mutableBytes, accessGroupDERData.mutableBytes + derPlistLength);
        void* aksParams = NULL;
        size_t aksParamsLength = 0;
        aks_operation_optional_params(accessGroupDERData.bytes, derPlistLength, NULL, 0, acmContext.bytes, (int)acmContext.length, &aksParams, &aksParamsLength);

        void* unwrappedKeyDERData = NULL;
        size_t unwrappedKeyDERLength = 0;
        int aksResult = aks_ref_key_decrypt(refKey, aksParams, aksParamsLength, wrappedKeyData.bytes, wrappedKeyData.length, &unwrappedKeyDERData, &unwrappedKeyDERLength);
        if (aksResult != 0) {
            CFDataRef accessControlData = SecAccessControlCopyData(accessControl);
            create_cferror_from_aks(aksResult, kAKSKeyOpDecrypt, 0, 0, accessControlData, (__bridge CFDataRef)acmContext, &cfError);
            CFReleaseNull(accessControlData);
            aks_ref_key_free(&refKey);
            free(aksParams);
            BridgeCFErrorToNSErrorOut(error, cfError);
            return nil;
        }
        if (!unwrappedKeyDERData) {
            SecError(errSecDecode, &cfError, CFSTR("SecDbKeychainItemV7: failed to decrypt item, Item can't be decrypted due to failed decode der, so drop the item."));
            aks_ref_key_free(&refKey);
            free(aksParams);
            BridgeCFErrorToNSErrorOut(error, cfError);
            return nil;
        }

        CFPropertyListRef unwrappedKeyData = NULL;
        der_decode_plist(NULL, kCFPropertyListImmutable, &unwrappedKeyData, &cfError, unwrappedKeyDERData, unwrappedKeyDERData + unwrappedKeyDERLength);
        SFAESKey* result = nil;
        if ([(__bridge NSData*)unwrappedKeyData isKindOfClass:[NSData class]]) {
            result = [[SFAESKey alloc] initWithData:(__bridge NSData*)unwrappedKeyData specifier:[self.class keySpecifier] error:error];
            CFReleaseNull(unwrappedKeyDERData);
        }
        else {
            SecError(errSecDecode, &cfError, CFSTR("SecDbKeychainItemV7: failed to decrypt item, Item can't be decrypted due to failed decode der, so drop the item."));
            aks_ref_key_free(&refKey);
            free(aksParams);
            free(unwrappedKeyDERData);
            BridgeCFErrorToNSErrorOut(error, cfError);
            return nil;
        }

        if (delete) {
            aksResult = aks_ref_key_delete(refKey, aksParams, aksParamsLength);
            if (aksResult != 0) {
                CFDataRef accessControlData = SecAccessControlCopyData(accessControl);
                create_cferror_from_aks(aksResult, kAKSKeyOpDelete, 0, 0, accessControlData, (__bridge CFDataRef)acmContext, &cfError);
                CFReleaseNull(accessControlData);
                aks_ref_key_free(&refKey);
                free(aksParams);
                free(unwrappedKeyDERData);
                BridgeCFErrorToNSErrorOut(error, cfError);
                return nil;
            }
        }

        BridgeCFErrorToNSErrorOut(error, cfError);
        aks_ref_key_free(&refKey);
        free(aksParams);
        free(unwrappedKeyDERData);
        return result;
    }
#endif
    else {
        return nil;
    }
}

@end
