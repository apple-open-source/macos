//
//  IOHIDRemoteSessionFilter.m
//  IOHIDRemoteSensorSessionFilter
//
//  Created by yg on 3/4/18.
//  Copyright © 2018 apple. All rights reserved.
//

#import <CoreFoundation/CoreFoundation.h>
#import <Foundation/Foundation.h>
#import <IOKit/hid/IOHIDSessionFilterPlugIn.h>
#import <IOKit/hid/IOHIDEventSystemKeys.h>
#import <IOKit/hid/IOHIDSession.h>
#import <RemoteHID/RemoteHID.h>
#import <objc/runtime.h>

void * IOHIDSessionSensorFilterFactory (CFAllocatorRef allocator, CFUUIDRef typeUUID);
static HRESULT _QueryInterface (void *self, REFIID iid, LPVOID *ppv);
static ULONG _AddRef (void *self);
static ULONG _Release (void *self);
static boolean_t _open (void * self, IOHIDSessionRef session, IOOptionBits options);
static void _close (void * self, IOHIDSessionRef session, IOOptionBits options);
static CFTypeRef _getPropertyForClient (void * self, CFStringRef key, CFTypeRef client);

IOHIDSessionFilterPlugInInterface __filter =
{
    // Required padding for COM
    NULL,
    // These three are the required COM functions
    _QueryInterface,
    _AddRef,
    _Release,
    // IOHIDSimpleSessionFilterPlugInInterface functions
    NULL,
    NULL,
    NULL,
    // IOHIDSessionFilterPlugInInterface functions
    _open,
    _close,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    _getPropertyForClient,
    NULL,
};

@interface HIDSessionFilterWrapper : HIDRemoteDeviceAACPServer {

@public
    IOHIDSessionFilterPlugInInterface   _filter;
    IOHIDSessionFilterPlugInInterface * _filterPrt;
}
-(nullable instancetype) init;
@end

@implementation HIDSessionFilterWrapper

-(nullable instancetype) init;
{
    dispatch_queue_t queue = dispatch_queue_create("com.apple.hidrc", DISPATCH_QUEUE_SERIAL);
    self = [super initWithQueue:queue];
    if (self) {
        _filter = __filter;
        _filter._reserved = self;
        _filterPrt = &_filter;
    }
    dispatch_release(queue);
    return self;
}

@end


static HRESULT _QueryInterface (void *self, REFIID iid, LPVOID *ppv)
{
    // Create a CoreFoundation UUIDRef for the requested interface.
    CFUUIDRef interfaceID = CFUUIDCreateFromUUIDBytes( NULL, iid );
    // Test the requested ID against the valid interfaces.
    if (CFEqual(interfaceID, kIOHIDSessionFilterPlugInInterfaceID)) {
        _AddRef(self);
        *ppv = self;
        CFRelease(interfaceID);
        return S_OK;
    }
    // Requested interface unknown, bail with error.
    *ppv = NULL;
    CFRelease (interfaceID);
    return E_NOINTERFACE;
}

static ULONG _AddRef (void *self)
{
    NSObject * obj = (*(IUnknownVTbl **) self)->_reserved;
    [obj retain];
    return (ULONG) [obj retainCount];
}

static ULONG _Release (void *self)
{
    NSObject * obj = (*(IUnknownVTbl **) self)->_reserved;
    NSUInteger result = [obj retainCount];
    [obj release];
    return (ULONG)--result;
}

static CFTypeRef  _getPropertyForClient (void * self __unused, CFStringRef key __unused, CFTypeRef client __unused)
{
    CFTypeRef result = NULL;
    if (CFEqual(key, CFSTR(kIOHIDSessionFilterDebugKey))) {
        NSMutableDictionary *  dict = [[NSMutableDictionary alloc] initWithCapacity:1];
        dict[@"Class"] = @"HIDRemoteAACPServer";
        result = (CFTypeRef)dict;
    }
    return result;
}

static boolean_t _open (void * self, IOHIDSessionRef session __unused, IOOptionBits options __unused)
{
    HIDSessionFilterWrapper * obj = (*(IUnknownVTbl **) self)->_reserved;
    [obj activate];
    return true;
}

static void _close (void * self, IOHIDSessionRef session __unused, IOOptionBits options __unused)
{
    HIDSessionFilterWrapper * obj = (*(IUnknownVTbl **) self)->_reserved;
    [obj cancel];
}

//------------------------------------------------------------------------------
// IOHIDSessionSensorFilterFactory
//------------------------------------------------------------------------------
// Implementation of the factory function for this type.
void * IOHIDSessionSensorFilterFactory (CFAllocatorRef allocator __unused, CFUUIDRef typeUUID)
{
    // If correct type is being requested, allocate an
    // instance of TestType and return the IUnknown interface.
    if (CFEqual(typeUUID, kIOHIDSessionFilterPlugInTypeID)) {
        HIDSessionFilterWrapper * obj;
//        self = (HIDSessionFilterWrapper *) CFAllocatorAllocate (allocator,
//                                                                class_getInstanceSize([HIDSessionFilterWrapper class]),
//                                                                0);
        obj = [[HIDSessionFilterWrapper alloc] init];
        if (obj) {
            return (void *)&obj->_filterPrt;
        }
        return NULL;
    }
    // If the requested type is incorrect, return NULL.
    return NULL;
}
