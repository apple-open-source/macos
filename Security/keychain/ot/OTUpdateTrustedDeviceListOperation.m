
#if OCTAGON

#import "keychain/ot/OTUpdateTrustedDeviceListOperation.h"

#import <CloudKit/CloudKit_Private.h>

#import <Security/SecKey.h>
#import <Security/SecKeyPriv.h>
#import <SecurityFoundation/SFKey_Private.h>

#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/ot/OTSOSUpgradeOperation.h"
#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/categories/OTAccountMetadataClassC+KeychainSupport.h"
#import "keychain/ot/OTOperationDependencies.h"

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

    self.finishedOp = [NSBlockOperation blockOperationWithBlock:^{
        // If we errored in some unknown way, ask to try again!
        STRONGIFY(self);

        if(self.error) {
            if(self.retryFlag == nil) {
                secerror("octagon-authkit: Received an error updating the trusted device list operation, but no retry flag present.");
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
        }
    }];
    [self dependOnBeforeGroupFinished:self.finishedOp];

    NSError* localError = nil;
    BOOL isAccountDemo = [self.deps.authKitAdapter accountIsDemoAccount:&localError];
    if(localError) {
        secerror("octagon-authkit: failed to fetch demo account flag: %@", localError);
    }

    [self.deps.authKitAdapter fetchCurrentDeviceList:^(NSSet<NSString *> * _Nullable machineIDs, NSError * _Nullable error) {
        STRONGIFY(self);

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
            [self afterAuthKitFetch:machineIDs accountIsDemo:isAccountDemo];
        }
    }];
}

- (void)afterAuthKitFetch:(NSSet<NSString *>*)allowedMachineIDs accountIsDemo:(BOOL)accountIsDemo
{
    WEAKIFY(self);
    BOOL honorIDMSListChanges = accountIsDemo ? NO : YES;

    [self.deps.cuttlefishXPCWrapper setAllowedMachineIDsWithContainer:self.deps.containerName
                                                              context:self.deps.contextID
                                                    allowedMachineIDs:allowedMachineIDs
                                                        honorIDMSListChanges:honorIDMSListChanges
                                                                reply:^(BOOL listDifferences, NSError * _Nullable error) {
            STRONGIFY(self);

            if (self.logForUpgrade) {
                [[CKKSAnalytics logger] logResultForEvent:OctagonEventUpgradeSetAllowList hardFailure:true result:error];
            }
            if(error) {
                secnotice("octagon-authkit", "Unable to save machineID allow-list: %@", error);
                self.error = error;
            } else {
                secnotice("octagon-authkit", "Successfully saved machineID allow-list (%@ change)", listDifferences ? @"some" : @"no");
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
