/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
#ifndef IOUSBHIDDRIVERUSERCLIENT_H
#define IOUSBHIDDRIVERUSERCLIENT_H

#include <mach/port.h>

#include <IOKit/IOUserClient.h>

#include <IOKit/usb/IOUSBHIDDriver.h>
#include <IOKit/usb/IOUSBHIDSelector.h>

enum
{
    kHIDDriverReportQueue = 0
};


class IOUSBHIDDataQueue;
class IOUSBHIDDriver;


class IOUSBHIDDriverUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOUSBHIDDriverUserClient)

protected:
    static /*const*/ IOExternalMethod
	sMethods[kUSBHIDNumMethods];
//    IOExternalAsyncMethod fAsyncMethods[kUSBHIDNumMethods];

    task_t fClient;
	IOUSBHIDDriver * fOwner;

    // Additions from Eric's init() to support reports:
	UInt32				reportSize;
	UInt32				numReports;
	mach_port_t			notifyPort;
	IOUSBHIDDataQueue *	reportQueue;

    virtual bool initWithTask(task_t owningTask, void *security_id, UInt32 type);
    virtual bool start(IOService *provider);

//// What? No clientOpened?
    virtual IOReturn clientClose(void);
	virtual IOReturn clientDied();
	virtual void clientDisconnect();
    
	virtual IOReturn clientMemoryForType(UInt32 type,
                                        IOOptionBits *options,
                                        IOMemoryDescriptor **memory);

	virtual IOReturn registerNotificationPort(mach_port_t port,
                                            UInt32 type,
                                            UInt32 refCon);
    
    virtual IOExternalMethod * getTargetAndMethodForIndex(IOService **target, UInt32 index);

    // Calls Specific To This User Client
	virtual IOReturn ucGetReport(UInt32 inReportType, UInt32 inReportID, 
                            UInt8 *vInBuf, UInt32 *vInSize, void *, void *);
	virtual IOReturn ucSetReport(UInt32 outReportType, UInt32 outReportID, 
                            UInt8 *vOutBuf, UInt32 vOutSize, void *, void *);
	virtual IOReturn ucGetHIDDescriptor(UInt32 inDescType, UInt32 inDescIndex, 
                            void *vOutBuf, void *vOutSize, void *, void *);
	virtual IOReturn ucGetVendorID(UInt32 *id, void *,
                            void *, void *, void *, void *);
	virtual IOReturn ucGetProductID(UInt32 *idf, void *,
                            void *, void *, void *, void *);
	virtual IOReturn ucGetVersionNumber(UInt32 *vers, void *,
                            void *, void *, void *, void *);
	virtual IOReturn ucGetMaxReportSize(UInt32 *size, void *,
                            void *, void *, void *, void *);
	virtual IOReturn ucGetManufacturerString(UInt32 lang, UInt8 *vOutBuf, 
                            UInt32 *vOutSize, void *, void *, void *);
	virtual IOReturn ucGetProductString(UInt32 lang, UInt8 *vOutBuf, 
                            UInt32 *vOutSize, void *, void *, void *);
	virtual IOReturn ucGetSerialNumberString(UInt32 lang, UInt8 *vOutBuf, 
                            UInt32 *vOutSize, void *, void *, void *);
	virtual IOReturn ucGetIndexedString(UInt32 index, UInt32 lang, 
                            UInt8 *vOutBuf, UInt32 *vOutSize, void *, void *);
};

#endif /* IOUSBHIDDRIVERUSERCLIENT_H */