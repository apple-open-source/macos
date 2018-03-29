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
#if OCTAGON

#import "OTEscrowKeys.h"

#import <Security/SecItemPriv.h>
#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#import <Foundation/Foundation.h>
#import <utilities/debugging.h>
#import <utilities/SecCFWrappers.h>

#import <SecurityFoundation/SFEncryptionOperation.h>
#import <SecurityFoundation/SFSigningOperation.h>
#import <SecurityFoundation/SFDigestOperation.h>
#import <SecurityFoundation/SFKey.h>
#import <SecurityFoundation/SFKey_Private.h>

#import "keychain/ot/OTDefines.h"

#import <corecrypto/cchkdf.h>
#import <corecrypto/ccsha2.h>
#import <corecrypto/ccec.h>

#import <CommonCrypto/CommonRandomSPI.h>
#import <Security/SecCFAllocator.h>
#import <Foundation/NSKeyedArchiver_Private.h>

static uint8_t escrowedSigningPrivKey[] = {'E', 's', 'c', 'r', 'o', 'w', ' ', 'S', 'i', 'g', 'n', 'i', 'n', 'g', ' ', 'P', 'r', 'i', 'v', 'a', 't', 'e', ' ', 'K', 'e', 'y'};
static uint8_t escrowedEncryptionPrivKey[] = { 'E', 's', 'c', 'r', 'o', 'w', ' ','E', 'n', 'c', 'r', 'y', 'p', 't', 'i', 'o', 'n', ' ', 'P', 'r', 'v', 'a', 't', 'e', ' ', 'K', 'e', 'y' };
static uint8_t escrowedSymmetric[] = {'E', 's', 'c', 'r', 'o', 'w', ' ', 'S', 'y', 'm', 'm', 'e', 't', 'r', 'i','c',' ', 'K', 'e', 'y' };

#define OT_ESCROW_SIGNING_HKDF_SIZE         56
#define OT_ESCROW_ENCRYPTION_HKDF_SIZE      56
#define OT_ESCROW_SYMMETRIC_HKDF_SIZE       32

@interface OTEscrowKeys ()
@property (nonatomic, strong) SFECKeyPair* encryptionKey;
@property (nonatomic, strong) SFECKeyPair* signingKey;
@property (nonatomic, strong) SFAESKey* symmetricKey;
@property (nonatomic, strong) NSData* secret;
@property (nonatomic, strong) NSString* dsid;
@end

@implementation OTEscrowKeys

- (nullable instancetype) initWithSecret:(NSData*)secret
                                    dsid:(NSString*)dsid
                                   error:(NSError* __autoreleasing *)error
{
    self = [super init];
    if (self) {
        NSError* localError = nil;

        if([secret length] == 0){
            if(error){
                *error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorEmptySecret userInfo:@{NSLocalizedDescriptionKey: @"entropy/secret is nil"}];
            }
            return nil;
        }
        _secret = [secret copy];
        
        if([dsid length] == 0){
            if(error){
                *error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorEmptyDSID userInfo:@{NSLocalizedDescriptionKey: @"dsid is nil"}];
            }
            return nil;
        }
        _dsid = [dsid copy];
        
        NSData *data = [OTEscrowKeys generateEscrowKey:kOTEscrowKeySigning masterSecret:secret dsid:self.dsid error:&localError];
        if (!data) {
            if(error){
                *error = localError;
            }
            return nil;
        }
        _signingKey =    [[SFECKeyPair alloc] initWithSecKey:[OTEscrowKeys createSecKey:data]];
        if(!_signingKey){
            if(error){
                *error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorKeyGeneration userInfo:@{NSLocalizedDescriptionKey: @"failed to create EC signing key"}];
            }
            return nil;
        }
        data = [OTEscrowKeys generateEscrowKey:kOTEscrowKeyEncryption masterSecret:secret dsid:self.dsid error:&localError];
        if (!data) {
            if(error){
                *error = localError;
            }
            return nil;
        }
        _encryptionKey = [[SFECKeyPair alloc] initWithSecKey:[OTEscrowKeys createSecKey:data]];
        if(!_encryptionKey){
            if(error){
                *error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorKeyGeneration userInfo:@{NSLocalizedDescriptionKey: @"failed to create EC encryption key"}];
            }
            return nil;
        }
        data = [OTEscrowKeys generateEscrowKey:kOTEscrowKeySymmetric masterSecret:secret dsid:self.dsid error:&localError];
        if (!data) {
            if(error){
                *error = localError;
            }
            return nil;
        }
        _symmetricKey =  [[SFAESKey alloc] initWithData:data specifier:[[SFAESKeySpecifier alloc] initWithBitSize:SFAESKeyBitSize256] error:&localError];
        if (!_symmetricKey) {
            if(error){
                *error = localError;
            }
            return nil;
        }

        BOOL result = [OTEscrowKeys storeEscrowedSigningKeyPair:[_signingKey keyData] error:&localError];
        if(!result || localError){
            secerror("octagon: could not store escrowed signing SPKI in keychain: %@", localError);
            if(error){
                *error = localError;
            }
            return nil;
        }
        result = [OTEscrowKeys storeEscrowedEncryptionKeyPair:[_encryptionKey keyData] error:error];
        if(!result || localError){
            secerror("octagon: could not store escrowed signing SPKI in keychain: %@", localError);
            if(error){
                *error = localError;
            }
            return nil;
        }
        result = [OTEscrowKeys storeEscrowedSymmetricKey:[_symmetricKey keyData] error:error];
        if(!result || localError){
            secerror("octagon: could not store escrowed signing SPKI in keychain: %@", localError);
            if(error){
                *error = localError;
            }
            return nil;
        }
    }
    return self;
}

+ (NSData* _Nullable) generateEscrowKey:(escrowKeyType)keyType
                           masterSecret:(NSData*)masterSecret
                                   dsid:(NSString *)dsid
                                  error:(NSError**)error
{
    NSUInteger keyLength = 0;
    const void *info = nil;
    size_t infoLength = 0;
    NSMutableData* derivedKey = NULL;

    switch(keyType)
    {
        case kOTEscrowKeySymmetric:
            keyLength = OT_ESCROW_SYMMETRIC_HKDF_SIZE;
            info = escrowedSymmetric;
            infoLength = sizeof(escrowedSymmetric);
            break;
        case kOTEscrowKeyEncryption:
            keyLength = OT_ESCROW_ENCRYPTION_HKDF_SIZE;
            info = escrowedEncryptionPrivKey;
            infoLength = sizeof(escrowedEncryptionPrivKey);
            break;
        case kOTEscrowKeySigning:
            keyLength = OT_ESCROW_SIGNING_HKDF_SIZE;
            info = escrowedSigningPrivKey;
            infoLength = sizeof(escrowedSigningPrivKey);
            break;
        default:
            break;
    }

    ccec_const_cp_t cp = ccec_cp_384();
    int status = 0;
    
    ccec_full_ctx_decl_cp(cp, fullKey);
    
    derivedKey = [NSMutableData dataWithLength:keyLength];
    status = cchkdf(ccsha384_di(),
                    [masterSecret length], [masterSecret bytes],
                    strlen([dsid UTF8String]),[dsid UTF8String],
                    infoLength, info,
                    keyLength, [derivedKey mutableBytes]);
    
   
    if (status != 0) {
        if(error){
            *error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorKeyGeneration userInfo:nil];
        }
        secerror("octagon: could not generate seed for signing keys");
        return nil;
    }
    if(keyType == kOTEscrowKeySymmetric){
        return derivedKey;
    }
    else if(keyType == kOTEscrowKeyEncryption || keyType == kOTEscrowKeySigning){
       
        status = ccec_generate_key_deterministic(cp,
                                                 [derivedKey length], [derivedKey mutableBytes],
                                                 ccDRBGGetRngState(),
                                                 CCEC_GENKEY_DETERMINISTIC_FIPS,
                                                 fullKey);
        if(status != 0){
            if(error){
                *error = [NSError errorWithDomain:octagonErrorDomain code:OTErrorKeyGeneration userInfo:nil];
            }
            secerror("octagon: could not generate signing keys");
            return nil;
        }
        
        size_t space = ccec_x963_export_size(true, ccec_ctx_pub(fullKey));
        NSMutableData* key = [[NSMutableData alloc]initWithLength:space];
        ccec_x963_export(true, [key mutableBytes], fullKey);
        derivedKey = key;
    }
    return derivedKey;
}

+ (SecKeyRef) createSecKey:(NSData*)keyData
{
    NSDictionary *keyAttributes = @{
                                    (__bridge id)kSecAttrKeyClass : (__bridge id)kSecAttrKeyClassPrivate,
                                    (__bridge id)kSecAttrKeyType : (__bridge id)kSecAttrKeyTypeEC,
                                    };
    
    SecKeyRef key = SecKeyCreateWithData((__bridge CFDataRef)keyData, (__bridge CFDictionaryRef)keyAttributes, NULL);
    return key;
}

+ (BOOL) setKeyMaterialInKeychain:(NSDictionary*)query error:(NSError* __autoreleasing *)error
{
    BOOL result = NO;

    CFTypeRef results = NULL;
    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)query, &results);
    
    NSError* localerror = nil;
    
    if(status == errSecDuplicateItem || status == errSecSuccess) {
        result  = YES;
    } else {
        localerror = [NSError errorWithDomain:@"securityd"
                                         code:status
                                     userInfo:nil];
    }
    if(status != errSecSuccess) {
        CFReleaseNull(results);
        
        if(error) {
            *error = localerror;
        }
    }
    
    return result;
}

+(NSString*) hashIt:(NSData*)keyData
{
    const struct ccdigest_info *di = ccsha384_di();
    NSMutableData* result = [[NSMutableData alloc] initWithLength:ccsha384_di()->output_size];

    ccdigest(di, [keyData length], [keyData bytes], [result mutableBytes]);

    NSString* hash = [result base64EncodedStringWithOptions:0];
    return hash;
}

+ (BOOL)storeEscrowedEncryptionKeyPair:(NSData*)keyData error:(NSError**)error
{
    NSDictionary* query = @{
                            (id)kSecClass : (id)kSecClassInternetPassword,
                            (id)kSecAttrAccessible: (id)kSecAttrAccessibleWhenUnlocked,
                            (id)kSecAttrNoLegacy : @YES,
                            (id)kSecAttrAccessGroup: @"com.apple.security.ckks",
                            (id)kSecAttrSynchronizable : (id)kCFBooleanFalse,
                            (id)kSecAttrServer : [self hashIt:keyData],
                            (id)kSecAttrLabel : @"Escrowed Encryption Key",
                            (id)kSecValueData : keyData,
                            };
    return [OTEscrowKeys setKeyMaterialInKeychain:query error:error];
}

+ (BOOL)storeEscrowedSigningKeyPair:(NSData*)keyData error:(NSError**)error
{
    NSDictionary* query = @{
                            (id)kSecClass : (id)kSecClassInternetPassword,
                            (id)kSecAttrAccessible: (id)kSecAttrAccessibleWhenUnlocked,
                            (id)kSecAttrNoLegacy : @YES,
                            (id)kSecAttrAccessGroup: @"com.apple.security.ckks",
                            (id)kSecAttrSynchronizable : (id)kCFBooleanFalse,
                            (id)kSecAttrLabel : @"Escrowed Signing Key",
                            (id)kSecAttrServer : [self hashIt:keyData],
                            (id)kSecValueData : keyData,
                            };
   return [OTEscrowKeys setKeyMaterialInKeychain:query error:error];
}

+ (BOOL)storeEscrowedSymmetricKey:(NSData*)keyData error:(NSError**)error
{
    NSDictionary* query = @{
                            (id)kSecClass : (id)kSecClassInternetPassword,
                            (id)kSecAttrAccessible: (id)kSecAttrAccessibleWhenUnlocked,
                            (id)kSecAttrNoLegacy : @YES,
                            (id)kSecAttrAccessGroup: @"com.apple.security.ckks",
                            (id)kSecAttrSynchronizable : (id)kCFBooleanFalse,
                            (id)kSecAttrLabel : @"Escrowed Symmetric Key",
                            (id)kSecAttrServer : [self hashIt:keyData],
                            (id)kSecValueData : keyData,
                            };
   return [OTEscrowKeys setKeyMaterialInKeychain:query error:error];
}

@end
#endif
