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
    kHIDLogCategoryCount
} HIDLogCategory;

__BEGIN_DECLS
// noinline to reduce binary size
os_log_t _HIDLogCategory(HIDLogCategory category) __attribute__ ((noinline));
__END_DECLS

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

#endif /* IOHIDDebug_h */

