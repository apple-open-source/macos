/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.2 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.  
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#ifndef _IOKIT_IOUSBInterfaceClass_H
#define _IOKIT_IOUSBInterfaceClass_H

#include <IOKit/usb/IOUSBLib.h>

#include "IOUSBIUnknown.h"

class IOUSBInterfaceClass : public IOUSBIUnknown
{
private:
    // Disable copy constructors
    IOUSBInterfaceClass(IOUSBInterfaceClass &src);
    void operator =(IOUSBInterfaceClass &src);

protected:
    IOUSBInterfaceClass();
    virtual ~IOUSBInterfaceClass();

    enum constants {
        kMaxPoolSize = 8
    };

    static IOCFPlugInInterface 		sIOCFPlugInInterfaceV1;
    static IOUSBInterfaceInterface192  	sUSBInterfaceInterfaceV192;

    struct InterfaceMap 	fUSBInterface;
    io_service_t 		fService;
    io_service_t		fDevice;
    io_connect_t 		fConnection;
    mach_port_t 		fAsyncPort;
    CFRunLoopSourceRef 		fCFSource;
    bool 			fIsOpen;
    UInt8			fClass;
    UInt8			fSubClass;
    UInt8			fProtocol;
    UInt8			fConfigValue;
    UInt8			fInterfaceNumber;
    UInt8			fAlternateSetting;
    UInt8			fNumEndpoints;
    UInt8			fStringIndex;
    // these fields are actually in the device, but are repeated here because they are useful
    // and you don't need to access the device to get them.
    UInt16			fVendor;
    UInt16			fProduct;
    UInt16			fDeviceReleaseNumber;
    UInt32			fLocationID;
    
    
public:
    static IOCFPlugInInterface **alloc();

    virtual HRESULT queryInterface(REFIID iid, void **ppv);

    virtual IOReturn probe(CFDictionaryRef propertyTable,
                           io_service_t service, SInt32 *order);
    virtual IOReturn start(CFDictionaryRef propertyTable,
                           io_service_t service);
    virtual IOReturn CreateInterfaceAsyncEventSource(CFRunLoopSourceRef *source);
    virtual CFRunLoopSourceRef GetInterfaceAsyncEventSource();
    virtual IOReturn CreateInterfaceAsyncPort(mach_port_t *port);
    virtual mach_port_t GetInterfaceAsyncPort();
    virtual IOReturn USBInterfaceOpen(bool seize);
    virtual IOReturn USBInterfaceClose();
    virtual IOReturn GetInterfaceClass(UInt8 *intfClass);
    virtual IOReturn GetInterfaceSubClass(UInt8 *intfSubClass);
    virtual IOReturn GetInterfaceProtocol(UInt8 *intfProtocol);
    virtual IOReturn GetDeviceVendor(UInt16 *devVendor);
    virtual IOReturn GetDeviceProduct(UInt16 *devProduct);
    virtual IOReturn GetDeviceReleaseNumber(UInt16 *devRelNum);
    virtual IOReturn GetConfigurationValue(UInt8 *configVal);
    virtual IOReturn GetInterfaceNumber(UInt8 *intfNumber);
    virtual IOReturn GetAlternateSetting(UInt8 *intfAlternateSetting);
    virtual IOReturn GetNumEndpoints(UInt8 *intfNumEndpoints);
    virtual IOReturn GetLocationID(UInt32 *locationID);
    virtual IOReturn GetDevice(io_service_t *device);
    virtual IOReturn SetAlternateInterface(UInt8 alternateSetting);
    virtual IOReturn GetBusFrameNumber(UInt64 *frame, AbsoluteTime *atTime);
    virtual IOReturn GetBandwidthAvailable(UInt32 *bandwidth);
    virtual IOReturn GetEndpointProperties(UInt8 alternateSetting, UInt8 endpointNumber, UInt8 direction, UInt8 *transferType, UInt16 *maxPacketSize, UInt8 *interval);
    virtual IOReturn ControlRequest(UInt8 pipeRef, IOUSBDevRequestTO *req);
    virtual IOReturn ControlRequestAsync(UInt8 pipeRef, IOUSBDevRequestTO *req, IOAsyncCallback1 callback, void *refCon);
    virtual IOReturn GetPipeProperties(UInt8 pipeRef, UInt8 *direction, UInt8 *address, UInt8 *attributes, UInt16 *maxpacketSize, UInt8 *interval);
    virtual IOReturn GetPipeStatus(UInt8 pipeRef);
    virtual IOReturn AbortPipe(UInt8 pipeRef);
    virtual IOReturn ResetPipe(UInt8 pipeRef);
    virtual IOReturn ClearPipeStall(UInt8 pipeRef, bool bothEnds);
    virtual IOReturn SetPipePolicy(UInt8 pipeRef, UInt16 maxPacketSize, UInt8 maxInterval);
    virtual IOReturn ReadPipe(UInt8 pipeRef, void *buf, UInt32 *size, UInt32 noDataTimeout, UInt32 completionTimeout);
    virtual IOReturn WritePipe(UInt8 pipeRef, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout);
    virtual IOReturn ReadPipeAsync(UInt8 pipeRef, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout, IOAsyncCallback1 callback, void *refcon);
    virtual IOReturn WritePipeAsync(UInt8 pipeRef, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout, IOAsyncCallback1 callback, void *refcon);
    virtual IOReturn ReadIsochPipeAsync(UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, IOUSBIsocFrame *frameList,
                                  IOAsyncCallback1 callback, void *refcon);
    virtual IOReturn WriteIsochPipeAsync(UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, IOUSBIsocFrame *frameList,
                                  IOAsyncCallback1 callback, void *refcon);
    virtual IOReturn LowLatencyReadIsochPipeAsync(UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, UInt32 updateFrequency, IOUSBLowLatencyIsocFrame *frameList, IOAsyncCallback1 callback, void *refcon);
    virtual IOReturn LowLatencyWriteIsochPipeAsync(UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, UInt32 updateFrequency, IOUSBLowLatencyIsocFrame *frameList, IOAsyncCallback1 callback, void *refcon);
    virtual IOReturn GetInterfaceStringIndex(UInt8 *intfSI);
				  
private:
    IOReturn GetPropertyInfo(void);

/*
 * Routing gumf for CFPlugIn interfaces
 */
protected:

    static inline IOUSBInterfaceClass *getThis(void *self)
        { return (IOUSBInterfaceClass *) ((InterfaceMap *) self)->obj; };

    // Methods for routing the iocfplugin Interface v1r1
    static IOReturn interfaceProbe(void *self,
                                CFDictionaryRef propertyTable,
                                io_service_t service, SInt32 *order);

    static IOReturn interfaceStart(void *self,
                                CFDictionaryRef propertyTable,
                                io_service_t service);

    static IOReturn interfaceStop(void *self);	// Calls close()

    static IOReturn interfaceCreateInterfaceAsyncEventSource(void *self, CFRunLoopSourceRef *source);
    static CFRunLoopSourceRef interfaceGetInterfaceAsyncEventSource(void *self);
    static IOReturn interfaceCreateInterfaceAsyncPort(void *self, mach_port_t *port);
    static mach_port_t interfaceGetInterfaceAsyncPort(void *self);
    static IOReturn interfaceUSBInterfaceOpen(void *self);
    static IOReturn interfaceUSBInterfaceClose(void *self);
    static IOReturn interfaceGetInterfaceClass(void *self, UInt8 *devClass);
    static IOReturn interfaceGetInterfaceSubClass(void *self, UInt8 *devSubClass);
    static IOReturn interfaceGetInterfaceProtocol(void *self, UInt8 *devProtocol);
    static IOReturn interfaceGetDeviceVendor(void *self, UInt16 *devVendor);
    static IOReturn interfaceGetDeviceProduct(void *self, UInt16 *devProduct);
    static IOReturn interfaceGetDeviceReleaseNumber(void *self, UInt16 *devRelNum);
    static IOReturn interfaceGetConfigurationValue(void *self, UInt8 *configVal);
    static IOReturn interfaceGetInterfaceNumber(void *self, UInt8 *intfNumber);
    static IOReturn interfaceGetAlternateSetting(void *self, UInt8 *intfAlternateSetting);
    static IOReturn interfaceGetNumEndpoints(void *self, UInt8 *intfNumEndpoints);
    static IOReturn interfaceGetLocationID(void *self, UInt32 *locationID);
    static IOReturn interfaceGetDevice(void *self, io_service_t *device);
    static IOReturn interfaceSetAlternateInterface(void *self, UInt8 alternateSetting);
    static IOReturn interfaceGetBusFrameNumber(void *self, UInt64 *frame, AbsoluteTime *atTime);
    static IOReturn interfaceControlRequest(void *self, UInt8 pipeRef, IOUSBDevRequest *req);
    static IOReturn interfaceControlRequestAsync(void *self, UInt8 pipeRef, IOUSBDevRequest *req, IOAsyncCallback1 callback, void *refCon);
    static IOReturn interfaceGetPipeProperties(void *self, UInt8 pipeRef, UInt8 *direction, UInt8 *address, UInt8 *attributes, 
                                    UInt16 *maxpacketSize, UInt8 *interval);
    static IOReturn interfaceGetPipeStatus(void *self, UInt8 pipeRef);
    static IOReturn interfaceAbortPipe(void *self, UInt8 pipeRef);
    static IOReturn interfaceResetPipe(void *self, UInt8 pipeRef);
    static IOReturn interfaceClearPipeStall(void *self, UInt8 pipeRef);
    static IOReturn interfaceReadPipe(void *self, UInt8 pipeRef, void *buf, UInt32 *size);
    static IOReturn interfaceWritePipe(void *self, UInt8 pipeRef, void *buf, UInt32 size);
    static IOReturn interfaceReadPipeAsync(void *self, UInt8 pipeRef, void *buf, UInt32 size, IOAsyncCallback1 callback, void *refcon);
    static IOReturn interfaceWritePipeAsync(void *self, UInt8 pipeRef, void *buf, UInt32 size, IOAsyncCallback1 callback, void *refcon);
    static IOReturn interfaceReadIsochPipeAsync(void *self, UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, IOUSBIsocFrame *frameList,
                                  IOAsyncCallback1 callback, void *refcon);
    static IOReturn interfaceWriteIsochPipeAsync(void *self, UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, IOUSBIsocFrame *frameList,
                                  IOAsyncCallback1 callback, void *refcon);
    // ----------------- added in 1.8.2
    static IOReturn interfaceControlRequestTO(void *self, UInt8 pipeRef, IOUSBDevRequestTO *req);
    static IOReturn interfaceControlRequestAsyncTO(void *self, UInt8 pipeRef, IOUSBDevRequestTO *req, IOAsyncCallback1 callback, void *refCon);
    static IOReturn interfaceReadPipeTO(void *self, UInt8 pipeRef, void *buf, UInt32 *size, UInt32 noDataTimeout, UInt32 completionTimeout);
    static IOReturn interfaceWritePipeTO(void *self, UInt8 pipeRef, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout);
    static IOReturn interfaceReadPipeAsyncTO(void *self, UInt8 pipeRef, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout, IOAsyncCallback1 callback, void *refcon);
    static IOReturn interfaceWritePipeAsyncTO(void *self, UInt8 pipeRef, void *buf, UInt32 size, UInt32 noDataTimeout, UInt32 completionTimeout, IOAsyncCallback1 callback, void *refcon);
    static IOReturn interfaceGetInterfaceStringIndex(void *self, UInt8 *intfSI);
    // ----------------- added in 1.8.3
    static IOReturn interfaceUSBInterfaceOpenSeize(void *self);
    // ----------------- added in 1.9.0
    static IOReturn interfaceClearPipeStallBothEnds(void *self, UInt8 pipeRef);
    static IOReturn interfaceSetPipePolicy(void *self, UInt8 pipeRef, UInt16 maxPacketSize, UInt8 maxInterval);
    static IOReturn interfaceGetBandwidthAvailable(void *self, UInt32 *bandwidth);
    static IOReturn interfaceGetEndpointProperties(void *self, UInt8 alternateSetting, UInt8 endpointNumber, UInt8 direction, UInt8 *transferType, UInt16 *maxPacketSize, UInt8 *interval);
    // ----------------- added in 1.9.2
    static IOReturn interfaceLowLatencyReadIsochPipeAsync(void *self, UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames,  
                                    UInt32 updateFrequency, IOUSBLowLatencyIsocFrame *frameList, IOAsyncCallback1 callback, void *refcon);
    static IOReturn interfaceLowLatencyWriteIsochPipeAsync(void *self, UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames,
                                  UInt32 updateFrequency, IOUSBLowLatencyIsocFrame *frameList, IOAsyncCallback1 callback, void *refcon);
   
};

#endif /* !_IOKIT_IOUSBInterfaceClass_H */
