
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

@implementation OctagonPendingFlag

- (instancetype)initWithFlag:(OctagonFlag*)flag delayInSeconds:(NSTimeInterval)delay
{
    if ((self = [super init])) {
        _flag = flag;
        _fireTime = [NSDate dateWithTimeIntervalSinceNow:delay];
        _conditions = 0;
    }
    return self;
}

- (instancetype)initWithFlag:(OctagonFlag*)flag
                  conditions:(OctagonPendingConditions)conditions
{
    if ((self = [super init])) {
        _flag = flag;
        _fireTime = nil;
        _conditions = conditions;
    }
    return self;
}

- (NSString*)description {
    if(self.fireTime) {
        return [NSString stringWithFormat:@"<OctagonPendingFlag: %@: %@>", self.flag, self.fireTime];
    } else {
        return [NSString stringWithFormat:@"<OctagonPendingFlag: %@: %@>", self.flag, OctagonPendingConditionsToString(self.conditions)];
    }
}

@end

#endif // OCTAGON
