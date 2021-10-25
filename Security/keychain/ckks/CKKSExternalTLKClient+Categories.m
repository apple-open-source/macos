
#if OCTAGON

#import "keychain/ckks/CKKSExternalTLKClient+Categories.h"
#import "keychain/categories/NSError+UsefulConstructors.h"

@implementation CKKSExternalKey (CKKSTranslation)
- (instancetype)initWithViewName:(NSString*)viewName
                             tlk:(CKKSKey*)tlk
{
    return [self initWithView:viewName
                         uuid:tlk.uuid
                parentTLKUUID:tlk.parentKeyUUID
                      keyData:tlk.wrappedKeyData];
}

- (CKKSKey* _Nullable)makeCKKSKey:(CKRecordZoneID*)zoneID error:(NSError**)error
{
    CKKSKey* key = [[CKKSKey alloc] initWithWrappedKeyData:self.keyData
                                                      uuid:self.uuid
                                             parentKeyUUID:self.parentKeyUUID ?: self.uuid
                                                  keyclass:SecCKKSKeyClassTLK
                                                     state:SecCKKSProcessedStateRemote
                                                    zoneID:zoneID
                                           encodedCKRecord:nil
                                                currentkey:0];

    return key;
}

- (CKKSKey* _Nullable)makeFakeCKKSClassKey:(CKKSKeyClass*)keyclass zoneiD:(CKRecordZoneID*)zoneID error:(NSError**)error
{
    CKKSKey* key = [[CKKSKey alloc] initWithWrappedKeyData:self.keyData
                                                      uuid:[NSString stringWithFormat:@"%@-fake-%@", self.uuid, keyclass]
                                             parentKeyUUID:self.parentKeyUUID
                                                  keyclass:keyclass
                                                     state:SecCKKSProcessedStateRemote
                                                    zoneID:zoneID
                                           encodedCKRecord:nil
                                                currentkey:0];

    return key;
}
@end

@implementation CKKSExternalTLKShare (CKKSTranslation)
- (instancetype)initWithViewName:(NSString*)viewName
                        tlkShare:(CKKSTLKShare*)tlkShareRecord
{
    return [self initWithView:viewName
                      tlkUUID:tlkShareRecord.tlkUUID
               receiverPeerID:[self datafyPeerID:tlkShareRecord.receiverPeerID]
                 senderPeerID:[self datafyPeerID:tlkShareRecord.senderPeerID]
                   wrappedTLK:tlkShareRecord.wrappedTLK
                    signature:tlkShareRecord.signature];
}


- (CKKSTLKShareRecord* _Nullable)makeTLKShareRecord:(CKRecordZoneID*)zoneID
{
    CKKSTLKShare* tlkShare = [[CKKSTLKShare alloc] initForKey:self.tlkUUID
                                                 senderPeerID:[self stringifyPeerID:self.senderPeerID]
                                               recieverPeerID:[self stringifyPeerID:self.receiverPeerID]
                                     receiverEncPublicKeySPKI:nil
                                                        curve:SFEllipticCurveNistp384
                                                      version:SecCKKSTLKShareVersion1
                                                        epoch:0
                                                     poisoned:0
                                                   wrappedKey:self.wrappedTLK
                                                    signature:self.signature
                                                       zoneID:zoneID];

    return [[CKKSTLKShareRecord alloc] initWithShare:tlkShare
                                              zoneID:zoneID
                                     encodedCKRecord:nil];
}

- (NSData*)datafyPeerID:(NSString*)peerID
{
    NSString* prefix = @"spid-";

    if ([peerID hasPrefix:prefix]) {
        peerID = [peerID substringFromIndex:[prefix length]];
    }

    return [[NSData alloc] initWithBase64EncodedString:peerID options:0];
}
@end

#endif
