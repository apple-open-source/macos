//
//  HIDServiceClientIvar.h
//  iohidobjc
//
//  Created by dekom on 10/5/18.
//

#ifndef HIDServiceClientIvar_h
#define HIDServiceClientIvar_h

#import <IOKit/hidobjc/hidobjcbase.h>
#import <CoreFoundation/CoreFoundation.h>
#import <objc/objc.h> // for objc_object
#include <os/lock_private.h>

#define HIDServiceClientIvar \
IOHIDEventSystemClientRef   system; \
CFTypeRef                   serviceID; \
os_unfair_recursive_lock    callbackLock; \
struct { \
    IOHIDServiceClientCallback  callback; \
    IOHIDServiceClientBlock     block; \
    void                        *target; \
    void                        *refcon; \
} removal; \
struct { \
    IOHIDVirtualServiceClientCallbacksV2  *callbacks; \
    void                                  *target; \
    void                                  *refcon; \
} virtualService; \
os_unfair_recursive_lock        serviceLock; \
CFMutableDictionaryRef          cachedProperties; \
IOHIDServiceFastPathInterface   **fastPathInterface; \
IOCFPlugInInterface             **plugInInterface; \
void                            *removalHandler; \
uint32_t                        primaryUsagePage; \
uint32_t                        primaryUsage; \
IOHIDServiceClientUsagePair     *usagePairs; \
uint32_t                        usagePairsCount;

typedef struct  {
    HIDServiceClientIvar
} HIDServiceClientStruct;

#endif /* HIDServiceClientIvar_h */
