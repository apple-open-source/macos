//
//  _PMWeakProxy.h
//  LowPowerMode-Embedded
//
//  Created by Pablo Pons Bordes on 4/16/25.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/// A proxy object that weakly retains its target and forwards messages to it.
///
/// If the target becomes `nil` (deallocated), message sends become no-ops
/// (or return `nil`/zero for methods with return values). This is useful for breaking
/// retain cycles, particularly with delegates or callback targets that might outlive
/// the object holding the proxy.
@interface _PMWeakProxy<TargetType : id> : NSProxy

/// The weakly referenced target object to which messages are forwarded.
///
/// Accessing this property returns the current target, or `nil` if the target
/// has been deallocated.
@property (nonatomic, weak, readonly, nullable) TargetType target;

/// Initializes the proxy with the given target object.
/// - Parameter target: The object to weakly reference and forward messages to.
/// - Returns: An initialized `WeakProxy` instance.
- (instancetype)initWithTarget:(TargetType)target;

/// Creates and returns a proxy with the given target object.
/// - Parameter target: The object to weakly reference and forward messages to.
/// - Returns: A new `WeakProxy` instance.
+ (instancetype)proxyWithTarget:(TargetType)target;

@end

NS_ASSUME_NONNULL_END
