//
//  ObjCFooBar.h
//  Security
//
//

#import <Foundation/Foundation.h>

#ifndef ObjCFooBar_h
#define ObjCFooBar_h
@interface FooBar : NSObject
@property NSString *foo;
@property NSString *bar;
- (instancetype)init;
- (instancetype)initWithFoo:(NSString*)foo withBar:(NSString*)bar;
- (void)printFooBar;
- (NSString*)fooValue;
- (NSString*)barValue;
@end

#endif /* ObjCFooBar_h */
