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
#ifndef _IOKIT_IOUSBDeviceClass_H
#define _IOKIT_IOUSBDeviceClass_H

#include <IOKit/usb/IOUSBLib.h>

#include "IOUSBIUnknown.h"

class IOUSBDeviceClass : public IOUSBIUnknown
{
private:
    // Disable copy constructors
    IOUSBDeviceClass(IOUSBDeviceClass &src);
    void operator =(IOUSBDeviceClass &src);

protected:
    IOUSBDeviceClass();
    virtual ~IOUSBDeviceClass();

    enum constants {
        kMaxPoolSize = 8
    };

    static IOCFPlugInInterface 		sIOCFPlugInInterfaceV1;
    static IOUSBDeviceInterface187  	sUSBDeviceInterfaceV187;

    struct InterfaceMap 		fUSBDevice;
    io_service_t 			fService;
    io_connect_t 			fConnection;
    mach_port_t 			fAsyncPort;
    CFRunLoopSourceRef 			fCFSource;
    bool 				fIsOpen;
    UInt8				fClass;
    UInt8				fSubClass;
    UInt8				fProtocol;
    UInt16				fVendor;
    UInt16				fProduct;
    UInt16				fDeviceReleaseNumber;
    UInt8				fManufacturerStringIndex;
    UInt8				fProductStringIndex;
    UInt8				fSerialNumberStringIndex;
    UInt8				fNumConfigurations;
    USBDeviceAddress			fAddress;
    UInt32				fPowerAvail;
    UInt8				fSpeed;
    UInt32				fLocationID;
    IOUSBConfigurationDescriptorPtr	*fConfigurations;
    bool				fConfigDescCacheValid;
    
public:
    static IOCFPlugInInterface **alloc();

    virtual HRESULT queryInterface(REFIID iid, void **ppv);

    virtual IOReturn probe(CFDictionaryRef propertyTable,
                           io_service_t service, SInt32 *order);
    virtual IOReturn start(CFDictionaryRef propertyTable,
                           io_service_t service);
    // No stop as such just map the deviceStop call onto close.
    virtual IOReturn CreateDeviceAsyncEventSource(CFRunLoopSourceRef *source);
    virtual CFRunLoopSourceRef GetDeviceAsyncEventSource(void);
    virtual IOReturn CreateDeviceAsyncPort(mach_port_t *port);
    virtual mach_port_t GetDeviceAsyncPort(void);
    virtual IOReturn USBDeviceOpen(bool seize);
    virtual IOReturn USBDeviceClose(void);
    virtual IOReturn GetDeviceClass(UInt8 *devClass);
    virtual IOReturn GetDeviceSubClass(UInt8 *devSubClass);
    virtual IOReturn GetDeviceProtocol(UInt8 *devProtocol);
    virtual IOReturn GetDeviceVendor(UInt16 *devVendor);
    virtual IOReturn GetDeviceProduct(UInt16 *devProduct);
    virtual IOReturn GetDeviceReleaseNumber(UInt16 *devRelNum);
    virtual IOReturn GetDeviceAddress(USBDeviceAddress *addr);
    virtual IOReturn GetDeviceBusPowerAvailable(UInt32 *powerAvailable);
    virtual IOReturn GetDeviceSpeed(UInt8 *devSpeed);
    virtual IOReturn GetNumberOfConfigurations(UInt8 *numConfig);
    virtual IOReturn GetLocationID(UInt32 *locationID);
    virtual IOReturn GetConfigurationDescriptorPtr(UInt8 configIndex, IOUSBConfigurationDescriptorPtr *desc);
    virtual IOReturn GetConfiguration(UInt8 *configNum);
    virtual IOReturn SetConfiguration(UInt8 configNum);
    virtual IOReturn GetBusFrameNumber(UInt64 *frame, AbsoluteTime *atTime);
    virtual IOReturn ResetDevice(void);
    virtual IOReturn DeviceRequest(IOUSBDevRequestTO *req);
    virtual IOReturn DeviceRequestAsync(IOUSBDevRequestTO *req, IOAsyncCallback1 callback, void *refCon);
    virtual IOReturn CreateInterfaceIterator(IOUSBFindInterfaceRequest *intReq, io_iterator_t *iter);
    // ----- new with 1.8.2
    virtual IOReturn USBDeviceSuspend(bool suspend);
    virtual IOReturn USBDeviceAbortPipeZero(void);
    virtual IOReturn USBDeviceGetManufacturerStringIndex(UInt8 *msi);
    virtual IOReturn USBDeviceGetProductStringIndex(UInt8 *psi);
    virtual IOReturn USBDeviceGetSerialNumberStringIndex(UInt8 *snsi);
    // ----- new with 1.8.7
    virtual IOReturn USBDeviceReEnumerate(UInt32 options);

/*
 * Routing gumf for CFPlugIn interfaces
 */
protected:

    IOReturn	CacheConfigDescriptor( void );
    
    static inline IOUSBDeviceClass *getThis(void *self)
        { return (IOUSBDeviceClass *) ((InterfaceMap *) self)->obj; };

    // Methods for routing the iocfplugin Interface v1r1
    static IOReturn deviceProbe(void *self,
                                CFDictionaryRef propertyTable,
                                io_service_t service, SInt32 *order);

    static IOReturn deviceStart(void *self,
                                CFDictionaryRef propertyTable,
                                io_service_t service);

    static IOReturn deviceStop(void *self);	// Calls close()
    static IOReturn deviceCreateDeviceAsyncEventSource(void *self, CFRunLoopSourceRef *source);
    static CFRunLoopSourceRef deviceGetDeviceAsyncEventSource(void *self);
    static IOReturn deviceCreateDeviceAsyncPort(void *self, mach_port_t *port);
    static mach_port_t deviceGetDeviceAsyncPort(void *self);
    static IOReturn deviceUSBDeviceOpen(void *self);
    static IOReturn deviceUSBDeviceClose(void *self);
    static IOReturn deviceGetDeviceClass(void *self, UInt8 *devClass);
    static IOReturn deviceGetDeviceSubClass(void *self, UInt8 *devSubClass);
    static IOReturn deviceGetDeviceProtocol(void *self, UInt8 *devProtocol);
    static IOReturn deviceGetDeviceVendor(void *self, UInt16 *devVendor);
    static IOReturn deviceGetDeviceProduct(void *self, UInt16 *devProduct);
    static IOReturn deviceGetDeviceReleaseNumber(void *self, UInt16 *devRelNum);
    static IOReturn deviceGetDeviceAddress(void *self, USBDeviceAddress *addr);
    static IOReturn deviceGetDeviceBusPowerAvailable(void *self, UInt32 *powerAvailable);
    static IOReturn deviceGetDeviceSpeed(void *self, UInt8 *devSpeed);
    static IOReturn deviceGetNumberOfConfigurations(void *self, UInt8 *numConfig);
    static IOReturn deviceGetLocationID(void *self, UInt32 *locationID);
    static IOReturn deviceGetConfigurationDescriptorPtr(void *self, UInt8 configIndex, IOUSBConfigurationDescriptorPtr *desc);
    static IOReturn deviceGetConfiguration(void *self, UInt8 *configNum);
    static IOReturn deviceSetConfiguration(void *self, UInt8 configNum);
    static IOReturn deviceGetBusFrameNumber(void *self, UInt64 *frame, AbsoluteTime *atTime);
    static IOReturn deviceResetDevice(void *self);
    static IOReturn deviceDeviceRequest(void *self, IOUSBDevRequest *req);
    static IOReturn deviceDeviceRequestAsync(void *self, IOUSBDevRequest *req, IOAsyncCallback1 callback, void *refCon);
    static IOReturn deviceCreateInterfaceIterator(void *self, IOUSBFindInterfaceRequest *intReq, io_iterator_t *iter);
    // -----added in 1.8.2
    static IOReturn deviceUSBDeviceOpenSeize(void *self);
    static IOReturn deviceDeviceRequestTO(void *self, IOUSBDevRequestTO *req);
    static IOReturn deviceDeviceRequestAsyncTO(void *self, IOUSBDevRequestTO *req, IOAsyncCallback1 callback, void *refCon);
    static IOReturn deviceUSBDeviceSuspend(void *self, Boolean suspend);
    static IOReturn deviceUSBDeviceAbortPipeZero(void *self);
    static IOReturn deviceGetManufacturerStringIndex(void *self, UInt8 *msi);
    static IOReturn deviceGetProductStringIndex(void *self, UInt8 *psi);
    static IOReturn deviceGetSerialNumberStringIndex(void *self, UInt8 *snsi);
    // -----added in 1.8.7
    static IOReturn deviceReEnumerateDevice(void *self, UInt32 options);
};

#endif /* !_IOKIT_IOUSBDeviceClass_H */
