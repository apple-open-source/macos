#if OCTAGON

#import "OTAuthKitAdapter.h"
#import "OTConstants.h"

#import "utilities/SecCFError.h"
#import "keychain/categories/NSError+UsefulConstructors.h"

#import <AppleAccount/AppleAccount.h>
#import <AppleAccount/AppleAccount_Private.h>
#import <AuthKit/AuthKit.h>
#import <AuthKit/AuthKit_Private.h>
#import <AuthKit/AKDeviceListDeltaMessagePayload.h>
#import <Foundation/NSDistributedNotificationCenter.h>
#import "keychain/ckks/CKKSListenerCollection.h"
#import "keychain/ckks/CKKSAnalytics.h"

#include "utilities/SecABC.h"

#import <AppleAccount/ACAccount+AppleAccount.h>

@interface OTAuthKitActualAdapter ()
@property CKKSListenerCollection<OTAuthKitAdapterNotifier>* notifiers;
@end

@implementation OTAuthKitActualAdapter

- (BOOL)accountIsCDPCapableByAltDSID:(NSString*)altDSID
{
    if([ACAccount class] == nil || [AKAccountManager class] == nil) {
        secnotice("authkit", "AuthKit not available");
        return NO;
    }

    BOOL isCdpCapable = NO;

    AKAccountManager *manager = [AKAccountManager sharedInstance];
    ACAccount *authKitAccount = [manager authKitAccountWithAltDSID:altDSID];
    AKAppleIDSecurityLevel securityLevel = [manager securityLevelForAccount:authKitAccount];
    if(securityLevel == AKAppleIDSecurityLevelHSA2 || securityLevel == AKAppleIDSecurityLevelManaged) {
        isCdpCapable = YES;
    }

    NSString* accountType = nil;
    switch (securityLevel)
    {
        case AKAppleIDSecurityLevelUnknown:
            accountType = @"Unknown";
            break;
        case AKAppleIDSecurityLevelPasswordOnly:
            accountType = @"PasswordOnly";
            break;
        case AKAppleIDSecurityLevelStandard:
            accountType = @"Standard";
            break;
        case AKAppleIDSecurityLevelHSA1:
            accountType = @"HSA1";
            break;
        case AKAppleIDSecurityLevelHSA2:
            accountType = @"HSA2";
            break;
        case AKAppleIDSecurityLevelManaged:
            accountType = @"Managed";
            break;
        default:
            accountType = @"oh no please file a radar to Security | iCloud Keychain security level";
            break;
    }

    secnotice("authkit", "Security level for altDSID %@ is %lu.  Account type: %@", [manager altDSIDForAccount:authKitAccount], (unsigned long)securityLevel, accountType);
    return isCdpCapable;
}

- (BOOL)accountIsDemoAccountByAltDSID:(NSString*)altDSID error:(NSError**)error
{
    AKAccountManager *manager = [AKAccountManager sharedInstance];
    ACAccount *authKitAccount = [manager authKitAccountWithAltDSID:altDSID];
    BOOL isDemo = [manager demoAccountForAccount:authKitAccount];

    secnotice("authkit", "Account with altDSID %@ is a demo account: %{bool}d", altDSID, isDemo);
    return isDemo;
}

- (NSString* _Nullable)machineID:(NSError**)error
{
    if([AKAnisetteProvisioningController class] == nil || [AKAnisetteData class] == nil) {
        secnotice("authkit", "AKAnisette not available");
        if(error) {
            *error = [NSError errorWithDomain:OctagonErrorDomain
                                         code:OctagonErrorRequiredLibrariesNotPresent
                                  description:@"AKAnisette not available"];
        }
        return nil;
    }

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
            [SecABC triggerAutoBugCaptureWithType:@"AuthKit" subType:@"missingMID"];
            *error = [NSError errorWithDomain:OctagonErrorDomain
                                         code:OctagonErrorAuthKitMachineIDMissing
                                  description:@"Anisette data does not have machineID"];
        }
        return nil;
    }

    secnotice("authkit", "fetched current machine ID as: %@", machineID);
    return machineID;
}

- (void)fetchCurrentDeviceListByAltDSID:(NSString*)altDSID
                                  reply:(void (^)(NSSet<NSString*>* _Nullable machineIDs, NSString* _Nullable version, NSError* _Nullable error))complete
{
    if([AKDeviceListRequestContext class] == nil || [AKAppleIDAuthenticationController class] == nil) {
        secnotice("authkit", "AuthKit not available");
        complete(nil, nil, [NSError errorWithDomain:OctagonErrorDomain
                                               code:OctagonErrorRequiredLibrariesNotPresent
                                        description:@"AKAnisette not available"]);
        return;
    }

    AKDeviceListRequestContext* context = [[AKDeviceListRequestContext alloc] init];
    if (context == nil) {
        NSError *error = [NSError errorWithDomain:OctagonErrorDomain
                                             code:OctagonErrorAuthKitAKDeviceListRequestContextClass
                                      description:@"can't get AKDeviceListRequestContextClass"];
        [[CKKSAnalytics logger] logUnrecoverableError:error forEvent:OctagonEventAuthKitDeviceList withAttributes:nil];
        complete(nil, nil, error);
        return;
    }

    context.altDSID = altDSID;

    AKAppleIDAuthenticationController *authController = [[AKAppleIDAuthenticationController alloc] init];
    if(authController == nil) {
        NSError *error = [NSError errorWithDomain:OctagonErrorDomain
                                             code:OctagonErrorAuthKitNoAuthenticationController
                                      description:@"can't get authController"];
        [[CKKSAnalytics logger] logUnrecoverableError:error forEvent:OctagonEventAuthKitDeviceList withAttributes:nil];
        complete(nil, nil, error);
        return;
    }

    [authController deviceListWithContext:context completion:^(AKDeviceListResponse *response, NSError *error) {
            if (error != nil) {
                [[CKKSAnalytics logger] logUnrecoverableError:error forEvent:OctagonEventAuthKitDeviceList withAttributes:nil];
                secnotice("authkit", "received no device list(%@): %@", altDSID, error);
                complete(nil, nil, error);
                return;
            }
            if (response == nil) {
                NSError *error = [NSError errorWithDomain:OctagonErrorDomain
                                                     code:OctagonErrorBadAuthKitResponse
                                              description:@"bad response from AuthKit"];
                complete(nil, nil, error);
                [[CKKSAnalytics logger] logUnrecoverableError:error forEvent:OctagonEventAuthKitDeviceList withAttributes:nil];
                return;
            }

            NSMutableSet *mids = [[NSMutableSet alloc] init];
            NSString* version = response.deviceListVersion;

            for (AKRemoteDevice *device in response.deviceList) {
                [mids addObject:device.machineId];
                secnotice("authkit", "Current machine ID on list for (%@) version %@: %@", altDSID, version, device.machineId);
            }

            complete(mids, version, error);
            [[CKKSAnalytics logger] logSuccessForEventNamed:OctagonEventAuthKitDeviceList];
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
    if([AKDeviceListDeltaMessagePayload class] == nil) {
        secnotice("authkit", "AuthKit not available; dropping device list notification");
        return;
    }

    NSDictionary *userInfo = nil;
    if (notification != nil) {
        userInfo = [notification userInfo];
    }

    [self deliverAKDeviceListDeltaMessagePayload:userInfo];
}

- (void)deliverAKDeviceListDeltaMessagePayload:(NSDictionary* _Nullable)notificationDictionary
{
    AKDeviceListDeltaMessagePayload* payload = [[AKDeviceListDeltaMessagePayload alloc] initWithResponseBody:notificationDictionary];

    secnotice("authkit", "received notifyAKDeviceListDeltaMessagePayload: %@, parsed payload: %{BOOL}d",
              notificationDictionary,
              // Logging the payload logs an address, so clean it up here.
              payload != nil);

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
