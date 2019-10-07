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

#import "OTIdentity.h"

#import <SecurityFoundation/SFKey.h>
#import <SecurityFoundation/SFKey_Private.h>
#import "keychain/ot/OTDefines.h"

#import "keychain/SecureObjectSync/SOSAccountTransaction.h"
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#import "keychain/SecureObjectSync/SOSAccount.h"
#pragma clang diagnostic pop

@interface OTIdentity ()

@property (nonatomic, strong) NSString*     peerID;
@property (nonatomic, strong) NSString*     spID;
@property (nonatomic, strong) SFECKeyPair*  peerSigningKey;
@property (nonatomic, strong) SFECKeyPair*  peerEncryptionKey;

@end

@implementation OTIdentity

- (instancetype) initWithPeerID:(nullable NSString*)peerID
                           spID:(nullable NSString*)spID
                 peerSigningKey:(SFECKeyPair*)peerSigningKey
              peerEncryptionkey:(SFECKeyPair*)peerEncryptionKey
                          error:(NSError**)error
{
    self = [super init];
    if (self) {
        _peerID = peerID;
        _spID = spID;
        _peerSigningKey = peerSigningKey;
        _peerEncryptionKey = peerEncryptionKey;
    }
    return self;
}

+ (nullable instancetype) currentIdentityFromSOS:(NSError**)error
{
    CFErrorRef circleCheckError = NULL;
    SOSCCStatus circleStatus = SOSCCThisDeviceIsInCircle(&circleCheckError);
    if(circleStatus != kSOSCCInCircle){
        if(circleCheckError){
            secerror("octagon: cannot retrieve octagon keys from SOS, not in circle, error: %@", circleCheckError);
            if(error){
                *error = (__bridge NSError*)circleCheckError;
            }
        }
        secerror("octagon: current circle status: %d",circleStatus);
        return nil;
    }
    __block NSString* sosPeerID = nil;
    __block NSError* sosPeerIDError = nil;

    SOSCCPerformWithPeerID(^(CFStringRef peerID, CFErrorRef error) {
        sosPeerID = (__bridge NSString *)(peerID);
        if(error){
            secerror("octagon: retrieving sos peer id error: %@", error);
            sosPeerIDError = CFBridgingRelease(error);
        }
    });

    if(sosPeerID == nil || sosPeerIDError != nil){
        secerror("octagon: cannot retrieve peer id from SOS, error: %@", sosPeerIDError);
        if(error){
            *error = sosPeerIDError;
        }
        return nil;
    }

    __block SFECKeyPair *peerEncryptionKey;
    __block SFECKeyPair *peerSigningKey;
    __block NSError* localError = nil;
    
    SOSCCPerformWithAllOctagonKeys(^(SecKeyRef octagonEncryptionKey, SecKeyRef octagonSigningKey, CFErrorRef cferror) {
        if(cferror) {
            localError = (__bridge NSError*)cferror;
            return;
        }
        if (!cferror && octagonEncryptionKey && octagonSigningKey) {
            peerSigningKey = [[SFECKeyPair alloc] initWithSecKey:octagonSigningKey];
            peerEncryptionKey = [[SFECKeyPair alloc] initWithSecKey:octagonEncryptionKey];
            
        }
    });
    
    if(!peerEncryptionKey || !peerSigningKey || localError != nil){
        secerror("octagon: failed to retrieve octagon keys from sos: %@", localError);
        if(error){
            *error = localError;
        }
        return nil;
    }
    return [[OTIdentity alloc] initWithPeerID:nil
                                         spID:sosPeerID
                               peerSigningKey:peerSigningKey
                            peerEncryptionkey:peerEncryptionKey
                                        error:error];
}

-(BOOL)isEqual:(OTIdentity*)identity
{
    return [self.peerID isEqualToString:identity.peerID] &&
    [self.spID isEqualToString:identity.spID] &&
    [self.peerSigningKey isEqual:identity.peerSigningKey] &&
    [self.peerEncryptionKey isEqual:identity.peerEncryptionKey];
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

+ (BOOL)storeOtagonKey:(NSData*)keyData
                      octagonKeyType:(OctagonKeyType)octagonKeyType
               restoredPeerID:(NSString*)restoredPeerID
      escrowSigningPubKeyHash:(NSString*)escrowSigningPubKeyHash
                 error:(NSError**)error
{
    NSNumber *keyType = [[NSNumber alloc]initWithInt:octagonKeyType];

    NSDictionary* query = @{
                            (id)kSecClass : (id)kSecClassInternetPassword,
                            (id)kSecAttrAccessible: (id)kSecAttrAccessibleWhenUnlocked,
                            (id)kSecUseDataProtectionKeychain : @YES,
                            (id)kSecAttrLabel : escrowSigningPubKeyHash,
                            (id)kSecAttrAccount : restoredPeerID,
                            (id)kSecAttrType : keyType,
                            (id)kSecAttrServer : (octagonKeyType == 1) ? @"Octagon Signing Key" : @"Octagon Encryption Key",
                            (id)kSecAttrAccessGroup: @"com.apple.security.ckks",
                            (id)kSecAttrSynchronizable : (id)kCFBooleanFalse,
                            (id)kSecValueData : keyData,
                            };
    return [OTIdentity setKeyMaterialInKeychain:query error:error];

}

+(BOOL) storeOctagonIdentityIntoKeychain:(_SFECKeyPair *)restoredSigningKey
                   restoredEncryptionKey:(_SFECKeyPair *)restoredEncryptionKey
                 escrowSigningPubKeyHash:(NSString *)escrowSigningPubKeyHash
                          restoredPeerID:(NSString *)peerID
                                   error:(NSError**)error
{
    NSError* localError = nil;

    BOOL result = [OTIdentity storeOtagonKey:[restoredSigningKey keyData] octagonKeyType:OctagonSigningKey restoredPeerID:peerID escrowSigningPubKeyHash:escrowSigningPubKeyHash error:&localError];
    if(!result || localError){
        secerror("octagon: could not store octagon signing key in keychain:%@", localError);
        if(error){
            *error = localError;
        }
        return NO;
    }
    result = [OTIdentity storeOtagonKey:[restoredEncryptionKey keyData] octagonKeyType:OctagonEncryptionKey restoredPeerID:peerID escrowSigningPubKeyHash:escrowSigningPubKeyHash error:&localError];
    if(!result || localError){
        secerror("octagon: could not store octagon encryption key in keychain:%@", localError);
        if(error){
            *error = localError;
        }
        return NO;
    }
    return result;
}

@end
#endif
