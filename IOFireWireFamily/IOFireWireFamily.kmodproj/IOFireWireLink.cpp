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
 * Copyright (c) 2000 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 * 09 Nov 2000 wgulland created.
 *
 */

// public
#import <IOKit/firewire/IOFireWireDevice.h>
#import <IOKit/firewire/IOFWDCLPool.h>

// protected
#include <IOKit/firewire/IOFireWireLink.h>
#import <IOKit/firewire/IOFWWorkLoop.h>

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClass( IOFireWireLink, IOService )
OSDefineAbstractStructors(IOFireWireLink, IOService)

OSMetaClassDefineReservedUnused(IOFireWireLink, 0);
OSMetaClassDefineReservedUnused(IOFireWireLink, 1);
OSMetaClassDefineReservedUnused(IOFireWireLink, 2);
OSMetaClassDefineReservedUnused(IOFireWireLink, 3);
OSMetaClassDefineReservedUnused(IOFireWireLink, 4);
OSMetaClassDefineReservedUnused(IOFireWireLink, 5);
OSMetaClassDefineReservedUnused(IOFireWireLink, 6);
OSMetaClassDefineReservedUnused(IOFireWireLink, 7);
OSMetaClassDefineReservedUnused(IOFireWireLink, 8);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOFireWireController * IOFireWireLink::createController()
{
    IOFireWireController *control;

    control = OSTypeAlloc( IOFireWireController );
    if(NULL == control)
        return NULL;

    if(!control->init(this)) {
        control->release();
        control = NULL;
    }
    return control;
}

IOFWWorkLoop * IOFireWireLink::createWorkLoop()
{
    return IOFWWorkLoop::workLoop();
}

IOFireWireDevice *
IOFireWireLink::createDeviceNub(CSRNodeUniqueID guid, const IOFWNodeScan *scan)
{
    IOFireWireDevice *newDevice;
    OSDictionary *propTable;

    newDevice = OSTypeAlloc( IOFireWireDevice );

    if (!newDevice)
        return NULL;

    do {
        OSObject * prop;
        propTable = OSDictionary::withCapacity(6);
        if (!propTable)
            continue;

        prop = OSNumber::withNumber(guid, 64);
        if(prop) {
            propTable->setObject(gFireWire_GUID, prop);
            prop->release();
        }
		prop = OSNumber::withNumber((OSSwapBigToHostInt32(scan->fSelfIDs[0]) & kFWSelfID0SP) >> kFWSelfID0SPPhase, 32);
		if(prop) {
            propTable->setObject(gFireWireSpeed, prop);
            prop->release();
        }

        if(!newDevice->init(propTable, scan)) {
            newDevice->release();
            newDevice = NULL;
        }
        
//        IOLog("IOFireWireLink::createDeviceNub - GUID is 0x%llx\n", guid );

        // use quadlet reads for config rom
		newDevice->setMaxPackLog(false, true, 2);
    } while (false);
    if(propTable)
        propTable->release();	// done with it after init

    return newDevice;
}

IOFireWireController * IOFireWireLink::getController() const
{
    return fControl;
}

IOWorkLoop * IOFireWireLink::getWorkLoop() const
{
    return fWorkLoop;
}

IOFWWorkLoop * IOFireWireLink::getFireWireWorkLoop() const
{
    return fWorkLoop;
}

IOFWDCLPool *
IOFireWireLink::createDCLPool ( 
	UInt32				capacity )
{
	return NULL ;
}

void IOFireWireLink::disablePHYPortOnSleep( UInt32 mask )
{
	// nothing to do
}

UInt32 * IOFireWireLink::getPingTimes ()
{
	return NULL ;
}

IOReturn IOFireWireLink::handleAsyncCompletion( IOFWCommand *cmd, IOReturn status )
{
	// nothing to do
	
	return kIOReturnSuccess;
}

void IOFireWireLink::handleSystemShutDown( UInt32 messageType )
{
	// nothing to do
}

void IOFireWireLink::configureAsyncRobustness( bool enabled )
{
	// nothing to do
}

bool IOFireWireLink::isPhysicalAccessEnabledForNodeID( UInt16 nodeID )
{
	return false;
}

void IOFireWireLink::notifyInvalidSelfIDs (void)
{

}

IOReturn IOFireWireLink::asyncPHYPacket( UInt32 data, UInt32 data2, IOFWAsyncPHYCommand * cmd )
{
	return kIOReturnUnsupported;
}

bool IOFireWireLink::enterLoggingMode( void )
{
	return false;
}

IOReturn IOFireWireLink::getCycleTimeAndUpTime( UInt32 &cycleTime, UInt64 &uptime )
{
	return kIOReturnUnsupported;
}

UInt32 IOFireWireLink::setLinkMode( UInt32 arg1, UInt32 arg2 )
{
	return 0;
}
