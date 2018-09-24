//
//  SOSAuthKitHelpers.m
//  Security
//
//  Created by murf on 6/19/18.
//

#import <Foundation/Foundation.h>
#import "SOSAuthKitHelpers.h"
#import <utilities/debugging.h>
#import <Security/SecureObjectSync/SOSAccount.h>
#import <Security/SecureObjectSync/SOSAccountPriv.h>
#import <Security/SecureObjectSync/SOSFullPeerInfo.h>
#import <Security/SecureObjectSync/SOSPeerInfoV2.h>
#import <Security/SecureObjectSync/SOSPeerInfoPriv.h>

#if !TARGET_OS_BRIDGE && !TARGET_OS_SIMULATOR
#import <AppleAccount/AppleAccount_Private.h>
#import <AuthKit/AuthKit.h>
#import <AuthKit/AuthKit_Private.h>

#define SUPPORT_MID 1
#endif

@implementation SOSAuthKitHelpers

#if SUPPORT_MID

SOFT_LINK_FRAMEWORK(PrivateFrameworks, AuthKit);
SOFT_LINK_FRAMEWORK(Frameworks, Accounts);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstrict-prototypes"
SOFT_LINK_CLASS(AuthKit, AKAccountManager);
SOFT_LINK_CLASS(AuthKit, AKAnisetteProvisioningController);
SOFT_LINK_CLASS(AuthKit, AKAppleIDAuthenticationController);
SOFT_LINK_CLASS(AuthKit, AKDeviceListRequestContext);
SOFT_LINK_CLASS(Accounts, Accounts);
SOFT_LINK_CLASS(Accounts, ACAccountStore);
SOFT_LINK_CONSTANT(AuthKit, AKServiceNameiCloud, const NSString *);
#pragma clang diagnostic pop



+ (NSString *) machineID {
    NSError *error = nil;
    NSString *retval = nil;
    secnotice("sosauthkit", "Entering machineID");

    AKAnisetteProvisioningController *anisetteController = [getAKAnisetteProvisioningControllerClass() new];
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

+ (bool) peerinfoHasMID: (SOSAccount *) account {
    SOSPeerInfoRef pi = SOSFullPeerInfoGetPeerInfo(account.fullPeerInfo);
    if(!pi) return false;
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

+ (void)activeMIDs:(void(^_Nonnull)(NSSet *activeMIDs, NSError *error))complete {
    AKDeviceListRequestContext *context;
    ACAccount *primaryAccount;

    ACAccountStore *store = [getACAccountStoreClass() new];
    if(!store) {
        complete(NULL, [NSError errorWithDomain:(__bridge NSString *)kSecErrorDomain code:errSecParam userInfo:@{NSLocalizedDescriptionKey : @"can't get store"}]);
        return;
    }
    primaryAccount = [store aa_primaryAppleAccount];
    if(!primaryAccount) {
        secnotice("sosauthkit", "can't get account");
        complete(NULL, [NSError errorWithDomain:(__bridge NSString *)kSecErrorDomain code:errSecParam userInfo:@{NSLocalizedDescriptionKey : @"no primary account"}]);
        return;
    }

    context = [getAKDeviceListRequestContextClass() new];
    if (context == NULL) {
        complete(NULL, [NSError errorWithDomain:(__bridge NSString *)kSecErrorDomain code:errSecParam userInfo:@{NSLocalizedDescriptionKey : @"can't get AKDeviceListRequestContextClass"}]);
        return;
    }
    context.altDSID = primaryAccount.aa_altDSID;
    context.services = @[ getAKServiceNameiCloud() ];

    AKAppleIDAuthenticationController *authController = [getAKAppleIDAuthenticationControllerClass() new];
    if(!authController) {
        complete(NULL, [NSError errorWithDomain:(__bridge NSString *)kSecErrorDomain code:errSecParam userInfo:@{NSLocalizedDescriptionKey : @"can't get authController"}]);
        return;
    }

    [authController fetchDeviceListWithContext:context completion:^(NSArray<AKRemoteDevice *> *deviceList, NSError *error) {
        NSMutableSet *mids = [[NSMutableSet alloc] init];
        if (deviceList != nil) {
            for (AKRemoteDevice *device in deviceList) {
                [mids addObject:device.machineId];
            }
        } else {
            secnotice("sosauthkit", "got no mIDs: %@", error);
        }
        if([mids count] == 0) {
            secnotice("sosauthkit", "found not devices in account");
            mids = nil;
        }

        complete(mids, error);
    }];
}


#else /* TARGET_OS_BRIDGE || TARGET_OS_SIMULATOR */

+ (NSString *) machineID {
    return nil;
}

+ (void)activeMIDs:(void(^_Nonnull)(NSSet *activeMIDs, NSError *error))complete {
    complete(NULL, NULL);
}

+ (bool) updateMIDInPeerInfo: (SOSAccount *) account {
    return true;
}

+ (bool) peerinfoHasMID: (SOSAccount *) account {
    return false;
}

#endif

@end

