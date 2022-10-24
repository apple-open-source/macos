//
//  IOHIDEventObjc.m
//  iohidobjc
//
//  Created by dekom on 7/30/18.
//

#import <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFRuntime.h>
#import <IOKit/hid/IOHIDEvent.h>
#import "HIDEventBasePrivate.h"
#include <pthread.h>

static CFTypeID __kIOHIDEventTypeID = _kCFRuntimeNotATypeID;
static pthread_once_t __eventTypeInit = PTHREAD_ONCE_INIT;

static const CFRuntimeClass __IOHIDEventClass = {
    0,                                          // version
    "IOHIDEvent",                               // className
    NULL,                                       // init
    NULL,                                       // copy
    NULL,                                       // finalize
    NULL,                                       // equal
    NULL,                                       // hash
    NULL,                                       // copyFormattingDesc
    NULL,                                       //copyDebugDesc
    NULL,                                       // reclaim
    NULL                                        // refcount
};

// __IOHIDEventRegister
//------------------------------------------------------------------------------
void __IOHIDEventRegister(void)
{
    __kIOHIDEventTypeID = _CFRuntimeRegisterClass(&__IOHIDEventClass);
}

//------------------------------------------------------------------------------
// IOHIDEventGetTypeID
//------------------------------------------------------------------------------
CFTypeID IOHIDEventGetTypeID(void)
{
    if (__kIOHIDEventTypeID == _kCFRuntimeNotATypeID) {
        pthread_once(&__eventTypeInit, __IOHIDEventRegister);
    }

    return __kIOHIDEventTypeID;
}

IOHIDEventRef _IOHIDEventCreate(CFAllocatorRef allocator,
                                CFIndex dataSize,
                                IOHIDEventType type,
                                uint64_t timeStamp,
                                IOOptionBits options)
{
    return (__bridge IOHIDEventRef)[[HIDEvent allocWithZone:(struct _NSZone *)allocator] initWithSize:dataSize
                                                                                                 type:type
                                                                                            timestamp:timeStamp
                                                                                              options:options];
}
