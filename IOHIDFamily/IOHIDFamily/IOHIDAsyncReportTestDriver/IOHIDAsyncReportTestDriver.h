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

#include <IOKit/IOService.h>
#include <IOKit/hid/IOHIDDeviceTypes.h>

class IOHIDInterface;
class IOHIDUserDevice;


/// Driver to validate async report operations on `IOHIDUserDevice`.
///
class IOHIDAsyncReportTestDriver : public IOService
{
    OSDeclareDefaultStructors( IOHIDAsyncReportTestDriver );
    using super = IOService;

    struct AsyncReportCall;
    typedef void (^CompletionAction)(IOReturn status);

public:

    virtual bool start(IOService * provider) APPLE_KEXT_OVERRIDE;
    virtual IOReturn setProperties(OSObject * properties) APPLE_KEXT_OVERRIDE;

private:

    void executeTest();
    void executeTestGated();

    void publishResults();

    void completionHandler(void * param, IOReturn status, uint32_t bufferSizeRemaining);
    void completionHandlerGated(void * param, IOReturn status, uint32_t bufferSizeRemaining);

    IOReturn getReport(IOMemoryDescriptor * report,
                       IOHIDReportType      reportType,
                       IOOptionBits         options,
                       UInt32               completionTimeout,
                       CompletionAction     completion);

    IOReturn getReportGated(IOMemoryDescriptor * report,
                            IOHIDReportType      reportType,
                            IOOptionBits         options,
                            UInt32               completionTimeout,
                            CompletionAction     completion);


    IOHIDInterface * _interface;
    IOHIDUserDevice * _device;

    OSPtr<IOCommandGate> _gate;

    UInt32 _reportID;
    OSSharedPtr<IOBufferMemoryDescriptor> _reportBuffer1;
    OSSharedPtr<IOBufferMemoryDescriptor> _reportBuffer2;
    unsigned int _cmpl1CallCnt;
    IOReturn _status1;
    unsigned int _cmpl2CallCnt;
    IOReturn _status2;
    bool _done;
    unsigned int _failCount;
};
