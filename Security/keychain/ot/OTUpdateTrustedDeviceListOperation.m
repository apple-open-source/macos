
#if OCTAGON

#import <TargetConditionals.h>

#import "keychain/ot/OTUpdateTrustedDeviceListOperation.h"
#import "keychain/categories/NSError+UsefulConstructors.h"

#import <CloudKit/CloudKit_Private.h>

#import <Security/SecKey.h>
#import <Security/SecKeyPriv.h>
#import <SecurityFoundation/SFKey_Private.h>

#if TARGET_OS_WATCH
#import <os/feature_private.h>
#endif /* TARGET_OS_WATCH */

#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/ot/OTSOSUpgradeOperation.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/categories/OTAccountMetadataClassC+KeychainSupport.h"
#import "keychain/ot/OTOperationDependencies.h"

#import "keychain/analytics/SecurityAnalyticsConstants.h"
#import "keychain/analytics/SecurityAnalyticsReporterRTC.h"
#import "keychain/analytics/AAFAnalyticsEvent+Security.h"

@interface OTUpdateTrustedDeviceListOperation ()
@property OTOperationDependencies* deps;

@property OctagonState* stateIfListUpdates;

@property OctagonState* stateIfAuthenticationError;

@property (nullable) OctagonFlag* retryFlag;

// Since we're making callback based async calls, use this operation trick to hold off the ending of this operation
@property NSOperation* finishedOp;
@end

@implementation OTUpdateTrustedDeviceListOperation
@synthesize nextState = _nextState;
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                    listUpdatesState:(OctagonState*)stateIfListUpdates
            authenticationErrorState:(OctagonState*)stateIfNotAuthenticated
                          errorState:(OctagonState*)errorState
                           retryFlag:(OctagonFlag*)retryFlag

{
    if((self = [super init])) {
        _deps = dependencies;

        _intendedState = intendedState;
        _nextState = errorState;
        _stateIfListUpdates = stateIfListUpdates;
        _stateIfAuthenticationError = stateIfNotAuthenticated;

        _retryFlag = retryFlag;
    }
    return self;
}

- (void)groupStart
{
    WEAKIFY(self);
    secnotice("octagon-authkit", "Attempting to update trusted device list");
    
    AAFAnalyticsEventSecurity *eventS = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:nil 
                                                                                                 altDSID:self.deps.activeAccount.altDSID
                                                                                                  flowID:self.deps.flowID
                                                                                         deviceSessionID:self.deps.deviceSessionID
                                                                                               eventName:kSecurityRTCEventNameUpdateTDL
                                                                                         testsAreEnabled:SecCKKSTestsEnabled()
                                                                                          canSendMetrics:self.deps.permittedToSendMetrics
                                                                                                category:kSecurityRTCEventCategoryAccountDataAccessRecovery];
    self.finishedOp = [NSBlockOperation blockOperationWithBlock:^{
        // If we errored in some unknown way, ask to try again!
        STRONGIFY(self);

        if(self.error) {
            if(self.retryFlag == nil) {
                secerror("octagon-authkit: Received an error updating the trusted device list operation, but no retry flag present.");
                [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:self.error];
                return;
            }

            OctagonPendingFlag* pendingFlag = nil;

            if([self.deps.lockStateTracker isLockedError:self.error]) {
                secnotice("octagon-authkit", "Setting the allowed device list failed due to lock state: %@", self.error);
                self.nextState = OctagonStateWaitForUnlock;
                pendingFlag = [[OctagonPendingFlag alloc] initWithFlag:self.retryFlag
                                                            conditions:OctagonPendingConditionsDeviceUnlocked];

            } else {
                secnotice("octagon-authkit", "Error is currently unknown, will not retry: %@", self.error);
            }

            if(pendingFlag) {
                secnotice("octagon-authkit", "Machine ID list error is not fatal: requesting retry: %@",
                          pendingFlag);
                [self.deps.flagHandler handlePendingFlag:pendingFlag];
            }              
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:self.error];
        } else {
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];
        }
    }];
    [self dependOnBeforeGroupFinished:self.finishedOp];

    NSString* altDSID = self.deps.activeAccount.altDSID;
    if(altDSID == nil) {
        secnotice("authkit", "No configured altDSID: %@", self.deps.activeAccount);
        self.error = [NSError errorWithDomain:OctagonErrorDomain
                                         code:OctagonErrorNoAppleAccount
                                  description:@"No altDSID configured"];
        [self runBeforeGroupFinished:self.finishedOp];
        return;
    }

    NSError* localError = nil;
    BOOL isAccountDemo = [self.deps.authKitAdapter accountIsDemoAccountByAltDSID:altDSID error:&localError];
    if(localError) {
        secerror("octagon-authkit: failed to fetch demo account flag: %@", localError);
    }

    [self.deps.authKitAdapter fetchCurrentDeviceListByAltDSID:altDSID reply:^(NSSet<NSString *> * _Nullable machineIDs,
                                                                              NSSet<NSString*>* _Nullable userInitiatedRemovals,
                                                                              NSSet<NSString*>* _Nullable evictedRemovals,
                                                                              NSSet<NSString*>* _Nullable unknownReasonRemovals,
                                                                              NSString* _Nullable version,
                                                                              NSError * _Nullable error) {
        STRONGIFY(self);

#if TARGET_OS_WATCH
        if (error == nil && os_feature_enabled(Security, WatchForceDeviceListFailure)) {
            error = [NSError errorWithDomain:OctagonErrorDomain
                                        code:OctagonErrorInjectedError
                                 description:@"forced device list failure"];
        }
#endif /* TARGET_OS_WATCH */

        if (error) {
            secerror("octagon-authkit: Unable to fetch machine ID list: %@", error);

            if (self.logForUpgrade) {
                [[CKKSAnalytics logger] logRecoverableError:error
                                                   forEvent:OctagonEventUpgradeFetchDeviceIDs
                                             withAttributes:NULL];
            }
            self.error = error;

            if([AKAppleIDAuthenticationErrorDomain isEqualToString:error.domain] && error.code == AKAuthenticationErrorNotPermitted) {
                self.nextState = self.stateIfAuthenticationError;
            }

            [self runBeforeGroupFinished:self.finishedOp];

        } else if (!machineIDs) {
            secerror("octagon-authkit: empty machine id list");
            if (self.logForUpgrade) {
                [[CKKSAnalytics logger] logRecoverableError:error
                                                   forEvent:OctagonEventUpgradeFetchDeviceIDs
                                             withAttributes:NULL];
            }
            self.error = error;
            [self runBeforeGroupFinished:self.finishedOp];
        } else {
            if (self.logForUpgrade) {
                [[CKKSAnalytics logger] logSuccessForEventNamed:OctagonEventUpgradeFetchDeviceIDs];
            }
            [self afterAuthKitFetch:machineIDs 
              userInitiatedRemovals:userInitiatedRemovals
                    evictedRemovals:evictedRemovals
              unknownReasonRemovals:unknownReasonRemovals
                      accountIsDemo:isAccountDemo
                            version:version];
        }
    }];
}

- (void)afterAuthKitFetch:(NSSet<NSString *>*)allowedMachineIDs
    userInitiatedRemovals:(NSSet<NSString *>*)userInitiatedRemovals
          evictedRemovals:(NSSet<NSString *>*)evictedRemovals
    unknownReasonRemovals:(NSSet<NSString *>*)unknownReasonRemovals
            accountIsDemo:(BOOL)accountIsDemo version:(NSString* _Nullable)version
{
    WEAKIFY(self);
    BOOL honorIDMSListChanges = accountIsDemo ? NO : YES;

    if ([self.deps.deviceInformationAdapter isMachineIDOverridden]) {
        honorIDMSListChanges = NO;
    }

    [self.deps.cuttlefishXPCWrapper setAllowedMachineIDsWithSpecificUser:self.deps.activeAccount
                                                       allowedMachineIDs:allowedMachineIDs
                                                   userInitiatedRemovals:userInitiatedRemovals
                                                         evictedRemovals:evictedRemovals
                                                   unknownReasonRemovals:unknownReasonRemovals
                                                    honorIDMSListChanges:honorIDMSListChanges
                                                                 version:version
                                                                   reply:^(BOOL listDifferences, NSError * _Nullable error) {
        STRONGIFY(self);

        if (self.logForUpgrade) {
            [[CKKSAnalytics logger] logResultForEvent:OctagonEventUpgradeSetAllowList hardFailure:true result:error];
        }
        if(error) {
            secnotice("octagon-authkit", "Unable to save machineID allow-list: %@", error);
            self.error = error;
        } else {
            secnotice("octagon-authkit", "Successfully saved machineID allow-list (%@ change), version = %@", listDifferences ? @"some" : @"no", version);
            if(listDifferences) {
                self.nextState = self.stateIfListUpdates;
            } else {
                self.nextState = self.intendedState;
            }
        }

        [self runBeforeGroupFinished:self.finishedOp];
    }];
}

@end

#endif // OCTAGON
