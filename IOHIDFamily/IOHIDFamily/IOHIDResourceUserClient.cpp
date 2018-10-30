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

#define kIOHIDManagerUserAccessUserDeviceEntitlement "com.apple.hid.manager.user-access-device"

#ifdef enqueue
    #undef enqueue
#endif

#include "IOHIDResourceUserClient.h"
#include <libkern/OSAtomic.h>
#include "IOHIDDebug.h"
#include "IOHIDFamilyTrace.h"

#define kHIDClientTimeoutUS     1000000ULL

#define kHIDQueueSize           16384

#define super IOUserClient


OSDefineMetaClassAndStructors( IOHIDResourceDeviceUserClient, IOUserClient )


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDResourceDeviceUserClient::_methods
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
const IOExternalMethodDispatch IOHIDResourceDeviceUserClient::_methods[kIOHIDResourceDeviceUserClientMethodCount] = {
    {   // kIOHIDResourceDeviceUserClientMethodCreate
        (IOExternalMethodAction) &IOHIDResourceDeviceUserClient::_createDevice,
        1, -1, /* 1 struct input : the report descriptor */
        0, 0
    },
    {   // kIOHIDResourceDeviceUserClientMethodTerminate
        (IOExternalMethodAction) &IOHIDResourceDeviceUserClient::_terminateDevice,
        0, 0,
        0, 0
    },
    {   // kIOHIDResourceDeviceUserClientMethodHandleReport
        (IOExternalMethodAction) &IOHIDResourceDeviceUserClient::_handleReport,
        1, -1, /* 1 struct input : the buffer */
        0, 0
    },
    {   // kIOHIDResourceDeviceUserClientMethodPostReportResult
        (IOExternalMethodAction) &IOHIDResourceDeviceUserClient::_postReportResult,
        kIOHIDResourceUserClientResponseIndexCount, -1, /* 1 scalar input: the result, 1 struct input : the buffer */
        0, 0
    }
};



//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::initWithTask
//----------------------------------------------------------------------------------------------------
bool IOHIDResourceDeviceUserClient::initWithTask(task_t owningTask, void * security_id, UInt32 type)
{
    bool result = false;
    
    OSObject* entitlement = copyClientEntitlement(owningTask, kIOHIDManagerUserAccessUserDeviceEntitlement);
    if (entitlement) {
      result = (entitlement == kOSBooleanTrue);
      entitlement->release();
    }
    if (!result) {
      proc_t      process;
      process = (proc_t)get_bsdtask_info(owningTask);
      char name[255];
      bzero(name, sizeof(name));
      proc_name(proc_pid(process), name, sizeof(name));
      HIDLogError("%s is not entitled", name);
      goto exit;
    }
 //   require_noerr_action(clientHasPrivilege(owningTask, kIOClientPrivilegeAdministrator), exit, result=false);
    result = super::initWithTask(owningTask, security_id, type);
    require_action(result, exit, HIDLogError("failed"));
    
    _pending            = OSSet::withCapacity(4);
    _maxClientTimeoutUS = kHIDClientTimeoutUS;

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
    
    _createDeviceTimer = IOTimerEventSource::timerEventSource(this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &IOHIDResourceDeviceUserClient::createAndStartDeviceAsyncCallback));
    require_action(_createDeviceTimer, exit, result=false);
    require_noerr_action(workLoop->addEventSource(_createDeviceTimer), exit, result=false);
    
    _commandGate = IOCommandGate::commandGate(this);
    require_action(_commandGate, exit, result=false);
    require_noerr_action(workLoop->addEventSource(_commandGate), exit, result=false);
    
    debugSerializer = OSSerializer::forTarget(this, OSMemberFunctionCast(OSSerializerCallback, this, &IOHIDResourceDeviceUserClient::serializeDebugState));
    require(debugSerializer, exit);
    setProperty("IOHIDUserDeviceDebugState", debugSerializer);
    
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
    
    if ( _createDeviceTimer ) {
        _createDeviceTimer->cancelTimeout();
        workLoop->removeEventSource(_createDeviceTimer);
    }
    
    if ( _commandGate ) {
        cleanupPendingReports();

        workLoop->removeEventSource(_commandGate);
    }

    releaseNotificationPort (_port);
    _port = MACH_PORT_NULL;

exit:
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

    if ( _createDeviceTimer )
        _createDeviceTimer->release();
    
    if ( _device )
        _device->release();
    
    if (_pending) {
        _pending->release();
    }
    
    if ( _queue )
        _queue->release();
    
    if ( _owner )
        _owner->release();
        
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
    
    require_action(!isInactive(), exit, result=kIOReturnOffline);
    require_action(_queue, exit, result=kIOReturnError);

    releaseNotificationPort (_port);
    
    _port = port;
    _queue->setNotificationPort(port);
    
    // 43101337 clear pending reports when our port is cleared, otherwise we will
    // wait until the timeout occurs, which could be a very long time.
    if (!_port && _commandGate) {
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
IOReturn IOHIDResourceDeviceUserClient::externalMethod(
                                            uint32_t                    selector, 
                                            IOExternalMethodArguments * arguments,
                                            IOExternalMethodDispatch *  dispatch, 
                                            OSObject *                  target, 
                                            void *                      reference)
{
    ExternalMethodGatedArguments gatedArguments = {selector, arguments, dispatch, target, reference};
    IOReturn result;
    
    require_action(!isInactive(), exit, result=kIOReturnOffline);
    
    result = _commandGate->runAction(OSMemberFunctionCast(IOCommandGate::Action, this, &IOHIDResourceDeviceUserClient::externalMethodGated), &gatedArguments);
    
exit:
    return result;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::externalMethodGated
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::externalMethodGated(ExternalMethodGatedArguments *arguments)
{
    IOReturn result;
    
    require_action(!isInactive(), exit, result=kIOReturnOffline);

    require_action(arguments->selector < (uint32_t) kIOHIDResourceDeviceUserClientMethodCount, exit, result=kIOReturnBadArgument);

    arguments->dispatch = (IOExternalMethodDispatch *) &_methods[arguments->selector];
    if (!arguments->target)
        arguments->target = this;
    
    result = super::externalMethod(arguments->selector, arguments->arguments, arguments->dispatch, arguments->target, arguments->reference);
    
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
        
        arguments->structureInputDescriptor->prepare();
        arguments->structureInputDescriptor->readBytes(0, report->getBytesNoCopy(), length);
        arguments->structureInputDescriptor->complete();
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
    if ( number )
        _maxClientTimeoutUS = number->unsigned32BitValue();

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
// IOHIDResourceDeviceUserClient::createAndStartDeviceAsyncCallback
//----------------------------------------------------------------------------------------------------
void IOHIDResourceDeviceUserClient::createAndStartDeviceAsyncCallback()
{
    createAndStartDevice();
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::createAndStartDeviceAsync
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::createAndStartDeviceAsync()
{
    _createDeviceTimer->setTimeoutMS(0);
    return kIOReturnSuccess;
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
    
    propertiesData = IOMalloc(propertiesLength);
    require_action(propertiesData, exit, result=kIOReturnNoMemory);
    
    result = propertiesDesc->prepare();
    require_noerr(result, exit);
    propertiesDesc->readBytes(0, propertiesData, propertiesLength);
    propertiesDesc->complete();
    
    require_action(strnlen((const char *) propertiesData, propertiesLength) < propertiesLength, exit, result=kIOReturnInternalError);

    object = OSUnserializeXML((const char *)propertiesData, propertiesLength);
    require_action(object, exit, result=kIOReturnInternalError);
    
    OSSafeReleaseNULL(_properties);
    
    _properties = OSDynamicCast(OSDictionary, object);
    require_action(_properties, exit, result=kIOReturnNoMemory);
    
    _properties->retain();
    
    if ( arguments->scalarInput[0] )
        result = createAndStartDeviceAsync();
    else
        result = createAndStartDevice();
    
    require_noerr(result, exit);

exit:
    
    if ( object )
        object->release();
    
    if ( propertiesData && propertiesLength )
        IOFree(propertiesData, propertiesLength);

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
    IOFree(pb, sizeof(*pb));
    
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
        
        IOHIDResourceDeviceUserClientAsyncParamBlock *pb =
        (IOHIDResourceDeviceUserClientAsyncParamBlock *)IOMalloc(sizeof(IOHIDResourceDeviceUserClientAsyncParamBlock));
        
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
            IOFree(pb, sizeof(*pb));
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

typedef struct {
    IOReturn                ret;
    IOMemoryDescriptor *    descriptor;
    u_int64_t               token;
} __ReportResult;

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::getReport
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::getReport(IOMemoryDescriptor *report, IOHIDReportType reportType, IOOptionBits options)
{
    ReportGatedArguments    arguments   = {report, reportType, options};
    IOReturn                result;
    
    require_action(!isInactive(), exit, result=kIOReturnOffline);

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
    AbsoluteTime                    ts;
    IOReturn                        ret;
    OSData *                        retData = NULL;
    
    require_action(!isInactive(), exit, ret=kIOReturnOffline);
    
    result.descriptor = arguments->report;
    result.token      = _tokenIndex++;
    
    retData = OSData::withBytesNoCopy(&result, sizeof(__ReportResult));
    require_action(retData, exit, ret=kIOReturnNoMemory);
    
    header.direction   = kIOHIDResourceReportDirectionIn;
    header.type        = arguments->reportType;
    header.reportID    = arguments->options&0xff;
    header.length      = (uint32_t)arguments->report->getLength();
    header.token       = result.token;

    _pending->setObject(retData);
    
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
    
    // if we successfully enqueue, let's sleep till we get a result from postReportResult
    clock_interval_to_deadline(kMicrosecondScale, _maxClientTimeoutUS, &ts);
    
    switch ( _commandGate->commandSleep(retData, ts, THREAD_ABORTSAFE) ) {
        case THREAD_AWAKENED:
            ret = result.ret;
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
    
exit:
    if ( retData ) {
        _pending->removeObject(retData);
        _commandGate->commandWakeup(&_pending);
        retData->release();
    }
    
    return ret;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::setReport
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::setReport(IOMemoryDescriptor *report, IOHIDReportType reportType, IOOptionBits options)
{
    ReportGatedArguments    arguments={report, reportType, options};
    IOReturn                result;
    
    require_action(!isInactive(), exit, result=kIOReturnOffline);
    
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
    AbsoluteTime                    ts;
    IOReturn                        ret;
    OSData *                        retData = NULL;
    
    require_action(!isInactive(), exit, ret=kIOReturnOffline);

    bzero(&result, sizeof(result));
    
    result.token       = _tokenIndex++;
    
    retData = OSData::withBytesNoCopy(&result, sizeof(result));
    require_action(retData, exit, ret=kIOReturnNoMemory);
    
    header.direction   = kIOHIDResourceReportDirectionOut;
    header.type        = arguments->reportType;
    header.reportID    = arguments->options&0xff;
    header.length      = (uint32_t)arguments->report->getLength();
    header.token       = result.token;

    _pending->setObject(retData);
    
    require_action(_queue, exit, ret = kIOReturnNotReady); // client has not mapped memory
    require_action(_port, exit, ret = kIOReturnOffline); // client has not registered a port
    
    _setReportCount++;
    
    require_action(_queue->enqueueReport(&header, arguments->report), exit, {
        HIDLogInfo("0x%llx: IOHIDUserDevice setReport enqueue failed.\n", getRegistryEntryID());
        ret = kIOReturnNoMemory;
        _setReportDroppedCount++;
        _enqueueFailCount++;
        _queue->sendDataAvailableNotification();
        IOHID_DEBUG(kIOHIDDebugCode_HIDUserDeviceEnqueueFail, mach_continuous_time(), 0, 0, 0);
    });

    // if we successfully enqueue, let's sleep till we get a result from postReportResult
    clock_interval_to_deadline(kMicrosecondScale, _maxClientTimeoutUS, (uint64_t *)&ts);
    
    switch ( _commandGate->commandSleep(retData, ts, THREAD_ABORTSAFE) ) {
        case THREAD_AWAKENED:
            ret = result.ret;
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

exit:
    if ( retData ) {
        _pending->removeObject(retData);
        _commandGate->commandWakeup(&_pending);
        retData->release();
    }
    
    return ret;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::postReportResult
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::postReportResult(IOExternalMethodArguments * arguments)
{
    OSObject * object = NULL;
    IOReturn result = kIOReturnNotFound;
    
    u_int64_t token = (u_int64_t)arguments->scalarInput[kIOHIDResourceUserClientResponseIndexToken];

    OSCollectionIterator * iterator = OSCollectionIterator::withCollection(_pending);
    require_action(iterator, exit, result = kIOReturnNoMemory);
    
    while ( (object = iterator->getNextObject()) ) {
        __ReportResult * pResult = (__ReportResult*)((OSData*)object)->getBytesNoCopy();
        
        if (pResult->token != token)
            continue;
        
        // RY: HIGHLY UNLIKELY > 4K
        if ( pResult->descriptor && arguments->structureInput ) {
            pResult->descriptor->writeBytes(0, arguments->structureInput, arguments->structureInputSize);
            
            // 12978252:  If we get an IOBMD passed in, set the length to be the # of bytes that were transferred
            IOBufferMemoryDescriptor * buffer = OSDynamicCast(IOBufferMemoryDescriptor, pResult->descriptor);
            if (buffer)
                buffer->setLength(MIN((vm_size_t)arguments->structureInputSize, buffer->getCapacity()));
            
        }
        
        pResult->ret = (IOReturn)arguments->scalarInput[kIOHIDResourceUserClientResponseIndexResult];
        
        _commandGate->commandWakeup(object);
        
        result = kIOReturnSuccess;
        break;
    }
    
    iterator->release();

exit:
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
// IOHIDResourceDeviceUserClient::cleanupPendingReports
//----------------------------------------------------------------------------------------------------
void IOHIDResourceDeviceUserClient::cleanupPendingReports()
{
    OSCollectionIterator *  iterator;
    OSObject *              object;
    
    iterator = OSCollectionIterator::withCollection(_pending);
    if ( !iterator )
        return;
        
    while ( (object = iterator->getNextObject()) ) {
        __ReportResult * pResult = (__ReportResult*)((OSData*)object)->getBytesNoCopy();
        
        pResult->ret = kIOReturnAborted;
        
        _commandGate->commandWakeup(object);
    }
    
    iterator->release();
    
    while ( _pending->getCount() ) {
        _commandGate->commandSleep(&_pending);
    }
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

void IOHIDResourceQueue::free()
{
    if ( _descriptor )
    {
        _descriptor->release();
        _descriptor = 0;
    }

    IOSharedDataQueue::free();
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
            
            if ( report )
                report->readBytes(0, ((UInt8*)&entry->data) + headerSize, reportSize);

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
            if ( report )
                report->readBytes(0, ((UInt8*)&dataQueue->queue->data) + headerSize, reportSize);
   
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
            if ( report )
                report->readBytes(0, ((UInt8*)&entry->data) + headerSize, reportSize);
            
            newTail = tail + entrySize;
        }
        else
        {
            return false;    // queue is full
        }
    }

    // Update tail with release barrier
    _enqueueTS = mach_continuous_time();
    __c11_atomic_store((_Atomic UInt32 *)&dataQueue->tail, newTail, __ATOMIC_RELEASE);
    
    // Send notification (via mach message) that data is available if either the
    // queue was empty prior to enqueue() or queue was emptied during enqueue()
    if ( ( head == tail ) || ( __c11_atomic_load((_Atomic UInt32 *)&dataQueue->head, __ATOMIC_ACQUIRE) == tail ) ) {
        sendDataAvailableNotification();
    }

    return true;
}

void IOHIDResourceQueue::setNotificationPort(mach_port_t port) 
{
    IOSharedDataQueue::setNotificationPort(port);

    if (dataQueue->head != dataQueue->tail)
        sendDataAvailableNotification();
}

IOMemoryDescriptor * IOHIDResourceQueue::getMemoryDescriptor()
{
    if (!_descriptor)
        _descriptor = IOSharedDataQueue::getMemoryDescriptor();

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
