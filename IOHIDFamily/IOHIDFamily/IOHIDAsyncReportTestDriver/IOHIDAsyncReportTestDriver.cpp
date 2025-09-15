/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * This document is the property of Apple Inc.
 * It is considered confidential and proprietary.
 *
 * This document may not be reproduced or transmitted in any form,
 * in whole or in part, without the express written permission of
 * Apple Inc.
 */

#define IOKIT_ENABLE_SHARED_PTR

#include <AssertMacros.h>
#include <libkern/Block.h>
#include <libkern/c++/OSValueObject.h>
#include "IOHIDAsyncReportTestDriver.h"
#include "IOHIDInterface.h"
#include "IOHIDUserDevice.h"
#include "IOHIDElement.h"
#include "IOHIDDebug.h"

#define CHECK(expr, msg, ...)\
    do {\
        if (!(expr)) {\
            HIDServiceLogError("[FAIL] [%s:%d] %s " msg, __FILE__, __LINE__, #expr, ##__VA_ARGS__);\
            _failCount++;\
        }\
    } while(0);


OSDefineMetaClassAndStructors(IOHIDAsyncReportTestDriver, IOService);

bool
IOHIDAsyncReportTestDriver::start(IOService * provider)
{
    bool ok = false;
    OSSharedPtr<OSObject> prop = nullptr;
    OSSharedPtr<OSDictionary> matching = nullptr;
    OSSharedPtr<OSArray> elements = nullptr;
    UInt32 reportSize = 0;

    HIDServiceLog("IOHIDAsyncReportTestDriver::start");

    require_quiet(super::start(provider), exit);

    _interface = OSRequiredCast(IOHIDInterface, provider);
    _device = OSRequiredCast(IOHIDUserDevice, provider->getProvider());
    _gate = IOCommandGate::commandGate(this);

    getWorkLoop()->addEventSource(_gate.get());
    _gate->enable();

    prop = _device->copyProperty(kIOHIDMaxFeatureReportSizeKey);
    reportSize = OSRequiredCast(OSNumber, prop.get())->unsigned32BitValue();

    _reportBuffer1 = IOBufferMemoryDescriptor::withOptions((IOOptionBits)kIODirectionOutIn |
                                                           (IOOptionBits)kIOMemoryKernelUserShared |
                                                           (IOOptionBits)kIOMemoryThreadSafe,
                                                           reportSize);
    assert(_reportBuffer1);

    _reportBuffer2 = IOBufferMemoryDescriptor::withOptions((IOOptionBits)kIODirectionOutIn |
                                                           (IOOptionBits)kIOMemoryKernelUserShared |
                                                           (IOOptionBits)kIOMemoryThreadSafe,
                                                           reportSize);
    assert(_reportBuffer2);

    // find report ID for any feature report
    matching = OSDictionary::withCapacity(1);
    matching->setObject(kIOHIDElementTypeKey, OSNumber::withNumber(kIOHIDElementTypeFeature, 32));

    elements = OSSharedPtr<OSArray>(_interface->createMatchingElements(matching.get()), OSNoRetain);
    require_quiet(elements->getCount() > 0, exit);

    _reportID = OSRequiredCast(IOHIDElement, elements->getObject(0))->getReportID();

    registerService();
    ok = true;

exit:
    return ok;
}

IOReturn
IOHIDAsyncReportTestDriver::setProperties(OSObject * properties)
{
    IOReturn ret = kIOReturnUnsupported;
    OSDictionary * propertyDict = nullptr;
    OSBoolean * run = nullptr;

    propertyDict = OSDynamicCast(OSDictionary, properties);
    require_action(propertyDict, exit, ret = kIOReturnBadArgument);

    run = OSDynamicCast(OSBoolean, propertyDict->getObject("RunTest"));
    if (run == kOSBooleanTrue) {
        HIDServiceLog("IOHIDAsyncReportTestDriver: executing test");
        executeTest();
        HIDServiceLog("IOHIDAsyncReportTestDriver: test finished with %u failures", _failCount);

        OSSharedPtr<OSNumber> num = OSNumber::withNumber(_failCount, 32);
        setProperty("FailedChecks", num.get());
        ret = kIOReturnSuccess;
    }

exit:
    return ret;
}

void
IOHIDAsyncReportTestDriver::executeTest()
{
    _gate->runActionBlock(^IOReturn{
        executeTestGated();
        return kIOReturnSuccess;
    });
}

void
IOHIDAsyncReportTestDriver::executeTestGated()
{
    IOReturn ret = kIOReturnInvalid;
    AbsoluteTime deadline = 0;
    clock_interval_to_deadline(5, kSecondScale, &deadline);
    IOHIDCompletion completion = {
        .target = this,
        .action = OSMemberFunctionCast(IOHIDCompletionAction,
                                       this,
                                       &IOHIDAsyncReportTestDriver::completionHandler),
        .parameter = reinterpret_cast<void *>(1),
    };

    ret = _device->getReport(_reportBuffer1.get(), kIOHIDReportTypeFeature, _reportID, 1000, &completion);
    CHECK(ret == kIOReturnSuccess, "(0x%x)", ret);

    ret = _gate->commandSleep(&_done, deadline, THREAD_UNINT);
    CHECK(ret == THREAD_AWAKENED, "(0x%x)", ret);
    CHECK(_done == true, "(%d)", _done);

    CHECK(_cmpl1CallCnt == 1, "(%u)", _cmpl1CallCnt);
    CHECK(_status1 == kIOReturnTimeout, "(0x%x)", _status1);

    CHECK(_cmpl2CallCnt == 1, "(%u)", _cmpl2CallCnt);
    CHECK(_status2 == kIOReturnSuccess, "(0x%x)", _status2);
}

void
IOHIDAsyncReportTestDriver::completionHandler(void * param, IOReturn status, uint32_t bufferSizeRemaining)
{
    _gate->runActionBlock(^IOReturn{
        completionHandlerGated(param, status, bufferSizeRemaining);
        return kIOReturnSuccess;
    });
}

void
IOHIDAsyncReportTestDriver::completionHandlerGated(void * param, IOReturn status, uint32_t bufferSizeRemaining)
{
    IOReturn ret = kIOReturnInvalid;
    uintptr_t i = reinterpret_cast<uintptr_t>(param);

    switch (i) {
        case 1:
            ++_cmpl1CallCnt;
            if (_cmpl1CallCnt == 1) {
                _status1 = status;

                // perform another async get report from the completion
                IOHIDCompletion completion = {
                    .target = this,
                    .action = OSMemberFunctionCast(IOHIDCompletionAction,
                                                   this,
                                                   &IOHIDAsyncReportTestDriver::completionHandler),
                    .parameter = reinterpret_cast<void *>(2),
                };
                ret = _device->getReport(_reportBuffer2.get(), kIOHIDReportTypeFeature, _reportID, 1000, &completion);
                CHECK(ret == kIOReturnSuccess, "(0x%x)", ret);
            }
            break;
        case 2:
            ++_cmpl2CallCnt;
            _status2 = status;
            _done = true;
            _gate->commandWakeup(&_done);
            break;
        default:
            break;
    }
}
