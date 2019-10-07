#if OCTAGON

#import "OTAuthKitAdapter.h"

#import "utilities/SecCFError.h"
#import "keychain/categories/NSError+UsefulConstructors.h"

#import <AppleAccount/AppleAccount.h>
#import <AppleAccount/AppleAccount_Private.h>
#import <AuthKit/AuthKit.h>
#import <AuthKit/AuthKit_Private.h>
#import <AuthKit/AKDeviceListDeltaMessagePayload.h>
#import <Foundation/NSDistributedNotificationCenter.h>
#import "keychain/ckks/CKKSListenerCollection.h"

#import <AppleAccount/ACAccount+AppleAccount.h>

@interface OTAuthKitActualAdapter ()
@property CKKSListenerCollection<OTAuthKitAdapterNotifier>* notifiers;
@end

@implementation OTAuthKitActualAdapter

- (NSString* _Nullable)primaryiCloudAccountAltDSID
{
    ACAccountStore *store = [[ACAccountStore alloc] init];
    ACAccount* primaryAccount = [store aa_primaryAppleAccount];
    if(!primaryAccount) {
        return nil;
    }

    return [primaryAccount aa_altDSID];
}

- (BOOL)accountIsHSA2ByAltDSID:(NSString*)altDSID
{
    bool hsa2 = false;

    AKAccountManager *manager = [AKAccountManager sharedInstance];
    ACAccount *authKitAccount = [manager authKitAccountWithAltDSID:altDSID];
    AKAppleIDSecurityLevel securityLevel = [manager securityLevelForAccount:authKitAccount];
    if(securityLevel == AKAppleIDSecurityLevelHSA2) {
        hsa2 = true;
    }
    secnotice("security-authkit", "Security level for altDSID %@ is %lu", altDSID, (unsigned long)securityLevel);
    return hsa2;
}

- (NSString* _Nullable)machineID:(NSError**)error
{
    AKAnisetteProvisioningController* anisetteController = [[AKAnisetteProvisioningController alloc] init];
    NSError* localError = nil;
    AKAnisetteData* anisetteData = [anisetteController anisetteDataWithError:&localError];
    if(!anisetteData) {
        secnotice("authkit", "Unable to fetch data: %@", localError);
        if(error) {
            *error = localError;
        }
        return nil;
    }

    NSString* machineID = anisetteData.machineID;
    if(!machineID) {
        secnotice("authkit", "Anisette data does not have machineID");
        if(error) {
            // TODO: this is a terrible error
            *error = [NSError errorWithDomain:(__bridge NSString *)kSecErrorDomain
                                         code:errSecParam
                                  description:@"Anisette data does not have machineID"];
        }
        return nil;
    }

    secnotice("authkit", "fetched current machine ID as: %@", machineID);

    return machineID;
}

- (void)fetchCurrentDeviceList:(void (^)(NSSet<NSString*>* _Nullable machineIDs, NSError* _Nullable error))complete
{
    ACAccountStore *store = [[ACAccountStore alloc] init];
    ACAccount* primaryAccount = [store aa_primaryAppleAccount];
    if(primaryAccount == nil) {
        secnotice("authkit", "can't get account");
        complete(nil, [NSError errorWithDomain:(__bridge NSString *)kSecErrorDomain
                                          code:errSecParam
                                   description:@"no primary account"]);
        return;
    }

    AKDeviceListRequestContext* context = [[AKDeviceListRequestContext alloc] init];
    if (context == nil) {
        complete(nil, [NSError errorWithDomain:(__bridge NSString *)kSecErrorDomain
                                          code:errSecParam
                                   description:@"can't get AKDeviceListRequestContextClass"]);
        return;
    }
    context.altDSID = primaryAccount.aa_altDSID;

    AKAppleIDAuthenticationController *authController = [[AKAppleIDAuthenticationController alloc] init];
    if(authController == nil) {
        complete(nil, [NSError errorWithDomain:(__bridge NSString *)kSecErrorDomain
                                          code:errSecParam
                                   description:@"can't get authController"]);
        return;
    }

    [authController fetchDeviceListWithContext:context completion:^(NSArray<AKRemoteDevice *> *deviceList, NSError *error) {
        if (deviceList) {
            NSMutableSet *mids = [[NSMutableSet alloc] init];

            for (AKRemoteDevice *device in deviceList) {
                [mids addObject:device.machineId];
            }

            secnotice("authkit", "Current machine ID list: %@", mids);
            complete(mids, error);
        } else {
            secnotice("authkit", "received no device list: %@", error);
            complete(nil, error);
        }
    }];
}

- (void)registerNotification:(id<OTAuthKitAdapterNotifier>)newNotifier
{
    if (self.notifiers == nil) {
        self.notifiers = [[CKKSListenerCollection<OTAuthKitAdapterNotifier> alloc] initWithName:@"otauthkitadapter-notifiers"];
        [[NSDistributedNotificationCenter defaultCenter] addObserver:self selector:@selector(notifyAKDeviceList:) name:AKDeviceListChangedNotification object:nil];
    }
    [self.notifiers registerListener:newNotifier];
}

- (void)notifyAKDeviceList:(NSNotification* _Nullable)notification
{
    AKDeviceListDeltaMessagePayload *payload = nil;
    NSDictionary *userInfo = nil;
    if (notification != nil) {
        userInfo = [notification userInfo];
        if (userInfo != nil) {
            payload = [[AKDeviceListDeltaMessagePayload alloc] initWithResponseBody:userInfo];
        }
    }

    secnotice("authkit", "received notifyAKDeviceList: %@, read payload: %@",
              notification.userInfo,
              // Logging the payload logs an address, so clean it up here.
              payload ? @"YES" : @"NO");

    [self.notifiers iterateListeners:^(id<OTAuthKitAdapterNotifier> listener) {
        NSString* altDSID = payload.altDSID;
        NSArray<NSString*>* machineIDs = payload.machineIDs;

        if (altDSID == nil || machineIDs == nil || machineIDs.count == 0) {
            secnotice("authkit", "partial push or no machine IDs in list; treating as incomplete");
            [listener incompleteNotificationOfMachineIDListChange];
            return;
        }
        switch (payload.operation) {
        case AKDeviceListDeltaOperationAdd:
            [listener machinesAdded:machineIDs altDSID:altDSID];
            return;
            break;
        case AKDeviceListDeltaOperationRemove:
            [listener machinesRemoved:machineIDs altDSID:altDSID];
            return;
            break;
        case AKDeviceListDeltaOperationUnknown:
        default:
            break;
        }
        [listener incompleteNotificationOfMachineIDListChange];
    }];
}

@end

#endif // OCTAGON
