/*
 * Copyright (c) 1998-2007 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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

#ifndef _IOKIT_IOUSBDEVICEUSERCLIENT_H
#define _IOKIT_IOUSBDEVICEUSERCLIENT_H

//================================================================================================
//
//   Headers
//
//================================================================================================
//
#include <libkern/OSByteOrder.h>

#include <IOKit/IOUserClient.h>
#include <IOKit/assert.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOMessage.h>
#include <IOKit/usb/IOUSBUserClient.h>
#include <IOKit/usb/IOUSBControllerV2.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/IOUSBInterface.h>
#include <IOKit/usb/IOUSBPipe.h>
#include <IOKit/usb/IOUSBLog.h>


//================================================================================================
//
//   Class Declaration for IOUSBDeviceUserClientV2
//
//================================================================================================
//
/*!
 @class IOUSBDeviceUserClientV2
 @abstract Connection to the IOUSBDevice objects from user space.
 @discussion This class can be overriden to provide for specific behaviors.
 */
class IOUSBDeviceUserClientV2 : public IOUserClient
{
    OSDeclareDefaultStructors(IOUSBDeviceUserClientV2)

    IOUSBDevice *					fOwner;
    task_t							fTask;
    mach_port_t						fWakePort;
    IOCommandGate *					fGate;
    IOWorkLoop	*					fWorkLoop;
    uint32_t						fOutstandingIO;
    bool							fDead;
    bool							fNeedToClose;
    
    struct IOUSBDeviceUserClientExpansionData 
    {
		UInt32						fSleepPowerAllocated;
		UInt32						fWakePowerAllocated;
    };
   
    IOUSBDeviceUserClientExpansionData * fIOUSBDeviceUserClientExpansionData;

protected:
        
    static const IOExternalMethodDispatch			sMethods[kIOUSBLibDeviceUserClientNumCommands];
	
	
public:
        
    // IOService methods
    //
    virtual void                        stop(IOService * provider);
    virtual bool                        start( IOService * provider );
    virtual bool                        finalize(IOOptionBits options);
    virtual void                        free();
    virtual bool                        willTerminate( IOService * provider, IOOptionBits options );
    virtual bool                        didTerminate( IOService * provider, IOOptionBits options, bool * defer );
    virtual IOReturn					message( UInt32 type, IOService * provider,  void * argument = 0 );

    // IOUserClient methods
    //
    virtual bool						initWithTask(task_t owningTask, void *security_id, UInt32 type, OSDictionary *properties);
	virtual IOReturn					externalMethod(	uint32_t selector, IOExternalMethodArguments * arguments, IOExternalMethodDispatch * dispatch, OSObject * target, void * reference);
    virtual IOReturn					clientClose( void );
    virtual IOReturn					clientDied( void );

	// Open the IOUSBDevice
    static	IOReturn					_open(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
    virtual IOReturn                    open(bool seize);
	
	// Close the IOUSBDevice
	static	IOReturn					_close(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
	virtual IOReturn					close(void);


    // IOUSBDevice methods
    //
  	static	IOReturn					_AbortPipeZero(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);  
    virtual IOReturn                    AbortPipeZero(void);
	
	static	IOReturn					_SetConfiguration(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
    virtual IOReturn                    SetConfiguration(UInt8 configIndex);
	
	static	IOReturn					_GetConfiguration(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
    virtual IOReturn					GetConfiguration(uint64_t *configIndex);
	
 	static	IOReturn					_GetConfigDescriptor(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
	virtual IOReturn                    GetConfigDescriptor(UInt8 configIndex, IOUSBConfigurationDescriptorPtr desc, UInt32 *size);
	virtual IOReturn                    GetConfigDescriptor(UInt8 configIndex, IOMemoryDescriptor * mem, uint32_t *size);

  	static	IOReturn					_CreateInterfaceIterator(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);  
	virtual IOReturn                    CreateInterfaceIterator(IOUSBFindInterfaceRequest *reqIn, io_object_t *iterOut, IOByteCount inCount, IOByteCount *outCount);
		
  	static	IOReturn					_DeviceRequestIn(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);  
    virtual IOReturn                    DeviceRequestIn(UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, mach_vm_size_t size, mach_vm_address_t buffer, UInt32 noDataTimeout, UInt32 completionTimeout, IOUSBCompletion * completion);
    virtual IOReturn                    DeviceRequestIn(UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, UInt32 noDataTimeout, UInt32 completionTimeout, void *requestBuffer, uint32_t *size);
	virtual	IOReturn					DeviceRequestIn(UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, UInt32 noDataTimeout, UInt32 completionTimeout, IOMemoryDescriptor *mem, uint32_t *pOutSize);
	
  	static	IOReturn					_DeviceRequestOut(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);  
    virtual IOReturn                    DeviceRequestOut(UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, mach_vm_size_t size, mach_vm_address_t buffer, UInt32 noDataTimeout, UInt32 completionTimeout, IOUSBCompletion * completion);
    virtual IOReturn                    DeviceRequestOut(UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, UInt32 noDataTimeout, UInt32 completionTimeout, const void *requestBuffer, uint32_t size);
	virtual	IOReturn					DeviceRequestOut(UInt8 bmRequestType,  UInt8 bRequest, UInt16 wValue, UInt16 wIndex, UInt32 noDataTimeout, UInt32 completionTimeout, IOMemoryDescriptor *mem);
		
  	static	IOReturn					_SetAsyncPort(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);  
    virtual IOReturn                    SetAsyncPort(mach_port_t port);
	
  	static	IOReturn					_ResetDevice(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);  
    virtual IOReturn                    ResetDevice( void );
	
  	static	IOReturn					_SuspendDevice(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);  
    virtual IOReturn                    SuspendDevice(bool suspend);
	
  	static	IOReturn					_ReEnumerateDevice(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);  
    virtual IOReturn                    ReEnumerateDevice(UInt32 options);
    
  	static	IOReturn					_GetFrameNumber(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);  
    virtual IOReturn                    GetFrameNumber(IOUSBGetFrameStruct *data, UInt32 *size);
	
  	static	IOReturn					_GetMicroFrameNumber(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);  
    virtual IOReturn                    GetMicroFrameNumber(IOUSBGetFrameStruct *data, UInt32 *size);
	
   	static	IOReturn					_GetFrameNumberWithTime(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);  
	virtual IOReturn                    GetFrameNumberWithTime(IOUSBGetFrameStruct *data, UInt32 *size);

	static	IOReturn					_GetDeviceInformation(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
    virtual IOReturn					GetDeviceInformation(uint64_t *configIndex);
	
	static	IOReturn					_RequestExtraPower(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
    virtual IOReturn					RequestExtraPower(UInt32 type, UInt32 requestedPower, uint64_t *powerAvailable);
	
	static	IOReturn					_ReturnExtraPower(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
    virtual IOReturn					ReturnExtraPower(UInt32 type, UInt32 powerReturned);
	
	static	IOReturn					_GetExtraPowerAllocated(IOUSBDeviceUserClientV2 * target, void * reference, IOExternalMethodArguments * arguments);
    virtual IOReturn					GetExtraPowerAllocated(UInt32 type, uint64_t *powerAllocated);
	
    // bookkeeping methods
    virtual void                        DecrementOutstandingIO(void);
    virtual void                        IncrementOutstandingIO(void);
    virtual UInt32                      GetOutstandingIO(void);
	
	void								PrintExternalMethodArgs( IOExternalMethodArguments * arguments, UInt32 level );

    // Getters
    //
    IOUSBDevice *						GetOwner(void)                          { return fOwner; }
    task_t								GetTask(void)                           { return fTask; }
    IOCommandGate *						GetCommandGate(void)                    { return fGate; }
    mach_port_t							GetWakePort(void)                       { return fWakePort; }
    bool								IsDead(void)                            { return fDead; }
    bool								NeedToClose(void)                       { return fNeedToClose; }
    
    // static methods
    //
    static void 			ReqComplete(void *obj, void *param, IOReturn status, UInt32 remaining);
    static IOReturn			ChangeOutstandingIO(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3);
    static IOReturn			GetGatedOutstandingIO(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3);

    // padding methods
    //
    OSMetaClassDeclareReservedUsed(IOUSBDeviceUserClientV2,	 0);
	virtual IOReturn					CreateInterfaceIterator(IOUSBFindInterfaceRequest *reqIn, uint64_t *returnIter);

    OSMetaClassDeclareReservedUnused(IOUSBDeviceUserClientV2,  1);
    OSMetaClassDeclareReservedUnused(IOUSBDeviceUserClientV2,  2);
    OSMetaClassDeclareReservedUnused(IOUSBDeviceUserClientV2,  3);
    OSMetaClassDeclareReservedUnused(IOUSBDeviceUserClientV2,  4);
    OSMetaClassDeclareReservedUnused(IOUSBDeviceUserClientV2,  5);
    OSMetaClassDeclareReservedUnused(IOUSBDeviceUserClientV2,  6);
    OSMetaClassDeclareReservedUnused(IOUSBDeviceUserClientV2,  7);
    OSMetaClassDeclareReservedUnused(IOUSBDeviceUserClientV2,  8);
    OSMetaClassDeclareReservedUnused(IOUSBDeviceUserClientV2,  9);
    OSMetaClassDeclareReservedUnused(IOUSBDeviceUserClientV2, 10);
    OSMetaClassDeclareReservedUnused(IOUSBDeviceUserClientV2, 11);
    OSMetaClassDeclareReservedUnused(IOUSBDeviceUserClientV2, 12);
    OSMetaClassDeclareReservedUnused(IOUSBDeviceUserClientV2, 13);
    OSMetaClassDeclareReservedUnused(IOUSBDeviceUserClientV2, 14);
    OSMetaClassDeclareReservedUnused(IOUSBDeviceUserClientV2, 15);
    OSMetaClassDeclareReservedUnused(IOUSBDeviceUserClientV2, 16);
    OSMetaClassDeclareReservedUnused(IOUSBDeviceUserClientV2, 17);
    OSMetaClassDeclareReservedUnused(IOUSBDeviceUserClientV2, 18);
    OSMetaClassDeclareReservedUnused(IOUSBDeviceUserClientV2, 19);
};


#endif /* ! _IOKIT_IOUSBDEVICEUSERCLIENT_H */

