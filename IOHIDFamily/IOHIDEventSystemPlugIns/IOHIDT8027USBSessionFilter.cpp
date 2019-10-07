//
//  IOHIDT8027USBSessionFilter.cpp
//  IOHIDT8027USBSessionFilter
//
//  Created by Paul Doerr on 11/30/18.
//

#include <sys/time.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDPrivateKeys.h>
#include <IOKit/hid/IOHIDEventSystemKeys.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/hid/IOHIDService.h>
#include "IOHIDT8027USBSessionFilter.hpp"
#include "IOHIDDebug.h"

#define DEBUG_ASSERT_MESSAGE(name, assertion, label, message, file, line, value) \
os_log_error(_HIDLogCategory(kHIDLogCategoryDefault), "AssertMacros: %s, %s", assertion, (message!=0) ? message : "");

#include <AssertMacros.h>


#define kAssertionTimeoutKey    "T8027USBAssertionTimeout"
#define kSetAssertionKey        "T8027USBSetAssertion"

CFStringRef const IOHIDT8027USBSessionFilter::ASSERTION_NAME = CFSTR("IOHIDT8027USBAssertion");

// C852D4BF-2C29-4DCB-BB7E-88548F66AA41
#define kIOHIDT8027USBSessionFilterFactory CFUUIDGetConstantUUIDWithBytes(kCFAllocatorSystemDefault, 0xc8, 0x52, 0xD4, 0xBF, 0x2C, 0x29, 0x4D, 0xCB, 0xBB, 0x7E, 0x88, 0x54, 0x8F, 0x66, 0xAA, 0x41)

extern "C" void * IOHIDT8027USBSessionFilterFactory(CFAllocatorRef allocator, CFUUIDRef typeUUID);


void *IOHIDT8027USBSessionFilterFactory(CFAllocatorRef allocator __unused, CFUUIDRef typeUUID)
{
    if (CFEqual(typeUUID, kIOHIDSessionFilterPlugInTypeID)) {
        void *alctr = CFAllocatorAllocate(kCFAllocatorDefault, sizeof(IOHIDT8027USBSessionFilter), 0);
        return new(alctr) IOHIDT8027USBSessionFilter(kIOHIDT8027USBSessionFilterFactory);
    }
    return NULL;
}


IOHIDSessionFilterPlugInInterface IOHIDT8027USBSessionFilter::sIOHIDT8027USBSessionFilterFtbl =
{
    // Required padding for COM
    NULL,
    // These three are the required COM functions
    IOHIDT8027USBSessionFilter::_QueryInterface,
    IOHIDT8027USBSessionFilter::_AddRef,
    IOHIDT8027USBSessionFilter::_Release,
    // IOHIDSimpleSessionFilterPlugInInterface functions
    IOHIDT8027USBSessionFilter::_filter,
    NULL,
    NULL,
    // IOHIDSessionFilterPlugInInterface functions
    NULL,
    NULL,
    NULL,
    NULL,
    IOHIDT8027USBSessionFilter::_registerService,
    IOHIDT8027USBSessionFilter::_unregisterService,
    IOHIDT8027USBSessionFilter::_scheduleWithDispatchQueue,
    IOHIDT8027USBSessionFilter::_unscheduleFromDispatchQueue,
    IOHIDT8027USBSessionFilter::_getPropertyForClient,
    IOHIDT8027USBSessionFilter::_setPropertyForClient,
};


IOHIDT8027USBSessionFilter::IOHIDT8027USBSessionFilter(CFUUIDRef factoryID) :
_sessionInterface(&sIOHIDT8027USBSessionFilterFtbl),
_factoryID( static_cast<CFUUIDRef>( CFRetain(factoryID) ) ),
_refCount(1),
_port(NULL),
_iterator(IO_OBJECT_NULL),
_timer(0),
_assertionTimeout(DEFAULT_ASSERTION_TIMEOUT),
_assertionID(0),
_hasT8027USB(false),
_asserting(false)
{
    CFPlugInAddInstanceForFactory( factoryID );
}


IOHIDT8027USBSessionFilter::~IOHIDT8027USBSessionFilter()
{
    CFPlugInRemoveInstanceForFactory( _factoryID );
    CFRelease( _factoryID );
}


HRESULT IOHIDT8027USBSessionFilter::_QueryInterface(void *self, REFIID iid, LPVOID *ppv)
{
    return static_cast<IOHIDT8027USBSessionFilter *>(self)->QueryInterface(iid, ppv);
}


HRESULT IOHIDT8027USBSessionFilter::QueryInterface( REFIID iid, LPVOID *ppv )
{
    CFUUIDRef interfaceID = CFUUIDCreateFromUUIDBytes( NULL, iid );
    if (CFEqual(interfaceID, kIOHIDSimpleSessionFilterPlugInInterfaceID) || CFEqual(interfaceID, kIOHIDSessionFilterPlugInInterfaceID)) {
        AddRef();
        *ppv = this;
        CFRelease(interfaceID);
        return S_OK;
    }
    if (CFEqual(interfaceID, IUnknownUUID)) {
        AddRef();
        *ppv = this;
        CFRelease(interfaceID);
        return S_OK;
    }
    *ppv = NULL;
    CFRelease( interfaceID );
    return E_NOINTERFACE;
}


ULONG IOHIDT8027USBSessionFilter::_AddRef(void *self)
{
    return static_cast<IOHIDT8027USBSessionFilter *>(self)->AddRef();
}

ULONG IOHIDT8027USBSessionFilter::AddRef()
{
    _refCount += 1;
    return _refCount;
}


ULONG IOHIDT8027USBSessionFilter::_Release(void *self)
{
    return static_cast<IOHIDT8027USBSessionFilter *>(self)->Release();
}

ULONG IOHIDT8027USBSessionFilter::Release()
{
    _refCount -= 1;
    if (_refCount == 0) {
        delete this;
        return 0;
    }
    return _refCount;
}


static CFStringRef __createDetailString(IOHIDServiceRef service)
{
    CFStringRef product = CFSTR("");
    uint64_t    senderID = 0;
    CFTypeRef   prop;

    prop = IOHIDServiceGetProperty(service, CFSTR(kIOHIDProductKey));
    if (prop && CFGetTypeID(prop) == CFStringGetTypeID()) {
        product = (CFStringRef)prop;
    }

    prop = IOHIDServiceGetRegistryID(service);
    if (prop && CFGetTypeID(prop) == CFNumberGetTypeID()) {
        CFNumberGetValue((CFNumberRef)prop, kCFNumberSInt64Type, &senderID);
    }

    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("RegID:0x%llx %@"), senderID, product);
}

void IOHIDT8027USBSessionFilter::_registerService(void * self, IOHIDServiceRef service)
{
    static_cast<IOHIDT8027USBSessionFilter *>(self)->registerService(service);
}

void IOHIDT8027USBSessionFilter::registerService(IOHIDServiceRef service)
{
    CFStringRefWrap transportString = CFStringRefWrap(kIOHIDTransportKey);
    CFTypeRef       prop;

    require(_usbHIDServices.Reference() && transportString.Reference(), exit);

    // Take power assertion for USB consumer control devices and keyboards.
    // This will prevent wake events from these devices from getting lost.
    // (On T8027, USB bus reenumerates upon wake)
    require_quiet(IOHIDServiceConformsTo(service, kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard) ||
                  IOHIDServiceConformsTo(service, kHIDPage_GenericDesktop, kHIDUsage_GD_Keypad) ||
                  IOHIDServiceConformsTo(service, kHIDPage_Consumer, kHIDUsage_Csmr_ConsumerControl),
                  exit);

    prop = IOHIDServiceGetProperty(service, transportString);
    require(prop && CFGetTypeID(prop) == CFStringGetTypeID(), exit);
    require_quiet(CFEqual(prop, CFSTR(kIOHIDTransportUSBValue)), exit);

    _usbHIDServices.SetValue(service);

    // If assertion previously timed out, do not take assertion on service connection,
    // until user activity has occured.
    require_action_quiet(_timedOut == false, exit,
                         os_log(_HIDLogCategory(kHIDLogCategoryDefault), "T8027 assertion previously timed out, not taking assertion"));

    if (_hasT8027USB) {
        CFStringRef detail = __createDetailString(service);

        os_log(_HIDLogCategory(kHIDLogCategoryDefault), "Creating T8027USB assertion for %@", detail);

        preventIdleSleepAssertion(detail);

        if (detail) {
            CFRelease(detail);
        }
    }

exit:
    return;
}


void IOHIDT8027USBSessionFilter::_unregisterService(void * self, IOHIDServiceRef service)
{
    static_cast<IOHIDT8027USBSessionFilter *>(self)->unregisterService(service);
}

void IOHIDT8027USBSessionFilter::unregisterService(IOHIDServiceRef service)
{
    require(_usbHIDServices.Reference(), exit);

    if (_usbHIDServices.ContainValue(service)) {
        _usbHIDServices.RemoveValue(service);

        // When all applicable devices are disconnected, release power assertion.
        if (_hasT8027USB && _usbHIDServices.Count() == 0) {
            os_log(_HIDLogCategory(kHIDLogCategoryDefault), "Removing T8027USB assertion");

            releaseIdleSleepAssertion();
        }
    }

exit:
    return;
}


void IOHIDT8027USBSessionFilter::_scheduleWithDispatchQueue(void * self, dispatch_queue_t queue)
{
    static_cast<IOHIDT8027USBSessionFilter *>(self)->scheduleWithDispatchQueue(queue);
}

void IOHIDT8027USBSessionFilter::scheduleWithDispatchQueue(dispatch_queue_t queue)
{
    kern_return_t result;

    _queue = queue;

    _port = IONotificationPortCreate(kIOMasterPortDefault);
    require(_port, exit);

    result =
    IOServiceAddMatchingNotification(_port,
                                     kIOFirstPublishNotification,
                                     IOServiceNameMatching("usb-drd,t8027"),
                                     &_serviceNotificationCallback,
                                     this,
                                     &_iterator);
    require_noerr_action(result,
                         exit,
                         os_log_error(_HIDLogCategory(kHIDLogCategoryDefault), "%s adding matching notification 0x%x", __PRETTY_FUNCTION__, (int)result));
    require(_iterator, exit);

    IONotificationPortSetDispatchQueue(_port, _queue);

    serviceNotificationCallback(_iterator);

exit:
    return;
}


void IOHIDT8027USBSessionFilter::_serviceNotificationCallback (void * refcon, io_iterator_t iterator)
{
    static_cast<IOHIDT8027USBSessionFilter *>(refcon)->serviceNotificationCallback(iterator);
}

void IOHIDT8027USBSessionFilter::serviceNotificationCallback(io_iterator_t iterator)
{
    io_service_t service = IOIteratorNext(iterator);
    require(service != IO_OBJECT_NULL, exit);

    _hasT8027USB = true;

exit:
    return;
}


void IOHIDT8027USBSessionFilter::_unscheduleFromDispatchQueue(void * self, dispatch_queue_t queue)
{
    static_cast<IOHIDT8027USBSessionFilter *>(self)->unscheduleFromDispatchQueue(queue);
}

void IOHIDT8027USBSessionFilter::unscheduleFromDispatchQueue(dispatch_queue_t queue __unused)
{
    if (_assertionID) {
        releaseIdleSleepAssertion();
    }

    if (_iterator) {
        IOObjectRelease(_iterator);
        _iterator = IO_OBJECT_NULL;
    }

    if (_port) {
        IONotificationPortDestroy(_port);
        _port = NULL;
    }
}


IOHIDEventRef IOHIDT8027USBSessionFilter::_filter(void * self, IOHIDServiceRef sender, IOHIDEventRef event)
{
    return static_cast<IOHIDT8027USBSessionFilter *>(self)->filter(sender, event);
}

IOHIDEventRef IOHIDT8027USBSessionFilter::filter(IOHIDServiceRef sender, IOHIDEventRef event)
{
    require(_usbHIDServices.Reference(), exit);

    // Check for keyboard down event from a USB HID service.
    require_quiet(event && sender, exit);
    require_quiet(IOHIDEventConformsTo(event, kIOHIDEventTypeKeyboard), exit);
    require_quiet(IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardDown), exit);
    require_quiet(_usbHIDServices.ContainValue(sender), exit);

    // Push out the assertion timeout.
    if (_hasT8027USB) {
        CFStringRef detail = __createDetailString(sender);

        os_log_info(_HIDLogCategory(kHIDLogCategoryDefault), "T8027USB HID activity");

        preventIdleSleepAssertion(detail);

        // User activity has occured, so re-allow assertions on service connection.
        _timedOut = false;

        if (detail) {
            CFRelease(detail);
        }
    }

exit:
    return event;
}


CFTypeRef IOHIDT8027USBSessionFilter::_getPropertyForClient (void * self, CFStringRef key, CFTypeRef client)
{
    return static_cast<IOHIDT8027USBSessionFilter *>(self)->getPropertyForClient(key,client);
}

CFTypeRef IOHIDT8027USBSessionFilter::getPropertyForClient(CFStringRef key, CFTypeRef client __unused)
{
    CFTypeRef result = NULL;

    if (CFEqual(key, CFSTR(kIOHIDSessionFilterDebugKey))) {
        CFMutableDictionaryRefWrap serializer;
        serialize(serializer);

        if (serializer) {
            result = CFRetain(serializer.Reference());
        }
    }

    return result;
}


void IOHIDT8027USBSessionFilter::_setPropertyForClient (void * self, CFStringRef key, CFTypeRef property, CFTypeRef client)
{
    static_cast<IOHIDT8027USBSessionFilter *>(self)->setPropertyForClient(key, property, client);
}

void IOHIDT8027USBSessionFilter::setPropertyForClient (CFStringRef key, CFTypeRef property, CFTypeRef client __unused)
{
    if (CFEqual(key, CFSTR(kAssertionTimeoutKey))) {
        uint64_t timeout;

        os_log(_HIDLogCategory(kHIDLogCategoryDefault), "Setting T8027 USB assertion timeout from %llu to %@", _assertionTimeout, (property ? property : CFSTR("")));

        require(property && CFGetTypeID(property) == CFNumberGetTypeID(), exit);

        CFNumberGetValue((CFNumberRef)property, kCFNumberSInt64Type, &timeout);
        _assertionTimeout = timeout;

    } else if (CFEqual(key, CFSTR(kSetAssertionKey))) {
        os_log(_HIDLogCategory(kHIDLogCategoryDefault), "Setting T8027 USB assertion state %@", (property ? property : CFSTR("")));

        if (property == kCFBooleanTrue) {
            preventIdleSleepAssertion(CFSTR("SetProperty"));
        } else {
            if (_assertionID) {
                releaseIdleSleepAssertion();
            }
        }
    }

exit:
    return;
}


void IOHIDT8027USBSessionFilter::serialize (CFMutableDictionaryRef dict) const {
    CFMutableDictionaryRefWrap serializer (dict);
    serializer.SetValueForKey(CFSTR("Class"), CFSTR("IOHIDT8027USBSessionFilter"));
    serializer.SetValueForKey(CFSTR("HasT8027USB"), (_hasT8027USB ? kCFBooleanTrue : kCFBooleanFalse));
    serializer.SetValueForKey(CFSTR("AssertionTimeout"), CFNumberRefWrap(_assertionTimeout));
    serializer.SetValueForKey(CFSTR("Asserting"), (_asserting ? kCFBooleanTrue : kCFBooleanFalse));
    serializer.SetValueForKey(CFSTR("USBHIDServiceCount"), _usbHIDServices.Count());
    if (_assertionID) {
        CFDictionaryRef pmDict = IOPMAssertionCopyProperties(_assertionID);
        if (pmDict) {
            serializer.SetValueForKey(CFSTR("AssertionProperties"), pmDict);
            CFRelease(pmDict);
        }
    }
}

void IOHIDT8027USBSessionFilter::preventIdleSleepAssertion(CFStringRef detail)
{
    CFNumberRef num = NULL;
    uint32_t    on = kIOPMAssertionLevelOn;
    IOReturn    status;

    // Create an assertion if there isn't one currently.
    if (_assertionID == 0) {
        require(initTimer(), exit);

        status = IOPMAssertionCreateWithDescription(kIOPMAssertPreventUserIdleSystemSleep,
                                                    IOHIDT8027USBSessionFilter::ASSERTION_NAME,
                                                    (detail ? detail : NULL),
                                                    NULL, NULL,
                                                    0.0, NULL,
                                                    &_assertionID);
        require_noerr_action(status,
                             exit,
                             os_log_error(_HIDLogCategory(kHIDLogCategoryDefault), "%s error creating assertion 0x%x", __PRETTY_FUNCTION__, (int)status));
    } else {
        // Re-enable the assertion.
        num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &on);
        require(num, exit);
        status = IOPMAssertionSetProperty(_assertionID, kIOPMAssertionLevelKey, num);
        require_noerr_action(status,
                             exit,
                             os_log_error(_HIDLogCategory(kHIDLogCategoryDefault), "%s error turning on assertion 0x%x", __PRETTY_FUNCTION__, (int)status));
    }

    // Kick out the timeout.
    dispatch_source_set_timer(_timer,
                              dispatch_time(DISPATCH_TIME_NOW, _assertionTimeout * NSEC_PER_SEC),
                              DISPATCH_TIME_FOREVER,
                              0);

    _asserting = true;

exit:
    if (num) {
        CFRelease(num);
    }
    return;
}

void IOHIDT8027USBSessionFilter::releaseIdleSleepAssertion()
{
    IOReturn status;

    if (_timer) {
        dispatch_source_cancel(_timer);
        _timer = NULL;
    }

    status = IOPMAssertionRelease(_assertionID);
    require_noerr_action(status,
                         exit,
                         os_log_error(_HIDLogCategory(kHIDLogCategoryDefault), "%s error releasing assertion 0x%x", __PRETTY_FUNCTION__, (int)status));

exit:
    _assertionID = 0;
    _asserting = false;

    return;
}

bool IOHIDT8027USBSessionFilter::initTimer()
{
    dispatch_source_t   timer = NULL;
    bool                ret = false;

    require_action_quiet(!_timer, exit, ret = true);
    require(_queue, exit);

    timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, DISPATCH_TIMER_STRICT, _queue);
    require(timer, exit);

    dispatch_source_set_event_handler(timer, ^{
        timerHandler();
    });

    dispatch_source_set_cancel_handler(timer, ^(void) {
        dispatch_release(timer);
    });

    dispatch_source_set_timer(timer, DISPATCH_TIME_FOREVER, 0, 0);
    dispatch_activate(timer);

    _timer = timer;

    ret = true;

exit:
    if (!ret) {
        os_log_error(_HIDLogCategory(kHIDLogCategoryDefault), "%s error", __PRETTY_FUNCTION__);
    }
    return ret;
}

void IOHIDT8027USBSessionFilter::timerHandler()
{
    CFNumberRef num = NULL;
    uint32_t    off = kIOPMAssertionLevelOff;
    IOReturn    status;

    require(_assertionID != 0, exit);

    os_log(_HIDLogCategory(kHIDLogCategoryDefault), "T8027USB HID assertion timeout");

    num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &off);
    require(num, exit);

    status = IOPMAssertionSetProperty(_assertionID, kIOPMAssertionLevelKey, num);
    require_noerr_action(status,
                         exit,
                         os_log_error(_HIDLogCategory(kHIDLogCategoryDefault), "%s error turning off assertion 0x%x", __PRETTY_FUNCTION__, (int)status));

    _asserting = false;
    _timedOut = true;

exit:
    if (num) {
        CFRelease(num);
    }
    return;
}
