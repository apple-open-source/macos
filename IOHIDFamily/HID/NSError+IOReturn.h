//
//  NSError+IOReturn.h
//  IOHIDFamily
//
//  Created by Matty on 7/10/18.
//

#ifndef NSError_IOReturn_h
#define NSError_IOReturn_h

#import <Foundation/Foundation.h>
#import <HID/HIDBase.h>

@interface NSError (IOReturn)

+ (nullable NSError *)errorWithIOReturn:(IOReturn)code;

@end

#endif /* NSError_IOReturn_h */
