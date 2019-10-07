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

#import "OTBottledPeer.h"

#if OCTAGON
#import <SecurityFoundation/SFEncryptionOperation.h>
#import <SecurityFoundation/SFSigningOperation.h>
#import <SecurityFoundation/SFDigestOperation.h>
#import <SecurityFoundation/SFKey.h>
#import <SecurityFoundation/SFKey_Private.h>

#import <Security/SecKeyPriv.h>

#import <corecrypto/cchkdf.h>
#import <corecrypto/ccsha2.h>
#import <corecrypto/ccec.h>

#import <utilities/debugging.h>

#import <CommonCrypto/CommonRandomSPI.h>

#if 0
#import "OTBottle.h"
#import "OTBottleContents.h"
#endif

#import "OTDefines.h"
#import "OTPrivateKey.h"
#import "OTPrivateKey+SF.h"
#import "OTAuthenticatedCiphertext.h"
#import "OTAuthenticatedCiphertext+SF.h"

@interface OTBottledPeer ()

@property (nonatomic, strong) NSString* peerID;
@property (nonatomic, strong) NSString* spID;
@property (nonatomic, strong) SFECKeyPair* peerSigningKey;
@property (nonatomic, strong) SFECKeyPair* peerEncryptionKey;
@property (nonatomic, strong) NSData* data;

@end

@implementation OTBottledPeer

+ (SFAuthenticatedEncryptionOperation *) encryptionOperation
{
    SFAESKeySpecifier *keySpecifier = [[SFAESKeySpecifier alloc] initWithBitSize:SFAESKeyBitSize256];
    return [[SFAuthenticatedEncryptionOperation alloc] initWithKeySpecifier:keySpecifier];
}

// Given a peer's details including private key material, and
// the keys generated from the escrow secret, encrypt the peer private keys,
// make a bottled peer object and serialize it into data.
- (nullable instancetype) initWithPeerID:(NSString * _Nullable)peerID
                                    spID:(NSString * _Nullable)spID
                          peerSigningKey:(SFECKeyPair *)peerSigningKey
                       peerEncryptionKey:(SFECKeyPair *)peerEncryptionKey
                              escrowKeys:(OTEscrowKeys *)escrowKeys
                                   error:(NSError**)error
{
    self = [super init];
    if (self) {
#if 0
        // Serialize the peer private keys into "contents"
        OTBottleContents *contentsObj = [[OTBottleContents alloc] init];
        contentsObj.peerSigningPrivKey = [OTPrivateKey fromECKeyPair:peerSigningKey];
        contentsObj.peerEncryptionPrivKey = [OTPrivateKey fromECKeyPair:peerEncryptionKey];
        NSData *clearContentsData = contentsObj.data;

        // Encrypt the contents
        SFAuthenticatedEncryptionOperation *op = [OTBottledPeer encryptionOperation];
        SFAuthenticatedCiphertext* cipher = [op encrypt:clearContentsData withKey:escrowKeys.symmetricKey error:error];
        if (!cipher) {
            return nil;
        }
        
        // Serialize the whole thing
        OTBottle *obj = [[OTBottle alloc] init];
        obj.peerID = peerID;
        obj.spID = spID;
        obj.escrowedSigningSPKI    = [escrowKeys.signingKey.publicKey encodeSubjectPublicKeyInfo];
        obj.escrowedEncryptionSPKI = [escrowKeys.encryptionKey.publicKey encodeSubjectPublicKeyInfo];
        obj.peerSigningSPKI        = [peerSigningKey.publicKey encodeSubjectPublicKeyInfo];
        obj.peerEncryptionSPKI     = [peerEncryptionKey.publicKey encodeSubjectPublicKeyInfo];
        obj.contents = [OTAuthenticatedCiphertext fromSFAuthenticatedCiphertext:cipher];

        _peerID = [peerID copy];
        _spID = [spID copy];
        _peerSigningKey = peerSigningKey;
        _peerEncryptionKey = peerEncryptionKey;
        _data = obj.data;
#endif
    }
    return self;
}

// Deserialize a bottle and decrypt the contents (peer keys)
// using the keys generated from the escrow secret.
- (nullable instancetype) initWithData:(NSData *)data
                            escrowKeys:(OTEscrowKeys *)escrowKeys
                                 error:(NSError**)error
{
    self = [super init];
    if (self) {
#if 0
        NSError* localError =nil;
        // Deserialize the whole thing
        OTBottle *obj = [[OTBottle alloc] initWithData:data];
        if (!obj) {
            secerror("octagon: failed to deserialize data into OTBottle");
            if(error){
                *error = [NSError errorWithDomain:OctagonErrorDomain code:OTErrorDeserializationFailure userInfo:@{NSLocalizedDescriptionKey: @"Failed to deserialize bottle peer"}];
            }
            return nil;


        // Decrypt contents
        SFAuthenticatedEncryptionOperation *op = [OTBottledPeer encryptionOperation];
        SFAuthenticatedCiphertext* ciphertext = [obj.contents asSFAuthenticatedCiphertext];
        NSData* clearContentsData = [op decrypt:ciphertext withKey:escrowKeys.symmetricKey error:&localError];
        if (!clearContentsData || clearContentsData.length == 0) {
            secerror("octagon: could not decrypt bottle contents: %@", localError);
            if(error){
                *error = localError;
            }
            return nil;
        }
        
        // Deserialize contents into private peer keys
        OTBottleContents *contentsObj = [[OTBottleContents alloc] initWithData:clearContentsData];
        if (!contentsObj) {
            secerror("octagon: could not deserialize bottle contents");
            if(error){
                *error = [NSError errorWithDomain:OctagonErrorDomain code:OTErrorDeserializationFailure userInfo:@{NSLocalizedDescriptionKey: @"Failed to deserialize bottle contents"}];
            }
            return nil;
        }
        
        _peerID = obj.peerID;
        _spID = obj.spID;
        if (self.keyType != OTPrivateKey_KeyType_EC_NIST_CURVES) {
            return nil;
        }
        return [[SFECKeyPair alloc] initWithSecKey:[EscrowKeys createSecKey:self.keyData]];

        _peerSigningKey    = [contentsObj.peerSigningPrivKey asECKeyPair];
        _peerEncryptionKey = [contentsObj.peerEncryptionPrivKey asECKeyPair];
        if (!_peerSigningKey || !_peerEncryptionKey) {
            secerror("octagon: could not get private EC keys from bottle contents");
            if(error){
                *error = [NSError errorWithDomain:OctagonErrorDomain code:OTErrorPrivateKeyFailure userInfo:@{NSLocalizedDescriptionKey: @"Failed to instantiate octagon peer keys"}];
            }
            return nil;
        }
        _data = [data copy];
        
        SFECPublicKey *peerSigningPubKey = [SFECPublicKey keyWithSubjectPublicKeyInfo:obj.peerSigningSPKI];
        SFECPublicKey *peerEncryptionPubKey = [SFECPublicKey keyWithSubjectPublicKeyInfo:obj.peerEncryptionSPKI];
        
        // Check the private keys match the public keys
        if (![_peerSigningKey.publicKey isEqual:peerSigningPubKey]) {
            secerror("octagon: public and private peer signing keys do not match");
            if(error){
                *error = [NSError errorWithDomain:OctagonErrorDomain code:OTErrorPrivateKeyFailure userInfo:@{NSLocalizedDescriptionKey: @"public and private peer signing keys do not match"}];
            }
            return nil;
        }
        if (![_peerEncryptionKey.publicKey isEqual:peerEncryptionPubKey]) {
            secerror("octagon: public and private peer encryption keys do not match");
            if(error){
                *error = [NSError errorWithDomain:OctagonErrorDomain code:OTErrorPrivateKeyFailure userInfo:@{NSLocalizedDescriptionKey: @"public and private peer encryption keys do not match"}];
            }
            return nil;
        }
#endif
    }
    return self;
}

@end

#endif


