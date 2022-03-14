
#import <Foundation/Foundation.h>

#if OCTAGON
#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "utilities/debugging.h"
#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>
#import <Security/SecXPCHelper.h>
#endif

NSXPCInterface* TrustedPeersHelperSetupProtocol(NSXPCInterface* interface)
{
#if OCTAGON
    static NSMutableSet *errClasses;

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        errClasses = [NSMutableSet setWithSet:CKAcceptableValueClasses()];
        [errClasses unionSet:[SecXPCHelper safeErrorClasses]];
    });

    @try {
        NSSet* arrayOfKeySets = [NSSet setWithArray:@[[NSArray class], [CKKSKeychainBackedKeySet class]]];
        NSSet* arrayOfTLKShares = [NSSet setWithArray:@[[NSArray class], [CKKSTLKShare class]]];
        NSSet* arrayOfCKRecords = [NSSet setWithArray:@[[NSArray class], [CKRecord class]]];
        NSSet* arrayOfStrings = [NSSet setWithArray:@[[NSArray class], [NSString class]]];
        NSSet* trustedPeersHelperPeerState = [NSSet setWithObject:[TrustedPeersHelperPeerState class]];

        NSSet* trustedPeersHelperCustodianRecoveryKey= [NSSet setWithObject:[TrustedPeersHelperCustodianRecoveryKey class]];

        NSSet* arrayOfTrustedPeersHelperPeer = [NSSet setWithArray:@[[NSArray class], [TrustedPeersHelperPeer class]]];
        NSSet* arrayOfSettings = [NSSet setWithArray:@[[NSArray class], [NSDictionary class], [NSString class], [TPPBPeerStableInfoSetting class]]];

        [interface setClasses:errClasses forSelector:@selector(dumpWithContainer:context:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(departByDistrustingSelfWithContainer:context:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(distrustPeerIDsWithContainer:context:peerIDs:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(trustStatusWithContainer:context:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(resetWithContainer:context:resetReason:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(localResetWithContainer:context:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(setAllowedMachineIDsWithContainer:context:allowedMachineIDs:honorIDMSListChanges:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(addAllowedMachineIDsWithContainer:context:machineIDs:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(removeAllowedMachineIDsWithContainer:context:machineIDs:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(fetchAllowedMachineIDsWithContainer:context:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(fetchEgoEpochWithContainer:context:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(prepareWithContainer:context:epoch:machineID:bottleSalt:bottleID:modelID:deviceName:serialNumber:osVersion:policyVersion:policySecrets:syncUserControllableViews:secureElementIdentity:setting:signingPrivKeyPersistentRef:encPrivKeyPersistentRef:reply:) argumentIndex:6 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(prepareInheritancePeerWithContainer:context:epoch:machineID:bottleSalt:bottleID:modelID:deviceName:serialNumber:osVersion:policyVersion:policySecrets:syncUserControllableViews:secureElementIdentity:signingPrivKeyPersistentRef:encPrivKeyPersistentRef:crk:reply:) argumentIndex:7 ofReply:YES];

        [interface setClasses:errClasses forSelector:@selector(establishWithContainer:context:ckksKeys:tlkShares:preapprovedKeys:reply:) argumentIndex:3 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(vouchWithContainer:context:peerID:permanentInfo:permanentInfoSig:stableInfo:stableInfoSig:ckksKeys:reply:) argumentIndex:2 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(preflightVouchWithBottleWithContainer:context:bottleID:reply:) argumentIndex:3 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(vouchWithBottleWithContainer:context:bottleID:entropy:bottleSalt:tlkShares:reply:) argumentIndex:4 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(preflightVouchWithRecoveryKeyWithContainer:context:recoveryKey:salt:reply:) argumentIndex:2 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(preflightVouchWithCustodianRecoveryKeyWithContainer:context:crk:reply:) argumentIndex:2 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(vouchWithRecoveryKeyWithContainer:context:recoveryKey:salt:tlkShares:reply:) argumentIndex:4 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(vouchWithCustodianRecoveryKeyWithContainer:context:crk:tlkShares:reply:) argumentIndex:4 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(recoverTLKSharesForInheritorWithContainer:context:crk:tlkShares:reply:) argumentIndex:2 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(joinWithContainer:context:voucherData:voucherSig:ckksKeys:tlkShares:preapprovedKeys:reply:) argumentIndex:3 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(preflightPreapprovedJoinWithContainer:context:preapprovedKeys:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(attemptPreapprovedJoinWithContainer:context:ckksKeys:tlkShares:preapprovedKeys:reply:) argumentIndex:3 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(updateWithContainer:context:forceRefetch:deviceName:serialNumber:osVersion:policyVersion:policySecrets:syncUserControllableViews:secureElementIdentity:reply:) argumentIndex:2 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(setPreapprovedKeysWithContainer:context:preapprovedKeys:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(updateTLKsWithContainer:context:ckksKeys:tlkShares:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(fetchViableBottlesWithContainer:context:reply:) argumentIndex:2 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(fetchViableEscrowRecordsWithContainer:context:forceFetch:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(fetchEscrowContentsWithContainer:context:reply:) argumentIndex:3 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(fetchPolicyDocumentsWithContainer:context:versions:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(fetchCurrentPolicyWithContainer:context:modelIDOverride:isInheritedAccount:reply:) argumentIndex:2 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(validatePeersWithContainer:context:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(fetchTrustStateWithContainer:context:reply:) argumentIndex:2 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(setRecoveryKeyWithContainer:context:recoveryKey:salt:ckksKeys:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(createCustodianRecoveryKeyWithContainer:context:recoveryKey:salt:ckksKeys:uuid:kind:reply:) argumentIndex:2 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(removeCustodianRecoveryKeyWithContainer:context:uuid:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(reportHealthWithContainer:context:stateMachineState:trustState:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(pushHealthInquiryWithContainer:context:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(requestHealthCheckWithContainer:context:requiresEscrowCheck:knownFederations:reply:) argumentIndex:5 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(getSupportAppInfoWithContainer:context:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(removeEscrowCacheWithContainer:context:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(fetchAccountSettingsWithContainer:context:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:arrayOfSettings forSelector:@selector(fetchAccountSettingsWithContainer:context:reply:) argumentIndex:0 ofReply:YES];

        [interface setClasses:errClasses forSelector:@selector(fetchRecoverableTLKSharesWithContainer:context:peerID:reply:) argumentIndex:1 ofReply:YES];
       
        [interface setClasses:arrayOfCKRecords forSelector:@selector(fetchRecoverableTLKSharesWithContainer:context:peerID:reply:) argumentIndex:0 ofReply:YES];

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
        [interface setClasses:arrayOfTLKShares forSelector:@selector(vouchWithBottleWithContainer:
                                                                     context:
                                                                     bottleID:
                                                                     entropy:
                                                                     bottleSalt:
                                                                     tlkShares:
                                                                     reply:) argumentIndex:2 ofReply:YES];
        [interface setClasses:arrayOfKeySets forSelector:@selector(setRecoveryKeyWithContainer:
                                                                   context:
                                                                   recoveryKey:
                                                                   salt:
                                                                   ckksKeys:
                                                                   reply:) argumentIndex:4 ofReply:NO];
        [interface setClasses:arrayOfCKRecords forSelector:@selector(setRecoveryKeyWithContainer:
                                                                     context:
                                                                     recoveryKey:
                                                                     salt:
                                                                     ckksKeys:
                                                                     reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:arrayOfKeySets forSelector:@selector(createCustodianRecoveryKeyWithContainer:
                                                                   context:
                                                                   recoveryKey:
                                                                   salt:
                                                                   ckksKeys:
                                                                   uuid:
                                                                   kind:
                                                                   reply:) argumentIndex:4 ofReply:NO];
        [interface setClasses:arrayOfCKRecords forSelector:@selector(createCustodianRecoveryKeyWithContainer:
                                                                   context:
                                                                   recoveryKey:
                                                                   salt:
                                                                   ckksKeys:
                                                                   uuid:
                                                                   kind:
                                                                   reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:arrayOfTLKShares forSelector:@selector(vouchWithRecoveryKeyWithContainer:
                                                                     context:
                                                                     recoveryKey:
                                                                     salt:
                                                                     tlkShares:
                                                                     reply:) argumentIndex:4 ofReply:NO];
        [interface setClasses:arrayOfTLKShares forSelector:@selector(vouchWithRecoveryKeyWithContainer:
                                                                     context:
                                                                     recoveryKey:
                                                                     salt:
                                                                     tlkShares:
                                                                     reply:) argumentIndex:2 ofReply:YES];
        [interface setClasses:arrayOfTLKShares forSelector:@selector(vouchWithCustodianRecoveryKeyWithContainer:
                                                                     context:
                                                                     crk:
                                                                     tlkShares:
                                                                     reply:) argumentIndex:3 ofReply:NO];
        
        [interface setClasses:arrayOfCKRecords forSelector:@selector(prepareInheritancePeerWithContainer:
                                                                     context:
                                                                     epoch:
                                                                     machineID:
                                                                     bottleSalt:
                                                                     bottleID:
                                                                     modelID:
                                                                     deviceName:
                                                                     serialNumber:
                                                                     osVersion:
                                                                     policyVersion:
                                                                     policySecrets:
                                                                     syncUserControllableViews:
                                                                     secureElementIdentity:
                                                                     signingPrivKeyPersistentRef:
                                                                     encPrivKeyPersistentRef:
                                                                     crk:
                                                                     reply:) argumentIndex:7 ofReply:YES];
        
        [interface setClasses:arrayOfTLKShares forSelector:@selector(vouchWithCustodianRecoveryKeyWithContainer:
                                                                     context:
                                                                     crk:
                                                                     tlkShares:
                                                                     reply:) argumentIndex:2 ofReply:YES];

        [interface setClasses:arrayOfTLKShares forSelector:@selector(recoverTLKSharesForInheritorWithContainer:
                                                                             context:
                                                                             crk:
                                                                             tlkShares:
                                                                             reply:)
                                                                             argumentIndex:3 ofReply:NO];

        [interface setClasses:arrayOfTLKShares forSelector:@selector(recoverTLKSharesForInheritorWithContainer:
                                                                             context:
                                                                             crk:
                                                                             tlkShares:
                                                                             reply:)
                                                                             argumentIndex:0 ofReply:YES];
        
        [interface setClasses:trustedPeersHelperCustodianRecoveryKey forSelector:@selector(createCustodianRecoveryKeyWithContainer:
                                                                                           context:
                                                                                           recoveryKey:
                                                                                           salt:
                                                                                           ckksKeys:
                                                                                           uuid:
                                                                                           kind:
                                                                                           reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:trustedPeersHelperCustodianRecoveryKey forSelector:@selector(preflightVouchWithCustodianRecoveryKeyWithContainer:
                                                                                           context:
                                                                                           crk:
                                                                                           reply:) argumentIndex:2 ofReply:NO];
        [interface setClasses:trustedPeersHelperCustodianRecoveryKey forSelector:@selector(vouchWithCustodianRecoveryKeyWithContainer:
                                                                                           context:
                                                                                           crk:
                                                                                           tlkShares:
                                                                                           reply:) argumentIndex:2 ofReply:NO];
        [interface setClasses:trustedPeersHelperPeerState forSelector:@selector(updateWithContainer:
                                                                                context:
                                                                                forceRefetch:
                                                                                deviceName:
                                                                                serialNumber:
                                                                                osVersion:
                                                                                policyVersion:
                                                                                policySecrets:
                                                                                syncUserControllableViews:
                                                                                secureElementIdentity:
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
        @throw e;
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
    return [NSString stringWithFormat:@"<TPHPeerState: %@ "
                         "preapproved:%d "
                         "status:%@ "
                         "memberChanges: %@ "
                         "unk. mIDs: %@ "
                         "osVersion: %@ "
                         ">",
            self.peerID,
            self.identityIsPreapproved,
            TPPeerStatusToString(self.peerStatus),
            self.memberChanges ? @"YES" : @"NO",
            self.unknownMachineIDsPresent ? @"YES" : @"NO",
            self.osVersion?:@"unknown"
            ];
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
                 egoPeerMachineID:(NSString* _Nullable)egoPeerMachineID
                           status:(TPPeerStatus)egoStatus
        viablePeerCountsByModelID:(NSDictionary<NSString*, NSNumber*>*)viablePeerCountsByModelID
            peerCountsByMachineID:(NSDictionary<NSString *,NSNumber *> * _Nonnull)peerCountsByMachineID
                       isExcluded:(BOOL)isExcluded
                         isLocked:(BOOL)isLocked
{
    if((self = [super init])) {
        _egoPeerID = egoPeerID;
        _egoPeerMachineID = egoPeerMachineID;
        _egoStatus = egoStatus;
        _viablePeerCountsByModelID = viablePeerCountsByModelID;
        _peerCountsByMachineID = peerCountsByMachineID;
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
    return [NSString stringWithFormat:@"<TPHEgoPeerState: %@ (mid:%@)>", self.egoPeerID, self.egoPeerMachineID];
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super init])) {
        _egoPeerID = [coder decodeObjectOfClass:[NSString class] forKey:@"peerID"];
        _egoPeerMachineID = [coder decodeObjectOfClass:[NSString class] forKey:@"mID"];
        _egoStatus = (TPPeerStatus)[coder decodeInt64ForKey:@"egoStatus"];
        _viablePeerCountsByModelID = [coder decodeObjectOfClasses:[NSSet setWithArray:@[[NSDictionary class], [NSString class], [NSNumber class]]] forKey:@"viablePeerCountsByModelID"];
        _numberOfPeersInOctagon = 0;
        for(NSNumber* n in _viablePeerCountsByModelID.allValues) {
            _numberOfPeersInOctagon += [n unsignedIntegerValue];
        }

        _peerCountsByMachineID = [coder decodeObjectOfClasses:[NSSet setWithArray:@[[NSDictionary class], [NSString class], [NSNumber class]]] forKey:@"peerCountsByMachineID"];

        _isExcluded = (BOOL)[coder decodeBoolForKey:@"isExcluded"];
        _isLocked = (BOOL)[coder decodeBoolForKey:@"isLocked"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:self.egoPeerID forKey:@"peerID"];
    [coder encodeObject:self.egoPeerMachineID forKey:@"mID"];
    [coder encodeInt64:self.egoStatus forKey:@"egoStatus"];
    [coder encodeObject:self.viablePeerCountsByModelID forKey:@"viablePeerCountsByModelID"];
    [coder encodeObject:self.peerCountsByMachineID forKey:@"peerCountsByMachineID"];
    [coder encodeBool:self.isExcluded forKey:@"isExcluded"];
    [coder encodeBool:self.isLocked forKey:@"isLocked"];
}

@end


@implementation TrustedPeersHelperPeer
- (instancetype)initWithPeerID:(NSString*)peerID
                   signingSPKI:(NSData*)signingSPKI
                encryptionSPKI:(NSData*)encryptionSPKI
         secureElementIdentity:(TPPBSecureElementIdentity * _Nullable)secureElementIdentity
                      viewList:(NSSet<NSString*>*)viewList
{
    if((self = [super init])) {
        _peerID = peerID;
        _signingSPKI = signingSPKI;
        _encryptionSPKI = encryptionSPKI;
        _secureElementIdentity = secureElementIdentity;
        _viewList = viewList;
    }
    return self;
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"<TPHPeer: %@ %@ %@ se:%@ (%lu views)>",
            self.peerID,
            self.signingSPKI,
            self.encryptionSPKI,
            [self.secureElementIdentity.peerIdentifier base64EncodedStringWithOptions:0],
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
        _secureElementIdentity = [coder decodeObjectOfClass:[TPPBSecureElementIdentity class] forKey:@"seIdentity"];
        _viewList = [coder decodeObjectOfClasses:[NSSet setWithArray:@[[NSSet class], [NSString class]]] forKey:@"viewList"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:self.peerID forKey:@"peerID"];
    [coder encodeObject:self.signingSPKI forKey:@"signingSPKI"];
    [coder encodeObject:self.encryptionSPKI forKey:@"encryptionSPKI"];
    [coder encodeObject:self.secureElementIdentity forKey:@"seIdentity"];
    [coder encodeObject:self.viewList forKey:@"viewList"];
}
@end

@implementation TrustedPeersHelperCustodianRecoveryKey
- (instancetype)initWithUUID:(NSString*)uuid
               encryptionKey:(NSData*)encryptionKey
                  signingKey:(NSData*)signingKey
              recoveryString:(NSString*)recoveryString
                        salt:(NSString*)salt
                        kind:(TPPBCustodianRecoveryKey_Kind)kind
{
    if ((self = [super init])) {
        _uuid = uuid;
        _encryptionKey = encryptionKey;
        _signingKey = signingKey;
        _recoveryString = recoveryString;
        _salt = salt;
        _kind = kind;
    }
    return self;
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"<CustodianRecoveryKey: %@, (%@)>",
                     self.uuid, TPPBCustodianRecoveryKey_KindAsString(self.kind)];
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super init])) {
        _uuid = [coder decodeObjectOfClass:[NSString class] forKey:@"uuid"];
        _encryptionKey = [coder decodeObjectOfClass:[NSData class] forKey:@"encryptionKey"];
        _signingKey = [coder decodeObjectOfClass:[NSData class] forKey:@"signingKey"];
        _recoveryString = [coder decodeObjectOfClass:[NSString class] forKey:@"recoveryString"];
        _salt = [coder decodeObjectOfClass:[NSString class] forKey:@"salt"];
        _kind = [coder decodeInt32ForKey:@"kind"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:self.uuid forKey:@"uuid"];
    [coder encodeObject:self.encryptionKey forKey:@"encryptionKey"];
    [coder encodeObject:self.signingKey forKey:@"signingKey"];
    [coder encodeObject:self.recoveryString forKey:@"recoveryString"];
    [coder encodeObject:self.salt forKey:@"salt"];
    [coder encodeInt32:self.kind forKey:@"kind"];
}
@end


@implementation TrustedPeersHelperTLKRecoveryResult
- (instancetype)initWithSuccessfulKeyUUIDs:(NSSet<NSString*>*)successfulKeysRecovered
                   totalTLKSharesRecovered:(int64_t)totalTLKSharesRecovered
                         tlkRecoveryErrors:(NSDictionary<NSString*, NSArray<NSError*>*>*)tlkRecoveryErrors
{
    if((self = [super init])) {
        _successfulKeysRecovered = successfulKeysRecovered;
        _totalTLKSharesRecovered = totalTLKSharesRecovered;
        _tlkRecoveryErrors = tlkRecoveryErrors;
    }
    return self;
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"<TLKRecoveryResult: %@ totalTLKSharesRecovered:%d>",
            self.successfulKeysRecovered,
            (int)self.totalTLKSharesRecovered];
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super init])) {
        _successfulKeysRecovered = [coder decodeObjectOfClasses:[NSSet setWithArray:@[[NSSet class], [NSString class]]] forKey:@"keys"];
        _totalTLKSharesRecovered = [coder decodeInt64ForKey:@"totalShares"];
        _tlkRecoveryErrors = [coder decodeObjectOfClasses:[SecXPCHelper safeErrorClasses] forKey:@"errors"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:self.successfulKeysRecovered forKey:@"keys"];
    [coder encodeInt64:self.totalTLKSharesRecovered forKey:@"totalShares"];
    [coder encodeObject:self.tlkRecoveryErrors forKey:@"errors"];
}
@end

