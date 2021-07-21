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

typedef enum {
    kHIDLogCategoryDefault,
    kHIDLogUPS,
    kHIDLogActivity,
    kHIDLogServicePlugin,
    kHIDLogCategoryCount
} HIDLogCategory;

static inline os_log_t _HIDLogCategory(HIDLogCategory category)
{
    static os_log_t log[kHIDLogCategoryCount] = { 0 };
    
    if (!log[0]) {
        log[kHIDLogCategoryDefault] = os_log_create("com.apple.iohid", "default");
        log[kHIDLogUPS]             = os_log_create("com.apple.iohid", "ups");
        log[kHIDLogActivity]        = os_log_create("com.apple.iohid", "activity");
        log[kHIDLogServicePlugin]   = os_log_create("com.apple.iohid", "serviceplugin");
    }
    
    return log[category];
}

#define HIDLog(fmt, ...)        os_log(_HIDLogCategory(kHIDLogCategoryDefault), fmt "\n", ##__VA_ARGS__)
#define HIDLogError(fmt, ...)   os_log_error(_HIDLogCategory(kHIDLogCategoryDefault), fmt "\n", ##__VA_ARGS__)
#define HIDLogDebug(fmt, ...)   os_log_debug(_HIDLogCategory(kHIDLogCategoryDefault), fmt "\n", ##__VA_ARGS__)
#define HIDLogInfo(fmt, ...)    os_log_info(_HIDLogCategory(kHIDLogCategoryDefault), fmt "\n", ##__VA_ARGS__)
#define HIDLogFault(fmt, ...)   os_log_fault(_HIDLogCategory(kHIDLogCategoryDefault), fmt "\n", ##__VA_ARGS__)

#define HIDServiceLogFault(fmt, ...)   HIDLogFault("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDServiceLogError(fmt, ...)   HIDLogError("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDServiceLog(fmt, ...)        HIDLog("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDServiceLogInfo(fmt, ...)    HIDLogInfo("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDServiceLogDebug(fmt, ...)   HIDLogDebug("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)

#define UPSLog(fmt, ...)        os_log(_HIDLogCategory(kHIDLogUPS), fmt, ##__VA_ARGS__)
#define UPSLogError(fmt, ...)   os_log_error(_HIDLogCategory(kHIDLogUPS), fmt, ##__VA_ARGS__)
#define UPSLogDebug(fmt, ...)   os_log_debug(_HIDLogCategory(kHIDLogUPS), fmt, ##__VA_ARGS__)

#define HIDLogActivityDebug(fmt, ...)   os_log_debug(_HIDLogCategory(kHIDLogActivity), fmt, ##__VA_ARGS__)


typedef enum {
    kHIDTraceGetReport = 1,
    kHIDTraceSetReport,
    kHIDTraceHandleReport,
} HIDTraceFunctionType;


#endif /* IOHIDDebug_h */

