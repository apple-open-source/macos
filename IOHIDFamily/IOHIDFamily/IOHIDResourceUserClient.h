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
    @constant kIOHIDResourceDeviceUserClientMethodHandleReport Sends a report and generates a keyboard event from that report.
    @constant kIOHIDResourceDeviceUserClientMethodCount
*/
typedef enum {
	kIOHIDResourceDeviceUserClientMethodCreate,
	kIOHIDResourceDeviceUserClientMethodTerminate,
	kIOHIDResourceDeviceUserClientMethodHandleReport,
	kIOHIDResourceDeviceUserClientMethodCount
} IOHIDResourceDeviceUserClientExternalMethods;


/*
 * Kernel
 */
#if KERNEL

#include <IOKit/IOUserClient.h>
#include "IOHIDResource.h"
#include "IOHIDUserDevice.h"


/*! @class IOHIDResourceDeviceUserClient : public IOUserClient
    @abstract 
*/
class IOHIDResourceDeviceUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOHIDResourceDeviceUserClient);
	
private:

    IOHIDResource		*_owner;
	IOHIDUserDevice		*_device;

	static const IOExternalMethodDispatch		_methods[kIOHIDResourceDeviceUserClientMethodCount];

	static IOReturn _createDevice(IOHIDResourceDeviceUserClient *target, void *reference, IOExternalMethodArguments *arguments);
	static IOReturn _terminateDevice(IOHIDResourceDeviceUserClient *target, void *reference, IOExternalMethodArguments *arguments);
	static IOReturn _handleReport(IOHIDResourceDeviceUserClient *target,  void *reference, IOExternalMethodArguments *arguments);

	IOReturn createDevice(IOHIDResourceDeviceUserClient *target, void *reference, IOExternalMethodArguments *arguments);
	IOReturn handleReport(IOHIDResourceDeviceUserClient *target,  void *reference, IOExternalMethodArguments *arguments);
	IOReturn terminateDevice();

    IOMemoryDescriptor * createMemoryDescriptorFromInputArguments(IOExternalMethodArguments * arguments);

public:
	/*! @function initWithTask
		@abstract 
		@discussion 
	*/
	bool initWithTask(task_t owningTask, void * security_id, UInt32 type);


	/*! @function clientClose
		@abstract 
		@discussion 
	*/
	virtual		IOReturn clientClose(void);


	/*! @function getService
		@abstract 
		@discussion 
	*/
	virtual		IOService * getService(void);


	/*! @function externalMethod
		@abstract 
		@discussion 
	*/
	IOReturn	externalMethod(uint32_t selector, IOExternalMethodArguments *arguments,
							   IOExternalMethodDispatch *dispatch, OSObject *target, 
							   void *reference);


	/*! @function start
		@abstract 
		@discussion 
	*/
	virtual bool start(IOService * provider);
};




#endif /* KERNEL */

#endif /* _IOHIDRESOURCE_USERCLIENT_H */

