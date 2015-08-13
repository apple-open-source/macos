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

#ifdef enqueue
    #undef enqueue
#endif

#include "IOHIDResourceUserClient.h"

#define kHIDClientTimeoutUS     1000000ULL

#define kHIDQueueSize           16384

#define MIN(a,b) ((a) < (b) ? (a) : (b))

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
    
#if !TARGET_OS_EMBEDDED
    require_noerr_action(clientHasPrivilege(owningTask, kIOClientPrivilegeAdministrator), exit, result=false);
#endif
    
    result = super::initWithTask(owningTask, security_id, type);
    require_action(result, exit, IOLog("%s failed\n", __FUNCTION__));
    
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
    IOWorkLoop *    workLoop;
    bool            result;
    
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
    
    result = true;
    
exit:
    if ( result==false ) {
        IOLog("%s failed\n", __FUNCTION__);
        stop(provider);
    }

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

    _port = port;
    _queue->setNotificationPort(port);
    
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
        _queue = IOHIDResourceQueue::withCapacity(kHIDQueueSize);
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
    IOMemoryDescriptor * report = NULL;
    
    if ( arguments->structureInputDescriptor ) {
        report = arguments->structureInputDescriptor;
        report->retain();
    } else {
        report = IOMemoryDescriptor::withAddress((void *)arguments->structureInput, arguments->structureInputSize, kIODirectionOut);
    }
    
    return report;
}


//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::getService
//----------------------------------------------------------------------------------------------------
IOService * IOHIDResourceDeviceUserClient::getService(void)
{
    return _owner ? _owner : NULL;
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
        IOLog("%s: result=0x%08x\n", __FUNCTION__, result);
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
    
    propertiesDesc->readBytes(0, propertiesData, propertiesLength);
    
    require_action(strnlen((const char *) propertiesData, propertiesLength) < propertiesLength, exit, result=kIOReturnInternalError);

    object = OSUnserializeXML((const char *)propertiesData, propertiesLength);
    require_action(object, exit, result=kIOReturnInternalError);

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
        IOLog("%s failed : device is NULL\n", __FUNCTION__);
        return kIOReturnNotOpen;
    }

    IOReturn                ret;
    IOMemoryDescriptor *    report;
    
    report = createMemoryDescriptorFromInputArguments(arguments);
    if ( !report ) {
        IOLog("%s failed : could not create descriptor\n", __FUNCTION__);
        return kIOReturnNoMemory;
    }
    
    if ( arguments->scalarInput[0] )
        AbsoluteTime_to_scalar(&timestamp) = arguments->scalarInput[0];
    else
        clock_get_uptime( &timestamp );
    
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
    
    require_action(_queue && _queue->enqueueReport(&header), exit, ret=kIOReturnNoMemory);
    
    // if we successfully enqueue, let's sleep till we get a result from postReportResult
    clock_interval_to_deadline(kMicrosecondScale, _maxClientTimeoutUS, &ts);
    
    switch ( _commandGate->commandSleep(retData, ts, THREAD_ABORTSAFE) ) {
        case THREAD_AWAKENED:
            ret = result.ret;
            break;
        case THREAD_TIMED_OUT:
            ret = kIOReturnTimeout;
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
    
    require_action(_queue && _queue->enqueueReport(&header, arguments->report), exit, ret=kIOReturnNoMemory);

    // if we successfully enqueue, let's sleep till we get a result from postReportResult
    clock_interval_to_deadline(kMicrosecondScale, _maxClientTimeoutUS, (uint64_t *)&ts);
    
    switch ( _commandGate->commandSleep(retData, ts, THREAD_ABORTSAFE) ) {
        case THREAD_AWAKENED:
            ret = result.ret;
            break;
        case THREAD_TIMED_OUT:
            ret = kIOReturnTimeout;
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
    
    u_int64_t token = (u_int64_t)arguments->scalarInput[kIOHIDResourceUserClientResponseIndexToken];

    OSCollectionIterator * iterator = OSCollectionIterator::withCollection(_pending);
    if ( !iterator )
        return kIOReturnNoMemory;
    
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
        
        return kIOReturnSuccess;
    }

    return kIOReturnNotFound;
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
    OSSafeRelease(_device);

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
    UInt32              dataSize    = ALIGNED_DATA_SIZE(headerSize + reportSize, sizeof(uint32_t));
    const UInt32        head        = dataQueue->head;  // volatile
    const UInt32        tail        = dataQueue->tail;
    const UInt32        entrySize   = dataSize + DATA_QUEUE_ENTRY_HEADER_SIZE;
    IODataQueueEntry *  entry;

    if ( ( tail > getQueueSize() || head > getQueueSize() ) )
    {
        return false;
    }

    if ( tail >= head )
    {
        // Is there enough room at the end for the entry?
        if ( (tail + entrySize) <= getQueueSize() )
        {
            entry = (IODataQueueEntry *)((UInt8 *)dataQueue->queue + tail);

            entry->size = dataSize;
            
            bcopy(header, &entry->data, headerSize);
            
            if ( report )
                report->readBytes(0, ((UInt8*)&entry->data) + headerSize, reportSize);

            // The tail can be out of bound when the size of the new entry
            // exactly matches the available space at the end of the queue.
            // The tail can range from 0 to getQueueSize() inclusive.

            dataQueue->tail += entrySize;
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
            dataQueue->tail = entrySize;
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

        if ( ( ( head - tail) > entrySize ) && ( tail + entrySize <= getQueueSize() ) )
        {
            entry = (IODataQueueEntry *)((UInt8 *)dataQueue->queue + tail);

            entry->size = dataSize;

            bcopy(header, &entry->data, sizeof(IOHIDResourceDataQueueHeader));
            if ( report )
                report->readBytes(0, ((UInt8*)&entry->data) + headerSize, reportSize);
            dataQueue->tail += entrySize;
        }
        else
        {
            return false;    // queue is full
        }
    }

    // Send notification (via mach message) that data is available if either the
    // queue was empty prior to enqueue() or queue was emptied during enqueue()
    if ( ( head == tail ) || ( dataQueue->head == tail ) )
        sendDataAvailableNotification();

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
