//
//  HIDPreferencesTestPlugin.h
//  IOHIDFamily
//
//  Created by AB on 10/8/19.
//

// Session plugin example to show property set

#import <HID/HID_Private.h>

NS_ASSUME_NONNULL_BEGIN

@interface HIDPreferencesTestPlugin : NSObject <HIDSessionFilter>

- (nullable instancetype)initWithSession:(HIDSession *)session;

- (nullable id)propertyForKey:(NSString *)key;

- (BOOL)setProperty:(nullable id)value
             forKey:(NSString *)key;

- (void)activate;
- (nullable HIDEvent *)filterEvent:(HIDEvent *)event forService:(HIDEventService *)service;

@property (weak) HIDSession         *session;

@end

NS_ASSUME_NONNULL_END
