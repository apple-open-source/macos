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
#ifndef _IOKIT_IOSCSIDeviceClass_H
#define _IOKIT_IOSCSIDeviceClass_H

#include <IOKit/cdb/IOCDBLib.h>
#include <IOKit/cdb/IOSCSILib.h>

#include "IOSCSIIUnknown.h"

class IOSCSIDeviceClass : public IOSCSIIUnknown
{
private:
    // Disable copy constructors
    IOSCSIDeviceClass(IOSCSIDeviceClass &src);
    void operator =(IOSCSIDeviceClass &src);

protected:
    IOSCSIDeviceClass();
    virtual ~IOSCSIDeviceClass();

    enum constants {
        kMaxPoolSize = 8
    };

    static IOCFPlugInInterface sIOCFPlugInInterfaceV1;
    static IOCDBDeviceInterface   sCDBDeviceInterfaceV1;
    static IOSCSIDeviceInterface  sSCSIDeviceInterfaceV1;

    struct InterfaceMap fCDBDevice;
    struct InterfaceMap fSCSIDevice;
    io_service_t fService;
    io_connect_t fConnection;
    mach_port_t fAsyncPort;
    CFRunLoopSourceRef fCFSource;
    bool fIsOpen;
    bool fIsLUNZero;
    
public:
    static IOCFPlugInInterface **alloc();

    virtual HRESULT queryInterface(REFIID iid, void **ppv);

    virtual IOReturn probe(CFDictionaryRef propertyTable,
                           io_service_t service, SInt32 *order);
    virtual IOReturn start(CFDictionaryRef propertyTable,
                           io_service_t service);
    // No stop as such just map the deviceStop call onto close.

    virtual IOReturn createAsyncEventSource(CFRunLoopSourceRef *source);
    virtual CFRunLoopSourceRef getAsyncEventSource();

    virtual IOReturn createAsyncPort(mach_port_t *port);
    virtual mach_port_t getAsyncPort();

    virtual IOReturn getInquiryData(void *inquiryBuffer,
                                UInt32 inquiryBufSize,
                                UInt32 *inquiryDataSize);
    virtual IOReturn open();
    virtual IOReturn close();

    virtual IOCDBCommandStruct **allocCommand();
    virtual IOReturn abort();
    virtual IOReturn reset();

    // Extra scsi device specifc functionality
    virtual IOReturn holdQueue(UInt32 queueType);
    virtual IOReturn releaseQueue(UInt32 queueType);
    virtual IOReturn flushQueue(UInt32 queueType, IOReturn rc);
    virtual IOReturn
        notifyIdle(void *target, IOCDBCallbackFunction callback, void *refcon);
    virtual IOReturn setTargetParms(SCSITargetParms *targetParms);
    virtual IOReturn getTargetParms(SCSITargetParms *targetParms);
    virtual IOReturn setLunParms(SCSILunParms *lunParms);
    virtual IOReturn getLunParms(SCSILunParms *lunParms);

/*
 * Routing gumf for CFPlugIn interfaces
 */
protected:

    static inline IOSCSIDeviceClass *getThis(void *self)
        { return (IOSCSIDeviceClass *) ((InterfaceMap *) self)->obj; };

    // Methods for routing the iocfplugin Interface v1r1
    static IOReturn deviceProbe(void *self,
                                CFDictionaryRef propertyTable,
                                io_service_t service, SInt32 *order);

    static IOReturn deviceStart(void *self,
                                CFDictionaryRef propertyTable,
                                io_service_t service);

    static IOReturn deviceStop(void *self);	// Calls close()

    // Methods for routing asynchronous completion plumbing.
    static IOReturn deviceCreateAsyncEventSource(void *self,
                                                 CFRunLoopSourceRef *source);
    static CFRunLoopSourceRef deviceGetAsyncEventSource(void *self);
    static IOReturn deviceCreateAsyncPort(void *self, mach_port_t *port);
    static mach_port_t deviceGetAsyncPort(void *self);

    // Methods for routing the cdb and scsi device interfaces v1
    static IOReturn deviceGetInquiryData(void *self, 
        void *inquiryBuffer,
        UInt32 inquiryBufSize,
        UInt32 *inquiryDataSize);
    static IOReturn deviceOpen(void *self);
    static IOReturn deviceClose(void *self);
    static IOCDBCommandStruct **deviceAllocCommand(void *self);
    static IOReturn deviceAbort(void *self);
    static IOReturn deviceReset(void *self);

    // Methods for routing the extra scsi device methods v1
    static IOReturn deviceHoldQueue(void *self, UInt32 queueType);
    static IOReturn deviceReleaseQueue(void *self, UInt32 queueType);
    static IOReturn deviceFlushQueue(void *self, UInt32 queueType, IOReturn rc);
    static IOReturn deviceNotifyIdle(void *self,
                void *target, IOCDBCallbackFunction callback, void *refcon);
    static IOReturn deviceSetTargetParms(void *self, SCSITargetParms *targetParms);
    static IOReturn deviceGetTargetParms(void *self, SCSITargetParms *targetParms);
    static IOReturn deviceSetLunParms(void *self, SCSILunParms *lunParms);
    static IOReturn deviceGetLunParms(void *self, SCSILunParms *lunParms);
};

#endif /* !_IOKIT_IOSCSIDeviceClass_H */
