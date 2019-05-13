//
//  HIDElementPrivate.h
//  IOHIDFamily
//
//  Created by AB on 1/16/19.
//

#ifndef HIDElementPrivate_h
#define HIDElementPrivate_h

#import <Foundation/Foundation.h>
#import <IOKit/hid/IOHIDDevice.h>

NS_ASSUME_NONNULL_BEGIN

@interface HIDElement (HIDElementPrivate)

- (instancetype) initWithObject:(IOHIDElementRef) element;

@property(nullable) IOHIDValueRef  valueRef;
@property IOHIDElementRef element;
@property (readonly) uint32_t cookie;

@end

NS_ASSUME_NONNULL_END

#endif /* HIDElementPrivate_h */
