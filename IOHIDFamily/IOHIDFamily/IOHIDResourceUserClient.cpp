/*
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 2008 Apple, Inc.  All Rights Reserved.
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

#include <IOKit/IOLib.h>

#ifdef enqueue
    #undef enqueue
#endif

#include "IOHIDResourceUserClient.h"

#define kHIDRTimeoutNS    1000000000

#define super IOUserClient


OSDefineMetaClassAndStructors( IOHIDResourceDeviceUserClient, IOUserClient )


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDResourceDeviceUserClient::_methods
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
const IOExternalMethodDispatch IOHIDResourceDeviceUserClient::_methods[kIOHIDResourceDeviceUserClientMethodCount] = {
	{   // kIOHIDResourceDeviceUserClientMethodCreate
		(IOExternalMethodAction) &IOHIDResourceDeviceUserClient::_createDevice,
		0, -1, /* 1 struct input : the report descriptor */
		0, 0
	},
	{   // kIOHIDResourceDeviceUserClientMethodTerminate
		(IOExternalMethodAction) &IOHIDResourceDeviceUserClient::_terminateDevice,
		0, 0,
		0, 0
	},
	{   // kIOHIDResourceDeviceUserClientMethodHandleReport
		(IOExternalMethodAction) &IOHIDResourceDeviceUserClient::_handleReport,
		0, -1, /* 1 struct input : the buffer */
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
#if !TARGET_OS_EMBEDDED
    if (kIOReturnSuccess != clientHasPrivilege(owningTask, kIOClientPrivilegeAdministrator))
        return false;
#endif
    
    if (!super::initWithTask(owningTask, security_id, type)) {
		IOLog("%s failed\n", __FUNCTION__);
		return false;
	}
	
	_device = NULL;
    _lock   = IOLockAlloc();
    _pending = OSSet::withCapacity(4);

	return true;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::start
//----------------------------------------------------------------------------------------------------
bool IOHIDResourceDeviceUserClient::start(IOService * provider)
{
	if (!super::start(provider)) {
		IOLog("%s failed\n", __FUNCTION__);
		return false;
	}	

    _owner = (IOHIDResource *) provider;
	_device = NULL;

	return true;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::stop
//----------------------------------------------------------------------------------------------------
void IOHIDResourceDeviceUserClient::stop(IOService * provider)
{
    if ( _device )
        _device->release();
        
    super::stop(provider);
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::free
//----------------------------------------------------------------------------------------------------
void IOHIDResourceDeviceUserClient::free()
{
    if ( _queue )
        _queue->release();

    if ( _lock )
        IOLockFree(_lock);
        
	return super::free();
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::registerNotificationPort
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::registerNotificationPort(mach_port_t port, UInt32 type __unused, io_user_reference_t refCon __unused)
{
    if ( isInactive() )
        return kIOReturnNoDevice;

    _port = port;
    _queue->setNotificationPort(port);
    return kIOReturnSuccess;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::clientMemoryForType
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::clientMemoryForType(UInt32 type __unused, IOOptionBits * options, IOMemoryDescriptor ** memory )
{
    IOReturn ret = kIOReturnNoMemory;
           
    if ( isInactive() )
        return kIOReturnNoDevice;

    if ( !_queue ) {
        UInt32 maxOutputReportSize  = 0;
        UInt32 maxFeatureReportSize = 0;
        OSNumber * number;
        
        number = (OSNumber*)_device->copyProperty(kIOHIDMaxOutputReportSizeKey);
        if ( OSDynamicCast(OSNumber, number) )
            maxOutputReportSize = number->unsigned32BitValue();
        OSSafeReleaseNULL(number);

        number = (OSNumber*)_device->copyProperty(kIOHIDMaxFeatureReportSizeKey);
        if ( OSDynamicCast(OSNumber, number) )
            maxFeatureReportSize = number->unsigned32BitValue();
        OSSafeReleaseNULL(number);
        
        _queue = IOHIDResourceQueue::withEntries(4, max(maxFeatureReportSize, maxOutputReportSize)+sizeof(IOHIDResourceDataQueueHeader));
    }
    
    if ( _queue ) {
        IOMemoryDescriptor * memoryToShare = _queue->getMemoryDescriptor();
    
        // if we got some memory
        if (memoryToShare)
        {
            // Memory will be released by user client
            // when last map is destroyed.

            memoryToShare->retain();

            ret = kIOReturnSuccess;
        }
        
        // set the result
        *options = 0;
        *memory  = memoryToShare;
    }
        
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
    if ( isInactive() )
        return kIOReturnNoDevice;
        
    if (selector < (uint32_t) kIOHIDResourceDeviceUserClientMethodCount)
    {
        dispatch = (IOExternalMethodDispatch *) &_methods[selector];
        if (!target)
            target = this;
    }

	return super::externalMethod(selector, arguments, dispatch, target, reference);
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
	return _owner;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::clientClose
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::clientClose(void)
{
    cleanupPendingReports();
    terminate();
	return kIOReturnSuccess;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::createDevice
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::createDevice(
                                        IOHIDResourceDeviceUserClient * target __unused, 
                                        void *                          reference __unused, 
                                        IOExternalMethodArguments *     arguments)
{
	if (_device == NULL)  { // Report descriptor is static and thus can only be set on creation
        
        IOReturn                ret;
        IOMemoryDescriptor *    propertiesDesc  = NULL;
        OSDictionary *          properties      = NULL;
        
        // Let's deal with our device properties from data
        propertiesDesc = createMemoryDescriptorFromInputArguments(arguments);
        if ( !propertiesDesc ) {
            IOLog("%s failed : could not create descriptor\n", __FUNCTION__);
            return kIOReturnNoMemory;
        }

        ret = propertiesDesc->prepare();
        if ( ret == kIOReturnSuccess ) {
        
            void *         propertiesData;
            IOByteCount    propertiesLength;
            
            propertiesLength = propertiesDesc->getLength();
            if ( propertiesLength ) { 
                propertiesData = IOMalloc(propertiesLength);
            
                if ( propertiesData ) { 
                    OSObject * object;
                
                    propertiesDesc->readBytes(0, propertiesData, propertiesLength);

                    object = OSUnserializeXML((const char *)propertiesData);
                    if (object) {
                        properties = OSDynamicCast(OSDictionary, object);
                        if( !properties )
                            object->release();
                    }
                    
                    IOFree(propertiesData, propertiesLength);
                }
            
            }
            propertiesDesc->complete();
        }
        propertiesDesc->release();
        
        // If after all the unwrapping we have a dictionary, let's create the device
        if ( properties ) { 
            _device = IOHIDUserDevice::withProperties(properties);
            properties->release();
        }

        
	} else {/* We already have a device. Close it before opening a new one */
		IOLog("%s failed : _device already exists\n", __FUNCTION__);
		return kIOReturnInternalError;
	}

	if (_device == NULL) {
		IOLog("%s failed : _device is NULL\n", __FUNCTION__);
		return kIOReturnNoResources;
	}

    IOReturn ret = kIOReturnInternalError;
    
	if (_device->attach(this) ) {
        if ( _device->start(this) ) {
            ret = kIOReturnSuccess;
        } else {
            IOLog("%s start failed\n", __FUNCTION__);
            _device->detach(this);
        }
	} else {
		IOLog("%s attach failed\n", __FUNCTION__);
    }
    
    if ( ret != kIOReturnSuccess ) {
		_device->release();
		_device = NULL;
    }

    return ret;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::_createDevice
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::_createDevice(
                                        IOHIDResourceDeviceUserClient * target, 
                                        void *                          reference, 
                                        IOExternalMethodArguments *     arguments)
{
	return target->createDevice(target, reference, arguments);
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
IOReturn IOHIDResourceDeviceUserClient::handleReport(
                                        IOHIDResourceDeviceUserClient * target, 
                                        void *                          reference __unused, 
                                        IOExternalMethodArguments *     arguments)
{
	if (_device == NULL) {
		IOLog("%s failed : device is NULL\n", __FUNCTION__);
		return kIOReturnNotOpen;
	}

	if (target != this) {
		IOLog("%s failed : this is not target\n", __FUNCTION__);
		return kIOReturnInternalError;
	}

    IOReturn                ret;
    IOMemoryDescriptor *    report;
    
    report = createMemoryDescriptorFromInputArguments(arguments);
    if ( !report ) {
		IOLog("%s failed : could not create descriptor\n", __FUNCTION__);
		return kIOReturnNoMemory;
	}

    if ( !arguments->asyncWakePort ) {
        ret = report->prepare();
        if ( ret == kIOReturnSuccess ) {
            ret = _device->handleReport(report);
            report->complete();
        }
        
        report->release();
    } else {
        IOHIDCompletion tap;
        
        IOHIDResourceDeviceUserClientAsyncParamBlock *pb =
        (IOHIDResourceDeviceUserClientAsyncParamBlock *)IOMalloc(sizeof(IOHIDResourceDeviceUserClientAsyncParamBlock));
        
        if (!pb) {
            report->release();
            return kIOReturnNoMemory;   // need to release report
        }
        
        target->retain();
        
        bcopy(arguments->asyncReference, pb->fAsyncRef, sizeof(OSAsyncReference64));
        pb->fAsyncCount = arguments->asyncReferenceCount;
        
        tap.target = target;
        tap.action = OSMemberFunctionCast(IOHIDCompletionAction, target, &IOHIDResourceDeviceUserClient::ReportComplete);
        tap.parameter = pb;
        
        AbsoluteTime   currentTime;
        
        clock_get_uptime( &currentTime );
        
        ret = report->prepare();
        if ( ret == kIOReturnSuccess ) {
            ret = _device->handleReportWithTimeAsync(currentTime, report, kIOHIDReportTypeInput, 0, 0, &tap);
            report->complete();
        }
        
        report->release();
        
        if (ret != kIOReturnSuccess) {
            IOFree(pb, sizeof(*pb));
            target->release();
        }
    }
    
    return ret;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::_handleReport
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::_handleReport(IOHIDResourceDeviceUserClient	*target, 
											 void						*reference, 
											 IOExternalMethodArguments	*arguments)
{
	return target->handleReport(target, reference, arguments);
}

typedef struct {
    IOReturn                ret;
    IOMemoryDescriptor *    descriptor;
} __ReportResult;

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::getReport
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::getReport(IOMemoryDescriptor *report, IOHIDReportType reportType, IOOptionBits options)
{
    IOHIDResourceDataQueueHeader    header;
    __ReportResult                  result;
    IOReturn                        ret = kIOReturnNoMemory;
    OSData *                        retData = NULL;
    
    result.descriptor = report;
    
    retData = OSData::withBytesNoCopy(&result, sizeof(__ReportResult));
    if ( retData ) {
    
        header.direction   = kIOHIDResourceReportDirectionIn;
        header.type        = reportType;
        header.reportID    = options&0xff;
        header.length      = report->getLength();
        header.token       = (intptr_t)retData;

        IOLockLock(_lock);

        _pending->setObject(retData);
        retData->release();
                
        // if we successfully enqueue, let's sleep till we get a result from postReportResult
        if ( _queue && _queue->enqueueReport(&header) ){
            AbsoluteTime ts;
            clock_interval_to_deadline(1, kHIDRTimeoutNS, &ts);
            switch ( IOLockSleepDeadline(_lock, (void *)retData, ts, THREAD_ABORTSAFE) ) {
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
        }
        _pending->removeObject(retData);
        
        IOLockUnlock(_lock);
    }

    return ret;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::setReport
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::setReport(IOMemoryDescriptor *report, IOHIDReportType reportType, IOOptionBits options)
{
    IOHIDResourceDataQueueHeader    header;
    __ReportResult                  result;
    IOReturn                        ret = kIOReturnNoMemory;
    OSData *                        retData = NULL;
    
    bzero(&result, sizeof(result));
    
    retData = OSData::withBytesNoCopy(&result, sizeof(result));
    if ( retData ) {
    
        header.direction   = kIOHIDResourceReportDirectionOut;
        header.type        = reportType;
        header.reportID    = options&0xff;
        header.length      = report->getLength();
        header.token       = (intptr_t)retData;

        IOLockLock(_lock);

        _pending->setObject(retData);
        retData->release();
                
        // if we successfully enqueue, let's sleep till we get a result from postReportResult
        if ( _queue && _queue->enqueueReport(&header, report) ) {   
            AbsoluteTime ts;
            clock_interval_to_deadline(1, kHIDRTimeoutNS, &ts);
            
            switch ( IOLockSleepDeadline(_lock, (void *)retData, ts, THREAD_ABORTSAFE) ) {
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
            
        }
        _pending->removeObject(retData);
        
        IOLockUnlock(_lock);
    }

    return ret;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::postReportResult
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::postReportResult(
                                        IOHIDResourceDeviceUserClient * target __unused, 
                                        void *                          reference __unused, 
                                        IOExternalMethodArguments *     arguments)
{
    OSObject * tokenObj = (OSObject*)arguments->scalarInput[kIOHIDResourceUserClientResponseIndexToken];

    IOLockLock(_lock);
    if ( tokenObj && _pending->containsObject(tokenObj) ) {
        OSData * data = OSDynamicCast(OSData, tokenObj);
        if ( data ) {
            __ReportResult * pResult = (__ReportResult*)data->getBytesNoCopy();
            
            // RY: HIGHLY UNLIKELY > 4K
            if ( pResult->descriptor && arguments->structureInput )
                pResult->descriptor->writeBytes(0, arguments->structureInput, arguments->structureInputSize);
                
            pResult->ret = arguments->scalarInput[kIOHIDResourceUserClientResponseIndexResult];

            IOLockWakeup(_lock, (void *)data, false);  
        }
            
    }
    IOLockUnlock(_lock);
    return kIOReturnSuccess;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::_postReportResult
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::_postReportResult(IOHIDResourceDeviceUserClient	*target, 
											 void						*reference, 
											 IOExternalMethodArguments	*arguments)
{
	return target->postReportResult(target, reference, arguments);
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
        
    while ( (object = iterator->getNextObject()) )
        IOLockWakeup(_lock, (void *)object, false);  
    
    iterator->release();
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::terminateDevice
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::terminateDevice()
{
	if (_device) {
		_device->terminate();
		_device->release();
	}
	_device = NULL;

    return kIOReturnSuccess;
}

//----------------------------------------------------------------------------------------------------
// IOHIDResourceDeviceUserClient::_terminateDevice
//----------------------------------------------------------------------------------------------------
IOReturn IOHIDResourceDeviceUserClient::_terminateDevice(
                                        IOHIDResourceDeviceUserClient	*target, 
                                        void                            *reference __unused, 
                                        IOExternalMethodArguments       *arguments __unused)
{
	return target->terminateDevice();
}


//====================================================================================================
// IOHIDResourceQueue
//====================================================================================================
#include <IOKit/IODataQueueShared.h>
OSDefineMetaClassAndStructors( IOHIDResourceQueue, IODataQueue )

IOHIDResourceQueue *IOHIDResourceQueue::withEntries(UInt32 numEntries, UInt32 entrySize)
{
    IOHIDResourceQueue *dataQueue = new IOHIDResourceQueue;

    if (dataQueue) {
        if  (!dataQueue->initWithEntries(numEntries, entrySize)) {
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

    IODataQueue::free();
}

#define ALIGNED_DATA_SIZE(data_size,align_size) ((((data_size - 1) / align_size) + 1) * align_size)

Boolean IOHIDResourceQueue::enqueueReport(IOHIDResourceDataQueueHeader * header, IOMemoryDescriptor * report)
{
    IOByteCount         headerSize  = sizeof(IOHIDResourceDataQueueHeader);
    IOByteCount         reportSize  = report ? report->getLength() : 0;
    IOByteCount         dataSize    = ALIGNED_DATA_SIZE(headerSize + reportSize, sizeof(uint32_t));
    const UInt32        head        = dataQueue->head;  // volatile
    const UInt32        tail        = dataQueue->tail;
    const UInt32        entrySize   = dataSize + DATA_QUEUE_ENTRY_HEADER_SIZE;
    IODataQueueEntry *  entry;

    if ( tail >= head )
    {
        // Is there enough room at the end for the entry?
        if ( (tail + entrySize) <= dataQueue->queueSize )
        {
            entry = (IODataQueueEntry *)((UInt8 *)dataQueue->queue + tail);

            entry->size = dataSize;
            
            bcopy(header, &entry->data, headerSize);
            
            if ( report )
                report->readBytes(0, ((UInt8*)&entry->data) + headerSize, reportSize);

            // The tail can be out of bound when the size of the new entry
            // exactly matches the available space at the end of the queue.
            // The tail can range from 0 to dataQueue->queueSize inclusive.

            dataQueue->tail += entrySize;
        }
        else if ( head > entrySize ) 	// Is there enough room at the beginning?
        {
            // Wrap around to the beginning, but do not allow the tail to catch
            // up to the head.

            dataQueue->queue->size = dataSize;

            // We need to make sure that there is enough room to set the size before
            // doing this. The user client checks for this and will look for the size
            // at the beginning if there isn't room for it at the end.

            if ( ( dataQueue->queueSize - tail ) >= DATA_QUEUE_ENTRY_HEADER_SIZE )
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
            return false;	// queue is full
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
            dataQueue->tail += entrySize;
        }
        else
        {
            return false;	// queue is full
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
    IODataQueue::setNotificationPort(port);

    if (dataQueue->head != dataQueue->tail)
        sendDataAvailableNotification();
}

IOMemoryDescriptor * IOHIDResourceQueue::getMemoryDescriptor()
{
    if (!_descriptor)
        _descriptor = IODataQueue::getMemoryDescriptor();

    return _descriptor;
}
