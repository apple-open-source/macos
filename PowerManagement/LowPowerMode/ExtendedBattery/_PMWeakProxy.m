//
//  _PMWeakProxy.m
//  LowPowerMode-Embedded
//
//  Created by Pablo Pons Bordes on 4/16/25.
//

#import "_PMWeakProxy.h"

@implementation _PMWeakProxy {
    __weak id _target;
}

// Designated Initializer
- (instancetype)initWithTarget:(id)target {
    // NSProxy doesn't have a designated initializer like -init.
    // We don't call [super init].
    _target = target;
    return self;
}

+ (instancetype)proxyWithTarget:(id)target {
    return [[_PMWeakProxy alloc] initWithTarget:target];
}

- (id)target {
    return _target;
}

#pragma mark - Message Forwarding

- (NSMethodSignature *)methodSignatureForSelector:(SEL)sel {
    id __strong strongTarget = _target; 
    if (strongTarget) {
        return [strongTarget methodSignatureForSelector:sel];
    } else {
        // Target is nil. Return dummy method signature Avoid crashing.
        return [NSMethodSignature signatureWithObjCTypes:"v@:"];
    }
}

// Forward the invocation to the target if it exists.
- (void)forwardInvocation:(NSInvocation *)invocation {
    id __strong strongTarget = _target; 
    if (strongTarget) {
        // Ensure the target still responds (just to be safer)
        if ([strongTarget respondsToSelector:[invocation selector]]) {
            [invocation setTarget:strongTarget];
            [invocation invoke];
        }
    } else {
        // Target is nil. Do nothing (effectively a no-op).
        // If the method has a non-void return type, the return value will be zero/nil.
        // We might need to explicitly set the return value to 0/nil for non-object types
        // if methodSignatureForSelector returned a non-void signature.
        NSMethodSignature *sig = [invocation methodSignature];
        const char *returnType = [sig methodReturnType];
        // Check if return type is not void (v)
        if (strcmp(returnType, @encode(void)) != 0) {
            // Set return value to zero/nil buffer
            void *nullBuffer = NULL;
            [invocation setReturnValue:&nullBuffer];
        }
    }
}

#pragma mark - Optional Overrides for Introspection

// Make the proxy behave more like the target for introspection checks.

- (BOOL)respondsToSelector:(SEL)aSelector {
    id __strong strongTarget = _target;
    return [strongTarget respondsToSelector:aSelector];
}

- (BOOL)isKindOfClass:(Class)aClass {
    id __strong strongTarget = _target;
    // Check if it's the proxy class itself or if the target is of the specified class
    return [super isKindOfClass:aClass] || [strongTarget isKindOfClass:aClass];
}

- (BOOL)conformsToProtocol:(Protocol *)aProtocol {
    id __strong strongTarget = _target;
    return [strongTarget conformsToProtocol:aProtocol];
}

- (NSUInteger)hash {
    id __strong strongTarget = _target;
    return [strongTarget hash];
}

- (BOOL)isEqual:(id)object {
    id __strong strongTarget = _target;
    // Check if comparing to self, the target, or forward comparison to target
    if (object == self) return YES;
    if (object == strongTarget) return YES;
    return [strongTarget isEqual:object];
}

- (NSString *)description {
    id __strong strongTarget = _target;
    return [NSString stringWithFormat:@"<%@: %p, target: %@>",
            [self class],
            self,
            strongTarget];
}

- (NSString *)debugDescription {
    id __strong strongTarget = _target;
    return [NSString stringWithFormat:@"<%@: %p, target: %@ (%@)>",
            [self class],
            self,
            strongTarget,
            strongTarget ? NSStringFromClass([strongTarget class]) : @"nil"];
}

- (Class)class {
    id __strong strongTarget = _target;
    return strongTarget ? [strongTarget class] : [self class];
}

@end
