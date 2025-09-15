/*
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 2012 Apple, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <AssertMacros.h>
#include <IOKit/IOLib.h>
#include <sys/proc.h>
#include <TargetConditionals.h>
#include <os/overflow.h>
#include <IOKit/IOKitKeys.h>
#include <stdatomic.h>

#define kIOHIDManagerUserAccessUserDeviceEntitlement "com.apple.hid.manager.user-access-device"
#define kIOHIDVirtualDeviceEntitlement "com.apple.developer.hid.virtual.device"

enum {
    kIOHIDUserDeviceCreateOptionStartWhenScheduled = (1<<0)
};

#ifdef enqueue
    #undef enqueue
#endif

#include "IOHIDResourceUserClient.h"
#include <libkern/OSAtomic.h>
#include <libkern/c++/OSValueObject.h>
#include "IOHIDDebug.h"
#include "IOHIDPrivateKeys.h"
#include "IOHIDFamilyPrivate.h"
#include "IOHIDFamilyTrace.h"

#define kHIDQueueSize           16384

#define kMaxQueueReportSize     4 * 1024 // 4k. This size already works.

#define super IOUserClient2022


OSDefineMetaClassAndStructors(IOHIDResourceDeviceUserClient, IOUserClient2022)

typedef struct {
    IOReturn             ret;
    IOMemoryDescriptor * descriptor;
    u_int64_t            token;
    IOHIDCompletion      completion;
    AbsoluteTime         deadline;
} __ReportResult;

OSDefineValueObjectForDependentType(__ReportResult)

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDResourceDeviceUserClient::_methods
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
const IOExternalMethodDispatch2022 IOHIDResourceDeviceUserClient::_methods[kIOHIDResourceDeviceUserClientMethodCount] = {
    {   // kIOHIDResourceDeviceUserClientMethodCreate
        (IOExternalMethodAction) &IOHIDResourceDeviceUserClient::_createDevice,
        1, kIOUCVariableStructureSize, /* 1 struct input : the report descriptor */
        0, 0,
        false
    },
    {   // kIOHIDResourceDeviceUserClientMethodTerminate
        (IOExternalMethodAction) &IOHIDResourceDeviceUserClient::_terminateDevice,
        0, 0,
        0, 0,
        false
    },
    {   // kIOHIDResourceDeviceUserClientMethodHandleReport
        (IOExternalMethodAction) &IOHIDResourceDeviceUserClient::_handleReport,
        1, kIOUCVariableStructureSize, /* 1 struct input : the buffer */
        0, 0,
        true
    },
    {   // kIOHIDResourceDeviceUserClientMethodPostReportResult
        (IOExternalMethodAction) &IOHIDResourceDeviceUserClient::_postReportResult,
        kIOHIDResourceUserClientResponseIndexCount, kIOUCVariableStructureSize, /* 1 scalar input: the result, 1 struct input : the buffer */
        0, 0,
        false
    },
    {   // kIOHIDResourceDeviceUserClientMethodRegisterService
        (IOExternalMethodAction) &IOHIDResourceDeviceUserClient::_registerService,
        0, 0,
        0, 0,
        false
    },
    { // kIOHIDResourceDeviceUserClientMethodReleaseToken
        (IOExternalMethodAction) &IOHIDResourceDeviceUserClient::_releaseToken,
        1, 0,
        0, 0,
        false
    },
};



//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::initWithTask
//----------------------------------------------------------------------------------------------------
bool IOHIDResourceDeviceUserClient::initWithTask(task_t owningTask, void * security_id, UInt32 type)
{
    bool result = false;
    OSObject* entitlement = NULL;
    
    require_action(super::initWithTask(owningTask, security_id, type), exit, HIDLogError("failed"));
    
    entitlement = copyClientEntitlement(owningTask, kIOHIDManagerUserAccessUserDeviceEntitlement);
    if (entitlement) {
      result = (entitlement == kOSBooleanTrue);
      entitlement->release();
    }
    
    // check for developer entitlement
    if (!result) {
        entitlement = copyClientEntitlement(owningTask, kIOHIDVirtualDeviceEntitlement);
        if (entitlement) {
            result = (entitlement == kOSBooleanTrue);
            entitlement->release();
        }
    } else {
        _privileged = true;
    }
    
    if (!result) {
      proc_t      process;
      process = (proc_t)get_bsdtask_info(owningTask);
      char name[255];
      bzero(name, sizeof(name));
      proc_name(proc_pid(process), name, sizeof(name));
      HIDServiceLogError("%s is not entitled", name);
      goto exit;
    }
    
    _pending            = OSSet::withCapacity(4);
    _maxClientTimeoutUS = kIOHIDDeviceDefaultAsyncRequestTimeout * 1000;
    _owningTask = owningTask;
    _overSizedReports = OSArray::withCapacity(2);

exit:
    return result;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::start
//----------------------------------------------------------------------------------------------------
bool IOHIDResourceDeviceUserClient::start(IOService * provider)
{
    IOWorkLoop *workLoop;
    bool result = false;
    OSSerializer *debugSerializer = NULL;
    
    _owner = OSDynamicCast(IOHIDResource, provider);
    require_action(_owner, exit, result=false);
    _owner->retain();
    
    require_action(super::start(provider), exit, result=false);
    
    workLoop = getWorkLoop();
    require_action(workLoop, exit, result=false);
    
    _asyncReportTimer = IOTimerEventSource::timerEventSource(this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &IOHIDResourceDeviceUserClient::setNextAsyncTimeout));
    require_action(_asyncReportTimer, exit, result=false);
    require_noerr_action(workLoop->addEventSource(_asyncReportTimer), exit, result=false);
    
    _commandGate = IOCommandGate::commandGate(this);
    require_action(_commandGate, exit, result=false);
    require_noerr_action(workLoop->addEventSource(_commandGate), exit, result=false);
    
    debugSerializer = OSSerializer::forTarget(this, OSMemberFunctionCast(OSSerializerCallback, this, &IOHIDResourceDeviceUserClient::serializeDebugState));
    require(debugSerializer, exit);

    setProperty("IOHIDUserDeviceDebugState",                              debugSerializer);
    setProperty(kIOUserClientDefaultLockingKey,                           kOSBooleanTrue);
    setProperty(kIOUserClientDefaultLockingSetPropertiesKey,              kOSBooleanTrue);
    setProperty(kIOUserClientDefaultLockingSingleThreadExternalMethodKey, kOSBooleanFalse);
    setProperty(kIOUserClientEntitlementsKey,                             kOSBooleanFalse);
    
    result = true;
    
exit:
    if ( result==false ) {
        HIDLogError("failed");
        stop(provider);
    }
    
    OSSafeReleaseNULL(debugSerializer);

    return result;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::stop
//----------------------------------------------------------------------------------------------------
void IOHIDResourceDeviceUserClient::stop(IOService * provider)
{
    IOWorkLoop * workLoop = getWorkLoop();
    
    require(workLoop, exit);
    
    if (_asyncReportTimer) {
        _asyncReportTimer->cancelTimeout();
        workLoop->removeEventSource(_asyncReportTimer);
    }
    
    if ( _commandGate ) {
        cleanupPendingReports();

        workLoop->removeEventSource(_commandGate);
    }

    releaseNotificationPort (_port);
    _port = MACH_PORT_NULL;

exit:
    OSSafeReleaseNULL(_device);

    super::stop(provider);
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::free
//----------------------------------------------------------------------------------------------------
void IOHIDResourceDeviceUserClient::free()
{
    if ( _properties )
        _properties->release();
    
    if ( _commandGate ) {
        _commandGate->release();
    }

    if ( _asyncReportTimer )
        _asyncReportTimer->release();
    
    if (_pending) {
        _pending->release();
    }
    
    if ( _queue )
        _queue->release();
    
    if ( _owner )
        _owner->release();

    if ( _overSizedReports )
        _overSizedReports->release();
        
    return super::free();
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::registerNotificationPort
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::registerNotificationPort(mach_port_t port, UInt32 type __unused, io_user_reference_t refCon __unused)
{
    IOReturn result;
    
    require_action(!isInactive(), exit, result=kIOReturnOffline);
    
    result = _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDResourceDeviceUserClient::registerNotificationPortGated), port);
    
exit:
    return result;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::registerNotificationPortGated
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::registerNotificationPortGated(mach_port_t port)
{
    IOReturn result;
    bool skipCleanup = false;
    
    require_action(!isInactive(), exit, result=kIOReturnOffline);
    require_action(_queue, exit, result=kIOReturnError);

    releaseNotificationPort (_port);
    
    _port = port;
    _queue->setNotificationPort(port);

#if DEVELOPMENT || KASAN
    // To support testing for 154632669, do not cleanup pending reports when user client removes its
    // notification port upon cancellation. This will delay full termination until all reports
    // complete (i.e., timeout).
    skipCleanup = _properties->getObject("test-hook-154632669") == kOSBooleanTrue;
#endif
    
    // 43101337 clear pending reports when our port is cleared, otherwise we will
    // wait until the timeout occurs, which could be a very long time.
    if (!_port && _commandGate && !skipCleanup) {
        cleanupPendingReports();
    }
    
    result = kIOReturnSuccess;

exit:
    
    return result;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::clientMemoryForType
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::clientMemoryForType(UInt32 type __unused, IOOptionBits * options, IOMemoryDescriptor ** memory )
{
    IOReturn result;
    
    require_action(!isInactive(), exit, result=kIOReturnOffline);

    result = _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDResourceDeviceUserClient::clientMemoryForTypeGated), options, memory);
    
exit:
    return result;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::clientMemoryForTypeGated
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::clientMemoryForTypeGated(IOOptionBits * options, IOMemoryDescriptor ** memory )
{
    IOReturn ret;
    IOMemoryDescriptor * memoryToShare = NULL;
    
    require_action(!isInactive(), exit, ret=kIOReturnOffline);
    
    if ( !_queue ) {
        _queue = IOHIDResourceQueue::withCapacity(this, kHIDQueueSize);
    }
    
    require_action(_queue, exit, ret = kIOReturnNoMemory);
    
    memoryToShare = _queue->getMemoryDescriptor();
    require_action(memoryToShare, exit, ret = kIOReturnNoMemory);

    memoryToShare->retain();

    ret = kIOReturnSuccess;

exit:
    // set the result
    *options = 0;
    *memory  = memoryToShare;

    return ret;
}
//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::externalMethod
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::externalMethod(uint32_t selector, IOExternalMethodArgumentsOpaque * args)
{
    ExternalMethodGatedArguments gatedArguments = {selector, args};
    IOReturn result;

    result = _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDResourceDeviceUserClient::externalMethodGated), &gatedArguments);

    return result;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::externalMethodGated
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::externalMethodGated(ExternalMethodGatedArguments * arguments)
{
    IOReturn result = kIOReturnOffline;

    require(!isInactive(), exit);

    result = dispatchExternalMethod(arguments->selector, arguments->arguments, _methods, sizeof(_methods) / sizeof(_methods[0]), this, NULL);

exit:
    return result;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::createMemoryDescriptorFromInputArguments
//----------------------------------------------------------------------------------------------------
IOMemoryDescriptor * IOHIDResourceDeviceUserClient::createMemoryDescriptorFromInputArguments(
                                            IOExternalMethodArguments * arguments)
{
    IOBufferMemoryDescriptor * report = NULL;
    
    if ( arguments->structureInputDescriptor ) {
        IOByteCount length = arguments->structureInputDescriptor->getLength();
        
        report = IOBufferMemoryDescriptor::withCapacity(length, kIODirectionOut);
        
        if(report) {
            arguments->structureInputDescriptor->prepare();
            arguments->structureInputDescriptor->readBytes(0, report->getBytesNoCopy(), length);
            arguments->structureInputDescriptor->complete();
        }
    } else {
        report = IOBufferMemoryDescriptor::withBytes(arguments->structureInput, arguments->structureInputSize, kIODirectionOut);
    }
    
    return report;
}


//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::getService
//----------------------------------------------------------------------------------------------------
IOService * IOHIDResourceDeviceUserClient::getService(void)
{
    return _device ? _device : NULL;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::clientClose
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::clientClose(void)
{
    terminate();
    return kIOReturnSuccess;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::createAndStartDevice
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::createAndStartDevice()
{
    IOReturn    result;
    OSNumber *  number = NULL;
    
    require_action(_device==NULL, exit, result=kIOReturnInternalError);
    
    number = OSDynamicCast(OSNumber, _properties->getObject(kIOHIDRequestTimeoutKey));
    _maxClientTimeoutUS = number && number->unsigned32BitValue() >= kIOHIDDeviceMinAsyncRequestTimeout * 1000 && number->unsigned32BitValue() <= kIOHIDDeviceMaxAsyncRequestTimeout * 1000 ? number->unsigned32BitValue() : _maxClientTimeoutUS;

    // If after all the unwrapping we have a dictionary, let's create the device
    _device = IOHIDUserDevice::withProperties(_properties);
    require_action(_device, exit, result=kIOReturnNoResources);
    
    require_action(_device->attach(this), exit, result=kIOReturnInternalError);
    
    require_action(_device->start(this), exit, _device->detach(this); result=kIOReturnInternalError);
    
    result = kIOReturnSuccess;
    
exit:
    if ( result!=kIOReturnSuccess ) {
        HIDLogError("result=0x%08x", result);
        OSSafeReleaseNULL(_device);
    }

    return result;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::createDevice
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::createDevice(IOExternalMethodArguments * arguments)
{
    IOMemoryDescriptor *    propertiesDesc      = NULL;
    void *                  propertiesData      = NULL;
    IOByteCount             propertiesLength    = 0;
    OSObject *              object              = NULL;
    IOReturn                result;
    // Report descriptor is static and thus can only be set on creation
    require_action(_device==NULL, exit, result=kIOReturnInternalError);
    
    // Let's deal with our device properties from data
    propertiesDesc = createMemoryDescriptorFromInputArguments(arguments);
    require_action(propertiesDesc, exit, result=kIOReturnNoMemory);
    
    propertiesLength = propertiesDesc->getLength();
    require_action(propertiesLength, exit, result=kIOReturnNoResources);
    
    propertiesData = IOMallocData(propertiesLength);
    require_action(propertiesData, exit, result=kIOReturnNoMemory);
    
    result = propertiesDesc->prepare();
    require_noerr(result, exit);
    propertiesDesc->readBytes(0, propertiesData, propertiesLength);
    propertiesDesc->complete();
    
    require_action(strnlen((const char *) propertiesData, propertiesLength) < propertiesLength, exit, result=kIOReturnInternalError);

    object = OSUnserializeXML((const char *)propertiesData, propertiesLength);
    require_action(object, exit, result=kIOReturnInternalError ; HIDServiceLogError("OSUnserializeXML:NULL"););
    
    OSSafeReleaseNULL(_properties);
    
    _properties = OSDynamicCast(OSDictionary, object);
    require_action(_properties, exit, result=kIOReturnInternalError ; HIDServiceLogError("properties is not OSDictionary"));
    _properties->retain();
    
    if (IsIOHIDRestrictedIOKitPropertyDictionary(_properties)) {
        result = kIOReturnBadArgument;
        goto exit;
    }
    
    // add privilege entitlements
    _properties->setObject(kIOHIDDevicePrivilegedKey, _privileged ? kOSBooleanTrue : kOSBooleanFalse);
    
    if (arguments->scalarInput[0] & kIOHIDUserDeviceCreateOptionStartWhenScheduled) {
        _properties->setObject(kIOHIDRegisterServiceKey, kOSBooleanFalse);
    }

    _asyncSupport = true;
#if DEVELOPMENT || KASAN
    _asyncSupport = _properties->getObject("AsyncSupport") != kOSBooleanFalse;
#endif
    
    result = createAndStartDevice();
    
    require_noerr(result, exit);

exit:
    
    if ( object )
        object->release();
    
    if ( propertiesData && propertiesLength )
        IOFreeData(propertiesData, propertiesLength);

    if ( propertiesDesc )
        propertiesDesc->release();
    
    return result;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::_createDevice
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::_createDevice(
                                        IOHIDResourceDeviceUserClient * target, 
                                        void *                          reference __unused,
                                        IOExternalMethodArguments *     arguments)
{
    return target->createDevice(arguments);
}

struct IOHIDResourceDeviceUserClientAsyncParamBlock {
    OSAsyncReference64                  fAsyncRef;
    uint32_t                            fAsyncCount;
};

void IOHIDResourceDeviceUserClient::ReportComplete(void *param, IOReturn res, UInt32 remaining __unused)
{
    IOHIDResourceDeviceUserClientAsyncParamBlock *pb = (IOHIDResourceDeviceUserClientAsyncParamBlock *)param;
    
    io_user_reference_t args[1];
    args[0] = 0;
    
    sendAsyncResult64(pb->fAsyncRef, res, args, 0);
    IOFreeType(pb, IOHIDResourceDeviceUserClientAsyncParamBlock);
    
    release();
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::handleReport
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::handleReport(IOExternalMethodArguments * arguments)
{
    AbsoluteTime timestamp;
    
    if (_device == NULL) {
        HIDLogError("failed : device is NULL");
        return kIOReturnNotOpen;
    }

    IOReturn                ret;
    IOMemoryDescriptor *    report;
    
    report = createMemoryDescriptorFromInputArguments(arguments);
    if ( !report ) {
        HIDLogError("failed : could not create descriptor");
        return kIOReturnNoMemory;
    }
    
    if ( arguments->scalarInput[0] )
        AbsoluteTime_to_scalar(&timestamp) = arguments->scalarInput[0];
    else
        clock_get_uptime( &timestamp );
    
    _handleReportCount++;
    
    if ( !arguments->asyncWakePort ) {
        ret = _device->handleReportWithTime(timestamp, report);
        report->release();
    } else {
        IOHIDCompletion tap;
        
        IOHIDResourceDeviceUserClientAsyncParamBlock *pb = IOMallocType(IOHIDResourceDeviceUserClientAsyncParamBlock);
        
        if (!pb) {
            report->release();
            return kIOReturnNoMemory;   // need to release report
        }
        
        retain();
        
        bcopy(arguments->asyncReference, pb->fAsyncRef, sizeof(OSAsyncReference64));
        pb->fAsyncCount = arguments->asyncReferenceCount;
        
        tap.target = this;
        tap.action = OSMemberFunctionCast(IOHIDCompletionAction, this, &IOHIDResourceDeviceUserClient::ReportComplete);
        tap.parameter = pb;

        ret = _device->handleReportWithTimeAsync(timestamp, report, kIOHIDReportTypeInput, 0, 0, &tap);
        
        report->release();
        
        if (ret != kIOReturnSuccess) {
            IOFreeType(pb, IOHIDResourceDeviceUserClientAsyncParamBlock);
            release();
        }
    }
    
    return ret;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::_handleReport
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::_handleReport(IOHIDResourceDeviceUserClient    *target, 
                                             void                        *reference __unused,
                                             IOExternalMethodArguments    *arguments)
{
    return target->handleReport(arguments);
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::getReport
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::getReport(IOMemoryDescriptor *report, IOHIDReportType reportType, IOOptionBits options)
{
    return getReport(report, reportType, options, 0);
}

IOReturn IOHIDResourceDeviceUserClient::getReport(IOMemoryDescriptor *report, IOHIDReportType reportType, IOOptionBits options, UInt32 completionTimeout, IOHIDCompletion * completion)
{
    ReportGatedArguments    arguments   = {report, reportType, options, completionTimeout, completion};
    IOReturn                result;
    
    require_action(!isInactive(), exit, result=kIOReturnOffline);
    require_action(!completion || _asyncSupport, exit, result = kIOReturnUnsupported);

    result = _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDResourceDeviceUserClient::getReportGated), &arguments);
exit:
    return result;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::getReport
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::getReportGated(ReportGatedArguments * arguments)
{
    IOHIDResourceDataQueueHeader    header;
    __ReportResult                  result;
    IOReturn                        ret = kIOReturnSuccess;
    OSValueObject<__ReportResult> * retData = NULL;
    bool                            async = arguments->completion && arguments->completion->action;
    
    require_action(!isInactive() && !_suspended, exit, ret=kIOReturnOffline);

    arguments->completionTimeout = async && arguments->completionTimeout >= kIOHIDDeviceMinAsyncRequestTimeout && arguments->completionTimeout <= kIOHIDDeviceMaxAsyncRequestTimeout ? arguments->completionTimeout * 1000 : _maxClientTimeoutUS;
    clock_interval_to_deadline(arguments->completionTimeout, kMicrosecondScale, &result.deadline);

    result.ret        = kIOReturnError;
    result.descriptor = arguments->report;
    result.token      = _tokenIndex++;
    result.completion = async ? *arguments->completion : (IOHIDCompletion){NULL, NULL, NULL};

    retData = OSValueObject<__ReportResult>::withValue(result);

    require_action(retData, exit, ret=kIOReturnNoMemory);
    
    header.direction   = kIOHIDResourceReportDirectionIn;
    header.type        = arguments->reportType;
    header.reportID    = arguments->options & 0xff;
    header.length      = (uint32_t)arguments->report->getLength();
    header.token       = result.token;
    header.reportFlags = 0;

    _pending->setObject(retData);
    if (result.descriptor) {
        result.descriptor->retain();
    }
    
    require_action(_queue, exit, ret = kIOReturnNotReady); // client has not mapped memory
    require_action(_port, exit, ret = kIOReturnOffline); // client has not registered a port
    
    _getReportCount++;
    require_action(_queue->enqueueReport(&header), exit, {
        HIDLogInfo("0x%llx: IOHIDUserDevice getReport enqueue failed.\n", getRegistryEntryID());
        ret = kIOReturnNoMemory;
        _getReportDroppedCount++;
        _enqueueFailCount++;
        _queue->sendDataAvailableNotification();
        IOHID_DEBUG(kIOHIDDebugCode_HIDUserDeviceEnqueueFail, mach_continuous_time(), 0, 0, 0);
    });

    if (async) {
        setNextAsyncTimeout();
        _outstandingAsyncCount++;
    } else {
        const __ReportResult& resultRef = retData->getRef();
        switch (_commandGate->commandSleep(retData, resultRef.deadline, THREAD_ABORTSAFE)) {
            case THREAD_AWAKENED:
                ret = resultRef.ret;
                _getReportCompletedCount++;
                break;
            case THREAD_TIMED_OUT:
                HIDLogError("0x%llx: IOHIDUserDevice getReport thread timed out.\n", getRegistryEntryID());
                ret = kIOReturnTimeout;
                _getReportTimeoutCount++;
                break;
            default:
                ret = kIOReturnError;
                break;
        }
    }
    
exit:
    if (retData && (!async || ret)) {
        _pending->removeObject(retData);
        OSSafeReleaseNULL(result.descriptor);
        if (!async) {
            _commandGate->commandWakeup(&_pending);
        }
    }
    OSSafeReleaseNULL(retData);
    return ret;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::setReport
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::setReport(IOMemoryDescriptor *report, IOHIDReportType reportType, IOOptionBits options)
{
    return setReport(report, reportType, options, 0);
}

IOReturn IOHIDResourceDeviceUserClient::setReport(IOMemoryDescriptor *report, IOHIDReportType reportType, IOOptionBits options, UInt32 completionTimeout, IOHIDCompletion * completion)
{
    ReportGatedArguments    arguments={report, reportType, options, completionTimeout, completion};
    IOReturn                result;
    
    require_action(!isInactive(), exit, result=kIOReturnOffline);
    require_action(!completion || _asyncSupport, exit, result = kIOReturnUnsupported);
    
    result = _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDResourceDeviceUserClient::setReportGated), &arguments);
exit:
    return result;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::setReportGated
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::setReportGated(ReportGatedArguments * arguments)
{
    IOHIDResourceDataQueueHeader    header;
    __ReportResult                  result;
    IOReturn                        ret = kIOReturnSuccess;
    OSValueObject<__ReportResult> * retData = NULL;
    IOMemoryDescriptor *            report = arguments->report;
    bool                            async = arguments->completion && arguments->completion->action;

    require_action(!isInactive() && !_suspended, exit, ret = kIOReturnOffline);
    
    arguments->completionTimeout = async && arguments->completionTimeout >= kIOHIDDeviceMinAsyncRequestTimeout && arguments->completionTimeout <= kIOHIDDeviceMaxAsyncRequestTimeout ? arguments->completionTimeout * 1000 : _maxClientTimeoutUS;
    clock_interval_to_deadline(arguments->completionTimeout, kMicrosecondScale, &result.deadline);

    result.ret        = kIOReturnError;
    result.descriptor = NULL;
    result.token      = _tokenIndex++;
    result.completion = async ? *arguments->completion : (IOHIDCompletion){NULL, NULL, NULL};

    retData = OSValueObject<__ReportResult>::withValue(result);

    require_action(retData, exit, ret = kIOReturnNoMemory);
    
    header.direction   = kIOHIDResourceReportDirectionOut;
    header.type        = arguments->reportType;
    header.reportID    = arguments->options & 0xff;
    header.length      = (uint32_t)report->getLength();
    header.token       = result.token;
    header.reportFlags = 0;

    _pending->setObject(retData);
    
    require_action(_queue, exit, ret = kIOReturnNotReady); // client has not mapped memory
    require_action(_port, exit, ret = kIOReturnOffline); // client has not registered a port
    
    _setReportCount++;

    if (report->getLength() > kMaxQueueReportSize) {
        bool status;
        IOMemoryMap *overSizedReportMap = report->createMappingInTask(_owningTask, 0, kIOMapAnywhere | kIOMapReadOnly);
        if (!overSizedReportMap) {
            HIDLogError("Could not create large report mapping.");
            ret = kIOReturnNoMemory;
            goto exit;
        }
        status = _overSizedReports->setObject(overSizedReportMap);
        overSizedReportMap->release();
        if (!status) {
            HIDLogError("Could not save large report mapping.");
            ret = kIOReturnNoMemory;
            goto exit;
        }
        IOHIDResourceOOBReportInfo reportInfo = {(user_addr_t)overSizedReportMap->getAddress(), (uint32_t)overSizedReportMap->getLength()};

        // Assigning a buffer here is overkill, but we want to keep the same interface for the enqueue reports.
        report = IOBufferMemoryDescriptor::withBytes(&reportInfo, sizeof(IOHIDResourceOOBReportInfo), kIODirectionInOut);
        header.reportFlags |= kIOHIDResourceOOBReport;
        header.length = (uint32_t)report->getLength();
        HIDLogDebug("Created OOB report data: %#llx(%d)", (uint64_t)reportInfo.token, (uint32_t)reportInfo.length);
    }

    require_action(_queue->enqueueReport(&header, report), exit, {
        HIDLogInfo("0x%llx: IOHIDUserDevice setReport enqueue failed.\n", getRegistryEntryID());
        ret = kIOReturnNoMemory;
        _setReportDroppedCount++;
        _enqueueFailCount++;
        _queue->sendDataAvailableNotification();
        IOHID_DEBUG(kIOHIDDebugCode_HIDUserDeviceEnqueueFail, mach_continuous_time(), 0, 0, 0);
    });

    if (async) {
        setNextAsyncTimeout();
        _outstandingAsyncCount++;
    } else {
        const __ReportResult& resultRef = retData->getRef();
        switch (_commandGate->commandSleep(retData, resultRef.deadline, THREAD_ABORTSAFE)) {
            case THREAD_AWAKENED:
                ret = resultRef.ret;
                _setReportCompletedCount++;
                break;
            case THREAD_TIMED_OUT:
                HIDLogError("0x%llx: IOHIDUserDevice setReport thread timed out.\n", getRegistryEntryID());
                ret = kIOReturnTimeout;
                _setReportTimeoutCount++;
                break;
            default:
                ret = kIOReturnError;
                break;
        }
    }

exit:
    if (retData && (!async || ret)) {
        _pending->removeObject(retData);
        if (!async) {
            _commandGate->commandWakeup(&_pending);
        }
    }
    OSSafeReleaseNULL(retData);

    if (header.reportFlags & kIOHIDResourceOOBReport) {
        report->release();
    }
    
    return ret;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::postReportResult
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::postReportResult(IOExternalMethodArguments * arguments)
{
    __block OSObject * target = NULL;
    __block IOReturn result = kIOReturnNotFound;
    __block IOByteCount descriptorLength = 0;
    __block __ReportResult * pResult = NULL;
    
    u_int64_t token = (u_int64_t)arguments->scalarInput[kIOHIDResourceUserClientResponseIndexToken];
    
    _pending->iterateObjects(^bool(OSObject * object) {
        IOMemoryDescriptor * reportDescriptor = NULL;
        IOMemoryMap * reportMap = NULL;

        pResult = (__ReportResult*)((OSValueObject<__ReportResult>*)object)->getBytesNoCopy();

        if (pResult->token != token) {
            pResult = NULL;
            return false;
        }
        target = object;

        if ( pResult->descriptor && arguments->structureInputDescriptor ){
            // response > 4K, we got a memory descriptor from the response.

            // Check that the HIDUserDevice didn't change the report size over the allocated size.
            descriptorLength = pResult->descriptor->getLength();

            if (descriptorLength < arguments->structureInputDescriptor->getLength()) {
                pResult->ret = kIOReturnOverrun;
                result = kIOReturnOverrun;
                HIDLogError("Invalid report length expected : %d got : %d",(int)descriptorLength, (int)arguments->structureInputSize);
                return true;
            }
            // Map result into kernel task
            reportDescriptor = arguments->structureInputDescriptor;
            reportMap = reportDescriptor->map(kIOMapReadOnly);
            if (!reportMap) {
                pResult->ret = kIOReturnNoMemory;
                result = kIOReturnNoMemory;
                HIDLogError("Failed to map report, could not copy results of get report.");
                return true;
            }

            pResult->descriptor->writeBytes(0, (void*)reportMap->getVirtualAddress(), reportDescriptor->getLength());

            OSSafeReleaseNULL(reportMap);
            // 12978252:  If we get an IOBMD passed in, set the length to be the # of bytes that were transferred
            IOBufferMemoryDescriptor * buffer = OSDynamicCast(IOBufferMemoryDescriptor, pResult->descriptor);
            if (buffer)
                buffer->setLength(MIN((vm_size_t)reportDescriptor->getLength(), buffer->getCapacity()));
        } else if ( pResult->descriptor && arguments->structureInput ) {
            
            // HID User device API with length allows caller to modify return length
            // which can be greater than orginal length of descriptor, we should check
            // that here
            descriptorLength = pResult->descriptor->getLength();

            if (descriptorLength < arguments->structureInputSize) {
                pResult->ret = kIOReturnOverrun;
                result = kIOReturnOverrun;
                HIDLogError("Invalid report length expected : %d got : %d",(int)descriptorLength, (int)arguments->structureInputSize);
                return true;
            }
        
            pResult->descriptor->writeBytes(0, arguments->structureInput, arguments->structureInputSize);
            
            // 12978252:  If we get an IOBMD passed in, set the length to be the # of bytes that were transferred
            IOBufferMemoryDescriptor * buffer = OSDynamicCast(IOBufferMemoryDescriptor, pResult->descriptor);
            if (buffer)
                buffer->setLength(MIN((vm_size_t)arguments->structureInputSize, buffer->getCapacity()));
        }
        
        pResult->ret = (IOReturn)arguments->scalarInput[kIOHIDResourceUserClientResponseIndexResult];
        
        result = kIOReturnSuccess;
        return true;
    });

    if (pResult) {
        if (pResult->completion.action) {
            pResult->completion.action(pResult->completion.target, pResult->completion.parameter, pResult->ret, (uint32_t)(descriptorLength - (pResult->descriptor ? pResult->descriptor->getLength() : 0)));
            OSSafeReleaseNULL(pResult->descriptor);
            _pending->removeObject(target);
            setNextAsyncTimeout();
            _outstandingAsyncCount--;
        } else {
            _commandGate->commandWakeup(target);
        }
    }

    return result;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::_postReportResult
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::_postReportResult(IOHIDResourceDeviceUserClient    *target, 
                                             void                        *reference __unused,
                                             IOExternalMethodArguments    *arguments)
{
    return target->postReportResult(arguments);
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::cleanupAsyncReports
//----------------------------------------------------------------------------------------------------
void IOHIDResourceDeviceUserClient::setNextAsyncTimeout()
{
    __block AbsoluteTime nextDeadline = UINT64_MAX;
    OSSet              * set          = OSSet::withSet(_pending);
    OSSet              * timedOut     = OSSet::withCapacity(set->getCount());
    require_action(set, exit, HIDLogError("Failed to allocate pending set copy"));
    require_action(timedOut, exit, HIDLogError("Failed to allocate set for requests that timed out"));

    _asyncReportTimer->cancelTimeout();

    // TODO: Decide if there is a better way to iterate pending. Iterators fail if the collection is modified, and sets do not use indices. Switch to array?
    set->iterateObjects(^bool(OSObject * object) {
        __ReportResult * pResult = (__ReportResult*)((OSValueObject<__ReportResult>*)object)->getBytesNoCopy();

        if (!pResult->completion.action) {
            return false;
        }

        if (pResult->deadline > mach_absolute_time()) {
            if (pResult->deadline < nextDeadline) {
                nextDeadline = pResult->deadline;
            }
            return false;
        }

        timedOut->setObject(object);
        _pending->removeObject(object);
        _outstandingAsyncCount--;
        return false;
    });

    if (nextDeadline < UINT64_MAX) {
        _asyncReportTimer->wakeAtTime(nextDeadline);
    }

    timedOut->iterateObjects(^bool(OSObject *object) {
        __ReportResult * pResult = (__ReportResult*)((OSValueObject<__ReportResult>*)object)->getBytesNoCopy();

        pResult->completion.action(pResult->completion.target, pResult->completion.parameter, kIOReturnTimeout, pResult->descriptor ? (uint32_t)pResult->descriptor->getLength() : 0);
        OSSafeReleaseNULL(pResult->descriptor);
        return false;
    });

exit:
    OSSafeReleaseNULL(set);
    OSSafeReleaseNULL(timedOut);
    return;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::cleanupPendingReports
//----------------------------------------------------------------------------------------------------
void IOHIDResourceDeviceUserClient::cleanupPendingReports()
{
    OSSet * set = OSSet::withSet(_pending);
    require_action(set, exit, HIDLogError("Failed to allocate pending set copy"));

    _asyncReportTimer->cancelTimeout();
    
    set->iterateObjects(^bool(OSObject * object) {
        __ReportResult * pResult = (__ReportResult*)((OSValueObject<__ReportResult>*)object)->getBytesNoCopy();

        pResult->ret = kIOReturnAborted;

        if (pResult->completion.action) {
            pResult->completion.action(pResult->completion.target, pResult->completion.parameter, pResult->ret, pResult->descriptor ? (uint32_t)pResult->descriptor->getLength() : 0);
            OSSafeReleaseNULL(pResult->descriptor);
            _pending->removeObject(object);
            _outstandingAsyncCount--;
        } else {
            _commandGate->commandWakeup(object);
        }
        return false;
    });
    
    while (_pending->getCount()) {
        _commandGate->commandSleep(&_pending);
    }
exit:
    OSSafeReleaseNULL(set);
    return;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::terminateDevice
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::terminateDevice()
{
    if (_device) {
        _device->terminate();
    }
    OSSafeReleaseNULL(_device);

    return kIOReturnSuccess;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::_terminateDevice
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::_terminateDevice(
                                        IOHIDResourceDeviceUserClient    *target, 
                                        void                            *reference __unused, 
                                        IOExternalMethodArguments       *arguments __unused)
{
    return target->terminateDevice();
}

//------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::_registerService
//------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::_registerService(
                                IOHIDResourceDeviceUserClient *target,
                                void *reference __unused,
                                IOExternalMethodArguments *arguments __unused)
{
    if (target->_device) {
        target->_device->registerService();
        return kIOReturnSuccess;
    } else {
        return kIOReturnOffline;
    }
}

IOReturn IOHIDResourceDeviceUserClient::releaseTokenGated(mach_vm_address_t token)
{
    for (int index = 0; index < _overSizedReports->getCount(); ++index) {
        IOMemoryMap *reportMap = OSDynamicCast(IOMemoryMap, _overSizedReports->getObject(index));
        if (!reportMap) {
            continue;
        }
        if (token == reportMap->getAddress()) {
            _overSizedReports->removeObject(index);
            break;
        }
    }
    return kIOReturnSuccess;
}

IOReturn IOHIDResourceDeviceUserClient::releaseToken(mach_vm_address_t token)
{
    return _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDResourceDeviceUserClient::releaseTokenGated), (void*)token);
}


//------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::_getReportForToken
//------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::_releaseToken(
                                IOHIDResourceDeviceUserClient *target,
                                void *reference __unused,
                                IOExternalMethodArguments *arguments)
{
    return target->releaseToken((mach_vm_address_t)arguments->scalarInput[0]);
}


IOReturn IOHIDResourceDeviceUserClient::setProperties(OSObject *properties)
{
    IOReturn ret;
    
    require_action(!isInactive(), exit, ret = kIOReturnOffline);
    
    ret = _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDResourceDeviceUserClient::setPropertiesGated), properties);
    
exit:
    return ret;
}

IOReturn IOHIDResourceDeviceUserClient::setPropertiesGated(OSObject *properties)
{
    IOReturn ret = kIOReturnSuccess;
    OSDictionary *propertyDict = NULL;
    OSBoolean *suspend = NULL;

    require((propertyDict = OSDynamicCast(OSDictionary, properties)), exit);

    if ((suspend = OSDynamicCast(OSBoolean, propertyDict->getObject(kIOHIDUserDeviceSuspendKey)))) {
        _suspended = suspend->getValue();
    } else {
        require_action(_device, exit, ret = kIOReturnOffline);
        
        propertyDict->iterateObjects(^bool(const OSSymbol * key, OSObject * object) {
            if (IsIOHIDRestrictedIOKitProperty(key)) {
                return false;
            }
            _device->setProperty(key, object);
            return false;
        });
    }

exit:
    return ret;
}

#define SET_DICT_NUM(dict, key, val) do { \
    if (val) { \
        OSNumber *num = OSNumber::withNumber(val, 64); \
        if (num) { \
            dict->setObject(key, num); \
            num->release(); \
        } \
    } \
} while (0);

bool IOHIDResourceDeviceUserClient::serializeDebugState(void *ref __unused, OSSerialize *serializer)
{
    bool result = false;
    OSDictionary *dict = OSDictionary::withCapacity(4);
    
    require(dict, exit);
    
    if (_queue) {
        dict->setObject("ReportQueue", _queue);
    }
    
    SET_DICT_NUM(dict, "SetReportCount", _setReportCount);
    SET_DICT_NUM(dict, "SetReportCompletedCount", _setReportCompletedCount);
    SET_DICT_NUM(dict, "SetReportDroppedCount", _setReportDroppedCount);
    SET_DICT_NUM(dict, "SetReportTimeoutCount", _setReportTimeoutCount);
    SET_DICT_NUM(dict, "GetReportCount", _getReportCount);
    SET_DICT_NUM(dict, "GetReportCompletedCount", _getReportCompletedCount);
    SET_DICT_NUM(dict, "GetReportDroppedCount", _getReportDroppedCount);
    SET_DICT_NUM(dict, "GetReportTimeoutCount", _getReportTimeoutCount);
    SET_DICT_NUM(dict, "EnqueueFailCount", _enqueueFailCount);
    SET_DICT_NUM(dict, "HandleReportCount", _handleReportCount);
    SET_DICT_NUM(dict, "OutstandingAsyncCount", _outstandingAsyncCount);
    SET_DICT_NUM(dict, "MaxClientTimeoutUS", _maxClientTimeoutUS);
    
    result = dict->serialize(serializer);
    OSSafeReleaseNULL(dict);
    
exit:
    return result;
}

//====================================================================================================
// IOHIDResourceQueue
//====================================================================================================
#include <IOKit/IODataQueueShared.h>

#undef super
#define super IOSharedDataQueue

OSDefineMetaClassAndStructors( IOHIDResourceQueue, IOSharedDataQueue )

IOHIDResourceQueue *IOHIDResourceQueue::withCapacity(UInt32 capacity)
{
    IOHIDResourceQueue *dataQueue = new IOHIDResourceQueue;

    if (dataQueue) {
        if (!dataQueue->initWithCapacity(capacity)) {
            dataQueue->release();
            dataQueue = 0;
        }
    }

    return dataQueue;
}

IOHIDResourceQueue *IOHIDResourceQueue::withCapacity(IOService *owner, UInt32 size)
{
    IOHIDResourceQueue *dataQueue = IOHIDResourceQueue::withCapacity(size);
    
    if (dataQueue) {
        dataQueue->_owner = owner;
    }
    
    return dataQueue;
}

Boolean IOHIDResourceQueue::initWithCapacity(UInt32 size)
{
    bool ret = super::initWithCapacity(size);
    if (!ret) {
        return ret;
    }
    return true;
}

void IOHIDResourceQueue::free()
{
    if ( _descriptor )
    {
        _descriptor->release();
        _descriptor = 0;
    }

    super::free();
}

#define ALIGNED_DATA_SIZE(data_size,align_size) ((((data_size - 1) / align_size) + 1) * align_size)

Boolean IOHIDResourceQueue::enqueueReport(IOHIDResourceDataQueueHeader * header, IOMemoryDescriptor * report)
{
    UInt32              headerSize  = sizeof(IOHIDResourceDataQueueHeader);
    UInt32              reportSize  = report ? (UInt32)report->getLength() : 0;
    UInt32              dataSize;
    UInt32              head;
    UInt32              tail;
    UInt32              newTail;
    UInt32              entrySize;
    IODataQueueEntry *  entry;
    UInt32              totalSize;
    
    // check overflow of headerSize + reportSize
    if (os_add_overflow(headerSize, reportSize, &totalSize)) {
        return false;
    }

    dataSize = ALIGNED_DATA_SIZE(totalSize, sizeof(uint32_t));
    
    // check overflow after alignment
    if (dataSize < totalSize) {
        return false;
    }
    
    // check overflow of entrySize
    if (os_add_overflow(dataSize, DATA_QUEUE_ENTRY_HEADER_SIZE, &entrySize)) {
        return false;
    }

    // Force a single read of head and tail
    tail = __c11_atomic_load((_Atomic UInt32 *)&dataQueue->tail, __ATOMIC_RELAXED);
    head = __c11_atomic_load((_Atomic UInt32 *)&dataQueue->head, __ATOMIC_ACQUIRE);

    if ( tail > getQueueSize() || head > getQueueSize() || dataSize < headerSize || entrySize < dataSize)
    {
        return false;
    }

    if ( tail >= head )
    {
        // Is there enough room at the end for the entry?
        if ((getQueueSize() - tail) >= entrySize )
        {
            entry = (IODataQueueEntry *)((UInt8 *)dataQueue->queue + tail);

            entry->size = dataSize;
            
            bcopy(header, &entry->data, headerSize);
            

            if ( report ) {
                report->readBytes(0, ((UInt8*)&entry->data) + headerSize, reportSize);
            }

            // The tail can be out of bound when the size of the new entry
            // exactly matches the available space at the end of the queue.
            // The tail can range from 0 to getQueueSize() inclusive.

            newTail = tail + entrySize;
        }
        else if ( head > entrySize )     // Is there enough room at the beginning?
        {
            // Wrap around to the beginning, but do not allow the tail to catch
            // up to the head.

            dataQueue->queue->size = dataSize;

            // We need to make sure that there is enough room to set the size before
            // doing this. The user client checks for this and will look for the size
            // at the beginning if there isn't room for it at the end.

            if ( ( getQueueSize() - tail ) >= DATA_QUEUE_ENTRY_HEADER_SIZE )
            {
                ((IODataQueueEntry *)((UInt8 *)dataQueue->queue + tail))->size = dataSize;
            }

            bcopy(header, &dataQueue->queue->data, sizeof(IOHIDResourceDataQueueHeader));

            if ( report ) {
                report->readBytes(0, ((UInt8*)&dataQueue->queue->data) + headerSize, reportSize);
            }
   
            newTail = entrySize;
        }
        else
        {
            return false;    // queue is full
        }
    }
    else
    {
        // Do not allow the tail to catch up to the head when the queue is full.
        // That's why the comparison uses a '>' rather than '>='.

        if ( (head - tail) > entrySize )
        {
            entry = (IODataQueueEntry *)((UInt8 *)dataQueue->queue + tail);

            entry->size = dataSize;

            bcopy(header, &entry->data, sizeof(IOHIDResourceDataQueueHeader));
            if ( report ) {
                report->readBytes(0, ((UInt8*)&entry->data) + headerSize, reportSize);
            }

            newTail = tail + entrySize;
        }
        else
        {
            return false;    // queue is full
        }
    }

    // Publish the data we just enqueued
    _enqueueTS = mach_continuous_time();
    __c11_atomic_store((_Atomic UInt32 *)&dataQueue->tail, newTail, __ATOMIC_RELEASE);

    if (tail != head) {
        // From <rdar://problem/43093190> IOSharedDataQueue stalls
        //
        // The memory barrier below pairs with the one in ::dequeue
        // so that either our store to the tail cannot be missed by
        // the next dequeue attempt, or we will observe the dequeuer
        // making the queue empty.
        //
        // Of course, if we already think the queue is empty,
        // there's no point paying this extra cost.
        //
        __c11_atomic_thread_fence(__ATOMIC_SEQ_CST);
        head = __c11_atomic_load((_Atomic UInt32 *)&dataQueue->head, __ATOMIC_RELAXED);
    }

    // Send notification (via mach message) that data is available if either the
    // queue was empty prior to enqueue() or queue was emptied during enqueue()
    if (head == tail) {
        sendDataAvailableNotification();
    }

    return true;
}

void IOHIDResourceQueue::setNotificationPort(mach_port_t port) 
{
    super::setNotificationPort(port);

    if (dataQueue->head != dataQueue->tail)
        sendDataAvailableNotification();
}

IOMemoryDescriptor * IOHIDResourceQueue::getMemoryDescriptor()
{
    if (!_descriptor)
        _descriptor = super::getMemoryDescriptor();

    return _descriptor;
}

bool IOHIDResourceQueue::serialize(OSSerialize * serializer) const
{
    bool ret = false;
    
    if (serializer->previouslySerialized(this)) {
        return true;
    }
    
    OSDictionary *dict = OSDictionary::withCapacity(2);
    if (dict) {
        SET_DICT_NUM(dict, "head", dataQueue->head);
        SET_DICT_NUM(dict, "tail", dataQueue->tail);
        SET_DICT_NUM(dict, "QueueSize", _reserved->queueSize);
        SET_DICT_NUM(dict, "EnqueueTimestamp", _enqueueTS);
        
        ret = dict->serialize(serializer);
        dict->release();
    }
    
    return ret;
}
