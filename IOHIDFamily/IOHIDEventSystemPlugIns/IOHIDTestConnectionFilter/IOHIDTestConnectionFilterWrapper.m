//
//  IOHIDTestConnectionFilter.m
//  IOHIDTestConnectionFilter
//
//  Created by Abhishek Nayyar on 3/28/18.
//

#include <CoreFoundation/CoreFoundation.h>
#if COREFOUNDATION_CFPLUGINCOM_SEPARATE
#include <CoreFoundation/CFPlugInCOM.h>
#endif

#include <IOKit/hid/IOHIDEventSystemConnection.h>
#include <IOKit/hid/IOHIDConnectionFilterPlugIn.h>
#include "IOHIDTestConnectionFilter.h"
#include <os/log.h>

void * IOHIDTestConnectionFilterFactory (CFAllocatorRef allocator, CFUUIDRef typeUUID);
static HRESULT _QueryInterface (void *self, REFIID iid, LPVOID *ppv);
static ULONG _AddRef (void *self);
static ULONG _Release (void *self);
static CFTypeRef _copyProperty (void * self, CFStringRef key);
static bool _setProperty (void * self, CFStringRef key, CFTypeRef property);
static IOHIDEventRef _filter(void * self, IOHIDEventRef event);

// The IOHIDNXEventTranslatorServiceFilter function table.
IOHIDConnectionFilterPlugInInterface  __ftbl =
{
    // Required padding for COM
    NULL,
    // These three are the required COM functions
    _QueryInterface,
    _AddRef,
    _Release,
    // IOHIDSimpleConnectionFilterPlugInInterface functions
    _filter,
    // IOHIDConnectionFilterPlugInInterface functions
    NULL,//activate
    NULL,//cancel
    NULL,//setDispatchQueue
    NULL,//setCancelHandler
    _copyProperty,
    _setProperty,
};

@interface HIDTestConnectionFilterWrapper : HIDTestConnectionFilter {

    IOHIDConnectionFilterPlugInInterface  _ftbl;
@public
    IOHIDConnectionFilterPlugInInterface  *_ftblPrt;
}
@end

@implementation HIDTestConnectionFilterWrapper
-(nullable instancetype) init
{
    self = [super init];
    if (!self) {
        return nil;
    }
    
    _ftbl = __ftbl;
    _ftbl._reserved = self;
    _ftblPrt = &_ftbl;
    return self;
}
@end

static HRESULT _QueryInterface (void *self, REFIID iid, LPVOID *ppv)
{
    // Create a CoreFoundation UUIDRef for the requested interface.
    CFUUIDRef interfaceID = CFUUIDCreateFromUUIDBytes( NULL, iid );
    // Test the requested ID against the valid interfaces.
    if (CFEqual(interfaceID, kIOHIDConnectionFilterPlugInInterfaceID)) {
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
static CFTypeRef _copyProperty (void * self, CFStringRef key)
{
    HIDTestConnectionFilterWrapper * obj = (*(IUnknownVTbl **) self)->_reserved;
    if (!obj) {
        return false;
    }
    
    return CFBridgingRetain([obj copyProperty:(__bridge NSString*)key]);
}
static bool _setProperty (void * self, CFStringRef key, CFTypeRef property)
{
    HIDTestConnectionFilterWrapper * obj = (*(IUnknownVTbl **) self)->_reserved;
    if (!obj) {
        return false;
    }
    
    return [obj setProperty:(__bridge NSString*)key property:(__bridge id)property];
}
static IOHIDEventRef _filter(void * self, IOHIDEventRef event)
{
    HIDTestConnectionFilterWrapper * obj = (*(IUnknownVTbl **) self)->_reserved;
    if (!obj) {
        return NULL;
    }
    
    return [obj filter:event];
}
//------------------------------------------------------------------------------
// IOHIDTestConnectionFilterFactory
//------------------------------------------------------------------------------
// Implementation of the factory function for this type.
void * IOHIDTestConnectionFilterFactory (CFAllocatorRef allocator __unused, CFUUIDRef typeUUID)
{
    // If correct type is being requested, allocate an
    // instance of TestType and return the IUnknown interface.
    if (CFEqual(typeUUID, kIOHIDConnectionFilterPlugInTypeID)) {
        HIDTestConnectionFilterWrapper * obj = [[HIDTestConnectionFilterWrapper alloc] init];
        if (obj) {
            return (void *)&obj->_ftblPrt;
        }
        return NULL;
    }
    // If the requested type is incorrect, return NULL.
    return NULL;
}


