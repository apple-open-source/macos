
#if OCTAGON

#import "utilities/debugging.h"

#import "keychain/ot/OTLocalCKKSResetOperation.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSViewManager.h"

#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/ObjCImprovements.h"
#import <KeychainCircle/AAFAnalyticsEvent+Security.h>
#import <KeychainCircle/SecurityAnalyticsConstants.h>

@interface OTLocalCKKSResetOperation ()
@property OTOperationDependencies* operationDependencies;

@property NSOperation* finishedOp;
@end

@implementation OTLocalCKKSResetOperation
@synthesize nextState = _nextState;
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
{
    if((self = [super init])) {
        _operationDependencies = dependencies;

        _intendedState = intendedState;
        _nextState = errorState;
    }
    return self;
}

- (void)groupStart
{
    secnotice("octagon-ckks", "Beginning an 'reset CKKS' operation");

    NSDictionary* metrics = nil;
    metrics = @{kSecurityRTCFieldAccountIsW : @(self.operationDependencies.accountIsW)};

    AAFAnalyticsEventSecurity *event = [[AAFAnalyticsEventSecurity alloc] initWithKeychainCircleMetrics:metrics
                                                                                                altDSID:self.operationDependencies.activeAccount.altDSID
                                                                                                 flowID:self.operationDependencies.flowID
                                                                                        deviceSessionID:self.operationDependencies.deviceSessionID
                                                                                              eventName:kSecurityRTCEventNameOTLocalCKKSResetOperation
                                                                                        testsAreEnabled:SecCKKSTestsEnabled()
                                                                                         canSendMetrics:self.operationDependencies.permittedToSendMetrics
                                                                                               category:kSecurityRTCEventCategoryAccountDataAccessRecovery];

    WEAKIFY(self);

    self.finishedOp = [NSBlockOperation blockOperationWithBlock:^{
        STRONGIFY(self);
        secnotice("octagon-ckks", "Finishing a ckks-local-reset operation with %@", self.error ?: @"no error");
        if (self.error) {
            [event sendMetricWithResult:NO error:self.error];
        } else {
            [event sendMetricWithResult:YES error:nil];
        }
    }];
    [self dependOnBeforeGroupFinished:self.finishedOp];

    [self.operationDependencies.ckks rpcResetLocal:nil reply: ^(NSError* _Nullable resultError) {
        STRONGIFY(self);

        secnotice("octagon-ckks", "Finished ckks-local-reset with %@", self.error ?: @"no error");

        if(resultError == nil) {
            self.nextState = self.intendedState;
        } else {
            self.error = resultError;
        }
        [self runBeforeGroupFinished:self.finishedOp];
    }];
}

@end

#endif // OCTAGON
