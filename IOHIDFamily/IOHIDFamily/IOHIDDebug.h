//
//  IOHIDDebug.h
//  IOHIDFamily
//
//  Created by YG on 10/5/15.
//
//

#ifndef IOHIDDebug_h
#define IOHIDDebug_h

#include <os/log.h>

static inline os_log_t _HIDLog(void)
{
    static os_log_t log = NULL;
    
    if (!log) {
        log = os_log_create("com.apple.iohid", "default");
    }
    
    return log;
}

#define HIDLog(fmt, ...)        os_log(_HIDLog(), fmt, ##__VA_ARGS__)
#define HIDLogError(fmt, ...)   os_log_error(_HIDLog(), fmt, ##__VA_ARGS__)
#define HIDLogDebug(fmt, ...)   os_log_debug(_HIDLog(), fmt, ##__VA_ARGS__)

/* probably won't use these */
#define HIDLogInfo(fmt, ...)    os_log_info(_HIDLog(), fmt, ##__VA_ARGS__)
#define HIDLogFault(fmt, ...)   os_log_fault(_HIDLog(), fmt, ##__VA_ARGS__)

#endif /* IOHIDDebug_h */
