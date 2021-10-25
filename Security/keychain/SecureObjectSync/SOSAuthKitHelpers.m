//
//  SOSAuthKitHelpers.m
//  Security
//
//

#import <Foundation/Foundation.h>
#import "SOSAuthKitHelpers.h"
#import <utilities/debugging.h>
#import "keychain/SecureObjectSync/SOSAccount.h"
#import "keychain/SecureObjectSync/SOSAccountPriv.h"
#import "keychain/SecureObjectSync/SOSFullPeerInfo.h"
#import "keychain/SecureObjectSync/SOSPeerInfoV2.h"
#import "keychain/SecureObjectSync/SOSPeerInfoPriv.h"

#if !TARGET_OS_BRIDGE && !TARGET_OS_SIMULATOR && __OBJC2__

/* Note that these will be weak-linked */
#import <AppleAccount/AppleAccount_Private.h>
#import <AuthKit/AuthKit.h>
#import <AuthKit/AuthKit_Private.h>

#define SUPPORT_MID 1

@implementation SOSAuthKitHelpers

@class SOSAuthKitHelpers;

+ (NSString *) machineID {
    NSError *error = nil;
    NSString *retval = nil;
    secnotice("sosauthkit", "Entering machineID");

    if([AKAnisetteProvisioningController class] == nil || [AKAnisetteData class] == nil) {
        secnotice("sosauthkit", "AKAnisette not available");
        return nil;
    }

    AKAnisetteProvisioningController *anisetteController = [AKAnisetteProvisioningController new];
    if(anisetteController) {
        AKAnisetteData *anisetteData = [anisetteController anisetteDataWithError:&error];
        if (anisetteData) {
            retval = [anisetteData.machineID copy];
            if(retval) {
                secnotice("sosauthkit", "machineID is %@", retval);
            } else {
                secnotice("sosauthkit", "Failed to get machineID");
            }
        } else {
            secnotice("sosauthkit", "can't get mID: %@", error);
        }
    } else {
        secnotice("sosauthkit", "can't get controller");
    }
    return retval;
}

static ACAccount *GetPrimaryAccount(void) {
    if([ACAccount class] == nil || [ACAccountStore class] == nil) {
        secnotice("sosauthkit", "ACAccount not available");
        return nil;
    }

    ACAccount *primaryAccount;
    
    ACAccountStore *store = [ACAccountStore new];

    if(!store) {
        secnotice("sosauthkit", "can't get store");
        return nil;
    }
    
    primaryAccount = [store aa_primaryAppleAccount];

    return primaryAccount;
}


+ (void)activeMIDs:(void(^_Nonnull)(NSSet <SOSTrustedDeviceAttributes *> *activeMIDs, NSError *error))complete {
    if([ACAccount class] == nil || [AKDeviceListRequestContext class] == nil || [AKAppleIDAuthenticationController class] == nil || AKServiceNameiCloud == nil) {
        secnotice("sosauthkit", "ACAccount not available");
        complete(NULL, [NSError errorWithDomain:(__bridge NSString *)kSecErrorDomain code:errSecParam userInfo:@{NSLocalizedDescriptionKey : @"AuthKit/AppleAccount not available"}]);
        return;
    }

    ACAccount *primaryAccount;
    AKDeviceListRequestContext *context;
    
    primaryAccount = GetPrimaryAccount();

    if(!primaryAccount) {
        secnotice("sosauthkit", "can't get account");
        complete(NULL, [NSError errorWithDomain:(__bridge NSString *)kSecErrorDomain code:errSecParam userInfo:@{NSLocalizedDescriptionKey : @"no primary account"}]);
        return;
    }

    context = [AKDeviceListRequestContext new];
    if (context == NULL) {
        complete(NULL, [NSError errorWithDomain:(__bridge NSString *)kSecErrorDomain code:errSecParam userInfo:@{NSLocalizedDescriptionKey : @"can't get AKDeviceListRequestContextClass"}]);
        return;
    }
    context.altDSID = primaryAccount.aa_altDSID;
    context.services = @[ AKServiceNameiCloud ];
    
    // -[AKAppleIDAuthenticationController fetchDeviceListWithContext:error:] is not exposed, use a semaphore
    AKAppleIDAuthenticationController *authController = [AKAppleIDAuthenticationController new];
    if(!authController) {
        complete(NULL, [NSError errorWithDomain:(__bridge NSString *)kSecErrorDomain code:errSecParam userInfo:@{NSLocalizedDescriptionKey : @"can't get authController"}]);
        return;
    }

    [authController fetchDeviceListWithContext:context completion:^(NSArray<AKRemoteDevice *> *deviceList, NSError *error) {
        NSMutableSet *mids = [NSMutableSet new];
        for(AKRemoteDevice *akdev in deviceList) {
            SOSTrustedDeviceAttributes *newdev = [SOSTrustedDeviceAttributes new];
            newdev.machineID = akdev.machineId;
            newdev.serialNumber = akdev.serialNumber;
            [mids addObject:newdev];
        }
        if([mids count] == 0) {
            secnotice("sosauthkit", "found no devices in account");
            mids = nil;
        }
        complete(mids, error);
    }];
}

+ (bool) peerinfoHasMID: (SOSAccount *) account {
    SOSPeerInfoRef pi = SOSFullPeerInfoGetPeerInfo(account.fullPeerInfo);
    if(!pi) return true;  // if there's no PI then just say "we don't need one"
    return SOSPeerInfoV2DictionaryHasString(pi, sMachineIDKey);
}



+ (bool) updateMIDInPeerInfo: (SOSAccount *) account {
    NSString *mid = [SOSAuthKitHelpers machineID];
    if(!mid) return true;
    CFErrorRef error = NULL;
    SOSAccountSetValue(account, sMachineIDKey, (__bridge CFStringRef)mid, &error);
    bool peerUpdated = SOSAccountUpdatePeerInfoAndPush(account, CFSTR("Add Machine ID"), &error, ^bool(SOSPeerInfoRef pi, CFErrorRef *error) {
        if(SOSPeerInfoV2DictionaryHasString(pi, sMachineIDKey)) {
            return false;
        }
        secnotice("sosauthkit", "Setting PeerInfo MID to %@", mid);
        SOSPeerInfoV2DictionarySetValue(pi, sMachineIDKey, (__bridge CFStringRef)mid);
        return true;
    });
    if(!peerUpdated) {
        secnotice("sosauthkit", "Failed to record MID in PeerInfo: %@", error);
    }
    CFReleaseNull(error);
    return peerUpdated;
}


+ (bool) accountIsHSA2 {
    bool hsa2 = false;

    if([ACAccount class] == nil || [AKAccountManager class] == nil) {
        secnotice("sosauthkit", "ACAccount not available");
        return false;
    }
    
    ACAccount *primaryAccount = GetPrimaryAccount();
    AKAccountManager *manager = [AKAccountManager new];

    if(manager && primaryAccount) {
        ACAccount *account = [manager authKitAccountWithAltDSID:[manager altDSIDForAccount:primaryAccount]];
        AKAppleIDSecurityLevel securityLevel = [manager securityLevelForAccount:account];
        if(securityLevel == AKAppleIDSecurityLevelHSA2) {
            hsa2 = true;
        } else {
            secnotice("sosauthkit", "Security level is %lu", (unsigned long)securityLevel);
        }
        secnotice("sosauthkit", "Account %s HSA2", (hsa2) ? "is": "isn't" );
    } else {
        secnotice("sosauthkit", "Failed to get manager");
    }
    return hsa2;
}


-(id) initWithActiveMIDS: (NSSet <SOSTrustedDeviceAttributes *> *) theMidList
{
    if ((self = [super init])) {
        NSMutableSet *MmachineIDs = [[NSMutableSet alloc] init];
        NSMutableSet *MserialNumbers = [[NSMutableSet alloc] init];
        _machineIDs = [[NSSet alloc] init];
        _serialNumbers = [[NSSet alloc] init];

        if(!theMidList) return nil;
        _midList = theMidList;

        for(SOSTrustedDeviceAttributes *dev in _midList) {
            if(dev.machineID) {
                [MmachineIDs addObject:dev.machineID];
            }
            if(dev.serialNumber) {
                [MserialNumbers addObject:dev.serialNumber];
            }

        }
        _machineIDs = MmachineIDs;
        _serialNumbers = MserialNumbers;
    }
    return self;
}

// if the ID passed in is null, the peer doesn't have one, we'll say true - we can't tell from the list
- (bool) midIsValidInList: (NSString *) machineId {
    return (machineId) ? [_machineIDs containsObject:machineId]: true;
}

- (bool) serialIsValidInList: (NSString *) serialNumber {
    return (serialNumber) ? [_serialNumbers containsObject:serialNumber]: true;
}

- (bool) isUseful {
    return [ _machineIDs count ] > 0;
}

#else


@implementation SOSAuthKitHelpers

@class SOSAuthKitHelpers;

+ (NSString *) machineID {
    return nil;
}

+ (void)activeMIDs:(void(^_Nonnull)(NSSet<SOSTrustedDeviceAttributes*> *activeMIDs, NSError *error))complete {
    complete(nil, nil);
}

+ (bool) updateMIDInPeerInfo: (SOSAccount *) account {
    return true;
}

+ (bool) peerinfoHasMID: (SOSAccount *) account {
    return true;
}

+ (bool) accountIsHSA2 {
    return false;
}

- (id _Nullable) initWithActiveMIDS: (NSSet *_Nullable) theMidList {
    return nil;
}

- (bool) midIsValidInList: (NSString *_Nullable) machineId {
    return true;
}

- (bool) serialIsValidInList: (NSString *_Nullable) serialNumber {
    return true;
}

- (bool) isUseful {
    return false;
}

#endif

@end

