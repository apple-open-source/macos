/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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

#import <Foundation/Foundation.h>

#include <AssertMacros.h>
#include <Security/SecFramework.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecItemInternal.h>
#include <Security/SecBasePriv.h>
#include <Security/SecAccessControlPriv.h>
#include <utilities/SecCFError.h>
#include <Security/SecCFAllocator.h>
#include <utilities/SecCFWrappers.h>
#import <CryptoTokenKit/CryptoTokenKit_Private.h>
#import <LocalAuthentication/LocalAuthentication_Private.h>
#include "OSX/sec/Security/SecItemShim.h"

#include "SecECKey.h"
#include "SecRSAKey.h"
#include "SecCTKKeyPriv.h"

#include "SecSoftLink.h"

const CFStringRef kSecUseToken = CFSTR("u_Token");
const CFStringRef kSecUseTokenSession = CFSTR("u_TokenSession");

@interface SecCTKKey : NSObject<NSCopying>
@property (nonatomic) TKClientTokenObject *tokenObject;
@property (nonatomic, readonly) NSDictionary *keychainAttributes;
@property (nonatomic) NSDictionary *sessionParameters;
@end

@implementation SecCTKKey

+ (SecCTKKey *)fromKeyRef:(SecKeyRef)keyRef {
    return (__bridge SecCTKKey *)keyRef->key;
}

- (nullable instancetype)initWithAttributes:(NSDictionary *)attributes error:(NSError **)error {
    if (self = [super init]) {
        // Get or connect or generate token/session/object.
        if (_tokenObject == nil) {
            TKClientTokenSession *session = attributes[(__bridge id)kSecUseTokenSession];
            if (session == nil) {
                // Get new session.
                if (isCryptoTokenKitAvailable()) {
                    TKClientToken *token = [[getTKClientTokenClass() alloc] initWithTokenID:attributes[(id)kSecAttrTokenID]];
                    session = [token sessionWithLAContext:attributes[(id)kSecUseAuthenticationContext] error:error];
                } else {
                    if (error != nil) {
                        *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:@{NSDebugDescriptionErrorKey: @"CryptoTokenKit is not available"}];
                    }
                }
                if (session == nil) {
                    return nil;
                }
            }

            NSData *objectID = attributes[(id)kSecAttrTokenOID];
            if (objectID != nil) {
                // Get actual tokenObject from the session.
                _tokenObject = [session objectForObjectID:objectID error:error];
            } else {
                // Generate new key.
                NSMutableDictionary *attrs = attributes.mutableCopy;
                if (attributes[(id)kSecUseAuthenticationContext] != nil) {
                    // LAContext is passed differently, not by attributes.
                    [attrs removeObjectForKey:(id)kSecUseAuthenticationContext];
                }
                id sac = attributes[(id)kSecAttrAccessControl];
                if (sac != nil && (CFGetTypeID((__bridge CFTypeRef)sac) == SecAccessControlGetTypeID())) {
                    // Convert SecAccessControl to NSData format as expected by TKClientToken interface.
                    sac = CFBridgingRelease(SecAccessControlCopyData((__bridge SecAccessControlRef)sac));
                    attrs[(id)kSecAttrAccessControl] = sac;
                }
                attributes = attrs.copy;
                _tokenObject = [session createObjectWithAttributes:attributes error:error];
            }

            if (_tokenObject == nil) {
                return nil;
            }
        }

        _sessionParameters = @{};

        // Get resulting attributes, by merging input attributes from keychain and token object attributes. Keychain attributes have preference.
        NSMutableDictionary *attrs = self.tokenObject.keychainAttributes.mutableCopy;
        [attrs addEntriesFromDictionary:attributes];

        // Convert kSecAttrAccessControl from data to CF object if needed.
        id accc = attrs[(id)kSecAttrAccessControl];
        if ([accc isKindOfClass:NSData.class]) {
            accc = CFBridgingRelease(SecAccessControlCreateFromData(kCFAllocatorDefault, (__bridge CFDataRef)accc, (void *)error));
            if (accc == nil) {
                return nil;
            }
            attrs[(id)kSecAttrAccessControl] = accc;
        }

        // Remove internal-only attributes.
        [attrs removeObjectForKey:(id)kSecAttrTokenOID];
        [attrs removeObjectForKey:(__bridge id)kSecUseTokenSession];

        // Convert some attributes which are stored as numbers in iOS keychain but a lot of code counts that the values
        // are actually strings as specified by kSecAttrXxx constants.
        for (id attrName in @[(id)kSecAttrKeyType, (id)kSecAttrKeyClass]) {
            id value = attrs[attrName];
            if ([value isKindOfClass:NSNumber.class]) {
                attrs[attrName] = [value stringValue];
            }
        }

        // Sanitize some important attributes.
        attrs[(id)kSecClass] = (id)kSecClassKey;
        attrs[(id)kSecAttrKeyClass] = (id)kSecAttrKeyClassPrivate;

        // Store resulting attributes as an immutable dictionary.
        _keychainAttributes = attrs.copy;
    }

    return self;
}

- (instancetype)initFromKey:(SecCTKKey *)source {
    if (self = [super init]) {
        _sessionParameters = source.sessionParameters;
        _keychainAttributes = source.keychainAttributes;
        _tokenObject = source.tokenObject;
    }

    return self;
}

- (nonnull id)copyWithZone:(nullable NSZone *)zone {
    return [[SecCTKKey alloc] initFromKey:self];
}

- (nullable id)performOperation:(TKTokenOperation)operation data:(nullable NSData *)data algorithms:(NSArray<NSString *> *)algorithms parameters:(NSDictionary *)parameters error:(NSError **)error {
    // Check, whether we are not trying to perform the operation with large data.  If yes, explicitly do the check whether
    // the operation is supported first, in order to avoid jetsam of target extension with operation type which is typically
    // not supported by the extension at all.
    // <rdar://problem/31762984> unable to decrypt large data with kSecKeyAlgorithmECIESEncryptionCofactorX963SHA256AESGCM
    if (data != nil) {
        if (data.length > 32 * 1024) {
            id result = [self.tokenObject operation:operation data:nil algorithms:algorithms parameters:parameters error:error];
            if (result == nil || [result isEqual:NSNull.null]) {
                // Operation failed or si not supported with specified algorithms.
                return result;
            }
        }
    }

    return [self.tokenObject operation:operation data:data algorithms:algorithms parameters:parameters error:error];
}

- (BOOL)isEqual:(id)object {
    SecCTKKey *other = object;
    return [self.tokenObject.session.token.tokenID isEqualToString:other.tokenObject.session.token.tokenID] && [self.tokenObject.objectID isEqualToData:other.tokenObject.objectID];
}

@end

static void SecCTKKeyDestroy(SecKeyRef keyRef) {
    @autoreleasepool {
        CFBridgingRelease(keyRef->key);
    }
}

static CFIndex SecCTKGetAlgorithmID(SecKeyRef keyRef) {
    SecCTKKey *key = [SecCTKKey fromKeyRef:keyRef];
    NSString *keyType = key.tokenObject.keychainAttributes[(id)kSecAttrKeyType];
    if ([keyType isEqualToString:(id)kSecAttrKeyTypeECSECPrimeRandom] ||
        [keyType isEqualToString:(id)kSecAttrKeyTypeECSECPrimeRandomPKA] ||
        [keyType isEqualToString:(id)kSecAttrKeyTypeSecureEnclaveAttestation]) {
        return kSecECDSAAlgorithmID;
    }
    return kSecRSAAlgorithmID;
}

static CFTypeRef SecCTKKeyCopyOperationResult(SecKeyRef keyRef, SecKeyOperationType operation, SecKeyAlgorithm algorithm,
                                              CFArrayRef algorithms, SecKeyOperationMode mode,
                                              CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    TKTokenOperation tokenOperation;
    switch (operation) {
        case kSecKeyOperationTypeSign:
            tokenOperation = TKTokenOperationSignData;
            break;
        case kSecKeyOperationTypeDecrypt:
            tokenOperation = TKTokenOperationDecryptData;
            break;
        case kSecKeyOperationTypeKeyExchange:
            tokenOperation = TKTokenOperationPerformKeyExchange;
            break;
        default:
            SecError(errSecParam, error, CFSTR("Invalid key operation %d"), (int)operation);
            return nil;
    }

    SecCTKKey *key = [SecCTKKey fromKeyRef:keyRef];
    key.tokenObject.session.authenticateWhenNeeded = YES;
    NSError *err;
    id result = [key.tokenObject operation:tokenOperation data:(__bridge NSData *)in1 algorithms:(__bridge NSArray *)algorithms parameters:(__bridge NSDictionary *)in2 error:&err];
    if (result == nil && error != NULL) {
        *error = (CFErrorRef)CFBridgingRetain(err);
    }
    return CFBridgingRetain(result);
}

static size_t SecCTKKeyBlockSize(SecKeyRef keyRef) {
    SecCTKKey *key = [SecCTKKey fromKeyRef:keyRef];
    NSNumber *bitSize = key.keychainAttributes[(id)kSecAttrKeySizeInBits];
    if ([bitSize isKindOfClass:NSNumber.class]) {
        return (bitSize.integerValue + 7) / 8;
    }

    return 0;
}

static OSStatus SecCTKKeyCopyPublicOctets(SecKeyRef keyRef, CFDataRef *data) {
    SecCTKKey *key = [SecCTKKey fromKeyRef:keyRef];
    *data = CFBridgingRetain(key.tokenObject.publicKey);
    return errSecSuccess;
}

static CFStringRef SecCTKKeyCopyKeyDescription(SecKeyRef keyRef) {
    SecCTKKey *key = [SecCTKKey fromKeyRef:keyRef];
    NSString *tokenID = key.keychainAttributes[(id)kSecAttrTokenID] ?: @"uninited";
    NSString *description = [NSString stringWithFormat:@"<SecKeyRef:('%@') %p>", tokenID, keyRef];
    return CFBridgingRetain(description);
}

static CFDictionaryRef SecCTKKeyCopyAttributeDictionary(SecKeyRef keyRef) {
    static NSArray<NSString *> *exportableAttributes;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        exportableAttributes = @[
            (id)kSecClass,
            (id)kSecAttrTokenID,
            (id)kSecAttrKeyClass,
            (id)kSecAttrAccessControl,
            (id)kSecAttrIsPrivate,
            (id)kSecAttrIsModifiable,
            (id)kSecAttrKeyType,
            (id)kSecAttrKeySizeInBits,
            (id)kSecAttrEffectiveKeySize,
            (id)kSecAttrIsSensitive,
            (id)kSecAttrWasAlwaysSensitive,
            (id)kSecAttrIsExtractable,
            (id)kSecAttrWasNeverExtractable,
            (id)kSecAttrCanEncrypt,
            (id)kSecAttrCanDecrypt,
            (id)kSecAttrCanDerive,
            (id)kSecAttrCanSign,
            (id)kSecAttrCanVerify,
            (id)kSecAttrCanSignRecover,
            (id)kSecAttrCanVerifyRecover,
            (id)kSecAttrCanWrap,
            (id)kSecAttrCanUnwrap,
        ];
    });

    SecCTKKey *key = [SecCTKKey fromKeyRef:keyRef];
    NSMutableDictionary *attrs = [[NSMutableDictionary alloc] init];
    for (NSString *attrName in exportableAttributes) {
        id value = key.keychainAttributes[attrName];
        if (value != nil) {
            attrs[attrName] = value;
        }
    }

    // Encode ApplicationLabel as SHA1 digest of public key bytes.
    NSData *publicKey = key.tokenObject.publicKey;
    attrs[(id)kSecAttrApplicationLabel] = CFBridgingRelease(SecSHA1DigestCreate(kCFAllocatorDefault, publicKey.bytes, publicKey.length));

    // Consistently with existing RSA and EC software keys implementation, mark all keys as permanent ones.
    attrs[(id)kSecAttrIsPermanent] = @YES;

    // Always export token_id and object_id.
    attrs[(id)kSecAttrTokenID] = key.tokenObject.session.token.tokenID;
    attrs[(id)kSecAttrTokenOID] = key.tokenObject.objectID;

    // Return immutable copy of created dictionary.
    return CFBridgingRetain(attrs.copy);
}

static SecKeyRef SecCTKKeyCreateDuplicate(SecKeyRef keyRef);

static LAContext *processAuthParameters(NSMutableDictionary *parameters) {
    LAContext *authContext;
    for (NSString *name in parameters.copy) {
        id value = parameters[name];
        if ([name isEqual:(id)kSecUseAuthenticationContext]) {
            authContext = value;
            [parameters removeObjectForKey:name];
        }

        if ([name isEqual:(id)kSecUseCredentialReference]) {
            if (isLocalAuthenticationAvailable()) {
                authContext = [[getLAContextClass() alloc] initWithExternalizedContext:value];
            }
            [parameters removeObjectForKey:name];
        }

#if !TARGET_OS_WATCH && !TARGET_OS_TV
        if ([name isEqual:(id)kSecUseOperationPrompt]) {
            if (isLocalAuthenticationAvailable()) {
                authContext = authContext ?: [[getLAContextClass() alloc] init];
                authContext.localizedReason = value;
            }
            [parameters removeObjectForKey:name];
        }
#endif

        if (isLocalAuthenticationAvailable() && [name isEqual:(id)kSecUseCallerName]) {
            if (isLocalAuthenticationAvailable()) {
                authContext = authContext ?: [[getLAContextClass() alloc] init];
                authContext.optionCallerName = value;
            }
            [parameters removeObjectForKey:name];
        }

        if ([name isEqual:(id)kSecUseAuthenticationUI]) {
            if (isLocalAuthenticationAvailable()) {
                authContext = authContext ?: [[getLAContextClass() alloc] init];
                authContext.optionNotInteractive = @([value isEqual:(id)kSecUseAuthenticationUIFail]);
            }
            [parameters removeObjectForKey:name];
        }
    }

    return authContext;
}

static Boolean SecCTKKeySetParameter(SecKeyRef keyRef, CFStringRef name, CFPropertyListRef value, CFErrorRef *error) {
    SecCTKKey *key = [SecCTKKey fromKeyRef:keyRef];
    NSMutableDictionary *parameters = key.sessionParameters.mutableCopy;
    parameters[(__bridge id)name] = (__bridge id)value;
    LAContext *authContext = processAuthParameters(parameters);

    // Reconnect token object.
    NSError *err;
    TKClientTokenObject *object;
    if (isCryptoTokenKitAvailable()) {
        TKClientTokenSession *session = [[getTKClientTokenSessionClass() alloc] initWithToken:key.tokenObject.session.token LAContext:authContext parameters:parameters error:&err];
        object = [session objectForObjectID:key.tokenObject.objectID error:&err];
    } else {
        err = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecUnimplemented userInfo:@{NSDebugDescriptionErrorKey: @"CryptoTokenKit is not available"}];
    }
    if (object == nil) {
        if (error != NULL) {
            *error = (CFErrorRef)CFBridgingRetain(err);
        }
        return false;
    }

    key.sessionParameters = parameters;
    key.tokenObject = object;

    return true;
}

static Boolean SecCTKKeyIsEqual(SecKeyRef keyRef1, SecKeyRef keyRef2) {
    SecCTKKey *key1 = [SecCTKKey fromKeyRef:keyRef1];
    SecCTKKey *key2 = [SecCTKKey fromKeyRef:keyRef2];
    return [key1 isEqual:key2];
}

static SecKeyDescriptor kSecCTKKeyDescriptor = {
    .version = kSecKeyDescriptorVersion,
    .name = "CTKKey",

    .destroy = SecCTKKeyDestroy,
    .blockSize = SecCTKKeyBlockSize,
    .copyDictionary = SecCTKKeyCopyAttributeDictionary,
    .describe = SecCTKKeyCopyKeyDescription,
    .getAlgorithmID = SecCTKGetAlgorithmID,
    .copyPublic = SecCTKKeyCopyPublicOctets,
    .copyOperationResult = SecCTKKeyCopyOperationResult,
    .isEqual = SecCTKKeyIsEqual,
    .createDuplicate = SecCTKKeyCreateDuplicate,
    .setParameter = SecCTKKeySetParameter,
};

static SecKeyRef SecCTKKeyCreateDuplicate(SecKeyRef keyRef) {
    SecCTKKey *sourceKey = [SecCTKKey fromKeyRef:keyRef];
    SecKeyRef result = SecKeyCreate(CFGetAllocator(keyRef), &kSecCTKKeyDescriptor, 0, 0, 0);
    result->key = (void *)CFBridgingRetain(sourceKey.copy);
    return result;
}

SecKeyRef SecKeyCreateCTKKey(CFAllocatorRef allocator, CFDictionaryRef refAttributes, CFErrorRef *error) {
    SecKeyRef keyRef = SecKeyCreate(allocator, &kSecCTKKeyDescriptor, 0, 0, 0);
    NSError *err;
    SecCTKKey *key = [[SecCTKKey alloc] initWithAttributes:(__bridge NSDictionary *)refAttributes error:&err];
    if (key == nil) {
        if (error != NULL) {
            *error = (CFErrorRef)CFBridgingRetain(err);
        }
        return NULL;
    }

    keyRef->key = (void *)CFBridgingRetain(key);
    return keyRef;
}

OSStatus SecCTKKeyGeneratePair(CFDictionaryRef parameters, SecKeyRef *publicKey, SecKeyRef *privateKey) {
    if (publicKey == NULL || privateKey == NULL) {
        return errSecParam;
    }

    // Generate new key from given parameters.
    NSMutableDictionary *params = [(__bridge NSDictionary *)parameters mutableCopy];
    NSDictionary *privateAttrs = params[(id)kSecPrivateKeyAttrs] ?: @{};
    [params removeObjectForKey:(id)kSecPrivateKeyAttrs];
    [params removeObjectForKey:(id)kSecPublicKeyAttrs];
    NSMutableDictionary *attrs = privateAttrs.mutableCopy;
    [attrs addEntriesFromDictionary:params];
    NSError *error;
    SecCTKKey *key = [[SecCTKKey alloc] initWithAttributes:attrs error:&error];
    if (key == nil) {
        return SecErrorGetOSStatus((__bridge CFErrorRef)error);
    }

    // Create non-token public key.
    NSData *publicKeyData = key.tokenObject.publicKey;
    id keyType = key.tokenObject.keychainAttributes[(id)kSecAttrKeyType];
    if ([keyType isEqual:(id)kSecAttrKeyTypeECSECPrimeRandom] || [keyType isEqual:(id)kSecAttrKeyTypeECSECPrimeRandomPKA] || [keyType isEqual:(id)kSecAttrKeyTypeSecureEnclaveAttestation]) {
        *publicKey = SecKeyCreateECPublicKey(SecCFAllocatorZeroize(), publicKeyData.bytes, publicKeyData.length, kSecKeyEncodingBytes);
    } else {
        *publicKey = SecKeyCreateRSAPublicKey(SecCFAllocatorZeroize(), publicKeyData.bytes, publicKeyData.length, kSecKeyEncodingBytes);
    }

    if (*publicKey == NULL) {
        return errSecInvalidKey;
    }

    *privateKey = SecKeyCreate(kCFAllocatorDefault, &kSecCTKKeyDescriptor, 0, 0, 0);
    (*privateKey)->key = (void *)CFBridgingRetain(key);
    return errSecSuccess;
}

const CFStringRef kSecKeyParameterSETokenAttestationNonce = CFSTR("com.apple.security.seckey.setoken.attestation-nonce");

SecKeyRef SecKeyCopyAttestationKey(SecKeyAttestationKeyType keyType, CFErrorRef *error) {
    return SecKeyCopySystemKey((SecKeySystemKeyType)keyType, error);
}

SecKeyRef SecKeyCopySystemKey(SecKeySystemKeyType keyType, CFErrorRef *error) {
    CFDictionaryRef attributes = NULL;
    CFDataRef object_id = NULL;
    SecKeyRef key = NULL;

    // [[TKTLVBERRecord alloc] initWithPropertyList:[@"com.apple.setoken.sik" dataUsingEncoding:NSUTF8StringEncoding]].data
    static const uint8_t sikObjectIDBytes[] = { 0x04, 21, 'c', 'o', 'm', '.', 'a', 'p', 'p', 'l', 'e', '.', 's', 'e', 't', 'o', 'k', 'e', 'n', '.', 's', 'i', 'k' };
    // [[TKTLVBERRecord alloc] initWithPropertyList:[@"com.apple.setoken.gid" dataUsingEncoding:NSUTF8StringEncoding]].data
    static const uint8_t gidObjectIDBytes[] = { 0x04, 21, 'c', 'o', 'm', '.', 'a', 'p', 'p', 'l', 'e', '.', 's', 'e', 't', 'o', 'k', 'e', 'n', '.', 'g', 'i', 'd' };
    // [[TKTLVBERRecord alloc] initWithPropertyList:[@"com.apple.setoken.uikc" dataUsingEncoding:NSUTF8StringEncoding]].data
    static const uint8_t uikCommittedObjectIDBytes[] = { 0x04, 22, 'c', 'o', 'm', '.', 'a', 'p', 'p', 'l', 'e', '.', 's', 'e', 't', 'o', 'k', 'e', 'n', '.', 'u', 'i', 'k', 'c' };
    // [[TKTLVBERRecord alloc] initWithPropertyList:[@"com.apple.setoken.uikp" dataUsingEncoding:NSUTF8StringEncoding]].data
    static const uint8_t uikProposedObjectIDBytes[] = { 0x04, 22, 'c', 'o', 'm', '.', 'a', 'p', 'p', 'l', 'e', '.', 's', 'e', 't', 'o', 'k', 'e', 'n', '.', 'u', 'i', 'k', 'p' };

    static const uint8_t casdObjectIDBytes[] = { 0x04, 27, 'c', 'o', 'm', '.', 'a', 'p', 'p', 'l', 'e', '.', 's', 'e', 'c', 'e', 'l', 'e', 'm', 't', 'o', 'k', 'e', 'n', '.', 'c', 'a', 's', 'd' };

    // [[TKTLVBERRecord alloc] initWithPropertyList:[@"com.apple.setoken.oikc" dataUsingEncoding:NSUTF8StringEncoding]].data
    static const uint8_t oikCommittedObjectIDBytes[] = { 0x04, 22, 'c', 'o', 'm', '.', 'a', 'p', 'p', 'l', 'e', '.', 's', 'e', 't', 'o', 'k', 'e', 'n', '.', 'o', 'i', 'k', 'c' };
    // [[TKTLVBERRecord alloc] initWithPropertyList:[@"com.apple.setoken.oikp" dataUsingEncoding:NSUTF8StringEncoding]].data
    static const uint8_t oikProposedObjectIDBytes[] = { 0x04, 22, 'c', 'o', 'm', '.', 'a', 'p', 'p', 'l', 'e', '.', 's', 'e', 't', 'o', 'k', 'e', 'n', '.', 'o', 'i', 'k', 'p' };

    // [[TKTLVBERRecord alloc] initWithPropertyList:[@"com.apple.setoken.dakc" dataUsingEncoding:NSUTF8StringEncoding]].data
    static const uint8_t dakCommittedObjectIDBytes[] = { 0x04, 22, 'c', 'o', 'm', '.', 'a', 'p', 'p', 'l', 'e', '.', 's', 'e', 't', 'o', 'k', 'e', 'n', '.', 'd', 'a', 'k', 'c' };
    // [[TKTLVBERRecord alloc] initWithPropertyList:[@"com.apple.setoken.dakp" dataUsingEncoding:NSUTF8StringEncoding]].data
    static const uint8_t dakProposedObjectIDBytes[] = { 0x04, 22, 'c', 'o', 'm', '.', 'a', 'p', 'p', 'l', 'e', '.', 's', 'e', 't', 'o', 'k', 'e', 'n', '.', 'd', 'a', 'k', 'p' };

    // [[TKTLVBERRecord alloc] initWithPropertyList:[@"com.apple.setoken.havenc" dataUsingEncoding:NSUTF8StringEncoding]].data
    static const uint8_t havenCommittedObjectIDBytes[] = { 0x04, 24, 'c', 'o', 'm', '.', 'a', 'p', 'p', 'l', 'e', '.', 's', 'e', 't', 'o', 'k', 'e', 'n', '.', 'h', 'a', 'v', 'e', 'n', 'c' };
    // [[TKTLVBERRecord alloc] initWithPropertyList:[@"com.apple.setoken.havenp" dataUsingEncoding:NSUTF8StringEncoding]].data
    static const uint8_t havenProposedObjectIDBytes[] = { 0x04, 24, 'c', 'o', 'm', '.', 'a', 'p', 'p', 'l', 'e', '.', 's', 'e', 't', 'o', 'k', 'e', 'n', '.', 'h', 'a', 'v', 'e', 'n', 'p' };

    CFStringRef token = kSecAttrTokenIDAppleKeyStore;
    
    switch (keyType) {
        case kSecKeySystemKeyTypeSIK:
            object_id = CFDataCreate(kCFAllocatorDefault, sikObjectIDBytes, sizeof(sikObjectIDBytes));
            break;
        case kSecKeySystemKeyTypeGID:
            object_id = CFDataCreate(kCFAllocatorDefault, gidObjectIDBytes, sizeof(gidObjectIDBytes));
            break;
        case kSecKeySystemKeyTypeUIKCommitted:
            object_id = CFDataCreate(kCFAllocatorDefault, uikCommittedObjectIDBytes, sizeof(uikCommittedObjectIDBytes));
            break;
        case kSecKeySystemKeyTypeUIKProposed:
            object_id = CFDataCreate(kCFAllocatorDefault, uikProposedObjectIDBytes, sizeof(uikProposedObjectIDBytes));
            break;
        case kSecKeySystemKeyTypeSecureElement:
            object_id = CFDataCreate(kCFAllocatorDefault, casdObjectIDBytes, sizeof(casdObjectIDBytes));
            token = kSecAttrTokenIDSecureElement;
            break;
        case kSecKeySystemKeyTypeOIKCommitted:
            object_id = CFDataCreate(kCFAllocatorDefault, oikCommittedObjectIDBytes, sizeof(uikCommittedObjectIDBytes));
            break;
        case kSecKeySystemKeyTypeOIKProposed:
            object_id = CFDataCreate(kCFAllocatorDefault, oikProposedObjectIDBytes, sizeof(uikProposedObjectIDBytes));
            break;
        case kSecKeySystemKeyTypeDAKCommitted:
            object_id = CFDataCreate(kCFAllocatorDefault, dakCommittedObjectIDBytes, sizeof(uikCommittedObjectIDBytes));
            break;
        case kSecKeySystemKeyTypeDAKProposed:
            object_id = CFDataCreate(kCFAllocatorDefault, dakProposedObjectIDBytes, sizeof(uikProposedObjectIDBytes));
            break;
        case kSecKeySystemKeyTypeHavenCommitted:
            object_id = CFDataCreate(kCFAllocatorDefault, havenCommittedObjectIDBytes, sizeof(havenCommittedObjectIDBytes));
            break;
        case kSecKeySystemKeyTypeHavenProposed:
            object_id = CFDataCreate(kCFAllocatorDefault, havenProposedObjectIDBytes, sizeof(havenProposedObjectIDBytes));
            break;
        default:
            SecError(errSecParam, error, CFSTR("unexpected attestation key type %d"), (int)keyType);
            goto out;
    }

    attributes = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                              kSecAttrTokenOID, object_id,
                                              kSecAttrTokenID, token,
                                              NULL);
    key = SecKeyCreateCTKKey(SecCFAllocatorZeroize(), attributes, error);

out:
    CFReleaseSafe(attributes);
    CFReleaseSafe(object_id);
    return key;
}

CFDataRef SecKeyCreateAttestation(SecKeyRef keyRef, SecKeyRef toAttestKeyRef, CFErrorRef *error) {
    SecCTKKey *key = [SecCTKKey fromKeyRef:keyRef];
    SecCTKKey *keyToAttest = [SecCTKKey fromKeyRef:toAttestKeyRef];

    if (keyRef->key_class != &kSecCTKKeyDescriptor) {
        SecError(errSecUnsupportedOperation, error, CFSTR("attestation not supported by key %@"), keyRef);
        return NULL;
    }
    if (toAttestKeyRef->key_class != &kSecCTKKeyDescriptor) {
        SecError(errSecUnsupportedOperation, error, CFSTR("attestation not supported for key %@"), toAttestKeyRef);
        return NULL;
    }

    NSError *err;
    NSData *attestation = [key.tokenObject attestKey:keyToAttest.tokenObject.objectID nonce:key.sessionParameters[(__bridge id)kSecKeyParameterSETokenAttestationNonce] error:&err];
    if (attestation == nil) {
        if (error != NULL) {
            *error = (CFErrorRef)CFBridgingRetain(err);
        }
        return NULL;
    }

    return CFBridgingRetain(attestation);
}

Boolean SecKeyControlLifetime(SecKeyRef keyRef, SecKeyControlLifetimeType type, CFErrorRef *error) {
    if (keyRef->key_class != &kSecCTKKeyDescriptor) {
        return SecError(errSecUnsupportedOperation, error, CFSTR("lifetimecontrol not supported for key %@"), keyRef);
    }

    BOOL result;
    NSError *err;
    SecCTKKey *key = [SecCTKKey fromKeyRef:keyRef];
    switch (type) {
        case kSecKeyControlLifetimeTypeBump:
            result = [key.tokenObject bumpKeyWithError:&err];
            break;
        case kSecKeyControlLifetimeTypeCommit:
            result = [key.tokenObject commitKeyWithError:&err];
            break;
        default:
            return SecError(errSecParam, error, CFSTR("Unsupported lifetime operation %d requested"), (int)type);
    }

    if (!result) {
        if (error != NULL) {
            *error = (CFErrorRef)CFBridgingRetain(err);
        }
        return false;
    }

    return true;
}
