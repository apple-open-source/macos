#include <assert.h>
#include <AssertMacros.h>
#include <stdio.h>
#include <stdlib.h>
#include <DriverKit/DriverKit.h>
#include <DriverKit/OSCollections.h>
#include <DriverKit/IOBufferMemoryDescriptor.h>
#include <HIDDriverKit/HIDDriverKit_Private.h>

#undef super
#define super IOService

kern_return_t IOHIDDevice::getReport(IOMemoryDescriptor      * report,
                                     IOHIDReportType         reportType,
                                     IOOptionBits            options,
                                     uint32_t                completionTimeout,
                                     OSAction                * action)
{
    HIDLogError("%s", __PRETTY_FUNCTION__);
    return  kIOReturnUnsupported;
}

kern_return_t IOHIDDevice::setReport(IOMemoryDescriptor      * report,
                                     IOHIDReportType         reportType,
                                     IOOptionBits            options,
                                     uint32_t                completionTimeout,
                                     OSAction                * action)
{
    HIDLogError("%s", __PRETTY_FUNCTION__);
    return  kIOReturnUnsupported;
}

void
IMPL(IOHIDDevice, _ProcessReport)
{
    kern_return_t ret;
    
    switch (command) {
        case kIOHIDReportCommandSetReport:
            ret = setReport(report, reportType, options,  completionTimeout, action);
            break;
        case kIOHIDReportCommandGetReport:
            ret = getReport(report, reportType, options,  completionTimeout, action);
            break;
        default:
            ret = kIOReturnBadArgument;
    }
    
    if (ret) {
        HIDServiceLogError("ProcessReport cmd:%d type:%d options:%x timeout:%d ret:0x%x", command, reportType, options, completionTimeout, ret);
    }
}

void
IMPL(IOHIDDevice, _SetProperty)
{
    kern_return_t              ret;
    uint64_t                   address  = 0;
    uint64_t                   length   = 0;
    OSSerializationPtr         serial   = NULL;
    OSObjectPtr                obj      = NULL;
    OSDictionary               * dict   = NULL;
    
    
    ret = serialization->Map(0, 0, 0, 0, &address, &length);
    require_noerr_action(ret, exit, HIDLogError("Map"));
    
    serial = OSSerialization::createFromBytes((const void *) address,
                                              length,
                                              ^(const void *, size_t ) {
                                              });
    require_action(serial, exit, HIDLogError("createFromBytes"));
    
    obj = serial->copyObject();
    require_action(obj, exit, HIDLogError("copyObject"));

    dict = OSDynamicCast(OSDictionary, obj);
    require_action(dict, exit, HIDLogError("OSDictionary"));

    dict->iterateObjects(^bool(OSObject *key, OSObject *value) {
        setProperty (key, value);
        return false;
    });

exit:
    
    OSSafeReleaseNULL(serial);
    OSSafeReleaseNULL(obj);
    
    return;
}

void IOHIDDevice::setProperty(OSObject * key, OSObject * value)
{
    
}

kern_return_t IOHIDDevice::handleReport(uint64_t timestamp,
                                        IOMemoryDescriptor *report,
                                        uint32_t reportLength,
                                        IOHIDReportType reportType,
                                        IOOptionBits options)
{
    return _HandleReport(timestamp, report, reportLength, reportType, options);
}
