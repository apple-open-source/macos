
#if OCTAGON

#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ckks/CKKSLocalResetOperation.h"
#import "keychain/ckks/CKKSZoneStateEntry.h"
#import "keychain/ckks/CKKSOutgoingQueueEntry.h"
#import "keychain/ckks/CKKSIncomingQueueEntry.h"
#import "keychain/ckks/CKKSCurrentItemPointer.h"
#import "keychain/ckks/CKKSMirrorEntry.h"
#import "keychain/ot/OTDefines.h"

#import "keychain/analytics/SecurityAnalyticsConstants.h"
#import "keychain/analytics/SecurityAnalyticsReporterRTC.h"
#import "keychain/analytics/AAFAnalyticsEvent+Security.h"

@implementation CKKSLocalResetOperation

@synthesize nextState = _nextState;
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(CKKSOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
{
    if(self = [super init]) {
        _deps = dependencies;

        _intendedState = intendedState;
        _nextState = errorState;

        self.name = @"ckks-local-reset";
    }
    return self;
}

- (void)main {
#if TARGET_OS_TV
    [self.deps.personaAdapter prepareThreadForKeychainAPIUseForPersonaIdentifier: nil];
#endif
    [self.deps.databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult {
        [self onqueuePerformLocalReset];
        return CKKSDatabaseTransactionCommit;
    }];
}

- (void)onqueuePerformLocalReset
{
    NSError* localerror = nil;

    AAFAnalyticsEventSecurity *eventS = [[AAFAnalyticsEventSecurity alloc] initWithCKKSMetrics:@{kSecurityRTCFieldNumViews: @(self.deps.views.count)}
                                                                                       altDSID:self.deps.activeAccount.altDSID
                                                                                     eventName:kSecurityRTCEventNameLocalReset
                                                                               testsAreEnabled:SecCKKSTestsEnabled()
                                                                                      category:kSecurityRTCEventCategoryAccountDataAccessRecovery
                                                                                    sendMetric:self.deps.sendMetric];
    for(CKKSKeychainViewState* view in self.deps.views) {
        view.viewKeyHierarchyState = SecCKKSZoneKeyStateResettingLocalData;

        CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry contextID:self.deps.contextID zoneName:view.zoneID.zoneName];
        ckse.ckzonecreated = false;
        ckse.ckzonesubscribed = false; // I'm actually not sure about this: can you be subscribed to a non-existent zone?
        ckse.changeToken = NULL;

        ckse.moreRecordsInCloudKit = NO;
        ckse.lastFetchTime = nil;
        ckse.lastLocalKeychainScanTime = nil;

        [ckse saveToDatabase:&localerror];
        if(localerror && self.error == nil) {
            ckkserror("local-reset", view.zoneID, "couldn't reset zone status: %@", localerror);
            self.error = localerror;
            localerror = nil;
        }

        [CKKSMirrorEntry deleteAllWithContextID:self.deps.contextID zoneID:view.zoneID error:&localerror];
        if(localerror && self.error == nil) {
            ckkserror("local-reset", view.zoneID, "couldn't delete all CKKSMirrorEntry: %@", localerror);
            self.error = localerror;
            localerror = nil;
        }

        [CKKSOutgoingQueueEntry deleteAllWithContextID:self.deps.contextID zoneID:view.zoneID error:&localerror];
        if(localerror && self.error == nil) {
            ckkserror("local-reset", view.zoneID, "couldn't delete all CKKSOutgoingQueueEntry: %@", localerror);
            self.error = localerror;
            localerror = nil;
        }

        [CKKSIncomingQueueEntry deleteAllWithContextID:self.deps.contextID zoneID:view.zoneID error:&localerror];
        if(localerror && self.error == nil) {
            ckkserror("local-reset", view.zoneID, "couldn't delete all CKKSIncomingQueueEntry: %@", localerror);
            self.error = localerror;
            localerror = nil;
        }

        [CKKSKey deleteAllWithContextID:self.deps.contextID zoneID:view.zoneID error:&localerror];
        if(localerror && self.error == nil) {
            ckkserror("local-reset", view.zoneID, "couldn't delete all CKKSKey: %@", localerror);
            self.error = localerror;
            localerror = nil;
        }

        [CKKSTLKShareRecord deleteAllWithContextID:self.deps.contextID zoneID:view.zoneID error:&localerror];
        if(localerror && self.error == nil) {
            ckkserror("local-reset", view.zoneID, "couldn't delete all CKKSTLKShare: %@", localerror);
            self.error = localerror;
            localerror = nil;
        }

        [CKKSCurrentKeyPointer deleteAllWithContextID:self.deps.contextID zoneID:view.zoneID error:&localerror];
        if(localerror && self.error == nil) {
            ckkserror("local-reset", view.zoneID, "couldn't delete all CKKSCurrentKeyPointer: %@", localerror);
            self.error = localerror;
            localerror = nil;
        }

        [CKKSCurrentItemPointer deleteAllWithContextID:self.deps.contextID zoneID:view.zoneID error:&localerror];
        if(localerror && self.error == nil) {
            ckkserror("local-reset", view.zoneID, "couldn't delete all CKKSCurrentItemPointer: %@", localerror);
            self.error = localerror;
            localerror = nil;
        }

        [CKKSDeviceStateEntry deleteAllWithContextID:self.deps.contextID zoneID:view.zoneID error:&localerror];
        if(localerror && self.error == nil) {
            ckkserror("local-reset", view.zoneID, "couldn't delete all CKKSDeviceStateEntry: %@", localerror);
            self.error = localerror;
            localerror = nil;
        }

        if(self.error) {
            break;
        }
    }

    if(!self.error) {
        ckksnotice_global("local-reset", "Successfully deleted all local data for zones: %@", self.deps.views);
        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];
        self.nextState = self.intendedState;
    } else {
        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:self.error];
    }
}

@end

#endif // OCTAGON

