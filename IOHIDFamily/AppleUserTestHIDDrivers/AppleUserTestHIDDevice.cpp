#include <assert.h>
#include <AssertMacros.h>
#include <stdio.h>
#include <stdlib.h>
#include <libkern/OSAtomic.h>
#include <IOKit/IOUserServer.h>
#include <DriverKit/DriverKit.h>
#include <DriverKit/OSCollections.h>

#include <IOKitUser/IODispatchQueue.h>
#include <IOKitUser/OSAction.h>
#include <IOKitUser/IOBufferMemoryDescriptor.h>

#include <HIDDriverKit/IOHIDInterface.h>
#include <HIDDriverKit/IOHIDDevice.h>

#include "Implementation/IOKitUser/AppleUserTestHIDDevice.h"


#define DLog(fmt, args...) printf("[DEF][AppleUserTestHIDDevice:%p] " fmt "\n", this,  ##args)
#define ELog(fmt, args...) printf("[ERR][AppleUserTestHIDDevice:%p] " fmt "\n" , this,  ##args)


struct AppleUserTestHIDDevice_IVars
{
    IOHIDInterface            * interface;
    OSAction                  * interfaceAction;
    OSDictionaryPtr             properies;
    IODispatchQueue           * queue;
};

#define _interface          (ivars->interface)
#define _interfaceAction    (ivars->interfaceAction)
#define _properies          (ivars->properies)
#define _queue              (ivars->queue)


#undef super
#define super IOUserHIDDevice


bool AppleUserTestHIDDevice::init ()
{
    bool ret;
    
    DLog("Init:%p", this);
    
    ret = super::init();
    require_action(ret, exit, ELog("Init(SUPERDISPATCH):%x", ret));
    
    assert(IOService::ivars);
    
    ret = (ivars = IONewZero(AppleUserTestHIDDevice_IVars, 1));
    
exit:
    
    return ret;
}

kern_return_t
IMPL(AppleUserTestHIDDevice, Start)
{
    kern_return_t       ret = kIOReturnError;
    uint64_t            pID;
    uint64_t            sID;
    
    provider->GetRegistryEntryID(&pID);
    
    GetRegistryEntryID(&sID);
    
    DLog("Start:%p (id:0x%llx) provider:%p (id:0x%llx) ", this, sID, provider, pID);
    
    _interface = OSDynamicCast(IOHIDInterface, provider);
    require_action (_interface, exit, ELog("Invalid provider"));
    _interface->retain();
    
    ret = _interface->CopyProperties(&_properies);
    require_noerr_action (ret, exit, ELog("CopyProperties:%x", ret));
    
    OSObjectLog (_properies);
    
    ret = Start(provider, SUPERDISPATCH);
    require_noerr_action (ret, exit, ELog("Start:%x", ret));
    
    ret = CopyDispatchQueue("Default", &_queue);
    require_noerr_action (ret, exit, ELog("CopyDispatchQueue:%x", ret));
    
    
    ret = CreateActionHandleReportCallback(
                           sizeof(uint64_t),
                           &_interfaceAction);
    require_noerr_action (ret, exit, ELog("CreateActionHandleReportCallback:%x", ret));
    
    ret = _interface->Open(0, _interfaceAction);
    require_noerr_action (ret, exit, ELog("IOHIDInterface::Open:%x", ret));
    
exit:
    
    return ret;
}

kern_return_t
IMPL(AppleUserTestHIDDevice, Stop)
{
    if (_interface) {
        _interface->Close(0);
    }
    
    Stop (provider, SUPERDISPATCH);
    
    return kIOReturnSuccess;
}

void AppleUserTestHIDDevice::free()
{
    OSSafeReleaseNULL(_interface);
    OSSafeReleaseNULL(_interfaceAction);
    OSSafeReleaseNULL(_queue);
    OSObjectSafeReleaseNULL(_properies);
    IOSafeDeleteNULL(ivars, AppleUserTestHIDDevice_IVars, 1);
    
    super::free ();
}

void
IMPL(AppleUserTestHIDDevice, HandleReportCallback)
{
    DLog("HandleReportCallback: %llx\n", timestamp);
    HandleReport(timestamp, report, type, 0);
}

kern_return_t AppleUserTestHIDDevice::getReport(IOMemoryDescriptor      * report,
                                       IOHIDReportType         reportType,
                                       IOOptionBits            options,
                                       uint32_t                completionTimeout,
                                       OSAction                * action)
{
    kern_return_t   ret;
    
    ret = _interface->GetReport(report, reportType, options & 0xff, options & (~0xff));
    require_noerr_action (ret, exit, ELog("IOHIDInterface::GetReport:0x%x\n", ret));
    
    if (action) {
        action->retain();
        _queue->DispatchAsync(^{
            CompleteReport(action, ret, 0);
            action->release();
        });
    }
    
exit:
    
    return ret;
}

kern_return_t AppleUserTestHIDDevice::setReport(IOMemoryDescriptor      * report,
                                       IOHIDReportType         reportType,
                                       IOOptionBits            options,
                                       uint32_t                completionTimeout,
                                       OSAction                * action)
{
    kern_return_t ret;
    
    ret = _interface->SetReport(report, reportType, options & 0xff, options & (~0xff));
    require_noerr_action (ret, exit, ELog("IOHIDInterface::SetReport:0x%x\n", ret));
    if (action) {
        action->retain();
        _queue->DispatchAsync(^{
            CompleteReport(action, ret, 0);
            action->release();
        });
    }
    
exit:
    
    return ret;
}

OSData * AppleUserTestHIDDevice::newReportDescriptor ()
{
    OSData * descriptor = OSDynamicCast (OSData, OSDictionaryGetValue (_properies, kIOHIDReportDescriptorKey));
    
    if (descriptor) {
        descriptor->retain();
    }
    
    return descriptor;
}

#define CopyProperty(d,s,key)                               \
{                                                           \
    OSObjectPtr value = OSDictionaryGetValue (s,key);       \
    if (value) {                                            \
        OSDictionarySetValue (d, key, value);               \
    }                                                       \
}

OSDictionary * AppleUserTestHIDDevice::newDeviceDescription ()
{
    OSDictionary * dictionary = OSDictionary::withCapacity(6);
    require_action (dictionary, exit,ELog("OSDictionaryCreate"));
    
    OSDictionarySetStringValue(dictionary, kIOHIDTransportKey, "TESTHID");
    
    CopyProperty(dictionary, _properies, kIOHIDReportIntervalKey);
    OSDictionarySetUInt64Value (dictionary, kIOHIDProductIDKey, 556);
    OSDictionarySetUInt64Value (dictionary, kIOHIDVendorIDKey, 556);
    OSDictionarySetStringValue (dictionary, kIOHIDTransportKey, "PassThrough");
    
exit:
    
    return  dictionary;
}

