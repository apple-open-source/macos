/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved. 
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#ifndef _IOUSBLIB_H
#define _IOUSBLIB_H

#include <IOKit/usb/USB.h>
#include <IOKit/IOKitLib.h>

#include <CoreFoundation/CFRunLoop.h>
#include <CoreFoundation/CFPlugIn.h>
#if COREFOUNDATION_CFPLUGINCOM_SEPARATE
#include <CoreFoundation/CFPlugInCOM.h>
#endif

#include <sys/cdefs.h>

__BEGIN_DECLS

// 9dc7b780-9ec0-11d4-a54f-000a27052861
/*! @defined kIOUSBDeviceUserClientTypeID
    @discussion Factory ID for creating a USB Device User Client. */
#define kIOUSBDeviceUserClientTypeID CFUUIDGetConstantUUIDWithBytes(NULL,	\
    0x9d, 0xc7, 0xb7, 0x80, 0x9e, 0xc0, 0x11, 0xD4,			\
    0xa5, 0x4f, 0x00, 0x0a, 0x27, 0x05, 0x28, 0x61)

// 2d9786c6-9ef3-11d4-ad51-000a27052861
/*! @defined kIOUSBInterfaceUserClientTypeID
    @discussion Factory ID for creating a USB Interface User Client. */
#define kIOUSBInterfaceUserClientTypeID CFUUIDGetConstantUUIDWithBytes(NULL,	\
    0x2d, 0x97, 0x86, 0xc6, 0x9e, 0xf3, 0x11, 0xD4,			\
    0xad, 0x51, 0x00, 0x0a, 0x27, 0x05, 0x28, 0x61)

// 4547a8aa-9ef3-11d4-a9bd-000a27052861
/*! @defined kIOUSBFactoryID
    @discussion UUID for the USB Factory. */
#define kIOUSBFactoryID CFUUIDGetConstantUUIDWithBytes(NULL,		\
    0x45, 0x47, 0xa8, 0xaa, 0x9e, 0xf3, 0x11, 0xD4,			\
    0xa9, 0xbd, 0x00, 0x0a, 0x27, 0x05, 0x28, 0x61)

// 5c8187d0-9ef3-11d4-8b45-000a27052861
/*! @defined kIOUSBDeviceInterfaceID
    @discussion InterfaceID for IOUSBDeviceInterface. */
#define kIOUSBDeviceInterfaceID CFUUIDGetConstantUUIDWithBytes(NULL,	\
    0x5c, 0x81, 0x87, 0xd0, 0x9e, 0xf3, 0x11, 0xD4,			\
    0x8b, 0x45, 0x00, 0x0a, 0x27, 0x05, 0x28, 0x61)

// 73c97ae8-9ef3-11d4-b1d0-000a27052861
/*! @defined kIOUSBInterfaceInterfaceID
    @discussion InterfaceID for IOUSBInterfaceInterface. */
#define kIOUSBInterfaceInterfaceID CFUUIDGetConstantUUIDWithBytes(NULL,	\
    0x73, 0xc9, 0x7a, 0xe8, 0x9e, 0xf3, 0x11, 0xD4,			\
    0xb1, 0xd0, 0x00, 0x0a, 0x27, 0x05, 0x28, 0x61)

/*! @struct IOCDBDeviceInterface
    @abstract Basic interface for a USB Device.  
    @discussion After rendezvous with a USB Device in the IORegistry you can create an instance of this interface as a proxy to the IOService.  Once you have this interface, or one of its subclasses, you can issue actual commands to the USB Device.
*/
typedef struct IOUSBDeviceStruct {
    IUNKNOWN_C_GUTS;
/*! @function CreateDeviceAsyncEventSource
    @abstract Create a run loop source for delivery of all asynchronous notifications on this device.
    @discussion The Mac OS X kernel does not spawn a thread to callback to the client.  Instead it deliveres completion notifications on a mach port, see createAsyncPort.  This routine wraps that port with the appropriate routing code so that the completion notifications can be automatically routed through the clients CFRunLoop.
    @param self Pointer to an IOUSBDeviceInterface for one IOService.
    @param source Pointer to a CFRunLoopSourceRef to return the newly created run loop event source.
    @result Returns kIOReturnSuccess if successful or a kern_return_t if failed.
*/
    IOReturn (*CreateDeviceAsyncEventSource)(void *self, CFRunLoopSourceRef *source);

/*! @function GetDeviceAsyncEventSource
    @abstract Return the CFRunLoopSourceRef for this IOService instance.
    @param self Pointer to an IOUSBDeviceInterface for one IOService.
    @result Returns the run loop source if one has been created, 0 otherwise.
*/
    CFRunLoopSourceRef (*GetDeviceAsyncEventSource)(void *self);

/*! @function CreateDeviceAsyncPort
    @abstract Create and register a mach_port_t for asynchronous communications.
    @discussion The Mac OS X kernel does not spawn a thread to callback to the client.  Instead it deliveres completion notifications on this mach port.  After receiving a message on this port the client is obliged to call the IOKitLib.h: IODispatchCalloutFromMessage() function for decoding the notification message.
    @param self Pointer to an IOUSBDeviceInterface for one IOService.
    @param port Pointer to a mach_port_t to return the newly created port.
    @result Returns kIOReturnSuccess if successful or a kern_return_t if failed.
*/
    IOReturn (*CreateDeviceAsyncPort)(void *self, mach_port_t *port);

/*! @function GetDeviceAsyncPort
    @abstract Return the mach_port_t port for this IOService instance.
    @param self Pointer to an IOUSBDeviceInterface for one IOService.
    @result Returns the port if one exists, 0 otherwise.
*/
    mach_port_t (*GetDeviceAsyncPort)(void *self);


/*! @function USBDeviceOpen
    @abstract Open up the IOUSBDevice for exclusive access.
    @discussion Before the client can issue USB commands which change the state of the device it must have succeeded in opening the device.  This establishes an exclusive link between the clients task and the actual device.  
    @param self Pointer to an IOUSBDeviceInterface for one IOService.
    @result Returns kIOReturnExclusiveAccess if some other task has the device opened already, kIOReturnError if the connection with the kernel can not be established or kIOReturnSuccess if successful.
*/
    IOReturn (*USBDeviceOpen)(void *self);


/*! @function USBDeviceClose
    @abstract Close the task's connection to the IOUSBDevice.
    @discussion Release the clients exclusive access to the IOUSBDevice.
    @param self Pointer to an IOUSBDeviceInterface for one IOService.
    @result Returns kIOReturnSuccess if successful, some other mach error if the connection is no longer valid.
*/
    IOReturn (*USBDeviceClose)(void *self);

/*! @function GetDeviceClass
    @abstract Return the USB Class of the device.
    @discussion The device does not have to be open.
    @param self Pointer to an IOUSBDeviceInterface for one IOService.
    @param devClass Pointer to UInt8 to hold the device Class
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*GetDeviceClass)(void *self, UInt8 *devClass);

/*! @function GetDeviceSubClass
    @abstract Return the USB SubClass of the device.
    @discussion The device does not have to be open.
    @param self Pointer to an IOUSBDeviceInterface for one IOService.
    @param devSubClass Pointer to UInt8 to hold the device SubClass
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*GetDeviceSubClass)(void *self, UInt8 *devSubClass);

/*! @function GetDeviceProtocol
    @abstract Return the USB protocol of the device.
    @discussion The device does not have to be open.
    @param self Pointer to an IOUSBDeviceInterface for one IOService.
    @param devProtocol Pointer to UInt8 to hold the device protocol
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*GetDeviceProtocol)(void *self, UInt8 *devProtocol);

/*! @function GetDeviceVendor
    @abstract Return the USB VendorID of the device.
    @discussion The device does not have to be open. The result is returned in correct endianess.
    @param self Pointer to an IOUSBDeviceInterface for one IOService.
    @param devVendor Pointer to UInt16 to hold the vendor ID.
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*GetDeviceVendor)(void *self, UInt16 *devVendor);

/*! @function GetDeviceProduct
    @abstract Return the USB ProductID of the device.
    @discussion The device does not have to be open. The result is returned in correct endianess.
    @param self Pointer to an IOUSBDeviceInterface for one IOService.
    @param devProduct Pointer to UInt16 to hold the product ID.
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*GetDeviceProduct)(void *self, UInt16 *devProduct);

/*! @function GetDeviceReleaseNumber
    @abstract Return the USB Release Number of the device.
    @discussion The device does not have to be open. The result is returned in correct endianess.
    @param self Pointer to an IOUSBDeviceInterface for one IOService.
    @param devRelNum Pointer to UInt16 to hold the release number.
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*GetDeviceReleaseNumber)(void *self, UInt16 *devRelNum);

/*! @function GetDeviceAddress
    @abstract Return the address of the device on its bus.
    @discussion The device does not have to be open.
    @param self Pointer to an IOUSBDeviceInterface for one IOService.
    @param addr Pointer to USBDeviceAddress to hold the release number.
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*GetDeviceAddress)(void *self, USBDeviceAddress *addr);

/*! @function GetDeviceBusPowerAvailable
    @abstract Return the power available to the device.
    @discussion The device does not have to be open.
    @param self Pointer to an IOUSBDeviceInterface for one IOService.
    @param powerAvailable Pointer to UInt32 to hold the power available (in 2ms increments).
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*GetDeviceBusPowerAvailable)(void *self, UInt32 *powerAvailable);

/*! @function GetDeviceSpeed
    @abstract Return the speed of the device.
    @discussion The device does not have to be open.
    @param self Pointer to an IOUSBDeviceInterface for one IOService.
    @param devSpeed Pointer to UInt8 to hold the speed (kUSBDeviceSpeedLow or kUSBDeviceSpeedFull).
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*GetDeviceSpeed)(void *self, UInt8 *devSpeed);

/*! @function GetNumberOfConfigurations
    @abstract Return the number of supported configurations in the device.
    @discussion The device does not have to be open.
    @param self Pointer to an IOUSBDeviceInterface for one IOService.
    @param numConfig Pointer to UInt8 to hold the number of configurations.
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*GetNumberOfConfigurations)(void *self, UInt8 *numConfig);

/*! @function GetLocationID
    @abstract Return the location ID.
    @discussion The device does not have to be open.
    @param self Pointer to an IOUSBDeviceInterface for one IOService.
    @param locationID Pointer to UInt32 to hold the location ID.
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*GetLocationID)(void *self, UInt32 *locationID);

/*! @function GetConfigurationDescriptorPtr
    @abstract Return a pointer to a configuration descriptor for a given index.
    @discussion The device does not have to be open.
    @param self Pointer to an IOUSBDeviceInterface for one IOService.
    @param configIndex The index of the desired config descriptor.
    @param desc Pointer to pointer to an IOUSBConfigurationDescriptor.
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*GetConfigurationDescriptorPtr)(void *self, UInt8 configIndex, IOUSBConfigurationDescriptorPtr *desc);

/*! @function GetConfiguration
    @abstract Return the currenttly selected configuration in the device.
    @discussion The device does not have to be open.
    @param self Pointer to an IOUSBDeviceInterface for one IOService.
    @param desc Pointer to UInt8 to hold the current config number.
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*GetConfiguration)(void *self, UInt8 *configNum);

/*! @function SetConfiguration
    @abstract Sets the configuration in the device.
    @discussion The device must be open.
    @param self Pointer to an IOUSBDeviceInterface for one IOService.
    @param configNum value of desired configuration (from IOUSBConfigurationDescriptor.bConfigurationValue).
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService, or kIOReturnNotOpen if the device is not open for exclusive access.
*/
    IOReturn (*SetConfiguration)(void *self, UInt8 configNum);

/*! @function GetBusFrameNumber
    @abstract Gets the current frame number of the bus to which the device is attached.
    @discussion The device does not have to be open.
    @param self Pointer to an IOUSBDeviceInterface for one IOService.
    @param frame Pointer to UInt64 to hold the frame number.
    @param atTime Pointer to an AbsoluteTime, which should be within 1ms of the time when the bus frame number was attained.
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*GetBusFrameNumber)(void *self, UInt64 *frame, AbsoluteTime *atTime);

/*! @function ResetDevice
    @abstract Resets the device.
    @discussion The device must be open.
    @param self Pointer to an IOUSBDeviceInterface for one IOService.
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService, or kIOReturnNotOpen if the device is not open for exclusive access.
*/
    IOReturn (*ResetDevice)(void *self);

/*! @function DeviceRequest
    @abstract Sends a USB request on the default control pipe for the device (pipe zero).
    @discussion The device must be open.
    @param self Pointer to an IOUSBDeviceInterface for one IOService.
    @param req Pointer to an IOUSBDevRequest containing the request.
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService, or kIOReturnNotOpen if the device is not open for exclusive access.
*/
    IOReturn (*DeviceRequest)(void *self, IOUSBDevRequest *req);

/*! @function DeviceRequestAsync
    @abstract Sends an asyncronous USB request on the default control pipe for the device (pipe zero).
    @discussion The device must be open.
    @param self Pointer to an IOUSBDeviceInterface for one IOService.
    @param req Pointer to an IOUSBDevRequest containing the request.
    @param callback An IOAsyncCallback1 method. A message addressed to this callback is posted to the Async port upon completion.
    @param refCon Arbitrary pointer which is passed as a parameter to the callback routine.
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService, kIOReturnNotOpen if the device is not open for exclusive access, or kIOUSBNoAsyncPortErr if no Async port has been created for this device.
*/
    IOReturn (*DeviceRequestAsync)(void *self, IOUSBDevRequest *req, IOAsyncCallback1 callback, void *refCon);
    
/*! @function CreateInterfaceIterator
    @abstract Creates an iterator to iterate over all interfaces of the device.
    @discussion The device does not have be open.
    @param self Pointer to an IOUSBDeviceInterface for one IOService.
    @param req Pointer to an IOUSBFindInterfaceRequest describing the desired interface(s).
    @param iter Pointer to an io_iterator_t which will iterate over the desired interfaces.
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*CreateInterfaceIterator)(void *self, IOUSBFindInterfaceRequest *req, io_iterator_t *iter);
} IOUSBDeviceInterface;


typedef struct IOUSBInterfaceStruct {
    IUNKNOWN_C_GUTS;
/*! @function CreateInterfaceAsyncEventSource
    @abstract Create a run loop source for delivery of all asynchronous notifications on this device.
    @discussion The Mac OS X kernel does not spawn a thread to callback to the client.  Instead it deliveres completion notifications on a mach port, see createAsyncPort.  This routine wraps that port with the appropriate routing code so that the completion notifications can be automatically routed through the clients CFRunLoop.
    @param self Pointer to an IOUSBInterfaceInterface for one IOService.
    @param source Pointer to a CFRunLoopSourceRef to return the newly created run loop event source.
    @result Returns kIOReturnSuccess if successful or a kern_return_t if failed.
*/
    IOReturn (*CreateInterfaceAsyncEventSource)(void *self, CFRunLoopSourceRef *source);

/*! @function GetInterfaceAsyncEventSource
    @abstract Return the CFRunLoopSourceRef for this IOService instance.
    @param self Pointer to an IOUSBInterfaceInterface for one IOService.
    @result Returns the run loop source if one has been created, 0 otherwise.
*/
    CFRunLoopSourceRef (*GetInterfaceAsyncEventSource)(void *self);

/*! @function CreateInterfaceAsyncPort
    @abstract Create and register a mach_port_t for asynchronous communications.
    @discussion The Mac OS X kernel does not spawn a thread to callback to the client.  Instead it deliveres completion notifications on this mach port.  After receiving a message on this port the client is obliged to call the IOKitLib.h: IODispatchCalloutFromMessage() function for decoding the notification message.
    @param self Pointer to an IOUSBInterfaceInterface for one IOService.
    @param port Pointer to a mach_port_t to return the newly created port.
    @result Returns kIOReturnSuccess if successful or a kern_return_t if failed.
*/
    IOReturn (*CreateInterfaceAsyncPort)(void *self, mach_port_t *port);

/*! @function GetInterfaceAsyncPort
    @abstract Return the mach_port_t port for this IOService instance.
    @param self Pointer to an IOUSBInterfaceInterface for one IOService.
    @result Returns the port if one exists, 0 otherwise.
*/
    mach_port_t (*GetInterfaceAsyncPort)(void *self);

/*! @function USBInterfaceOpen
    @abstract Open up the IOUSBInterface for exclusive access.
    @discussion Before the client can transfer data to and from the interface, it must have succeeded in opening the interface.  This establishes an exclusive link between the clients task and the actual interface device.Opening the interface causes pipes to be created on each endpoint contained in the interface.  
    @param self Pointer to an IOUSBInterfaceInterface for one IOService.
    @result Returns kIOReturnExclusiveAccess if some other task has the device opened already, kIOReturnError if the connection with the kernel can not be established or kIOReturnSuccess if successful.
*/
    IOReturn (*USBInterfaceOpen)(void *self);


/*! @function USBInterfaceClose
    @abstract Close the task's connection to the IOUSBInterface.
    @discussion Release the clients exclusive access to the IOUSBInterface.
    @param self Pointer to an IOUSBInterfaceInterface for one IOService.
    @result Returns kIOReturnSuccess if successful, some other mach error if the connection is no longer valid.
*/
    IOReturn (*USBInterfaceClose)(void *self);

/*! @function GetInterfaceClass
    @abstract Return the USB Class of the interface.
    @discussion The interface does not have to be open.
    @param self Pointer to an IOUSBInterfaceInterface for one IOService.
    @param intfClass Pointer to UInt8 to hold the interface Class
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*GetInterfaceClass)(void *self, UInt8 *intfClass);

/*! @function GetInterfaceSubClass
    @abstract Return the USB Subclass of the interface.
    @discussion The interface does not have to be open.
    @param self Pointer to an IOUSBInterfaceInterface for one IOService.
    @param intfSubClass Pointer to UInt8 to hold the interface Subclass
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*GetInterfaceSubClass)(void *self, UInt8 *intfSubClass);

/*! @function GetInterfaceProtocol
    @abstract Return the USB Protocol of the interface.
    @discussion The interface does not have to be open.
    @param self Pointer to an IOUSBInterfaceInterface for one IOService.
    @param intfProtocol Pointer to UInt8 to hold the interface Protocol
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*GetInterfaceProtocol)(void *self, UInt8 *intfProtocol);

/*! @function GetDeviceVendor
    @abstract Return the VendorID of the device of which this interface is a part.
    @discussion The interface does not have to be open.
    @param self Pointer to an IOUSBInterfaceInterface for one IOService.
    @param devVendor Pointer to UInt16 to hold the vendorID
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*GetDeviceVendor)(void *self, UInt16 *devVendor);

/*! @function GetDeviceProduct
    @abstract Return the ProductID of the device of which this interface is a part.
    @discussion The interface does not have to be open.
    @param self Pointer to an IOUSBInterfaceInterface for one IOService.
    @param devProduct Pointer to UInt16 to hold the ProductID
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*GetDeviceProduct)(void *self, UInt16 *devProduct);

/*! @function GetDeviceReleaseNumber
    @abstract Return the Release Number of the device of which this interface is a part.
    @discussion The interface does not have to be open.
    @param self Pointer to an IOUSBInterfaceInterface for one IOService.
    @param devRelNum Pointer to UInt16 to hold the Release Number
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*GetDeviceReleaseNumber)(void *self, UInt16 *devRelNum);

/*! @function GetConfigurationValue
    @abstract Return the current configuration value set in the device. The interface will be part of that configuration.
    @discussion The interface does not have to be open.
    @param self Pointer to an IOUSBInterfaceInterface for one IOService.
    @param configVal Pointer to UInt8 to hold the configuration value.
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*GetConfigurationValue)(void *self, UInt8 *configVal);

/*! @function GetInterfaceNumber
    @abstract Return the interface number (zero based index) of this interface within the current configuration of the device.
    @discussion The interface does not have to be open.
    @param self Pointer to an IOUSBInterfaceInterface for one IOService.
    @param configVal Pointer to UInt8 to hold the configuration value.
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*GetInterfaceNumber)(void *self, UInt8 *intfNumber);

/*! @function GetAlternateSetting
    @abstract Return the alternate setting currently selected in this interface.
    @discussion The interface does not have to be open.
    @param self Pointer to an IOUSBInterfaceInterface for one IOService.
    @param intfAltSetting Pointer to UInt8 to hold the alternate setting value.
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*GetAlternateSetting)(void *self, UInt8 *intfAltSetting);

/*! @function GetNumEndpoints
    @abstract Return the number of endpoints in this interface.
    @discussion The interface does not have to be open.
    @param self Pointer to an IOUSBInterfaceInterface for one IOService.
    @param intfNumEndpoints Pointer to UInt8 to hold the number of endpoints.
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*GetNumEndpoints)(void *self, UInt8 *intfNumEndpoints);

/*! @function GetLocationID
    @abstract Return the location ID.
    @discussion The interface does not have to be open.
    @param self Pointer to an IOUSBDeviceInterface for one IOService.
    @param locationID Pointer to UInt32 to hold the location ID.
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*GetLocationID)(void *self, UInt32 *locationID);

/*! @function GetDevice
    @abstract Return the device of which this interface is part.
    @discussion The interface does not have to be open. The returned device can be used to create a CFPlugin to talk to the device.
    @param self Pointer to an IOUSBInterfaceInterface for one IOService.
    @param device Pointer to io_service_t to hold the result.
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*GetDevice)(void *self, io_service_t *device);

/*! @function SetAlternateInterface
    @abstract Change the AltInterface setting.
    @discussion The interface must be open.
    @param self Pointer to an IOUSBInterfaceInterface for one IOService.
    @param alternateSetting The new alternate setting for the interface.
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService, or kIOReturnNotOpen if the interface is not open for exclusive access.
*/
    IOReturn (*SetAlternateInterface)(void *self, UInt8 alternateSetting);

/*! @function GetBusFrameNumber
    @abstract Gets the current frame number of the bus to which the interface and its device is attached.
    @discussion The interface does not have to be open.
    @param self Pointer to an IOUSBInterfaceInterface for one IOService.
    @param frame Pointer to UInt64 to hold the frame number.
    @param atTime Pointer to an AbsoluteTime, which should be within 1ms of the time when the bus frame number was attained.
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService.
*/
    IOReturn (*GetBusFrameNumber)(void *self, UInt64 *frame, AbsoluteTime *atTime);

/*! @function ControlRequest
    @abstract Sends a USB request on a control pipe. Use pipeRef=0 for the default device control pipe.
    @discussion If the request is a standard request which will change the state of the device, the device must be open, which means you should be using the IOUSBDeviceInterface for this command.
    @param self Pointer to an IOUSBInterfaceInterface for one IOService.
    @param req Pointer to an IOUSBDevRequest containing the request.
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService, or kIOReturnNotOpen if the interface is not open for exclusive access.
*/
    IOReturn (*ControlRequest)(void *self, UInt8 pipeRef, IOUSBDevRequest *req);

/*! @function ControlRequestAsync
    @abstract Sends an asyncronous USB request on a control pipe. Use pipeRef=0 for the default device control pipe.
    @discussion If the request is a standard request which will change the state of the device, the device must be open, which means you should be using the IOUSBDeviceInterface for this command.
    @param self Pointer to an IOUSBInterfaceInterface for one IOService.
    @param req Pointer to an IOUSBDevRequest containing the request.
    @param callback An IOAsyncCallback1 method. A message addressed to this callback is posted to the Async port upon completion.
    @param refCon Arbitrary pointer which is passed as a parameter to the callback routine.
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService, kIOReturnNotOpen if the interface is not open for exclusive access, or kIOUSBNoAsyncPortErr if no Async port has been created for this interface.
*/
    IOReturn (*ControlRequestAsync)(void *self, UInt8 pipeRef, IOUSBDevRequest *req, IOAsyncCallback1 callback, void *refCon);

/*! @function GetPipeProperties
    @abstract Gets the properties for a pipe.
    @discussion Once an Interface is opened, all of the pipes in that Interface get created by the kernel. The number of pipes can be retrieved by GetNumEndpoints. The client can then get the properties of any pipe using an index of 1 to GetNumEndpoints. Pipe 0 is the default control pipe in the device.
    @param self Pointer to an IOUSBInterfaceInterface for one IOService.
    @param pipeRef Index for the desired pipe (1-GetNumEndpoints).
    @param direction Pointer to an UInt8 to get the direction of the pipe.
    @param number Pointer to an UInt8 to get the pipe number.
    @param transferType Pointer to an UInt8 to get the transfer type of the pipe.
    @param maxPacketSize Pointer to an UInt16 to get the maxPacketSize of the pipe.
    @param interval Pointer to an UInt8 to get the interval for polling the pipe for data (in milliseconds).
    @result Returns kIOReturnSuccess if successful, kIOReturnNoDevice if there is no connection to an IOService, or kIOReturnNotOpen if the interface is not open for exclusive access.
*/
    IOReturn (*GetPipeProperties)(void *self, UInt8 pipeRef, UInt8 *direction, UInt8 *number, UInt8 *transferType, UInt16 *maxPacketSize, UInt8 *interval);
    IOReturn (*GetPipeStatus)(void *self, UInt8 pipeRef);
    IOReturn (*AbortPipe)(void *self, UInt8 pipeRef);
    IOReturn (*ResetPipe)(void *self, UInt8 pipeRef);
    IOReturn (*ClearPipeStall)(void *self, UInt8 pipeRef);
    IOReturn (*ReadPipe)(void *self, UInt8 pipeRef, void *buf, UInt32 *size);
    IOReturn (*WritePipe)(void *self, UInt8 pipeRef, void *buf, UInt32 size);
    IOReturn (*ReadPipeAsync)(void *self, UInt8 pipeRef, void *buf, UInt32 size, IOAsyncCallback1 callback, void *refcon);
    IOReturn (*WritePipeAsync)(void *self, UInt8 pipeRef, void *buf, UInt32 size, IOAsyncCallback1 callback, void *refcon);
    IOReturn (*ReadIsochPipeAsync)(void *self, UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, IOUSBIsocFrame *frameList,
                                  IOAsyncCallback1 callback, void *refcon);
    IOReturn (*WriteIsochPipeAsync)(void *self, UInt8 pipeRef, void *buf, UInt64 frameStart, UInt32 numFrames, IOUSBIsocFrame *frameList,
                                  IOAsyncCallback1 callback, void *refcon);
} IOUSBInterfaceInterface;

#define kIOUSBDeviceClassName		"IOUSBDevice"
#define kIOUSBInterfaceClassName	"IOUSBInterface"

__END_DECLS

#endif /* ! _IOUSBLIB_H */
