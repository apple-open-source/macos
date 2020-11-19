//
//  HIDPreferencesHelperClient.h
//  IOHIDFamily
//
//  Created by AB on 10/15/19.
//

#import <Foundation/Foundation.h>
#import <HIDPreferences/HIDPreferencesProtocol.h>

NS_ASSUME_NONNULL_BEGIN

@class HIDPreferencesHelperListener;

@interface HIDPreferencesHelperClient : NSObject <HIDPreferencesProtocol>

-(nullable instancetype) init NS_UNAVAILABLE;
-(nullable instancetype) initWithConnection:(xpc_connection_t) connection listener:(HIDPreferencesHelperListener*) listener;
@end

NS_ASSUME_NONNULL_END
