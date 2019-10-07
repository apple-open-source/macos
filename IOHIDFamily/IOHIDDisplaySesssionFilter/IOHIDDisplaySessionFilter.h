//
//  IOHIDDisplaySessionFilter.h
//  IOHIDFamily
//
//  Created by AB on 2/6/19.
//

#ifndef IOHIDDisplaySessionFilter_h
#define IOHIDDisplaySessionFilter_h

#import <Foundation/Foundation.h>
#import <HID/HID_Private.h>

NS_ASSUME_NONNULL_BEGIN

@interface IOHIDDisplaySessionFilter : NSObject <HIDSessionFilter>

- (nullable instancetype)initWithSession:(HIDSession *)session;

- (nullable id)propertyForKey:(NSString *)key;

- (void)activate;

- (BOOL)setProperty:(nullable id)value
             forKey:(NSString *)key;

- (nullable HIDEvent *)filterEvent:(HIDEvent *)event
                        forService:(HIDEventService *)service;

@end

NS_ASSUME_NONNULL_END


#endif /* IOHIDDisplaySessionFilter_h */
