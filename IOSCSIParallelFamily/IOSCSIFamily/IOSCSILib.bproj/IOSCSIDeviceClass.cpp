/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
#define CFRUNLOOP_NEW_API 1

#include <CoreFoundation/CFMachPort.h>

#include "IOSCSIDeviceClass.h"
#include "IOCDBCommandClass.h"

#include "IOSCSIUserClient.h"

__BEGIN_DECLS
#include <mach/mach_interface.h>
#include <IOKit/iokitmig.h>
__END_DECLS

#define connectCheck() do {	    \
    if (!fConnection)		    \
	return kIOReturnNoDevice;   \
} while (0)

#define openCheck() do {	    \
    if (!fIsOpen)		    \
        return kIOReturnNotOpen;    \
} while (0)

#define allChecks() do {	    \
    connectCheck();		    \
    openCheck();		    \
} while (0)

IOCFPlugInInterface ** IOSCSIDeviceClass::alloc()
{
    IOSCSIDeviceClass *me;

    me = new IOSCSIDeviceClass;
    if (me)
        return (IOCFPlugInInterface **) &me->iunknown.pseudoVTable;
    else
        return 0;
}

IOSCSIDeviceClass::IOSCSIDeviceClass()
: IOSCSIIUnknown(&sIOCFPlugInInterfaceV1),
  fService(MACH_PORT_NULL),
  fConnection(MACH_PORT_NULL),
  fAsyncPort(MACH_PORT_NULL),
  fIsOpen(false),
  fIsLUNZero(false)
{
    fCDBDevice.pseudoVTable = (IUnknownVTbl *) &sCDBDeviceInterfaceV1;
    fCDBDevice.obj = this;

    fSCSIDevice.pseudoVTable = (IUnknownVTbl *)  &sSCSIDeviceInterfaceV1;
    fSCSIDevice.obj = this;
}

IOSCSIDeviceClass::~IOSCSIDeviceClass()
{
    if (fConnection) {
        IOServiceClose(fConnection);
        fConnection = MACH_PORT_NULL;
    }
        
    if (fService) {
        IOObjectRelease(fService);
        fService = MACH_PORT_NULL;
    }
}

HRESULT IOSCSIDeviceClass::queryInterface(REFIID iid, void **ppv)
{
    CFUUIDRef uuid = CFUUIDCreateFromUUIDBytes(NULL, iid);
    HRESULT res = S_OK;

    if (CFEqual(uuid, kIOCDBCommandInterfaceID)) {
        *ppv = (void *) allocCommand();
    }
    else if (CFEqual(uuid, IUnknownUUID)
         ||  CFEqual(uuid, kIOCFPlugInInterfaceID)) {
        *ppv = &iunknown;
        addRef();
    }
    else if (CFEqual(uuid, kIOCDBDeviceInterfaceID)) {
        *ppv = &fCDBDevice;
        addRef();
    }
    else if (CFEqual(uuid, kIOSCSIDeviceInterfaceID)) {
        *ppv = &fSCSIDevice;
        addRef();
    }
    else
        *ppv = 0;

    if (!*ppv)
        res = E_NOINTERFACE;

    CFRelease(uuid);
    return res;
}

IOReturn IOSCSIDeviceClass::
probe(CFDictionaryRef propertyTable, io_service_t inService, SInt32 *order)
{
    if (!inService || !IOObjectConformsTo(inService, "IOSCSIDevice"))
        return kIOReturnBadArgument;

    return kIOReturnSuccess;
}

IOReturn IOSCSIDeviceClass::
start(CFDictionaryRef propertyTable, io_service_t inService)
{
    IOReturn res;
    CFMutableDictionaryRef entryProperties = 0;
    kern_return_t kr;

    fService = inService;
    res = IOServiceOpen(fService, mach_task_self(), 0, &fConnection);
    if (res != kIOReturnSuccess)
        return res;

    connectCheck();

    kr = IORegistryEntryCreateCFProperties(fService,
                                           &entryProperties,
                                           NULL, 0);
    if (entryProperties) {
        CFTypeRef val;
        val = CFDictionaryGetValue(entryProperties,
                                    CFSTR(kSCSIPropertyLun));
        if (val && CFGetTypeID(val) == CFNumberGetTypeID()) {
            int zeroVal = 0;
            CFNumberRef zero = CFNumberCreate(NULL,
                                              kCFNumberIntType,
                                              &zeroVal);
    
            // @@@ gvdl: Check logical unit zero
            fIsLUNZero = !CFNumberCompare((CFNumberRef) val, zero, 0);
            CFRelease(zero);
        }
        CFRelease(entryProperties);
    }


    return kIOReturnSuccess;
}

IOReturn IOSCSIDeviceClass::
createAsyncEventSource(CFRunLoopSourceRef *source)
{
    IOReturn ret;
    CFMachPortRef cfPort;
    CFMachPortContext context;
    Boolean shouldFreeInfo;

    if (!fAsyncPort) {     
        ret = createAsyncPort(0);
        if (kIOReturnSuccess != ret)
            return ret;
    }

    context.version = 1;
    context.info = this;
    context.retain = NULL;
    context.release = NULL;
    context.copyDescription = NULL;

    cfPort = CFMachPortCreateWithPort(NULL, fAsyncPort,
                (CFMachPortCallBack) IODispatchCalloutFromMessage,
                &context, &shouldFreeInfo);
    if (!cfPort)
        return kIOReturnNoMemory;
    
    fCFSource = CFMachPortCreateRunLoopSource(NULL, cfPort, 0);
    CFRelease(cfPort);
    if (!fCFSource)
        return kIOReturnNoMemory;

    if (source)
        *source = fCFSource;

    return kIOReturnSuccess;
}

CFRunLoopSourceRef IOSCSIDeviceClass::getAsyncEventSource()
{
    return fCFSource;
}

IOReturn IOSCSIDeviceClass::createAsyncPort(mach_port_t *port)
{
    IOReturn ret;

    ret = IOCreateReceivePort(kOSAsyncCompleteMessageID, &fAsyncPort);
    if (kIOReturnSuccess == ret) {
        if (port)
            *port = fAsyncPort;

        if (fIsOpen) {
            natural_t asyncRef[1];
            mach_msg_type_number_t len = 0;
        
            // async kIOCDBUserClientSetAsyncPort,  kIOUCScalarIScalarO,    0,	0
            return io_async_method_structureI_structureO(
                    fConnection, fAsyncPort, asyncRef, 1,
                    kIOCDBUserClientSetAsyncPort, NULL, 0, NULL, &len);
        }
    }

    return ret;
}

mach_port_t IOSCSIDeviceClass::getAsyncPort()
{
    return fAsyncPort;
}

IOReturn IOSCSIDeviceClass::
getInquiryData(void *inquiryBuffer,
               UInt32 inquiryBufSize,
               UInt32 *inquiryDataSize)
{
    connectCheck();

    int args[6], i = 0;

    args[i++] = (int) inquiryBufSize;

    // kIOCDBUserClientGetInquiryData,	kIOUCScalarIStructO,	 1, -1
    return io_connect_method_scalarI_structureO(fConnection,
	kIOCDBUserClientGetInquiryData, args, i,
        (char *) inquiryBuffer, (mach_msg_type_number_t *) inquiryDataSize);
}

IOReturn IOSCSIDeviceClass::open()
{
    IOReturn ret;

    connectCheck();

    if (fIsOpen)
        return kIOReturnSuccess;

    mach_msg_type_number_t len = 0;

    //  kIOCDBUserClientOpen,  kIOUCScalarIScalarO,    0,	0
    ret = io_connect_method_scalarI_scalarO(
            fConnection, kIOCDBUserClientOpen, NULL, 0, NULL, &len);
    if (ret != kIOReturnSuccess)
        return ret;

    fIsOpen = true;

    if (fAsyncPort) {
        natural_t asyncRef[1];
        mach_msg_type_number_t len = 0;
    
        // async 
        // kIOCDBUserClientSetAsyncPort,  kIOUCScalarIScalarO,    0,	0
        ret = io_async_method_scalarI_scalarO(
                fConnection, fAsyncPort, asyncRef, 1,
                kIOCDBUserClientSetAsyncPort, NULL, 0, NULL, &len);
        if (ret != kIOReturnSuccess) {
            close();
            return ret;
        }
    }

    return ret;
}

IOReturn IOSCSIDeviceClass::close()
{
    allChecks();

    IOCDBCommandClass::
	commandDeviceClosing((IOCDBDeviceInterface **) &fCDBDevice); 

    mach_msg_type_number_t len = 0;

    // kIOCDBUserClientClose,	kIOUCScalarIScalarO,	 0,  0
    (void) io_connect_method_scalarI_scalarO(fConnection,
	kIOCDBUserClientClose, NULL, 0, NULL, &len);

    fIsOpen = false;
    fIsLUNZero = false;

    return kIOReturnSuccess;
}

IOCDBCommandInterface **IOSCSIDeviceClass::allocCommand()
{
    // Handover a fCDBDevice interface reference and a fConnection
    if (fIsOpen)
        return IOCDBCommandClass::
            alloc((IOCDBDeviceInterface **) &fCDBDevice, fConnection);
    else 
        return 0;
}

IOReturn IOSCSIDeviceClass::abort()
{
    allChecks();

    mach_msg_type_number_t len = 0;

    // kIOCDBUserClientAbort,	kIOUCScalarIScalarO,	 0,  0
    return io_connect_method_scalarI_scalarO(fConnection,
	kIOCDBUserClientAbort, NULL, 0, NULL, &len);
}

IOReturn IOSCSIDeviceClass::reset()
{
    allChecks();

    mach_msg_type_number_t len = 0;

    // kIOCDBUserClientReset,	kIOUCScalarIScalarO,	 0,  0
    return io_connect_method_scalarI_scalarO(fConnection,
	kIOCDBUserClientReset, NULL, 0, NULL, &len);
}

IOReturn IOSCSIDeviceClass::holdQueue(UInt32 queueType)
{
    allChecks();

    mach_msg_type_number_t len = 0;
    int args[6], i = 0;

    args[i++] = (int) queueType;

    // kIOSCSIUserClientHoldQueue,  kIOUCScalarIScalarO,     1,	 0
    return io_connect_method_scalarI_scalarO(fConnection,
	kIOSCSIUserClientHoldQueue, args, i, NULL, &len);
}

IOReturn IOSCSIDeviceClass::releaseQueue(UInt32 queueType)
{
    allChecks();

    mach_msg_type_number_t len = 0;
    int args[6], i = 0;

    args[i++] = (int) queueType;

    // kIOSCSIUserClientReleaseQueue,	kIOUCScalarIScalarO,	 1,  0
    return io_connect_method_scalarI_scalarO(fConnection,
	kIOSCSIUserClientReleaseQueue, args, i, NULL, &len);
}

IOReturn IOSCSIDeviceClass::flushQueue(UInt32 queueType, IOReturn rc)
{
    allChecks();

    mach_msg_type_number_t len = 0;
    int args[6], i = 0;

    args[i++] = (int) queueType;
    args[i++] = (int) rc;

    // kIOSCSIUserClientFlushQueue, kIOUCScalarIScalarO,     2,	 0
    return io_connect_method_scalarI_scalarO(fConnection,
	kIOSCSIUserClientFlushQueue, args, i, NULL, &len);
}

IOReturn IOSCSIDeviceClass::setTargetParms(SCSITargetParms *targetParms)
{
    allChecks();

    mach_msg_type_number_t len = 0;

    // kIOSCSIUserClientSetTargetParms, kIOUCStructIStructO,	 n,  0
    return io_connect_method_structureI_structureO(fConnection,
	kIOSCSIUserClientSetTargetParms,
	(char *) targetParms, sizeof(SCSITargetParms), NULL, &len);
}

IOReturn IOSCSIDeviceClass::getTargetParms(SCSITargetParms *targetParms)
{
    allChecks();

    mach_msg_type_number_t len = sizeof(SCSITargetParms);

    // kIOSCSIUserClientGetTargetParms, kIOUCStructIStructO,	 0,  n
    return io_connect_method_structureI_structureO(fConnection,
	kIOSCSIUserClientGetTargetParms,
	NULL, 0, (char *) targetParms, &len);
}

IOReturn IOSCSIDeviceClass::setLunParms(SCSILunParms *lunParms)
{
    allChecks();

    mach_msg_type_number_t len = 0;

    // kIOSCSIUserClientSetLunParms,	kIOUCStructIStructO,	 n,  0
    return io_connect_method_structureI_structureO(fConnection,
	kIOSCSIUserClientSetLunParms,
	(char *) lunParms, sizeof(SCSILunParms), NULL, &len);
}

IOReturn IOSCSIDeviceClass::getLunParms(SCSILunParms *lunParms)
{
    allChecks();

    mach_msg_type_number_t len = sizeof(SCSILunParms);

    // kIOSCSIUserClientGetLunParms,	kIOUCStructIStructO,	 0,  n
    return io_connect_method_structureI_structureO(fConnection,
	kIOSCSIUserClientGetLunParms,
	NULL, 0, (char *) lunParms, &len);
}

IOReturn IOSCSIDeviceClass::
notifyIdle(void *target, IOCDBCallbackFunction callback, void *refcon)
{
    allChecks();

    mach_msg_type_number_t len = 0;
    int args[6], i = 0;

    args[i++] = (int) target;
    args[i++] = (int) callback;
    args[i++] = (int) refcon;

    // kIOSCSIUserClientNotifyIdle, kIOUCScalarIScalarO,     3,	 0
    return io_connect_method_scalarI_scalarO(fConnection,
	kIOSCSIUserClientNotifyIdle, args, i, NULL, &len);
}

IOCFPlugInInterface IOSCSIDeviceClass::sIOCFPlugInInterfaceV1 = {
    0,
    &IOSCSIIUnknown::genericQueryInterface,
    &IOSCSIIUnknown::genericAddRef,
    &IOSCSIIUnknown::genericRelease,
    1, 0,	// version/revision
    &IOSCSIDeviceClass::deviceProbe,
    &IOSCSIDeviceClass::deviceStart,
    &IOSCSIDeviceClass::deviceClose
};

IOCDBDeviceInterface IOSCSIDeviceClass::sCDBDeviceInterfaceV1 = {
    0,
    &IOSCSIIUnknown::genericQueryInterface,
    &IOSCSIIUnknown::genericAddRef,
    &IOSCSIIUnknown::genericRelease,
    &IOSCSIDeviceClass::deviceCreateAsyncEventSource,
    &IOSCSIDeviceClass::deviceGetAsyncEventSource,
    &IOSCSIDeviceClass::deviceCreateAsyncPort,
    &IOSCSIDeviceClass::deviceGetAsyncPort,
    &IOSCSIDeviceClass::deviceGetInquiryData,
    &IOSCSIDeviceClass::deviceOpen,
    &IOSCSIDeviceClass::deviceClose,
    &IOSCSIDeviceClass::deviceAllocCommand,
    &IOSCSIDeviceClass::deviceAbort,
    &IOSCSIDeviceClass::deviceReset
};

IOSCSIDeviceStruct IOSCSIDeviceClass::sSCSIDeviceInterfaceV1 = {
    0,
    &IOSCSIIUnknown::genericQueryInterface,
    &IOSCSIIUnknown::genericAddRef,
    &IOSCSIIUnknown::genericRelease,
    &IOSCSIDeviceClass::deviceCreateAsyncEventSource,
    &IOSCSIDeviceClass::deviceGetAsyncEventSource,
    &IOSCSIDeviceClass::deviceCreateAsyncPort,
    &IOSCSIDeviceClass::deviceGetAsyncPort,
    &IOSCSIDeviceClass::deviceGetInquiryData,
    &IOSCSIDeviceClass::deviceOpen,
    &IOSCSIDeviceClass::deviceClose,
    &IOSCSIDeviceClass::deviceAllocCommand,
    &IOSCSIDeviceClass::deviceAbort,
    &IOSCSIDeviceClass::deviceReset,
    &IOSCSIDeviceClass::deviceHoldQueue,
    &IOSCSIDeviceClass::deviceReleaseQueue,
    &IOSCSIDeviceClass::deviceFlushQueue,
    &IOSCSIDeviceClass::deviceNotifyIdle,
    &IOSCSIDeviceClass::deviceSetTargetParms,
    &IOSCSIDeviceClass::deviceGetTargetParms,
    &IOSCSIDeviceClass::deviceSetLunParms,
    &IOSCSIDeviceClass::deviceGetLunParms
};

// Methods for routing iocfplugin interface
IOReturn IOSCSIDeviceClass::
deviceProbe(void *self,
            CFDictionaryRef propertyTable,
            io_service_t inService, SInt32 *order)
    { return getThis(self)->probe(propertyTable, inService, order); }

IOReturn IOSCSIDeviceClass::deviceStart(void *self,
                                            CFDictionaryRef propertyTable,
                                            io_service_t inService)
    { return getThis(self)->start(propertyTable, inService); }

IOReturn IOSCSIDeviceClass::deviceStop(void *self)
    { return getThis(self)->close(); }

// Methods for routing asynchronous completion plumbing.
IOReturn IOSCSIDeviceClass::
deviceCreateAsyncEventSource(void *self, CFRunLoopSourceRef *source)
    { return getThis(self)->createAsyncEventSource(source); }

CFRunLoopSourceRef IOSCSIDeviceClass::
deviceGetAsyncEventSource(void *self)
    { return getThis(self)->getAsyncEventSource(); }

IOReturn IOSCSIDeviceClass::
deviceCreateAsyncPort(void *self, mach_port_t *port)
    { return getThis(self)->createAsyncPort(port); }

mach_port_t IOSCSIDeviceClass::
deviceGetAsyncPort(void *self)
    { return getThis(self)->getAsyncPort(); }

// Methods for routing cdb and scsi device interfaces
IOReturn IOSCSIDeviceClass::
deviceGetInquiryData(void *self, 
                     void *inquiryBuffer,
                     UInt32 inquiryBufSize,
                     UInt32 *inquiryDataSize)
{
    return getThis(self)->getInquiryData(inquiryBuffer,
                                         inquiryBufSize,
                                         inquiryDataSize);
}

IOReturn IOSCSIDeviceClass::deviceOpen(void *self)
    { return getThis(self)->open(); }

IOReturn IOSCSIDeviceClass::deviceClose(void *self)
    { return getThis(self)->close(); }

IOCDBCommandInterface **IOSCSIDeviceClass::deviceAllocCommand(void *self)
    { return getThis(self)->allocCommand(); }

IOReturn IOSCSIDeviceClass::deviceAbort(void *self)
    { return getThis(self)->abort(); }

IOReturn IOSCSIDeviceClass::deviceReset(void *self)
    { return getThis(self)->reset(); }

// Methods for routing the extra methods in scsi device interface
IOReturn IOSCSIDeviceClass::deviceHoldQueue(void *self, UInt32 queueType)
    { return getThis(self)->holdQueue(queueType); }

IOReturn IOSCSIDeviceClass::deviceReleaseQueue(void *self, UInt32 queueType)
    { return getThis(self)->releaseQueue(queueType); }

IOReturn IOSCSIDeviceClass::
deviceFlushQueue(void *self, UInt32 queueType, IOReturn rc)
    { return getThis(self)->flushQueue(queueType, rc); }

IOReturn IOSCSIDeviceClass::
deviceNotifyIdle(void *self,
                 void *target, IOCDBCallbackFunction callback, void *refcon)
    { return getThis(self)->notifyIdle(target, callback, refcon); }

IOReturn IOSCSIDeviceClass::
deviceSetTargetParms(void *self, SCSITargetParms *targetParms)
    { return getThis(self)->setTargetParms(targetParms); }

IOReturn IOSCSIDeviceClass::
deviceGetTargetParms(void *self, SCSITargetParms *targetParms)
    { return getThis(self)->getTargetParms(targetParms); }

IOReturn IOSCSIDeviceClass::
deviceSetLunParms(void *self, SCSILunParms *lunParms)
    { return getThis(self)->setLunParms(lunParms); }

IOReturn IOSCSIDeviceClass::
deviceGetLunParms(void *self, SCSILunParms *lunParms)
    { return getThis(self)->getLunParms(lunParms); }
