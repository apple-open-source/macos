//
//  AppleUserT1TouchBarHIDDevice.cpp
//  AppleUserT1TouchBarHIDDevice
//
//  Created by yg on 11/07/20.
//

#include <assert.h>
#include <AssertMacros.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <DriverKit/DriverKit.h>
#include <DriverKit/OSCollections.h>
#include <HIDDriverKit/HIDDriverKit_Private.h>
#include "AppleUserT1TouchBarHIDDevice.h"
#include "AppleUserT1TouchBar.h"

#undef  super
#define super IOUserUSBHostHIDDevice

#ifndef countof
#define countof(x)          (sizeof(x)/sizeof(x[0]))
#endif

typedef struct __attribute__ ((packed))
{
    uint8_t   index   :4;
    uint8_t   touch   :1;
    uint8_t   range   :1;
    uint8_t   reserved:2;
    uint16_t  x;
    uint8_t   y;
} DigitizerReport;

typedef struct __attribute__ ((packed))
{
    DigitizerReport path[10];
    uint64_t  touchTime;
    uint32_t  generationCount;
} TouchBarReport;

typedef struct __attribute__ ((packed))
{
    DigitizerReport path[11];
    uint64_t  touchTime;
    uint32_t  generationCount;
} ExtTouchBarReport;



struct AppleUserT1TouchBarHIDDevice_IVars
{
    ExtTouchBarReport report;
};

bool AppleUserT1TouchBarHIDDevice::init ()
{
    bool ret;

    ret = super::init();
    require_action(ret, exit, HIDLogError("init:%x", ret));
    
    assert(IOService::ivars);

    ivars = IONewZero(AppleUserT1TouchBarHIDDevice_IVars, 1);

exit:
    
    return ret;
}

kern_return_t
IMPL(AppleUserT1TouchBarHIDDevice, Start)
{
    kern_return_t   ret;
 
    ret = Start(provider, SUPERDISPATCH);
    require_noerr_action (ret, exit, HIDLogError("Start:0x%x", ret));

    ret = RegisterService();
    require_noerr_action(ret, exit, HIDServiceLog("RegisterService:0x%x\n", ret));

exit:
    
    if (ret) {
        HIDServiceLogFault("Start failed: 0x%x", ret);
        Stop(provider);
    }
    
    HIDServiceLog("Start ret: 0x%x", ret);
    
    return ret;
}

kern_return_t
IMPL(AppleUserT1TouchBarHIDDevice, Stop)
{
    kern_return_t   ret;
    
    ret = Stop(provider, SUPERDISPATCH);
    HIDServiceLog("Stop: 0x%x", ret);
    
    return ret;
}

kern_return_t
AppleUserT1TouchBarHIDDevice::handleReport(uint64_t                  timestamp,
                                           IOMemoryDescriptor        *report,
                                           uint32_t                  reportLength,
                                           IOHIDReportType           reportType,
                                           IOOptionBits              options)
{
    
    kern_return_t   ret;
    IOMemoryMap * map = NULL;
    
    if (reportLength != sizeof(TouchBarReport)) {
        HIDServiceLogFault("reportLength:%d expected:%lu", reportLength, sizeof(TouchBarReport));
        return kIOReturnUnsupported;
    }
    
    ret = report->CreateMapping(/* options */   0,
                                /* address */   0,
                                /* offset */    0,
                                /* length */    0,
                                /* alignment */ 0,
                                &map);
    if (ret) {
        HIDServiceLogFault("CreateMapping failed: 0x%x", ret);
        return ret;
    }
    
    TouchBarReport * touchBarReport = (TouchBarReport*)map->GetAddress();
    
    ivars->report.touchTime = touchBarReport->touchTime;
    ivars->report.generationCount = touchBarReport->generationCount;
    
    for (unsigned int index = 0; index < countof(ivars->report.path); index++) {
        ivars->report.path[index].touch = 0;
        ivars->report.path[index].range = 0;
    }
    
    for (unsigned int index = 0; index < countof(touchBarReport->path); index++) {
        if (touchBarReport->path[index].index) {
            ivars->report.path[touchBarReport->path[index].index - 1] = touchBarReport->path[index];
        }
    }
    
    memcpy ((void*)touchBarReport, (void*)&ivars->report, sizeof(ivars->report));
    
    OSSafeReleaseNULL(map);

    return super::handleReport(timestamp, report, sizeof(ivars->report), reportType, options);
}

OSData *
AppleUserT1TouchBarHIDDevice::newReportDescriptor ()
{
    static uint8_t descriptor[] = {AppleUserT1TouchBarDescriptor};
    return OSData::withBytesNoCopy(descriptor, sizeof(descriptor));
}
