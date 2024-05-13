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
        NSSet* trustedPeersHelperPeerState = [NSSet setWithObject:[TrustedPeersHelperPeerState class]];

        NSSet* trustedPeersHelperCustodianRecoveryKey = [NSSet setWithObject:[TrustedPeersHelperCustodianRecoveryKey class]];
        NSSet* trustedPeersHelperHealthCheckResult = [NSSet setWithObject:[TrustedPeersHelperHealthCheckResult class]];

        NSSet* arrayOfTrustedPeersHelperPeer = [NSSet setWithArray:@[[NSArray class], [TrustedPeersHelperPeer class]]];
        NSSet* arrayOfSettings = [NSSet setWithArray:@[[NSArray class], [NSDictionary class], [NSString class], [TPPBPeerStableInfoSetting class]]];

        [interface setClasses:errClasses forSelector:@selector(dumpWithSpecificUser:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(departByDistrustingSelfWithSpecificUser:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(distrustPeerIDsWithSpecificUser:peerIDs:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(dropPeerIDsWithSpecificUser:peerIDs:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(trustStatusWithSpecificUser:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(resetWithSpecificUser:resetReason:idmsTargetContext:idmsCuttlefishPassword:notifyIdMS:internalAccount:demoAccount:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(localResetWithSpecificUser:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(setAllowedMachineIDsWithSpecificUser:allowedMachineIDs:userInitiatedRemovals:evictedRemovals:unknownReasonRemovals:honorIDMSListChanges:version:flowID:deviceSessionID:canSendMetrics:altDSID:trustedDeviceHash:deletedDeviceHash:trustedDevicesUpdateTimestamp:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(fetchAllowedMachineIDsWithSpecificUser:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(fetchEgoEpochWithSpecificUser:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(prepareWithSpecificUser:epoch:machineID:bottleSalt:bottleID:modelID:deviceName:serialNumber:osVersion:policyVersion:policySecrets:syncUserControllableViews:secureElementIdentity:setting:signingPrivKeyPersistentRef:encPrivKeyPersistentRef:reply:) argumentIndex:6 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(prepareInheritancePeerWithSpecificUser:epoch:machineID:bottleSalt:bottleID:modelID:deviceName:serialNumber:osVersion:policyVersion:policySecrets:syncUserControllableViews:secureElementIdentity:signingPrivKeyPersistentRef:encPrivKeyPersistentRef:crk:reply:) argumentIndex:7 ofReply:YES];

        [interface setClasses:errClasses forSelector:@selector(establishWithSpecificUser:ckksKeys:tlkShares:preapprovedKeys:reply:) argumentIndex:3 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(vouchWithSpecificUser:peerID:permanentInfo:permanentInfoSig:stableInfo:stableInfoSig:ckksKeys:flowID:deviceSessionID:canSendMetrics:reply:) argumentIndex:2 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(preflightVouchWithBottleWithSpecificUser:bottleID:reply:) argumentIndex:3 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(vouchWithBottleWithSpecificUser:bottleID:entropy:bottleSalt:tlkShares:reply:) argumentIndex:4 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(preflightVouchWithRecoveryKeyWithSpecificUser:recoveryKey:salt:reply:) argumentIndex:2 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(preflightVouchWithCustodianRecoveryKeyWithSpecificUser:crk:reply:) argumentIndex:2 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(vouchWithRecoveryKeyWithSpecificUser:recoveryKey:salt:tlkShares:reply:) argumentIndex:4 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(recoverTLKSharesForInheritorWithSpecificUser:crk:tlkShares:reply:) argumentIndex:2 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(vouchWithCustodianRecoveryKeyWithSpecificUser:crk:tlkShares:reply:) argumentIndex:4 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(vouchWithRerollWithSpecificUser:oldPeerID:tlkShares:reply:) argumentIndex:4 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(joinWithSpecificUser:voucherData:voucherSig:ckksKeys:tlkShares:preapprovedKeys:flowID:deviceSessionID:canSendMetrics:reply:) argumentIndex:3 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(preflightPreapprovedJoinWithSpecificUser:preapprovedKeys:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(attemptPreapprovedJoinWithSpecificUser:ckksKeys:tlkShares:preapprovedKeys:reply:) argumentIndex:3 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(updateWithSpecificUser:forceRefetch:deviceName:serialNumber:osVersion:policyVersion:policySecrets:syncUserControllableViews:secureElementIdentity:walrusSetting:webAccess:reply:) argumentIndex:2 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(setPreapprovedKeysWithSpecificUser:preapprovedKeys:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(updateTLKsWithSpecificUser:ckksKeys:tlkShares:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(fetchViableBottlesWithSpecificUser:source:reply:) argumentIndex:2 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(fetchViableEscrowRecordsWithSpecificUser:source:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(fetchEscrowContentsWithSpecificUser:reply:) argumentIndex:3 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(fetchPolicyDocumentsWithSpecificUser:versions:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(fetchRecoverableTLKSharesWithSpecificUser:peerID:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(fetchCurrentPolicyWithSpecificUser:modelIDOverride:isInheritedAccount:reply:) argumentIndex:2 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(fetchTrustStateWithSpecificUser:reply:) argumentIndex:2 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(setRecoveryKeyWithSpecificUser:recoveryKey:salt:ckksKeys:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(createCustodianRecoveryKeyWithSpecificUser:recoveryKey:salt:ckksKeys:uuid:kind:reply:) argumentIndex:2 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(removeCustodianRecoveryKeyWithSpecificUser:uuid:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(findCustodianRecoveryKeyWithSpecificUser:uuid:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(requestHealthCheckWithSpecificUser:requiresEscrowCheck:repair:knownFederations:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(getSupportAppInfoWithSpecificUser:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(resetAccountCDPContentsWithSpecificUser:idmsTargetContext:idmsCuttlefishPassword:notifyIdMS:internalAccount:demoAccount:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(removeEscrowCacheWithSpecificUser:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(fetchAccountSettingsWithSpecificUser:forceFetch:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(isRecoveryKeySet:reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:errClasses forSelector:@selector(testSemaphoreWithSpecificUser:arg:reply:) argumentIndex:0 ofReply:YES];

        [interface setClasses:arrayOfSettings forSelector:@selector(fetchAccountSettingsWithSpecificUser:forceFetch:reply:) argumentIndex:0 ofReply:YES];
       
        [interface setClasses:arrayOfCKRecords forSelector:@selector(fetchRecoverableTLKSharesWithSpecificUser:peerID:reply:) argumentIndex:0 ofReply:YES];

        [interface setClasses:arrayOfKeySets   forSelector:@selector(establishWithSpecificUser:ckksKeys:tlkShares:preapprovedKeys:reply:) argumentIndex:1 ofReply:NO];
        [interface setClasses:arrayOfTLKShares forSelector:@selector(establishWithSpecificUser:ckksKeys:tlkShares:preapprovedKeys:reply:) argumentIndex:2 ofReply:NO];
        [interface setClasses:arrayOfCKRecords forSelector:@selector(establishWithSpecificUser:ckksKeys:tlkShares:preapprovedKeys:reply:) argumentIndex:1 ofReply:YES];

        [interface setClasses:arrayOfKeySets   forSelector:@selector(joinWithSpecificUser:voucherData:voucherSig:ckksKeys:tlkShares:preapprovedKeys:flowID:deviceSessionID:canSendMetrics:reply:) argumentIndex:3 ofReply:NO];
        [interface setClasses:arrayOfTLKShares forSelector:@selector(joinWithSpecificUser:voucherData:voucherSig:ckksKeys:tlkShares:preapprovedKeys:flowID:deviceSessionID:canSendMetrics:reply:) argumentIndex:4 ofReply:NO];
        [interface setClasses:arrayOfCKRecords forSelector:@selector(joinWithSpecificUser:voucherData:voucherSig:ckksKeys:tlkShares:preapprovedKeys:flowID:deviceSessionID:canSendMetrics:reply:) argumentIndex:1 ofReply:YES];

        [interface setClasses:arrayOfKeySets   forSelector:@selector(attemptPreapprovedJoinWithSpecificUser:ckksKeys:tlkShares:preapprovedKeys:reply:) argumentIndex:1 ofReply:NO];
        [interface setClasses:arrayOfTLKShares forSelector:@selector(attemptPreapprovedJoinWithSpecificUser:ckksKeys:tlkShares:preapprovedKeys:reply:) argumentIndex:2 ofReply:NO];
        [interface setClasses:arrayOfCKRecords forSelector:@selector(attemptPreapprovedJoinWithSpecificUser:ckksKeys:tlkShares:preapprovedKeys:reply:) argumentIndex:1 ofReply:YES];
        
        [interface setClasses:arrayOfKeySets      forSelector:@selector(vouchWithSpecificUser:
                                                                     peerID:
                                                                     permanentInfo:
                                                                     permanentInfoSig:
                                                                     stableInfo:
                                                                     stableInfoSig:
                                                                     ckksKeys:
                                                                     flowID:
                                                                     deviceSessionID:
                                                                     canSendMetrics:
                                                                     reply:) argumentIndex:6 ofReply:NO];

        [interface setClasses:arrayOfTLKShares forSelector:@selector(vouchWithBottleWithSpecificUser:
                                                                     bottleID:
                                                                     entropy:
                                                                     bottleSalt:
                                                                     tlkShares:
                                                                     reply:) argumentIndex:4 ofReply:NO];
        [interface setClasses:arrayOfTLKShares forSelector:@selector(vouchWithBottleWithSpecificUser:
                                                                     bottleID:
                                                                     entropy:
                                                                     bottleSalt:
                                                                     tlkShares:
                                                                     reply:) argumentIndex:2 ofReply:YES];
        [interface setClasses:arrayOfKeySets forSelector:@selector(setRecoveryKeyWithSpecificUser:
                                                                   recoveryKey:
                                                                   salt:
                                                                   ckksKeys:
                                                                   reply:) argumentIndex:3 ofReply:NO];
        [interface setClasses:arrayOfCKRecords forSelector:@selector(setRecoveryKeyWithSpecificUser:
                                                                     recoveryKey:
                                                                     salt:
                                                                     ckksKeys:
                                                                     reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:arrayOfKeySets forSelector:@selector(createCustodianRecoveryKeyWithSpecificUser:
                                                                   recoveryKey:
                                                                   salt:
                                                                   ckksKeys:
                                                                   uuid:
                                                                   kind:
                                                                   reply:) argumentIndex:3 ofReply:NO];
        [interface setClasses:arrayOfCKRecords forSelector:@selector(createCustodianRecoveryKeyWithSpecificUser:
                                                                   recoveryKey:
                                                                   salt:
                                                                   ckksKeys:
                                                                   uuid:
                                                                   kind:
                                                                   reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:arrayOfTLKShares forSelector:@selector(vouchWithRecoveryKeyWithSpecificUser:
                                                                     recoveryKey:
                                                                     salt:
                                                                     tlkShares:
                                                                     reply:) argumentIndex:3 ofReply:NO];
        [interface setClasses:arrayOfTLKShares forSelector:@selector(vouchWithRecoveryKeyWithSpecificUser:
                                                                     recoveryKey:
                                                                     salt:
                                                                     tlkShares:
                                                                     reply:) argumentIndex:2 ofReply:YES];
        [interface setClasses:arrayOfTLKShares forSelector:@selector(vouchWithCustodianRecoveryKeyWithSpecificUser:
                                                                     crk:
                                                                     tlkShares:
                                                                     reply:) argumentIndex:2 ofReply:NO];
        [interface setClasses:arrayOfTLKShares forSelector:@selector(vouchWithRerollWithSpecificUser:
                                                                     oldPeerID:
                                                                     tlkShares:
                                                                     reply:) argumentIndex:2 ofReply:NO];
        
        [interface setClasses:arrayOfCKRecords forSelector:@selector(prepareInheritancePeerWithSpecificUser:
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
        
        [interface setClasses:arrayOfTLKShares forSelector:@selector(vouchWithCustodianRecoveryKeyWithSpecificUser:
                                                                     crk:
                                                                     tlkShares:
                                                                     reply:) argumentIndex:2 ofReply:YES];

        [interface setClasses:arrayOfTLKShares forSelector:@selector(recoverTLKSharesForInheritorWithSpecificUser:
                                                                             crk:
                                                                             tlkShares:
                                                                             reply:)
                                                                             argumentIndex:2 ofReply:NO];

        [interface setClasses:arrayOfTLKShares forSelector:@selector(recoverTLKSharesForInheritorWithSpecificUser:
                                                                             crk:
                                                                             tlkShares:
                                                                             reply:)
                                                                             argumentIndex:0 ofReply:YES];
        
        [interface setClasses:trustedPeersHelperCustodianRecoveryKey forSelector:@selector(createCustodianRecoveryKeyWithSpecificUser:
                                                                                           recoveryKey:
                                                                                           salt:
                                                                                           ckksKeys:
                                                                                           uuid:
                                                                                           kind:
                                                                                           reply:) argumentIndex:1 ofReply:YES];
        [interface setClasses:trustedPeersHelperCustodianRecoveryKey forSelector:@selector(preflightVouchWithCustodianRecoveryKeyWithSpecificUser:
                                                                                           crk:
                                                                                           reply:) argumentIndex:1 ofReply:NO];
        [interface setClasses:trustedPeersHelperCustodianRecoveryKey forSelector:@selector(vouchWithCustodianRecoveryKeyWithSpecificUser:
                                                                                           crk:
                                                                                           tlkShares:
                                                                                           reply:) argumentIndex:1 ofReply:NO];
        [interface setClasses:trustedPeersHelperCustodianRecoveryKey forSelector:@selector(findCustodianRecoveryKeyWithSpecificUser:uuid:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:trustedPeersHelperHealthCheckResult forSelector:@selector(requestHealthCheckWithSpecificUser:requiresEscrowCheck:repair:knownFederations:reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:trustedPeersHelperPeerState forSelector:@selector(updateWithSpecificUser:
                                                                                forceRefetch:
                                                                                deviceName:
                                                                                serialNumber:
                                                                                osVersion:
                                                                                policyVersion:
                                                                                policySecrets:
                                                                                syncUserControllableViews:
                                                                                secureElementIdentity:
                                                                                walrusSetting:
                                                                                webAccess:
                                                                                reply:) argumentIndex:0 ofReply:YES];

        [interface setClasses:trustedPeersHelperPeerState   forSelector:@selector(fetchTrustStateWithSpecificUser:
                                                                                  reply:) argumentIndex:0 ofReply:YES];
        [interface setClasses:arrayOfTrustedPeersHelperPeer forSelector:@selector(fetchTrustStateWithSpecificUser:
                                                                                  reply:) argumentIndex:1 ofReply:YES];

        [interface setClasses:arrayOfKeySets forSelector:@selector(updateTLKsWithSpecificUser:
                                                                   ckksKeys:
                                                                   tlkShares:
                                                                   reply:) argumentIndex:1 ofReply:NO];
        [interface setClasses:arrayOfTLKShares forSelector:@selector(updateTLKsWithSpecificUser:
                                                                     ckksKeys:
                                                                     tlkShares:
                                                                     reply:) argumentIndex:2 ofReply:NO];
        [interface setClasses:arrayOfCKRecords forSelector:@selector(updateTLKsWithSpecificUser:
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
                        walrus:(TPPBPeerStableInfoSetting*)walrus
                     webAccess:(TPPBPeerStableInfoSetting*)webAccess
{
    if((self = [super init])) {
        _peerID = peerID;
        _identityIsPreapproved = isPreapproved;
        _peerStatus = peerStatus;
        _memberChanges = memberChanges;
        _unknownMachineIDsPresent = unknownMachineIDs;
        _osVersion = osVersion;
        _walrus = walrus;
        _webAccess = webAccess;
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
                         "walrus: %@ "
                         "webAccess: %@"
                         ">",
            self.peerID,
            self.identityIsPreapproved,
            TPPeerStatusToString(self.peerStatus),
            self.memberChanges ? @"YES" : @"NO",
            self.unknownMachineIDsPresent ? @"YES" : @"NO",
            self.osVersion?:@"unknown"
           ,self.walrus
           ,self.webAccess
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
        _walrus = [coder decodeObjectOfClass:[TPPBPeerStableInfoSetting class] forKey:@"walrus"];
        _webAccess = [coder decodeObjectOfClass:[TPPBPeerStableInfoSetting class] forKey:@"webAccess"];
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
    [coder encodeObject:self.walrus forKey:@"walrus"];
    [coder encodeObject:self.webAccess forKey:@"webAccess"];
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
