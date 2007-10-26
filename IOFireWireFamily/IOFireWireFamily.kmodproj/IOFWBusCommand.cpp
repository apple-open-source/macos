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
 
//#define IOASSERT 1	// Set to 1 to activate assert()

// public
#include <IOKit/firewire/IOFWCommand.h>
#include <IOKit/firewire/IOFireWireController.h>
#include <IOKit/firewire/IOFireWireNub.h>
#include <IOKit/firewire/IOLocalConfigDirectory.h>

// system
#include <IOKit/assert.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommand.h>

OSDefineMetaClass( IOFWBusCommand, IOFWCommand )
OSDefineAbstractStructors(IOFWBusCommand, IOFWCommand)
OSMetaClassDefineReservedUnused(IOFWBusCommand, 0);

#pragma mark -

// initWithController
//
//

bool IOFWBusCommand::initWithController(IOFireWireController *control, FWBusCallback completion, void *refcon)
{
    if(!IOFWCommand::initWithController(control))
	return false;

    fSync = completion == NULL;
    fComplete = completion;
    fRefCon = refcon;
    return true;
}

// reinit
//
//

IOReturn IOFWBusCommand::reinit(FWBusCallback completion, void *refcon)
{
    if(fStatus == kIOReturnBusy || fStatus == kIOFireWirePending)
	return fStatus;

    fSync = completion == NULL;
    fComplete = completion;
    fRefCon = refcon;
    return kIOReturnSuccess;
}

// complete
//
//

IOReturn IOFWBusCommand::complete(IOReturn state)
{
    state = IOFWCommand::complete(state);
    if(fSync)
        fSyncWakeup->signal(state);
    else if(fComplete)
		(*fComplete)(fRefCon, state, fControl, this);
    return state;
}
