/*
 * Copyright (c) 1998-2003 Apple Computer, Inc. All rights reserved.
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
#include <IOKit/IOLib.h>	// For IOLog()...

#include <IOKit/usb/IOUSBHIDDriver.h>
#include <IOKit/usb/IOUSBHIDDriverUserClient.h>
#include <IOKit/usb/IOUSBHIDDataQueue.h>

#define super IOUserClient


OSDefineMetaClassAndStructors(IOUSBHIDDriverUserClient, super)


// Called by IOServiceOpen time to start basic initialization
bool IOUSBHIDDriverUserClient::
initWithTask(task_t owningTask, void *security_id, UInt32 type)
{
//    IOLog("IOUSBHIDDriverUserClient: initWithTask()\n");

    if (!super::init())
        return false;

    fClient = owningTask;

    // May not need this, but it doesn't hurt and it protects you from
    // unexpected task termination so that you can clean up properly in 
    // an organised manner.
    task_reference(fClient);

    // Additions from Eric's init() to support reports:
    numReports = 64;
    reportSize = 4;
    
    reportQueue = (IOUSBHIDDataQueue *)0;
    notifyPort = MACH_PORT_NULL;
    
    return true;
}

#if 0	// Eric's original
bool IOUSBHIDDriverUserClient::init(IOUSBHIDDriver *driver, UInt32 numReports, UInt32 reportSize)
{
//    IOLog("IOUSBHIDDriverUserClient: init()\n");
    
    if (!driver) 
    {
        return false;
    }

    fOwner = driver;
    
    IOUSBHIDDriverUserClient::numReports = numReports;	// called w/ 64
    IOUSBHIDDriverUserClient::reportSize = reportSize;	// called w/ 4
    
    reportQueue = (IOUSBHIDDataQueue *)0;
    notifyPort = MACH_PORT_NULL;
    
    return true;
}
#endif


// Start routine, first time that we should allocate resource
// Eric's original had no start().
bool IOUSBHIDDriverUserClient::
start(IOService *provider)
{
//    IOLog("IOUSBHIDDriverUserClient: start()\n");

    if (!super::start(provider))
        return false;

    fOwner = OSDynamicCast(IOUSBHIDDriver, provider);
    if (fOwner && fOwner->open(this))
        return true;  // We got an exclusive open return OK

    // As fOwner is used as flag to close our provider and we haven't
    // opened it so set it to zero before returning a failure.
    fOwner = 0;
    return false;
}


// Note clientClose may be called twice, once by
// IOServiceClose and once by the actual client process dying.
IOReturn IOUSBHIDDriverUserClient::
clientClose(void)
{
//    IOLog("IOUSBHIDDriverUserClient: clientClose()\n");

    // From Eric's version. Shut down reports.
    clientDisconnect();

    // From Godfrey's version. His example was of a custome driver for 
    // Bill's application and it looks like he shuts down the full driver if there
    // is no more user client portion. USB HID driver may need to stay around for
    // other devices, so i'm not so sure i should call fOwner->close() here. -KH
    if (fOwner)
    {							// Have been started so we better detach
        fOwner->close(this);	// Aborts any outstanding I/O getData

        // This should happen automatically but it doesn't yet.
        // be warned this detach will be unnecessary in the future and
        // will probably cause you driver to panic in the future.
        detach(fOwner);
        fOwner = 0;
    }

    // Better release our client task reference
    if (fClient) {
        task_deallocate(fClient);
        fClient = 0;
    }

    return kIOReturnSuccess;
}

#if 0	// Eric's original
IOReturn IOUSBHIDDriverUserClient::clientClose()
{
//    IOLog("IOUSBHIDDriverUserClient: clientClose()\n");
    
    clientDisconnect();

    return kIOReturnSuccess;
}
#endif

// Eric's original. Why don't we do everything that now happens
// in clientClose()? -KH
IOReturn IOUSBHIDDriverUserClient::clientDied()
{
//    IOLog("IOUSBHIDDriverUserClient: clientDied()\n");
    
    clientDisconnect();

    return kIOReturnSuccess;
}


// Eric's original. Shuts down reports.
void IOUSBHIDDriverUserClient::clientDisconnect()
{
#if 0	// IOService
// User Client not called by IOHIDDevice version, so this was never made to compile.
//    IOLog("IOUSBHIDDriverUserClient: clientDisconnect()\n");
    
    if (fOwner && reportQueue) 
    {
        fOwner->removeReportQueue(reportQueue);
        reportQueue->release();
        reportQueue = (IOUSBHIDDataQueue *)0;
    }
#endif    
    return;
}


// Called by IOMapMemory() from user application.
IOReturn IOUSBHIDDriverUserClient::clientMemoryForType(UInt32 type,
                                                  IOOptionBits *options,
                                                  IOMemoryDescriptor **memory)
{
#if 0	// IOService
// User Client not called by IOHIDDevice version, so this was never made to compile.
    IOReturn result = kIOReturnSuccess;

//    IOLog("IOUSBHIDDriverUserClient: clientMemoryForType()\n");
    
    switch (type) 
    {
        case kHIDDriverReportQueue:
            reportQueue = IOUSBHIDDataQueue::withEntries(numReports, reportSize);
            if (reportQueue) 
            {
                IOMemoryDescriptor *memoryDescriptor;
                memoryDescriptor = reportQueue->getMemoryDescriptor();
                if (memoryDescriptor) 
                {
                    *memory = memoryDescriptor;
                    *options = 0;
                    if (notifyPort != MACH_PORT_NULL) 
                    {
                        reportQueue->setNotificationPort(notifyPort);
                        fOwner->addReportQueue(reportQueue);
                    }
                } 
                else 
                {
                    reportQueue->release();
                    reportQueue = (IOUSBHIDDataQueue *)0;
                    result = kIOReturnNoMemory;
                }
            } 
            else 
            {
                result = kIOReturnNoMemory;
            }
            break;
            
        default:
            result = kIOReturnUnsupported;
            break;
    }
                
    return result;
#else	// IOHIDDevice
    return kIOReturnNoMemory;
#endif
}


IOReturn IOUSBHIDDriverUserClient::registerNotificationPort(mach_port_t port,
                                                       UInt32 type,
                                                       UInt32 refCon)
{
#if 0	// IOService
// User Client not called by IOHIDDevice version, so this was never made to compile.

//    IOLog("IOUSBHIDDriverUserClient: registerNotificationPort()\n");
    
    notifyPort = port;
    if (fOwner && reportQueue && (notifyPort != MACH_PORT_NULL)) 
    {
        reportQueue->setNotificationPort(notifyPort);
        fOwner->addReportQueue(reportQueue);
    }
    
    return kIOReturnSuccess;
#else	// IOHIDDevice
    return kIOReturnNoMemory;
#endif
}


// Router method converts a function code to a pointer to an IOExternalMethod
// and also returns what object to send the IOExternalMethod to.
IOExternalMethod *IOUSBHIDDriverUserClient::
getTargetAndMethodForIndex(IOService **target, UInt32 index)
{
//    IOLog("IOUSBHIDDriverUserClient: getTargetAndMethodForIndex() index = %d\n", (int)index);

    if (index < (UInt32)kUSBHIDNumMethods)
    {
        *target = this;
        return &sMethods[index];
    }
    else
        return NULL;
}


// *************************************************************************************
// ************************ HID Driver Dispatch Table Functions ************************
// *************************************************************************************

//////const IOExternalMethod IOUSBHIDDriverUserClient::
IOExternalMethod IOUSBHIDDriverUserClient::
sMethods[kUSBHIDNumMethods] =
{
    { //    kUSBHIDGetReport
        NULL,
        (IOMethod) &IOUSBHIDDriverUserClient::ucGetReport,
        kIOUCScalarIStructO,
        2,
        0xffffffff
    },
    { //    kUSBHIDSetReport
        NULL,
        (IOMethod) &IOUSBHIDDriverUserClient::ucSetReport,
        kIOUCScalarIStructI,
        2,
        0xffffffff
    },
    { //    kUSBHIDGetHIDDescriptor
        NULL,
        (IOMethod) &IOUSBHIDDriverUserClient::ucGetHIDDescriptor,
        kIOUCScalarIStructO,
        2,
        0xffffffff
    },
    { //    kUSBHIDGetVendorID
        NULL,
        (IOMethod) &IOUSBHIDDriverUserClient::ucGetVendorID,
        kIOUCScalarIScalarO,
        0,
        1
    },
    { //    kUSBHIDGetProductID
        NULL,
        (IOMethod) &IOUSBHIDDriverUserClient::ucGetProductID,
        kIOUCScalarIScalarO,
        0,
        1
    },
    { //    kUSBHIDGetVersionNumber
        NULL,
        (IOMethod) &IOUSBHIDDriverUserClient::ucGetVersionNumber,
        kIOUCScalarIScalarO,
        0,
        1
    },
    { //    kUSBHIDGetMaxReportSize
        NULL,
        (IOMethod) &IOUSBHIDDriverUserClient::ucGetMaxReportSize,
        kIOUCScalarIScalarO,
        0,
        1
    },
    { //    kUSBHIDGetManufacturerString
        NULL,
        (IOMethod) &IOUSBHIDDriverUserClient::ucGetManufacturerString,
        kIOUCScalarIStructO,
        1,
        0xffffffff
    },
    { //    kUSBHIDGetProductString
        NULL,
        (IOMethod) &IOUSBHIDDriverUserClient::ucGetProductString,
        kIOUCScalarIStructO,
        1,
        0xffffffff
    },
    { //    kUSBHIDGetSerialNumberString
        NULL,
        (IOMethod) &IOUSBHIDDriverUserClient::ucGetSerialNumberString,
        kIOUCScalarIStructO,
        1,
        0xffffffff
    },
    { //    kUSBHIDGetIndexedString
        NULL,
        (IOMethod) &IOUSBHIDDriverUserClient::ucGetIndexedString,
        kIOUCScalarIStructO,
        2,
        0xffffffff
    },
};


IOReturn IOUSBHIDDriverUserClient::
ucGetReport(UInt32 inReportType, UInt32 inReportID, UInt8 *vInBuf, UInt32 *vInSize, void *, void *)
{
    return fOwner->GetReport(inReportType, inReportID, vInBuf, vInSize);
}

IOReturn IOUSBHIDDriverUserClient::
ucSetReport(UInt32 outReportType, UInt32 outReportID, UInt8 *vOutBuf, UInt32 vOutSize, void *, void *)
{
    return fOwner->SetReport(outReportType, outReportID, vOutBuf, vOutSize);
}

IOReturn IOUSBHIDDriverUserClient::
ucGetHIDDescriptor(UInt32 inDescType, UInt32 inDescIndex, void *vOutBuf, void *vOutSize, void *, void *)
{
    return fOwner->GetHIDDescriptor((UInt8)inDescType, (UInt8)inDescIndex, (UInt8 *)vOutBuf, (UInt32 *)vOutSize);
}

IOReturn IOUSBHIDDriverUserClient::
ucGetVendorID(UInt32 *id, void *, void *, void *, void *, void *)
{
    IOReturn err;
    UInt16 vendID;

    err = fOwner->GetVendorID(&vendID);
    *id = vendID;
    return err;
}

IOReturn IOUSBHIDDriverUserClient::
ucGetProductID(UInt32 *id, void *, void *, void *, void *, void *)
{
    IOReturn err;
    UInt16 prodID;

    err = fOwner->GetProductID(&prodID);
    *id = prodID;
    return err;
}

IOReturn IOUSBHIDDriverUserClient::
ucGetVersionNumber(UInt32 *vers, void *, void *, void *, void *, void *)
{
    IOReturn err;
    UInt16 versNum;

    err = fOwner->GetVersionNumber(&versNum);
    *vers = versNum;
    return err;
}

IOReturn IOUSBHIDDriverUserClient::
ucGetMaxReportSize(UInt32 *size, void *, void *, void *, void *, void *)
{
    IOReturn err;
    UInt32 maxSize;

    err = fOwner->GetMaxReportSize(&maxSize);
    *size = maxSize;
    return err;
}

IOReturn IOUSBHIDDriverUserClient::
ucGetManufacturerString(UInt32 lang, UInt8 *vOutBuf, UInt32 *vOutSize, void *, void *, void *)
{
	// Valid language?
	if (lang >= 0x10000)
	{
        IOLog("     Invalid language selector.\n");
        return kIOReturnBadArgument;
	}

    return fOwner->GetManufacturerString(vOutBuf, vOutSize, (UInt16)lang);
}

IOReturn IOUSBHIDDriverUserClient::
ucGetProductString(UInt32 lang, UInt8 *vOutBuf, UInt32 *vOutSize, void *, void *, void *)
{
	// Valid language?
	if (lang >= 0x10000)
	{
        IOLog("     Invalid language selector.\n");
        return kIOReturnBadArgument;
	}

    return fOwner->GetProductString(vOutBuf, vOutSize, (UInt16)lang);
}

IOReturn IOUSBHIDDriverUserClient::
ucGetSerialNumberString(UInt32 lang, UInt8 *vOutBuf, UInt32 *vOutSize, void *, void *, void *)
{
	// Valid language?
	if (lang >= 0x10000)
	{
        IOLog("     Invalid language selector.\n");
        return kIOReturnBadArgument;
	}

    return fOwner->GetSerialNumberString(vOutBuf, vOutSize, (UInt16)lang);
}

IOReturn IOUSBHIDDriverUserClient::
ucGetIndexedString(UInt32 index, UInt32 lang, UInt8 *vOutBuf, UInt32 *vOutSize, void *, void *)
{
	// Valid string index?
	if (index >= 0x100)
	{
        IOLog("     Invalid string index.\n");
        return kIOReturnBadArgument;
	}

	// Valid language?
	if (lang >= 0x10000)
	{
        IOLog("     Invalid language selector.\n");
        return kIOReturnBadArgument;
	}

    return fOwner->GetIndexedString((UInt8)index, vOutBuf, vOutSize, (UInt16)lang);
}
