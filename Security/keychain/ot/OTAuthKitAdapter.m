#if OCTAGON

#import <Foundation/Foundation.h>

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
#import "keychain/analytics/AAFAnalyticsEvent+Security.h"
#import "keychain/analytics/SecurityAnalyticsReporterRTC.h"
#import "keychain/analytics/SecurityAnalyticsConstants.h"
#import "keychain/ckks/CKKS.h"

@interface OTAuthKitActualAdapter ()
@property CKKSListenerCollection<OTAuthKitAdapterNotifier>* notifiers;
@end

@interface AKDeviceListResponse (Security)
@property (nonatomic, copy, readonly) NSString *trustedDeviceHash;
@property (nonatomic, copy, readonly) NSString *deletedDeviceHash;
@property (nonatomic, copy, readonly) NSNumber *trustedDevicesUpdateTimestamp;
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
    NSError *error = nil;
    ACAccount *authKitAccount = [manager authKitAccountWithAltDSID:altDSID error:&error];
    AKAppleIDSecurityLevel securityLevel = AKAppleIDSecurityLevelUnknown;
    if (authKitAccount != nil) {
        securityLevel = [manager securityLevelForAccount:authKitAccount];
    } else {
        secnotice("authkit", "failed to get AK account: %@", error);
    }
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

    secnotice("authkit", "Security level for altDSID %@ is %lu.  Account type: %@", altDSID, (unsigned long)securityLevel, accountType);
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

- (NSString* _Nullable)machineID:(NSString* _Nullable)altDSID
                          flowID:(NSString* _Nullable)flowID
                 deviceSessionID:(NSString* _Nullable)deviceSessionID
                  canSendMetrics:(BOOL)canSendMetrics
                           error:(NSError**)error
{
    AAFAnalyticsEventSecurity *eventS = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:nil
                                                                                                 altDSID:altDSID
                                                                                                  flowID:flowID
                                                                                         deviceSessionID:deviceSessionID
                                                                                               eventName:kSecurityRTCEventNameFetchMachineID
                                                                                         testsAreEnabled:SecCKKSTestsEnabled()
                                                                                          canSendMetrics:canSendMetrics
                                                                                                category:kSecurityRTCEventCategoryAccountDataAccessRecovery];

    if([AKAnisetteProvisioningController class] == nil || [AKAnisetteData class] == nil) {
        secnotice("authkit", "AKAnisette not available");
        NSError* localError = [NSError errorWithDomain:OctagonErrorDomain
                                                  code:OctagonErrorRequiredLibrariesNotPresent
                                           description:@"AKAnisette not available"];
        if(error) {
            *error = localError;
        }

        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
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

        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
        return nil;
    }

    NSString* machineID = anisetteData.machineID;

    if(!machineID) {
        secnotice("authkit", "Anisette data does not have machineID");
        NSError* localError = [NSError errorWithDomain:OctagonErrorDomain
                                                  code:OctagonErrorAuthKitMachineIDMissing
                                           description:@"Anisette data does not have machineID"];
        if(error) {
            [SecABC triggerAutoBugCaptureWithType:@"AuthKit" subType:@"missingMID"];
            *error = localError;
        }

        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:localError];
        return nil;
    }

    secnotice("authkit", "fetched current machine ID as: %@", machineID);

    [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];
    return machineID;
}

- (void)fetchCurrentDeviceListByAltDSID:(NSString*)altDSID
                                 flowID:(NSString*)flowID
                        deviceSessionID:(NSString*)deviceSessionID
                                  reply:(void (^)(NSSet<NSString*>* _Nullable machineIDs,
                                                  NSSet<NSString*>* _Nullable userInitiatedRemovals,
                                                  NSSet<NSString*>* _Nullable evictedRemovals,
                                                  NSSet<NSString*>* _Nullable unknownReasonRemovals,
                                                  NSString* _Nullable version,
                                                  NSString* _Nullable trustedDeviceHash,
                                                  NSString* _Nullable deletedDeviceHash,
                                                  NSNumber* _Nullable trustedDevicesUpdateTimestamp,
                                                  NSError* _Nullable error))complete
{
    if([AKDeviceListRequestContext class] == nil || [AKAppleIDAuthenticationController class] == nil) {
        secnotice("authkit", "AuthKit not available");
        complete(nil, nil, nil, nil, nil, nil, nil, nil, [NSError errorWithDomain:OctagonErrorDomain
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
        complete(nil, nil, nil, nil, nil, nil, nil, nil, error);
        return;
    }

    context.altDSID = altDSID;
    // Set to AKDeviceListRequestContextTypeServerOnly to bypass any caching and force a server fetch
    context.type = AKDeviceListRequestContextTypeServerOnly;

    AKAppleIDAuthenticationController *authController = [[AKAppleIDAuthenticationController alloc] init];
    if(authController == nil) {
        NSError *error = [NSError errorWithDomain:OctagonErrorDomain
                                             code:OctagonErrorAuthKitNoAuthenticationController
                                      description:@"can't get authController"];
        [[CKKSAnalytics logger] logUnrecoverableError:error forEvent:OctagonEventAuthKitDeviceList withAttributes:nil];
        complete(nil, nil, nil, nil, nil, nil, nil, nil, error);
        return;
    }

    [authController deviceListWithContext:context completion:^(AKDeviceListResponse *response, NSError *error) {
        if (error != nil) {
            [[CKKSAnalytics logger] logUnrecoverableError:error forEvent:OctagonEventAuthKitDeviceList withAttributes:nil];

            AAFAnalyticsEventSecurity *event = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:@{} 
                                                                                                        altDSID:altDSID
                                                                                                         flowID:flowID
                                                                                                deviceSessionID:deviceSessionID
                                                                                                      eventName:kSecurityRTCEventNameTrustedDeviceListFailure
                                                                                                testsAreEnabled:SecCKKSTestsEnabled()
                                                                                                 canSendMetrics:YES
                                                                                                       category:kSecurityRTCEventCategoryAccountDataAccessRecovery];

            [SecurityAnalyticsReporterRTC sendMetricWithEvent:event success:NO error: error];

            secnotice("authkit", "received no device list(%@): %@", altDSID, error);
            complete(nil, nil, nil, nil, nil, nil, nil, nil, error);
            return;
        }
        if (response == nil) {
            NSError *error = [NSError errorWithDomain:OctagonErrorDomain
                                                 code:OctagonErrorBadAuthKitResponse
                                          description:@"bad response from AuthKit"];
            complete(nil, nil, nil, nil, nil, nil, nil, nil, error);
            [[CKKSAnalytics logger] logUnrecoverableError:error forEvent:OctagonEventAuthKitDeviceList withAttributes:nil];
            return;
        }

        NSMutableSet *mids = [NSMutableSet set];

        NSMutableSet *userInitiatedRemovals = [NSMutableSet set];
        NSMutableSet *evictedMids = [NSMutableSet set];
        NSMutableSet *unknownReasonList = [NSMutableSet set];

        NSString* version = response.deviceListVersion;

        for (AKRemoteDevice *device in response.deviceList) {
            [mids addObject:device.machineId];
            secnotice("authkit", "Current machine ID on list for (%@) version %@: %@", altDSID, version, device.machineId);
        }

        for (AKRemoteDevice *device in response.deletedDeviceList) {
            switch (device.removalReason) {
                case AKRemoteDeviceRemovalReasonUserAction:
                    [userInitiatedRemovals addObject:device.machineId];
                    secnotice("authkit", "User initiated removed machine ID for (%@) version %@: %@", altDSID, version, device.machineId);
                    break;
                case AKRemoteDeviceRemovalReasonDeviceLimitMIDEviction:
                    [evictedMids addObject:device.machineId];
                    secnotice("authkit", "Device evicted due to limit for (%@) version %@: %@", altDSID, version, device.machineId);
                    break;
                case AKRemoteDeviceRemovalReasonUnknown:
                    [unknownReasonList addObject:device.machineId];
                    secnotice("authkit", "Device evicted for unknown reason for (%@) version %@: %@", altDSID, version, device.machineId);
                    break;
                default:
                    secerror("authkit: super shrug here. Device is in the deletedDeviceList but has an undefined removal reason (%ld) for (%@) version %@: %@",
                             (long)device.removalReason, altDSID, version, device.machineId);
                    [unknownReasonList addObject:device.machineId];
                    break;
            }
        }
        NSString* trustedDeviceHash = @"";
        if ([response respondsToSelector:@selector(trustedDeviceHash)]) {
            trustedDeviceHash = response.trustedDeviceHash;
        }

        NSString* deletedDeviceHash = @"";
        if ([response respondsToSelector:@selector(deletedDeviceHash)]) {
            deletedDeviceHash = response.deletedDeviceHash;
        }

        NSNumber* trustedDevicesUpdateTimestamp = nil;
        if ([response respondsToSelector:@selector(trustedDevicesUpdateTimestamp)]) {
            trustedDevicesUpdateTimestamp = response.trustedDevicesUpdateTimestamp;
        }

        complete(mids, userInitiatedRemovals, evictedMids, unknownReasonList, version, trustedDeviceHash, deletedDeviceHash, trustedDevicesUpdateTimestamp, error);
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
    secnotice("authkit", "received notifyAKDeviceListDeltaMessagePayload");

    [self.notifiers iterateListeners:^(id<OTAuthKitAdapterNotifier> listener) {
        [listener notificationOfMachineIDListChange];
    }];
}

@end

#endif // OCTAGON
