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

#import "keychain/ckks/CKKSTLKShare.h"

#import <Foundation/NSKeyedArchiver_Private.h>
#import <SecurityFoundation/SFDigestOperation.h>
#import <SecurityFoundation/SFEncryptionOperation.h>
#import <SecurityFoundation/SFKey.h>
#import <SecurityFoundation/SFSigningOperation.h>

#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ckks/CKKSPeer.h"
#import "keychain/ckks/CloudKitCategories.h"

@interface CKKSTLKShare ()
@end

@implementation CKKSTLKShare
- (instancetype)init:(CKKSKeychainBackedKey*)key
              sender:(id<CKKSSelfPeer>)sender
            receiver:(id<CKKSPeer>)receiver
               curve:(SFEllipticCurve)curve
             version:(SecCKKSTLKShareVersion)version
               epoch:(NSInteger)epoch
            poisoned:(NSInteger)poisoned
              zoneID:(CKRecordZoneID*)zoneID
{
    if((self = [super init])) {
        _zoneID = zoneID;

        _curve = curve;
        _version = version;
        _tlkUUID = key.uuid;

        _receiverPeerID = receiver.peerID;
        _receiverPublicEncryptionKeySPKI = receiver.publicEncryptionKey.keyData;

        _senderPeerID = sender.peerID;

        _epoch = epoch;
        _poisoned = poisoned;
    }
    return self;
}

- (instancetype)initForKey:(NSString*)tlkUUID
              senderPeerID:(NSString*)senderPeerID
            recieverPeerID:(NSString*)receiverPeerID
  receiverEncPublicKeySPKI:(NSData* _Nullable)publicKeySPKI
                     curve:(SFEllipticCurve)curve
                   version:(SecCKKSTLKShareVersion)version
                     epoch:(NSInteger)epoch
                  poisoned:(NSInteger)poisoned
                wrappedKey:(NSData*)wrappedKey
                 signature:(NSData*)signature
                    zoneID:(CKRecordZoneID*)zoneID
{
    if((self = [super init])) {
        _zoneID = zoneID;
        _tlkUUID = tlkUUID;
        _senderPeerID = senderPeerID;

        _receiverPeerID = receiverPeerID;
        _receiverPublicEncryptionKeySPKI = publicKeySPKI;

        _curve = curve;
        _version = version;
        _epoch = epoch;
        _poisoned = poisoned;

        _wrappedTLK = wrappedKey;
        _signature = signature;
    }
    return self;
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"<CKKSTLKShareCore(%@): recv:%@ send:%@>",
                                      self.tlkUUID,
                                      self.receiverPeerID,
                                      self.senderPeerID];
}

- (NSData*)wrap:(CKKSKeychainBackedKey*)key
      publicKey:(SFECPublicKey*)receiverPublicKey
          error:(NSError* __autoreleasing*)error
{
    NSData* plaintext = [key serializeAsProtobuf:error];
    if(!plaintext) {
        return nil;
    }

    SFIESOperation* sfieso = [[SFIESOperation alloc] initWithCurve:self.curve];
    SFIESCiphertext* ciphertext =
        [sfieso encrypt:plaintext withKey:receiverPublicKey error:error];

    // Now use NSCoding to turn the ciphertext into something transportable
    NSKeyedArchiver* archiver = [[NSKeyedArchiver alloc] initRequiringSecureCoding:YES];
    [ciphertext encodeWithCoder:archiver];

    return archiver.encodedData;
}

- (CKKSKeychainBackedKey* _Nullable)unwrapUsing:(id<CKKSSelfPeer>)localPeer
                                          error:(NSError* __autoreleasing*)error
{
    // Unwrap the ciphertext using NSSecureCoding
    NSKeyedUnarchiver* coder =
        [[NSKeyedUnarchiver alloc] initForReadingFromData:self.wrappedTLK error:nil];
    SFIESCiphertext* ciphertext = [[SFIESCiphertext alloc] initWithCoder:coder];
    [coder finishDecoding];

    SFIESOperation* sfieso = [[SFIESOperation alloc] initWithCurve:self.curve];

    NSError* localerror = nil;
    NSData* plaintext =
        [sfieso decrypt:ciphertext withKey:localPeer.encryptionKey error:&localerror];
    if(!plaintext || localerror) {
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    return [CKKSKeychainBackedKey loadFromProtobuf:plaintext error:error];
}

// Serialize this record in some format suitable for signing.
// This record must serialize exactly the same on the other side for the signature to verify.
- (NSData*)dataForSigning:(CKRecord* _Nullable)record
{
    // Ideally, we'd put this as DER or some other structured, versioned format.
    // For now, though, do the straightforward thing and concatenate the fields of interest.
    NSMutableData* dataToSign = [[NSMutableData alloc] init];

    uint64_t version = OSSwapHostToLittleConstInt64(self.version);
    [dataToSign appendBytes:&version length:sizeof(version)];

    // We only include the peer IDs in the signature; the receiver doesn't care if we signed the receiverPublicKey field;
    // if it's wrong or doesn't match, the receiver will simply fail to decrypt the encrypted record.
    [dataToSign appendData:[self.receiverPeerID dataUsingEncoding:NSUTF8StringEncoding]];
    [dataToSign appendData:[self.senderPeerID dataUsingEncoding:NSUTF8StringEncoding]];

    [dataToSign appendData:self.wrappedTLK];

    uint64_t curve = OSSwapHostToLittleConstInt64(self.curve);
    [dataToSign appendBytes:&curve length:sizeof(curve)];

    uint64_t epoch = OSSwapHostToLittleConstInt64(self.epoch);
    [dataToSign appendBytes:&epoch length:sizeof(epoch)];

    uint64_t poisoned = OSSwapHostToLittleConstInt64(self.poisoned);
    [dataToSign appendBytes:&poisoned length:sizeof(poisoned)];

    // If we have a CKRecord passed in, add any unknown fields (that don't start with server_) to the signed data
    // in sorted order by CKRecord key
    if(record) {
        NSMutableDictionary<NSString*, id>* extraData = [NSMutableDictionary dictionary];

        for(NSString* key in record.allKeys) {
            if([key isEqualToString:SecCKRecordSenderPeerID] ||
               [key isEqualToString:SecCKRecordReceiverPeerID] ||
               [key isEqualToString:SecCKRecordReceiverPublicEncryptionKey] ||
               [key isEqualToString:SecCKRecordCurve] || [key isEqualToString:SecCKRecordEpoch] ||
               [key isEqualToString:SecCKRecordPoisoned] ||
               [key isEqualToString:SecCKRecordSignature] || [key isEqualToString:SecCKRecordVersion] ||
               [key isEqualToString:SecCKRecordParentKeyRefKey] ||
               [key isEqualToString:SecCKRecordWrappedKeyKey]) {
                // This version of CKKS knows about this data field. Ignore them with prejudice.
                continue;
            }

            if([key hasPrefix:@"server_"]) {
                // Ignore all fields prefixed by "server_"
                continue;
            }

            extraData[key] = record[key];
        }

        NSArray* extraKeys = [[extraData allKeys] sortedArrayUsingSelector:@selector(compare:)];
        for(NSString* extraKey in extraKeys) {
            id obj = extraData[extraKey];

            // Skip CKReferences, NSArray, CLLocation, and CKAsset.
            if([obj isKindOfClass:[NSString class]]) {
                [dataToSign appendData:[obj dataUsingEncoding:NSUTF8StringEncoding]];
            } else if([obj isKindOfClass:[NSData class]]) {
                [dataToSign appendData:obj];
            } else if([obj isKindOfClass:[NSDate class]]) {
                NSISO8601DateFormatter* formatter = [[NSISO8601DateFormatter alloc] init];
                NSString* str = [formatter stringForObjectValue:obj];
                [dataToSign appendData:[str dataUsingEncoding:NSUTF8StringEncoding]];
            } else if([obj isKindOfClass:[NSNumber class]]) {
                // Add an NSNumber
                uint64_t n64 = OSSwapHostToLittleConstInt64([obj unsignedLongLongValue]);
                [dataToSign appendBytes:&n64 length:sizeof(n64)];
            }
        }
    }

    return dataToSign;
}

// Returns the signature, but not the signed data itself;
- (NSData* _Nullable)signRecord:(SFECKeyPair*)signingKey
                       ckrecord:(CKRecord* _Nullable)ckrecord
                          error:(NSError* __autoreleasing*)error
{
    // TODO: the digest operation can't be changed, as we don't have a good way of communicating it, like self.curve
    SFEC_X962SigningOperation* xso = [[SFEC_X962SigningOperation alloc]
        initWithKeySpecifier:[[SFECKeySpecifier alloc] initWithCurve:self.curve]
             digestOperation:[[SFSHA256DigestOperation alloc] init]];

    NSData* data = [self dataForSigning:ckrecord];
    SFSignedData* signedData = [xso sign:data withKey:signingKey error:error];

    return signedData.signature;
}

- (bool)verifySignature:(NSData*)signature
          verifyingPeer:(id<CKKSPeer>)peer
               ckrecord:(CKRecord* _Nullable)ckrecord
                  error:(NSError* __autoreleasing*)error
{
    if(!peer.publicSigningKey) {
        ckkserror("ckksshare", self.zoneID, "no signing key for peer: %@", peer);
        if(error) {
            *error = [NSError
                errorWithDomain:CKKSErrorDomain
                           code:CKKSNoSigningKey
                    description:[NSString stringWithFormat:@"Peer(%@) has no signing key", peer]];
        }
        return false;
    }

    // TODO: the digest operation can't be changed, as we don't have a good way of communicating it, like self.curve
    SFEC_X962SigningOperation* xso = [[SFEC_X962SigningOperation alloc]
        initWithKeySpecifier:[[SFECKeySpecifier alloc] initWithCurve:self.curve]
             digestOperation:[[SFSHA256DigestOperation alloc] init]];
    SFSignedData* signedData =
        [[SFSignedData alloc] initWithData:[self dataForSigning:ckrecord] signature:signature];

    bool ret = [xso verify:signedData withKey:peer.publicSigningKey error:error];
    return ret;
}

- (bool)signatureVerifiesWithPeerSet:(NSSet<id<CKKSPeer>>*)peerSet
                            ckrecord:(CKRecord* _Nullable)ckrecord
                               error:(NSError**)error
{
    NSError* lastVerificationError = nil;
    for(id<CKKSPeer> peer in peerSet) {
        if([peer.peerID isEqualToString:self.senderPeerID]) {
            // Does the signature verify using this peer?
            NSError* localerror = nil;
            bool isSigned = [self verifySignature:self.signature
                                    verifyingPeer:peer
                                         ckrecord:ckrecord
                                            error:&localerror];
            if(localerror) {
                ckkserror("ckksshare", self.zoneID, "signature didn't verify for %@ %@: %@", self, peer, localerror);
                lastVerificationError = localerror;
            }
            if(isSigned) {
                return true;
            }
        }
    }

    if(error) {
        if(lastVerificationError) {
            *error = lastVerificationError;
        } else {
            *error = [NSError
                errorWithDomain:CKKSErrorDomain
                           code:CKKSNoTrustedTLKShares
                    description:[NSString stringWithFormat:@"No TLK share from %@", self.senderPeerID]];
        }
    }
    return false;
}

- (instancetype)copyWithZone:(NSZone*)zone
{
    CKKSTLKShare* share = [[[self class] allocWithZone:zone] init];
    share.curve = self.curve;
    share.version = self.version;
    share.tlkUUID = [self.tlkUUID copy];
    share.senderPeerID = [self.senderPeerID copy];
    share.epoch = self.epoch;
    share.poisoned = self.poisoned;
    share.wrappedTLK = [self.wrappedTLK copy];
    share.signature = [self.signature copy];

    share.receiverPeerID = [self.receiverPeerID copy];
    share.receiverPublicEncryptionKeySPKI = [self.receiverPublicEncryptionKeySPKI copy];

    return share;
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (void)encodeWithCoder:(nonnull NSCoder*)coder
{
    [coder encodeObject:self.zoneID forKey:@"zoneID"];
    [coder encodeInt64:(int64_t)self.curve forKey:@"curve"];
    [coder encodeInt64:self.version forKey:@"version"];
    [coder encodeObject:self.tlkUUID forKey:@"tlkUUID"];
    [coder encodeObject:self.senderPeerID forKey:@"senderPeerID"];
    [coder encodeInt64:self.epoch forKey:@"epoch"];
    [coder encodeInt64:self.poisoned forKey:@"poisoned"];
    [coder encodeObject:self.wrappedTLK forKey:@"wrappedTLK"];
    [coder encodeObject:self.signature forKey:@"signature"];

    [coder encodeObject:self.receiverPeerID forKey:@"receiverPeerID"];
    [coder encodeObject:self.receiverPublicEncryptionKeySPKI forKey:@"receiverSPKI"];
}

- (nullable instancetype)initWithCoder:(nonnull NSCoder*)decoder
{
    if ((self = [super init])) {
        _zoneID = [decoder decodeObjectOfClass:[CKRecordZoneID class] forKey:@"zoneID"];
        _curve = (SFEllipticCurve) [decoder decodeInt64ForKey:@"curve"];
        _version = (SecCKKSTLKShareVersion)[decoder decodeInt64ForKey:@"version"];
        _tlkUUID = [decoder decodeObjectOfClass:[NSString class] forKey:@"tlkUUID"];
        _senderPeerID = [decoder decodeObjectOfClass:[NSString class] forKey:@"senderPeerID"];
        _epoch = (NSInteger)[decoder decodeInt64ForKey:@"epoch"];
        _poisoned = (NSInteger)[decoder decodeInt64ForKey:@"poisoned"];
        _wrappedTLK = [decoder decodeObjectOfClass:[NSData class] forKey:@"wrappedTLK"];
        _signature = [decoder decodeObjectOfClass:[NSData class] forKey:@"signature"];


        _receiverPeerID = [decoder decodeObjectOfClass:[NSString class] forKey:@"receiverPeerID"];
        _receiverPublicEncryptionKeySPKI = [decoder decodeObjectOfClass:[NSData class] forKey:@"receiverSPKI"];
    }
    return self;
}

- (BOOL)isEqual:(id)object
{
    if(![object isKindOfClass:[CKKSTLKShare class]]) {
        return NO;
    }

    CKKSTLKShare* obj = (CKKSTLKShare*)object;

    return ([self.tlkUUID isEqualToString:obj.tlkUUID] && [self.zoneID isEqual:obj.zoneID] &&
            [self.senderPeerID isEqualToString:obj.senderPeerID] &&
            ((self.receiverPeerID == nil && obj.receiverPeerID == nil) ||
             [self.receiverPeerID isEqual:obj.receiverPeerID]) &&
            ((self.receiverPublicEncryptionKeySPKI == nil && obj.receiverPublicEncryptionKeySPKI == nil) ||
             [self.receiverPublicEncryptionKeySPKI isEqual:obj.receiverPublicEncryptionKeySPKI]) &&
            self.epoch == obj.epoch && self.curve == obj.curve && self.poisoned == obj.poisoned &&
            ((self.wrappedTLK == nil && obj.wrappedTLK == nil) || [self.wrappedTLK isEqual:obj.wrappedTLK]) &&
            ((self.signature == nil && obj.signature == nil) || [self.signature isEqual:obj.signature]) && true)
               ? YES
               : NO;
}

+ (CKKSTLKShare* _Nullable)share:(CKKSKeychainBackedKey*)key
                                  as:(id<CKKSSelfPeer>)sender
                                  to:(id<CKKSPeer>)receiver
                               epoch:(NSInteger)epoch
                            poisoned:(NSInteger)poisoned
                               error:(NSError* __autoreleasing*)error
{
    NSError* localerror = nil;
    CKKSTLKShare* share = [[CKKSTLKShare alloc] init:key
                                                      sender:sender
                                                    receiver:receiver
                                                       curve:SFEllipticCurveNistp384
                                                     version:SecCKKSTLKShareCurrentVersion
                                                       epoch:epoch
                                                    poisoned:poisoned
                                                      zoneID:key.zoneID];

    share.wrappedTLK =
        [share wrap:key publicKey:receiver.publicEncryptionKey error:&localerror];
    if(localerror) {
        ckkserror("ckksshare", key.zoneID, "couldn't share %@ (wrap failed): %@", key, localerror);
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    share.signature = [share signRecord:sender.signingKey
                               ckrecord:nil
                                 error:&localerror];
    if(localerror) {
        ckkserror("ckksshare", key.zoneID, "couldn't share %@ (signing failed): %@", key, localerror);
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    return share;
}

- (CKKSKeychainBackedKey* _Nullable)recoverTLK:(id<CKKSSelfPeer>)recoverer
                                  trustedPeers:(NSSet<id<CKKSPeer>>*)peers
                                      ckrecord:(CKRecord* _Nullable)ckrecord
                                         error:(NSError* __autoreleasing*)error
{
    NSError* localerror = nil;

    id<CKKSPeer> peer = nil;
    for(id<CKKSPeer> p in peers) {
        if([p.peerID isEqualToString:self.senderPeerID]) {
            peer = p;
        }
    }

    if(!peer) {
        localerror = [NSError
            errorWithDomain:CKKSErrorDomain
                       code:CKKSNoTrustedPeer
                description:[NSString stringWithFormat:@"No trusted peer signed %@", self]];
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    bool isSigned = [self verifySignature:self.signature
                            verifyingPeer:peer
                                 ckrecord:ckrecord
                                    error:error];
    if(!isSigned) {
        return nil;
    }

    CKKSKeychainBackedKey* tlkTrial = [self unwrapUsing:recoverer error:error];
    if(!tlkTrial) {
        return nil;
    }

    if(![self.tlkUUID isEqualToString:tlkTrial.uuid]) {
        localerror = [NSError errorWithDomain:CKKSErrorDomain
                                         code:CKKSDataMismatch
                                  description:[NSString stringWithFormat:@"Signed UUID doesn't match unsigned UUID for %@",
                                                                         self]];
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    return tlkTrial;
}

@end

#endif  // OCTAGON
