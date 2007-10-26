/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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
#include <IOKit/firewire/IOFireWireUnit.h>
#include <IOKit/firewire/IOFireWireBus.h>
#include <IOKit/firewire/IOFWAddressSpace.h>
#include <IOKit/firewire/IOFireWireDevice.h>
#include <IOKit/firewire/IOFireWireBus.h>
#include "IOFWAsyncStreamReceivePort.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
OSDefineMetaClassAndStructors(IOFWAsyncStreamReceivePort, IOFWLocalIsochPort)


bool IOFWAsyncStreamReceivePort::init(IODCLProgram *program, IOFireWireController *control, UInt32 channel)
{
	if(!IOFWLocalIsochPort::init(program, control))
		return false;

	fChannel = channel ;

	return true;
}

IOReturn IOFWAsyncStreamReceivePort::getSupported(IOFWSpeed &maxSpeed, UInt64 &chanSupported)
{
	IOFWLocalIsochPort::getSupported( maxSpeed, chanSupported ) ;
	
	if ((chanSupported & ((UInt64)1) << (63 - fChannel)) == 0)	// make sure desired channel is supported by hardware
		return kIOReturnNoChannels ;
		
	chanSupported = ((UInt64)1) << (63 - fChannel) ;	// we want channel 31 (standard for async streaming packets)

    return kIOReturnSuccess;
}

