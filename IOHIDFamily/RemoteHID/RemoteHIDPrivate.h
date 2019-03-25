//
//  RemoteHIDPrivate.h
//  IOHIDFamily
//
//  Created by yg on 2/8/18.
//

#ifndef RemoteHIDPrivate_h
#define RemoteHIDPrivate_h

#import <os/log.h>

static inline os_log_t RemoteHIDLog (void)
{
    static os_log_t log = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        log = os_log_create("com.apple.RemoteHID", "default");
    });
    return log;
}

static inline os_log_t RemoteHIDLogPackets (void)
{
    static os_log_t log = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        log = os_log_create("com.apple.RemoteHID", "packets");
    });
    return log;
}

#endif /* RemoteHIDPrivate_h */
