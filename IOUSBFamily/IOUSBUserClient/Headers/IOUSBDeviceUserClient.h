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
#ifndef _IOKIT_IOUSBDEVICEUSERCLIENT_H
#define _IOKIT_IOUSBDEVICEUSERCLIENT_H

#include <IOKit/IOUserClient.h>
#include <IOKit/usb/IOUSBUserClient.h>
#include <IOKit/usb/IOUSBControllerV2.h>

class IOUSBDevice;
class OSSet;


class IOUSBDeviceUserClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOUSBDeviceUserClient)

    // this class has never been and is not intended to be subclassed
private:
    IOUSBDevice *			fOwner;
    task_t				fTask;
    const IOExternalMethod		*fMethods;
    const IOExternalAsyncMethod 	*fAsyncMethods;
    IOCommandGate 			*fGate;
    UInt32				fNumMethods;
    UInt32				fNumAsyncMethods;
    mach_port_t 			fWakePort;
    UInt32				fOutstandingIO;
    bool				fDead;
    bool				fNeedToClose;
    IOWorkLoop	*			fWorkLoop;
    
    static const IOExternalMethod	sMethods[kNumUSBDeviceMethods];
    static const IOExternalAsyncMethod	sAsyncMethods[kNumUSBDeviceAsyncMethods];

    // my protected methods
    virtual void 	SetExternalMethodVectors(void);

    // IOService methods
    virtual void 	stop(IOService * provider);
    virtual bool 	start( IOService * provider );
    virtual bool 	finalize(IOOptionBits options);
    virtual void 	free();
    // "new" IOService methods
    virtual bool 	willTerminate( IOService * provider, IOOptionBits options );
    virtual bool 	didTerminate( IOService * provider, IOOptionBits options, bool * defer );

    // psuedo IOKit methods - these methods are NOT the IOService:: methods, since both IOService::open
    // and IOService::close require an IOService* as the first parameter
    virtual IOReturn  	open(bool seize);
    virtual IOReturn  	close(void);

    // IOUserClient methods
    virtual bool			initWithTask(task_t owningTask, void *security_id, UInt32 type);
    virtual IOExternalMethod* 		getTargetAndMethodForIndex(IOService **target, UInt32 index);
    virtual IOExternalAsyncMethod* 	getAsyncTargetAndMethodForIndex(IOService **target, UInt32 index);
    virtual IOReturn 			clientClose( void );
    virtual IOReturn 			clientDied( void );

    // IOUSBDevice methods
    virtual IOReturn SetConfiguration(UInt8 configIndex);
    virtual IOReturn GetConfigDescriptor(UInt8 configIndex, IOUSBConfigurationDescriptorPtr desc, UInt32 *size);
    virtual IOReturn CreateInterfaceIterator(IOUSBFindInterfaceRequest *reqIn, io_object_t *iterOut, IOByteCount inCount, IOByteCount *outCount);
    virtual IOReturn GetFrameNumber(IOUSBGetFrameStruct *data, UInt32 *size);
    virtual IOReturn GetMicroFrameNumber(IOUSBGetFrameStruct *data, UInt32 *size);

    // transactions on pipe zero
    virtual IOReturn DeviceReqIn(UInt16 param1, UInt32 param2, UInt32 noDataTimeout, UInt32 completionTimeout, void *buf, UInt32 *size);
    virtual IOReturn DeviceReqOut(UInt16 param1, UInt32 param2, UInt32 noDataTimeout, UInt32 completionTimeout, void *buf, UInt32 size);
    virtual IOReturn DeviceReqInOOL(IOUSBDevRequestTO *reqIn, IOByteCount inCount, UInt32 *sizeOut, IOByteCount *outCount);
    virtual IOReturn DeviceReqOutOOL(IOUSBDevRequestTO *reqIn, IOByteCount inCount);
    virtual IOReturn DeviceReqInAsync(OSAsyncReference asyncRef, IOUSBDevRequestTO *reqIn, IOByteCount inCount);
    virtual IOReturn DeviceReqOutAsync(OSAsyncReference asyncRef, IOUSBDevRequestTO *reqIn, IOByteCount inCount);
    virtual IOReturn SetAsyncPort(OSAsyncReference asyncRef);
    virtual IOReturn ResetDevice( void );
    virtual IOReturn SuspendDevice(bool suspend);
    virtual IOReturn AbortPipeZero(void);
    virtual IOReturn ReEnumerateDevice(UInt32 options);
    
    // bookkeeping methods
    void		DecrementOutstandingIO(void);
    void		IncrementOutstandingIO(void);
    UInt32		GetOutstandingIO(void);

    // static methods
    static void 	ReqComplete(void *obj, void *param, IOReturn status, UInt32 remaining);
    static IOReturn	ChangeOutstandingIO(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3);
    static IOReturn	GetGatedOutstandingIO(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3);
};


#endif /* ! _IOKIT_IOUSBDEVICEUSERCLIENT_H */

