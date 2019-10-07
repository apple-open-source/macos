#include <assert.h>
#include <AssertMacros.h>
#include <stdio.h>
#include <stdlib.h>
#include <DriverKit/DriverKit.h>
#include <DriverKit/OSCollections.h>
#include <HIDDriverKit/HIDDriverKit_Private.h>

#undef super
#define super IOHIDDevice

kern_return_t
IMPL(IOUserHIDDevice, Start)
{
    kern_return_t    ret;
    OSDictionary *   description = NULL;
    OSData  *        descriptor  = NULL;
    uint64_t         regID = 0;
    
    provider->GetRegistryEntryID(&regID);
    HIDTrace(kHIDDK_Dev_Start, getRegistryID(), regID, 0, 0);
    
    ret = Start(provider, SUPERDISPATCH);
    require_noerr_action (ret, exit, HIDServiceLogError("Start(SUPERDISPATCH):%x", ret));
    
    require_action(handleStart(provider), exit, {
        HIDServiceLogError("handleStart:false");
        ret = kIOReturnError;
    });
    
    description = newDeviceDescription ();
    if (NULL == description) {
        description = OSDictionary::withCapacity(1);
        HIDServiceLogError("no description");
    }
    require_action(description, exit, {
        HIDServiceLogError("no description");
        ret = kIOReturnError;
    });
    
    descriptor = newReportDescriptor();
    require_action(descriptor, exit, {
        HIDServiceLogError("newReportDescriptor returned NULL");
        ret = kIOReturnError;
    });

    description->setObject (kIOHIDReportDescriptorKey, descriptor);
    
    ret = SetProperties(description);
    require_noerr_action (ret, exit, HIDServiceLogError("SetProperties:%x", ret));
    //
    //  finalize start of IOHIDDevice in kernel
    //
    ret = KernelStart (provider);
    
exit:
    
    OSSafeReleaseNULL (description);
    OSSafeReleaseNULL (descriptor);

    if (ret) {
        HIDServiceLogFault("Start failed: 0x%x", ret);
    }
    
    return ret;
}


bool IOUserHIDDevice::handleStart (IOService * provider)
{
    return true;
}

OSDictionary * IOUserHIDDevice::newDeviceDescription ()
{
    return NULL;
}

OSData * IOUserHIDDevice::newReportDescriptor ()
{
    return NULL;
}
