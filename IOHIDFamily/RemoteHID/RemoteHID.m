//
//  RemoteHID.m
//  RemoteHID
//
//  Created by yg on 11/5/18.
//

#import <Foundation/Foundation.h>
#import "RemoteHIDPrivate.h"

os_log_t RemoteHIDLog (void)
{
    static os_log_t log = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        log = os_log_create("com.apple.RemoteHID", "default");
    });
    return log;
}

os_log_t RemoteHIDLogPackets (void)
{
    static os_log_t log = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        log = os_log_create("com.apple.RemoteHID", "packets");
    });
    return log;
}
