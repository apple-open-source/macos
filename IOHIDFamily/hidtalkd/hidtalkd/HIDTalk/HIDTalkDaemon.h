//
//  HIDTalkDaemon.h
//  IOHIDFamily
//
//  Created by Josh Kergan on 9/17/19.
//

#ifndef HIDTalkDaemon_h
#define HIDTalkDaemon_h

#define kHIDDeviceRemoteService "com.apple.HIDDeviceRemote"
#define kHIDClientRemoteService "com.apple.HIDClientRemote"

@protocol HIDTalkService

- (void)setDispatchQueue:(dispatch_queue_t)queue;
- (bool)registerService:(NSString*)serviceName usingRemoteXPC:(bool)remoteXPC;
- (bool)activateService;

@end

@interface HIDTalkService: NSObject<HIDTalkService>

@end

#endif /* HIDTalkDaemon_h */
