//
//  IOHIDDisplaySessionFilterWrapper.m
//  IOHIDDisplaySessionFilter
//
//  Created by AB on 1/25/19.
//

#import <Foundation/Foundation.h>
#include <CoreFoundation/CoreFoundation.h>
#if COREFOUNDATION_CFPLUGINCOM_SEPARATE
#include <CoreFoundation/CFPlugInCOM.h>
#endif
#include <IOKit/hid/IOHIDSessionFilterPlugIn.h>
#include <IOKit/hid/IOHIDEventSystemKeys.h>
#include "IOHIDDisplaySessionFilter.h"

//82A14D6D-0138-4DE2-8784-A2DBEEB6A100
#define kIOHIDDisplaySessionFilterFactory CFUUIDGetConstantUUIDWithBytes(kCFAllocatorSystemDefault, 0x82, 0xA1, 0x4D, 0x6D, 0x01, 0x38, 0x4D, 0xE2, 0x87, 0x84, 0xA2, 0xDB, 0xEE, 0xB6, 0xA1, 0x00)

void * IOHIDDisplaySessionFilterFactory (CFAllocatorRef allocator, CFUUIDRef typeUUID);
static HRESULT _QueryInterface (void *self, REFIID iid, LPVOID *ppv);
static ULONG _AddRef (void *self);
static ULONG _Release (void *self);
static boolean_t _open (void * self, IOHIDSessionRef session, IOOptionBits options);
static void _close (void * self, IOHIDSessionRef session, IOOptionBits options);
static CFTypeRef _getPropertyForClient(void * self, CFStringRef key, CFTypeRef client);

IOHIDSessionFilterPlugInInterface sIOHIDDisplaySessionFilterFtbl =
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


@interface HIDDisplaySessionFilterWrapper : HIDDisplaySessionFilter {
    IOHIDSessionFilterPlugInInterface _ftbl;
    CFUUIDRef _factoryID;
    @public
    IOHIDSessionFilterPlugInInterface *_ftblPtr;
}
-(nullable instancetype) initWithFactoryID:(CFUUIDRef) factoryID;
@end

@implementation HIDDisplaySessionFilterWrapper

-(nullable instancetype) initWithFactoryID:(CFUUIDRef __nonnull) factoryID
{
    self = [super init];
    if (!self) {
        return nil;
    }
    _ftbl = sIOHIDDisplaySessionFilterFtbl;
    _ftbl._reserved = self;
    _ftblPtr = &_ftbl;
    _factoryID = factoryID;
    CFPlugInAddInstanceForFactory(_factoryID);
    return self;
}

-(void) dealloc
{
    CFPlugInRemoveInstanceForFactory(_factoryID);
    CFRelease(_factoryID);
    [super dealloc];
}

@end

// Non interface functions
static HRESULT _QueryInterface (void *self, REFIID iid, LPVOID *ppv)
{
    // Create a CoreFoundation UUIDRef for the requested interface.
    CFUUIDRef interfaceID = CFUUIDCreateFromUUIDBytes( NULL, iid );
    // Test the requested ID against the valid interfaces.
    if (CFEqual(interfaceID, kIOHIDSimpleSessionFilterPlugInInterfaceID) || CFEqual(interfaceID, kIOHIDSessionFilterPlugInInterfaceID)) {
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

static boolean_t _open (void * self, IOHIDSessionRef __unused session, IOOptionBits __unused options)
{
    HIDDisplaySessionFilterWrapper * obj = (*(IUnknownVTbl **) self)->_reserved;
    if (!obj) {
        return false;
    }
    
    return [obj open];
    
}

static void _close (void * self, IOHIDSessionRef __unused session, IOOptionBits __unused options)
{
    HIDDisplaySessionFilterWrapper * obj = (*(IUnknownVTbl **) self)->_reserved;
    if (!obj) {
        return;
    }
    
    return [obj close];
}

static CFTypeRef _getPropertyForClient(void * self, CFStringRef __unused key, CFTypeRef __unused client)
{
    HIDDisplaySessionFilterWrapper * obj = (*(IUnknownVTbl **) self)->_reserved;
    if (!obj) {
        return NULL;
    }
    if (CFEqual(key, CFSTR(kIOHIDSessionFilterDebugKey))) {
        return CFBridgingRetain([obj getProperty]);
    }
    
    return NULL;
}

void * IOHIDDisplaySessionFilterFactory (CFAllocatorRef __unused allocator, CFUUIDRef typeUUID)
{
    // If correct type is being requested, allocate an
    // instance of TestType and return the IUnknown interface.
    if (CFEqual(typeUUID, kIOHIDSessionFilterPlugInTypeID)) {
        HIDDisplaySessionFilterWrapper * obj = [[HIDDisplaySessionFilterWrapper alloc] initWithFactoryID:kIOHIDDisplaySessionFilterFactory];
        if (obj) {
            return (void *)&obj->_ftblPtr;
        }
        return NULL;
    }
    // If the requested type is incorrect, return NULL.
    return NULL;
}

