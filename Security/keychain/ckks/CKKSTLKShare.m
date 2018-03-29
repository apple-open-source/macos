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

#import <Foundation/NSKeyedArchiver_Private.h>

#import "keychain/ckks/CKKSTLKShare.h"
#import "keychain/ckks/CKKSPeer.h"
#import "keychain/ckks/CloudKitCategories.h"

#import <SecurityFoundation/SFKey.h>
#import <SecurityFoundation/SFEncryptionOperation.h>
#import <SecurityFoundation/SFSigningOperation.h>
#import <SecurityFoundation/SFDigestOperation.h>

@interface CKKSTLKShare ()
@end

@implementation CKKSTLKShare
-(instancetype)init:(CKKSKey*)key
             sender:(id<CKKSSelfPeer>)sender
           receiver:(id<CKKSPeer>)receiver
              curve:(SFEllipticCurve)curve
            version:(SecCKKSTLKShareVersion)version
              epoch:(NSInteger)epoch
           poisoned:(NSInteger)poisoned
             zoneID:(CKRecordZoneID*)zoneID
    encodedCKRecord:(NSData*)encodedCKRecord
{
    if((self = [super initWithCKRecordType:SecCKRecordTLKShareType
                           encodedCKRecord:encodedCKRecord
                                    zoneID:zoneID])) {
        _curve = curve;
        _version = version;
        _tlkUUID = key.uuid;

        _receiver = receiver;
        _senderPeerID = sender.peerID;

        _epoch = epoch;
        _poisoned = poisoned;
    }
    return self;
}

- (instancetype)initForKey:(NSString*)tlkUUID
              senderPeerID:(NSString*)senderPeerID
            recieverPeerID:(NSString*)receiverPeerID
      receiverEncPublicKey:(SFECPublicKey*)publicKey
                     curve:(SFEllipticCurve)curve
                   version:(SecCKKSTLKShareVersion)version
                     epoch:(NSInteger)epoch
                  poisoned:(NSInteger)poisoned
                wrappedKey:(NSData*)wrappedKey
                 signature:(NSData*)signature
                    zoneID:(CKRecordZoneID*)zoneID
           encodedCKRecord:(NSData*)encodedCKRecord
{
    if((self = [super initWithCKRecordType:SecCKRecordTLKShareType
                           encodedCKRecord:encodedCKRecord
                                    zoneID:zoneID])) {
        _tlkUUID = tlkUUID;
        _senderPeerID = senderPeerID;

        _receiver = [[CKKSSOSPeer alloc] initWithSOSPeerID:receiverPeerID encryptionPublicKey:publicKey signingPublicKey:nil];

        _curve = curve;
        _version = version;
        _epoch = epoch;
        _poisoned = poisoned;

        _wrappedTLK = wrappedKey;
        _signature = signature;
    }
    return self;
}

- (NSString*)description {
    return [NSString stringWithFormat:@"<CKKSTLKShare(%@): recv:%@ send:%@>", self.tlkUUID, self.receiver, self.senderPeerID];
}

- (NSData*)wrap:(CKKSKey*)key publicKey:(SFECPublicKey*)receiverPublicKey error:(NSError* __autoreleasing *)error {
    NSData* plaintext = [key serializeAsProtobuf:error];
    if(!plaintext) {
        return nil;
    }

    SFIESOperation* sfieso = [[SFIESOperation alloc] initWithCurve:self.curve];
    SFIESCiphertext* ciphertext = [sfieso encrypt:plaintext withKey:receiverPublicKey error:error];

    // Now use NSCoding to turn the ciphertext into something transportable
    NSKeyedArchiver *archiver = [[NSKeyedArchiver alloc] initRequiringSecureCoding:YES];
    [ciphertext encodeWithCoder:archiver];

    return archiver.encodedData;
}

- (CKKSKey*)unwrapUsing:(id<CKKSSelfPeer>)localPeer error:(NSError * __autoreleasing *)error {
    // Unwrap the ciphertext using NSSecureCoding
    NSKeyedUnarchiver *coder = [[NSKeyedUnarchiver alloc] initForReadingFromData:self.wrappedTLK error:nil];
    SFIESCiphertext* ciphertext = [[SFIESCiphertext alloc] initWithCoder:coder];
    [coder finishDecoding];

    SFIESOperation* sfieso = [[SFIESOperation alloc] initWithCurve:self.curve];

    NSError* localerror = nil;
    NSData* plaintext = [sfieso decrypt:ciphertext withKey:localPeer.encryptionKey error:&localerror];
    if(!plaintext || localerror) {
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    return [CKKSKey loadFromProtobuf:plaintext error:error];
}

// Serialize this record in some format suitable for signing.
// This record must serialize exactly the same on the other side for the signature to verify.
- (NSData*)dataForSigning {
    // Ideally, we'd put this as DER or some other structured, versioned format.
    // For now, though, do the straightforward thing and concatenate the fields of interest.
    NSMutableData* dataToSign = [[NSMutableData alloc] init];

    uint64_t version = OSSwapHostToLittleConstInt64(self.version);
    [dataToSign appendBytes:&version length:sizeof(version)];

    // We only include the peer IDs in the signature; the receiver doesn't care if we signed the receiverPublicKey field;
    // if it's wrong or doesn't match, the receiver will simply fail to decrypt the encrypted record.
    [dataToSign appendData:[self.receiver.peerID dataUsingEncoding:NSUTF8StringEncoding]];
    [dataToSign appendData:[self.senderPeerID dataUsingEncoding:NSUTF8StringEncoding]];

    [dataToSign appendData:self.wrappedTLK];

    uint64_t curve = OSSwapHostToLittleConstInt64(self.curve);
    [dataToSign appendBytes:&curve length:sizeof(curve)];

    uint64_t epoch = OSSwapHostToLittleConstInt64(self.epoch);
    [dataToSign appendBytes:&epoch length:sizeof(epoch)];

    uint64_t poisoned = OSSwapHostToLittleConstInt64(self.poisoned);
    [dataToSign appendBytes:&poisoned length:sizeof(poisoned)];

    // If we have a CKRecord saved here, add any unknown fields (that don't start with server_) to the signed data
    // in sorted order by CKRecord key
    CKRecord* record = self.storedCKRecord;
    if(record) {
        NSMutableDictionary<NSString*,id>* extraData = [NSMutableDictionary dictionary];

        for(NSString* key in record.allKeys) {
            if([key isEqualToString:SecCKRecordSenderPeerID] ||
               [key isEqualToString:SecCKRecordReceiverPeerID] ||
               [key isEqualToString:SecCKRecordReceiverPublicEncryptionKey] ||
               [key isEqualToString:SecCKRecordCurve] ||
               [key isEqualToString:SecCKRecordEpoch] ||
               [key isEqualToString:SecCKRecordPoisoned] ||
               [key isEqualToString:SecCKRecordSignature] ||
               [key isEqualToString:SecCKRecordVersion] ||
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
            if([obj isKindOfClass: [NSString class]]) {
                [dataToSign appendData: [obj dataUsingEncoding: NSUTF8StringEncoding]];
            } else if([obj isKindOfClass: [NSData class]]) {
                [dataToSign appendData: obj];
            } else if([obj isKindOfClass:[NSDate class]]) {
                NSISO8601DateFormatter *formatter = [[NSISO8601DateFormatter alloc] init];
                NSString* str = [formatter stringForObjectValue: obj];
                [dataToSign appendData: [str dataUsingEncoding: NSUTF8StringEncoding]];
            } else if([obj isKindOfClass: [NSNumber class]]) {
                // Add an NSNumber
                uint64_t n64 = OSSwapHostToLittleConstInt64([obj unsignedLongLongValue]);
                [dataToSign appendBytes:&n64 length:sizeof(n64)];
            }
        }
    }

    return dataToSign;
}

// Returns the signature, but not the signed data itself;
- (NSData*)signRecord:(SFECKeyPair*)signingKey error:(NSError* __autoreleasing *)error {
    // TODO: the digest operation can't be changed, as we don't have a good way of communicating it, like self.curve
    SFEC_X962SigningOperation* xso = [[SFEC_X962SigningOperation alloc] initWithKeySpecifier:[[SFECKeySpecifier alloc] initWithCurve:self.curve]
                                                                             digestOperation:[[SFSHA256DigestOperation alloc] init]];

    NSData* data = [self dataForSigning];
    SFSignedData* signedData = [xso sign:data withKey:signingKey error:error];

    return signedData.signature;
}

- (bool)verifySignature:(NSData*)signature verifyingPeer:(id<CKKSPeer>)peer error:(NSError* __autoreleasing *)error {
    if(!peer.publicSigningKey) {
        secerror("ckksshare: no signing key for peer: %@", peer);
        if(error) {
            *error = [NSError errorWithDomain:CKKSErrorDomain
                                         code:CKKSNoSigningKey
                                  description:[NSString stringWithFormat:@"Peer(%@) has no signing key", peer]];
        }
        return false;
    }

    // TODO: the digest operation can't be changed, as we don't have a good way of communicating it, like self.curve
    SFEC_X962SigningOperation* xso = [[SFEC_X962SigningOperation alloc] initWithKeySpecifier:[[SFECKeySpecifier alloc] initWithCurve:self.curve]
                                                                             digestOperation:[[SFSHA256DigestOperation alloc] init]];
    SFSignedData* signedData = [[SFSignedData alloc] initWithData:[self dataForSigning] signature:signature];

    bool ret = [xso verify:signedData withKey:peer.publicSigningKey error:error];
    return ret;
}

- (bool)signatureVerifiesWithPeerSet:(NSSet<id<CKKSPeer>>*)peerSet error:(NSError**)error {
    NSError* lastVerificationError = nil;
    for(id<CKKSPeer> peer in peerSet) {
        if([peer.peerID isEqualToString: self.senderPeerID]) {
            // Does the signature verify using this peer?
            NSError* localerror = nil;
            bool isSigned = [self verifySignature:self.signature verifyingPeer:peer error:&localerror];
            if(localerror) {
                secerror("ckksshare: signature didn't verify for %@ %@: %@", self, peer, localerror);
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
            *error = [NSError errorWithDomain:CKKSErrorDomain
                                         code:CKKSNoTrustedTLKShares
                                  description:[NSString stringWithFormat:@"No TLK share from %@", self.senderPeerID]];
        }
    }
    return false;
}

- (instancetype)copyWithZone:(NSZone *)zone {
    CKKSTLKShare* share = [[[self class] allocWithZone:zone] init];
    share.curve = self.curve;
    share.version = self.version;
    share.tlkUUID = [self.tlkUUID copy];
    share.senderPeerID = [self.senderPeerID copy];
    share.epoch = self.epoch;
    share.poisoned = self.poisoned;
    share.wrappedTLK = [self.wrappedTLK copy];
    share.signature = [self.signature copy];

    share.receiver = self.receiver;
    return share;
}

- (BOOL)isEqual:(id)object {
    if(![object isKindOfClass:[CKKSTLKShare class]]) {
        return NO;
    }

    CKKSTLKShare* obj = (CKKSTLKShare*) object;

    // Note that for purposes of CKKSTLK equality, we only care about the receiver's peer ID and publicEncryptionKey
    // <rdar://problem/34897551> SFKeys should support [isEqual:]
    return ([self.tlkUUID isEqualToString:obj.tlkUUID] &&
            [self.zoneID isEqual: obj.zoneID] &&
            [self.senderPeerID isEqualToString:obj.senderPeerID] &&
            ((self.receiver.peerID == nil && obj.receiver.peerID == nil) || [self.receiver.peerID isEqual: obj.receiver.peerID]) &&
            ((self.receiver.publicEncryptionKey == nil && obj.receiver.publicEncryptionKey == nil)
                || [self.receiver.publicEncryptionKey.keyData isEqual: obj.receiver.publicEncryptionKey.keyData]) &&
            self.epoch == obj.epoch &&
            self.curve == obj.curve &&
            self.poisoned == obj.poisoned &&
            ((self.wrappedTLK == nil && obj.wrappedTLK == nil) || [self.wrappedTLK isEqual: obj.wrappedTLK]) &&
            ((self.signature == nil && obj.signature == nil) || [self.signature isEqual: obj.signature]) &&
            true) ? YES : NO;
}

+ (CKKSTLKShare*)share:(CKKSKey*)key
                    as:(id<CKKSSelfPeer>)sender
                    to:(id<CKKSPeer>)receiver
                 epoch:(NSInteger)epoch
              poisoned:(NSInteger)poisoned
                 error:(NSError* __autoreleasing *)error
{
    NSError* localerror = nil;

    // Load any existing TLK Share, so we can update it
    CKKSTLKShare* oldShare =  [CKKSTLKShare tryFromDatabase:key.uuid
                                            receiverPeerID:receiver.peerID
                                              senderPeerID:sender.peerID
                                                    zoneID:key.zoneID
                                                     error:&localerror];
    if(localerror) {
        secerror("ckksshare: couldn't load old share for %@: %@", key, localerror);
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    CKKSTLKShare* share = [[CKKSTLKShare alloc] init:key
                                              sender:sender
                                            receiver:receiver
                                               curve:SFEllipticCurveNistp384
                                             version:SecCKKSTLKShareCurrentVersion
                                               epoch:epoch
                                            poisoned:poisoned
                                              zoneID:key.zoneID
                                     encodedCKRecord:oldShare.encodedCKRecord];

    share.wrappedTLK = [share wrap:key publicKey:receiver.publicEncryptionKey error:&localerror];
    if(localerror) {
        secerror("ckksshare: couldn't share %@ (wrap failed): %@", key, localerror);
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    share.signature = [share signRecord:sender.signingKey error:&localerror];
    if(localerror) {
        secerror("ckksshare: couldn't share %@ (signing failed): %@", key, localerror);
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    return share;
}

- (CKKSKey*)recoverTLK:(id<CKKSSelfPeer>)recoverer
          trustedPeers:(NSSet<id<CKKSPeer>>*)peers
                 error:(NSError* __autoreleasing *)error
{
    NSError* localerror = nil;

    id<CKKSPeer> peer = nil;
    for(id<CKKSPeer> p in peers) {
        if([p.peerID isEqualToString: self.senderPeerID]) {
            peer = p;
        }
    }

    if(!peer) {
        localerror = [NSError errorWithDomain:CKKSErrorDomain
                                         code:CKKSNoTrustedPeer
                                  description:[NSString stringWithFormat:@"No trusted peer signed %@", self]];
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    bool isSigned = [self verifySignature:self.signature verifyingPeer:peer error:error];
    if(!isSigned) {
        return nil;
    }

    CKKSKey* tlkTrial = [self unwrapUsing:recoverer error:error];
    if(!tlkTrial) {
        return nil;
    }

    if(![self.tlkUUID isEqualToString:tlkTrial.uuid]) {
        localerror = [NSError errorWithDomain:CKKSErrorDomain
                                         code:CKKSDataMismatch
                                  description:[NSString stringWithFormat:@"Signed UUID doesn't match unsigned UUID for %@", self]];
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    return tlkTrial;
}

#pragma mark - Database Operations

+ (instancetype)fromDatabase:(NSString*)uuid
             receiverPeerID:(NSString*)receiverPeerID
                senderPeerID:(NSString*)senderPeerID
                      zoneID:(CKRecordZoneID*)zoneID
                       error:(NSError * __autoreleasing *)error {
    return [self fromDatabaseWhere: @{@"uuid":CKKSNilToNSNull(uuid),
                                      @"recvpeerid":CKKSNilToNSNull(receiverPeerID),
                                      @"senderpeerid":CKKSNilToNSNull(senderPeerID),
                                      @"ckzone": CKKSNilToNSNull(zoneID.zoneName)} error:error];
}

+ (instancetype)tryFromDatabase:(NSString*)uuid
                receiverPeerID:(NSString*)receiverPeerID
                   senderPeerID:(NSString*)senderPeerID
                         zoneID:(CKRecordZoneID*)zoneID
                          error:(NSError * __autoreleasing *)error {
    return [self tryFromDatabaseWhere: @{@"uuid":CKKSNilToNSNull(uuid),
                                         @"recvpeerid":CKKSNilToNSNull(receiverPeerID),
                                         @"senderpeerid":CKKSNilToNSNull(senderPeerID),
                                         @"ckzone": CKKSNilToNSNull(zoneID.zoneName)} error:error];
}

+ (NSArray<CKKSTLKShare*>*)allFor:(NSString*)receiverPeerID
                          keyUUID:(NSString*)uuid
                           zoneID:(CKRecordZoneID*)zoneID
                            error:(NSError * __autoreleasing *)error {
    return [self allWhere:@{@"recvpeerid":CKKSNilToNSNull(receiverPeerID),
                            @"uuid":uuid,
                            @"ckzone": CKKSNilToNSNull(zoneID.zoneName)} error:error];
}

+ (NSArray<CKKSTLKShare*>*)allForUUID:(NSString*)uuid
                               zoneID:(CKRecordZoneID*)zoneID
                                error:(NSError * __autoreleasing *)error {
    return [self allWhere:@{@"uuid":CKKSNilToNSNull(uuid),
                            @"ckzone":CKKSNilToNSNull(zoneID.zoneName)} error:error];
}

+ (NSArray<CKKSTLKShare*>*)allInZone:(CKRecordZoneID*)zoneID
                               error:(NSError * __autoreleasing *)error {
    return [self allWhere:@{@"ckzone": CKKSNilToNSNull(zoneID.zoneName)} error:error];
}

+ (instancetype)tryFromDatabaseFromCKRecordID:(CKRecordID*)recordID
                                        error:(NSError * __autoreleasing *)error {
    // Welp. Try to parse!
    NSError *localerror = NULL;
    NSRegularExpression *regex = [NSRegularExpression regularExpressionWithPattern:@"^tlkshare-(?<uuid>[0-9A-Fa-f-]*)::(?<receiver>.*)::(?<sender>.*)$"
                                                                           options:NSRegularExpressionCaseInsensitive
                                                                             error:&localerror];
    if(localerror) {
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    NSTextCheckingResult* regexmatch = [regex firstMatchInString:recordID.recordName options:0 range:NSMakeRange(0, recordID.recordName.length)];
    if(!regexmatch) {
        if(error) {
            *error = [NSError errorWithDomain:CKKSErrorDomain
                                         code:CKKSNoSuchRecord
                                  description:[NSString stringWithFormat:@"Couldn't parse '%@' as a TLKShare ID", recordID.recordName]];
        }
        return nil;
    }

    NSString* uuid = [recordID.recordName substringWithRange:[regexmatch rangeWithName:@"uuid"]];
    NSString* receiver = [recordID.recordName substringWithRange:[regexmatch rangeWithName:@"receiver"]];
    NSString* sender = [recordID.recordName substringWithRange:[regexmatch rangeWithName:@"sender"]];

    return [self tryFromDatabaseWhere: @{@"uuid":CKKSNilToNSNull(uuid),
                                         @"recvpeerid":CKKSNilToNSNull(receiver),
                                         @"senderpeerid":CKKSNilToNSNull(sender),
                                         @"ckzone": CKKSNilToNSNull(recordID.zoneID.zoneName)} error:error];
}

#pragma mark - CKKSCKRecordHolder methods

+ (NSString*)ckrecordPrefix {
    return @"tlkshare";
}

- (NSString*)CKRecordName {
    return [NSString stringWithFormat:@"tlkshare-%@::%@::%@", self.tlkUUID, self.receiver.peerID, self.senderPeerID];
}

- (CKRecord*)updateCKRecord:(CKRecord*)record zoneID:(CKRecordZoneID*)zoneID {
    if(![record.recordID.recordName isEqualToString: [self CKRecordName]]) {
        @throw [NSException
                exceptionWithName:@"WrongCKRecordNameException"
                reason:[NSString stringWithFormat: @"CKRecord name (%@) was not %@", record.recordID.recordName, [self CKRecordName]]
                userInfo:nil];
    }
    if(![record.recordType isEqualToString: SecCKRecordTLKShareType]) {
        @throw [NSException
                exceptionWithName:@"WrongCKRecordTypeException"
                reason:[NSString stringWithFormat: @"CKRecordType (%@) was not %@", record.recordType, SecCKRecordTLKShareType]
                userInfo:nil];
    }

    record[SecCKRecordSenderPeerID] = self.senderPeerID;
    record[SecCKRecordReceiverPeerID] = self.receiver.peerID;
    record[SecCKRecordReceiverPublicEncryptionKey] = [self.receiver.publicEncryptionKey.keyData base64EncodedStringWithOptions:0];
    record[SecCKRecordCurve] = [NSNumber numberWithUnsignedInteger:(NSUInteger)self.curve];
    record[SecCKRecordVersion] = [NSNumber numberWithUnsignedInteger:(NSUInteger)self.version];
    record[SecCKRecordEpoch] = [NSNumber numberWithLong:(long)self.epoch];
    record[SecCKRecordPoisoned] = [NSNumber numberWithLong:(long)self.poisoned];

    record[SecCKRecordParentKeyRefKey] = [[CKReference alloc] initWithRecordID: [[CKRecordID alloc] initWithRecordName: self.tlkUUID zoneID: zoneID]
                                                                        action: CKReferenceActionValidate];

    record[SecCKRecordWrappedKeyKey] = [self.wrappedTLK base64EncodedStringWithOptions:0];
    record[SecCKRecordSignature] = [self.signature base64EncodedStringWithOptions:0];

    return record;
}

- (bool)matchesCKRecord:(CKRecord*)record {
    if(![record.recordType isEqualToString: SecCKRecordTLKShareType]) {
        return false;
    }

    if(![record.recordID.recordName isEqualToString: [self CKRecordName]]) {
        return false;
    }

    CKKSTLKShare* share = [[CKKSTLKShare alloc] initWithCKRecord:record];
    return [self isEqual: share];
}

- (void)setFromCKRecord: (CKRecord*) record {
    if(![record.recordType isEqualToString: SecCKRecordTLKShareType]) {
        @throw [NSException
                exceptionWithName:@"WrongCKRecordTypeException"
                reason:[NSString stringWithFormat: @"CKRecordType (%@) was not %@", record.recordType, SecCKRecordDeviceStateType]
                userInfo:nil];
    }

    [self setStoredCKRecord:record];

    self.senderPeerID = record[SecCKRecordSenderPeerID];
    self.curve = [record[SecCKRecordCurve] longValue]; // TODO: sanitize
    self.version = [record[SecCKRecordVersion] longValue];

    NSData* pubkeydata = CKKSUnbase64NullableString(record[SecCKRecordReceiverPublicEncryptionKey]);
    NSError* error = nil;
    SFECPublicKey* receiverPublicKey = pubkeydata ? [[SFECPublicKey alloc] initWithData:pubkeydata
                                                                              specifier:[[SFECKeySpecifier alloc] initWithCurve:self.curve]
                                                                                  error:&error] : nil;

    if(error) {
        ckkserror("ckksshare", record.recordID.zoneID, "Couldn't make public key from data: %@", error);
        receiverPublicKey = nil;
    }

    self.receiver = [[CKKSSOSPeer alloc] initWithSOSPeerID:record[SecCKRecordReceiverPeerID] encryptionPublicKey:receiverPublicKey signingPublicKey:nil];

    self.epoch = [record[SecCKRecordEpoch] longValue];
    self.poisoned = [record[SecCKRecordPoisoned] longValue];

    self.tlkUUID = ((CKReference*)record[SecCKRecordParentKeyRefKey]).recordID.recordName;

    self.wrappedTLK = CKKSUnbase64NullableString(record[SecCKRecordWrappedKeyKey]);
    self.signature = CKKSUnbase64NullableString(record[SecCKRecordSignature]);
}

#pragma mark - CKKSSQLDatabaseObject methods

+ (NSString*)sqlTable {
    return @"tlkshare";
}

+ (NSArray<NSString*>*)sqlColumns {
    return @[@"ckzone", @"uuid", @"senderpeerid", @"recvpeerid", @"recvpubenckey", @"poisoned", @"epoch", @"curve", @"version", @"wrappedkey", @"signature", @"ckrecord"];
}

- (NSDictionary<NSString*,NSString*>*)whereClauseToFindSelf {
    return @{@"uuid":self.tlkUUID,
             @"senderpeerid":self.senderPeerID,
             @"recvpeerid":self.receiver.peerID,
             @"ckzone":self.zoneID.zoneName,
             };
}

- (NSDictionary<NSString*,NSString*>*)sqlValues {
    return @{@"uuid":            self.tlkUUID,
             @"senderpeerid":    self.senderPeerID,
             @"recvpeerid":      self.receiver.peerID,
             @"recvpubenckey":   CKKSNilToNSNull([self.receiver.publicEncryptionKey.keyData base64EncodedStringWithOptions:0]),
             @"poisoned":        [NSString stringWithFormat:@"%ld", (long)self.poisoned],
             @"epoch":           [NSString stringWithFormat:@"%ld", (long)self.epoch],
             @"curve":           [NSString stringWithFormat:@"%ld", (long)self.curve],
             @"version":         [NSString stringWithFormat:@"%ld", (long)self.version],
             @"wrappedkey":      CKKSNilToNSNull([self.wrappedTLK      base64EncodedStringWithOptions:0]),
             @"signature":       CKKSNilToNSNull([self.signature       base64EncodedStringWithOptions:0]),
             @"ckzone":          CKKSNilToNSNull(self.zoneID.zoneName),
             @"ckrecord":        CKKSNilToNSNull([self.encodedCKRecord base64EncodedStringWithOptions:0]),
             };
}

+ (instancetype)fromDatabaseRow:(NSDictionary<NSString*,NSString*>*)row {
    CKRecordZoneID* zoneID = [[CKRecordZoneID alloc] initWithZoneName: row[@"ckzone"] ownerName:CKCurrentUserDefaultName];

    SFEllipticCurve curve = (SFEllipticCurve)[row[@"curve"] integerValue];  // TODO: sanitize
    SecCKKSTLKShareVersion version = (SecCKKSTLKShareVersion)[row[@"version"] integerValue]; // TODO: sanitize

    NSData* keydata = CKKSUnbase64NullableString(row[@"recvpubenckey"]);
    NSError* error = nil;
    SFECPublicKey* receiverPublicKey = keydata ? [[SFECPublicKey alloc] initWithData:keydata
                                                                           specifier:[[SFECKeySpecifier alloc] initWithCurve:curve]
                                                                               error:&error] : nil;

    if(error) {
        ckkserror("ckksshare", zoneID, "Couldn't make public key from data: %@", error);
        receiverPublicKey = nil;
    }

    return [[CKKSTLKShare alloc] initForKey:row[@"uuid"]
                               senderPeerID:row[@"senderpeerid"]
                             recieverPeerID:row[@"recvpeerid"]
                       receiverEncPublicKey:receiverPublicKey
                                      curve:curve
                                    version:version
                                      epoch:[row[@"epoch"] integerValue]
                                   poisoned:[row[@"poisoned"] integerValue]
                                 wrappedKey:CKKSUnbase64NullableString(row[@"wrappedkey"])
                                  signature:CKKSUnbase64NullableString(row[@"signature"])
                                     zoneID:zoneID
                            encodedCKRecord:CKKSUnbase64NullableString(row[@"ckrecord"])
            ];
}

@end

#endif // OCTAGON
