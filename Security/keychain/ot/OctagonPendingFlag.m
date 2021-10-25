
#if OCTAGON

#import "keychain/ot/OctagonPendingFlag.h"

NSString* OctagonPendingConditionsToString(OctagonPendingConditions cond)
{
    if((cond & OctagonPendingConditionsDeviceUnlocked) != 0x0) {
        return @"unlock";
    }
    if(cond == 0x0) {
        return @"none";
    }
    return [NSString stringWithFormat:@"Unknown conditions: 0x%x", (int)cond];
}

@interface OctagonPendingFlag()

// If this pending flag depends on a scheduler, it needs to hold a strong reference to that scheduler
@property (nullable) CKKSNearFutureScheduler* scheduler;
@end

@implementation OctagonPendingFlag

- (instancetype)initWithFlag:(OctagonFlag*)flag delayInSeconds:(NSTimeInterval)delay
{
    if ((self = [super init])) {
        _flag = flag;
        _fireTime = [NSDate dateWithTimeIntervalSinceNow:delay];
        _afterOperation = nil;
        _conditions = 0;
    }
    return self;
}

- (instancetype)initWithFlag:(OctagonFlag*)flag
                  conditions:(OctagonPendingConditions)conditions
{
    return [self initWithFlag:flag conditions:conditions delayInSeconds:0];
}

- (instancetype)initWithFlag:(OctagonFlag*)flag
                  conditions:(OctagonPendingConditions)conditions
              delayInSeconds:(NSTimeInterval)delay
{
    if ((self = [super init])) {
        _flag = flag;
        _fireTime = delay > 0 ? [NSDate dateWithTimeIntervalSinceNow:delay] : nil;
        _afterOperation = nil;
        _conditions = conditions;
    }
    return self;
}

- (instancetype)initWithFlag:(OctagonFlag*)flag
                       after:(NSOperation*)op
{
    return [self initWithFlag:flag
                   conditions:0
                        after:op];
}

- (instancetype)initWithFlag:(OctagonFlag*)flag
                   scheduler:(CKKSNearFutureScheduler*)scheduler
{
    return [self initWithFlag:flag
                   conditions:0
                    scheduler:scheduler];
}


- (instancetype)initWithFlag:(OctagonFlag*)flag
                  conditions:(OctagonPendingConditions)conditions
                   scheduler:(CKKSNearFutureScheduler*)scheduler
{
    [scheduler trigger];

    if ((self = [super init])) {
        _flag = flag;
        _fireTime = nil;
        _scheduler = scheduler;
        _afterOperation = scheduler.operationDependency;
        _conditions = conditions;
    }
    return self;
}

- (instancetype)initWithFlag:(OctagonFlag*)flag
                  conditions:(OctagonPendingConditions)conditions
                       after:(NSOperation*)op
{
    if ((self = [super init])) {
        _flag = flag;
        _fireTime = nil;
        _afterOperation = op;
        _conditions = conditions;
    }
    return self;
}

- (NSString*)description {
    if(self.fireTime) {
        return [NSString stringWithFormat:@"<OctagonPendingFlag: %@: %@>", self.flag, self.fireTime];
    } else if(self.afterOperation) {
        if(self.conditions == 0) {
            return [NSString stringWithFormat:@"<OctagonPendingFlag: %@: %@>", self.flag, self.afterOperation];
        } else {
            return [NSString stringWithFormat:@"<OctagonPendingFlag: %@: %@ %@>", self.flag, self.afterOperation, OctagonPendingConditionsToString(self.conditions)];
        }
    } else {
        return [NSString stringWithFormat:@"<OctagonPendingFlag: %@: %@>", self.flag, OctagonPendingConditionsToString(self.conditions)];
    }
}

@end

#endif // OCTAGON
