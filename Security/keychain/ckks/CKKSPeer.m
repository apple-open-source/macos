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

#import "keychain/ckks/CKKSPeer.h"
#import "keychain/ckks/CKKSViewManager.h"

NSString* const CKKSSOSPeerPrefix = @"spid-";

@implementation CKKSSelves
- (instancetype)initWithCurrent:(id<CKKSSelfPeer>)selfPeer
                      allSelves:(NSSet<id<CKKSSelfPeer>>*)allSelves {
    if((self = [super init])) {
        _currentSelf = selfPeer;

        // Ensure allSelves contains selfPeer
        _allSelves = allSelves ? [allSelves setByAddingObject:selfPeer] :
                        (selfPeer ? [NSSet setWithObject:selfPeer] : [NSSet set]);
    }
    return self;
}

- (NSString*)description {
    NSMutableSet* pastSelves = [self.allSelves mutableCopy];
    [pastSelves removeObject:self.currentSelf];
    return [NSString stringWithFormat:@"<CKKSSelves: %@ %@>", self.currentSelf, pastSelves.count == 0u ? @"(no past selves)" : pastSelves ];
}

@end

#pragma mark - CKKSActualPeer

@implementation CKKSActualPeer
- (NSString*)description {
    // Return the first 16 bytes of the public keys (for reading purposes)
    return [NSString stringWithFormat:@"<CKKSActualPeer(%@): pubEnc:%@ pubSign:%@ views:%d>",
           self.peerID,
           [self.publicEncryptionKey.keyData subdataWithRange:NSMakeRange(0, MIN(16u,self.publicEncryptionKey.keyData.length))],
           [self.publicSigningKey.keyData subdataWithRange:NSMakeRange(0, MIN(16u,self.publicSigningKey.keyData.length))],
           (int)self.viewList.count];
}

- (instancetype)initWithPeerID:(NSString*)syncingPeerID
           encryptionPublicKey:(SFECPublicKey*)encryptionKey
              signingPublicKey:(SFECPublicKey*)signingKey
                      viewList:(NSSet<NSString*>*)viewList
{
    if((self = [super init])) {
        _peerID = syncingPeerID;

        _publicEncryptionKey = encryptionKey;
        _publicSigningKey = signingKey;
        _viewList = viewList;
    }
    return self;
}

- (bool)matchesPeer:(id<CKKSPeer>)peer {
    return (self.peerID == nil && peer.peerID == nil) ||
            [self.peerID isEqualToString:peer.peerID];
}

- (BOOL)shouldHaveView:(NSString *)viewName
{
    return [self.viewList containsObject:viewName];
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (void)encodeWithCoder:(nonnull NSCoder*)coder
{
    [coder encodeObject:self.peerID forKey:@"peerID"];
    [coder encodeObject:self.publicEncryptionKey.encodeSubjectPublicKeyInfo forKey:@"encryptionKey"];
    [coder encodeObject:self.publicSigningKey.encodeSubjectPublicKeyInfo forKey:@"signingKey"];
    [coder encodeObject:self.viewList forKey:@"viewList"];
}

- (nullable instancetype)initWithCoder:(nonnull NSCoder*)decoder
{
    if ((self = [super init])) {
        _peerID = [decoder decodeObjectOfClass:[NSString class] forKey:@"peerID"];

        NSData* encryptionSPKI = [decoder decodeObjectOfClass:[NSData class] forKey:@"encryptionKey"];
        if(encryptionSPKI) {
            _publicEncryptionKey = [SFECPublicKey keyWithSubjectPublicKeyInfo:encryptionSPKI];
        }

        NSData* signingSPKI = [decoder decodeObjectOfClass:[NSData class] forKey:@"signingKey"];
        if(signingSPKI) {
            _publicSigningKey = [SFECPublicKey keyWithSubjectPublicKeyInfo:signingSPKI];
        }

        _viewList = [decoder decodeObjectOfClasses:[NSSet setWithArray:@[[NSSet class], [NSString class]]] forKey:@"viewList"];
    }
    return self;
}
@end

#pragma mark - CKKSSOSPeer

@interface CKKSSOSPeer ()
@property NSString* spid;
@property NSSet<NSString*>* viewList;
@end

@implementation CKKSSOSPeer
@synthesize publicEncryptionKey = _publicEncryptionKey;
@synthesize publicSigningKey = _publicSigningKey;

- (NSString*)description {
    // Return the first 16 bytes of the public keys (for reading purposes)
    return [NSString stringWithFormat:@"<CKKSSOSPeer(%@): pubEnc:%@ pubSign:%@ views:%d>",
           self.peerID,
           [self.publicEncryptionKey.keyData subdataWithRange:NSMakeRange(0, MIN(16u,self.publicEncryptionKey.keyData.length))],
           [self.publicSigningKey.keyData subdataWithRange:NSMakeRange(0, MIN(16u,self.publicSigningKey.keyData.length))],
           (int)self.viewList.count];
}

- (instancetype)initWithSOSPeerID:(NSString*)syncingPeerID
              encryptionPublicKey:(SFECPublicKey*)encryptionKey
                 signingPublicKey:(SFECPublicKey*)signingKey
                         viewList:(NSSet<NSString*>* _Nullable)viewList
{
    if((self = [super init])) {
        if([syncingPeerID hasPrefix:CKKSSOSPeerPrefix]) {
            _spid = [syncingPeerID  substringFromIndex:CKKSSOSPeerPrefix.length];
        } else {
            _spid = syncingPeerID;
        }
        _publicEncryptionKey = encryptionKey;
        _publicSigningKey = signingKey;
        _viewList = viewList;
    }
    return self;
}

- (NSString*)peerID {
    return [NSString stringWithFormat:@"%@%@", CKKSSOSPeerPrefix, self.spid];
}

- (bool)matchesPeer:(id<CKKSPeer>)peer {
    return (self.peerID == nil && peer.peerID == nil) ||
            [self.peerID isEqualToString:peer.peerID];
}

- (BOOL)shouldHaveView:(NSString *)viewName
{
    return [self.viewList containsObject:viewName];
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (void)encodeWithCoder:(nonnull NSCoder*)coder
{
    [coder encodeObject:self.spid forKey:@"spid"];
    [coder encodeObject:self.publicEncryptionKey.encodeSubjectPublicKeyInfo forKey:@"encryptionKey"];
    [coder encodeObject:self.publicSigningKey.encodeSubjectPublicKeyInfo forKey:@"signingKey"];
}

- (nullable instancetype)initWithCoder:(nonnull NSCoder*)decoder
{
    if ((self = [super init])) {
        _spid = [decoder decodeObjectOfClass:[NSString class] forKey:@"spid"];

        NSData* encryptionSPKI = [decoder decodeObjectOfClass:[NSData class] forKey:@"encryptionKey"];
        if(encryptionSPKI) {
            _publicEncryptionKey = [SFECPublicKey keyWithSubjectPublicKeyInfo:encryptionSPKI];
        }

        NSData* signingSPKI = [decoder decodeObjectOfClass:[NSData class] forKey:@"signingKey"];
        if(signingSPKI) {
            _publicSigningKey = [SFECPublicKey keyWithSubjectPublicKeyInfo:signingSPKI];
        }
    }
    return self;
}
@end

@interface CKKSSOSSelfPeer ()
@property NSString* spid;
@end

@implementation CKKSSOSSelfPeer
- (NSString*)description {
    return [NSString stringWithFormat:@"<CKKSSOSSelfPeer(%@): pubEnc:%@ pubSign:%@ views:%d>",
            self.peerID,
            [self.publicEncryptionKey.keyData subdataWithRange:NSMakeRange(0, MIN(16u,self.publicEncryptionKey.keyData.length))],
            [self.publicSigningKey.keyData subdataWithRange:NSMakeRange(0, MIN(16u,self.publicSigningKey.keyData.length))],
            (int)self.viewList.count];
}

- (instancetype)initWithSOSPeerID:(NSString*)syncingPeerID
                    encryptionKey:(SFECKeyPair*)encryptionKey
                       signingKey:(SFECKeyPair*)signingKey
                         viewList:(NSSet<NSString*>* _Nullable)viewList
{
    if((self = [super init])) {
        if([syncingPeerID hasPrefix:CKKSSOSPeerPrefix]) {
            _spid = [syncingPeerID  substringFromIndex:CKKSSOSPeerPrefix.length];
        } else {
            _spid = syncingPeerID;
        }
        _encryptionKey = encryptionKey;
        _signingKey = signingKey;
        _viewList = viewList;
    }
    return self;
}

-(SFECPublicKey*)publicEncryptionKey {
    return self.encryptionKey.publicKey;
}
-(SFECPublicKey*)publicSigningKey {
    return self.signingKey.publicKey;
}
- (NSString*)peerID {
    return [NSString stringWithFormat:@"%@%@", CKKSSOSPeerPrefix, self.spid];
}

- (bool)matchesPeer:(id<CKKSPeer>)peer {
    return (self.peerID == nil && peer.peerID == nil) ||
    [self.peerID isEqualToString:peer.peerID];
}

- (BOOL)shouldHaveView:(NSString *)viewName
{
    return [self.viewList containsObject:viewName];
}
@end

NSSet<Class>* CKKSPeerClasses(void)
{
    return [NSSet setWithArray:@[[CKKSSOSPeer class], [CKKSActualPeer class]]];
}

#endif // OCTAGON
