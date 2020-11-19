//
//  AppleUserUSBHostHIDDevice.cpp
//  AppleUserHIDDrivers
//
//  Created by yg on 12/23/18.
//

#include <assert.h>
#include <AssertMacros.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <DriverKit/DriverKit.h>
#include <DriverKit/OSCollections.h>
#include <HIDDriverKit/HIDDriverKit_Private.h>
#include "AppleUserUSBHostHIDDevice.h"

#undef  super
#define super IOUserUSBHostHIDDevice

#define kNativeInstrumentsVID 0x17CC

enum {
    kAppleUserUSBHostHIDDeviceDisable           = 0x1,
    kAppleUserUSBHostHIDDeviceOverrideTopCase   = 0x2
};

bool AppleUserUSBHostHIDDevice::init ()
{
    bool ret;

    ret = super::init();
    require_action(ret, exit, HIDLogError("init:%x", ret));
    
    assert(IOService::ivars);

exit:
    
    return ret;
}

kern_return_t
IMPL(AppleUserUSBHostHIDDevice, Start)
{
    kern_return_t   ret;
    bool            status;
    uint32_t        debug = 0;
    OSDictionaryPtr properties = NULL;

    IOParseBootArgNumber("AppleUserUSBHostHIDDevice-debug", &debug, sizeof(debug));
    if (debug & kAppleUserUSBHostHIDDeviceDisable) {
        return kIOReturnUnsupported;
    }
   
    ret = Start(provider, SUPERDISPATCH);
    require_noerr_action (ret, exit, HIDLogError("Start:0x%x", ret));

    if (CopyProperties(&properties, SUPERDISPATCH) == KERN_SUCCESS) {
        uint64_t debugProperty = 0;
        char     blacklist[256] = {0};

        do {
            status = IOParseBootArgString("AppleUserUSBHostHIDDevice-blacklist", blacklist, sizeof(blacklist));
            if (status) {
                char * curr = &blacklist[0];
                char * next = curr;
                uint64_t did = 0;

                did = OSDictionaryGetUInt64Value(properties, kIOHIDVendorIDKey) |
                (OSDictionaryGetUInt64Value(properties, kIOHIDProductIDKey) << 16);

                do {
                    uint64_t value = strtoull(curr, &next, 0);
                    if (value == did) {
                        ret = kIOReturnUnsupported;
                        HIDServiceLog("Device blacklisted:0x%llx\n", did)
                        break;
                    }
                } while (next > curr && *next != '\0');
            }
        
            debugProperty = OSDictionaryGetUInt64Value (properties, "AppleUserUSBHostHIDDevice-debug");
            if ((debugProperty & debug) != debugProperty) {
                HIDServiceLog("Device support require AppleUserUSBHostHIDDevice-debug:0x%llx\n", debugProperty);
                ret = kIOReturnUnsupported;
                break;
            }
        } while (false);
    }
    require_noerr(ret, exit);

    ret = RegisterService();
    require_noerr_action(ret, exit, HIDServiceLog("RegisterService:0x%x\n", ret));

exit:
    
    if (ret) {
        HIDServiceLogFault("Start failed: 0x%x", ret);
        Stop(provider);
    }
    if (properties) {
        OSSafeReleaseNULL(properties);
    }
    
    HIDServiceLog("Start ret: 0x%x", ret);
    
    return ret;
}

kern_return_t
IMPL(AppleUserUSBHostHIDDevice, Stop)
{
    kern_return_t   ret;
    
    ret = Stop(provider, SUPERDISPATCH);
    HIDServiceLog("Stop: 0x%x", ret);
    
    return ret;
}
