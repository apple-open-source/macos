/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
 *
 *	IOFireWireAVCLocalNode.h
 *
 * Implementation of class to initialize the Local node's AVC Target mode support
 *
 */

#include <IOKit/avc/IOFireWireAVCLocalNode.h>

OSDefineMetaClassAndStructors(IOFireWireAVCLocalNode, IOService);

#pragma mark -
#pragma mark еее IOService methods еее

bool IOFireWireAVCLocalNode::start(IOService *provider)
{
	//IOLog( "IOFireWireAVCLocalNode::start\n");

    fDevice = OSDynamicCast(IOFireWireNub, provider);
	if(!fDevice)
        return false;
	
    if (!IOService::start(provider))
        return false;

    fPCRSpace = IOFireWirePCRSpace::getPCRAddressSpace(fDevice->getBus());
    if(!fPCRSpace)
        return false;
    fPCRSpace->activate();

    fAVCTargetSpace = IOFireWireAVCTargetSpace::getAVCTargetSpace(fDevice->getController());
    if(!fAVCTargetSpace)
        return false;
    fAVCTargetSpace->activateWithUserClient((IOFireWireAVCProtocolUserClient*)0xFFFFFFFF);

	// Enable the communication between the PCR space and the Target space objects
	fPCRSpace->setAVCTargetSpacePointer(fAVCTargetSpace);
	
    registerService();

	fStarted = true;
	
    return true;
}

bool IOFireWireAVCLocalNode::finalize(IOOptionBits options)
{
	//IOLog( "IOFireWireAVCLocalNode::finalize\n");

	return IOService::finalize(options);
}

void IOFireWireAVCLocalNode::stop(IOService *provider)
{
	//IOLog( "IOFireWireAVCLocalNode::stop\n");

	IOService::stop(provider);
}

void IOFireWireAVCLocalNode::free(void)
{
	//IOLog( "IOFireWireAVCLocalNode::free\n");

    if(fPCRSpace)
	{
        fPCRSpace->deactivate();
        fPCRSpace->release();
    }

    if(fAVCTargetSpace)
	{
        fAVCTargetSpace->deactivateWithUserClient((IOFireWireAVCProtocolUserClient*)0xFFFFFFFF);
        fAVCTargetSpace->release();
    }
	
	return IOService::free();
}

IOReturn IOFireWireAVCLocalNode::message(UInt32 type, IOService *provider, void *argument)
{
    IOReturn res = kIOReturnUnsupported;

	//IOLog( "IOFireWireAVCLocalNode::message\n");

	switch (type)
	{
		case kIOMessageServiceIsTerminated:
		case kIOMessageServiceIsRequestingClose:
		case kIOMessageServiceIsResumed:
			res = kIOReturnSuccess;
			break;

		// This message is received when a bus-reset start happens!
		case kIOMessageServiceIsSuspended:
			res = kIOReturnSuccess;
			if((fStarted == true) && (fPCRSpace))
				fPCRSpace->clearAllP2PConnections();
			break;

		default:
			break;
	}
	
	messageClients(type);

    return res;
}


