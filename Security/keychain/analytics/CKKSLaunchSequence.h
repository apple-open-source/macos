//
//  CKKSLaunchSequence.h
//
//  Takes a sequence of events and report their time relative from the starting point
//  Duplicate events are counted.
//
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface CKKSLaunchSequence : NSObject
@property (readonly) bool launched;
@property (assign) bool firstLaunch;

- (instancetype)init NS_UNAVAILABLE;

// name should be dns reverse notation, com.apple.label
- (instancetype)initWithRocketName:(NSString *)name;

// value must be a valid JSON compatible type
- (void)addAttribute:(NSString *)key value:(id)value;
- (void)addEvent:(NSString *)eventname;

- (void)launch;

- (void)addDependantLaunch:(NSString *)name child:(CKKSLaunchSequence *)child;

// For including in human readable diagnostics
- (NSArray<NSString *> *)eventsByTime;
@end

NS_ASSUME_NONNULL_END
