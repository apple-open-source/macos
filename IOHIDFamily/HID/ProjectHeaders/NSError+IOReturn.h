/*!
 * NSError+IOReturn.h
 * HID
 *
 * Copyright © 2022 Apple Inc. All rights reserved.
 */

#ifndef NSError_IOReturn_h
#define NSError_IOReturn_h

#import <Foundation/Foundation.h>
#import <HID/HIDBase.h>

/*!
 * @category NSError
 *
 * @abstract
 * NSError extension to allow methods to return errors with IOReturn codes.
 */
@interface NSError (IOReturn)

+ (nullable NSError *)errorWithIOReturn:(IOReturn)code;

@end

#endif /* NSError_IOReturn_h */
