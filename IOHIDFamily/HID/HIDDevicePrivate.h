//
//  HIDDevicePrivate.h
//  HID
//
//  Created by dekom on 10/9/17.
//

#ifndef HIDDevicePrivate_h
#define HIDDevicePrivate_h

#import "HIDDevice.h"
#import <IOKit/hid/IOHIDDevice.h>

@interface HIDDevice (priv)

- (BOOL)openSeize: (out NSError  * _Nullable * _Nullable)outError;

@end


#endif /* HIDDevicePrivate_h */
