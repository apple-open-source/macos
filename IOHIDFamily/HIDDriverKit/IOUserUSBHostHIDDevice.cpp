#include <assert.h>
#include <AssertMacros.h>
#include <stdio.h>
#include <stdlib.h>
#include <DriverKit/DriverKit.h>
#include <DriverKit/OSCollections.h>
#include <DriverKit/IODispatchQueue.h>
#include <DriverKit/IOTimerDispatchSource.h>
#include <DriverKit/IOBufferMemoryDescriptor.h>
#include <USBDriverKit/USBDriverKit.h>
#include <HIDDriverKit/HIDDriverKit_Private.h>

// TODO: fix me
#if TARGET_OS_DRIVERKIT
#include <DriverKit/IOLib.h>
#else
#include <mach/mach_time.h>
#endif

enum {
    kHIDMillisecondScale = 1000 * 1000,
};

// TODO: remove
#if !TARGET_OS_DRIVERKIT
#include <IOKit/usb/AppleUSBDefinitions.h>
#endif

#ifndef kUSBHostReturnPipeStalled
#define kUSBHostReturnPipeStalled                               0xe0005000
#endif

#ifndef kUSBHostPropertyLocationID
#define kUSBHostPropertyLocationID                              "locationID"
#endif

#ifndef kUSBHostHIDDevicePropertyIdlePolicy
#define kUSBHostHIDDevicePropertyIdlePolicy                     "kUSBHIDDeviceIdlePolicy"
#endif

#ifndef kUSBHostMatchingPropertyPortType
#define kUSBHostMatchingPropertyPortType                        "USBPortType"
#endif

#define kUSBHostHIDDeviceInterfaceIdlePolicyKey                 "kUSBHIDDeviceInterfaceIdlePolicy"

#define kUSBHostHIDDevicePipeIdlePolicyKey                      "kUSBHIDDevicePipeIdlePolicy"

#ifndef kUSBHostClassRequestCompletionTimeout
#define kUSBHostClassRequestCompletionTimeout                   5000ULL
#endif

#ifndef kHIDDriverRetryCount
#define kHIDDriverRetryCount                                    3
#endif

#define DISPATCH_TIME_FOREVER (~0ull)

// USB Language Identifiers 1.0
enum
{
    kLanguageIDEnglishUS = 0x0409
};


enum
{
    kInterruptRetries      = 10,
    kErrorRecoveryInterval = 50     // Milliseconds
};


// types of HID reports (input, output, feature)
enum
{
    kHIDInputReport         =     1,
    kHIDOutputReport,
    kHIDFeatureReport,
    kHIDUnknownReport        =    255
};


#define HID_MGR_2_USB_REPORT_TYPE(x) (x + 1)

#define USB_2_HID_MGR_REPORT_TYPE(x) (x - 1)

struct IOUSBHostHIDDescriptorInfo {
    uint8_t                        hidDescriptorType;
    uint16_t                       hidDescriptorLength;
    uint16_t getLength () const {
        return USBToHost16(hidDescriptorLength);
    }
    uint16_t getType () const {
        return hidDescriptorType;
    }
} __attribute__((packed));

/*!
 @typedef IOUSBHostHIDDescriptor
 @discussion USB HID Descriptor.  See the USB HID Specification at <a href="http://www.usb.org"TARGET="_blank">http://www.usb.org</a>.  (This structure
 should have used the #pragma pack(1) compiler directive to get byte alignment.
 */
struct IOUSBHostHIDDescriptor : public IOUSBDescriptorHeader
{
    uint16_t                        descVersNum;
    uint8_t                         hidCountryCode;
    uint8_t                         hidNumDescriptors;
    IOUSBHostHIDDescriptorInfo      entries[1];
}  __attribute__((packed));


struct IOUSBHostStringDescriptor : public IOUSBDescriptorHeader
{
    uint8_t     bString[1];
}  __attribute__((packed));


typedef struct {
    OSAction                    * hidAction;
    IOMemoryDescriptor          * report;
} IOUSBHIDCompletion;

enum
{
    kUSBHIDDKClass          = 3,
};

enum
{
    kUSBHIDDKBootInterfaceSubClass = 0x01,
};

enum
{
    kHIDDKKeyboardInterfaceProtocol = 1,
};


/*!
 @enum HID Protocol
 @discussion  Used in the SET_PROTOCOL device request
 */
enum
{
    kHIDDKBootProtocolValue   = 0,
    kHIDDKReportProtocolValue = 1
};

/*!
 @enum Default timeout values
 @discussion default values used for data and completion timeouts.
 */
enum
{
    kUSBDKDefaultControlNoDataTimeoutMS     = 5000,
    kUSBDKDefaultControlCompletionTimeoutMS = 0
};

/*!
 @enum HID requests
 @discussion Constants for HID requests.
 */
enum
{
    kHIDDKRqGetReport   = 1,
    kHIDDKRqGetIdle     = 2,
    kHIDDKRqGetProtocol = 3,
    kHIDDKRqSetReport   = 9,
    kHIDDKRqSetIdle     = 10,
    kHIDDKRqSetProtocol = 11
};


#define GetHIDDescriptor(cd,id) \
(const IOUSBHostHIDDescriptor *) IOUSBGetNextAssociatedDescriptorWithType(  \
    (const IOUSBConfigurationDescriptor*)cd ,                               \
    (const IOUSBDescriptorHeader*) id,                                      \
    NULL,                                                                   \
    kIOUSBDecriptorTypeHID);


struct IOUserUSBHostHIDDevice_IVars
{
    IOUSBHostDevice                     * device;
    IOUSBHostInterface                  * interface;

    const IOUSBInterfaceDescriptor      * interfaceDescriptor;
    const IOUSBConfigurationDescriptor  * configurationDescriptor;
    
    IOUSBHostPipe                       * inPipe;
    uint64_t                            inBufferSize;
    uint32_t                            inInterval;
    const IOUSBEndpointDescriptor *     inDescriptor;
    uint32_t                            retryCount;

    
    IOUSBHostPipe                       * outPipe;
    uint32_t                            outMaxPacketSize;
    const IOUSBEndpointDescriptor *     outDescriptor;

    IOBufferMemoryDescriptor            *zlpBuffer;
    IOBufferMemoryDescriptor            *reportBuffer;
    OSAction                            *ioAction;
    
    IOTimerDispatchSource               * timerSource;
    OSAction                            * timerAction;
    IODispatchQueue                     * queue;
    bool                                termination;
};


#define _bInterfaceNumber           (ivars->interfaceDescriptor->bInterfaceNumber)
#define _interfaceDescriptor        (ivars->interfaceDescriptor)
#define _configurationDescriptor    (ivars->configurationDescriptor)
#define _interface                  (ivars->interface)
#define _timerSource                (ivars->timerSource)
#define _timerAction                (ivars->timerAction)
#define _queue                      (ivars->queue)
#define _device                     (ivars->device)
#define _inPipe                     (ivars->inPipe)
#define _outPipe                    (ivars->outPipe)
#define _inBufferSize               (ivars->inBufferSize)
#define _outMaxPacketSize           (ivars->outMaxPacketSize)
#define _zlpBuffer                  (ivars->zlpBuffer)
#define _reportBuffer               (ivars->reportBuffer)
#define _ioAction                   (ivars->ioAction)
#define _inInterval                 (ivars->inInterval)
#define _retryCount                 (ivars->retryCount)
#define _inDescriptor               (ivars->inDescriptor)
#define _outDescriptor              (ivars->outDescriptor)
#define _termination                (ivars->termination)



#undef  super
#define super IOUserHIDDevice


bool IOUserUSBHostHIDDevice::init ()
{
    bool ret;
    
    ret = super::init();
    require_action(ret, exit, HIDLogError("init:%x", ret));
    
    assert(IOService::ivars);
    
    ivars = IONewZero(IOUserUSBHostHIDDevice_IVars, 1);

exit:

    return ret;
}

kern_return_t
IMPL(IOUserUSBHostHIDDevice, Start)
{
    OSDictionary *                      properties          = NULL;
    kern_return_t                       ret;
    uint8_t                             speed;
    uint64_t                            inputReportSize;
    uint64_t                            inMaxPacketSize;
    uint64_t                            idlePolicy;
    const IOUSBDeviceDescriptor *       deviceDescriptor    = NULL;
    

    ret = CopyDispatchQueue("Default", &_queue);
    require_noerr_action (ret, exit, HIDServiceLogError("CopyDispatchQueue:%x", ret));

    _interface = OSDynamicCast(IOUSBHostInterface, provider);
    require_action (_interface, exit, ret = kIOReturnError; HIDServiceLogError("IOUSBHostInterface"));
    
    _interface->retain();
    
    ret = _interface->CopyDevice(&_device);
    require_noerr_action (ret, exit, HIDServiceLogError("CopyDevice:%x", ret));

    _configurationDescriptor = _interface->CopyConfigurationDescriptor();
    require_action (_configurationDescriptor, exit, ret = kIOReturnNoResources; HIDServiceLogError("CopyConfigurationDescriptor"));

    _interfaceDescriptor = _interface->GetInterfaceDescriptor(_configurationDescriptor);
    require_action (_interfaceDescriptor, exit, ret = kIOReturnNoResources; HIDServiceLogError("GetInterfaceDescriptor"));

    deviceDescriptor = _device->CopyDeviceDescriptor();
    require_action (deviceDescriptor, exit, ret = kIOReturnNoResources; HIDServiceLogError("CopyDeviceDescriptor"));

    ret = Start(provider, SUPERDISPATCH);
    require_noerr_action (ret, exit, HIDServiceLogError("Start(SUPERDISPATCH):%x", ret));

    ret = CopyProperties(&properties, SUPERDISPATCH);
    require_noerr_action (ret, exit, HIDServiceLogError("CopyProperties:%x", ret));
    
    ret = _inPipe->GetSpeed (&speed);
    require_noerr_action (ret, exit, HIDServiceLogError("GetSpeed:%x", ret));
    
    _inInterval = IOUSBGetEndpointIntervalFrames(speed, _inDescriptor);

    inputReportSize = OSDictionaryGetUInt64Value(properties, kIOHIDMaxInputReportSizeKey);
    inMaxPacketSize = IOUSBGetEndpointMaxPacketSize (speed, _inDescriptor);
    
    inputReportSize = (0 == inputReportSize) ? inMaxPacketSize : inputReportSize;
    require_action (inputReportSize, exit, ret = kIOReturnError; HIDServiceLogError("inputReportSize"));
    
    _inBufferSize = ((inputReportSize + (inMaxPacketSize - 1)) / inMaxPacketSize) * inMaxPacketSize;
    
    HIDServiceLog ("inPipe:%d  inputReportSize:%d inMaxPacketSize:%d inBufferSize:%d",
                   _inPipe ? 1 : 0,
                   (unsigned int)inputReportSize,
                   (unsigned int)inMaxPacketSize,
                   (unsigned int)_inBufferSize);

    do {
        IOReturn status;
        if (!_outPipe) {
            break;
        }
        
        status = IOBufferMemoryDescriptor::Create(kIOMemoryDirectionInOut, sizeof(_outMaxPacketSize), 0, &_zlpBuffer);
        if (status) {
            HIDServiceLogError("IOMemoryDescriptor::Create:%x", status);
            break;
        }

        status = _outPipe->GetSpeed (&speed);
        if (status) {
            HIDServiceLogError("outPipe->GetSpeed:%x", status);
            break;
        }
        _outMaxPacketSize =  IOUSBGetEndpointMaxPacketSize (speed, _outDescriptor);
        
    } while (false);

    HIDServiceLog ("outPipe:%d  outMaxPacketSize:%d",
                   _outPipe ? 1 : 0,
                   (unsigned int)_outMaxPacketSize);


    if (  (_interfaceDescriptor->bInterfaceClass == kUSBHIDDKClass)
       && (_interfaceDescriptor->bInterfaceSubClass == kUSBHIDDKBootInterfaceSubClass)
       && (_interfaceDescriptor->bInterfaceProtocol == kHIDDKKeyboardInterfaceProtocol)) {
        
        setIdle (USBToHost16(deviceDescriptor->idVendor) == kIOUSBAppleVendorID ? 0 : 24);
    }
    
    if(kUSBHIDDKBootInterfaceSubClass == _interfaceDescriptor->bInterfaceSubClass) {
        setProtocol(kHIDDKReportProtocolValue);
    }
    
    idlePolicy = OSDictionaryGetUInt64Value (properties, kUSBHostHIDDevicePropertyIdlePolicy);
    if (idlePolicy) {
        setIdlePolicy (USBIdlePolicyTypeInterface, idlePolicy);
        setIdlePolicy (USBIdlePolicyTypePipe, idlePolicy);
    }
    
    idlePolicy = OSDictionaryGetUInt64Value (properties, kUSBHostHIDDeviceInterfaceIdlePolicyKey);
    if (idlePolicy) {
        setIdlePolicy (USBIdlePolicyTypeInterface, idlePolicy);
    }

    idlePolicy = OSDictionaryGetUInt64Value (properties, kUSBHostHIDDevicePipeIdlePolicyKey);
    if (idlePolicy) {
        setIdlePolicy (USBIdlePolicyTypePipe, idlePolicy);
    }

    _retryCount = kInterruptRetries;
    
    ret = _interface->CreateIOBuffer(kIOMemoryDirectionIn, _inBufferSize, &_reportBuffer);
    require_noerr_action(ret, exit, HIDServiceLogError("CreateIOBuffer: %x", ret));
    
    ret = CreateActionCompleteInputReport(
                           0,
                           &_ioAction);
    require_noerr_action (ret, exit, HIDServiceLogError("CreateActionCompleteInputReport:%x", ret));
   
    ret = initInputReport ();
    
exit:
    if (ret != kIOReturnSuccess) {
        HIDServiceLogFault("Start failed: 0x%x", ret);
    }
    
    OSSafeReleaseNULL(properties);
    
    if (deviceDescriptor) {
        IOUSBHostFreeDescriptor(deviceDescriptor);
    }
    
    return ret;
}

#define ShouldCancel(s) (s ? 1 : 0)
#define DoCancel(s,h) {if(s){(s)->Cancel(h);}}

kern_return_t
IMPL(IOUserUSBHostHIDDevice, Stop)
{
    
    kern_return_t ret = kIOReturnSuccess;
    uint64_t regID = 0;
    
    provider->GetRegistryEntryID(&regID);
    HIDTrace(kHIDDK_Dev_Stop, getRegistryID(), regID, 0, 0);
    
    __block _Atomic uint32_t cancelCount = 0;
    
    _termination = true;
    
    if (_interface) {
        IOReturn close = _interface->Close(this, 0);
        HIDServiceLog("Close interface: 0x%llx 0x%x", regID, close);
    }
    
    cancelCount += ShouldCancel (_timerSource);
    cancelCount += ShouldCancel (_ioAction);
    
    void (^finalize)(void) = ^{
        if (__c11_atomic_fetch_sub(&cancelCount, 1U, __ATOMIC_RELAXED) <= 1) {
            IOReturn status;
            status = Stop(provider, SUPERDISPATCH);
        }
    };
    
    if (!cancelCount) {
        ret = Stop(provider, SUPERDISPATCH);
    } else {
        DoCancel (_timerSource, finalize);
        DoCancel (_ioAction, finalize);
    }
    
    return ret;
}

//----------------------------------------------------------------------------------------------------------------------
// IOUSBHostHIDDevice::free
//----------------------------------------------------------------------------------------------------------------------
void IOUserUSBHostHIDDevice::free()
{
    
    if (_configurationDescriptor && _interface) {
       IOUSBHostFreeDescriptor(_configurationDescriptor);
    }
    OSSafeReleaseNULL(_timerSource);
    OSSafeReleaseNULL(_timerAction);
    OSSafeReleaseNULL(_device);
    OSSafeReleaseNULL(_interface);
    OSSafeReleaseNULL(_inPipe);
    OSSafeReleaseNULL(_outPipe);
    OSSafeReleaseNULL(_zlpBuffer);
    OSSafeReleaseNULL(_queue);
    OSSafeReleaseNULL(_reportBuffer);
    OSSafeReleaseNULL(_ioAction);
    IOSafeDeleteNULL(ivars, IOUserUSBHostHIDDevice_IVars, 1);
    super::free ();
}


bool IOUserUSBHostHIDDevice::handleStart (IOService * provider)
{
    kern_return_t   ret;
    uint64_t regID = 0;
 
    provider->GetRegistryEntryID(&regID);
    require_action(super::handleStart(provider), exit, ret = kIOReturnError; HIDServiceLogError("handleStart:false 0x%llx", regID));

    ret = _interface->Open(this, 0, 0);
    HIDServiceLog("Open interface: 0x%llx", regID);
    require_noerr_action(ret, exit, HIDServiceLogError("Open:%x", ret));

    ret = initPipes ();
    require_noerr_action (ret, exit,HIDServiceLogError("InitPipes:%x", ret));

    ret = IOTimerDispatchSource::Create(_queue, &_timerSource);
    require_noerr_action (ret, exit, HIDServiceLogError("IOTimerDispatchSource::Create:%x", ret));

    ret = CreateActionTimerOccurred(
                           sizeof(void *),
                           &_timerAction);
    require_noerr_action (ret, exit, HIDServiceLogError("CreateActionTimerOccurred:%x", ret));
    
    ret = _timerSource->SetHandler(_timerAction);
    require_noerr_action (ret, exit, HIDServiceLogError("IOTimerDispatchSource::SetHandler:%x", ret));

exit:
    
    return (ret == kIOReturnSuccess) ? true : false;
}


OSDictionary * IOUserUSBHostHIDDevice::newDeviceDescription ()
{
    kern_return_t                   ret;
    OSObjectPtr                     value;
    OSDictionary *                  properties       = NULL;
    OSDictionary *                  dictionary       = NULL;
    const IOUSBDeviceDescriptor *   deviceDescriptor = NULL;
    const IOUSBHostHIDDescriptor *  hidDescriptor;
    uint64_t                        portType         = 0;
    
    dictionary = OSDictionary::withCapacity(10);
    require_action (dictionary, exit, ret = kIOReturnNoMemory; HIDServiceLogError("OSDictionaryCreate"));
    
    ret = _interface->CopyProperties(&properties);
    require_noerr_action (ret, exit, HIDServiceLogError("CopyProperties:%x", ret));
    
    deviceDescriptor = _device->CopyDeviceDescriptor();
    require_action (deviceDescriptor, exit, ret = kIOReturnNoResources; HIDServiceLogError("CopyDeviceDescriptor"));

    hidDescriptor = GetHIDDescriptor(_configurationDescriptor, _interfaceDescriptor);
    require_action (hidDescriptor, exit, ret = kIOReturnNoResources; HIDServiceLogError("Ho HID descriptor"));

    
    OSDictionarySetUInt64Value(dictionary, kIOHIDReportIntervalKey, _inInterval);
    OSDictionarySetUInt64Value(dictionary, kIOHIDVendorIDKey, USBToHost16(deviceDescriptor->idVendor));
    OSDictionarySetUInt64Value(dictionary, kIOHIDProductIDKey, USBToHost16(deviceDescriptor->idProduct));
    OSDictionarySetStringValue(dictionary, kIOHIDTransportKey, "USB");
    OSDictionarySetUInt64Value(dictionary, kIOHIDVersionNumberKey, USBToHost16(deviceDescriptor->bcdDevice));
    OSDictionarySetUInt64Value(dictionary, kIOHIDCountryCodeKey, USBToHost16(hidDescriptor->hidCountryCode));
    OSDictionarySetUInt64Value(dictionary, kIOHIDRequestTimeoutKey, kUSBHostClassRequestCompletionTimeout * 1000);

    
    value = OSDictionaryGetValue (properties, kUSBHostPropertyLocationID);
    if (value) {
        OSDictionarySetValue(dictionary, kIOHIDLocationIDKey, value);
    }

    value = copyStringAtIndex(deviceDescriptor->iManufacturer, kLanguageIDEnglishUS);
    if (value) {
        OSDictionarySetValue(dictionary, kIOHIDManufacturerKey, value);
        OSSafeReleaseNULL(value);
    }

    value = copyStringAtIndex(deviceDescriptor->iProduct, kLanguageIDEnglishUS);
    if (value) {
        OSDictionarySetValue(dictionary, kIOHIDProductKey, value);
        OSSafeReleaseNULL(value);
    }

    value = copyStringAtIndex(deviceDescriptor->iSerialNumber, kLanguageIDEnglishUS);
    if (value) {
        OSDictionarySetValue(dictionary, kIOHIDSerialNumberKey, value);
        OSSafeReleaseNULL(value);
    }

    portType = OSDictionaryGetUInt64Value (properties, kUSBHostMatchingPropertyPortType);
    if (portType == kIOUSBHostPortTypeInternal) {
        OSDictionarySetValue(dictionary, kIOHIDBuiltInKey, kOSBooleanTrue);
    }
    
exit:
    
    OSSafeReleaseNULL(properties);
    if (deviceDescriptor) {
        IOUSBHostFreeDescriptor(deviceDescriptor);
    }
    
    return dictionary;
}

OSData * IOUserUSBHostHIDDevice::newReportDescriptor ()
{
    kern_return_t                       ret             = kIOReturnNotFound;
    uint64_t                            address         = 0;
    uint64_t                            length          = 0;
    IOBufferMemoryDescriptor            * buffer        = NULL;
    const IOUSBHostHIDDescriptorInfo    * reportDescInfo;
    uint8_t                             reportDescIndex;
    uint16_t                            bytesTransferred = 0;
    OSData                              * descriptor     = NULL;
    
    ret = getHIDDescriptorInfo(kIOUSBDecriptorTypeReport , &reportDescInfo, &reportDescIndex);
    require_noerr_action (ret, exit, HIDServiceLogError("GetHIDDescriptorInfo:%x", ret));
    
    ret = IOBufferMemoryDescriptor::Create (kIOMemoryDirectionIn, reportDescInfo->getLength(), 0, &buffer);
    require_noerr_action (ret, exit, HIDServiceLogError("IOBufferMemoryDescriptor::Create:%x", ret));
    
    for (unsigned int i = 0; i < kHIDDriverRetryCount ; i++)
    {
        ret = _device->DeviceRequest (_interface,
                                      kIOUSBDeviceRequestDirectionIn | kIOUSBDeviceRequestTypeStandard | kIOUSBDeviceRequestRecipientInterface,
                                      kIOUSBDeviceRequestGetDescriptor,
                                      static_cast<uint16_t>((kIOUSBDecriptorTypeReport << 8) + reportDescIndex),
                                      _bInterfaceNumber,
                                      static_cast<uint16_t>(reportDescInfo->getLength()),
                                      buffer,
                                      &bytesTransferred,
                                      kUSBDKDefaultControlNoDataTimeoutMS);

        if (ret) {
            HIDServiceLogError("DeviceRequest [%x %x %x %x]:0x%x",
                               kIOUSBDeviceRequestDirectionIn | kIOUSBDeviceRequestTypeStandard | kIOUSBDeviceRequestRecipientInterface,
                               kIOUSBDeviceRequestGetDescriptor,
                               static_cast<uint16_t>((kIOUSBDecriptorTypeReport << 8) + reportDescIndex),
                               _bInterfaceNumber,
                               ret);
            IOSleep(100);
        } else {
            break;
        }
    }
    require_noerr_action (ret, exit, HIDServiceLogError("No report descriptor"));
    
    ret = buffer->Map(0, 0, 0, 0, &address, &length);
    require_action ((ret == kIOReturnSuccess) && length, exit, HIDServiceLogError("buffer->Map:%x (length:%llu)", ret, length));
    
    HIDServiceLogInfo("HID descriptor interface:%d index:%d length:%d %llu %d",
                      _bInterfaceNumber, reportDescIndex, reportDescInfo->getLength(), length, bytesTransferred);
    
    descriptor = OSDataCreate ((const void *) address, length);
    require_action (descriptor, exit, HIDServiceLogError("OSDataCreate"));

exit:

    OSSafeReleaseNULL (buffer);
    return descriptor;
}

kern_return_t IOUserUSBHostHIDDevice::initInputReport()
{
    kern_return_t ret;
    
    require_action(!_termination && _ioAction && _reportBuffer, exit, ret = kIOReturnOffline);
    
    ret = _inPipe->AsyncIO(_reportBuffer, _inBufferSize, _ioAction, 0);
    if (ret == kUSBHostReturnPipeStalled) {
        ret = _inPipe->ClearStall(false);
        if (ret == kIOReturnSuccess) {
            ret = _inPipe->AsyncIO(_reportBuffer, _inBufferSize, _ioAction, 0);
        }
    }
    
exit:
    if (ret) {
        HIDServiceLogError("initInputReport:0x%x", ret);
    }
 
    return ret;
}

void IOUserUSBHostHIDDevice::cancelInputReportRetry ()
{
    kern_return_t ret;
    ret = _timerSource->WakeAtTime(0, DISPATCH_TIME_FOREVER, 0);
    if (ret) {
        HIDServiceLogError("WakeAtTime:0x%x", ret);
    }
    _retryCount = kInterruptRetries;
}

void IOUserUSBHostHIDDevice::scheduleInputReportRetry (kern_return_t reason)
{
    kern_return_t   ret;
    uint64_t        deadline;

    if  (_termination == true) {
        return;
    }
    
    if (reason != kIOReturnAborted && _retryCount < 1) {
        reset ();
        return;
    }
    
    if (reason == kIOReturnAborted) {
        deadline = _inInterval * kHIDMillisecondScale;
    } else {
        deadline = kErrorRecoveryInterval * kHIDMillisecondScale;
        --_retryCount;
    }
    
    HIDServiceLogError("Schedule retry reason:0x%x count:%d deadline:%llums", reason, _retryCount, deadline/kHIDMillisecondScale);
    
    deadline += mach_absolute_time() ;
    ret = _timerSource->WakeAtTime(0, deadline, 0);
    if (ret) {
        HIDServiceLogError("WakeAtTime:0x%x", ret);
    }

}

#define  SYNC_REPORT_HADLING

kern_return_t IOUserUSBHostHIDDevice::getReport(IOMemoryDescriptor      * report,
                                                IOHIDReportType         reportType,
                                                IOOptionBits            options,
                                                uint32_t                completionTimeout,
                                                OSAction                * action)
{

    kern_return_t   status;
    uint32_t        bytesTransferred = 0;

    status = getReport(report, reportType, options, completionTimeout, &bytesTransferred);
    if (status == kIOReturnSuccess) {
        IOBufferMemoryDescriptor * reportBuffer = OSDynamicCast (IOBufferMemoryDescriptor, report);
        kern_return_t ret = reportBuffer->SetLength(bytesTransferred);
        if (ret) {
            HIDServiceLogError("SetLength:%x", ret);
        }
    }
    CompleteReport(action, status, bytesTransferred);

    return kIOReturnSuccess;
}


kern_return_t IOUserUSBHostHIDDevice::setReport(IOMemoryDescriptor      * report,
                                                IOHIDReportType         reportType,
                                                IOOptionBits            options,
                                                uint32_t                completionTimeout,
                                                OSAction                * action)
{
    kern_return_t ret = setReport(report, reportType, options, completionTimeout);
    CompleteReport(action, ret, 0);

    return ret;
}



kern_return_t IOUserUSBHostHIDDevice::setProtocol (uint16_t protocol)
{
    kern_return_t ret;
    
    ret = _device->DeviceRequest (_interface,
                                  kIOUSBDeviceRequestDirectionOut | kIOUSBDeviceRequestTypeClass | kIOUSBDeviceRequestRecipientInterface,
                                  kHIDDKRqSetProtocol,
                                  protocol,
                                  _bInterfaceNumber,
                                  0,
                                  NULL,
                                  NULL,
                                  kUSBDKDefaultControlNoDataTimeoutMS);
    if (ret) {
        HIDServiceLogError("DeviceRequest [%x %x %x %x]:0x%x",
                           kIOUSBDeviceRequestDirectionOut | kIOUSBDeviceRequestTypeClass | kIOUSBDeviceRequestRecipientInterface,
                           kHIDDKRqSetProtocol,
                           protocol,
                           _bInterfaceNumber,
                           ret);

    }
    return ret;
}


void
IMPL(IOUserUSBHostHIDDevice, CompleteInputReport)
{
    HIDTrace(kHIDDK_Dev_InputReport, getRegistryID(), completionTimestamp, status, actualByteCount);
    
    if (status) {
        HIDServiceLogDebug("CompleteInReport:0x%x", status);
    }
    
    //HIDServiceLogDebug("CompleteInReport actualByteCount:%d", actualByteCount);
    
    require(_termination == false, exit);

    if (status == kIOReturnSuccess) {
        cancelInputReportRetry();
        handleReport(completionTimestamp, _reportBuffer, actualByteCount, kIOHIDReportTypeInput, 0);
        status = initInputReport();
    }
    
    if (status) {
        scheduleInputReportRetry(status);
    }
    
exit:
    return;
}


void
IMPL(IOUserUSBHostHIDDevice, CompleteZLP)
{
    return;
}

void
IMPL(IOUserUSBHostHIDDevice, TimerOccurred)
{
    HIDServiceLog("TimerOccurred retry:%d", _retryCount);

    _timerSource->WakeAtTime(0, DISPATCH_TIME_FOREVER, 0);

    kern_return_t  ret = initInputReport();
    if (ret) {
        scheduleInputReportRetry (ret);
    }
    
    return;
}

kern_return_t IOUserUSBHostHIDDevice::setIdle (uint16_t idleTimeMs)
{
    kern_return_t  ret;
    
    HIDServiceLog("SetIdle:%dms", idleTimeMs);

    ret = _device->DeviceRequest (_interface,
                                  kIOUSBDeviceRequestDirectionOut | kIOUSBDeviceRequestTypeClass | kIOUSBDeviceRequestRecipientInterface,
                                  kHIDDKRqSetIdle,
                                  ((idleTimeMs / 4) << 8),
                                  _bInterfaceNumber,
                                  0,
                                  NULL,
                                  NULL,
                                  kUSBDKDefaultControlNoDataTimeoutMS);

    if (ret) {
        HIDServiceLogError("DeviceRequest [%x %x %x %x]:0x%x",
                           kIOUSBDeviceRequestDirectionOut | kIOUSBDeviceRequestTypeClass | kIOUSBDeviceRequestRecipientInterface,
                           kHIDDKRqSetIdle,
                           ((idleTimeMs / 4) << 8),
                           _bInterfaceNumber,
                           ret);
    }
    return ret;
}

kern_return_t IOUserUSBHostHIDDevice::setIdlePolicy(USBIdlePolicyType type, uint16_t idleTimeMs)
{
    kern_return_t ret = kIOReturnSuccess;
    
    HIDServiceLog("setIdlePolicy:%dms type:%d", idleTimeMs, (int) type);

    switch (type) {
        case USBIdlePolicyTypeInterface:
            ret = _interface->SetIdlePolicy(idleTimeMs);
            break;
        case USBIdlePolicyTypePipe:
            if (_inPipe) {
                ret = _inPipe->SetIdlePolicy(idleTimeMs);
            }
            if (_outPipe) {
                ret = _outPipe->SetIdlePolicy(idleTimeMs);
            }
            break;
        default:
            break;
    }
    if (ret) {
        HIDServiceLogError("setIdlePolicy:%dms type:%d ret:0x%x", idleTimeMs, (int) type, ret);
    }

    return ret;
}


kern_return_t IOUserUSBHostHIDDevice::initPipes ()
{
    kern_return_t ret = kIOReturnSuccess;
    const IOUSBEndpointDescriptor * endpointDescriptor = NULL;
 
    
    while ((endpointDescriptor = IOUSBGetNextEndpointDescriptor (_configurationDescriptor,
                                                                _interfaceDescriptor,
                                                                (const IOUSBDescriptorHeader *)endpointDescriptor)) != NULL) {
        
        if (IOUSBGetEndpointType (endpointDescriptor) == kIOUSBEndpointTypeInterrupt) {
            IOUSBHostPipe ** pipe = (IOUSBGetEndpointDirection(endpointDescriptor) == kIOUSBEndpointDirectionIn) ? &_inPipe : &_outPipe;
            const IOUSBEndpointDescriptor **  decriptor = (IOUSBGetEndpointDirection(endpointDescriptor) == kIOUSBEndpointDirectionIn) ? &_inDescriptor : &_outDescriptor;

            *decriptor = endpointDescriptor;
            
            ret  = _interface->CopyPipe (IOUSBGetEndpointAddress(endpointDescriptor), pipe);
            require_noerr_action (ret, exit, HIDServiceLogError("CopyPipe:%x", ret));
            if (_inPipe != NULL && _outPipe != NULL) {
                break;
            }
        }
    }
    
    ret = _inPipe != NULL ? kIOReturnSuccess : kIOReturnNotFound;
    
exit:
    
    return ret;
}

kern_return_t IOUserUSBHostHIDDevice::getHIDDescriptorInfo (uint8_t type, const IOUSBHostHIDDescriptorInfo ** info, uint8_t * index)
{
    const IOUSBHostHIDDescriptor * descriptor = GetHIDDescriptor(_configurationDescriptor, _interfaceDescriptor);
    require_action (descriptor, exit, HIDServiceLogError("No HID decriptor"));
    
    for (unsigned int i = 0;  i < descriptor->hidNumDescriptors; i++) {
        if (descriptor->entries[i].getType() == type) {
            if (info) {
                *info = &descriptor->entries[i];
            }
            if (index) {
                *index = i;
            }
            return kIOReturnSuccess;
        }
    }
    
exit:
    
    return kIOReturnNotFound;
}

OSString * IOUserUSBHostHIDDevice::copyStringAtIndex (uint8_t index, uint16_t lang)
{
    const IOUSBStringDescriptor * descriptor;
    OSString                    * string = NULL;
    char                        strVal [256];
    unsigned int                strIdx;
    
    if (!index) {
        return NULL;
    }

    descriptor = _interface->CopyStringDescriptor (index, lang);
    require_action (descriptor, exit, HIDServiceLogError("CopyStringDescriptor(%d,%d)", index, lang));
    
    for (strIdx = 0 ; strIdx < (descriptor->bLength - sizeof(IOUSBDescriptorHeader)) / 2; strIdx++) {
        if (*(uint16_t*)&(descriptor->bString[strIdx * 2]) == 0) {
            break;
        }
        strVal[strIdx] = descriptor->bString[strIdx * 2];
    }
    
    strVal[strIdx] = 0;
    
    string = OSStringCreate(strVal, strIdx);

exit:

    if (descriptor) {
        IOUSBHostFreeDescriptor(descriptor);
    }
 
    return string;
}


kern_return_t
IOUserUSBHostHIDDevice::getReport (IOMemoryDescriptor      * report,
                                   IOHIDReportType         reportType,
                                   IOOptionBits            options,
                                   uint32_t                completionTimeout,
                                   uint32_t                * bytesTransferred)
{
    kern_return_t           ret              = kIOReturnNotReady;
    uint64_t                reportLength     = 0;
    uint8_t                 reportID;
    uint8_t                 usbReportType;
    
    require_action(report && bytesTransferred , exit, ret = kIOReturnBadArgument);
    require_action(!_termination, exit, ret = kIOReturnOffline);
    
    if (!completionTimeout) {
        completionTimeout = kUSBHostClassRequestCompletionTimeout;
    }

    report->GetLength(&reportLength);

    usbReportType = HID_MGR_2_USB_REPORT_TYPE(reportType);
    reportID = (uint8_t)(options & 0xff);

    ret = _device->DeviceRequest (_interface,
                                  kIOUSBDeviceRequestDirectionIn | kIOUSBDeviceRequestTypeClass | kIOUSBDeviceRequestRecipientInterface,
                                  kHIDDKRqGetReport,
                                  (uint16_t)((usbReportType << 8) | reportID),
                                  _bInterfaceNumber,
                                  (uint16_t)(reportLength),
                                  report,
                                  (uint16_t *)bytesTransferred,
                                  completionTimeout);
    
    
exit:
    return ret;
}


kern_return_t
IOUserUSBHostHIDDevice::setReport (IOMemoryDescriptor      * report,
                                   IOHIDReportType         reportType,
                                   IOOptionBits            options,
                                   uint32_t                completionTimeout)
{
    kern_return_t           ret              = kIOReturnNotReady;
    uint64_t                reportLength     = 0;
    uint8_t                 reportID;
    uint8_t                 usbReportType;
    uint32_t                bytesTransferred = 0;
    
    require_action(!_termination, exit, ret = kIOReturnOffline);
    
    if (!completionTimeout) {
        completionTimeout = kUSBHostClassRequestCompletionTimeout;
    }
    
    report->GetLength(&reportLength);
    
    usbReportType = HID_MGR_2_USB_REPORT_TYPE(reportType);
    
    do {
        if (usbReportType != kHIDOutputReport) {
            break;
        }
        
        if (_outPipe == NULL || _outMaxPacketSize == 0) {
            break;
        }
        
        ret = _outPipe->IO(report,
                           (uint32_t)(reportLength),
                           &bytesTransferred,
                           0);
        if (ret != kIOReturnSuccess) {
            HIDServiceLogError("_outPipe->IO(%d):0x%x", (int)reportLength, ret);
            break;
        }
        
        if (reportLength != _outMaxPacketSize && (reportLength % _outMaxPacketSize) == 0) {
            _outPipe->IO(_zlpBuffer, 0, &bytesTransferred, 0);
        }
        
        return ret;
    } while (false);
    
    reportID = (uint8_t)(options & 0xff);
    bytesTransferred = 0;
    ret = _device->DeviceRequest (_interface,
                                  kIOUSBDeviceRequestDirectionOut | kIOUSBDeviceRequestTypeClass | kIOUSBDeviceRequestRecipientInterface,
                                  kHIDDKRqSetReport,
                                  (uint16_t)((usbReportType << 8) | reportID),
                                  _bInterfaceNumber,
                                  (uint16_t)(reportLength),
                                  report,
                                  (uint16_t*)&bytesTransferred,
                                  completionTimeout);
    
exit:
    return ret;
}


void  IOUserUSBHostHIDDevice::setProperty(OSObject * key, OSObject * value)
{
    OSString * keyStr = NULL;
    OSNumber * valNum = NULL;
    
    require (_termination == false && (keyStr = OSDynamicCast(OSString, key)), exit);

    if (keyStr->isEqualTo(kUSBHostHIDDevicePropertyIdlePolicy) &&
        (valNum = OSDynamicCast(OSNumber, value))) {
        setIdlePolicy (USBIdlePolicyTypeInterface, valNum->unsigned32BitValue());
        setIdlePolicy (USBIdlePolicyTypePipe, valNum->unsigned32BitValue());
        return;
    }

    if (keyStr->isEqualTo(kUSBHostHIDDeviceInterfaceIdlePolicyKey) &&
        (valNum = OSDynamicCast(OSNumber, value))) {
        setIdlePolicy (USBIdlePolicyTypeInterface, valNum->unsigned32BitValue());
        return;
    }

    if (keyStr->isEqualTo(kUSBHostHIDDevicePipeIdlePolicyKey) &&
        (valNum = OSDynamicCast(OSNumber, value))) {
        setIdlePolicy (USBIdlePolicyTypePipe, valNum->unsigned32BitValue());
        return;
    }

exit:

    super::setProperty(key, value);
}

void IOUserUSBHostHIDDevice::reset ()
{
    kern_return_t   ret;
    
    HIDServiceLogError("USB Device Reset");
    OSDictionaryPtr property = OSDictionaryCreate();
    
    if (property) {
        OSDictionarySetStringValue(property, "State", "Reset");
        SetProperties(property);
        OSSafeReleaseNULL(property);
    }
    
    ret = _device->Reset();
    if (ret) {
        HIDServiceLogError("Reset:0x%x", ret);
    }
}
