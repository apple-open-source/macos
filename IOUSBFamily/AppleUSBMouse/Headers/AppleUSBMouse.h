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
#ifndef _IOKIT_APPLEUSBMOUSE_H
#define _IOKIT_APPLEUSBMOUSE_H

#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/hidsystem/IOHIPointing.h>
#include <IOKit/hidsystem/IOHIDDescriptorParser.h>
#include <IOKit/IOBufferMemoryDescriptor.h>

#include <IOKit/usb/IOUSBBus.h>
#include <IOKit/usb/IOUSBDevice.h>
#include <IOKit/usb/USB.h>

#define kMouseRetryCount	3

class AppleUSBMouse : public IOHIPointing
{
    OSDeclareDefaultStructors(AppleUSBMouse)

private:
    IOUSBInterface *		_interface;
    IOUSBDevice *		_device;
    IOUSBPipe * 		_interruptPipe;
    IOBufferMemoryDescriptor *	_buffer;
    IOCommandGate *		_gate;

    IOUSBCompletion		_completion;
    HIDPreparsedDataRef		_preparsedReportDescriptorData;
    Bounds			_bounds;
    IOItemCount			_numButtons;
    IOFixed     		_resolution;

    thread_call_t		_deviceDeadCheckThread;
    thread_call_t		_clearFeatureEndpointHaltThread;

    SInt32			_buttonCollection;
    SInt32			_xCollection;
    SInt32			_yCollection;
    SInt32			_tipPressureCollection;
    SInt32			_digitizerButtonCollection;
    SInt32			_scrollWheelCollection;
    SInt32			_tipPressureMin;

    UInt32			_retryCount;
    UInt32			_outstandingIO;

    SInt16			_tipPressureMax;

    UInt16			_maxPacketSize;

    bool			_absoluteCoordinates;
    bool			_hasInRangeReport;
    bool			_deviceDeadThreadActive;
    bool			_deviceIsDead;
    bool			_deviceHasBeenDisconnected;
    bool			_needToClose;
    IONotifier * 		_notifier;


    // IOService methods
    virtual bool	init(OSDictionary *properties);
    virtual bool	start(IOService * provider);
    virtual void 	stop(IOService *  provider);
    virtual IOReturn 	message(UInt32 type, IOService * provider,  void * argument = 0);
    virtual bool 	finalize(IOOptionBits options);

    // "new" IOService methods. Some of these may go away before we ship 1.8.5
    virtual bool 	willTerminate( IOService * provider, IOOptionBits options );
    virtual bool 	didTerminate( IOService * provider, IOOptionBits options, bool * defer );
#if 0
    virtual bool 	requestTerminate( IOService * provider, IOOptionBits options );
    virtual bool 	terminate( IOOptionBits options = 0 );
    virtual void 	free( void );
    virtual bool 	terminateClient( IOService * client, IOOptionBits options );
#endif

    // IOHIDevice methods
    virtual UInt32 	interfaceID(void);
    virtual UInt32 	deviceType(void);

    // IOHIPointing methods
    IOFixed     	resolution();
    IOItemCount 	buttonCount();

    // misc methods
    void                MoveMouse(UInt8 *mouseData, UInt32 ret_bufsize);
    virtual bool	parseHIDDescriptor();
    void 		InterruptReadHandler(IOReturn status, UInt32 bufferSizeRemaining);
    void		CheckForDeadDevice();
    void		ClearFeatureEndpointHalt(void);
    void		DecrementOutstandingIO(void);
    void		IncrementOutstandingIO(void);

    // static methods for callbacks, the command gate, new threads, etc.
    static void 	InterruptReadHandlerEntry(OSObject *target, void *param, IOReturn status, UInt32 bufferSizeRemaining);
    static void 	CheckForDeadDeviceEntry(OSObject *target);
    static void		ClearFeatureEndpointHaltEntry(OSObject *target);
    static IOReturn	ChangeOutstandingIO(OSObject *target, void *arg0, void *arg1, void *arg2, void *arg3);
    static IOReturn 	PowerDownHandler(void *target, void *refCon, UInt32 messageType, IOService *service,
                                      void *messageArgument, vm_size_t argSize);
};

#endif _IOKIT_APPLEUSBMOUSE_H
