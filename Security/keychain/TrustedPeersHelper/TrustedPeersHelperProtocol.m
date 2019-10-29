
#import <Foundation/Foundation.h>

#if OCTAGON
#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "utilities/debugging.h"
#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>
#endif

NSXPCInterface* TrustedPeersHelperSetupProtocol(NSXPCInterface* interface)
{
#if OCTAGON
    static NSMutableSet *errClasses;

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        errClasses = [NSMutableSet setWithSet:CKAcceptableValueClasses()];

        char *classes[] = {
            "CKPrettyError",
            "CKRecordID",
            "NSArray",
            "NSData",
            "NSDate",
            "NSDictionary",
            "NSError",
            "NSNull",
            "NSNumber",
            "NSOrderedSet",
            "NSSet",
            "NSString",
            "NSURL",
        };

        for (unsigned n = 0; n < sizeof(classes)/sizeof(classes[0]); n++) {
            Class cls = objc_getClass(classes[n]);
            if (cls) {
                [errClasses addObject:cls];
            }
        }
    });

    @try {
        NSSet* arrayOfKeySets = [NSSet setWithArray:@[[NSArray class], [CKKSKeychainBackedKeySet class]]];
        NSSet* arrayOfTLKShares = [NSSet setWithArray:@[[NSArray class], [CKKSTLKShare class]]];
        NSSet* arrayOfCKRecords = [NSSet setWithArray:@[[NSArray class], [CKRecord class]]];
        NSSet* arrayOfStrings = [NSSet setWithArray:@[[NSArray class], [NSString class]]];
        NSSet* trustedPeersHelperPeerState = [NSSet setWithObject:[TrustedPeersHelperPeerState class]];

        NSSet* arrayOfTrustedPeersHelperPeer = [NSSet setWithArray:@[[NSArray class], [TrustedPeersHelperPeer class]]];

        [interface setClasses:arrayOfStrings   forSelector:@selector(addAllowedMachineIDsWithContainer:context:machineIDs:reply:) argumentIndex:2 ofReply:NO];
        [interface setClasses:arrayOfStrings   forSelector:@selector(removeAllowedMachineIDsWithContainer:context:machineIDs:reply:) argumentIndex:2 ofReply:NO];

        [interface setClasses:arrayOfKeySets   forSelector:@selector(establishWithContainer:context:ckksKeys:tlkShares:preapprovedKeys:reply:) argumentIndex:2 ofReply:NO];
        [interface setClasses:arrayOfTLKShares forSelector:@selector(establishWithContainer:context:ckksKeys:tlkShares:preapprovedKeys:reply:) argumentIndex:3 ofReply:NO];
        [interface setClasses:arrayOfCKRecords forSelector:@selector(establishWithContainer:context:ckksKeys:tlkShares:preapprovedKeys:reply:) argumentIndex:1 ofReply:YES];

        [interface setClasses:arrayOfKeySets   forSelector:@selector(joinWithContainer:context:voucherData:voucherSig:ckksKeys:tlkShares:preapprovedKeys:reply:) argumentIndex:4 ofReply:NO];
        [interface setClasses:arrayOfTLKShares forSelector:@selector(joinWithContainer:context:voucherData:voucherSig:ckksKeys:tlkShares:preapprovedKeys:reply:) argumentIndex:5 ofReply:NO];
        [interface setClasses:arrayOfCKRecords forSelector:@selector(joinWithContainer:context:voucherData:voucherSig:ckksKeys:tlkShares:preapprovedKeys:reply:) argumentIndex:1 ofReply:YES];

        [interface setClasses:arrayOfKeySets   forSelector:@selector(attemptPreapprovedJoinWithContainer:context:ckksKeys:tlkShares:preapprovedKeys:reply:) argumentIndex:2 ofReply:NO];
        [interface setClasses:arrayOfTLKShares forSelector:@selector(attemptPreapprovedJoinWithContainer:context:ckksKeys:tlkShares:preapprovedKeys:reply:) argumentIndex:3 ofReply:NO];
        [interface setClasses:arrayOfCKRecords forSelector:@selector(attemptPreapprovedJoinWithContainer:context:ckksKeys:tlkShares:preapprovedKeys:reply:) argumentIndex:1 ofReply:YES];

        [interface setClasses:arrayOfKeySets      forSelector:@selector(vouchWithContainer:
                                                                     context:
                                                                     peerID:
                                                                     permanentInfo:
                                                                     permanentInfoSig:
                                                                     stableInfo:
                                                                     stableInfoSig:
                                                                     ckksKeys:
                                                                     reply:) argumentIndex:7 ofReply:NO];

        [interface setClasses:arrayOfTLKShares forSelector:@selector(vouchWithBottleWithContainer:
                                                                     context:
                                                                     bottleID:
                                                                     entropy:
                                                                     bottleSalt:
                                                                     tlkShares:
                                                                     reply:) argumentIndex:5 ofReply:NO];
        [interface setClasses:arrayOfKeySets forSelector:@selector(setRecoveryKeyWithContainer:
                                                                   context:
                                                                   recoveryKey:
                                                                   salt:
                                                                   ckksKeys:
                                                                   reply:) argumentIndex:4 ofReply:NO];
        [interface setClasses:arrayOfTLKShares forSelector:@selector(vouchWithRecoveryKeyWithContainer:
                                                                     context:
                                                                     recoveryKey:
                                                                     salt:
                                                                     tlkShares:
                                                                     reply:) argumentIndex:4 ofReply:NO];

        [interface setClasses:[NSSet setWithObject:[TPPolicy class]] forSelector:@selector(fetchPolicyWithContainer:
                                                                                           context:
                                                                                           reply:) argumentIndex:0 ofReply:YES];

        [interface setClasses:trustedPeersHelperPeerState forSelector:@selector(updateWithContainer:
                                                                                context:
                                                                                deviceName:
                                                                                serialNumber:
                                                                                osVersion:
                                                                                policyVersion:
                                                                                policySecrets:
                                                                                reply:) argumentIndex:0 ofReply:YES];

        [interface setClasses:trustedPeersHelperPeerState   forSelector:@selector(fetchTrustStateWithContainer:
                                                                                  context:
                                                                                  reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:arrayOfTrustedPeersHelperPeer forSelector:@selector(fetchTrustStateWithContainer:
                                                                                  context:
                                                                                  reply:) argumentIndex:1 ofReply:YES];

        [interface setClasses:arrayOfKeySets forSelector:@selector(updateTLKsWithContainer:
                                                                   context:
                                                                   ckksKeys:
                                                                   tlkShares:
                                                                   reply:) argumentIndex:2 ofReply:NO];
        [interface setClasses:arrayOfTLKShares forSelector:@selector(updateTLKsWithContainer:
                                                                     context:
                                                                     ckksKeys:
                                                                     tlkShares:
                                                                     reply:) argumentIndex:3 ofReply:NO];
        [interface setClasses:arrayOfCKRecords forSelector:@selector(updateTLKsWithContainer:
                                                                     context:
                                                                     ckksKeys:
                                                                     tlkShares:
                                                                     reply:) argumentIndex:0 ofReply:YES];
    }
    @catch(NSException* e) {
        secerror("TrustedPeersHelperSetupProtocol failed, continuing, but you might crash later: %@", e);
#if DEBUG
        @throw e;
#endif
    }
#endif

    return interface;
}

@implementation TrustedPeersHelperPeerState

- (instancetype)initWithPeerID:(NSString* _Nullable)peerID
                 isPreapproved:(BOOL)isPreapproved
                        status:(TPPeerStatus)peerStatus
                 memberChanges:(BOOL)memberChanges
             unknownMachineIDs:(BOOL)unknownMachineIDs
                     osVersion:(NSString *)osVersion
{
    if((self = [super init])) {
        _peerID = peerID;
        _identityIsPreapproved = isPreapproved;
        _peerStatus = peerStatus;
        _memberChanges = memberChanges;
        _unknownMachineIDsPresent = unknownMachineIDs;
        _osVersion = osVersion;
    }
    return self;
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"<TPHPeerState: %@ preapproved:%d status:%lld memberChanges: %@ unk. mIDs: %@ osVersion: %@>",
            self.peerID,
            self.identityIsPreapproved,
            (int64_t)self.peerStatus,
            self.memberChanges ? @"YES" : @"NO",
            self.unknownMachineIDsPresent ? @"YES" : @"NO",
            self.osVersion?:@"unknown"];
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super init])) {
        _peerID = [coder decodeObjectOfClass:[NSString class] forKey:@"peerID"];
        _identityIsPreapproved = [coder decodeBoolForKey:@"identityIsPreapproved"];
        _peerStatus = (TPPeerStatus)[coder decodeInt64ForKey:@"peerStatus"];
        _memberChanges = (BOOL)[coder decodeInt64ForKey:@"memberChanges"];
        _unknownMachineIDsPresent = (BOOL)[coder decodeInt64ForKey:@"unknownMachineIDs"];
        _osVersion = [coder decodeObjectOfClass:[NSString class] forKey:@"osVersion"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:self.peerID forKey:@"peerID"];
    [coder encodeBool:self.identityIsPreapproved forKey:@"identityIsPreapproved"];
    [coder encodeInt64:(int64_t)self.peerStatus forKey:@"peerStatus"];
    [coder encodeInt64:(int64_t)self.memberChanges forKey:@"memberChanges"];
    [coder encodeInt64:(int64_t)self.unknownMachineIDsPresent forKey:@"unknownMachineIDs"];
    [coder encodeObject:self.osVersion forKey:@"osVersion"];

}
@end

@implementation TrustedPeersHelperEgoPeerStatus

- (instancetype)initWithEgoPeerID:(NSString* _Nullable)egoPeerID
                           status:(TPPeerStatus)egoStatus
        viablePeerCountsByModelID:(NSDictionary<NSString*, NSNumber*>*)viablePeerCountsByModelID
                       isExcluded:(BOOL)isExcluded
                         isLocked:(BOOL)isLocked
{
    if((self = [super init])) {
        _egoPeerID = egoPeerID;
        _egoStatus = egoStatus;
        _viablePeerCountsByModelID = viablePeerCountsByModelID;
        _numberOfPeersInOctagon = 0;
        for(NSNumber* n in viablePeerCountsByModelID.allValues) {
            _numberOfPeersInOctagon += [n unsignedIntegerValue];
        }
        _isExcluded = isExcluded;
        _isLocked = isLocked;
    }
    return self;
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"<TPHEgoPeerState: %@>", self.egoPeerID];
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super init])) {
        _egoPeerID = [coder decodeObjectOfClass:[NSString class] forKey:@"peerID"];
        _egoStatus = (TPPeerStatus)[coder decodeInt64ForKey:@"egoStatus"];
        _viablePeerCountsByModelID = [coder decodeObjectOfClasses:[NSSet setWithArray:@[[NSDictionary class], [NSString class], [NSNumber class]]] forKey:@"viablePeerCountsByModelID"];
        _numberOfPeersInOctagon = 0;
        for(NSNumber* n in _viablePeerCountsByModelID.allValues) {
            _numberOfPeersInOctagon += [n unsignedIntegerValue];
        }

        _isExcluded = (BOOL)[coder decodeBoolForKey:@"isExcluded"];
        _isLocked = (BOOL)[coder decodeBoolForKey:@"isLocked"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:self.egoPeerID forKey:@"peerID"];
    [coder encodeInt64:self.egoStatus forKey:@"egoStatus"];
    [coder encodeObject:self.viablePeerCountsByModelID forKey:@"viablePeerCountsByModelID"];
    [coder encodeBool:self.isExcluded forKey:@"isExcluded"];
    [coder encodeBool:self.isLocked forKey:@"isLocked"];
}

@end


@implementation TrustedPeersHelperPeer
- (instancetype)initWithPeerID:(NSString*)peerID
                   signingSPKI:(NSData*)signingSPKI
                encryptionSPKI:(NSData*)encryptionSPKI
                      viewList:(NSSet<NSString*>*)viewList
{
    if((self = [super init])) {
        _peerID = peerID;
        _signingSPKI = signingSPKI;
        _encryptionSPKI = encryptionSPKI;
        _viewList = viewList;
    }
    return self;
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"<TPHPeer: %@ %@ %@ (%lu views)>",
            self.peerID,
            self.signingSPKI,
            self.encryptionSPKI,
            (unsigned long)self.viewList.count];
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super init])) {
        _peerID = [coder decodeObjectOfClass:[NSString class] forKey:@"peerID"];
        _signingSPKI = [coder decodeObjectOfClass:[NSData class] forKey:@"signingSPKI"];
        _encryptionSPKI = [coder decodeObjectOfClass:[NSData class] forKey:@"encryptionSPKI"];
        _viewList = [coder decodeObjectOfClasses:[NSSet setWithArray:@[[NSSet class], [NSString class]]] forKey:@"viewList"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:self.peerID forKey:@"peerID"];
    [coder encodeObject:self.signingSPKI forKey:@"signingSPKI"];
    [coder encodeObject:self.encryptionSPKI forKey:@"encryptionSPKI"];
    [coder encodeObject:self.viewList forKey:@"viewList"];
}
@end
