/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 1999-2002 Apple Computer, Inc.  All rights reserved.
 *
 * IOFWIsochPort is an abstract object that represents hardware on the bus
 * (locally or remotely) that sends or receives isochronous packets.
 * Local ports are implemented by the local device driver,
 * Remote ports are implemented by the driver for the remote device.
 *
 * HISTORY
 *
 */
#include <IOKit/IOWorkLoop.h>
#include <IOKit/firewire/IOFWIsochPort.h>
#include <IOKit/firewire/IOFWLocalIsochPort.h>
#include <IOKit/firewire/IOFWDCLProgram.h>
#include <IOKit/firewire/IOFWCommand.h>
#include <IOKit/firewire/IOFireWireController.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
OSDefineMetaClass( IOFWIsochPort, OSObject )
OSDefineAbstractStructors(IOFWIsochPort, OSObject)
OSMetaClassDefineReservedUnused(IOFWIsochPort, 0);
OSMetaClassDefineReservedUnused(IOFWIsochPort, 1);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
OSDefineMetaClassAndStructors(IOFWLocalIsochPort, IOFWIsochPort)
OSMetaClassDefineReservedUnused(IOFWLocalIsochPort, 0);
OSMetaClassDefineReservedUnused(IOFWLocalIsochPort, 1);

bool IOFWLocalIsochPort::init(IODCLProgram *program, IOFireWireController *control)
{
    if(!IOFWIsochPort::init())
	return false;
    fProgram = program; // belongs to us.
    fControl = control;
    return true;
}

void IOFWLocalIsochPort::free()
{
    if(fProgram)
	fProgram->release();
    IOFWIsochPort::free();
}

// Return maximum speed and channels supported
// (bit n set = chan n supported)
IOReturn IOFWLocalIsochPort::getSupported(IOFWSpeed &maxSpeed, UInt64 &chanSupported)
{
    maxSpeed = kFWSpeedMaximum;
    chanSupported = ~(UInt64)0;
    return kIOReturnSuccess;
}

/*
 * Allocate hardware resources for port, via workloop
 * Then compile program, not on workloop.
 */
IOReturn IOFWLocalIsochPort::allocatePort(IOFWSpeed speed, UInt32 chan)
{
    IOReturn res;

    fControl->closeGate();
    res = fProgram->allocateHW(speed, chan);
    fControl->openGate();
    
    if(kIOReturnSuccess != res)
		return res; 
		
    return fProgram->compile(speed, chan);	// Not on workloop
}

IOReturn IOFWLocalIsochPort::releasePort()
{
    IOReturn res;

    fControl->closeGate();
    res = fProgram->releaseHW();
    fControl->openGate();
    return res;
}

IOReturn IOFWLocalIsochPort::start()
{
    IOReturn res;

    fControl->closeGate();
    res = fProgram->start();
    fControl->openGate();
    return res;
}

IOReturn IOFWLocalIsochPort::stop()
{
    fControl->closeGate();
    fProgram->stop();
    fControl->openGate();
    return kIOReturnSuccess;
}

IOReturn IOFWLocalIsochPort::notify(UInt32 notificationType,
	DCLCommand** dclCommandList, UInt32 numDCLCommands)
{
    IOReturn res;

    res = fProgram->notify(notificationType,
                                dclCommandList, numDCLCommands);
    return res;
}
