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
#ifndef _IOKIT_APPLEUSBHUBPORT_H
#define _IOKIT_APPLEUSBHUBPORT_H

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

#include <IOKit/usb/USB.h>
#include <IOKit/usb/USBHub.h>
#include <IOKit/usb/IOUSBLog.h>

#include "AppleUSBHub.h"

#include <kern/thread_call.h>

class AppleUSBHubPort;
class AppleUSBHub;

typedef IOReturn (AppleUSBHubPort::*ChangeHandlerFuncPtr)(UInt16 changeFlags);

typedef struct {
    ChangeHandlerFuncPtr handler;
    UInt32 bit;
    UInt32 clearFeature;
} portStatusChangeVector;

enum{
    kNumChangeHandlers = 5
};

typedef enum HubPortState
{
    hpsNormal = 0,
    hpsDead,
    hpsDeviceZero,
    hpsDeadDeviceZero,
    hpsSetAddress,
    hpsSetAddressFailed
} HubPortState;

class AppleUSBHubPort : public OSObject
{
    friend  class AppleUSBHub;
    
    OSDeclareDefaultStructors(AppleUSBHubPort)

protected:
    IOUSBController		*_bus;
    AppleUSBHub 		*_hub;
    IOUSBHubDescriptor		*_hubDesc;
    IOUSBDevice			*_portDevice;
    bool			_devZero;
    bool			_captive;
    bool			_retryPortStatus;
    bool			_statusChangedThreadActive;
    UInt8			_statusChangedState;
    UInt8			_connectionChangedState;
    bool			_initThreadActive;
    bool			_inCommandSleep;
    UInt32			_attachRetry;
    
    portStatusChangeVector	_changeHandler[kNumChangeHandlers];
    
    struct ExpansionData { /* */ };
    ExpansionData * _expansionData;

private:
    thread_call_t		_initThread;
    thread_call_t		_portStatusChangedHandlerThread;
    IOUSBHubPortStatus  	_portStatus;
    HubPortState		_state;
    IOLock*			_runLock;
    bool			_getDeviceDescriptorFailed;
    UInt8			_setAddressFailed;
    UInt32			_devZeroCounter;
    
    static void			PortInitEntry(OSObject *target);			// this will run on its own thread
    static void			PortStatusChangedHandlerEntry(OSObject *target);	// this will run on its own thread
 
    IOReturn			DetachDevice();
    IOReturn			GetDevZeroDescriptorWithRetries();
    bool			AcquireDeviceZero();
    void			ReleaseDeviceZero();
    
protected:
    virtual IOReturn        	init(AppleUSBHub *	parent, int portNum, UInt32 powerAvailable, bool captive);
    virtual void		PortInit(void);
    virtual void		PortStatusChangedHandler(void);

public:

    IOUSBDeviceDescriptor	_desc;
    UInt8			_speed;					// kUSBDeviceSpeedLow or kUSBDeviceSpeedFull
    UInt32			_portPowerAvailable;

    int 			_portNum;

    virtual IOReturn		start(void);
    virtual void 		stop(void);

    IOReturn 		AddDevice(void);
    void 		RemoveDevice();
    IOReturn		ResetPort();
    bool 		StatusChanged(void);

    void 		InitPortVectors(void);
    void 		SetPortVector(ChangeHandlerFuncPtr routine, UInt32 condition);
    IOReturn 		DefaultOverCrntChangeHandler(UInt16 changeFlags);
    IOReturn 		DefaultResetChangeHandler(UInt16 changeFlags);
    IOReturn 		DefaultSuspendChangeHandler(UInt16 changeFlags);
    IOReturn 		DefaultEnableChangeHandler(UInt16 changeFlags);
    IOReturn 		DefaultConnectionChangeHandler(UInt16 changeFlags);
    IOReturn 		AddDeviceResetChangeHandler(UInt16 changeFlags);
    IOReturn 		HandleResetPortHandler(UInt16 changeFlags);
    IOReturn 		HandleSuspendPortHandler(UInt16 changeFlags);
    void 		FatalError(IOReturn err, char *str);

    AppleUSBHub *	GetHub(void) { return _hub; }
    bool		IsCaptive(void) { return _captive; }
    
    bool		GetDevZeroLock(void) { return _devZero; }
    IOReturn		ReleaseDevZeroLock( void);
    UInt32		GetPortTimeStamp(void) { return _devZeroCounter; }

    IOReturn		SuspendPort(bool suspend);
    IOReturn		ReEnumeratePort(UInt32 options);

    void		DisplayOverCurrentNotice(bool individual);

};

#endif  _IOKIT_APPLEUSBHUBPORT_H
