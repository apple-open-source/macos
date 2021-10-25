
#if OCTAGON

#import "keychain/ckks/CKKSNewTLKOperation.h"
#import "keychain/ot/OTWaitOnPriorityViews.h"
#import "keychain/ot/ObjCImprovements.h"

@interface OTWaitOnPriorityViews ()
@property OTOperationDependencies* operationDependencies;
@end

@implementation OTWaitOnPriorityViews

- (instancetype)initWithDependencies:(OTOperationDependencies*)dependencies
 {
    if((self = [super init])) {
        _operationDependencies = dependencies;
    }
    return self;
}


- (void)groupStart
{
    WEAKIFY(self);
    CKKSResultOperation* proceedAfterFetch = [CKKSResultOperation named:@"proceed-after-fetch"
                                                            withBlock:^{
        STRONGIFY(self);

        [self addNullableSuccessDependency:self.operationDependencies.ckks.zoneChangeFetcher.inflightFetch];
        
        secnotice("octagon-ckks", "Waiting for CKKS Priority view download for %@", self.operationDependencies.ckks);
        [self addSuccessDependency:[self.operationDependencies.ckks rpcProcessIncomingQueue:nil
                                                                       errorOnClassAFailure:false]];
    }];

    [self runBeforeGroupFinished:proceedAfterFetch];
}
@end

#endif // OCTAGON
