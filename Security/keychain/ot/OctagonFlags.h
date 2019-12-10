
#if OCTAGON

#import "keychain/ot/OctagonStateMachineHelpers.h"
#import "keychain/ot/OctagonPendingFlag.h"

NS_ASSUME_NONNULL_BEGIN

// OctagonFlags allow you to set binary flags for consumption by the state machine, similar to processor interrupts
// This allows the state machine to respond to external inputs or requests that don't need a timeout attached to them.
// Setting and removing flags are idempotent.

@protocol OctagonFlagContainer
- (BOOL)_onqueueContains:(OctagonFlag*)flag;
- (NSArray<NSString*>*)dumpFlags;
- (CKKSCondition*)conditionForFlag:(OctagonFlag*)flag;
@end

@protocol OctagonFlagSetter <OctagonFlagContainer>
- (void)setFlag:(OctagonFlag*)flag;
- (void)_onqueueSetFlag:(OctagonFlag*)flag;
@end

@protocol OctagonFlagClearer <OctagonFlagSetter>
- (void)_onqueueRemoveFlag:(OctagonFlag*)flag;
@end

@interface OctagonFlags : NSObject <OctagonFlagContainer,
                                    OctagonFlagSetter,
                                    OctagonFlagClearer>

@property NSMutableDictionary<OctagonFlag*, CKKSCondition*>* flagConditions;

- (instancetype)initWithQueue:(dispatch_queue_t)queue
                        flags:(NSSet<OctagonFlag*>*)possibleFlags;

- (NSString*)contentsAsString;
@end

NS_ASSUME_NONNULL_END

#endif // OCTAGON
