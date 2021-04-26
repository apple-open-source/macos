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

#import "keychain/ckks/CKKSTLKShareRecord.h"
#import "keychain/ckks/CKKSPeer.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/categories/NSError+UsefulConstructors.h"

#import <SecurityFoundation/SFKey.h>
#import <SecurityFoundation/SFEncryptionOperation.h>
#import <SecurityFoundation/SFSigningOperation.h>
#import <SecurityFoundation/SFDigestOperation.h>

@interface CKKSTLKShareRecord ()
@end

@implementation CKKSTLKShareRecord

- (instancetype)init:(CKKSTLKShare*)share
              zoneID:(CKRecordZoneID*)zoneID
     encodedCKRecord:(NSData*)encodedCKRecord
{
    if((self = [super initWithCKRecordType:SecCKRecordTLKShareType
                           encodedCKRecord:encodedCKRecord
                                    zoneID:zoneID])) {
        _share = share;
    }
    return self;
}

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

        _share = [[CKKSTLKShare alloc] init:key.keycore
                                         sender:sender
                                       receiver:receiver
                                          curve:curve
                                        version:version
                                          epoch:epoch
                                       poisoned:poisoned
                                         zoneID:zoneID];
    }
    return self;
}

- (instancetype)initForKey:(NSString*)tlkUUID
              senderPeerID:(NSString*)senderPeerID
            recieverPeerID:(NSString*)receiverPeerID
  receiverEncPublicKeySPKI:(NSData*)publicKeySPKI
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

        _share = [[CKKSTLKShare alloc] initForKey:tlkUUID
                                         senderPeerID:senderPeerID
                                       recieverPeerID:receiverPeerID
                             receiverEncPublicKeySPKI:publicKeySPKI
                                                curve:curve
                                              version:version
                                                epoch:epoch
                                             poisoned:poisoned
                                           wrappedKey:wrappedKey
                                            signature:signature
                                               zoneID:zoneID];
    }
    return self;
}

- (NSString*)description {
    return [NSString stringWithFormat:@"<CKKSTLKShare(%@): recv:%@ send:%@>",
            self.share.tlkUUID,
            self.share.receiverPeerID,
            self.share.senderPeerID];
}

- (NSString*)tlkUUID
{
    return self.share.tlkUUID;
}

- (NSString*)senderPeerID
{
    return self.share.senderPeerID;
}
- (NSInteger)epoch
{
    return self.share.epoch;
}

- (NSInteger)poisoned
{
    return self.share.poisoned;
}

- (NSData*)wrappedTLK
{
    return self.share.wrappedTLK;
}
- (NSData*)signature
{
    return self.share.signature;
}

- (CKKSKey*)unwrapUsing:(id<CKKSSelfPeer>)localPeer
                  error:(NSError * __autoreleasing *)error
{
    CKKSKeychainBackedKey* realkey = [self.share unwrapUsing:localPeer
                                                       error:error];

    if(!realkey) {
        return nil;
    }

    return [[CKKSKey alloc] initWithKeyCore:realkey];
}

- (NSData*)dataForSigning
{
    return [self.share dataForSigning:self.storedCKRecord];
}

// Returns the signature, but not the signed data itself;
- (NSData*)signRecord:(SFECKeyPair*)signingKey
                error:(NSError* __autoreleasing *)error
{
    return [self.share signRecord:signingKey
                         ckrecord:self.storedCKRecord
                            error:error];
}

- (bool)verifySignature:(NSData*)signature
          verifyingPeer:(id<CKKSPeer>)peer
                  error:(NSError* __autoreleasing *)error
{
    return [self.share verifySignature:signature
                         verifyingPeer:peer
                              ckrecord:self.storedCKRecord
                                 error:error];
}

- (bool)signatureVerifiesWithPeerSet:(NSSet<id<CKKSPeer>>*)peerSet
                               error:(NSError**)error
{
    return [self.share signatureVerifiesWithPeerSet:peerSet
                                           ckrecord:self.storedCKRecord
                                              error:error];
}

- (instancetype)copyWithZone:(NSZone *)zone {
    CKKSTLKShareRecord* shareRecord = [[[self class] allocWithZone:zone] init];
    shareRecord.share = [self.share copyWithZone:zone];
    return shareRecord;
}

- (BOOL)isEqual:(id)object {
    if(![object isKindOfClass:[CKKSTLKShareRecord class]]) {
        return NO;
    }

    CKKSTLKShareRecord* obj = (CKKSTLKShareRecord*) object;
    return [self.share isEqual: obj.share];
}

+ (CKKSTLKShareRecord*)share:(CKKSKey*)key
                    as:(id<CKKSSelfPeer>)sender
                    to:(id<CKKSPeer>)receiver
                 epoch:(NSInteger)epoch
              poisoned:(NSInteger)poisoned
                 error:(NSError* __autoreleasing *)error
{
    NSError* localerror = nil;
    // Load any existing TLK Share, so we can update it
    CKKSTLKShareRecord* oldShare =  [CKKSTLKShareRecord tryFromDatabase:key.uuid
                                                         receiverPeerID:receiver.peerID
                                                           senderPeerID:sender.peerID
                                                                 zoneID:key.zoneID
                                                                  error:&localerror];
    if(localerror) {
        ckkserror("ckksshare", key.zoneID, "couldn't load old share for %@: %@", key, localerror);
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    CKKSTLKShare* share = [CKKSTLKShare share:key.keycore
                                                   as:sender
                                                   to:receiver
                                                epoch:epoch
                                             poisoned:poisoned
                                                error:error];
    if(!share) {
        return nil;
    }

    CKKSTLKShareRecord* sharerecord = [[CKKSTLKShareRecord alloc] init:share
                                                                zoneID:key.zoneID
                                                       encodedCKRecord:oldShare.encodedCKRecord];
    return sharerecord;
}

- (CKKSKey*)recoverTLK:(id<CKKSSelfPeer>)recoverer
          trustedPeers:(NSSet<id<CKKSPeer>>*)peers
                 error:(NSError* __autoreleasing *)error
{
    CKKSKeychainBackedKey* realkey = [self.share recoverTLK:recoverer
                                               trustedPeers:peers
                                                   ckrecord:self.storedCKRecord
                                                      error:error];
    if(!realkey) {
        return nil;
    }
    return [[CKKSKey alloc] initWithKeyCore:realkey];
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

+ (NSArray<CKKSTLKShareRecord*>*)allFor:(NSString*)receiverPeerID
                          keyUUID:(NSString*)uuid
                           zoneID:(CKRecordZoneID*)zoneID
                            error:(NSError * __autoreleasing *)error {
    return [self allWhere:@{@"recvpeerid":CKKSNilToNSNull(receiverPeerID),
                            @"uuid":uuid,
                            @"ckzone": CKKSNilToNSNull(zoneID.zoneName)} error:error];
}

+ (NSArray<CKKSTLKShareRecord*>*)allForUUID:(NSString*)uuid
                               zoneID:(CKRecordZoneID*)zoneID
                                error:(NSError * __autoreleasing *)error {
    return [self allWhere:@{@"uuid":CKKSNilToNSNull(uuid),
                            @"ckzone":CKKSNilToNSNull(zoneID.zoneName)} error:error];
}

+ (NSArray<CKKSTLKShareRecord*>*)allInZone:(CKRecordZoneID*)zoneID
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
    return [NSString stringWithFormat:@"tlkshare-%@::%@::%@", self.share.tlkUUID, self.share.receiverPeerID, self.share.senderPeerID];
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

    record[SecCKRecordSenderPeerID] = self.share.senderPeerID;
    record[SecCKRecordReceiverPeerID] = self.share.receiverPeerID;
    record[SecCKRecordReceiverPublicEncryptionKey] = [self.share.receiverPublicEncryptionKeySPKI base64EncodedStringWithOptions:0];
    record[SecCKRecordCurve] = [NSNumber numberWithUnsignedInteger:(NSUInteger)self.share.curve];
    record[SecCKRecordVersion] = [NSNumber numberWithUnsignedInteger:(NSUInteger)self.share.version];
    record[SecCKRecordEpoch] = [NSNumber numberWithLong:(long)self.share.epoch];
    record[SecCKRecordPoisoned] = [NSNumber numberWithLong:(long)self.share.poisoned];

    record[SecCKRecordParentKeyRefKey] = [[CKReference alloc] initWithRecordID: [[CKRecordID alloc] initWithRecordName: self.share.tlkUUID zoneID: zoneID]
                                                                        action: CKReferenceActionValidate];

    record[SecCKRecordWrappedKeyKey] = [self.share.wrappedTLK base64EncodedStringWithOptions:0];
    record[SecCKRecordSignature] = [self.share.signature base64EncodedStringWithOptions:0];

    return record;
}

- (bool)matchesCKRecord:(CKRecord*)record {
    if(![record.recordType isEqualToString: SecCKRecordTLKShareType]) {
        return false;
    }

    if(![record.recordID.recordName isEqualToString: [self CKRecordName]]) {
        return false;
    }

    CKKSTLKShareRecord* share = [[CKKSTLKShareRecord alloc] initWithCKRecord:record];
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

    NSData* pubkeydata = CKKSUnbase64NullableString(record[SecCKRecordReceiverPublicEncryptionKey]);

    self.share = [[CKKSTLKShare alloc] initForKey:((CKReference*)record[SecCKRecordParentKeyRefKey]).recordID.recordName
                                         senderPeerID:record[SecCKRecordSenderPeerID]
                                       recieverPeerID:record[SecCKRecordReceiverPeerID]
                             receiverEncPublicKeySPKI:pubkeydata
                                                curve:[record[SecCKRecordCurve] longValue] // TODO: sanitize
                                              version:[record[SecCKRecordVersion] longValue]
                                                epoch:[record[SecCKRecordEpoch] longValue]
                                             poisoned:[record[SecCKRecordPoisoned] longValue]
                                           wrappedKey:[[NSData alloc] initWithBase64EncodedString:record[SecCKRecordWrappedKeyKey] options:0]
                                            signature:[[NSData alloc] initWithBase64EncodedString:record[SecCKRecordSignature] options:0]
                                               zoneID:record.recordID.zoneID];
}

#pragma mark - CKKSSQLDatabaseObject methods

+ (NSString*)sqlTable {
    return @"tlkshare";
}

+ (NSArray<NSString*>*)sqlColumns {
    return @[@"ckzone", @"uuid", @"senderpeerid", @"recvpeerid", @"recvpubenckey", @"poisoned", @"epoch", @"curve", @"version", @"wrappedkey", @"signature", @"ckrecord"];
}

- (NSDictionary<NSString*,NSString*>*)whereClauseToFindSelf {
    return @{@"uuid":self.share.tlkUUID,
             @"senderpeerid":self.share.senderPeerID,
             @"recvpeerid":self.share.receiverPeerID,
             @"ckzone":self.zoneID.zoneName,
             };
}

- (NSDictionary<NSString*,NSString*>*)sqlValues {
    return @{@"uuid":            self.share.tlkUUID,
             @"senderpeerid":    self.share.senderPeerID,
             @"recvpeerid":      self.share.receiverPeerID,
             @"recvpubenckey":   CKKSNilToNSNull([self.share.receiverPublicEncryptionKeySPKI base64EncodedStringWithOptions:0]),
             @"poisoned":        [NSString stringWithFormat:@"%ld", (long)self.share.poisoned],
             @"epoch":           [NSString stringWithFormat:@"%ld", (long)self.share.epoch],
             @"curve":           [NSString stringWithFormat:@"%ld", (long)self.share.curve],
             @"version":         [NSString stringWithFormat:@"%ld", (long)self.share.version],
             @"wrappedkey":      CKKSNilToNSNull([self.share.wrappedTLK      base64EncodedStringWithOptions:0]),
             @"signature":       CKKSNilToNSNull([self.share.signature       base64EncodedStringWithOptions:0]),
             @"ckzone":          CKKSNilToNSNull(self.zoneID.zoneName),
             @"ckrecord":        CKKSNilToNSNull([self.encodedCKRecord base64EncodedStringWithOptions:0]),
             };
}

+ (instancetype)fromDatabaseRow:(NSDictionary<NSString*, CKKSSQLResult*>*)row {
    CKRecordZoneID* zoneID = [[CKRecordZoneID alloc] initWithZoneName: row[@"ckzone"].asString ownerName:CKCurrentUserDefaultName];

    SFEllipticCurve curve = (SFEllipticCurve)row[@"curve"].asNSInteger;  // TODO: sanitize
    SecCKKSTLKShareVersion version = (SecCKKSTLKShareVersion)row[@"version"].asNSInteger; // TODO: sanitize

    return [[CKKSTLKShareRecord alloc] initForKey:row[@"uuid"].asString
                               senderPeerID:row[@"senderpeerid"].asString
                             recieverPeerID:row[@"recvpeerid"].asString
                   receiverEncPublicKeySPKI:row[@"recvpubenckey"].asBase64DecodedData
                                      curve:curve
                                    version:version
                                      epoch:row[@"epoch"].asNSInteger
                                   poisoned:row[@"poisoned"].asNSInteger
                                 wrappedKey:row[@"wrappedkey"].asBase64DecodedData
                                  signature:row[@"signature"].asBase64DecodedData
                                     zoneID:zoneID
                            encodedCKRecord:row[@"ckrecord"].asBase64DecodedData
            ];
}

+ (BOOL)intransactionRecordChanged:(CKRecord*)record resync:(BOOL)resync error:(NSError**)error
{
    NSError* localerror = nil;

    // CKKSTLKShares get saved with no modification
    CKKSTLKShareRecord* share = [[CKKSTLKShareRecord alloc] initWithCKRecord:record];
    bool saved = [share saveToDatabase:&localerror];
    if(!saved || localerror) {
        ckkserror("ckksshare", record.recordID.zoneID, "Couldn't save new TLK share to database: %@ %@", share, localerror);
        if(error) {
            *error = localerror;
        }
        return NO;
    }
    return YES;
}

+ (BOOL)intransactionRecordDeleted:(CKRecordID*)recordID resync:(BOOL)resync error:(NSError**)error
{
    NSError* localerror = nil;
    ckksinfo("ckksshare", recordID.zoneID, "CloudKit notification: deleted tlk share record(%@): %@", SecCKRecordTLKShareType, recordID);

    CKKSTLKShareRecord* share = [CKKSTLKShareRecord tryFromDatabaseFromCKRecordID:recordID error:&localerror];
    [share deleteFromDatabase:&localerror];

    if(localerror) {
        ckkserror("ckksshare", recordID.zoneID, "CK notification: Couldn't delete deleted TLKShare: %@ %@", recordID,  localerror);
        if(error) {
            *error = localerror;
        }
        return NO;
    }
    return YES;
}

+ (NSNumber* _Nullable)counts:(CKRecordZoneID*)zoneID error:(NSError * __autoreleasing *)error
{
    __block NSNumber *result = nil;

    [CKKSSQLDatabaseObject queryDatabaseTable:[[self class] sqlTable]
                                        where:@{@"ckzone": CKKSNilToNSNull(zoneID.zoneName)}
                                      columns:@[@"count(rowid)"]
                                      groupBy:nil
                                      orderBy:nil
                                        limit:-1
                                   processRow:^(NSDictionary<NSString*, CKKSSQLResult*>* row) {
                                       result = row[@"count(rowid)"].asNSNumberInteger;
                                   }
                                        error: error];
    return result;
}

@end

#endif // OCTAGON
