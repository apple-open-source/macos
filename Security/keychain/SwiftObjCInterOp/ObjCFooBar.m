//
//  ObjCFooBar.m
//

#import <Foundation/Foundation.h>
#import "ObjCFooBar.h"

#if KCSHARING_SWIFT_OBJC_TESTING // rdar://NNNNN xcode @objc export all swift tests
#import <XCTest/XCTest.h>
#endif
#import "keychain/SwiftObjCInterOp/securityd-SwiftGlue.h"

@implementation FooBar
- (instancetype) init {
    if(self = [super init]){
        self.foo = @"foo";
        self.bar = @"bar";
        return self;
    }
    else {
        NSLog(@"Error while initing FooBar object");
    }
    return self;
}

- (instancetype)initWithFoo:(NSString*)foo withBar:(NSString*)bar {
    if (self = [super init]) {
        self.foo = foo;
        self.bar =  bar;
    }
    else {
        NSLog(@"Error while initing FooBar object");
    }
    return self;
}

- (void)printFooBar {
    NSLog(@"Foo: %@ Bar: %@", self.foo, self.bar);
}

- (NSString *)fooValue {
    return self.foo;
}

- (NSString *)barValue {
    return self.bar;
}

@end
