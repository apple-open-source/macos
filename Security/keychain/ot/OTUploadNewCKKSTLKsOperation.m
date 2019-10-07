
#if OCTAGON

#import "utilities/debugging.h"

#import <CloudKit/CloudKit_Private.h>

#import "keychain/ot/OTUploadNewCKKSTLKsOperation.h"
#import "keychain/ot/OTCuttlefishAccountStateHolder.h"
#import "keychain/ot/OTFetchCKKSKeysOperation.h"
#import "keychain/ckks/CKKSCurrentKeyPointer.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CloudKitCategories.h"

#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import "keychain/ot/ObjCImprovements.h"

@interface OTUploadNewCKKSTLKsOperation ()
@property OTOperationDependencies* deps;

@property NSOperation* finishedOp;
@property int retries;
@property int maxRetries;
@property int delay;
@property CKKSNearFutureScheduler* retrySched;
@end

@implementation OTUploadNewCKKSTLKsOperation
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
                       intendedState:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
{
    if((self = [super init])) {
        _deps = dependencies;

        _retries = 0;
        _maxRetries = 5;
        _delay = 1;

        _intendedState = intendedState;
        _nextState = errorState;
    }
    return self;
}

- (void)groupStart
{
    secnotice("octagon", "Beginning to upload any pending CKKS tlks operation");

    WEAKIFY(self);

    NSMutableSet<CKKSKeychainView*>* viewsToUpload = [NSMutableSet set];

    // One (or more) of our sub-CKKSes believes it needs to upload new TLKs.
    for(CKKSKeychainView* view in [self.deps.viewManager currentViews]) {
        if([view.keyHierarchyState isEqualToString:SecCKKSZoneKeyStateWaitForTLKUpload] ||
           [view.keyHierarchyState isEqualToString:SecCKKSZoneKeyStateWaitForTLKCreation]) {
            secnotice("octagon-ckks", "CKKS view %@ needs TLK uploads!", view);
            [viewsToUpload addObject: view];
        }
    }

    if(viewsToUpload.count == 0) {
         // Nothing to do; return to ready
        secnotice("octagon-ckks", "No CKKS views need uploads");
        self.nextState = self.intendedState;
        return;
    }

    self.finishedOp = [NSBlockOperation blockOperationWithBlock:^{
        STRONGIFY(self);
        secnotice("octagon", "Finishing an update TLKs operation with %@", self.error ?: @"no error");
    }];
    [self dependOnBeforeGroupFinished:self.finishedOp];

    OTFetchCKKSKeysOperation* fetchKeysOp = [[OTFetchCKKSKeysOperation alloc] initWithViews:viewsToUpload];
    [self runBeforeGroupFinished:fetchKeysOp];

    CKKSResultOperation* proceedWithKeys = [CKKSResultOperation named:@"upload-tlks-with-keys"
                                                            withBlock:^{
                                                                STRONGIFY(self);
                                                                [self proceedWithKeys:fetchKeysOp.viewKeySets
                                                                     pendingTLKShares:fetchKeysOp.tlkShares
                                                                        viewsToUpload:viewsToUpload];
                                                            }];

    [proceedWithKeys addDependency:fetchKeysOp];
    [self runBeforeGroupFinished:proceedWithKeys];
}

- (BOOL)isRetryable:(NSError* _Nonnull)error {
    return [error isCuttlefishError:CuttlefishErrorTransactionalFailure];
}

- (int)retryDelay:(NSError* _Nonnull)error {
    NSError* underlyingError = error.userInfo[NSUnderlyingErrorKey];
    int ret = self->_delay;
    if (underlyingError) {
        id tmp = underlyingError.userInfo[@"retryafter"];
        if ([tmp isKindOfClass:[NSNumber class]]) {
            ret = [(NSNumber*)tmp intValue];
        }
    }
    ret = MAX(MIN(ret, 32), self->_delay);
    self->_delay *= 2;
    return ret;
}

- (void)proceedWithKeys:(NSArray<CKKSKeychainBackedKeySet*>*)viewKeySets
       pendingTLKShares:(NSArray<CKKSTLKShare*>*)pendingTLKShares
          viewsToUpload:(NSSet<CKKSKeychainView*>*)viewsToUpload
{
    WEAKIFY(self);

    secnotice("octagon-ckks", "Beginning tlk upload with keys: %@", viewKeySets);
    [[self.deps.cuttlefishXPC remoteObjectProxyWithErrorHandler:^(NSError * _Nonnull error) {
        STRONGIFY(self);
        secerror("octagon-ckks: Can't talk with TrustedPeersHelper: %@", error);
        [[CKKSAnalytics logger] logRecoverableError:error forEvent:OctagonEventEstablishIdentity withAttributes:NULL];
        self.error = error;
        [self runBeforeGroupFinished:self.finishedOp];

    }] updateTLKsWithContainer:self.deps.containerName
                       context:self.deps.contextID
                      ckksKeys:viewKeySets
                     tlkShares:pendingTLKShares
                         reply:^(NSArray<CKRecord*>* _Nullable keyHierarchyRecords, NSError * _Nullable error) {
                             STRONGIFY(self);

                             if(error) {
                                 secerror("octagon: Error calling tlk upload: %@", error);
                                 if (self.retries < self.maxRetries && [self isRetryable:error]) {
                                     ++self.retries;
                                     if (!self.retrySched) {
                                         self.retrySched = [[CKKSNearFutureScheduler alloc] initWithName:@"cuttlefish-updatetlk-retry"
                                                                                                   delay:1*NSEC_PER_SEC
                                                                                        keepProcessAlive:true
                                                                               dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                                                   block:^{
                                                 CKKSResultOperation* retryOp = [CKKSResultOperation named:@"retry-updatetlk"
                                                                                                 withBlock:^{
                                                         STRONGIFY(self);
                                                         secnotice("octagon", "retrying (%d/%d) updateTLKs", self.retries, self->_maxRetries);
                                                         [self proceedWithKeys:viewKeySets pendingTLKShares:pendingTLKShares viewsToUpload:viewsToUpload];
                                                     }];
                                                 STRONGIFY(self);
                                                 [self runBeforeGroupFinished:retryOp];
                                             }];
                                     }
                                     int delay_s = [self retryDelay:error];
                                     [self.retrySched waitUntil:delay_s*NSEC_PER_SEC];
                                     [self.retrySched trigger];
                                     return;
                                 }
                                 self.error = error;

                             } else {

                                 // Tell CKKS about our shiny new records!
                                 for(CKKSKeychainView* view in viewsToUpload) {
                                     secnotice("octagon-ckks", "Providing records to %@", view);
                                     [view receiveTLKUploadRecords: keyHierarchyRecords];
                                 }

                                 self.nextState = self.intendedState;
                             }
                             [self runBeforeGroupFinished:self.finishedOp];
                         }];
}

@end

#endif // OCTAGON
