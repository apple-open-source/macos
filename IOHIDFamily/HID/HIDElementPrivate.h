//
//  HIDElementPrivate.h
//  HID
//
//  Created by dekom on 10/9/17.
//

#ifndef HIDElementPrivate_h
#define HIDElementPrivate_h

#import <IOKit/hid/IOHIDElement.h>
#import <IOKit/hid/IOHIDValue.h>
#import "HIDElement.h"

@interface HIDElement (priv)

@property (nullable) IOHIDValueRef valueRef;
@property (readonly) uint32_t cookie;

@end

#endif /* HIDElementPrivate_h */
