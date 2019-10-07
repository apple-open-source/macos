
#if OCTAGON

#import <Foundation/Foundation.h>
#import "keychain/ot/OctagonStateMachineHelpers.h"

NS_ASSUME_NONNULL_BEGIN

// An OctagonPendingFlag asks the state machine to add a flag in the future, when some conditions are met

// Currently, this is only time-based.
// Future planned conditions include "device is probably unlocked" and "device has network again"

typedef NS_OPTIONS(NSUInteger, OctagonPendingConditions) {
    OctagonPendingConditionsDeviceUnlocked = 1,
};

NSString* OctagonPendingConditionsToString(OctagonPendingConditions cond);

@interface OctagonPendingFlag : NSObject
@property (readonly) OctagonFlag* flag;

// NSDate after which this flag should become unpending
@property (nullable, readonly) NSDate* fireTime;

@property (readonly) OctagonPendingConditions conditions;

- (instancetype)initWithFlag:(OctagonFlag*)flag delayInSeconds:(NSTimeInterval)delay;
- (instancetype)initWithFlag:(OctagonFlag*)flag conditions:(OctagonPendingConditions)conditions;
@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON
