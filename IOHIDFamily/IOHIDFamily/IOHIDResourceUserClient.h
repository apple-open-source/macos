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

#ifndef _IOHIDRESOURCE_USERCLIENT_H
#define _IOHIDRESOURCE_USERCLIENT_H

#include <IOKit/hid/IOHIDKeys.h>
/*!
    @enum IOHIDResourceUserClientType
    @abstract List of support user client types
    @description Current the only support type is kIOHIDResourceUserClientTypeDevice.  The hope is to expand this out to support other HID resources/
    @constant kIOHIDResourceUserClientTypeDevice Type for creating an in kernel representation of a HID driver that resides in user space
*/
typedef enum {
	kIOHIDResourceUserClientTypeDevice = 0
} IOHIDResourceUserClientType;

/*!
    @enum IOHIDResourceDeviceUserClientExternalMethods
    @abstract List of external methods to be called from user land
    @constant kIOHIDResourceDeviceUserClientMethodCreate Creates a device using a passed serialized property dictionary.
	@constant kIOHIDResourceDeviceUserClientMethodTerminate Closes the device and releases memory.
    @constant kIOHIDResourceDeviceUserClientMethodHandleReport Sends a report.
    @constant kIOHIDResourceDeviceUserClientMethodPostReportResult Posts a report requested via GetReport and SetReport
    @constant kIOHIDResourceDeviceUserClientMethodCount
*/
typedef enum {
	kIOHIDResourceDeviceUserClientMethodCreate,
	kIOHIDResourceDeviceUserClientMethodTerminate,
	kIOHIDResourceDeviceUserClientMethodHandleReport,
    kIOHIDResourceDeviceUserClientMethodPostReportResponse,
	kIOHIDResourceDeviceUserClientMethodCount
} IOHIDResourceDeviceUserClientExternalMethods;

/*!
    @enum IOHIDResourceUserClientResponseIndex
    @abstract reponse indexes for report response
*/
typedef enum {
	kIOHIDResourceUserClientResponseIndexResult = 0,
	kIOHIDResourceUserClientResponseIndexToken,
	kIOHIDResourceUserClientResponseIndexCount
} IOHIDResourceUserClientResponseIndex;

typedef enum { 
    kIOHIDResourceReportDirectionIn,
    kIOHIDResourceReportDirectionOut
} IOHIDResourceReportDirection;
/*!
    @enum IOHIDResourceDataQueueHeader
    @abstract Header used for sending requests to user process
*/
typedef struct {
    IOHIDResourceReportDirection    direction;    
    IOHIDReportType                 type;
    uint32_t                        reportID;
    uint32_t                        length;
    uint64_t                        token;
} IOHIDResourceDataQueueHeader;

/*
 * Kernel
 */
#if KERNEL

#include <IOKit/IOUserClient.h>
#include <IOKit/IODataQueue.h>
#include "IOHIDResource.h"
#include "IOHIDUserDevice.h"


/*! @class IOHIDResourceDeviceUserClient : public IOUserClient
    @abstract 
*/
/*! @class IOHIDResourceQueue : public IODataQueue
    @abstract 
*/
class IOHIDResourceQueue: public IODataQueue
{
    OSDeclareDefaultStructors( IOHIDResourceQueue )
    
protected:
    IOMemoryDescriptor *    _descriptor;

public:
    static IOHIDResourceQueue *withEntries(UInt32 numEntries, UInt32 entrySize);
    virtual void free();
    
    virtual Boolean enqueueReport(IOHIDResourceDataQueueHeader * header, IOMemoryDescriptor * report = NULL);

    virtual IOMemoryDescriptor *getMemoryDescriptor();
    virtual void setNotificationPort(mach_port_t port);
};

class IOHIDResourceDeviceUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOHIDResourceDeviceUserClient);
	
private:

    IOHIDResource *         _owner;
	IOHIDUserDevice *       _device;
    mach_port_t             _port;
    IOHIDResourceQueue *    _queue;
    IOLock *                _lock;
    OSSet *                 _pending;

	static const IOExternalMethodDispatch _methods[kIOHIDResourceDeviceUserClientMethodCount];

	static IOReturn _createDevice(IOHIDResourceDeviceUserClient *target, void *reference, IOExternalMethodArguments *arguments);
	static IOReturn _terminateDevice(IOHIDResourceDeviceUserClient *target, void *reference, IOExternalMethodArguments *arguments);
	static IOReturn _handleReport(IOHIDResourceDeviceUserClient *target,  void *reference, IOExternalMethodArguments *arguments);
	static IOReturn _postReportResult(IOHIDResourceDeviceUserClient *target,  void *reference, IOExternalMethodArguments *arguments);

	IOReturn createDevice(IOHIDResourceDeviceUserClient *target, void *reference, IOExternalMethodArguments *arguments);
	IOReturn handleReport(IOHIDResourceDeviceUserClient *target,  void *reference, IOExternalMethodArguments *arguments);
    IOReturn postReportResult(IOHIDResourceDeviceUserClient *target,  void *reference, IOExternalMethodArguments *arguments);
	IOReturn terminateDevice();
    void cleanupPendingReports();

    IOMemoryDescriptor * createMemoryDescriptorFromInputArguments(IOExternalMethodArguments * arguments);

    void ReportComplete(void *param, IOReturn res, UInt32 remaining);

public:
	/*! @function initWithTask
		@abstract 
		@discussion 
	*/
	virtual bool initWithTask(task_t owningTask, void * security_id, UInt32 type);


	/*! @function clientClose
		@abstract 
		@discussion 
	*/
	virtual IOReturn clientClose(void);


	/*! @function getService
		@abstract 
		@discussion 
	*/
	virtual IOService * getService(void);


	/*! @function externalMethod
		@abstract 
		@discussion 
	*/
	virtual IOReturn externalMethod(uint32_t selector, IOExternalMethodArguments *arguments,
							   IOExternalMethodDispatch *dispatch, OSObject *target, 
							   void *reference);

    virtual IOReturn clientMemoryForType(UInt32 type, IOOptionBits * options, IOMemoryDescriptor ** memory );


	/*! @function start
		@abstract 
		@discussion 
	*/
	virtual bool start(IOService * provider);
    
	virtual void stop(IOService * provider);

    virtual void free();

    virtual IOReturn registerNotificationPort(mach_port_t port, UInt32 type, io_user_reference_t refCon);
    
    virtual IOReturn getReport(IOMemoryDescriptor *report, IOHIDReportType reportType, IOOptionBits options);

    virtual IOReturn setReport(IOMemoryDescriptor *report, IOHIDReportType reportType, IOOptionBits options);

};


#endif /* KERNEL */

#endif /* _IOHIDRESOURCE_USERCLIENT_H */

