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
#import <Foundation/Foundation.h>
#import "OTBottledPeer.h"
#import "OTBottledPeerSigned.h"
#import "SFPublicKey+SPKI.h"
#import "OTIdentity.h"

#import <SecurityFoundation/SFEncryptionOperation.h>
#import <SecurityFoundation/SFSigningOperation.h>
#import <SecurityFoundation/SFDigestOperation.h>
#import <SecurityFoundation/SFKey.h>
#import <SecurityFoundation/SFKey_Private.h>

#import <corecrypto/cchkdf.h>
#import <corecrypto/ccsha2.h>
#import <corecrypto/ccec.h>

#include <utilities/debugging.h>

@interface OTBottledPeerSigned ()
@property (nonatomic, strong) OTBottledPeer*  bp;
@property (nonatomic, strong) NSData*         signatureUsingEscrowKey;
@property (nonatomic, strong) NSData*         signatureUsingPeerKey;
@property (nonatomic, strong) NSData*         escrowSigningPublicKey;
@end

@implementation OTBottledPeerSigned

// Create signatures
- (nullable instancetype) initWithBottledPeer:(OTBottledPeer*)bp
                           escrowedSigningKey:(SFECKeyPair *)escrowedSigningKey
                               peerSigningKey:(SFECKeyPair *)peerSigningKey
                                        error:(NSError**)error
{
    self = [super init];
    if (self) {
        _bp = bp;
        _escrowSigningSPKI = [escrowedSigningKey.publicKey asSPKI];
        SFEC_X962SigningOperation* xso = [OTBottledPeerSigned signingOperation];
        _signatureUsingEscrowKey = [xso sign:bp.data withKey:escrowedSigningKey error:error].signature;
        if (!_signatureUsingEscrowKey) {
            return nil;
        }
        _signatureUsingPeerKey = [xso sign:bp.data withKey:peerSigningKey error:error].signature;
        if (!_signatureUsingPeerKey) {
            return nil;
        }
    }
    return self;
}

-(NSString*) escrowSigningPublicKeyHash
{
    const struct ccdigest_info *di = ccsha384_di();
    NSMutableData* result = [[NSMutableData alloc] initWithLength:ccsha384_di()->output_size];

    ccdigest(di, [self.escrowSigningPublicKey length], [self.escrowSigningPublicKey bytes], [result mutableBytes]);

    return [result base64EncodedStringWithOptions:0];
}

// Verify signatures, or return nil
- (nullable instancetype) initWithBottledPeer:(OTBottledPeer*)bp
                         signatureUsingEscrow:(NSData*)signatureUsingEscrow
                        signatureUsingPeerKey:(NSData*)signatureUsingPeerKey
                        escrowedSigningPubKey:(SFECPublicKey *)escrowedSigningPubKey
                                        error:(NSError**)error
{
    self = [super init];
    if (self) {
        _bp = bp;
        _escrowSigningSPKI = [escrowedSigningPubKey asSPKI];
        _signatureUsingPeerKey = signatureUsingPeerKey;
        _signatureUsingEscrowKey = signatureUsingEscrow;
        _escrowSigningPublicKey = [escrowedSigningPubKey keyData];

        SFEC_X962SigningOperation* xso = [OTBottledPeerSigned signingOperation];

        SFSignedData *escrowSigned = [[SFSignedData alloc] initWithData:bp.data signature:signatureUsingEscrow];
        if (![xso verify:escrowSigned withKey:escrowedSigningPubKey error:error]) {
            return nil;
        }
        SFSignedData *peerSigned = [[SFSignedData alloc] initWithData:bp.data signature:signatureUsingPeerKey];
        if (![xso verify:peerSigned withKey:bp.peerSigningKey.publicKey error:error]) {
            return nil;
        }
        //stuff restored keys in the keychain
        [OTIdentity storeOctagonIdentityIntoKeychain:self.bp.peerSigningKey restoredEncryptionKey:self.bp.peerEncryptionKey escrowSigningPubKeyHash:self.escrowSigningPublicKeyHash restoredPeerID:self.bp.spID error:error];
    }
    return self;
}

+ (SFEC_X962SigningOperation*) signingOperation
{
    SFECKeySpecifier *keySpecifier = [[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384];
    id<SFDigestOperation> digestOperation = [[SFSHA384DigestOperation alloc] init];
    return [[SFEC_X962SigningOperation alloc] initWithKeySpecifier:keySpecifier digestOperation:digestOperation];
}

+ (BOOL) verifyBottleSignature:(NSData*)data signature:(NSData*)signature key:(_SFECPublicKey*) pubKey error:(NSError**)error
{
    SFEC_X962SigningOperation* xso = [OTBottledPeerSigned signingOperation];

    SFSignedData *peerSigned = [[SFSignedData alloc] initWithData:data signature:signature];
    
    return ([xso verify:peerSigned withKey:pubKey error:error] != nil);

}

- (nullable instancetype) initWithBottledPeerRecord:(OTBottledPeerRecord *)record
                                         escrowKeys:(OTEscrowKeys *)escrowKeys
                                              error:(NSError**)error
{
    OTBottledPeer *bp = [[OTBottledPeer alloc] initWithData:record.bottle
                                                 escrowKeys:escrowKeys
                                                      error:error];
    if (!bp) {
        return nil;
    }
    return [self initWithBottledPeer:bp
                signatureUsingEscrow:record.signatureUsingEscrowKey
               signatureUsingPeerKey:record.signatureUsingPeerKey
               escrowedSigningPubKey:escrowKeys.signingKey.publicKey
                               error:error];
}

- (OTBottledPeerRecord *)asRecord:(NSString*)escrowRecordID
{
    OTBottledPeerRecord *rec = [[OTBottledPeerRecord alloc] init];
    rec.spID = self.bp.spID;
    rec.escrowRecordID = [escrowRecordID copy];
    rec.peerSigningSPKI = [self.bp.peerSigningKey.publicKey asSPKI];
    rec.escrowedSigningSPKI = self.escrowSigningSPKI;
    rec.bottle = self.bp.data;
    rec.signatureUsingPeerKey = self.signatureUsingPeerKey;
    rec.signatureUsingEscrowKey = self.signatureUsingEscrowKey;
    rec.launched = @"NO";
    return rec;
}

@end
#endif

