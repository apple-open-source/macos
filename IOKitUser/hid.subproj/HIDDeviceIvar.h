//
//  HIDDeviceIvar.h
//  iohidobjc
//
//  Created by dekom on 10/17/18.
//

#ifndef HIDDeviceIvar_h
#define HIDDeviceIvar_h

#import <IOKit/hidobjc/hidobjcbase.h>
#import <CoreFoundation/CoreFoundation.h>
#import <objc/objc.h> // for objc_object
#include <os/lock_private.h>

#define HIDDeviceIvar \
io_service_t                            service; \
uint64_t                                regID; \
IOHIDDeviceDeviceInterface              **deviceInterface; \
IOHIDDeviceTimeStampedDeviceInterface   **deviceTimeStampedInterface; \
IOCFPlugInInterface                     **plugInInterface; \
os_unfair_recursive_lock                deviceLock; \
CFMutableDictionaryRef                  properties; \
CFMutableSetRef                         elements; \
CFStringRef                             rootKey; \
CFStringRef                             UUIDKey; \
IONotificationPortRef                   notificationPort; \
io_object_t                             notification; \
CFRunLoopSourceRef                      asyncEventSource; \
CFRunLoopSourceContext1                 sourceContext; \
CFMachPortRef                           queuePort; \
CFRunLoopRef                            runLoop; \
CFStringRef                             runLoopMode; \
dispatch_queue_t                        dispatchQueue; \
dispatch_mach_t                         dispatchMach; \
_Atomic uint32_t                        dispatchStateMask; \
dispatch_block_t                        cancelHandler; \
IOHIDQueueRef                           queue; \
CFArrayRef                              inputMatchingMultiple; \
Boolean                                 loadProperties; \
Boolean                                 isDirty; \
void                                    *transaction; \
os_unfair_recursive_lock                callbackLock; \
CFMutableDataRef                        reportBuffer; \
CFMutableArrayRef                       batchElements; \
CFMutableSetRef                         removalCallbackSet; \
CFMutableSetRef                         inputReportCallbackSet; \
CFMutableSetRef                         inputValueCallbackSet; \
void  * _Atomic                         elementHandler; \
void  * _Atomic                         removalHandler; \
void  * _Atomic                         inputReportHandler;

typedef struct  {
    HIDDeviceIvar
} HIDDeviceStruct;

#endif /* HIDDeviceIvar_h */
